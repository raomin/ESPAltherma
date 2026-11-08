#include <math.h>
#include <string.h>
#include <unity.h>

#include "onewire_temp.h"

// A fake 1-Wire bus with DS18B20s: answers the search, the conversion and the scratchpad reads
struct FakeSensor
{
    uint8_t rom[8];
    float celsius;
    bool present = true;
    bool corrupt = false;   // the next scratchpad has a wrong CRC
    bool powerOn = false;   // the scratchpad still holds the power-on value (85 °C)
    bool converted = false;
};

struct FakeBus : OneWireBus
{
    FakeSensor *sensors[8];
    size_t count = 0;
    size_t searchAt = 0;
    FakeSensor *selected = nullptr;
    uint8_t pad[9];
    int padAt = 9;
    bool lastWasSkip = false;
    int conversions = 0;

    void add(FakeSensor *s) { sensors[count++] = s; }

    bool reset() override
    {
        selected = nullptr;
        lastWasSkip = false;
        for (size_t i = 0; i < count; i++)
            if (sensors[i]->present)
                return true;
        return false;
    }
    void skip() override { lastWasSkip = true; }
    void select(const uint8_t rom[8]) override
    {
        for (size_t i = 0; i < count; i++)
            if (sensors[i]->present && memcmp(sensors[i]->rom, rom, 8) == 0)
                selected = sensors[i];
    }
    void write(uint8_t b) override
    {
        if (b == ONEWIRE_CMD_CONVERT && lastWasSkip)
        {
            conversions++;
            for (size_t i = 0; i < count; i++)
                sensors[i]->converted = true;
        }
        else if (b == ONEWIRE_CMD_READ_SCRATCHPAD)
        {
            memset(pad, 0xFF, sizeof(pad)); // nobody answering: the pull-up reads ones
            padAt = 0;
            if (selected == nullptr)
                return;
            int16_t raw = selected->powerOn ? ONEWIRE_POWER_ON_RAW : (int16_t)lroundf(selected->celsius * 16);
            pad[0] = (uint8_t)raw;
            pad[1] = (uint8_t)(raw >> 8);
            pad[2] = 0x4B;
            pad[3] = 0x46;
            pad[4] = 0x7F; // 12 bits
            pad[5] = 0xFF;
            pad[6] = 0x0C;
            pad[7] = 0x10;
            pad[8] = oneWireCrc8(pad, 8);
            if (selected->corrupt)
                pad[8] ^= 0x55;
        }
    }
    uint8_t read() override { return padAt < 9 ? pad[padAt++] : 0xFF; }
    void resetSearch() override { searchAt = 0; }
    bool search(uint8_t rom[8]) override
    {
        while (searchAt < count && !sensors[searchAt]->present)
            searchAt++;
        if (searchAt >= count)
            return false;
        memcpy(rom, sensors[searchAt++]->rom, 8);
        return true;
    }
};

static void makeRom(uint8_t rom[8], uint8_t family, uint8_t serial)
{
    rom[0] = family;
    for (int i = 1; i < 7; i++)
        rom[i] = (uint8_t)(serial + i);
    rom[7] = oneWireCrc8(rom, 7);
}

static FakeBus busA, busB;
static FakeSensor top, bottom, floorLoop;

void setUp()
{
    busA = FakeBus();
    busB = FakeBus();
    top = FakeSensor();
    bottom = FakeSensor();
    floorLoop = FakeSensor();
    makeRom(top.rom, ONEWIRE_FAMILY_DS18B20, 0x10);
    makeRom(bottom.rom, ONEWIRE_FAMILY_DS18B20, 0x20);
    makeRom(floorLoop.rom, ONEWIRE_FAMILY_DS1822, 0x30);
    top.celsius = 54.3125f;
    bottom.celsius = 31.5f;
    floorLoop.celsius = -2.25f;
    busA.add(&top);
    busA.add(&bottom);
    busB.add(&floorLoop);
}
void tearDown() {}

static OneWireTemps twoBuses()
{
    OneWireTemps t;
    OneWireBus *list[2] = {&busA, &busB};
    t.attach(list, 2);
    return t;
}

void test_crc8_of_a_known_rom()
{
    // ROM code of a real DS18B20 from the Maxim application note 27 example
    const uint8_t rom[8] = {0x02, 0x1C, 0xB8, 0x01, 0x00, 0x00, 0x00, 0xA2};
    TEST_ASSERT_EQUAL_HEX8(0xA2, oneWireCrc8(rom, 7));
}

void test_rom_hex_round_trip()
{
    char hex[17];
    uint8_t back[8];
    oneWireRomToHex(top.rom, hex);
    TEST_ASSERT_EQUAL(16, strlen(hex));
    TEST_ASSERT_TRUE(oneWireHexToRom(hex, back));
    TEST_ASSERT_EQUAL_UINT8_ARRAY(top.rom, back, 8);
    TEST_ASSERT_FALSE(oneWireHexToRom("28FF", back));
    TEST_ASSERT_FALSE(oneWireHexToRom("28FF641E8216034G", back));
}

void test_finds_the_sensors_of_every_bus_and_skips_other_families()
{
    FakeSensor other;
    makeRom(other.rom, 0x01, 0x40); // a DS2401 serial number chip
    busA.add(&other);
    OneWireTemps t = twoBuses();
    t.tick(0);
    TEST_ASSERT_EQUAL(3, t.count);
    TEST_ASSERT_EQUAL(0, t.sensors[0].bus);
    TEST_ASSERT_EQUAL(0, t.sensors[1].bus);
    TEST_ASSERT_EQUAL(1, t.sensors[2].bus);
    TEST_ASSERT_TRUE(t.changed);
}

void test_reads_after_the_conversion_without_waiting_in_the_loop()
{
    OneWireTemps t = twoBuses();
    t.tick(0);
    TEST_ASSERT_EQUAL(1, busA.conversions); // one conversion for the whole bus
    TEST_ASSERT_EQUAL(1, busB.conversions);
    TEST_ASSERT_FALSE(t.sensors[0].valid);
    t.tick(ONEWIRE_CONVERSION_MS - 1);
    TEST_ASSERT_FALSE(t.sensors[0].valid);
    t.tick(ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_TRUE(t.sensors[0].valid);
    TEST_ASSERT_EQUAL_FLOAT(54.3125f, t.sensors[0].celsius);
    TEST_ASSERT_EQUAL_FLOAT(31.5f, t.sensors[1].celsius);
    TEST_ASSERT_EQUAL_FLOAT(-2.25f, t.sensors[2].celsius); // negative values
    t.tick(t.interval - 1);
    TEST_ASSERT_EQUAL(1, busA.conversions);
    t.tick(t.interval);
    TEST_ASSERT_EQUAL(2, busA.conversions);
}

// The place of a sensor in the drawing comes from the settings; one without a name keeps its ROM code as label
void test_a_place_without_a_name()
{
    OneWireTemps t = twoBuses();
    OneWireName names[1] = {};
    oneWireRomToHex(bottom.rom, names[0].rom);
    strcpy(names[0].role, "tank0");
    t.setNames(names, 1);
    t.tick(0);
    TEST_ASSERT_EQUAL_STRING("tank0", t.roleFor(bottom.rom));
    TEST_ASSERT_EQUAL_STRING("", t.roleFor(top.rom));
    TEST_ASSERT_NULL(t.nameFor(bottom.rom));
    char hex[17], expected[40];
    oneWireRomToHex(bottom.rom, hex);
    snprintf(expected, sizeof(expected), "DS18B20: %s", hex);
    TEST_ASSERT_EQUAL_STRING(expected, t.sensors[1].label);
}

void test_only_plain_places_are_kept()
{
    TEST_ASSERT_TRUE(oneWireValidRole("tank10"));
    TEST_ASSERT_TRUE(oneWireValidRole("hp_flow"));
    TEST_ASSERT_TRUE(oneWireValidRole(""));
    TEST_ASSERT_FALSE(oneWireValidRole("Tank"));
    TEST_ASSERT_FALSE(oneWireValidRole("a\"b"));
    TEST_ASSERT_FALSE(oneWireValidRole("much_too_long_role"));
}

void test_names_from_the_settings_and_the_rom_code_otherwise()
{
    OneWireTemps t = twoBuses();
    OneWireName names[2];
    oneWireRomToHex(top.rom, names[0].rom);
    strcpy(names[0].name, "Tank \"top\"");
    oneWireRomToHex(floorLoop.rom, names[1].rom);
    strcpy(names[1].name, "Floor loop");
    t.setNames(names, 2);
    t.tick(0);
    t.tick(ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_EQUAL_STRING("DS18B20: Tank  top ", t.sensors[0].label); // no quote in a JSON key or template
    char hex[17], expected[40];
    oneWireRomToHex(bottom.rom, hex);
    snprintf(expected, sizeof(expected), "DS18B20: %s", hex);
    TEST_ASSERT_EQUAL_STRING(expected, t.sensors[1].label);
    TEST_ASSERT_EQUAL_STRING("DS18B20: Floor loop", t.sensors[2].label);
    char json[512] = "{";
    t.appendJson(json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"DS18B20: Tank  top \":54.31,"));
    TEST_ASSERT_NOT_NULL(strstr(json, "\"DS18B20: Floor loop\":-2.25,"));
}

void test_a_bad_crc_keeps_the_last_reading_then_null_after_three_cycles()
{
    OneWireTemps t = twoBuses();
    t.tick(0);
    t.tick(ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_EQUAL_FLOAT(54.3125f, t.sensors[0].celsius);
    top.corrupt = true;
    top.celsius = 60;
    unsigned long now = 0;
    for (int cycle = 1; cycle <= 2; cycle++)
    {
        now = cycle * t.interval;
        t.tick(now);
        t.tick(now + ONEWIRE_CONVERSION_MS);
        TEST_ASSERT_TRUE(t.sensors[0].valid);
        TEST_ASSERT_EQUAL_FLOAT(54.3125f, t.sensors[0].celsius);
    }
    now = 3 * t.interval;
    t.tick(now);
    t.tick(now + ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_FALSE(t.sensors[0].valid);
    char json[512] = "{";
    t.appendJson(json, sizeof(json));
    TEST_ASSERT_NOT_NULL(strstr(json, ":null,"));
    top.corrupt = false;
    now = 4 * t.interval;
    t.tick(now);
    t.tick(now + ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_TRUE(t.sensors[0].valid);
    TEST_ASSERT_EQUAL_FLOAT(60.0f, t.sensors[0].celsius);
}

void test_a_sensor_unplugged_reads_null_and_comes_back()
{
    OneWireTemps t = twoBuses();
    t.tick(0);
    t.tick(ONEWIRE_CONVERSION_MS);
    bottom.present = false;
    for (int cycle = 1; cycle <= 3; cycle++)
    {
        t.tick(cycle * t.interval);
        t.tick(cycle * t.interval + ONEWIRE_CONVERSION_MS);
    }
    TEST_ASSERT_FALSE(t.sensors[1].valid);
    TEST_ASSERT_TRUE(t.sensors[0].valid); // the others go on
    bottom.present = true;
    t.tick(4 * t.interval);
    t.tick(4 * t.interval + ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_TRUE(t.sensors[1].valid);
}

void test_85_degrees_after_a_power_on_is_dropped_unless_close_to_the_last_reading()
{
    OneWireTemps t = twoBuses();
    top.powerOn = true;
    t.tick(0);
    t.tick(ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_FALSE(t.sensors[0].valid); // no reading before: a power-on leftover
    top.powerOn = false;
    top.celsius = 83.5f; // a hot tank
    t.tick(t.interval);
    t.tick(t.interval + ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_EQUAL_FLOAT(83.5f, t.sensors[0].celsius);
    top.celsius = 85.0f; // really 85 °C: close to the last reading, kept
    t.tick(2 * t.interval);
    t.tick(2 * t.interval + ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_EQUAL_FLOAT(85.0f, t.sensors[0].celsius);
    TEST_ASSERT_EQUAL(0, t.sensors[0].fails);
}

void test_a_new_search_keeps_the_readings_of_known_sensors()
{
    OneWireTemps t = twoBuses();
    t.tick(0);
    t.tick(ONEWIRE_CONVERSION_MS);
    t.changed = false;
    t.search();
    TEST_ASSERT_TRUE(t.changed);
    TEST_ASSERT_EQUAL(3, t.count);
    TEST_ASSERT_TRUE(t.sensors[0].valid);
    TEST_ASSERT_EQUAL_FLOAT(54.3125f, t.sensors[0].celsius);
}

void test_no_bus_does_nothing()
{
    OneWireTemps t;
    t.tick(0);
    t.tick(100000);
    TEST_ASSERT_EQUAL(0, t.count);
}

// The sensors' supply from a GPIO: switching it off restarts the sensors (when heals is set)
struct FakePower : OneWirePower
{
    bool on = true;
    int offs = 0;
    bool heals = true;

    void set(bool value) override
    {
        on = value;
        if (value)
            return;
        offs++;
        if (heals)
            top.present = bottom.present = floorLoop.present = true;
    }
};

static void hangAllSensors()
{
    top.present = bottom.present = floorLoop.present = false;
}

// Ticks every 100 ms from one time to another, as the main loop does
static void runUntil(OneWireTemps &t, unsigned long from, unsigned long to)
{
    for (unsigned long now = from; now <= to; now += 100)
        t.tick(now);
}

void test_a_hung_bus_is_switched_off_and_on_and_searched_again()
{
    FakePower power;
    OneWireTemps t = twoBuses();
    t.power[0] = t.power[1] = &power; // one supply for both buses
    runUntil(t, 0, ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_TRUE(t.sensors[0].valid);
    hangAllSensors();
    unsigned long allLost = 3 * t.interval + ONEWIRE_CONVERSION_MS;
    runUntil(t, ONEWIRE_CONVERSION_MS + 100, allLost);
    TEST_ASSERT_EQUAL(1, t.powerCycles);
    TEST_ASSERT_FALSE(power.on);
    runUntil(t, allLost + 100, allLost + ONEWIRE_POWER_OFF_MS);
    TEST_ASSERT_TRUE(power.on);
    runUntil(t, allLost + ONEWIRE_POWER_OFF_MS + 100, allLost + ONEWIRE_POWER_OFF_MS + ONEWIRE_POWER_UP_MS + 2 * ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_EQUAL(3, t.count);
    TEST_ASSERT_TRUE(t.sensors[0].valid);
    TEST_ASSERT_EQUAL_FLOAT(54.3125f, t.sensors[0].celsius);
    TEST_ASSERT_EQUAL(1, t.powerCycles);
}

void test_an_empty_bus_is_switched_off_at_most_every_ten_minutes()
{
    FakePower power;
    power.heals = false;
    OneWireTemps t = twoBuses();
    t.power[0] = t.power[1] = &power; // one supply for both buses
    hangAllSensors();
    unsigned long firstOff = 0;
    for (unsigned long now = 0; power.offs == 0; now += 100)
    {
        t.tick(now);
        firstOff = now;
    }
    runUntil(t, firstOff + 100, firstOff + ONEWIRE_POWER_CYCLE_EVERY_MS - 100);
    TEST_ASSERT_EQUAL(1, t.powerCycles);
    runUntil(t, firstOff + ONEWIRE_POWER_CYCLE_EVERY_MS, firstOff + ONEWIRE_POWER_CYCLE_EVERY_MS + t.interval + ONEWIRE_CONVERSION_MS);
    TEST_ASSERT_EQUAL(2, t.powerCycles);
}

void test_one_sensor_lost_does_not_switch_the_others_off()
{
    FakePower power;
    OneWireTemps t = twoBuses();
    t.power[0] = t.power[1] = &power; // one supply for both buses
    runUntil(t, 0, ONEWIRE_CONVERSION_MS);
    bottom.present = false;
    runUntil(t, ONEWIRE_CONVERSION_MS + 100, 10 * t.interval);
    TEST_ASSERT_FALSE(t.sensors[1].valid);
    TEST_ASSERT_EQUAL(0, power.offs);
    TEST_ASSERT_TRUE(power.on);
}

// Two supplies: only the bus where no sensor answers is switched off and on
void test_only_the_hung_bus_is_switched_off()
{
    FakePower supplyA, supplyB;
    supplyA.heals = supplyB.heals = false;
    OneWireTemps t = twoBuses();
    t.power[0] = &supplyA;
    t.power[1] = &supplyB;
    runUntil(t, 0, ONEWIRE_CONVERSION_MS);
    top.present = bottom.present = false; // bus A hangs, bus B goes on
    runUntil(t, ONEWIRE_CONVERSION_MS + 100, 4 * t.interval);
    TEST_ASSERT_EQUAL(1, supplyA.offs);
    TEST_ASSERT_EQUAL(0, supplyB.offs);
    TEST_ASSERT_TRUE(t.sensors[t.count - 1].valid); // the floor loop on bus B still reads
}

void test_without_a_supply_pin_a_hung_bus_just_reads_null()
{
    OneWireTemps t = twoBuses();
    runUntil(t, 0, ONEWIRE_CONVERSION_MS);
    hangAllSensors();
    runUntil(t, ONEWIRE_CONVERSION_MS + 100, 5 * t.interval - 100); // between two cycles
    TEST_ASSERT_FALSE(t.sensors[0].valid);
    TEST_ASSERT_EQUAL(ONEWIRE_IDLE, t.phase);
}

// The bus at the level of the time slots: every device drives the line at once (wired AND), as on the wire
struct FakeBitBus : OneWireBits
{
    struct Device
    {
        uint8_t rom[8];
        float celsius;
        bool active; // still in the search, or addressed
    };
    Device devices[8];
    int count = 0;
    enum
    {
        IDLE,
        COMMAND,
        SEARCH,
        MATCH,
        FUNCTION,
        READ
    } state = IDLE;
    uint8_t byte = 0;
    int bitCount = 0, searchBit = 0, searchPhase = 0, matchBit = 0;
    uint8_t matchRom[8];
    uint8_t pad[9];
    int padBit = 0;
    int conversions = 0;

    void add(const uint8_t rom[8], float c)
    {
        memcpy(devices[count].rom, rom, 8);
        devices[count].celsius = c;
        devices[count].active = false;
        count++;
    }
    static bool romBit(const uint8_t rom[8], int i) { return (rom[i / 8] >> (i % 8)) & 1; }

    bool resetPulse() override
    {
        state = COMMAND;
        byte = 0;
        bitCount = 0;
        for (int i = 0; i < count; i++)
            devices[i].active = true;
        return count > 0;
    }
    void takeByte(bool bit, void (FakeBitBus::*done)(uint8_t))
    {
        if (bit)
            byte |= (uint8_t)(1 << bitCount);
        if (++bitCount == 8)
        {
            uint8_t b = byte;
            byte = 0;
            bitCount = 0;
            (this->*done)(b);
        }
    }
    void onCommand(uint8_t b)
    {
        if (b == ONEWIRE_CMD_SEARCH_ROM)
        {
            state = SEARCH;
            searchBit = searchPhase = 0;
        }
        else if (b == ONEWIRE_CMD_SKIP_ROM)
            state = FUNCTION;
        else if (b == ONEWIRE_CMD_MATCH_ROM)
        {
            state = MATCH;
            matchBit = 0;
            memset(matchRom, 0, 8);
        }
        else
            state = IDLE;
    }
    void onFunction(uint8_t b)
    {
        if (b == ONEWIRE_CMD_CONVERT)
        {
            conversions++;
            state = IDLE;
            return;
        }
        if (b != ONEWIRE_CMD_READ_SCRATCHPAD)
        {
            state = IDLE;
            return;
        }
        memset(pad, 0xFF, sizeof(pad));
        int selected = -1, n = 0;
        for (int i = 0; i < count; i++)
            if (devices[i].active)
            {
                selected = i;
                n++;
            }
        if (n == 1)
        {
            int16_t raw = (int16_t)lroundf(devices[selected].celsius * 16);
            const uint8_t rest[6] = {0x4B, 0x46, 0x7F, 0xFF, 0x0C, 0x10};
            pad[0] = (uint8_t)raw;
            pad[1] = (uint8_t)(raw >> 8);
            memcpy(pad + 2, rest, 6);
            pad[8] = oneWireCrc8(pad, 8);
        }
        state = READ;
        padBit = 0;
    }
    void writeBit(bool bit) override
    {
        if (state == COMMAND)
            takeByte(bit, &FakeBitBus::onCommand);
        else if (state == FUNCTION)
            takeByte(bit, &FakeBitBus::onFunction);
        else if (state == SEARCH && searchPhase == 2)
        {
            for (int i = 0; i < count; i++)
                if (devices[i].active && romBit(devices[i].rom, searchBit) != bit)
                    devices[i].active = false;
            searchPhase = 0;
            if (++searchBit == 64)
                state = IDLE;
        }
        else if (state == MATCH)
        {
            if (bit)
                matchRom[matchBit / 8] |= (uint8_t)(1 << (matchBit % 8));
            if (++matchBit == 64)
            {
                for (int i = 0; i < count; i++)
                    devices[i].active = memcmp(devices[i].rom, matchRom, 8) == 0;
                state = FUNCTION;
            }
        }
    }
    bool readBit() override
    {
        if (state == SEARCH && searchPhase < 2)
        {
            bool complement = searchPhase == 1, line = true;
            for (int i = 0; i < count; i++)
                if (devices[i].active)
                    line = line && (romBit(devices[i].rom, searchBit) != complement);
            searchPhase++;
            return line;
        }
        if (state == READ && padBit < 72)
        {
            bool b = (pad[padBit / 8] >> (padBit % 8)) & 1;
            padBit++;
            return b;
        }
        return true; // the pull-up
    }
};

// The ROM search finds every sensor once, whatever their codes share, then reports the end
void test_the_rom_search_finds_every_sensor_once()
{
    FakeBitBus wire;
    uint8_t roms[5][8];
    const uint8_t serials[5] = {0x00, 0x01, 0x80, 0x81, 0x7F}; // codes that differ in the first and the last bits
    for (int i = 0; i < 5; i++)
    {
        makeRom(roms[i], ONEWIRE_FAMILY_DS18B20, serials[i]);
        wire.add(roms[i], 20 + i);
    }
    OneWireProtocol bus;
    bus.bits = &wire;
    bus.resetSearch();
    int found[5] = {0};
    uint8_t rom[8], first[8];
    int n = 0;
    while (bus.search(rom) && n < 10)
    {
        if (n == 0)
            memcpy(first, rom, 8);
        n++;
        for (int i = 0; i < 5; i++)
            if (memcmp(rom, roms[i], 8) == 0)
                found[i]++;
    }
    TEST_ASSERT_EQUAL(5, n);
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_EQUAL(1, found[i]);
    TEST_ASSERT_TRUE(bus.search(rom)); // the call after the end starts a new search
    TEST_ASSERT_EQUAL_UINT8_ARRAY(first, rom, 8); // the same order again
}

void test_the_rom_search_on_an_empty_bus()
{
    FakeBitBus wire;
    OneWireProtocol bus;
    bus.bits = &wire;
    bus.resetSearch();
    uint8_t rom[8];
    TEST_ASSERT_FALSE(bus.search(rom));
}

// The whole chain over the bit slots: search, one conversion for the bus, a scratchpad per addressed sensor
void test_readings_over_the_bit_slots()
{
    FakeBitBus wire;
    uint8_t a[8], b[8];
    makeRom(a, ONEWIRE_FAMILY_DS18B20, 0x11);
    makeRom(b, ONEWIRE_FAMILY_DS18B20, 0x22);
    wire.add(a, 47.5f);
    wire.add(b, -0.5f);
    OneWireProtocol bus;
    bus.bits = &wire;
    OneWireTemps t;
    OneWireBus *list[1] = {&bus};
    t.attach(list, 1);
    t.tick(0);
    TEST_ASSERT_EQUAL(2, t.count);
    TEST_ASSERT_EQUAL(1, wire.conversions);
    t.tick(ONEWIRE_CONVERSION_MS);
    for (size_t i = 0; i < t.count; i++)
    {
        TEST_ASSERT_TRUE(t.sensors[i].valid);
        float want = memcmp(t.sensors[i].rom, a, 8) == 0 ? 47.5f : -0.5f;
        TEST_ASSERT_EQUAL_FLOAT(want, t.sensors[i].celsius);
    }
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc8_of_a_known_rom);
    RUN_TEST(test_rom_hex_round_trip);
    RUN_TEST(test_finds_the_sensors_of_every_bus_and_skips_other_families);
    RUN_TEST(test_reads_after_the_conversion_without_waiting_in_the_loop);
    RUN_TEST(test_names_from_the_settings_and_the_rom_code_otherwise);
    RUN_TEST(test_a_place_without_a_name);
    RUN_TEST(test_only_plain_places_are_kept);
    RUN_TEST(test_a_bad_crc_keeps_the_last_reading_then_null_after_three_cycles);
    RUN_TEST(test_a_sensor_unplugged_reads_null_and_comes_back);
    RUN_TEST(test_85_degrees_after_a_power_on_is_dropped_unless_close_to_the_last_reading);
    RUN_TEST(test_a_new_search_keeps_the_readings_of_known_sensors);
    RUN_TEST(test_no_bus_does_nothing);
    RUN_TEST(test_a_hung_bus_is_switched_off_and_on_and_searched_again);
    RUN_TEST(test_an_empty_bus_is_switched_off_at_most_every_ten_minutes);
    RUN_TEST(test_one_sensor_lost_does_not_switch_the_others_off);
    RUN_TEST(test_only_the_hung_bus_is_switched_off);
    RUN_TEST(test_without_a_supply_pin_a_hung_bus_just_reads_null);
    RUN_TEST(test_the_rom_search_finds_every_sensor_once);
    RUN_TEST(test_the_rom_search_on_an_empty_bus);
    RUN_TEST(test_readings_over_the_bit_slots);
    return UNITY_END();
}
