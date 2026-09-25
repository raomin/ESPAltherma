#include <string>
#include <unity.h>

#include "survey.h"

// Fake heat pump: answers the registries it has, like the X10A port would.
struct FakeReg
{
    uint8_t id;
    uint8_t len;
    uint8_t payload[SURVEY_MAX_PAYLOAD];
};

static char fakeProtocol;
static const FakeReg *fakeRegs;
static int fakeRegCount;
static int dropNextQueries; // Simulates lost frames
static int queryCount;

static unsigned char crc(const unsigned char *src, int len)
{
    unsigned char b = 0;
    for (int i = 0; i < len; i++)
        b += src[i];
    return ~b;
}

static bool fakeQuery(uint8_t regID, unsigned char *buffer, char protocol)
{
    queryCount++;
    if (dropNextQueries > 0)
    {
        dropNextQueries--;
        return false;
    }
    if (protocol != fakeProtocol)
        return false;
    for (int i = 0; i < fakeRegCount; i++)
    {
        const FakeReg &r = fakeRegs[i];
        if (r.id != regID)
            continue;
        if (protocol == 'I')
        {
            buffer[0] = 0x40;
            buffer[1] = regID;
            buffer[2] = r.len + 2;
            memcpy(buffer + 3, r.payload, r.len);
            buffer[3 + r.len] = crc(buffer, 3 + r.len);
        }
        else
        {
            buffer[0] = regID;
            memcpy(buffer + 1, r.payload, r.len);
            buffer[1 + r.len] = crc(buffer, 1 + r.len);
        }
        return true;
    }
    return false; // Unknown registry: 0x15 0xEA or timeout
}

static void useFake(char protocol, const FakeReg *regs, int count)
{
    fakeProtocol = protocol;
    fakeRegs = regs;
    fakeRegCount = count;
    dropNextQueries = 0;
    queryCount = 0;
}

// L-family (Gen-1) layout: no 0xA0/0xA1, no 0x65.
static const FakeReg L_UNIT[] = {
    {0x00, 13, {0x05, 0x01, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x12, 0x34, 0x50}},
    {0x10, 18, {0x01}},
    {0x11, 6, {0x01, 0x70, 0x53, 0x48, 0x01, 0x00}}, // outdoor 1P705348-1x (a real EPGA16DAV3)
    {0x20, 14, {0}},
    {0x21, 20, {0}},
    {0x30, 11, {0}},
    {0x60, 17, {0x00, 0x00, 0x00, 0x00, 0xAB, 0xCD, 0x5A, 0, 0, 0, 0, 0, 0, 0x02, 0x91, 0x82, 0x07}},
    {0x61, 16, {0}},
    {0x62, 16, {0}},
    {0x63, 9, {0x00, 0x00, 0x01, 0x70, 0x81, 0x71, 0x03, 0x06, 0x00}}, // indoor AS1708171-30 (a real EAVH16S18DA6V)
    {0x64, 12, {0}},
};

// PROTOCOL_S_ROTEX: 0x53..0x56, no 0x50.
static const FakeReg ROTEX_UNIT[] = {
    {0x53, 16, {0x01}},
    {0x54, 16, {0x98, 0x1e}},
    {0x55, 16, {0}},
    {0x56, 2, {0x00, 0x01}},
};

void setUp(void) {}
void tearDown(void) {}

void test_survey_protocol_i(void)
{
    useFake('I', L_UNIT, sizeof(L_UNIT) / sizeof(FakeReg));
    Survey s;
    surveyRun(s, fakeQuery);

    TEST_ASSERT_EQUAL_CHAR('I', s.protocol);
    TEST_ASSERT_TRUE(s.has(0x60));
    TEST_ASSERT_TRUE(s.has(0x64));
    TEST_ASSERT_FALSE(s.has(0xA0));
    TEST_ASSERT_FALSE(s.has(0xA1));
    // 0x65 is the first missing one of the 0x61.. walk: it is recorded, the walk stops there
    TEST_ASSERT_NOT_NULL(s.find(0x65));
    TEST_ASSERT_FALSE(s.has(0x65));
    TEST_ASSERT_NULL(s.find(0x66));
    // 0x60 + 8 listed + 0x61..0x65
    TEST_ASSERT_EQUAL_UINT8(14, s.count);

    TEST_ASSERT_EQUAL_UINT8(17, s.find(0x60)->len);
    TEST_ASSERT_EQUAL_INT(0x5A, s.byteAt(0x60, 6));
    TEST_ASSERT_EQUAL_INT(-1, s.byteAt(0x60, 17));
    TEST_ASSERT_EQUAL_INT(-1, s.byteAt(0xA0, 0));

    char key[96];
    surveyKey(s, key, sizeof(key));
    TEST_ASSERT_EQUAL_STRING("I|63:017081710306|60:9182|CAP:5A|11:017053480100|00:1234", key);

    AsNumber as;
    TEST_ASSERT_TRUE(surveyAsNumber(s, as));
    TEST_ASSERT_EQUAL_UINT32(1708171, as.body);
    TEST_ASSERT_EQUAL_INT(3, as.suffix1);
    TEST_ASSERT_EQUAL_INT(0, as.suffix2);
    TEST_ASSERT_EQUAL_CHAR('F', as.letter);
}

void test_survey_json(void)
{
    useFake('I', L_UNIT, sizeof(L_UNIT) / sizeof(FakeReg));
    Survey s;
    surveyRun(s, fakeQuery);
    char json[2048];
    size_t len = surveyToJson(s, json, sizeof(json), "1.2.3", "esp32", "{\"family\":\"L\"}");
    std::string j(json);

    TEST_ASSERT_EQUAL_size_t(strlen(json), len);
    TEST_ASSERT_EQUAL_STRING("{\"v\":1,\"fw\":\"1.2.3\",\"board\":\"esp32\",\"protocol\":\"I\",", j.substr(0, 51).c_str());
    TEST_ASSERT_TRUE(j.find("\"iu_eeprom\":\"017081710306\"") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"iu_as\":\"AS1708171-30 F\"") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"iu_sw\":\"9182\"") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"iu_cap\":90") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"iu_eeprom_ver\":7") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"iu_code\":\"ABCD\"") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"ou_pn\":\"1P705348-1\"") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"ou_mpu\":\"1234\"") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"ou_cap\":80}") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"A0\":null") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"65\":null") != std::string::npos);
    TEST_ASSERT_TRUE(j.find("\"11\":\"017053480100\"") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING(",\"detect\":{\"family\":\"L\"}}", j.substr(j.size() - 25).c_str());
}

void test_survey_protocol_s(void)
{
    useFake('S', ROTEX_UNIT, sizeof(ROTEX_UNIT) / sizeof(FakeReg));
    Survey s;
    surveyRun(s, fakeQuery);

    TEST_ASSERT_EQUAL_CHAR('S', s.protocol);
    TEST_ASSERT_TRUE(s.has(0x53));
    TEST_ASSERT_TRUE(s.has(0x56));
    TEST_ASSERT_FALSE(s.has(0x50));
    TEST_ASSERT_FALSE(s.has(0x60)); // failed Protocol I probe is not kept
    TEST_ASSERT_EQUAL_INT(0x98, s.byteAt(0x54, 0));

    char key[96];
    surveyKey(s, key, sizeof(key));
    TEST_ASSERT_EQUAL_STRING("S|50:0|56:1", key);
}

void test_survey_no_answer(void)
{
    useFake('X', nullptr, 0);
    Survey s;
    surveyRun(s, fakeQuery);
    TEST_ASSERT_EQUAL_CHAR(0, s.protocol);
    TEST_ASSERT_EQUAL_UINT8(0, s.count);

    char json[256];
    surveyToJson(s, json, sizeof(json), "1.2.3", "esp32", nullptr);
    TEST_ASSERT_EQUAL_STRING("{\"v\":1,\"fw\":\"1.2.3\",\"board\":\"esp32\",\"protocol\":null,\"key\":\"\",\"regs\":{}}", json);
}

void test_survey_retries_lost_frame(void)
{
    useFake('I', L_UNIT, sizeof(L_UNIT) / sizeof(FakeReg));
    dropNextQueries = 1; // the first 0x60 probe is lost
    Survey s;
    surveyRun(s, fakeQuery);
    TEST_ASSERT_EQUAL_CHAR('I', s.protocol);
    TEST_ASSERT_TRUE(s.has(0x60));
}

void test_survey_json_never_overflows(void)
{
    useFake('I', L_UNIT, sizeof(L_UNIT) / sizeof(FakeReg));
    Survey s;
    surveyRun(s, fakeQuery);
    char json[64];
    memset(json, 'x', sizeof(json));
    size_t len = surveyToJson(s, json, 40, "1.2.3", "esp32", nullptr);
    TEST_ASSERT_EQUAL_size_t(39, len);
    TEST_ASSERT_EQUAL_CHAR(0, json[39]);
    TEST_ASSERT_EQUAL_CHAR('x', json[40]);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_survey_protocol_i);
    RUN_TEST(test_survey_json);
    RUN_TEST(test_survey_protocol_s);
    RUN_TEST(test_survey_no_answer);
    RUN_TEST(test_survey_retries_lost_frame);
    RUN_TEST(test_survey_json_never_overflows);
    UNITY_END();
    return 0;
}
