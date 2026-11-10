#include <unity.h>
#include <math.h>

#include "stats.h"

void setUp(void) {}
void tearDown(void) {}

// A heating run: 12 l/min, 35 / 30 C, 6 A at 230 V
static StatsInput heating(float inv = 40)
{
    StatsInput in;
    in.inv = inv;
    in.flow = 12;
    in.lw = 35;
    in.inlet = 30;
    in.amp = 6;
    in.outdoor = 5;
    in.v3 = 0;
    in.defrost = 0;
    in.buh = 0;
    return in;
}

static Stats fresh(StatsDay &rtc)
{
    Stats s;
    memset(&rtc, 0x5A, sizeof(rtc)); // a power-up: anything in the RTC memory
    s.begin(&rtc);
    return s;
}

const uint32_t T0 = 1760000000; // an epoch in the middle of a day (UTC)

// Heat from flow x delta T, electricity from current x voltage; no COP in the first two minutes
void test_powers_and_transient(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    TEST_ASSERT_EQUAL(0, rtc.lastRunSec); // nothing kept from the garbage
    TEST_ASSERT_EQUAL(0, rtc.starts);
    s.update(heating(), T0, 10);
    TEST_ASSERT_TRUE(s.running);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 12.0f / 60 * 4.18f * 5, s.heatKW); // 4.18 kW
    TEST_ASSERT_FLOAT_WITHIN(0.001, 1.38f, s.elecKW);
    TEST_ASSERT_TRUE(isnan(s.cop)); // transient
    s.update(heating(), T0 + 60, 70);
    TEST_ASSERT_TRUE(isnan(s.cop));
    s.update(heating(), T0 + 120, 130);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.18f / 1.38f, s.cop);
    TEST_ASSERT_EQUAL(1, rtc.starts);
    TEST_ASSERT_EQUAL(120, rtc.runSec);
}

// Three phases multiply the phase voltage by 3; the unit's own voltage reading wins over the setting; aux watts
// while the compressor or the pump runs
void test_voltage_sources(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    s.settings.phases = 3;
    s.settings.voltage = 230;
    s.settings.auxW = 50;
    s.update(heating(), T0, 10);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 6 * 230 * 3 / 1000.0f + 0.05f, s.elecKW);
    StatsInput in = heating();
    in.volt = 235;
    s.settings.phases = 1;
    s.update(in, T0 + 10, 20);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 6 * 235 / 1000.0f + 0.05f, s.elecKW);
    in.inv = 0; // compressor stopped, the pump still running: its watts stay
    s.update(in, T0 + 20, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 6 * 235 / 1000.0f + 0.05f, s.elecKW);
    in.flow = 0; // pump off: no aux
    s.update(in, T0 + 30, 40);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 6 * 235 / 1000.0f, s.elecKW);
}

// Energies integrate over the run, per mode; the day's COP needs a few samples
void test_day_energy_per_mode(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    for (int i = 0; i <= 60; i++) // one hour of heating at 4.18 kW / 1.38 kW
        s.update(heating(), T0 + i * 60, 10 + i * 60);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.18f, rtc.heatKWh[STATS_HEATING]);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.38f, rtc.elecKWh[STATS_HEATING]);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.18f / 1.38f, s.copToday(STATS_HEATING));
    TEST_ASSERT_TRUE(isnan(s.copToday(STATS_DHW)));
    StatsInput dhw = heating();
    dhw.v3 = 1;
    for (int i = 1; i <= 30; i++)
        s.update(dhw, T0 + 3600 + i * 60, 3610 + i * 60);
    TEST_ASSERT_EQUAL(STATS_DHW, s.mode);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 2.09f, rtc.heatKWh[STATS_DHW]);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.18f, rtc.heatKWh[STATS_HEATING]); // untouched
    TEST_ASSERT_EQUAL(1, rtc.starts);                                   // the valve switch is not a start
}

// Starts, last run, last pause, starts in the last hour; a run shorter than the threshold still counts as a start
void test_cycles(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    uint32_t t = T0;
    for (int cycle = 0; cycle < 3; cycle++)
    {
        for (int i = 0; i < 20; i++, t += 60)
            s.update(heating(), t, t - T0);
        for (int i = 0; i < 10; i++, t += 60)
            s.update(heating(0), t, t - T0);
    }
    TEST_ASSERT_EQUAL(3, rtc.starts);
    TEST_ASSERT_EQUAL(1200, rtc.lastRunSec);
    TEST_ASSERT_EQUAL(600, rtc.lastPauseSec);
    TEST_ASSERT_UINT32_WITHIN(60, 3600, rtc.runSec); // the sampling: one interval a cycle
    TEST_ASSERT_EQUAL(1, s.startsLastHour(t, t - T0)); // the starts were 90, 60 and 30 min ago
    TEST_ASSERT_FALSE(s.running);
    TEST_ASSERT_EQUAL(600, s.currentSec(t, t - T0));
}

// The power factor scales the inverter's part only, never the pump nor the backup heater
void test_power_factor(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    s.settings.powerFactor = 0.7f;
    s.settings.auxW = 50;
    s.update(heating(), T0, 10);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 6 * 230 * 0.7f / 1000.0f + 0.05f, s.elecKW);
    StatsInput d = heating();
    d.buh = 1;
    s.update(d, T0 + 10, 20);
    TEST_ASSERT_FLOAT_WITHIN(0.001, 6 * 230 * 0.7f / 1000.0f + 0.05f + 3, s.elecKW);
}

// Defrost edges are counted; the COP is not computed during one; the backup heater's time adds up
void test_defrost_and_buh(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    for (int i = 0; i < 5; i++)
        s.update(heating(), T0 + i * 60, 10 + i * 60);
    TEST_ASSERT_FALSE(isnan(s.cop));
    StatsInput d = heating();
    d.defrost = 1;
    d.buh = 1;
    s.update(d, T0 + 300, 310);
    s.update(d, T0 + 360, 370);
    TEST_ASSERT_TRUE(isnan(s.cop));
    TEST_ASSERT_EQUAL(1, rtc.defrosts);
    TEST_ASSERT_EQUAL(120, rtc.buhSec);
    // the heater's step adds its kW to the electricity, and the heat is read after it when that sensor is there
    TEST_ASSERT_FLOAT_WITHIN(0.01, 6 * 230 / 1000.0f + 3, s.elecKW);
    d.lwAfter = 38;
    d.buh2 = 1;
    s.update(d, T0 + 400, 410);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 6 * 230 / 1000.0f + 6, s.elecKW);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 12 / 60.0f * 4.18f * 8, s.heatKW);
    s.update(heating(), T0 + 420, 430);
    TEST_ASSERT_EQUAL(1, rtc.defrosts);
    TEST_ASSERT_FALSE(isnan(s.cop));
}

// An ECH2O making hot water (#659): 47 C out of the exchanger, 40 C after the tank coil, 39.7 C return. The sensor
// after the heater would give 0.3 kW for 8; with the heater in the tank the one before it is taken, in every mode
void test_heater_in_tank_takes_the_exchanger_outlet(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    StatsInput in = heating();
    in.flow = 16.4f;
    in.lw = 46.8f;
    in.lwAfter = 40.0f;
    in.inlet = 39.7f;
    in.v3 = 1;
    s.update(in, T0, 10);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 16.4f / 60 * 4.18f * 0.3f, s.heatKW); // any other unit: the water after the heater
    s.settings.heaterInTank = true;
    s.update(in, T0 + 30, 40);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 16.4f / 60 * 4.18f * 7.1f, s.heatKW); // 8.1 kW
    TEST_ASSERT_EQUAL(STATS_DHW, s.mode);
    in.v3 = 0; // heating: the same sensor, no jump of the two sensors' gap at the change of mode
    in.lw = 35.4f;
    in.lwAfter = 35.0f;
    in.inlet = 30;
    s.update(in, T0 + 60, 70);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 16.4f / 60 * 4.18f * 5.4f, s.heatKW);
    // without the sensor before the heater: the other one in heating, nothing in hot water (it is on the coil's return)
    in.lw = NAN;
    s.update(in, T0 + 90, 100);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 16.4f / 60 * 4.18f * 5.0f, s.heatKW);
    in.v3 = 1;
    s.update(in, T0 + 120, 130);
    TEST_ASSERT_TRUE(isnan(s.heatKW));
}

// The heater in the tank: its kW is heat as it is electricity, the water does not carry it, flow or not
void test_heater_in_tank_counts_its_heat(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    s.settings.heaterInTank = true;
    s.settings.buhKW[0] = 3;
    s.settings.buhKW[1] = 6;
    StatsInput in = heating();
    in.buh = 1;
    s.update(in, T0, 10);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.18f + 3, s.heatKW);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 1.38f + 3, s.elecKW);
    in.buh2 = 1;
    s.update(in, T0 + 30, 40);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.18f + 9, s.heatKW);
    // the heater alone, compressor and pump stopped: its heat still counts, into the day's figures too
    StatsInput alone;
    alone.inv = 0;
    alone.flow = 0;
    alone.lw = 30;
    alone.inlet = 30;
    alone.amp = 0;
    alone.v3 = 1;
    alone.buh = 1;
    s.update(alone, T0 + 60, 70);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 3, s.heatKW);
    float before = rtc.heatKWh[STATS_DHW];
    s.update(alone, T0 + 3660, 3670); // more than ten minutes later: a gap, nothing integrated
    s.update(alone, T0 + 3960, 3970);
    TEST_ASSERT_FLOAT_WITHIN(0.01, before + 3 * 300 / 3600.0f, rtc.heatKWh[STATS_DHW]);
    // any other unit: the heater's heat is in the water after it, not added
    s.settings.heaterInTank = false;
    in.buh2 = 0;
    in.lwAfter = 38;
    s.update(in, T0 + 4000, 4010);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 12 / 60.0f * 4.18f * 8, s.heatKW);
}

// Implausible readings are dropped: more than the unit's cap, a COP over 11 or over Carnot x 0.55
void test_sanity_caps(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    StatsInput in = heating();
    in.flow = 40;
    in.lw = 60; // 40/60 x 4.18 x 30 = 83 kW: a glitch at any capacity
    s.update(in, T0, 10);
    TEST_ASSERT_TRUE(isnan(s.heatKW));
    // the cap follows the unit: 15 kW is heat on a 16 kW unit (cap 25.6), a glitch on a 6 kW one (cap 9.6)
    in = heating();
    in.flow = 28.7f;
    in.lw = 40;
    in.inlet = 32.5f; // 28.7/60 x 4.18 x 7.5 = 15.0 kW
    s.settings.maxHeatKW = 1.6f * 16;
    s.update(in, T0 + 20, 30);
    TEST_ASSERT_FLOAT_WITHIN(0.1, 15.0f, s.heatKW);
    s.settings.maxHeatKW = 1.6f * 6;
    s.update(in, T0 + 30, 40);
    TEST_ASSERT_TRUE(isnan(s.heatKW));
    s.settings.maxHeatKW = STATS_MAX_HEAT_KW;
    in = heating();
    in.amp = 0.5f; // 4.18 kW for 115 W: COP 36
    for (int i = 0; i < 4; i++)
        s.update(in, T0 + 60 + i * 60, 70 + i * 60);
    TEST_ASSERT_TRUE(isnan(s.cop));
    TEST_ASSERT_TRUE(isnan(s.copToday(STATS_HEATING)));
    // Carnot: 35 C leaving at 30 C outdoor allows 308/5 x 0.55 = 33.9; at -10 C outdoor only 308/45 x 0.55 = 3.8
    TEST_ASSERT_FLOAT_WITHIN(0.1, 3.76f, Stats::carnotCeiling(35, -10));
    in = heating();
    in.outdoor = -10; // COP 3.03 is fine
    s.update(in, T0 + 400, 410);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 3.03f, s.cop);
    in.amp = 4; // 4.54: over the ceiling, the smoothed value stays
    s.update(in, T0 + 460, 470);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 3.03f, s.cop);
}

// The day changes at the local midnight; the counters before the clock arrived become today's
void test_day_change(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    s.settings.tzMinutes = 120; // UTC+2
    s.update(heating(), 0, 10); // no clock yet
    s.update(heating(), 0, 70);
    TEST_ASSERT_EQUAL(-1, rtc.day);
    TEST_ASSERT_EQUAL(1, rtc.starts);
    uint32_t midnight = (T0 / 86400 + 1) * 86400 - 120 * 60; // local midnight, in UTC
    s.update(heating(), midnight - 600, 130);                 // the clock arrives: the counters are kept
    TEST_ASSERT_EQUAL(1, rtc.starts);
    TEST_ASSERT_EQUAL(s.dayOf(midnight - 600), rtc.day);
    TEST_ASSERT_EQUAL(120, s.currentSec(midnight - 600, 130)); // the run's start moved to the epoch
    TEST_ASSERT_EQUAL(1, s.startsLastHour(midnight - 600, 130));
    s.update(heating(), midnight - 300, 430);
    TEST_ASSERT_TRUE(rtc.heatKWh[STATS_HEATING] > 0);
    s.update(heating(), midnight + 60, 790); // a new day
    TEST_ASSERT_EQUAL(rtc.day, s.dayOf(midnight + 60));
    TEST_ASSERT_EQUAL(0, rtc.starts);
    TEST_ASSERT_FLOAT_WITHIN(0.01, 4.18f * 360 / 3600, rtc.heatKWh[STATS_HEATING]); // only the interval across midnight
    TEST_ASSERT_TRUE(s.inRun); // the run itself goes on
}

// The RTC block survives a restart of the board: begin() keeps it when the magic matches
void test_rtc_survives_restart(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    for (int i = 0; i < 10; i++)
        s.update(heating(), T0 + i * 60, 10 + i * 60);
    Stats again;
    again.begin(&rtc);
    TEST_ASSERT_EQUAL(1, rtc.starts);
    TEST_ASSERT_TRUE(rtc.heatKWh[STATS_HEATING] > 0);
    again.update(heating(), T0 + 700, 5); // nothing integrated across the gap; the run goes on, no new start
    TEST_ASSERT_EQUAL(1, rtc.starts);
    TEST_ASSERT_TRUE(again.running);
}

// A restart of the board (an update, a reset) while the compressor runs is not a start; a start while the board
// restarted, the compressor stopped before and running after, is
void test_a_restart_during_a_run_is_not_a_start(void)
{
    StatsDay rtc;
    Stats s = fresh(rtc);
    s.update(heating(), T0, 10);
    s.update(heating(), T0 + 60, 70);
    TEST_ASSERT_EQUAL(1, rtc.starts);
    for (int restart = 0; restart < 4; restart++) // four updates in a row during the same run
    {
        Stats again;
        again.begin(&rtc);
        again.update(heating(), T0 + 120 + restart * 60, 5);
    }
    TEST_ASSERT_EQUAL(1, rtc.starts);
    Stats stopped;
    stopped.begin(&rtc);
    stopped.update(heating(0), T0 + 600, 5); // the run ends
    Stats restarted;
    restarted.begin(&rtc);
    restarted.update(heating(), T0 + 900, 5); // started while the board restarted
    TEST_ASSERT_EQUAL(2, rtc.starts);
    Stats unknownFirst;
    unknownFirst.begin(&rtc);
    StatsInput noInverter = heating();
    noInverter.inv = NAN;
    unknownFirst.update(noInverter, T0 + 960, 5); // a first reading without the inverter tells nothing yet
    unknownFirst.update(heating(), T0 + 1020, 65);
    TEST_ASSERT_EQUAL(2, rtc.starts);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_powers_and_transient);
    RUN_TEST(test_voltage_sources);
    RUN_TEST(test_power_factor);
    RUN_TEST(test_day_energy_per_mode);
    RUN_TEST(test_cycles);
    RUN_TEST(test_defrost_and_buh);
    RUN_TEST(test_heater_in_tank_takes_the_exchanger_outlet);
    RUN_TEST(test_heater_in_tank_counts_its_heat);
    RUN_TEST(test_sanity_caps);
    RUN_TEST(test_day_change);
    RUN_TEST(test_rtc_survives_restart);
    RUN_TEST(test_a_restart_during_a_run_is_not_a_start);
    return UNITY_END();
}
