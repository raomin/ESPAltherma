#include <unity.h>
#include "earlydefrost.h"
#include "erga_20261113.h"

static DefrostInput input(uint8_t rps=48,uint8_t flags=DF_STATUS)
{
    DefrostInput in;in.rps=rps;in.flags=flags;in.outdoorTenths=20;in.coilTenths=-105;return in;
}
static void runUntil(EarlyDefrostRecorder &r,uint32_t start,unsigned steps,DefrostInput in=input())
{
    for(unsigned i=1;i<=steps;i++) r.observe(in,start+i*5000,1760000000+i*5);
}
static void start(EarlyDefrostRecorder &r)
{
    r.observe(input(0),1000,1760000000);r.observe(input(),6000,1760000005);
}
static void event(EarlyDefrostRecorder &r)
{
    start(r);runUntil(r,6000,83);r.observe(input(48,DF_STATUS|DF_DEFROST),426000,1760000425);
}

void test_detects_early_defrost_with_bounded_trace_and_24h_notice()
{
    EarlyDefrostRecorder r;event(r);
    TEST_ASSERT_TRUE(r.visible(426000));
    TEST_ASSERT_EQUAL(420,r.saved.elapsedSeconds);
    TEST_ASSERT_EQUAL(5,r.saved.startUncertaintySeconds);
    TEST_ASSERT_EQUAL(-105,r.saved.trace.minimumCoilTenths());
    TEST_ASSERT_EQUAL(0,r.saved.trace.samples[0].deltaSeconds);
    TEST_ASSERT_EQUAL(5,r.saved.trace.samples[1].deltaSeconds);
    TEST_ASSERT_EQUAL(ICE_NOT_CHECKED,r.saved.observation);
    TEST_ASSERT_TRUE(r.visible(426000+DEFROST_NOTICE_MS-1));
    TEST_ASSERT_FALSE(r.visible(426000+DEFROST_NOTICE_MS));
    TEST_ASSERT_LESS_OR_EQUAL(2048,sizeof(r));
}

void test_boot_mid_run_and_cooling_do_not_create_false_start()
{
    EarlyDefrostRecorder r;
    r.observe(input(),1000,0);runUntil(r,1000,20);
    TEST_ASSERT_FALSE(r.observe(input(48,DF_STATUS|DF_DEFROST),106000,0));
    TEST_ASSERT_FALSE(r.visible(106000));
    EarlyDefrostRecorder cool;
    cool.observe(input(0),1000,0);cool.observe(input(48,DF_STATUS|DF_COOLING),6000,0);
    TEST_ASSERT_FALSE(cool.observe(input(48,DF_STATUS|DF_COOLING|DF_DEFROST),11000,0));
}

void test_dhw_start_counts_but_mode_change_does_not_restart_window()
{
    EarlyDefrostRecorder r;
    r.observe(input(0,DF_STATUS|DF_DHW|DF_DHW_KNOWN),1000,0);
    r.observe(input(48,DF_STATUS|DF_DHW|DF_DHW_KNOWN),6000,0);
    runUntil(r,6000,180); // changed to space heating, compressor never stopped
    TEST_ASSERT_FALSE(r.observe(input(48,DF_STATUS|DF_DEFROST),911000,0));
    TEST_ASSERT_FALSE(r.visible(911000));
    EarlyDefrostRecorder dhw;dhw.observe(input(0),1000,0);dhw.observe(input(48,DF_STATUS|DF_DHW),6000,0);
    TEST_ASSERT_TRUE(dhw.observe(input(48,DF_STATUS|DF_DHW|DF_DEFROST),11000,0));
}

void test_defrost_restart_does_not_reset_compressor_start_time()
{
    EarlyDefrostRecorder r;event(r);uint32_t id=r.saved.id;
    r.observe(input(0,DF_STATUS|DF_DEFROST),431000,0);
    r.observe(input(30,DF_STATUS|DF_DEFROST),436000,0);
    r.observe(input(),441000,0);
    runUntil(r,441000,100);
    TEST_ASSERT_FALSE(r.observe(input(48,DF_STATUS|DF_DEFROST),946000,0));
    TEST_ASSERT_EQUAL_UINT32(id,r.saved.id);
}

void test_saved_case_survives_recording_the_next_start()
{
    EarlyDefrostRecorder r;event(r);uint32_t id=r.saved.id;
    r.observe(input(0,DF_STATUS|DF_DEFROST),431000,0);
    r.observe(input(30,DF_STATUS|DF_DEFROST),436000,0);
    r.observe(input(),441000,0);runUntil(r,441000,160);
    r.observe(input(0),1246000,0);
    DefrostInput newer=input();newer.coilTenths=-25;
    r.observe(newer,1251000,0);
    TEST_ASSERT_EQUAL_UINT32(id,r.saved.id);
    TEST_ASSERT_EQUAL(-105,r.saved.trace.minimumCoilTenths());
    TEST_ASSERT_EQUAL(-25,r.working.minimumCoilTenths());
    TEST_ASSERT_TRUE(r.annotate(id,ICE_FREE,1251000));
    TEST_ASSERT_FALSE(r.annotate(id+1,ICE_PRESENT,1251000));
    TEST_ASSERT_EQUAL(ICE_FREE,r.saved.observation);
}

void test_lost_feedback_breaks_start_history_and_marks_gap()
{
    EarlyDefrostRecorder r;start(r);
    DefrostInput missing;
    runUntil(r,6000,14,missing);
    r.observe(input(),81000,0);
    TEST_ASSERT_FALSE(r.observe(input(48,DF_STATUS|DF_DEFROST),86000,0));
    TEST_ASSERT_FALSE(r.visible(86000));
}

void test_uncertainty_and_clock_wrap_are_handled_conservatively()
{
    EarlyDefrostRecorder r;start(r);runUntil(r,6000,178);
    TEST_ASSERT_FALSE(r.observe(input(48,DF_STATUS|DF_DEFROST),906000,0)); // 900 s + 5 s uncertainty
    EarlyDefrostRecorder wrapped;
    uint32_t base=UINT32_MAX-10000;
    wrapped.observe(input(0),base,0);wrapped.observe(input(),base+5000,0);
    TEST_ASSERT_TRUE(wrapped.observe(input(48,DF_STATUS|DF_DEFROST),base+15000,0));
}

void test_trace_capacity_and_unknown_temperature_encoding()
{
    DefrostTrace t;t.begin(1000);
    for(unsigned i=0;i<=200;i++) t.append(1000+i*5000,input());
    TEST_ASSERT_EQUAL(DEFROST_SAMPLES,t.count);
    TEST_ASSERT_EQUAL(INT8_MIN,DefrostTrace::half(INT16_MIN));
    TEST_ASSERT_EQUAL(INT8_MIN,DefrostTrace::half(-2000));
    t.begin(1000);t.append(1000,input());t.append(300000,input());
    TEST_ASSERT_TRUE(t.truncated);
    TEST_ASSERT_EQUAL(1,t.count);
}

void test_recorded_ice_free_start_is_detected_without_claiming_ice_absence()
{
    EarlyDefrostRecorder r;
    for (const RecordedStart &s:ERGA_START)
    {
        DefrostInput in;in.outdoorTenths=s.outdoor;in.coilTenths=s.coil;in.rps=s.rps;in.flags=s.flags;
        r.observe(in,s.ms,1794555433+s.ms/1000);
    }
    TEST_ASSERT_TRUE(r.visible(ERGA_START[119].ms));
    TEST_ASSERT_INT_WITHIN(5,410,r.saved.elapsedSeconds);
    TEST_ASSERT_EQUAL(-105,r.saved.trace.minimumCoilTenths());
    TEST_ASSERT_EQUAL(ICE_NOT_CHECKED,r.saved.observation);
}

void test_timestamp_quantisation_does_not_accumulate_jitter_error()
{
    DefrostTrace trace;trace.begin(1000);trace.append(1000,input());
    uint32_t last=1000;
    for (unsigned i=1;i<100;i++) {last=1000+i*5000+(i%2?120:0);trace.append(last,input());}
    uint32_t sum=0;for(size_t i=0;i<trace.count;i++) sum+=trace.samples[i].deltaSeconds;
    TEST_ASSERT_INT_WITHIN(1,(last-1000)/1000,sum);
}

int main(int,char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_detects_early_defrost_with_bounded_trace_and_24h_notice);
    RUN_TEST(test_boot_mid_run_and_cooling_do_not_create_false_start);
    RUN_TEST(test_dhw_start_counts_but_mode_change_does_not_restart_window);
    RUN_TEST(test_defrost_restart_does_not_reset_compressor_start_time);
    RUN_TEST(test_saved_case_survives_recording_the_next_start);
    RUN_TEST(test_lost_feedback_breaks_start_history_and_marks_gap);
    RUN_TEST(test_uncertainty_and_clock_wrap_are_handled_conservatively);
    RUN_TEST(test_trace_capacity_and_unknown_temperature_encoding);
    RUN_TEST(test_recorded_ice_free_start_is_detected_without_claiming_ice_absence);
    RUN_TEST(test_timestamp_quantisation_does_not_accumulate_jitter_error);
    return UNITY_END();
}
