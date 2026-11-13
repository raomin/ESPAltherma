#include <unity.h>
#include <stdio.h>
#include <string.h>
#include "defrostlayout.h"

void test_actual_erga_layout_is_supported_and_decodes_recorded_values()
{
    int model=catalogFindModel("Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW)");
    TEST_ASSERT_GREATER_OR_EQUAL(0,model);
    DefrostLayout layout;
    TEST_ASSERT_TRUE(layout.configure(model));
    DefrostInput in;
    const uint8_t mode[]={1,0x20};
    const uint8_t temperatures[]={20,0,0,0,0,0,0,0,0x97,0xFF};
    const uint8_t frequency[]={48};
    layout.decode(0x10,mode,sizeof(mode),in);
    layout.decode(0x20,temperatures,sizeof(temperatures),in);
    layout.decode(0x30,frequency,sizeof(frequency),in);
    TEST_ASSERT_TRUE(in.usable());
    TEST_ASSERT_TRUE(in.flags&DF_STARTUP);
    TEST_ASSERT_FALSE(in.flags&DF_DEFROST);
    TEST_ASSERT_EQUAL(20,in.outdoorTenths);
    TEST_ASSERT_EQUAL(-105,in.coilTenths);
    TEST_ASSERT_EQUAL(48,in.rps);
}

void test_missing_short_and_unmapped_layouts_do_not_guess()
{
    DefrostLayout layout;
    TEST_ASSERT_FALSE(layout.configure(-1));
    TEST_ASSERT_FALSE(layout.configure(catalogFindModel("PROTOCOL_S_ROTEX")));
    DefrostInput in;const uint8_t bytes[]={1,0x10};
    layout.decode(0x10,bytes,sizeof(bytes),in);
    TEST_ASSERT_FALSE(in.usable());
    layout.modeConverter=201;
    layout.decode(0x20,bytes,sizeof(bytes),in);
    TEST_ASSERT_EQUAL(INT16_MIN,in.coilTenths);
}

int main(int,char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_actual_erga_layout_is_supported_and_decodes_recorded_values);
    RUN_TEST(test_missing_short_and_unmapped_layouts_do_not_guess);
    return UNITY_END();
}
