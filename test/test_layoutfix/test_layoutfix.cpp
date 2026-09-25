#include <stdio.h>
#include <unity.h>

#include "layoutfix.h"
#include "real_epga_series.h"

static void cycleOf(int n, LayoutCycle &cycle)
{
    cycle.clear();
    for (const RealReply &r : REAL_EPGA_SERIES[n])
    {
        uint8_t payload[LAYOUT_PAYLOAD];
        uint8_t len = 0;
        for (size_t i = 0; r.hex[2 * i]; i++)
        {
            unsigned v;
            sscanf(r.hex + 2 * i, "%2x", &v);
            payload[len++] = (uint8_t)v;
        }
        cycle.add(r.reg, payload, len);
    }
}

static const LayoutCheck *checkOf(const LayoutChecker &lc, uint8_t reg)
{
    for (uint8_t i = 0; i < lc.count; i++)
        if (lc.regs[i].reg == reg)
            return &lc.regs[i];
    return nullptr;
}

static void feed(LayoutChecker &lc, int cycles)
{
    LayoutCycle cycle;
    for (int n = 0; n < cycles; n++)
    {
        cycleOf(n, cycle);
        layoutCheckCycle(lc, cycle);
    }
}

void setUp(void) {}
void tearDown(void) {}

void test_checked_registries(void)
{
    int epga = catalogFindModel("Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW)");
    LayoutChecker lc;
    layoutCheckInit(lc, epga);
    printf("EPGA registries with conflicting layouts:");
    for (uint8_t i = 0; i < lc.count; i++)
        printf(" 0x%02X (%d layouts)", lc.regs[i].reg, lc.regs[i].count);
    printf("\n");
    TEST_ASSERT_NOT_NULL(checkOf(lc, 0x21));
    TEST_ASSERT_NOT_NULL(checkOf(lc, 0x30));
    TEST_ASSERT_NULL(checkOf(lc, 0x20)); // one layout in every definition
    TEST_ASSERT_NULL(checkOf(lc, 0x10));
}

// The real EPGA16: 0x21 switches to the layout with 2-byte values at odd offsets (7, 9, 11, 13)
void test_real_unit_fixes_0x21(void)
{
    int epga = catalogFindModel("Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW)");
    LayoutChecker lc;
    layoutCheckInit(lc, epga);

    feed(lc, LAYOUT_MIN_POLLS - 1);
    TEST_ASSERT_EQUAL_INT(-1, layoutCheckDecide(*checkOf(lc, 0x21))); // not enough polls yet

    layoutCheckInit(lc, epga);
    feed(lc, REAL_EPGA_CYCLES);
    const LayoutCheck &rc = *checkOf(lc, 0x21);
    for (uint8_t k = 0; k < rc.count; k++)
    {
        const LayoutCandidate &c = rc.c[k];
        printf("   %s %-58s similar %3d plausible %2u/%2u jumps %2u/%2u mirrors %2u\n", k ? "  " : "->", CATALOG_MODELS[c.model].name, c.similarity, c.plausible, c.checks, c.jumps, c.steps, c.mirrors);
    }
    int k = layoutCheckDecide(rc);
    TEST_ASSERT_TRUE(k > 0);
    const LayoutCandidate &fix = rc.c[k];
    printf("   0x21 fixed with the layout of %s\n", CATALOG_MODELS[fix.model].name);
    bool odd = false;
    for (uint8_t s = 0; s < fix.slotCount; s++)
        odd |= fix.slots[s].offset == 7;
    TEST_ASSERT_TRUE(odd);

    // No evidence for 0x30 (all zero while idle): kept
    TEST_ASSERT_EQUAL_INT(-1, layoutCheckDecide(*checkOf(lc, 0x30)));
}

// A definition that matches the unit is never changed
void test_matching_definition_kept(void)
{
    int da = catalogFindModel("Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW)");
    LayoutChecker lc;
    layoutCheckInit(lc, da);
    feed(lc, REAL_EPGA_CYCLES);
    for (uint8_t i = 0; i < lc.count; i++)
        TEST_ASSERT_EQUAL_INT(-1, layoutCheckDecide(lc.regs[i]));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_checked_registries);
    RUN_TEST(test_real_unit_fixes_0x21);
    RUN_TEST(test_matching_definition_kept);
    UNITY_END();
    return 0;
}
