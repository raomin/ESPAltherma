#include <stdio.h>
#include <unity.h>

#include "detect.h"

// The survey a unit of this model would give: its registries answer, replies are exactly as long as the
// model reads, and physical values are plausible. Registries are listed in the order surveyRun reads them.
static void syntheticSurvey(int model, Survey &s)
{
    memset(&s, 0, sizeof(s));
    const CatalogModel &m = CATALOG_MODELS[model];
    s.protocol = m.protocol;

    uint8_t maxEnd[256] = {0};
    bool used[256] = {false};
    for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
    {
        const CatalogEntry &e = CATALOG_ENTRIES[i];
        if (!catalogInModel(e, model) || e.size == 0)
            continue;
        used[e.reg] = true;
        if (e.offset + e.size > maxEnd[e.reg])
            maxEnd[e.reg] = e.offset + e.size;
    }

    uint8_t order[32];
    int n = 0;
    if (m.protocol == 'I')
    {
        order[n++] = 0x60;
        for (uint8_t reg : SURVEY_REGS_I)
            order[n++] = reg;
        for (uint8_t reg = 0x61; reg <= 0x6F; reg++)
        {
            order[n++] = reg;
            if (!used[reg])
                break;
        }
    }
    else
    {
        order[n++] = 0x53;
        for (uint8_t reg : SURVEY_REGS_S)
            order[n++] = reg;
    }

    for (int i = 0; i < n; i++)
    {
        SurveyReg &r = s.regs[s.count++];
        r.id = order[i];
        r.answered = used[r.id];
        r.len = r.answered ? maxEnd[r.id] : 0;
    }

    // Outdoor capacity (registry 0x00 offset 12, kW x10) in the range of the model
    SurveyReg *r0 = const_cast<SurveyReg *>(s.find(0x00));
    if (m.capMax > 0 && r0 != nullptr && r0->answered && r0->len > 12)
        r0->payload[12] = (m.capMin + m.capMax) * 5;

    for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
    {
        const CatalogEntry &e = CATALOG_ENTRIES[i];
        if (!catalogInModel(e, model) || e.size == 0)
            continue;
        SurveyReg *r = const_cast<SurveyReg *>(s.find(e.reg));
        uint8_t *p = r->payload + e.offset;
        int v = -1;
        switch (detectQuantity(e))
        {
        case QTY_TEMP:
            v = 200; // 20.0 C
            break;
        case QTY_VOLTAGE:
            v = e.conv == 105 ? 2300 : 230;
            break;
        case QTY_CURRENT:
            v = 50;
            break;
        case QTY_FLOW:
            v = 150;
            break;
        default:
            break;
        }
        if (v < 0)
            continue;
        bool bigEndian = e.conv == 152 || e.conv == 161;
        if (e.size == 1)
            p[0] = v & 0xff;
        else if (bigEndian)
        {
            p[0] = v >> 8;
            p[1] = v & 0xff;
        }
        else
        {
            p[0] = v & 0xff;
            p[1] = v >> 8;
        }
    }
}

void setUp(void) {}
void tearDown(void) {}

void test_catalog_models(void)
{
    TEST_ASSERT_TRUE(CATALOG_MODEL_COUNT > 30);
    TEST_ASSERT_TRUE(catalogFindModel("PROTOCOL_S") >= 0);
    TEST_ASSERT_TRUE(catalogFindModel("PROTOCOL_S_ROTEX") >= 0);
    TEST_ASSERT_EQUAL_INT(-1, catalogFindModel("nope"));
    TEST_ASSERT_EQUAL_INT(-1, catalogFindModel(""));
    TEST_ASSERT_EQUAL_INT(-1, catalogFindModel(nullptr));
    for (int i = 0; i < CATALOG_MODEL_COUNT; i++)
    {
        const CatalogModel &m = CATALOG_MODELS[i];
        TEST_ASSERT_EQUAL_CHAR(m.family == 'S' || m.family == 'R' ? 'S' : 'I', m.protocol);
    }
}

void test_catalog_build_recommended(void)
{
    int model = catalogFindModel("Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW)");
    TEST_ASSERT_TRUE(model >= 0);
    std::vector<LabelDef> labels;
    catalogBuildLabels(model, nullptr, 0, labels);
    TEST_ASSERT_EQUAL_size_t(30, labels.size());
    bool hasMode = false;
    for (auto &l : labels)
    {
        TEST_ASSERT_EQUAL_STRING("", l.asString);
        if (strcmp(l.label, "Operation Mode") == 0)
        {
            hasMode = true;
            TEST_ASSERT_EQUAL_INT(0x10, l.registryID);
            TEST_ASSERT_EQUAL_INT(0, l.offset);
            TEST_ASSERT_EQUAL_INT(217, l.convid);
        }
    }
    TEST_ASSERT_TRUE(hasMode);
}

void test_catalog_build_selection(void)
{
    int model = catalogFindModel("Altherma(ERGA E EHV-EHB-EHVZ E_EJ series 04-08kW)");
    // Operation Mode {0x10,0,217,1} and a key of no value of this model
    uint32_t keys[] = {0x10u << 24 | 0u << 16 | 217u << 4 | 1u, 0xEE000011u};
    std::vector<LabelDef> labels;
    catalogBuildLabels(model, keys, 2, labels);
    TEST_ASSERT_EQUAL_size_t(1, labels.size());
    TEST_ASSERT_EQUAL_STRING("Operation Mode", labels[0].label);

    int s = catalogFindModel("PROTOCOL_S");
    catalogBuildLabels(s, nullptr, 0, labels);
    TEST_ASSERT_EQUAL_size_t(25, labels.size());
}

void test_catalog_keys_are_stable(void)
{
    // Same key for the same slot whatever the model: selections survive a model change within a family
    const CatalogEntry *first = nullptr;
    for (int i = 0; i < CATALOG_ENTRY_COUNT; i++)
    {
        const CatalogEntry &e = CATALOG_ENTRIES[i];
        if (e.reg == 0x10 && e.offset == 0 && e.conv == 217)
        {
            if (first == nullptr)
                first = &e;
            TEST_ASSERT_EQUAL_HEX32(catalogKey(*first), catalogKey(e));
        }
    }
    TEST_ASSERT_NOT_NULL(first);
}

void test_detect_no_answer(void)
{
    Survey s;
    memset(&s, 0, sizeof(s));
    DetectResult r;
    detectModel(s, r);
    TEST_ASSERT_EQUAL_INT(-1, r.model);
    TEST_ASSERT_EQUAL_INT(DETECT_NONE, r.confidence);
    char json[128];
    detectToJson(r, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING("{\"model\":null,\"confidence\":\"none\"}", json);
}

// Every model's own synthetic survey must be detected in the right family, and as that model or an
// equivalent one (same recommended values) for most of them.
void test_detect_synthetic_surveys(void)
{
    int exact = 0, equivalent = 0, familyOk = 0, high = 0;
    for (int model = 0; model < CATALOG_MODEL_COUNT; model++)
    {
        Survey s;
        syntheticSurvey(model, s);
        DetectResult r;
        detectModel(s, r);
        TEST_ASSERT_TRUE(r.model >= 0);
        bool same = r.model == model;
        bool equiv = same || detectDecodeHash(r.model) == detectDecodeHash(model);
        bool family = CATALOG_MODELS[r.model].family == CATALOG_MODELS[model].family;
        exact += same;
        equivalent += equiv;
        familyOk += family;
        high += r.confidence == DETECT_HIGH;
        uint32_t keys[CATALOG_MAX_SELECTED];
        int safe = detectSafeKeys(r, keys, CATALOG_MAX_SELECTED);
        printf("%-5s %-6s %-3d safe %-3d %c %-62s -> %s\n", same ? "exact" : (equiv ? "equiv" : "WRONG"),
               detectConfidenceName(r.confidence), r.margin, safe, CATALOG_MODELS[model].family,
               CATALOG_MODELS[model].name, CATALOG_MODELS[r.model].name);
        // A wrong model must never be claimed with high confidence
        if (!equiv)
            TEST_ASSERT_NOT_EQUAL(DETECT_HIGH, r.confidence);
        // The values published before the user confirms must decode right on the real model
        TEST_ASSERT_TRUE(safe == -1 || safe >= 10);
        for (int k = 0; k < safe; k++)
            TEST_ASSERT_TRUE(detectModelHasKey(model, keys[k]));
    }
    printf("exact %d, equivalent %d, family %d, high confidence %d / %d\n", exact, equivalent, familyOk, high, CATALOG_MODEL_COUNT);
    TEST_ASSERT_EQUAL_INT(CATALOG_MODEL_COUNT, familyOk);
}

// Writes the identification bytes of registry 0x63 (payload offsets 2-7)
static void setAsBytes(Survey &s, const uint8_t bytes[6])
{
    SurveyReg *r = const_cast<SurveyReg *>(s.find(0x63));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(r->answered && r->len >= 8);
    memcpy(r->payload + 2, bytes, 6);
}

// A real unit: EPGA16DAV3 + EAVH16S18DA6V, whose registry 0x63 reads 01 70 81 71 03 (AS1708171-30)
void test_detect_as_number_real_unit(void)
{
    int epga = catalogFindModel("Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW)");
    Survey s;
    syntheticSurvey(epga, s);
    const uint8_t as[6] = {0x01, 0x70, 0x81, 0x71, 0x03, 0x06};
    setAsBytes(s, as);
    DetectResult r;
    detectModel(s, r);
    TEST_ASSERT_EQUAL_INT(epga, r.model);
    TEST_ASSERT_EQUAL_INT(DETECT_HIGH, r.confidence);
    TEST_ASSERT_EQUAL_STRING("as_number", detectMethod(r));
    uint32_t keys[CATALOG_MAX_SELECTED];
    TEST_ASSERT_EQUAL_INT(-1, detectSafeKeys(r, keys, CATALOG_MAX_SELECTED));

    // Spare part board (AS1708171-2, suffix not in the model list): the whole body tells
    const uint8_t spare[6] = {0x01, 0x70, 0x81, 0x71, 0x02, 0x06};
    setAsBytes(s, spare);
    detectModel(s, r);
    TEST_ASSERT_EQUAL_INT(epga, r.model);
    TEST_ASSERT_EQUAL_INT(DETECT_HIGH, r.confidence);
}

// DA and DJ units share their AS numbers (AS1707450-31 is EHVX04S18DA6V and EHVX04S18DJ6V): the AS number
// gives both candidates, the survey tells them apart.
void test_detect_as_number_da_dj(void)
{
    int da = catalogFindModel("Altherma(ERGA D EHV-EHB-EHVZ DA series 04-08kW)");
    int dj = catalogFindModel("Altherma(ERGA D EHV-EHB-EHVZ DJ series 04-08 kW)");
    const uint8_t as[6] = {0x01, 0x70, 0x74, 0x50, 0x03, 0x01};
    for (int model : {da, dj})
    {
        Survey s;
        syntheticSurvey(model, s);
        setAsBytes(s, as);
        DetectResult r;
        detectModel(s, r);
        printf("DA/DJ: %s -> %s (%s, margin %d)\n", CATALOG_MODELS[model].name, CATALOG_MODELS[r.model].name, detectConfidenceName(r.confidence), r.margin);
        TEST_ASSERT_EQUAL_INT(model, r.model);
        TEST_ASSERT_EQUAL_STRING("as_number+rules", detectMethod(r));
        TEST_ASSERT_EQUAL_INT(2, detectPopcount(r.asCandidates));
    }
}

// An AS number the table does not know: detection falls back to the rules alone.
void test_detect_as_number_unknown(void)
{
    int epga = catalogFindModel("Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW)");
    Survey s;
    syntheticSurvey(epga, s);
    const uint8_t as[6] = {0x09, 0x99, 0x99, 0x99, 0x09, 0x00};
    setAsBytes(s, as);
    DetectResult r;
    detectModel(s, r);
    TEST_ASSERT_EQUAL_UINT64(0, r.asCandidates);
    TEST_ASSERT_EQUAL_STRING("rules", detectMethod(r));
}

static uint8_t bcd(int v) { return (uint8_t)((v / 10) << 4 | (v % 10)); }

// Every known AS number, on a synthetic survey of each of its candidate models: the candidate must
// be detected (or an equivalent one), with a high confidence when it is the only candidate.
static bool hasFullSuffix(uint32_t body, int s1, int s2)
{
    for (int i = 0; i < CATALOG_AS_COUNT; i++)
    {
        const CatalogAsNumber &a = CATALOG_AS[i];
        if (a.body == body && a.suffix1 == s1 && a.suffix2 == s2)
            return true;
    }
    return false;
}

// Every known AS number, on a synthetic survey of each of its candidate models: the candidate must
// be detected (or an equivalent one), with a high confidence when it is the only candidate. The second suffix
// digit is written in both layouts that fit the real unit (offset 6 or offset 7 high nibble): same result.
void test_detect_all_as_numbers(void)
{
    int cases = 0, right = 0, high = 0;
    for (int i = 0; i < CATALOG_AS_COUNT; i++)
    {
        const CatalogAsNumber &a = CATALOG_AS[i];
        if (a.suffix1 == 0xFF)
            continue;
        int s2 = a.suffix2;
        if (s2 == 0xFF)
        {
            // A second digit without an entry of its own, so that the first digit entry is the one used
            for (s2 = 0; s2 <= 9 && hasFullSuffix(a.body, a.suffix1, s2); s2++)
                ;
            if (s2 > 9)
                continue;
        }
        const uint8_t letter = 3; // C
        const uint8_t layouts[2][2] = {{(uint8_t)(s2 << 4 | a.suffix1), letter}, {a.suffix1, (uint8_t)(s2 << 4 | letter)}};
        for (int model = 0; model < CATALOG_MODEL_COUNT; model++)
        {
            if (!((a.models >> model) & 1))
                continue;
            int detected[2];
            for (int l = 0; l < 2; l++)
            {
                uint8_t as[6] = {(uint8_t)(a.body / 1000000), bcd(a.body / 10000 % 100), bcd(a.body / 100 % 100), bcd(a.body % 100), layouts[l][0], layouts[l][1]};
                Survey s;
                syntheticSurvey(model, s);
                setAsBytes(s, as);
                DetectResult r;
                detectModel(s, r);
                detected[l] = r.model;
                cases++;
                bool ok = r.model == model || detectDecodeHash(r.model) == detectDecodeHash(model);
                right += ok;
                high += r.confidence == DETECT_HIGH;
                if (!ok)
                {
                    printf("AS%07u-%d%d %s -> %s (%s)\n", (unsigned)a.body, a.suffix1, s2, CATALOG_MODELS[model].name, CATALOG_MODELS[r.model].name, detectConfidenceName(r.confidence));
                    // Never wrong with a high confidence: the user is asked
                    TEST_ASSERT_NOT_EQUAL(DETECT_HIGH, r.confidence);
                }
                if (detectPopcount(a.models) == 1)
                    TEST_ASSERT_EQUAL_INT(DETECT_HIGH, r.confidence);
            }
            TEST_ASSERT_EQUAL_INT(detected[0], detected[1]);
        }
    }
    printf("AS numbers: %d cases, %d right, %d high confidence\n", cases, right, high);
    TEST_ASSERT_TRUE(right >= cases - 4);
}

// The first real survey: EPGA16DAV3 + EAVH16S18DA6V (AS1708171-30 F), registries in the order surveyRun read them
static void realEpgaSurvey(Survey &s)
{
    static const struct
    {
        uint8_t id;
        const char *hex; // nullptr: no answer
    } regs[] = {
        {0x60, "80004000000000F4016801200023948200"}, {0x00, "0D0100020201010204012180A0"},
        {0x10, "0000000000000080F603000000000000"}, {0x11, "017053480100"},
        {0x20, "DE00EC0039010701E800180198009A0001"}, {0x21, "00000000E600A5E8001801EC00070100"},
        {0x30, "00000000000000000000777F"}, {0xA0, nullptr}, {0xA1, nullptr},
        {0x61, "800047013A0118011301F20116010000"}, {0x62, "8000085E01A0000001FFFF00640000BBFF"},
        {0x63, "8000017081710306"}, {0x64, "800002000000000000000000F2010905"}, {0x65, nullptr},
    };
    memset(&s, 0, sizeof(s));
    s.protocol = 'I';
    for (auto &r : regs)
    {
        SurveyReg &sr = s.regs[s.count++];
        sr.id = r.id;
        sr.answered = r.hex != nullptr;
        for (size_t i = 0; r.hex && r.hex[2 * i]; i++)
        {
            unsigned v;
            sscanf(r.hex + 2 * i, "%2x", &v);
            sr.payload[sr.len++] = (uint8_t)v;
        }
    }
}

void test_detect_real_epga(void)
{
    int epga = catalogFindModel("Altherma(EPGA D EAB-EAV-EAVZ D(J) series 11-16kW)");
    Survey s;
    realEpgaSurvey(s);
    DetectResult r;
    detectModel(s, r);
    TEST_ASSERT_EQUAL_INT(epga, r.model);
    TEST_ASSERT_EQUAL_INT(DETECT_HIGH, r.confidence);
    TEST_ASSERT_EQUAL_STRING("as_number", detectMethod(r));

    // Without the AS number: what the rules alone make of a real unit
    const uint8_t noAs[6] = {0, 0, 0, 0, 0, 0};
    setAsBytes(s, noAs);
    detectModel(s, r);
    printf("real EPGA, rules only: %s (%s, margin %d), EPGA scores %d\n", CATALOG_MODELS[r.model].name, detectConfidenceName(r.confidence), r.margin, r.allScores[epga]);
    for (int c = 0; c < DETECT_CANDIDATES; c++)
        printf("   %d %s\n", r.scores[c], CATALOG_MODELS[r.candidates[c]].name);
    // Never sure from the rules alone: the user confirms, and only values every near candidate reads alike are published
    TEST_ASSERT_NOT_EQUAL(DETECT_HIGH, r.confidence);
    uint32_t keys[CATALOG_MAX_SELECTED];
    int safe = detectSafeKeys(r, keys, CATALOG_MAX_SELECTED);
    int wrong = 0;
    for (int k = 0; k < safe; k++)
    {
        if (!detectModelHasKey(epga, keys[k]))
        {
            wrong++;
            printf("   not in EPGA: 0x%02X/%u conv %u\n", keys[k] >> 24, (keys[k] >> 16) & 0xFF, (keys[k] >> 4) & 0x3FF);
        }
    }
    printf("   safe values: %d, not in the EPGA definition: %d\n", safe, wrong);
}

void test_detect_json(void)
{
    Survey s;
    syntheticSurvey(catalogFindModel("PROTOCOL_S_ROTEX"), s);
    DetectResult r;
    detectModel(s, r);
    TEST_ASSERT_EQUAL_INT(catalogFindModel("PROTOCOL_S_ROTEX"), r.model);
    char json[512];
    detectToJson(r, json, sizeof(json));
    TEST_ASSERT_EQUAL_STRING_LEN("{\"model\":\"PROTOCOL_S_ROTEX\",\"family\":\"R\",\"confidence\":\"high\",\"method\":\"rules\"", json, strlen("{\"model\":\"PROTOCOL_S_ROTEX\",\"family\":\"R\",\"confidence\":\"high\",\"method\":\"rules\""));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_catalog_models);
    RUN_TEST(test_catalog_build_recommended);
    RUN_TEST(test_catalog_build_selection);
    RUN_TEST(test_catalog_keys_are_stable);
    RUN_TEST(test_detect_no_answer);
    RUN_TEST(test_detect_synthetic_surveys);
    RUN_TEST(test_detect_as_number_real_unit);
    RUN_TEST(test_detect_as_number_da_dj);
    RUN_TEST(test_detect_as_number_unknown);
    RUN_TEST(test_detect_all_as_numbers);
    RUN_TEST(test_detect_real_epga);
    RUN_TEST(test_detect_json);
    UNITY_END();
    return 0;
}
