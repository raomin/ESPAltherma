#include <string.h>
#include <unity.h>

#include "control.h"

// Fake hydro board: answers the identification, memory and RAM commands of the X10A port on a small memory image.
struct FakeUnit
{
    uint8_t softwareId[4] = {0x04, 0x29, 0x00, 0x18}; // reads "18002904"
    uint8_t memory[1024];                               // the board's memory, by offset
    uint8_t ram[0x4000];                                // RAM, by address
    bool answer = true;
    int ramWrites = 0, memoryWrites = 0, badWrites = 0;
    uint16_t lastRamAddr = 0, lastMemOffset = 0;

    void reset()
    {
        const uint8_t id[4] = {0x04, 0x29, 0x00, 0x18};
        memcpy(softwareId, id, 4);
        memset(memory, 0, sizeof(memory));
        memset(ram, 0, sizeof(ram));
        answer = true;
        ramWrites = memoryWrites = badWrites = 0;
        // weather-dependent tank mode, curve 52 (low outdoor temperature) to 48 (high), limits 35+21 / 45+11 -> 45..55
        memory[278] = 0x01;
        memory[366] = (uint8_t)((48 - 35) << 2 | 0x01);
        memory[367] = (uint8_t)((52 - 45) << 2 | 0x03);
        const uint8_t limits[16] = {0xFF, 0x00, 0x15, 0x23, 0x08, 0xFF, 0x00, 0x0B, 0x2D, 0x08, 0xFF, 0x00, 0x1B, 0x1D, 0x08, 0xFF};
        memcpy(ram + 0x30C0, limits, 16);
        setWord(0x187A, 48 << 8);
        setWord(0x18B8, 52 << 8);
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
            lastRamAddr = addr;
            ramWrites++;
            if (addr != 0x187A && addr != 0x18B8)
                badWrites++;
            memcpy(ram + addr, data + 2, n - 2);
        }
        else if (cmd == 0x45 && n == 3)
        {
            uint16_t off = data[0] | data[1] << 8;
            lastMemOffset = off;
            memoryWrites++;
            if (off != 366 && off != 367)
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

static DhwControl fresh()
{
    DhwControl c;
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
    DhwControl c = fresh();
    TEST_ASSERT_TRUE(c.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_READY, c.status);
    TEST_ASSERT_EQUAL_STRING("18002904", c.softwareId);
    TEST_ASSERT_EQUAL(45, c.minTarget);
    TEST_ASSERT_EQUAL(55, c.maxTarget);
    TEST_ASSERT_EQUAL(48, c.target);
    TEST_ASSERT_EQUAL(52, c.targetLo);
    TEST_ASSERT_EQUAL(-1, c.saved); // the two points differ
    TEST_ASSERT_TRUE(c.changed);
    TEST_ASSERT_EQUAL(0, unit.ramWrites + unit.memoryWrites);
}

void test_refuses_an_unknown_software()
{
    unit.softwareId[0] = 0x05;
    DhwControl c = fresh();
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
    DhwControl c = fresh();
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
    DhwControl c = fresh();
    TEST_ASSERT_FALSE(c.identify(1000));
    TEST_ASSERT_EQUAL(CONTROL_NO_ANSWER, c.status);
    TEST_ASSERT_EQUAL(-1, c.target);
}

void test_applies_then_saves()
{
    DhwControl c = fresh();
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

void test_saves_at_most_every_five_minutes()
{
    DhwControl c = fresh();
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
    DhwControl c = fresh();
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
    DhwControl c = fresh();
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
    DhwControl c = fresh();
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
    DhwControl c = fresh();
    c.identify(1000);
    c.changed = false;
    unit.setWord(0x187A, 53 << 8);
    unit.setWord(0x18B8, 53 << 8);
    c.tick(1000 + CONTROL_READ_INTERVAL_MS);
    TEST_ASSERT_EQUAL(53, c.target);
    TEST_ASSERT_EQUAL(53, c.targetLo);
    TEST_ASSERT_TRUE(c.changed);
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
    RUN_TEST(test_saves_at_most_every_five_minutes);
    RUN_TEST(test_clamps_to_the_limits_of_the_unit);
    RUN_TEST(test_a_value_already_saved_is_not_written_again);
    RUN_TEST(test_stops_when_the_interface_leaves_the_curve_mode);
    RUN_TEST(test_reads_back_a_change_made_on_the_interface);
    return UNITY_END();
}
