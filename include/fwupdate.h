#ifndef ESPALTHERMA_FWUPDATE_H
#define ESPALTHERMA_FWUPDATE_H

// The daily check for a newer firmware. The board reads the manifest of its own board on the ESPAltherma site, the
// file the web page's "Check for an update" reads too, and notes whether a newer build exists: the home page shows
// it and Home Assistant gets an update entity. Nothing is downloaded or installed by itself; the web page does that.

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define UPDATE_SITE "https://espaltherma.pages.dev/"
#define UPDATE_CHECK_FIRST_MS (3UL * 60UL * 1000UL)         // after boot, once online
#define UPDATE_CHECK_EVERY_MS (24UL * 3600UL * 1000UL)      // after a check
#define UPDATE_CHECK_RETRY_MS (60UL * 60UL * 1000UL)        // after a failed attempt

struct FirmwareUpdate
{
  char latest[24] = "";     // the newest version on the site, once read
  uint32_t latestBuild = 0; // its commit time, when the manifest carries one
  bool checked = false;     // the site answered at least once since boot
  bool available = false;   // the site has a newer build than this firmware
  unsigned long checkedAt = 0;
  bool dirty = false;       // Home Assistant has not been told the current state
};
static FirmwareUpdate fwUpdate;

// "2.0.0-beta.7" -> {2, 0, 0, 1, 7}: alpha < beta < rc < the release. False for a development build ("2.0.0-dev",
// "dev-1a2b3c4"), which has no version order.
static bool versionRank(const char *v, int out[5])
{
  if (v == nullptr)
    return false;
  char *end;
  for (int i = 0; i < 3; i++)
  {
    long n = strtol(v, &end, 10);
    if (end == v || n < 0)
      return false;
    out[i] = (int)n;
    v = end;
    if (i < 2)
    {
      if (*v != '.')
        return false;
      v++;
    }
  }
  out[3] = 3; // a release
  out[4] = 0;
  if (*v == 0)
    return true;
  if (*v != '-')
    return false;
  v++;
  static const char *const stages[] = {"alpha", "beta", "rc"};
  int stage = -1;
  for (int i = 0; i < 3; i++)
    if (strncmp(v, stages[i], strlen(stages[i])) == 0)
    {
      stage = i;
      v += strlen(stages[i]);
    }
  if (stage < 0)
    return false;
  out[3] = stage;
  if (*v == '.')
    v++;
  if (*v)
  {
    long n = strtol(v, &end, 10);
    if (end == v || *end != 0 || n < 0)
      return false;
    out[4] = (int)n;
  }
  return true;
}

// A build is newer when the manifest and this firmware both carry a commit time and the manifest's is later; else
// by the version. A development build on either side is never newer: the web page offers those by hand only.
static bool updateNewer(const char *latest, uint32_t latestBuild, const char *current, uint32_t currentBuild)
{
  if (latest == nullptr || !latest[0])
    return false;
  if (latestBuild && currentBuild)
    return latestBuild > currentBuild;
  int a[5], b[5];
  if (!versionRank(latest, a) || !versionRank(current, b))
    return false;
  for (int i = 0; i < 5; i++)
    if (a[i] != b[i])
      return a[i] > b[i];
  return false;
}

#endif
