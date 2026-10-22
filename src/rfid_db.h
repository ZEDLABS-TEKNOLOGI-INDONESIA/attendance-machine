#pragma once
static void clearRfidCacheRaw()
{
  memset(rfidCacheFlat, 0, sizeof(rfidCacheFlat));
  rfidCacheCount = 0;
  rfidCacheLoaded = false;
  rfidDbValid = false;
}

void clearRfidCache()
{
  if (!lockCache(pdMS_TO_TICKS(3000)))
    return;
  clearRfidCacheRaw();
  unlockCache();
}

static bool loadRfidCacheImpl()
{
  clearRfidCacheRaw();
  rfidCacheDiscarded = 0;

  if (!sd.exists(RFID_DB_FILE) && sd.exists(RFID_DB_BAK))
    sd.rename(RFID_DB_BAK, RFID_DB_FILE);
  if (!sd.exists(RFID_DB_FILE))
  {
    return false;
  }
  FsFile f;
  if (!f.open(RFID_DB_FILE, O_RDONLY))
  {
    return false;
  }
  char line[12];
  int idx = 0;
  long discarded = 0;
  while (f.fgets(line, sizeof(line)) > 0 && idx < RFID_CACHE_MAX)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    if (len != 10)
      continue;
    bool ok = true;
    for (int j = 0; j < 10 && ok; j++)
      ok = isdigit((unsigned char)line[j]);
    if (!ok)
      continue;
    memcpy(rfidCacheFlat[idx], line, 10);
    rfidCacheFlat[idx][10] = '\0';
    idx++;
  }
  if (idx >= RFID_CACHE_MAX)
  {
    while (f.fgets(line, sizeof(line)) > 0)
    {
      esp_task_wdt_reset();
      taskYIELD();
      int len = strlen(line);
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
      if (len == 10)
        discarded++;
    }
  }
  f.close();
  rfidCacheCount = idx;
  rfidCacheDiscarded = (int)discarded;

  rfidCacheLoaded = true;
  rfidDbValid = true;
  return true;
}

bool loadRfidCacheFromFileLocked()
{
  if (!lockCache(pdMS_TO_TICKS(3000)))
    return false;
  bool ok = loadRfidCacheImpl();
  unlockCache();
  return ok;
}

bool loadRfidCacheFromFile()
{
  if (!sdCardAvailable)
    return false;
  if (!acquireSD())
    return false;
  selectSD();
  bool ok = loadRfidCacheImpl();
  deselectSD();
  releaseSD();
  return ok;
}

static RfidLookup rfidLookupCache(const char *rfid)
{
  if (!lockCache(pdMS_TO_TICKS(2500)))
    return RFID_BUSY;

  RfidLookup result = RFID_NOT_FOUND;
  if (!rfidDbValid || !rfidCacheLoaded)
  {
    result = RFID_NO_DB;
  }
  else
  {
    for (int i = 0; i < rfidCacheCount; i++)
    {
      if (strcmp(rfidCacheFlat[i], rfid) == 0)
      {
        result = RFID_FOUND;
        break;
      }
    }
  }
  unlockCache();
  return result;
}

void loadAdminRfidList()
{
  char tmp[5][11];
  int cnt = 0;
  if (!sdCardAvailable)
    return;
  if (!acquireSD())
    return;
  selectSD();
  FsFile f;
  if (sd.exists(ADMIN_RFID_FILE) && f.open(ADMIN_RFID_FILE, O_RDONLY))
  {
    char line[12];
    while (cnt < 5 && f.fgets(line, sizeof(line)) > 0)
    {
      int len = strlen(line);
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
      if (len == 10)
      {
        memcpy(tmp[cnt], line, 11);
        cnt++;
      }
    }
    f.close();
  }
  deselectSD();
  releaseSD();
  portENTER_CRITICAL(&wdtMux);
  memcpy(adminRfidList, tmp, sizeof(adminRfidList));
  adminRfidCount = cnt;
  portEXIT_CRITICAL(&wdtMux);
}

bool isAdminRfid(const char *rfid)
{
  bool found = false;
  portENTER_CRITICAL(&wdtMux);
  for (int i = 0; i < adminRfidCount; i++)
    if (strcmp(adminRfidList[i], rfid) == 0)
    {
      found = true;
      break;
    }
  portEXIT_CRITICAL(&wdtMux);
  return found;
}

void handleAdminScan(const char *rfid)
{
  (void)rfid;
  showOLED(F("ADMIN MODE"), "SYNC + STATUS");
  playToneNotify();
  char buf[24];
  snprintf(buf, sizeof(buf), "Q:%d SC:%d", cachedPendingRecords + nvsGetCount(), nvsGetScanCount());
  showOLED(F("STATUS"), buf);
  delay(2000);
  if (isWifiConnected())
  {
    pendingCacheDirty = true;
    forceSyncRequested = true;
  }
}
