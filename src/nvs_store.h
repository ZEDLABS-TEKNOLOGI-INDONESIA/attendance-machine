#pragma once
void nvsSaveLastTime(time_t t)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putULong(NVS_KEY_LAST_TIME, (unsigned long)t);
  prefs.end();
  unlockNvs();
}

time_t nvsLoadLastTime()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  unsigned long t = prefs.getULong(NVS_KEY_LAST_TIME, 0);
  prefs.end();
  unlockNvs();
  return (time_t)t;
}

void nvsBumpScanCount()
{
  struct tm ti;
  if (!getTimeWithFallback(&ti))
    return;
  char today[9];
  snprintf(today, sizeof(today), "%04u%02u%02u",
           (unsigned)(ti.tm_year + 1900) % 10000u,
           (unsigned)(ti.tm_mon + 1) % 100u,
           (unsigned)ti.tm_mday % 100u);
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  char stored[9] = "";
  prefs.getString(NVS_KEY_SCAN_DATE, stored, sizeof(stored));
  int cnt = strcmp(stored, today) == 0 ? prefs.getInt(NVS_KEY_SCAN_COUNT, 0) : 0;
  prefs.putString(NVS_KEY_SCAN_DATE, today);
  prefs.putInt(NVS_KEY_SCAN_COUNT, cnt + 1);
  prefs.end();
  unlockNvs();
}

int nvsGetScanCount()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int c = prefs.getInt(NVS_KEY_SCAN_COUNT, 0);
  prefs.end();
  unlockNvs();
  return c;
}

int nvsGetBootRetry()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int c = prefs.getInt(NVS_KEY_BOOT_RETRY, 0);
  prefs.end();
  unlockNvs();
  return c;
}

void nvsSetBootRetry(int v)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  if (prefs.getInt(NVS_KEY_BOOT_RETRY, 0) != v)
    prefs.putInt(NVS_KEY_BOOT_RETRY, v);
  prefs.end();
  unlockNvs();
}

static int nvsGetI(const char *key)
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int v = prefs.getInt(key, 0);
  prefs.end();
  unlockNvs();
  return v;
}

static void nvsSetIChanged(const char *key, int v)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  if (prefs.getInt(key, 0) != v)
    prefs.putInt(key, v);
  prefs.end();
  unlockNvs();
}
