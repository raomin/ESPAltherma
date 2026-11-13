#include <unity.h>
#include "targetconfirmation.h"

void test_read_at_zero_and_millis_wrap_are_valid()
{
    TargetConfirmation c;
    TEST_ASSERT_EQUAL_STRING("not_read", c.name(0, 360000));
    c.observe(45,45,true,0);
    TEST_ASSERT_TRUE(c.fresh(0,360000));
    TEST_ASSERT_EQUAL(0,c.ageSeconds(0));
    c.observe(45,45,true,UINT32_MAX-1000);
    TEST_ASSERT_TRUE(c.fresh(1000,360000));
    TEST_ASSERT_EQUAL(2,c.ageSeconds(1000));
    TEST_ASSERT_LESS_OR_EQUAL(20,sizeof(TargetConfirmation));
}

void test_write_failure_is_not_erased_by_a_later_read()
{
    TargetConfirmation c;
    c.queue(50); c.writeFailed(); c.observe(45,45,true,1000);
    TEST_ASSERT_EQUAL_STRING("write_failed",c.name(1000,360000));
    TEST_ASSERT_FALSE(c.fresh(1000,360000));
    c.queue(50); c.written(50,50); c.observe(50,50,true,2000);
    TEST_ASSERT_EQUAL_STRING("confirmed",c.name(2000,360000));
}

int main(int,char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_read_at_zero_and_millis_wrap_are_valid);
    RUN_TEST(test_write_failure_is_not_erased_by_a_later_read);
    return UNITY_END();
}
