#include <unity.h>

#include "gpio_control.h"

// A chip with 40 pins, 34-39 input only, that records what is done to its pins. An input reads what is wired to
// it (outside), or its pull; an output reads its own level.
struct FakeChip : GpioDriver
{
    GpioMode applied[GPIO_CONTROL_MAX_PINS];
    int outside[GPIO_CONTROL_MAX_PINS]; // -1: nothing wired
    int resets = 0;

    FakeChip()
    {
        for (int i = 0; i < GPIO_CONTROL_MAX_PINS; i++)
        {
            applied[i] = GPIO_UNUSED;
            outside[i] = -1;
        }
    }
    bool exists(int pin) const override { return pin >= 0 && pin < 40; }
    bool canOutput(int pin) const override { return pin < 34; }
    void apply(int pin, GpioMode mode) override { applied[pin] = mode; }
    void release(int pin) override
    {
        applied[pin] = GPIO_UNUSED;
        resets++;
    }
    int read(int pin) const override
    {
        switch (applied[pin])
        {
        case GPIO_OUTPUT_HIGH:
            return 1;
        case GPIO_OUTPUT_LOW:
            return 0;
        case GPIO_INPUT_PULLUP:
            return outside[pin] >= 0 ? outside[pin] : 1;
        default:
            return outside[pin] >= 0 ? outside[pin] : 0;
        }
    }
};

static FakeChip chip;
static GpioControl gpio;
static const int8_t LOCKED[] = {0, 19, 20};

static void planWithBus(int busPin)
{
    gpio.beginPlan();
    gpio.lock(LOCKED, 3);
    gpio.reserve(13, "X10A RX");
    gpio.reserve(14, "X10A TX");
    gpio.reserve(busPin, "DS18B20 data");
    gpio.reserve(47, "DS18B20 supply"); // a pin the chip does not have: ignored
    gpio.endPlan();
}

void setUp()
{
    chip = FakeChip();
    gpio = GpioControl();
    gpio.driver = &chip;
    planWithBus(21);
}
void tearDown() {}

void test_an_output_goes_high_and_low_and_reads_its_level()
{
    TEST_ASSERT_TRUE(gpio.set(18, GPIO_OUTPUT_HIGH));
    TEST_ASSERT_EQUAL(GPIO_OUTPUT_HIGH, chip.applied[18]);
    TEST_ASSERT_EQUAL(1, gpio.level(18));
    int resets = chip.resets;
    TEST_ASSERT_TRUE(gpio.set(18, GPIO_OUTPUT_LOW));
    TEST_ASSERT_EQUAL(0, gpio.level(18));
    TEST_ASSERT_EQUAL(resets, chip.resets); // no reset between two modes: no glitch
    TEST_ASSERT_TRUE(gpio.set(18, GPIO_UNUSED));
    TEST_ASSERT_EQUAL(GPIO_UNUSED, chip.applied[18]);
    TEST_ASSERT_EQUAL(-1, gpio.level(18)); // not declared: not read
}

void test_an_input_reads_what_is_wired_to_it()
{
    TEST_ASSERT_TRUE(gpio.set(5, GPIO_INPUT_PULLUP));
    TEST_ASSERT_EQUAL(1, gpio.level(5)); // open contact, pulled up
    chip.outside[5] = 0;                 // contact closed to GND
    TEST_ASSERT_EQUAL(0, gpio.level(5));
    TEST_ASSERT_TRUE(gpio.set(35, GPIO_INPUT)); // an input-only pin takes an input
    chip.outside[35] = 1;
    TEST_ASSERT_EQUAL(1, gpio.level(35));
}

void test_pins_in_use_locked_or_missing_are_refused_with_the_reason()
{
    TEST_ASSERT_FALSE(gpio.set(13, GPIO_OUTPUT_HIGH));
    TEST_ASSERT_EQUAL_STRING("X10A RX", gpio.whyNot(13));
    TEST_ASSERT_FALSE(gpio.set(21, GPIO_INPUT));
    TEST_ASSERT_EQUAL_STRING("DS18B20 data", gpio.whyNot(21));
    TEST_ASSERT_FALSE(gpio.set(19, GPIO_INPUT_PULLUP));
    TEST_ASSERT_EQUAL_STRING(GPIO_USE_LOCKED, gpio.whyNot(19));
    TEST_ASSERT_FALSE(gpio.set(35, GPIO_OUTPUT_HIGH));
    TEST_ASSERT_EQUAL_STRING(GPIO_USE_INPUT_ONLY, gpio.whyNot(35, GPIO_OUTPUT_HIGH));
    TEST_ASSERT_FALSE(gpio.set(47, GPIO_INPUT));
    TEST_ASSERT_EQUAL_STRING(GPIO_USE_NO_PIN, gpio.whyNot(47));
    TEST_ASSERT_FALSE(gpio.set(-1, GPIO_INPUT));
    TEST_ASSERT_FALSE(gpio.set(64, GPIO_INPUT));
    TEST_ASSERT_EQUAL(GPIO_UNUSED, chip.applied[13]);
    TEST_ASSERT_EQUAL(GPIO_UNUSED, chip.applied[21]);
    TEST_ASSERT_EQUAL(GPIO_UNUSED, chip.applied[19]);
}

// The settings move the bus to a declared pin: the pin is released before the bus takes it
void test_a_declared_pin_that_becomes_used_is_released()
{
    TEST_ASSERT_TRUE(gpio.set(18, GPIO_OUTPUT_HIGH));
    TEST_ASSERT_TRUE(gpio.set(22, GPIO_INPUT_PULLDOWN));
    planWithBus(18);
    TEST_ASSERT_EQUAL(GPIO_UNUSED, chip.applied[18]);
    TEST_ASSERT_EQUAL(GPIO_UNUSED, gpio.mode[18]);
    TEST_ASSERT_EQUAL_STRING("DS18B20 data", gpio.whyNot(18));
    TEST_ASSERT_EQUAL(GPIO_INPUT_PULLDOWN, chip.applied[22]); // the others stay
    TEST_ASSERT_TRUE(gpio.isFree(21));                        // the old bus pin is free again
}

// At start, and after the settings changed: the kept declarations, where their pin is free
void test_kept_declarations_are_applied_where_the_pin_is_free()
{
    GpioDeclaration kept[] = {{18, GPIO_OUTPUT_HIGH}, {21, GPIO_INPUT}, {5, GPIO_INPUT_PULLUP}};
    TEST_ASSERT_EQUAL(1, gpio.applyDeclarations(kept, 3)); // 21 is the bus: it waits
    TEST_ASSERT_EQUAL(GPIO_OUTPUT_HIGH, chip.applied[18]);
    TEST_ASSERT_EQUAL(GPIO_INPUT_PULLUP, chip.applied[5]);
    int resets = chip.resets;
    gpio.applyDeclarations(kept, 3); // nothing to do for the pins already in their mode
    TEST_ASSERT_EQUAL(resets, chip.resets);
    planWithBus(4); // the bus moves away from 21: its declaration applies now
    TEST_ASSERT_EQUAL(0, gpio.applyDeclarations(kept, 3));
    TEST_ASSERT_EQUAL(GPIO_INPUT, chip.applied[21]);
}

void test_declarations_are_added_changed_and_removed()
{
    GpioDeclaration list[GPIO_CONTROL_MAX_DECLARED];
    int n = 0;
    n = gpioDeclare(list, n, 18, GPIO_OUTPUT_HIGH);
    n = gpioDeclare(list, n, 5, GPIO_INPUT);
    TEST_ASSERT_EQUAL(2, n);
    n = gpioDeclare(list, n, 18, GPIO_OUTPUT_LOW);
    TEST_ASSERT_EQUAL(2, n);
    TEST_ASSERT_EQUAL(GPIO_OUTPUT_LOW, list[0].mode);
    n = gpioDeclare(list, n, 18, GPIO_UNUSED);
    TEST_ASSERT_EQUAL(1, n);
    TEST_ASSERT_EQUAL(5, list[0].pin);
    TEST_ASSERT_EQUAL(1, gpioDeclare(list, n, 7, GPIO_UNUSED)); // removing what is not there
}

void test_modes_by_name()
{
    GpioMode m = GPIO_UNUSED;
    TEST_ASSERT_TRUE(gpioModeFromName("input_pullup", m));
    TEST_ASSERT_EQUAL(GPIO_INPUT_PULLUP, m);
    TEST_ASSERT_TRUE(gpioModeFromName("HIGH", m));
    TEST_ASSERT_EQUAL(GPIO_OUTPUT_HIGH, m);
    TEST_ASSERT_FALSE(gpioModeFromName("output", m));
    TEST_ASSERT_FALSE(gpioModeFromName(nullptr, m));
    TEST_ASSERT_EQUAL_STRING("input_pulldown", gpioModeName(GPIO_INPUT_PULLDOWN));
}

void test_nothing_is_set_while_the_plan_is_rebuilt()
{
    gpio.beginPlan();
    TEST_ASSERT_FALSE(gpio.set(18, GPIO_OUTPUT_HIGH));
    gpio.endPlan();
    TEST_ASSERT_TRUE(gpio.set(18, GPIO_OUTPUT_HIGH));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_an_output_goes_high_and_low_and_reads_its_level);
    RUN_TEST(test_an_input_reads_what_is_wired_to_it);
    RUN_TEST(test_pins_in_use_locked_or_missing_are_refused_with_the_reason);
    RUN_TEST(test_a_declared_pin_that_becomes_used_is_released);
    RUN_TEST(test_kept_declarations_are_applied_where_the_pin_is_free);
    RUN_TEST(test_declarations_are_added_changed_and_removed);
    RUN_TEST(test_modes_by_name);
    RUN_TEST(test_nothing_is_set_while_the_plan_is_rebuilt);
    return UNITY_END();
}
