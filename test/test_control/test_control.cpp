#include <stdlib.h>
#include <string.h>
#include <unity.h>

#include "control.h"

// Limit records of the four curve settings, 5 bytes each: hot water A 35+21 / B 45+11 -> 45..55,
// leaving water B 29+27 / A 29+17 -> 29..45 (the same block on both boards, at different addresses)
static const uint8_t LIMITS[32] = {0xFF, 0x00, 0x15, 0x23, 0x08, 0xFF, 0x00, 0x0B, 0x2D, 0x08, 0xFF, 0x00, 0x1B, 0x1D, 0x08, 0xFF,
                                   0x00, 0x11, 0x1D, 0x08, 0xFF, 0x00, 0x10, 0x0A, 0x08, 0xFF, 0x00, 0x0F, 0x08, 0x08, 0xFF, 0x00};

// Fake hydro board: answers the identification, memory and RAM commands of the X10A port on a small memory image.
struct FakeUnit
{
    uint8_t softwareId[4];
    uint8_t memory[1024]; // the board's memory, by offset
    uint8_t ram[0x6000];  // RAM, by address
    bool answer = true;
    int ramWrites = 0, memoryWrites = 0, badWrites = 0;

    void reset()
    {
        const uint8_t id[4] = {0x04, 0x29, 0x00, 0x18}; // reads "18002904"
        memcpy(softwareId, id, 4);
        memset(memory, 0, sizeof(memory));
        memset(ram, 0, sizeof(ram));
        answer = true;
        ramWrites = memoryWrites = badWrites = 0;
        // hot water: weather-dependent tank mode, curve 52 (low outdoor temperature) to 48 (high),
        // limits 35+21 / 45+11 -> 45..55, effective 48 capped by a ceiling of 51
        memory[278] = 0x01;
        memory[366] = (uint8_t)((48 - 35) << 2 | 0x01);
        memory[367] = (uint8_t)((52 - 45) << 2 | 0x03);
        setWord(0x187A, 48 << 8);
        setWord(0x18B8, 52 << 8);
        setWord(0x1306, 48 << 8);
        setWord(0x1552, 51 << 8);
        // leaving water: weather-dependent main zone, curve 40 (low) to 36 (high), limits 29+27 / 29+17 -> 29..45,
        // the resulting target 37 (interpolated)
        memory[374] = 0x04;
        memory[372] = (uint8_t)((40 - 29) << 2 | 0x03);
        memory[373] = (uint8_t)((36 - 29) << 2 | 0x03);
        setWord(0x18BA, 40 << 8);
        setWord(0x187C, 36 << 8);
        setWord(0x136A, 37 << 8 | 0x4D);
        memcpy(ram + 0x30C0, LIMITS, 32);
        // power limitation: none, power kind, limit 1 = 20 kW (float), amps 50, nothing active
        ram[0x2FE8] = 0;
        ram[0x2FE9] = 1;
        setFloat(0x1880, 20.0f);
        ram[0x2FC8] = 50;
        setFloat(0x19F4, 0.0f);
    }

    void setFloat(uint16_t addr, float v) { memcpy(ram + addr, &v, 4); }
    float getFloat(uint16_t addr) const { float v; memcpy(&v, ram + addr, 4); return v; }

    // The same unit on another hydro board software: the memory bytes are the same, every RAM address comes
    // from the profile (the limit records sit 5 bytes apart, in the order hot water A, B, leaving water B, A)
    void board(const ControlProfile &p)
    {
        for (int i = 0; i < 4; i++)
        {
            char hex[3] = {p.softwareId[2 * i], p.softwareId[2 * i + 1], 0};
            softwareId[3 - i] = (uint8_t)strtol(hex, nullptr, 16);
        }
        memset(ram, 0, sizeof(ram));
        setWord(p.dhw.ramA, 48 << 8);
        setWord(p.dhw.ramB, 52 << 8);
        setWord(p.dhw.ramEffective, 48 << 8);
        setWord(p.dhw.ramCeiling, 51 << 8);
        setWord(p.lwt.ramB, 40 << 8);
        setWord(p.lwt.ramA, 36 << 8);
        setWord(p.lwt.ramEffective, 37 << 8 | 0x4D);
        memcpy(ram + p.dhw.slotA, LIMITS, 32);
        if (p.limit.ramEnable) // power limitation: none, power kind, limit 1 = 20 kW, amps 50, nothing active
        {
            ram[p.limit.ramEnable] = 0;
            ram[p.limit.ramKind] = 1;
            setFloat(p.limit.ramKw, 20.0f);
            for (int i = 0; i < 4; i++)
                ram[p.limit.ramAmp + i] = 50;
        }
    }

    static const ControlProfile &profileFor(const char *id)
    {
        for (size_t i = 0; i < CONTROL_PROFILE_COUNT; i++)
            if (strcmp(CONTROL_PROFILES[i].softwareId, id) == 0)
                return CONTROL_PROFILES[i];
        return CONTROL_PROFILES[0];
    }

    static bool isCurveWord(uint16_t addr)
    {
        for (size_t i = 0; i < CONTROL_PROFILE_COUNT; i++)
        {
            const ControlProfile &p = CONTROL_PROFILES[i];
            if (addr == p.dhw.ramA || addr == p.dhw.ramB || addr == p.lwt.ramA || addr == p.lwt.ramB)
                return true;
            if (p.limit.ramEnable && (addr == p.limit.ramEnable || addr == p.limit.ramKw || addr == p.limit.ramAmp))
                return true;
        }
        return false;
    }

    void setWord(uint16_t addr, uint16_t v)
    {
        ram[addr] = (uint8_t)v;
        ram[addr + 1] = (uint8_t)(v >> 8);
    }
    int word(uint16_t addr) const { return ram[addr] | ram[addr + 1] << 8; }

    static uint8_t checksum(const uint8_t *b, size_t n)
    {
        uint8_t s = 0;
        for (size_t i = 0; i < n; i++)
            s += b[i];
        return (uint8_t)~s;
    }

    size_t exchange(const uint8_t *frame, size_t len, uint8_t *reply, size_t maxLen)
    {
        if (!answer || len < 3 || frame[0] != len - 1 || checksum(frame, len - 1) != frame[len - 1])
            return 0;
        uint8_t cmd = frame[1];
        const uint8_t *data = frame + 2;
        size_t n = len - 3, out = 0;
        reply[out++] = cmd;
        if (cmd == 0x4C && n == 0)
        {
            memcpy(reply + out, softwareId, 4);
            out += 4;
        }
        else if (cmd == 0x49 && n == 2)
        {
            reply[out++] = memory[data[0] | data[1] << 8];
        }
        else if (cmd == 0x52 && n == 2)
        {
            memcpy(reply + out, ram + (data[0] | data[1] << 8), 16);
            out += 16;
        }
        else if (cmd == 0x46 && n >= 3 && n <= 6)
        {
            uint16_t addr = data[0] | data[1] << 8;
            ramWrites++;
            if (!isCurveWord(addr))
                badWrites++;
            memcpy(ram + addr, data + 2, n - 2);
        }
        else if (cmd == 0x45 && n == 3)
        {
            uint16_t off = data[0] | data[1] << 8;
            memoryWrites++;
            if (off != 366 && off != 367 && off != 372 && off != 373)
                badWrites++;
            memory[off] = data[2];
        }
        else
        {
            reply[0] = 0x15; // NAK
            return 1;
        }
        reply[out] = checksum(reply, out);
        out++;
        return out > maxLen ? maxLen : out;
    }
};

static FakeUnit unit;

static size_t fakeExchange(const uint8_t *frame, size_t len, uint8_t *reply, size_t maxLen, unsigned long)
{
    return unit.exchange(frame, len, reply, maxLen);
}

static CurveControl fresh(ControlKind kind = CONTROL_DHW)
{
    CurveControl c;
    c.kind = kind;
    c.exchange = fakeExchange;
    return c;
}

void setUp() { unit.reset(); }
void tearDown() {}

void test_frame_bytes()
{
    uint8_t out[CONTROL_FRAME_MAX];
    const uint8_t a[2] = {0xE8, 0x26};
    TEST_ASSERT_EQUAL(5, x10aFrame(0x52, a, 2, out));
    const uint8_t wantRead[5] = {0x04, 0x52, 0xE8, 0x26, 0x9B};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(wantRead, out, 5);
    const uint8_t w[3] = {0xF4, 0x26, 0x2F};
    TEST_ASSERT_EQUAL(6, x10aFrame(0x46, w, 3, out));
    const uint8_t wantWrite[6] = {0x05, 0x46, 0xF4, 0x26, 0x2F, 0x6B};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(wantWrite, out, 6);
}

void test_identifies_the_unit()
{
    CurveControl c = fresh();
    TEST_ASSERT_TRUE(c.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_READY, c.status);
    TEST_ASSERT_EQUAL_STRING("18002904", c.softwareId);
    TEST_ASSERT_EQUAL(45, c.minTarget);
    TEST_ASSERT_EQUAL(55, c.maxTarget);
    TEST_ASSERT_EQUAL(48, c.target);
    TEST_ASSERT_EQUAL(52, c.targetLo);
    TEST_ASSERT_EQUAL(48, c.effective);
    TEST_ASSERT_EQUAL(51, c.ceiling);
    TEST_ASSERT_EQUAL(-1, c.saved); // the two points differ
    TEST_ASSERT_TRUE(c.changed);
    TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
}

void test_refuses_an_unknown_software()
{
    unit.softwareId[0] = 0x05;
    CurveControl c = fresh();
    TEST_ASSERT_FALSE(c.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, c.status);
    TEST_ASSERT_EQUAL_STRING("18002905", c.softwareId);
    c.request(50);
    c.tick(2000);
    c.tick(700000); // identifies again, still unsupported
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, c.status);
    TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
}

void test_refuses_without_the_curve_mode()
{
    unit.memory[278] = 0x00;
    CurveControl c = fresh();
    TEST_ASSERT_FALSE(c.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_NOT_CURVE_MODE, c.status);
    c.request(50);
    c.tick(2000);
    TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
    // the mode is switched on the interface: found at the next check
    unit.memory[278] = 0x01;
    c.tick(1000 + CONTROL_CHECK_INTERVAL_MS);
    TEST_ASSERT_EQUAL(CONTROL_READY, c.status);
}

void test_no_answer()
{
    unit.answer = false;
    CurveControl c = fresh();
    TEST_ASSERT_FALSE(c.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_NO_ANSWER, c.status);
    TEST_ASSERT_EQUAL(-1, c.target);
}

void test_applies_then_saves()
{
    CurveControl c = fresh();
    c.identify(1000);
    c.changed = false;
    c.request(50);
    c.tick(2000);
    // both points of the curve in RAM at once, nothing in the memory yet
    TEST_ASSERT_EQUAL(2, unit.ramWrites);
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    TEST_ASSERT_EQUAL(50, c.target);
    TEST_ASSERT_EQUAL(50, c.targetLo);
    TEST_ASSERT_EQUAL(50, c.applied);
    TEST_ASSERT_TRUE(c.changed);
    TEST_ASSERT_EQUAL(50, (int)c.wanted); // still pending its save
    c.tick(2000 + CONTROL_SAVE_DELAY_MS - 1);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    c.tick(2000 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(2, unit.memoryWrites);
    TEST_ASSERT_EQUAL((50 - 35) << 2 | 0x01, unit.memory[366]); // flag bits kept
    TEST_ASSERT_EQUAL((50 - 45) << 2 | 0x03, unit.memory[367]);
    TEST_ASSERT_EQUAL(50, c.saved);
    TEST_ASSERT_EQUAL(-1, (int)c.wanted);
    TEST_ASSERT_EQUAL(2, unit.ramWrites); // not written again
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

void test_reads_the_effective_target_after_a_write()
{
    CurveControl c = fresh();
    c.identify(1000);
    c.request(55);
    c.tick(2000);
    TEST_ASSERT_EQUAL(48, c.effective); // the board has not recomputed yet
    unit.setWord(0x1306, 51 << 8); // capped by the ceiling
    c.changed = false;
    c.tick(2000 + CONTROL_READ_AFTER_WRITE_MS - 1000);
    TEST_ASSERT_EQUAL(48, c.effective);
    c.tick(2000 + CONTROL_READ_AFTER_WRITE_MS);
    TEST_ASSERT_EQUAL(51, c.effective);
    TEST_ASSERT_EQUAL(51, c.ceiling);
    TEST_ASSERT_TRUE(c.changed);
}

void test_saves_at_most_every_five_minutes()
{
    CurveControl c = fresh();
    c.identify(1000);
    c.request(50);
    c.tick(2000);
    c.tick(2000 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(50, c.saved);
    unsigned long t = 2000 + CONTROL_SAVE_DELAY_MS + 1000;
    c.request(51);
    c.tick(t);
    TEST_ASSERT_EQUAL(51 << 8, unit.word(0x187A)); // applied at once
    c.tick(t + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(50, c.saved); // not saved yet: too soon after the last save
    TEST_ASSERT_EQUAL(2, unit.memoryWrites);
    c.tick(2000 + CONTROL_SAVE_DELAY_MS + CONTROL_SAVE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(51, c.saved);
    TEST_ASSERT_EQUAL(4, unit.memoryWrites);
}

void test_clamps_to_the_limits_of_the_unit()
{
    CurveControl c = fresh();
    c.identify(1000);
    c.request(70);
    c.tick(2000);
    TEST_ASSERT_EQUAL(55, c.target);
    TEST_ASSERT_EQUAL(55 << 8, unit.word(0x187A));
    c.request(20);
    c.tick(3000);
    TEST_ASSERT_EQUAL(45, c.target);
}

void test_a_value_already_saved_is_not_written_again()
{
    unit.memory[366] = (uint8_t)((50 - 35) << 2 | 0x01);
    unit.memory[367] = (uint8_t)((50 - 45) << 2 | 0x03);
    unit.setWord(0x187A, 50 << 8);
    unit.setWord(0x18B8, 50 << 8);
    CurveControl c = fresh();
    c.identify(1000);
    TEST_ASSERT_EQUAL(50, c.saved);
    c.request(50);
    c.tick(2000);
    c.tick(2000 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(0, unit.ramWrites);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    TEST_ASSERT_EQUAL(-1, (int)c.wanted);
}

void test_stops_when_the_interface_leaves_the_curve_mode()
{
    CurveControl c = fresh();
    c.identify(1000);
    unit.memory[278] = 0x00; // changed on the interface meanwhile
    c.request(50);
    c.tick(2000);
    TEST_ASSERT_EQUAL(0, unit.ramWrites);
    TEST_ASSERT_EQUAL(CONTROL_NOT_CURVE_MODE, c.status);
    TEST_ASSERT_EQUAL(-1, (int)c.wanted);
}

void test_reads_back_a_change_made_on_the_interface()
{
    CurveControl c = fresh();
    c.identify(1000);
    c.changed = false;
    unit.setWord(0x187A, 53 << 8);
    unit.setWord(0x18B8, 53 << 8);
    c.tick(1000 + CONTROL_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL(53, c.target);
    TEST_ASSERT_EQUAL(53, c.targetLo);
    TEST_ASSERT_TRUE(c.changed);
}

void test_leaving_water_identifies_with_its_own_curve()
{
    CurveControl c = fresh(CONTROL_LWT);
    TEST_ASSERT_TRUE(c.identify(1000));
    TEST_ASSERT_EQUAL(29, c.minTarget);
    TEST_ASSERT_EQUAL(45, c.maxTarget);
    TEST_ASSERT_EQUAL(36, c.target);   // high outdoor temperature point
    TEST_ASSERT_EQUAL(40, c.targetLo); // low outdoor temperature point
    TEST_ASSERT_EQUAL(37, c.effective);
    TEST_ASSERT_EQUAL(-1, c.ceiling); // none for the leaving water
    TEST_ASSERT_EQUAL(-1, c.saved);
    // the zone is not weather dependent: refused
    unit.memory[374] = 0x00;
    TEST_ASSERT_FALSE(c.identify(2000));
    TEST_ASSERT_EQUAL(CONTROL_NOT_CURVE_MODE, c.status);
}

void test_leaving_water_writes_its_own_words_and_bytes()
{
    CurveControl c = fresh(CONTROL_LWT);
    c.identify(1000);
    c.request(38);
    c.tick(2000);
    TEST_ASSERT_EQUAL(38 << 8, unit.word(0x18BA));
    TEST_ASSERT_EQUAL(38 << 8, unit.word(0x187C));
    TEST_ASSERT_EQUAL(48 << 8, unit.word(0x187A)); // the hot water curve untouched
    c.tick(2000 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL((38 - 29) << 2 | 0x03, unit.memory[372]);
    TEST_ASSERT_EQUAL((38 - 29) << 2 | 0x03, unit.memory[373]);
    TEST_ASSERT_EQUAL(38, c.saved);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
    c.request(60); // above the unit's own limit for the curve
    c.tick(3000 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(45, c.target);
}

// Every profile but the first (the one reset() builds): the same unit on that board writes that board's words
// and the same memory bytes, and nothing at the first board's addresses.
void test_every_other_software_writes_its_own_addresses()
{
    TEST_ASSERT_GREATER_THAN(1, CONTROL_PROFILE_COUNT);
    for (size_t i = 1; i < CONTROL_PROFILE_COUNT; i++)
    {
        const ControlProfile &p = CONTROL_PROFILES[i];
        unit.reset();
        unit.board(p);
        CurveControl c = fresh();
        TEST_ASSERT_TRUE_MESSAGE(c.identify(1000), p.softwareId);
        TEST_ASSERT_EQUAL_STRING(p.softwareId, c.softwareId);
        TEST_ASSERT_EQUAL(45, c.minTarget);
        TEST_ASSERT_EQUAL(55, c.maxTarget);
        TEST_ASSERT_EQUAL(48, c.target);
        TEST_ASSERT_EQUAL(52, c.targetLo);
        TEST_ASSERT_EQUAL(48, c.effective);
        TEST_ASSERT_EQUAL(51, c.ceiling);
        c.request(50);
        c.tick(2000);
        TEST_ASSERT_EQUAL_MESSAGE(50 << 8, unit.word(p.dhw.ramA), p.softwareId);
        TEST_ASSERT_EQUAL(50 << 8, unit.word(p.dhw.ramB));
        TEST_ASSERT_EQUAL(0, unit.word(CONTROL_PROFILES[0].dhw.ramA)); // nothing at the first board's addresses
        c.tick(2000 + CONTROL_SAVE_DELAY_MS);
        TEST_ASSERT_EQUAL((50 - 35) << 2 | 0x01, unit.memory[366]); // the same memory bytes as the first board
        TEST_ASSERT_EQUAL((50 - 45) << 2 | 0x03, unit.memory[367]);
        TEST_ASSERT_EQUAL(50, c.saved);

        CurveControl l = fresh(CONTROL_LWT);
        TEST_ASSERT_TRUE_MESSAGE(l.identify(1000), p.softwareId);
        TEST_ASSERT_EQUAL(29, l.minTarget);
        TEST_ASSERT_EQUAL(45, l.maxTarget);
        TEST_ASSERT_EQUAL(36, l.target);
        TEST_ASSERT_EQUAL(40, l.targetLo);
        TEST_ASSERT_EQUAL(37, l.effective);
        TEST_ASSERT_EQUAL(-1, l.ceiling);
        l.request(38);
        l.tick(2000);
        TEST_ASSERT_EQUAL_MESSAGE(38 << 8, unit.word(p.lwt.ramB), p.softwareId);
        TEST_ASSERT_EQUAL(38 << 8, unit.word(p.lwt.ramA));
        TEST_ASSERT_EQUAL(50 << 8, unit.word(p.dhw.ramA)); // the hot water curve untouched
        TEST_ASSERT_EQUAL_MESSAGE(0, unit.badWrites, p.softwareId);
    }
}

// The curve found before the first write is remembered, and a restore puts it back at once, both points, in the RAM
// and in the memory (no 30 s wait), then forgets it.
void test_restores_the_original_curve()
{
    CurveControl c = fresh();
    TEST_ASSERT_TRUE(c.identify(1000));
    TEST_ASSERT_EQUAL(-1, c.origA);
    c.requestRestore(); // nothing known yet: nothing happens
    c.tick(1500);
    TEST_ASSERT_EQUAL(0, unit.ramWrites);
    c.request(50);
    c.tick(2000);
    TEST_ASSERT_EQUAL(48, c.origA); // the curve as it was: 48 at high, 52 at low outdoor temperature
    TEST_ASSERT_EQUAL(52, c.origB);
    TEST_ASSERT_TRUE(c.origChanged);
    c.origChanged = false;
    c.request(51); // a second request keeps the original
    c.tick(3000);
    TEST_ASSERT_EQUAL(48, c.origA);
    TEST_ASSERT_FALSE(c.origChanged);
    c.tick(3000 + CONTROL_SAVE_DELAY_MS); // saved flat at 51
    TEST_ASSERT_EQUAL(51, c.saved);
    unsigned long t = 3000 + CONTROL_SAVE_DELAY_MS + CONTROL_SAVE_INTERVAL_MS;
    int writes = unit.memoryWrites;
    c.requestRestore();
    c.tick(t);
    TEST_ASSERT_EQUAL(48 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(52 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(writes + 2, unit.memoryWrites); // the memory right away
    TEST_ASSERT_EQUAL((48 - 35) << 2 | 0x01, unit.memory[366]);
    TEST_ASSERT_EQUAL((52 - 45) << 2 | 0x03, unit.memory[367]);
    TEST_ASSERT_EQUAL(48, c.target);
    TEST_ASSERT_EQUAL(52, c.targetLo);
    TEST_ASSERT_EQUAL(-1, c.saved); // the points differ again
    TEST_ASSERT_EQUAL(-1, c.origA); // forgotten: the unit has its curve
    TEST_ASSERT_TRUE(c.origChanged);
    TEST_ASSERT_FALSE(c.restoring);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// A restore waits for the memory's 5 minute interval, but the RAM is written at once; the original given by
// the caller (from the saved configuration) is used like one captured here.
void test_restore_from_the_saved_original_keeps_the_memory_interval()
{
    CurveControl c = fresh();
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(50);
    c.tick(2000);
    c.tick(2000 + CONTROL_SAVE_DELAY_MS); // memory written now
    TEST_ASSERT_EQUAL(50, c.saved);
    c.setOriginal(46, 54); // as the caller would after a restart
    int writes = unit.memoryWrites;
    c.requestRestore();
    c.tick(3000 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(46 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(54 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(writes, unit.memoryWrites); // too soon for the memory
    TEST_ASSERT_TRUE(c.restoring);
    TEST_ASSERT_EQUAL(46, c.origA);
    c.tick(2000 + CONTROL_SAVE_DELAY_MS + CONTROL_SAVE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(writes + 2, unit.memoryWrites);
    TEST_ASSERT_EQUAL((46 - 35) << 2 | 0x01, unit.memory[366]);
    TEST_ASSERT_EQUAL((54 - 45) << 2 | 0x03, unit.memory[367]);
    TEST_ASSERT_FALSE(c.restoring);
    TEST_ASSERT_EQUAL(-1, c.origA);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

static LimitControl freshLimit()
{
    LimitControl c;
    c.exchange = fakeExchange;
    return c;
}

// The power limit: identified from the unit's own state, applied as limit 1 + "continuous", read back, released
void test_power_limit_applies_and_releases()
{
    LimitControl c = freshLimit();
    TEST_ASSERT_TRUE(c.identify(1000));
    TEST_ASSERT_EQUAL_STRING("18002904", c.softwareId);
    TEST_ASSERT_EQUAL(LIMIT_POWER, c.kind);
    TEST_ASSERT_EQUAL(0, c.enable);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, c.limit);
    TEST_ASSERT_FALSE(c.on);
    c.request(3.0f);
    c.tick(2000);
    TEST_ASSERT_EQUAL(1, unit.ram[0x2FE8]);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, unit.getFloat(0x1880));
    TEST_ASSERT_TRUE(c.on);
    TEST_ASSERT_EQUAL(0, c.origEnable);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, c.applied);
    unit.setFloat(0x19F4, 3.0f); // the unit applies it
    c.tick(8000);                // read back 5 s after the write
    TEST_ASSERT_EQUAL_FLOAT(3.0f, c.active);
    c.request(0.2f); // under the floor: clamped, the compressor would stop below its minimum
    c.tick(9000);
    TEST_ASSERT_EQUAL_FLOAT(LIMIT_MIN_KW, unit.getFloat(0x1880));
    c.requestOn(false);
    c.tick(10000);
    TEST_ASSERT_EQUAL(0, unit.ram[0x2FE8]); // the installer's "none" is back
    TEST_ASSERT_FALSE(c.on);
    TEST_ASSERT_TRUE(c.released);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites); // never the board's memory
}

// After a restart of the heat pump its own settings are back: the limit is written again, once per minute at most
void test_power_limit_is_applied_again_after_the_unit_restarts()
{
    LimitControl c = freshLimit();
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(4.0f);
    c.tick(2000);
    unit.ram[0x2FE8] = 0; // the unit restarted
    unit.setFloat(0x1880, 20.0f);
    c.tick(2000 + LIMIT_READ_INTERVAL_MS); // read back: gone, but written only 2 minutes ago... re-apply allowed after 1 min
    TEST_ASSERT_EQUAL(1, unit.ram[0x2FE8]);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, unit.getFloat(0x1880));
    TEST_ASSERT_EQUAL(0, c.origEnable); // still the installer's setting, not our own 1
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// The limit saved by the caller (ESPAltherma restarted) is applied once the unit is ready; a unit set to
// current limits takes amps as a byte; an unmapped board is unsupported
void test_power_limit_restored_and_current_kind()
{
    LimitControl c = freshLimit();
    c.restore(true, 5.0f, 0);
    c.tick(1000); // identifies
    c.tick(1500); // applies the restored limit
    TEST_ASSERT_EQUAL(1, unit.ram[0x2FE8]);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, unit.getFloat(0x1880));

    unit.reset();
    unit.ram[0x2FE9] = 0; // current kind
    LimitControl a = freshLimit();
    TEST_ASSERT_TRUE(a.identify(1000));
    TEST_ASSERT_EQUAL(LIMIT_CURRENT, a.kind);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, a.limit);
    a.request(12.0f);
    a.tick(2000);
    TEST_ASSERT_EQUAL(12, unit.ram[0x2FC8]);
    TEST_ASSERT_EQUAL(1, unit.ram[0x2FE8]);

    unit.reset();
    unit.board(unit.profileFor("20002202")); // a board without the limitation addresses
    LimitControl u = freshLimit();
    TEST_ASSERT_FALSE(u.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, u.status);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// Every mapped board: the limit is written at that board's words and nothing at the first board's; a unit set
// to current limits reads the active limit from the board's own A float where it has one
void test_power_limit_on_every_mapped_board()
{
    int mapped = 0;
    for (size_t i = 1; i < CONTROL_PROFILE_COUNT; i++)
    {
        const ControlProfile &p = CONTROL_PROFILES[i];
        if (p.limit.ramEnable == 0)
            continue;
        mapped++;
        unit.reset();
        unit.board(p);
        LimitControl c = freshLimit();
        TEST_ASSERT_TRUE_MESSAGE(c.identify(1000), p.softwareId);
        TEST_ASSERT_EQUAL(LIMIT_POWER, c.kind);
        TEST_ASSERT_EQUAL_FLOAT(20.0f, c.limit);
        c.request(5.0f);
        c.tick(2000);
        TEST_ASSERT_EQUAL_FLOAT_MESSAGE(5.0f, unit.getFloat(p.limit.ramKw), p.softwareId);
        TEST_ASSERT_EQUAL(1, unit.ram[p.limit.ramEnable]);
        TEST_ASSERT_EQUAL(0, unit.ram[CONTROL_PROFILES[0].limit.ramEnable]); // nothing at the first board's addresses
        TEST_ASSERT_EQUAL_FLOAT(0.0f, unit.getFloat(CONTROL_PROFILES[0].limit.ramKw));
        unit.reset();
        unit.board(p);
        unit.ram[p.limit.ramKind] = 0; // current kind
        unit.setFloat(p.limit.ramActive, 20.0f);
        unit.setFloat(p.limit.ramActiveAmp ? p.limit.ramActiveAmp : p.limit.ramActive, 50.0f);
        LimitControl a = freshLimit();
        TEST_ASSERT_TRUE_MESSAGE(a.identify(1000), p.softwareId);
        TEST_ASSERT_EQUAL(LIMIT_CURRENT, a.kind);
        TEST_ASSERT_EQUAL_FLOAT(50.0f, a.active);
    }
    TEST_ASSERT_EQUAL(8, mapped);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_frame_bytes);
    RUN_TEST(test_identifies_the_unit);
    RUN_TEST(test_refuses_an_unknown_software);
    RUN_TEST(test_refuses_without_the_curve_mode);
    RUN_TEST(test_no_answer);
    RUN_TEST(test_applies_then_saves);
    RUN_TEST(test_reads_the_effective_target_after_a_write);
    RUN_TEST(test_saves_at_most_every_five_minutes);
    RUN_TEST(test_clamps_to_the_limits_of_the_unit);
    RUN_TEST(test_a_value_already_saved_is_not_written_again);
    RUN_TEST(test_stops_when_the_interface_leaves_the_curve_mode);
    RUN_TEST(test_reads_back_a_change_made_on_the_interface);
    RUN_TEST(test_leaving_water_identifies_with_its_own_curve);
    RUN_TEST(test_leaving_water_writes_its_own_words_and_bytes);
    RUN_TEST(test_every_other_software_writes_its_own_addresses);
    RUN_TEST(test_restores_the_original_curve);
    RUN_TEST(test_restore_from_the_saved_original_keeps_the_memory_interval);
    RUN_TEST(test_power_limit_applies_and_releases);
    RUN_TEST(test_power_limit_is_applied_again_after_the_unit_restarts);
    RUN_TEST(test_power_limit_restored_and_current_kind);
    RUN_TEST(test_power_limit_on_every_mapped_board);
    return UNITY_END();
}
