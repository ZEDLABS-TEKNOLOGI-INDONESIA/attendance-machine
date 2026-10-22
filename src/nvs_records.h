#pragma once
int nvsGetCount()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int c = prefs.getInt(NVS_KEY_COUNT, 0);
  prefs.end();
  unlockNvs();
  return c;
}

void nvsSetCount(int count)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(NVS_KEY_COUNT, count);
  prefs.end();
  unlockNvs();
}

bool nvsLoadRecord(int idx, OfflineRecord &rec)
{
  char key[16];
  snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, idx);
  if (!lockNvs())
    return false;
  prefs.begin(NVS_NAMESPACE, true);
  size_t len = prefs.getBytesLength(key);
  if (len != sizeof(OfflineRecord))
  {
    prefs.end();
    unlockNvs();
    return false;
  }
  prefs.getBytes(key, &rec, sizeof(OfflineRecord));
  prefs.end();
  unlockNvs();
  return true;
}

bool nvsSaveRecord(int idx, const OfflineRecord &rec)
{
  char key[16];
  snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, idx);
  if (!lockNvs())
    return false;
  prefs.begin(NVS_NAMESPACE, false);
  size_t w = prefs.putBytes(key, &rec, sizeof(OfflineRecord));
  prefs.end();
  unlockNvs();
  return w == sizeof(OfflineRecord);
}

void nvsDeleteRecord(int idx)
{
  char key[16];
  snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, idx);
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.remove(key);
  prefs.end();
  unlockNvs();
}

bool nvsIsDuplicate(const char *rfid, unsigned long t)
{
  int cnt = nvsGetCount();
  for (int i = 0; i < cnt; i++)
  {
    OfflineRecord rec;
    if (!nvsLoadRecord(i, rec))
      continue;
    if (strcmp(rec.rfid, rfid) == 0 && t >= rec.unixTime && (t - rec.unixTime) < MIN_REPEAT_INTERVAL)
      return true;
  }
  return false;
}

bool nvsSaveToBuffer(const char *rfid, const char *ts, unsigned long t)
{
  if (!lockNvs(pdMS_TO_TICKS(2000)))
    return false;

  prefs.begin(NVS_NAMESPACE, false);
  int cnt = prefs.getInt(NVS_KEY_COUNT, 0);
  bool ok = false;

  if (cnt < NVS_MAX_RECORDS)
  {
    OfflineRecord rec = {};
    strncpy(rec.rfid, rfid, sizeof(rec.rfid) - 1);
    strncpy(rec.timestamp, ts, sizeof(rec.timestamp) - 1);
    strncpy(rec.deviceId, deviceId, sizeof(rec.deviceId) - 1);
    rec.unixTime = t;

    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, cnt);
    if (prefs.putBytes(key, &rec, sizeof(OfflineRecord)) == sizeof(OfflineRecord))
    {
      prefs.putInt(NVS_KEY_COUNT, cnt + 1);
      ok = true;
    }
  }

  prefs.end();
  unlockNvs();
  return ok;
}

unsigned long nvsGetRfidDbVer()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  unsigned long v = prefs.getULong(NVS_KEY_RFID_VER, 0);
  prefs.end();
  unlockNvs();
  return v;
}

void nvsSetRfidDbVer(unsigned long ver)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putULong(NVS_KEY_RFID_VER, ver);
  prefs.end();
  unlockNvs();
}

void nvsSaveLastScan(const char *rfid, unsigned long t)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_LAST_RFID, rfid);
  prefs.putULong(NVS_KEY_LAST_SCAN_T, t);
  prefs.end();
  unlockNvs();
}

bool nvsIsRecentScan(const char *rfid, unsigned long t)
{
  char stored[11] = "";
  if (!lockNvs())
    return false;
  prefs.begin(NVS_NAMESPACE, true);
  prefs.getString(NVS_KEY_LAST_RFID, stored, sizeof(stored));
  unsigned long storedT = prefs.getULong(NVS_KEY_LAST_SCAN_T, 0);
  prefs.end();
  unlockNvs();
  if (stored[0] == '\0' || storedT == 0)
    return false;
  if (strcmp(stored, rfid) != 0)
    return false;
  return (t >= storedT && (t - storedT) < MIN_REPEAT_INTERVAL);
}
