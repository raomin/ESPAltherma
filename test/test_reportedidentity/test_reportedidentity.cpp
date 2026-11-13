#include <unity.h>
#include "reportedidentity.h"

void test_actual_evh_identity_does_not_invent_missing_mpu()
{
    Survey s = {}; s.protocol = 'I'; s.hasSwId = true;
    const uint8_t sw[] = {0x03,0x22,0x00,0x20}; memcpy(s.swId,sw,4);
    s.count = 2;
    s.regs[0].id=0x11; s.regs[0].answered=true; s.regs[0].len=6;
    const uint8_t part[] = {0x01,0x50,0x29,0x63,0x07,0x02}; memcpy(s.regs[0].payload,part,6);
    s.regs[1].id=0; s.regs[1].answered=true; s.regs[1].len=10;
    ReportedIdentity id; id.read(s);
    TEST_ASSERT_EQUAL_STRING("20002203", id.hydroSoftware);
    TEST_ASSERT_EQUAL_STRING("L", id.hydroSource);
    TEST_ASSERT_EQUAL_STRING("1P502963-7", id.outdoorPart);
    TEST_ASSERT_EQUAL(2,id.outdoorRevision);
    TEST_ASSERT_EQUAL_STRING("",id.outdoorMpu);
}

void test_sleeping_missing_and_ffff_fields_are_unknown()
{
    Survey s = {}; s.protocol='I'; s.hasSwId=true; memset(s.swId,255,4);
    s.count=2; s.regs[0].id=0x11; s.regs[0].answered=true; s.regs[0].len=6;
    s.regs[1].id=0; s.regs[1].answered=true; s.regs[1].len=12;
    s.regs[1].payload[10]=255; s.regs[1].payload[11]=255;
    ReportedIdentity id; id.read(s);
    TEST_ASSERT_EQUAL_STRING("",id.hydroSoftware);
    TEST_ASSERT_EQUAL_STRING("",id.outdoorPart);
    TEST_ASSERT_EQUAL_STRING("",id.outdoorMpu);
    TEST_ASSERT_EQUAL(-1,id.outdoorRevision);
}

void test_older_software_id_has_explicit_registry_source()
{
    Survey s={};s.protocol='I';s.count=1;s.regs[0].id=0x60;s.regs[0].answered=true;s.regs[0].len=16;
    s.regs[0].payload[14]=0xF2;s.regs[0].payload[15]=0x76;
    ReportedIdentity id;id.read(s);
    TEST_ASSERT_EQUAL_STRING("76F2",id.hydroSoftware);
    TEST_ASSERT_EQUAL_STRING("0x60",id.hydroSource);
    s.protocol='S';id.read(s);
    TEST_ASSERT_EQUAL_STRING("",id.hydroSoftware);
    TEST_ASSERT_NULL(id.hydroSource);
}

int main(int,char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_actual_evh_identity_does_not_invent_missing_mpu);
    RUN_TEST(test_sleeping_missing_and_ffff_fields_are_unknown);
    RUN_TEST(test_older_software_id_has_explicit_registry_source);
    return UNITY_END();
}
