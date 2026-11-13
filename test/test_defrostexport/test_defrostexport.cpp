#include <unity.h>
#include <string.h>
#include "defrostexport.h"
void setUp() {}
void tearDown() {}
void test_export_preserves_signed_and_unknown_values()
{
    char out[112];
    DefrostSample s={5,4,-21,40,DF_STATUS|DF_DEFROST};
    defrostJsonRow(s,true,out,sizeof(out));TEST_ASSERT_EQUAL_STRING("[5,4,-21,40,18]",out);
    defrostCsvRow(s,410,out,sizeof(out));TEST_ASSERT_EQUAL_STRING("410,2.0,-10.5,40,0,1,,0\n",out);
    s={0,INT8_MIN,INT8_MIN,255,0};
    defrostCsvRow(s,0,out,sizeof(out));TEST_ASSERT_EQUAL_STRING("0,,,,,,,\n",out);
    defrostJsonRow(s,false,out,sizeof(out));TEST_ASSERT_EQUAL_STRING(",\n[0,-128,-128,255,0]",out);
}
void test_full_trace_export_is_bounded_and_has_real_relative_times()
{
    DefrostTrace trace;trace.begin(1000);
    DefrostInput in;in.rps=40;in.flags=DF_STATUS;
    for (unsigned i=0;i<=180;i++) trace.append(1000+i*5000,in);
    uint32_t seconds=0;size_t jsonBytes=0;
    for (size_t i=0;i<trace.count;i++)
    {
        char out[112];seconds+=trace.samples[i].deltaSeconds;
        int n=defrostJsonRow(trace.samples[i],i==0,out,sizeof(out));
        TEST_ASSERT_TRUE(n>0 && n<(int)sizeof(out));jsonBytes+=n;
    }
    TEST_ASSERT_EQUAL(181,trace.count);TEST_ASSERT_EQUAL(900,seconds);
    TEST_ASSERT_LESS_THAN(6000,jsonBytes);TEST_ASSERT_LESS_OR_EQUAL(2048,sizeof(EarlyDefrostRecorder));
}
int main(int,char **)
{
    UNITY_BEGIN();RUN_TEST(test_export_preserves_signed_and_unknown_values);
    RUN_TEST(test_full_trace_export_is_bounded_and_has_real_relative_times);return UNITY_END();
}
