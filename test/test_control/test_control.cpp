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
    bool hasL = true;      // the 2014-2017 boards have no 'L' command: registry 0x60 carries their software
    uint8_t reg60[17];     // registry 0x60 payload, the software word at bytes 15 and 14
    uint8_t memory[1024]; // the board's memory, by offset
    uint8_t ram[0x6000];  // RAM, by address
    bool answer = true;
    int ramWrites = 0, memoryWrites = 0, badWrites = 0;

    void reset()
    {
        const uint8_t id[4] = {0x04, 0x29, 0x00, 0x18}; // reads "18002904"
        memcpy(softwareId, id, 4);
        hasL = true;
        memset(reg60, 0, sizeof(reg60));
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
    void setU32(uint16_t addr, uint32_t v) { memcpy(ram + addr, &v, 4); }
    float getFloat(uint16_t addr) const { float v; memcpy(&v, ram + addr, 4); return v; }

    // The same unit on another hydro board software: the memory bytes are the same, every RAM address comes
    // from the profile (the limit records sit 5 bytes apart, in the order hot water A, B, leaving water B, A)
    void board(const ControlProfile &p)
    {
        hasL = strlen(p.softwareId) == 8;
        for (int i = 0; i < 4 && hasL; i++)
        {
            char hex[3] = {p.softwareId[2 * i], p.softwareId[2 * i + 1], 0};
            softwareId[3 - i] = (uint8_t)strtol(hex, nullptr, 16);
        }
        if (!hasL) // "76F2": xx = 76 at byte 15, yy = F2 at byte 14
        {
            char hi[3] = {p.softwareId[0], p.softwareId[1], 0}, lo[3] = {p.softwareId[2], p.softwareId[3], 0};
            reg60[15] = (uint8_t)strtol(hi, nullptr, 16);
            reg60[14] = (uint8_t)strtol(lo, nullptr, 16);
        }
        memset(ram, 0, sizeof(ram));
        if (p.dhw.ramA == 0) // mapped for the meter and the counters only
            return;
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
        if (cmd == 0x4C && n == 0 && hasL)
        {
            memcpy(reply + out, softwareId, 4);
            out += 4;
        }
        else if (cmd == 0x40 && n == 1 && data[0] == 0x60) // 40 60 L payload[L-2] crc
        {
            reply[out++] = 0x60;
            reply[out++] = (uint8_t)(sizeof(reg60) + 2);
            memcpy(reply + out, reg60, sizeof(reg60));
            out += sizeof(reg60);
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

// A heat pump just powered up (ESPAltherma with it, when the X10A port powers it) answers before its RAM holds the
// limits: tried again after half a minute, then a minute later, and ready as soon as the limits are there
void test_a_unit_just_powered_up_is_tried_again_soon()
{
    uint8_t limits[32];
    memcpy(limits, unit.ram + 0x30C0, sizeof(limits));
    memset(unit.ram + 0x30C0, 0, sizeof(limits)); // the limit records still empty
    CurveControl c = fresh();
    c.tick(1000);
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, c.status);
    c.tick(1000 + CONTROL_RETRY_FIRST_MS - 1);
    TEST_ASSERT_EQUAL(1000, c.checkedAt); // not yet
    c.tick(1000 + CONTROL_RETRY_FIRST_MS);
    TEST_ASSERT_EQUAL(1000 + CONTROL_RETRY_FIRST_MS, c.checkedAt);
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, c.status);
    memcpy(unit.ram + 0x30C0, limits, sizeof(limits)); // the board has loaded them
    c.tick(1000 + 3 * CONTROL_RETRY_FIRST_MS - 1);
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, c.status); // the second retry waits twice as long
    c.tick(1000 + 3 * CONTROL_RETRY_FIRST_MS);
    TEST_ASSERT_EQUAL(CONTROL_READY, c.status);
    TEST_ASSERT_EQUAL(CONTROL_RETRY_FIRST_MS, c.retryAfter); // the next loss of the unit starts short again
}

// A unit that never gets ready is not asked more often than before: the wait grows up to the check interval
void test_a_unit_never_ready_is_tried_at_the_check_interval_in_the_end()
{
    memset(unit.ram + 0x30C0, 0, 32);
    CurveControl c = fresh();
    unsigned long now = 1000;
    c.tick(now);
    for (int i = 0; i < 10; i++)
    {
        now += c.retryAfter;
        c.tick(now);
    }
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, c.status);
    TEST_ASSERT_EQUAL(CONTROL_CHECK_INTERVAL_MS, c.retryAfter);
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
        if (p.dhw.ramA == 0)
            continue; // the meter and the counters only
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

// RAM only: the target is written in the RAM and never in the board's memory, however long it stays
void test_ram_only_never_writes_the_memory()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(50);
    c.tick(2000);
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(-1, (int)c.wanted); // done at once: nothing waits for a save
    TEST_ASSERT_EQUAL(50, c.held);
    c.request(53);
    c.tick(3000);
    c.tick(3000 + CONTROL_SAVE_DELAY_MS + CONTROL_SAVE_INTERVAL_MS);
    TEST_ASSERT_EQUAL(53 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(53, c.held);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    TEST_ASSERT_EQUAL((48 - 35) << 2 | 0x01, unit.memory[366]); // the unit's own curve, untouched
    TEST_ASSERT_EQUAL((52 - 45) << 2 | 0x03, unit.memory[367]);
    TEST_ASSERT_EQUAL(48, c.origA);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// A restart of the heat pump reloads its curve from the memory: the held target is written again at the next
// read-back, without capturing the reloaded curve as a new original
void test_ram_only_writes_the_target_again_after_the_unit_restarts()
{
    CurveControl c = fresh(CONTROL_LWT);
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(38);
    c.tick(2000);
    TEST_ASSERT_EQUAL(38 << 8, unit.word(0x187C));
    TEST_ASSERT_EQUAL(38 << 8, unit.word(0x18BA));
    c.origChanged = false;
    unit.setWord(0x187C, 36 << 8); // the unit restarted: its curve from the memory
    unit.setWord(0x18BA, 40 << 8);
    int writes = unit.ramWrites;
    c.tick(2000 + CONTROL_READ_INTERVAL_MS - 1);
    TEST_ASSERT_EQUAL(writes, unit.ramWrites); // not read back yet
    c.tick(2000 + CONTROL_READ_INTERVAL_MS);   // read back: the target is gone
    c.tick(2000 + CONTROL_READ_INTERVAL_MS + 10);
    TEST_ASSERT_EQUAL(38 << 8, unit.word(0x187C));
    TEST_ASSERT_EQUAL(38 << 8, unit.word(0x18BA));
    TEST_ASSERT_EQUAL(writes + 2, unit.ramWrites);
    TEST_ASSERT_EQUAL(36, c.origA); // still the curve from before the first write
    TEST_ASSERT_EQUAL(40, c.origB);
    TEST_ASSERT_FALSE(c.origChanged);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// A restore puts the curve back in the RAM; the memory still holds it, so it is not written, and the hold ends
void test_ram_only_restore_releases_without_writing_the_memory()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(50);
    c.tick(2000);
    c.requestRestore();
    c.tick(3000);
    TEST_ASSERT_EQUAL(48 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(52 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(-1, c.held);
    TEST_ASSERT_EQUAL(-1, c.origA);
    TEST_ASSERT_FALSE(c.restoring);
    int writes = unit.ramWrites;
    c.tick(3000 + CONTROL_READ_INTERVAL_MS); // read back, nothing held: nothing written
    c.tick(3000 + CONTROL_READ_INTERVAL_MS + 10);
    TEST_ASSERT_EQUAL(writes, unit.ramWrites);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// Unticking RAM only saves the held target in the unit's memory, like a fresh request, and ends the hold
void test_ram_only_off_saves_the_held_target()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(50);
    c.tick(2000);
    TEST_ASSERT_EQUAL(50, c.held);
    TEST_ASSERT_TRUE(c.heldChanged); // the caller persists it
    c.heldChanged = false;
    c.ramOnly = false;
    c.tick(2500);
    TEST_ASSERT_EQUAL(-1, c.held);
    TEST_ASSERT_TRUE(c.heldChanged);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites); // after the usual delay
    c.tick(2500 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(2, unit.memoryWrites);
    TEST_ASSERT_EQUAL(50, c.saved);
    TEST_ASSERT_EQUAL((50 - 35) << 2 | 0x01, unit.memory[366]);
    TEST_ASSERT_EQUAL((50 - 45) << 2 | 0x03, unit.memory[367]);
    unit.setWord(0x187A, 48 << 8); // a later RAM change is not chased any more
    unit.setWord(0x18B8, 52 << 8);
    int writes = unit.ramWrites;
    c.tick(2500 + CONTROL_SAVE_DELAY_MS + CONTROL_READ_INTERVAL_MS);
    c.tick(2500 + CONTROL_SAVE_DELAY_MS + CONTROL_READ_INTERVAL_MS + 10);
    TEST_ASSERT_EQUAL(writes, unit.ramWrites);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// Leaving the weather-dependent mode ends the hold: back in that mode later, the old target does not return
void test_ram_only_hold_ends_when_the_curve_mode_is_left()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(50);
    c.tick(2000);
    TEST_ASSERT_EQUAL(50, c.held);
    unit.memory[278] = 0x00; // absolute setpoint on the interface
    c.tick(1000 + CONTROL_CHECK_INTERVAL_MS);
    TEST_ASSERT_EQUAL(CONTROL_NOT_CURVE_MODE, c.status);
    TEST_ASSERT_EQUAL(-1, c.held);
    unit.memory[278] = 0x01; // weather dependent again, with the unit's curve in RAM
    unit.setWord(0x187A, 48 << 8);
    unit.setWord(0x18B8, 52 << 8);
    int writes = unit.ramWrites;
    c.tick(1000 + 2 * CONTROL_CHECK_INTERVAL_MS);
    TEST_ASSERT_EQUAL(CONTROL_READY, c.status);
    c.tick(1000 + 2 * CONTROL_CHECK_INTERVAL_MS + CONTROL_READ_INTERVAL_MS);
    c.tick(1000 + 2 * CONTROL_CHECK_INTERVAL_MS + CONTROL_READ_INTERVAL_MS + 10);
    TEST_ASSERT_EQUAL(writes, unit.ramWrites);
    TEST_ASSERT_EQUAL(48 << 8, unit.word(0x187A));
}

// No answer is what a restart of the heat pump looks like: the hold survives it and the target comes back
void test_ram_only_hold_survives_a_unit_that_does_not_answer()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(50);
    c.tick(2000);
    unit.answer = false;
    c.tick(1000 + CONTROL_CHECK_INTERVAL_MS);
    TEST_ASSERT_EQUAL(CONTROL_NO_ANSWER, c.status);
    TEST_ASSERT_EQUAL(50, c.held);
    unit.answer = true; // back, with its curve from the memory
    unit.setWord(0x187A, 48 << 8);
    unit.setWord(0x18B8, 52 << 8);
    c.tick(1000 + 2 * CONTROL_CHECK_INTERVAL_MS);
    TEST_ASSERT_EQUAL(CONTROL_READY, c.status);
    c.tick(1000 + 2 * CONTROL_CHECK_INTERVAL_MS + 10); // the lost target is seen
    c.tick(1000 + 2 * CONTROL_CHECK_INTERVAL_MS + 20); // and written again
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
}

// Another hydro board software (a board replaced, a firmware update): the held target belonged to the old one
void test_ram_only_hold_ends_with_another_software()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.request(50);
    c.tick(2000);
    unit.board(FakeUnit::profileFor("20002203"));
    c.tick(1000 + CONTROL_CHECK_INTERVAL_MS);
    TEST_ASSERT_EQUAL_STRING("20002203", c.softwareId);
    TEST_ASSERT_EQUAL(-1, c.held);
    TEST_ASSERT_TRUE(c.heldChanged);
}

// A hold given back by the caller after an ESP restart is written again when the unit lost it
void test_ram_only_hold_from_the_saved_configuration()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000));
    c.held = 50; // as the caller does from the configuration
    c.tick(1000 + CONTROL_READ_INTERVAL_MS);
    c.tick(1000 + CONTROL_READ_INTERVAL_MS + 10);
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// A held target outside the unit's limits (narrowed since, or a hand-edited configuration) is written clamped,
// and the hold follows the clamped value
void test_ram_only_hold_is_written_within_the_limits()
{
    CurveControl c = fresh();
    c.ramOnly = true;
    TEST_ASSERT_TRUE(c.identify(1000)); // hot water: 45..55
    c.held = 60;
    c.tick(1000 + CONTROL_READ_INTERVAL_MS);
    c.tick(1000 + CONTROL_READ_INTERVAL_MS + 10);
    TEST_ASSERT_EQUAL(55 << 8, unit.word(0x187A));
    TEST_ASSERT_EQUAL(55 << 8, unit.word(0x18B8));
    TEST_ASSERT_EQUAL(55, c.held);
    TEST_ASSERT_TRUE(c.heldChanged); // the caller saves the clamped value
    int writes = unit.ramWrites;
    c.tick(1000 + 2 * CONTROL_READ_INTERVAL_MS + 10); // read back: the RAM holds it, nothing more to write
    c.tick(1000 + 2 * CONTROL_READ_INTERVAL_MS + 20);
    TEST_ASSERT_EQUAL(writes, unit.ramWrites);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// RAM only: the leaving water range is the cold point's ([9-01]..[9-00], 29..55 here) instead of the mild point's
// 45, a rule of the interface for that setting which the memory never gets; the hot water range is unchanged.
// Unticking the box saves the held target within the memory's range.
void test_ram_only_opens_the_leaving_water_range_to_the_units_own()
{
    CurveControl c = fresh(CONTROL_LWT);
    TEST_ASSERT_TRUE(c.identify(1000));
    TEST_ASSERT_EQUAL(45, c.maxTarget);
    c.setRamOnly(true, 1500);
    TEST_ASSERT_EQUAL(29, c.minTarget);
    TEST_ASSERT_EQUAL(55, c.maxTarget);
    TEST_ASSERT_TRUE(c.changed);
    c.request(50);
    c.tick(2000);
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x187C));
    TEST_ASSERT_EQUAL(50 << 8, unit.word(0x18BA));
    TEST_ASSERT_EQUAL(50, c.held);
    TEST_ASSERT_EQUAL(0, unit.memoryWrites);
    c.setRamOnly(false, 3000); // back to the intersection: the held 50 is saved as 45
    TEST_ASSERT_EQUAL(45, c.maxTarget);
    c.tick(3100);
    c.tick(3100 + CONTROL_SAVE_DELAY_MS);
    TEST_ASSERT_EQUAL(-1, c.held);
    TEST_ASSERT_EQUAL(45 << 8, unit.word(0x187C));
    TEST_ASSERT_EQUAL(45 << 8, unit.word(0x18BA));
    TEST_ASSERT_EQUAL(45, c.saved);
    TEST_ASSERT_EQUAL(0, unit.badWrites);
    CurveControl d = fresh();
    d.ramOnly = true;
    TEST_ASSERT_TRUE(d.identify(1000)); // hot water: unchanged
    TEST_ASSERT_EQUAL(45, d.minTarget);
    TEST_ASSERT_EQUAL(55, d.maxTarget);
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
    unit.board(unit.profileFor("19016505")); // a board without the limitation addresses (mapped for its counters only)
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
        if (p.limit.ramActive)
        {
            unit.setFloat(p.limit.ramActive, 20.0f);
            unit.setFloat(p.limit.ramActiveAmp ? p.limit.ramActiveAmp : p.limit.ramActive, 50.0f);
        }
        LimitControl a = freshLimit();
        TEST_ASSERT_TRUE_MESSAGE(a.identify(1000), p.softwareId);
        TEST_ASSERT_EQUAL(LIMIT_CURRENT, a.kind);
        TEST_ASSERT_EQUAL_FLOAT(p.limit.ramActive ? 50.0f : -1.0f, a.active); // unknown where not mapped (20002202)
    }
    TEST_ASSERT_EQUAL(11, mapped); // 20017701 and 20002202 from their owners' dumps and reads included
    TEST_ASSERT_EQUAL(0, unit.badWrites);
}

// The 2014-2017 boards have no 'L' command: the software comes from registry 0x60, and the profile keyed on that
// word drives both controls. A board that answers neither is "no answer", as before.
void test_a_board_without_the_L_command_is_identified_by_registry_0x60()
{
    const ControlProfile &p = FakeUnit::profileFor("76F2");
    TEST_ASSERT_EQUAL_STRING("76F2", p.softwareId);
    unit.board(p);
    CurveControl c = fresh();
    TEST_ASSERT_TRUE(c.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_READY, c.status);
    TEST_ASSERT_EQUAL_STRING("76F2", c.softwareId);
    TEST_ASSERT_EQUAL(48, c.target);
    LimitControl l = freshLimit();
    TEST_ASSERT_TRUE(l.identify(1000));
    TEST_ASSERT_EQUAL_STRING("76F2", l.softwareId);
    TEST_ASSERT_EQUAL_FLOAT(20.0f, l.limit);
    // the word of an unknown software: unsupported, not "no answer"
    unit.reg60[15] = 0x45;
    unit.reg60[14] = 0x62;
    CurveControl u = fresh();
    TEST_ASSERT_FALSE(u.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, u.status);
    TEST_ASSERT_EQUAL_STRING("4562", u.softwareId);
}

// The unit's own energy meter where the profile knows its address: a 32-bit count read once a minute, unknown
// after a failed read until the next one, not offered on the boards without the address. Nothing written.
void test_the_unit_energy_meter_is_read_where_mapped()
{
    unit.board(FakeUnit::profileFor("76F2"));
    const uint8_t count[4] = {0xEC, 0x34, 0x01, 0x00}; // 79084, little-endian
    memcpy(unit.ram + 0x084E, count, 4);
    MeterReader m;
    m.exchange = fakeExchange;
    m.tick(1000);
    TEST_ASSERT_EQUAL(CONTROL_READY, m.status);
    TEST_ASSERT_EQUAL_STRING("76F2", m.softwareId);
    TEST_ASSERT_TRUE(m.known);
    TEST_ASSERT_EQUAL_UINT32(79084, m.heatKWh);
    unit.ram[0x084E] = 0xED; // 79085
    m.tick(30000);           // not yet
    TEST_ASSERT_EQUAL_UINT32(79084, m.heatKWh);
    m.tick(61000);
    TEST_ASSERT_EQUAL_UINT32(79085, m.heatKWh);
    unit.answer = false;
    m.tick(122000);
    TEST_ASSERT_FALSE(m.known);
    TEST_ASSERT_EQUAL(CONTROL_READY, m.status);
    TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
    // a board without a meter address: nothing else read
    unit.reset();
    unit.board(FakeUnit::profileFor("20017701"));
    MeterReader u;
    u.exchange = fakeExchange;
    u.tick(1000);
    TEST_ASSERT_EQUAL(CONTROL_UNSUPPORTED, u.status);
    TEST_ASSERT_EQUAL_STRING("20017701", u.softwareId);
    TEST_ASSERT_FALSE(u.known);
}

// Every software mapped for the meter reads it and its counters at its own addresses, 32 bits each; a software
// mapped for the meter only has no counters
void test_every_mapped_software_reads_its_meter_and_counters()
{
    int mapped = 0;
    for (size_t i = 0; i < CONTROL_PROFILE_COUNT; i++)
    {
        const ControlProfile &p = CONTROL_PROFILES[i];
        if (p.ramHeatKwh == 0)
            continue;
        mapped++;
        unit.reset();
        unit.board(p);
        unit.setU32(p.ramHeatKwh, 70000 + i);
        for (size_t k = 0; k < UNIT_COUNTER_COUNT; k++)
            if (unitCounterAddress(p.counters, k))
                unit.setU32(unitCounterAddress(p.counters, k), 65536 * (k + 1) + i); // beyond 16 bits
        MeterReader m;
        m.exchange = fakeExchange;
        m.tick(1000);
        TEST_ASSERT_EQUAL_MESSAGE(CONTROL_READY, m.status, p.softwareId);
        TEST_ASSERT_EQUAL_STRING(p.softwareId, m.softwareId);
        TEST_ASSERT_EQUAL_UINT32(70000 + i, m.heatKWh);
        for (size_t k = 0; k < UNIT_COUNTER_COUNT; k++)
        {
            TEST_ASSERT_EQUAL_MESSAGE(unitCounterAddress(p.counters, k) != 0, m.counterMapped(k), p.softwareId);
            if (m.counterMapped(k))
            {
                uint32_t raw = 65536 * (k + 1) + i, expected = 0;
                TEST_ASSERT_TRUE(unitCounterDecode(UNIT_COUNTER_DEFS[k].kind, (const uint8_t *)&raw, expected));
                TEST_ASSERT_TRUE_MESSAGE(m.counterKnown[k], p.softwareId);
                TEST_ASSERT_EQUAL_UINT32(expected, m.counter[k]);
            }
            else
                TEST_ASSERT_FALSE(m.counterKnown[k]);
        }
        TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
    }
    TEST_ASSERT_EQUAL(14, mapped); // the Hybrid 5190 included
}

// 20002203: the heat produced, the compressor and pump hours and the compressor starts of the interface, 32-bit
// counts read together once a minute. Nothing written.
static MeterReader meterOn20002203()
{
    unit.board(FakeUnit::profileFor("20002203"));
    unit.setU32(0x08E6, 18720);
    unit.setU32(0x36B2, 3240); // compressor hours, space heating
    unit.setU32(0x3696, 251);  // space cooling
    unit.setU32(0x3650, 691);  // hot water
    unit.setU32(0x3692, 4552); // pump
    unit.setU32(0x3752, 2751); // compressor starts
    unit.setFloat(0x36C2, 12940.6f); // heat produced, space heating (kWh)
    unit.setFloat(0x3664, 4881.8f);  // hot water
    unit.setFloat(0x36A6, 915.0f);   // cooling
    unit.setU32(0x08E2, 4272);       // electricity used, all modes (kWh)
    unit.setU32(0x3722, 2708262);    // space heating (Wh)
    unit.setU32(0x3702, 1362763);    // hot water
    unit.setU32(0x36FA, 153516);     // cooling
    unit.setU32(0x371A, 47093);      // backup heater
    MeterReader m;
    m.exchange = fakeExchange;
    return m;
}

void test_the_unit_counters_are_read_with_the_meter()
{
    MeterReader m = meterOn20002203();
    m.tick(1000);
    TEST_ASSERT_EQUAL(CONTROL_READY, m.status);
    TEST_ASSERT_EQUAL_UINT32(18720, m.heatKWh);
    TEST_ASSERT_EQUAL_STRING("Unit meter: Compressor hours, space heating (h)", UNIT_COUNTER_DEFS[0].label);
    for (size_t i = 0; i < UNIT_COUNTER_COUNT; i++)
        TEST_ASSERT_TRUE(m.counterMapped(i) && m.counterKnown[i]);
    TEST_ASSERT_EQUAL_UINT32(3240, m.counter[0]);
    TEST_ASSERT_EQUAL_UINT32(251, m.counter[1]);
    TEST_ASSERT_EQUAL_UINT32(691, m.counter[2]);
    TEST_ASSERT_EQUAL_UINT32(4552, m.counter[3]);
    TEST_ASSERT_EQUAL_UINT32(2751, m.counter[4]);
    TEST_ASSERT_EQUAL(UNIT_COUNT, UNIT_COUNTER_DEFS[4].kind);
    unit.answer = false;
    m.tick(62000);
    TEST_ASSERT_FALSE(m.counterKnown[0]); // unknown until the next read
    TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
}

// A heat pump just powered up answers before it has loaded its counts: they read 0. Nothing is published (a
// lifetime total at 0 and back would count twice in Home Assistant's statistics); the unit is tried again a minute
// later, and ready once the counts are there.
void test_a_unit_just_powered_up_publishes_no_zero()
{
    MeterReader m = meterOn20002203();
    unit.setU32(0x08E6, 0);
    unit.setU32(0x36B2, 0);
    m.tick(1000);
    TEST_ASSERT_EQUAL(CONTROL_NO_ANSWER, m.status);
    TEST_ASSERT_FALSE(m.known);
    TEST_ASSERT_FALSE(m.counterKnown[0]);
    m.tick(1000 + METER_READ_INTERVAL_MS - 1);
    TEST_ASSERT_EQUAL(CONTROL_NO_ANSWER, m.status); // not tried yet
    unit.setU32(0x08E6, 18720); // loaded
    unit.setU32(0x36B2, 3240);
    m.tick(1000 + METER_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL(CONTROL_READY, m.status);
    TEST_ASSERT_EQUAL_UINT32(18720, m.heatKWh);
    TEST_ASSERT_EQUAL_UINT32(3240, m.counter[0]);
    // the unit restarts while ESPAltherma runs on: its counts read 0 again, unknown, not 0
    unit.setU32(0x08E6, 0);
    m.tick(1000 + 2 * METER_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL(CONTROL_READY, m.status);
    TEST_ASSERT_FALSE(m.known);
    TEST_ASSERT_FALSE(m.counterKnown[0]);
}

// A lifetime count read lower than one already seen (a glitch) keeps the higher one
void test_lifetime_counts_never_go_back()
{
    MeterReader m = meterOn20002203();
    m.tick(1000);
    TEST_ASSERT_EQUAL(CONTROL_READY, m.status);
    unit.setU32(0x3752, 2750); // lower than the 2751 seen
    unit.setU32(0x08E6, 18719);
    m.tick(1000 + METER_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(2751, m.counter[4]);
    TEST_ASSERT_TRUE(m.counterKnown[4]);
    TEST_ASSERT_EQUAL_UINT32(18720, m.heatKWh);
    TEST_ASSERT_TRUE(m.known);
    unit.setU32(0x3752, 2752);
    m.tick(1000 + 2 * METER_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL_UINT32(2752, m.counter[4]);
}

// 20002203: the heat and the electricity of each mode, read with the meter and kept in Wh: the heat floats to the
// Wh, the electricity counts as they are, the total in whole kWh. Lifetime totals like the others.
void test_the_energy_of_each_mode_is_read_on_20002203()
{
    MeterReader m = meterOn20002203();
    m.tick(1000);
    TEST_ASSERT_EQUAL(CONTROL_READY, m.status);
    for (size_t i = 5; i < UNIT_COUNTER_COUNT; i++)
    {
        TEST_ASSERT_TRUE(m.counterMapped(i) && m.counterKnown[i]);
        TEST_ASSERT_TRUE(unitCounterIsEnergy(UNIT_COUNTER_DEFS[i].kind));
    }
    TEST_ASSERT_UINT32_WITHIN(1, 12940600, m.counter[5]);
    TEST_ASSERT_UINT32_WITHIN(1, 4881800, m.counter[6]);
    TEST_ASSERT_EQUAL_UINT32(915000, m.counter[7]);
    TEST_ASSERT_EQUAL_UINT32(4272000, m.counter[8]);
    TEST_ASSERT_EQUAL_UINT32(2708262, m.counter[9]);
    TEST_ASSERT_EQUAL_UINT32(1362763, m.counter[10]);
    TEST_ASSERT_EQUAL_UINT32(153516, m.counter[11]);
    TEST_ASSERT_EQUAL_UINT32(47093, m.counter[12]);
    TEST_ASSERT_EQUAL_STRING("Unit meter: Electricity used, backup heater (kWh)", UNIT_COUNTER_DEFS[12].label);
    // a float that is not a count: unknown, the last value kept
    unit.setU32(0x3664, 0x7FC00000); // NaN
    unit.setFloat(0x36A6, -1.0f);
    m.tick(1000 + METER_READ_INTERVAL_MS);
    TEST_ASSERT_FALSE(m.counterKnown[6]);
    TEST_ASSERT_FALSE(m.counterKnown[7]);
    TEST_ASSERT_EQUAL_UINT32(915000, m.counter[7]);
    TEST_ASSERT_TRUE(m.counterKnown[5]);
    // the other softwares: not mapped
    for (size_t p = 0; p < CONTROL_PROFILE_COUNT; p++)
        if (strcmp(CONTROL_PROFILES[p].softwareId, "20002203") != 0)
            for (size_t i = 5; i < UNIT_COUNTER_COUNT; i++)
                TEST_ASSERT_EQUAL_UINT16(0, unitCounterAddress(CONTROL_PROFILES[p].counters, i));
    TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_frame_bytes);
    RUN_TEST(test_identifies_the_unit);
    RUN_TEST(test_refuses_an_unknown_software);
    RUN_TEST(test_refuses_without_the_curve_mode);
    RUN_TEST(test_no_answer);
    RUN_TEST(test_a_unit_just_powered_up_is_tried_again_soon);
    RUN_TEST(test_a_unit_never_ready_is_tried_at_the_check_interval_in_the_end);
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
    RUN_TEST(test_ram_only_never_writes_the_memory);
    RUN_TEST(test_ram_only_writes_the_target_again_after_the_unit_restarts);
    RUN_TEST(test_ram_only_restore_releases_without_writing_the_memory);
    RUN_TEST(test_ram_only_off_saves_the_held_target);
    RUN_TEST(test_ram_only_hold_ends_when_the_curve_mode_is_left);
    RUN_TEST(test_ram_only_hold_survives_a_unit_that_does_not_answer);
    RUN_TEST(test_ram_only_hold_ends_with_another_software);
    RUN_TEST(test_ram_only_hold_from_the_saved_configuration);
    RUN_TEST(test_ram_only_hold_is_written_within_the_limits);
    RUN_TEST(test_ram_only_opens_the_leaving_water_range_to_the_units_own);
    RUN_TEST(test_power_limit_applies_and_releases);
    RUN_TEST(test_power_limit_is_applied_again_after_the_unit_restarts);
    RUN_TEST(test_power_limit_restored_and_current_kind);
    RUN_TEST(test_power_limit_on_every_mapped_board);
    RUN_TEST(test_a_board_without_the_L_command_is_identified_by_registry_0x60);
    RUN_TEST(test_the_unit_energy_meter_is_read_where_mapped);
    RUN_TEST(test_every_mapped_software_reads_its_meter_and_counters);
    RUN_TEST(test_the_unit_counters_are_read_with_the_meter);
    RUN_TEST(test_a_unit_just_powered_up_publishes_no_zero);
    RUN_TEST(test_lifetime_counts_never_go_back);
    RUN_TEST(test_the_energy_of_each_mode_is_read_on_20002203);
    return UNITY_END();
}
