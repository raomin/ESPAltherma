#include <unity.h>
#include "valuequality.h"

void test_quality_distinguishes_never_missing_failed_and_stale()
{
    ValueQualityTracker q;
    TEST_ASSERT_EQUAL(VALUE_NEVER, q.quality(0x20, false, 0, 30000));
    q.note(0x20, true, 0);
    TEST_ASSERT_EQUAL(VALUE_OK, q.quality(0x20, true, 0, 30000));
    TEST_ASSERT_EQUAL(VALUE_MISSING, q.quality(0x20, false, 0, 30000));
    TEST_ASSERT_EQUAL(VALUE_OK, q.quality(0x20, true, 89999, 30000));
    TEST_ASSERT_EQUAL(VALUE_STALE, q.quality(0x20, true, 90000, 30000));
    q.note(0x20, false, 91000);
    TEST_ASSERT_EQUAL(VALUE_READ_FAILED, q.quality(0x20, false, 91000, 30000));
    TEST_ASSERT_EQUAL(91, q.ageSeconds(0x20, 91000)); // a valid read at boot millisecond zero is retained
    q.note(0x20, true, 92000);
    TEST_ASSERT_EQUAL(VALUE_OK, q.quality(0x20, true, 92000, 30000));
    TEST_ASSERT_EQUAL(0, q.ageSeconds(0x20, 92000));
}

void test_registries_expire_independently_and_wrap_safely()
{
    ValueQualityTracker q;
    q.note(0x20, true, UINT32_MAX - 30000);
    q.note(0x30, false, UINT32_MAX - 30000);
    TEST_ASSERT_EQUAL(VALUE_OK, q.quality(0x20, true, 20000, 30000));
    TEST_ASSERT_EQUAL(VALUE_READ_FAILED, q.quality(0x30, true, 20000, 30000));
    TEST_ASSERT_EQUAL(-1, q.ageSeconds(0x30, 20000));
    TEST_ASSERT_EQUAL(VALUE_STALE, q.quality(0x20, true, 60000, 30000));
    q.reset();
    TEST_ASSERT_EQUAL(VALUE_NEVER, q.quality(0x20, true, 60000, 30000));
    TEST_ASSERT_EQUAL(-1, q.ageSeconds(0x20, 60000));
}

void test_period_bounds_and_memory_budget()
{
    TEST_ASSERT_EQUAL(60000, ValueQualityTracker::maxAge(1));
    TEST_ASSERT_EQUAL(90000, ValueQualityTracker::maxAge(30000));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, ValueQualityTracker::maxAge(UINT32_MAX));
    TEST_ASSERT_LESS_OR_EQUAL(264, sizeof(ValueQualityTracker));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_quality_distinguishes_never_missing_failed_and_stale);
    RUN_TEST(test_registries_expire_independently_and_wrap_safely);
    RUN_TEST(test_period_bounds_and_memory_budget);
    return UNITY_END();
}
