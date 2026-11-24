#include <unity.h>
#include "unitguard.h"

// Registry 0x00 of an EHBX16CB3V + ERLQ016CV3 through a restart of the unit (#714), read every 5 s
static const uint8_t COMPLETE[13] = {0x0A, 0x01, 0x00, 0x02, 0x02, 0x01, 0x01, 0x02, 0x04, 0x01, 0x47, 0x02, 0xA0};
static const uint8_t ZERO[13] = {0};
static const uint8_t TAIL1[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xA0};
static const uint8_t TAIL3[13] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x47, 0x02, 0xA0};

static UnitGuardVerdict same(UnitGuard &g, const uint8_t *reply, unsigned long now)
{
    return g.poll(reply, 13, reply, 13, now);
}

void test_the_recorded_restart_is_held_from_its_first_poll_to_its_last()
{
    UnitGuard g;
    unsigned long now = 1000;
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, COMPLETE, now));
    TEST_ASSERT_FALSE(g.began || g.ended || g.gaveUp);
    // 3 polls all zero, 16 with the last byte, 5 with the last three: 24 polls over 120 s
    const uint8_t *steps[] = {ZERO, TAIL1, TAIL3};
    const int counts[] = {3, 16, 5};
    int polls = 0;
    for (int s = 0; s < 3; s++)
        for (int i = 0; i < counts[s]; i++)
        {
            now += 5000;
            TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, steps[s], now));
            TEST_ASSERT_EQUAL(polls == 0, g.began);
            polls++;
        }
    now += 5000;
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, COMPLETE, now));
    TEST_ASSERT_TRUE(g.ended);
    TEST_ASSERT_FALSE(g.gaveUp);
    now += 5000;
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, COMPLETE, now));
    TEST_ASSERT_FALSE(g.ended);
}

void test_a_restart_that_begins_or_ends_during_the_poll_holds_it()
{
    UnitGuard g;
    same(g, COMPLETE, 0);
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, g.poll(COMPLETE, 13, ZERO, 13, 30000)); // the registries after the first read may be zeros
    TEST_ASSERT_TRUE(g.began);
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, g.poll(TAIL3, 13, COMPLETE, 13, 60000)); // those before the second read as well
    TEST_ASSERT_FALSE(g.began);
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, g.poll(COMPLETE, 13, COMPLETE, 13, 90000));
}

void test_a_value_that_changes_without_reading_zero_is_not_a_restart()
{
    UnitGuard g;
    same(g, COMPLETE, 0);
    uint8_t other[13];
    memcpy(other, COMPLETE, 13);
    other[0] = 0x0B;
    other[2] = 0x07; // a byte that was zero may take a value
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, other, 30000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, COMPLETE, 60000)); // and is expected from then on
}

void test_zeros_that_last_are_accepted_and_become_what_is_expected()
{
    UnitGuard g;
    same(g, COMPLETE, 0);
    unsigned long now = 0;
    while (now + 30000 < UNIT_GUARD_MAX_MS)
    {
        now += 30000;
        TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, ZERO, now));
    }
    now = 30000 + UNIT_GUARD_MAX_MS;
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, ZERO, now));
    TEST_ASSERT_TRUE(g.gaveUp);
    TEST_ASSERT_FALSE(g.ended);
    // an outdoor unit without power: its zeros are readings from there on, without an event at every poll
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, ZERO, now + 30000));
    TEST_ASSERT_FALSE(g.gaveUp || g.began || g.ended);
    // it comes back a byte at a time: nothing complete was seen since, so nothing is held
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, TAIL1, now + 60000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, COMPLETE, now + 90000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, TAIL1, now + 120000));
}

void test_nothing_is_held_before_the_registry_was_read_with_content()
{
    UnitGuard g;
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, ZERO, 0));
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, ZERO, 30000));
    TEST_ASSERT_EQUAL(0, g.knownLen);
    TEST_ASSERT_FALSE(g.began);
}

void test_a_poll_without_an_answer_keeps_the_last_decision()
{
    UnitGuard g;
    same(g, COMPLETE, 0);
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, g.poll(nullptr, 0, nullptr, 0, 30000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, ZERO, 60000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, g.poll(nullptr, 0, nullptr, 0, 90000));
    // one read is enough to decide
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, g.poll(TAIL1, 13, nullptr, 0, 120000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, g.poll(COMPLETE, 13, nullptr, 0, 150000));
    // and silence does not hold longer than zeros would
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, ZERO, 200000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, g.poll(nullptr, 0, nullptr, 0, 200000 + UNIT_GUARD_MAX_MS));
    TEST_ASSERT_TRUE(g.gaveUp);
    TEST_ASSERT_EQUAL(0, g.knownLen);
}

void test_a_shorter_reply_lacks_the_bytes_it_does_not_have()
{
    UnitGuard g;
    same(g, COMPLETE, 0);
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, g.poll(COMPLETE, 12, COMPLETE, 12, 30000));
}

void test_a_registry_that_never_answers_is_no_longer_asked()
{
    UnitGuard g;
    for (int i = 0; i < UNIT_GUARD_UNANSWERED; i++)
    {
        TEST_ASSERT_TRUE(g.wanted());
        g.answered(false);
    }
    TEST_ASSERT_FALSE(g.wanted());
    g.retry(); // a new model or selection: asked again
    TEST_ASSERT_TRUE(g.wanted());
    g.answered(true);
    for (int i = 0; i < 300; i++)
        g.answered(false); // one that answered once is asked for good (a unit switched off comes back)
    TEST_ASSERT_TRUE(g.wanted());
}

void test_the_time_counter_may_wrap_during_a_hold()
{
    UnitGuard g;
    unsigned long now = 0xFFFFFFFFUL - 20000;
    same(g, COMPLETE, now);
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, ZERO, now + 10000));
    TEST_ASSERT_EQUAL(UNIT_GUARD_HOLD, same(g, ZERO, now + 40000)); // past the wrap
    TEST_ASSERT_EQUAL(UNIT_GUARD_PASS, same(g, ZERO, now + 10000 + UNIT_GUARD_MAX_MS));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_recorded_restart_is_held_from_its_first_poll_to_its_last);
    RUN_TEST(test_a_restart_that_begins_or_ends_during_the_poll_holds_it);
    RUN_TEST(test_a_value_that_changes_without_reading_zero_is_not_a_restart);
    RUN_TEST(test_zeros_that_last_are_accepted_and_become_what_is_expected);
    RUN_TEST(test_nothing_is_held_before_the_registry_was_read_with_content);
    RUN_TEST(test_a_poll_without_an_answer_keeps_the_last_decision);
    RUN_TEST(test_a_shorter_reply_lacks_the_bytes_it_does_not_have);
    RUN_TEST(test_a_registry_that_never_answers_is_no_longer_asked);
    RUN_TEST(test_the_time_counter_may_wrap_during_a_hold);
    return UNITY_END();
}
