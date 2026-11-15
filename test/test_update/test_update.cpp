#include <string.h>
#include <unity.h>

#include "fwupdate.h"

void setUp(void) {}
void tearDown(void) {}

// Versions rank as the web page ranks them: alpha < beta < rc < release, then the number
void test_version_rank(void)
{
    int r[5];
    TEST_ASSERT_TRUE(versionRank("2.0.0-beta.7", r));
    TEST_ASSERT_EQUAL_INT(2, r[0]);
    TEST_ASSERT_EQUAL_INT(0, r[1]);
    TEST_ASSERT_EQUAL_INT(0, r[2]);
    TEST_ASSERT_EQUAL_INT(1, r[3]);
    TEST_ASSERT_EQUAL_INT(7, r[4]);
    TEST_ASSERT_TRUE(versionRank("2.0.0", r));
    TEST_ASSERT_EQUAL_INT(3, r[3]);
    TEST_ASSERT_EQUAL_INT(0, r[4]);
    TEST_ASSERT_TRUE(versionRank("2.1.3-rc1", r));
    TEST_ASSERT_EQUAL_INT(2, r[3]);
    TEST_ASSERT_EQUAL_INT(1, r[4]);
    TEST_ASSERT_FALSE(versionRank("2.0.0-dev", r));
    TEST_ASSERT_FALSE(versionRank("dev-1a2b3c4", r));
    TEST_ASSERT_FALSE(versionRank("", r));
    TEST_ASSERT_FALSE(versionRank(nullptr, r));
}

void test_newer_by_version(void)
{
    TEST_ASSERT_TRUE(updateNewer("2.0.0-beta.19", 0, "2.0.0-beta.18", 0));
    TEST_ASSERT_FALSE(updateNewer("2.0.0-beta.18", 0, "2.0.0-beta.18", 0));
    TEST_ASSERT_FALSE(updateNewer("2.0.0-beta.17", 0, "2.0.0-beta.18", 0));
    TEST_ASSERT_TRUE(updateNewer("2.0.0", 0, "2.0.0-beta.18", 0));   // the release follows its betas
    TEST_ASSERT_TRUE(updateNewer("2.0.0-rc.1", 0, "2.0.0-beta.18", 0));
    TEST_ASSERT_TRUE(updateNewer("2.1.0", 0, "2.0.9", 0));
    TEST_ASSERT_FALSE(updateNewer("1.9.9", 0, "2.0.0", 0));
}

// The commit time decides when both builds carry one (CI builds): a development build from the preview site
// can be ahead of the last release, and a release ahead of an old development build
void test_newer_by_build_time(void)
{
    TEST_ASSERT_TRUE(updateNewer("2.0.0-beta.19", 1800, "dev-1a2b3c4", 1700));
    TEST_ASSERT_FALSE(updateNewer("2.0.0-beta.19", 1700, "dev-1a2b3c4", 1800));
    TEST_ASSERT_FALSE(updateNewer("2.0.0-beta.19", 1800, "2.0.0-beta.19", 1800));
    // the build time wins over the version strings
    TEST_ASSERT_FALSE(updateNewer("2.0.0-beta.20", 1700, "2.0.0-beta.19", 1800));
}

// A development build on either side, without build times, is never "newer": the page offers those by hand
void test_development_builds_are_not_offered(void)
{
    TEST_ASSERT_FALSE(updateNewer("2.0.0-beta.19", 0, "2.0.0-dev", 0));
    TEST_ASSERT_FALSE(updateNewer("2.0.0-beta.19", 1800, "2.0.0-dev", 0));
    TEST_ASSERT_FALSE(updateNewer("dev-1a2b3c4", 0, "2.0.0-beta.18", 0));
    TEST_ASSERT_FALSE(updateNewer("", 1800, "2.0.0-beta.18", 1700));
}

// The polls wait while the blocks of a web update keep coming, and no longer than that
void test_upload_in_progress(void)
{
    TEST_ASSERT_FALSE(uploadInProgress(0, 5000)); // no upload
    TEST_ASSERT_TRUE(uploadInProgress(5000, 5000));
    TEST_ASSERT_TRUE(uploadInProgress(5000, 5000 + UPLOAD_QUIET_MS - 1));
    TEST_ASSERT_FALSE(uploadInProgress(5000, 5000 + UPLOAD_QUIET_MS)); // gone quiet: the polls go on
    // across the wrap of millis() after 49 days
    TEST_ASSERT_TRUE(uploadInProgress(0xFFFFFF00u, 0x00000100u));
    TEST_ASSERT_FALSE(uploadInProgress(0xFFFFFF00u, 0x00000100u + UPLOAD_QUIET_MS));
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_version_rank);
    RUN_TEST(test_newer_by_version);
    RUN_TEST(test_newer_by_build_time);
    RUN_TEST(test_development_builds_are_not_offered);
    RUN_TEST(test_upload_in_progress);
    return UNITY_END();
}
