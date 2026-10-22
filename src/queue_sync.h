#pragma once
bool readQueueFileLocked(const char *fn, OfflineRecord *recs, int *cnt, int maxCnt)
{
  *cnt = 0;
  if (!file.open(fn, O_RDONLY))
    return false;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  char line[144];
  if (file.available())
    file.fgets(line, sizeof(line));
  while (file.available() && *cnt < maxCnt)
  {
    esp_task_wdt_reset();
    if (file.fgets(line, sizeof(line)) <= 0)
      break;
    OfflineRecord tmp;

    if (classifyQueueLine(line, &tmp, now, nowValid) == LINE_OK)
      recs[(*cnt)++] = tmp;
  }
  file.close();
  return *cnt > 0;
}

static bool detachFromWriterLocked(const char *fn)
{
  int idx;
  if (!parseQueueIndex(fn + 1, &idx))
    return false;
  syncingQueueFile = idx;
  if (idx != currentQueueFile)
    return true;
  int nextIdx;
  if (!findAvailableQueueSlotLocked((currentQueueFile + 1) % MAX_QUEUE_FILES, &nextIdx) || nextIdx == idx)
  {
    syncingQueueFile = -1;
    return false;
  }
  char nfn[20];
  getQueueFileName(nextIdx, nfn, sizeof(nfn));
  if (!sd.exists(nfn))
  {
    if (!file.open(nfn, O_WRONLY | O_CREAT))
    {
      syncingQueueFile = -1;
      return false;
    }
    file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
    file.close();
  }
  currentQueueFile = nextIdx;
  saveMetadataLocked();
  selectSD();
  return true;
}

SyncFileResult syncQueueFile(const char *fn)
{
  if (!sdCardAvailable || !isWifiConnected())
    return SYNC_FILE_NO_WIFI;
  OfflineRecord recs[MAX_RECORDS_PER_FILE];
  int validCnt = 0;
  if (!acquireSD())
    return SYNC_FILE_HTTP_FAIL;
  selectSD();
  if (!sd.exists(fn))
  {
    deselectSD();
    releaseSD();
    pendingCacheDirty = true;
    return SYNC_FILE_EMPTY;
  }
  FsFile probe;
  if (!probe.open(fn, O_RDONLY))
  {
    deselectSD();
    releaseSD();
    return SYNC_FILE_HTTP_FAIL;
  }
  probe.close();
  if (!detachFromWriterLocked(fn))
  {
    deselectSD();
    releaseSD();
    return SYNC_FILE_HTTP_FAIL;
  }
  bool hasData = readQueueFileLocked(fn, recs, &validCnt, MAX_RECORDS_PER_FILE);
  if (!hasData || validCnt == 0)
  {
    bool removed = removeQueueFileSafeLocked(fn, 0);
    deselectSD();
    releaseSD();
    if (!removed)
      return SYNC_FILE_HTTP_FAIL;
    pendingCacheDirty = true;
    return SYNC_FILE_EMPTY;
  }
  deselectSD();
  releaseSD();
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(45000);
  http.setConnectTimeout(15000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/sync-bulk"))
    return SYNC_FILE_HTTP_FAIL;
  if (!http.begin(client, url))
    return SYNC_FILE_HTTP_FAIL;
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  JsonDocument doc;
  JsonArray arr = doc["data"].to<JsonArray>();
  for (int i = 0; i < validCnt; i++)
  {
    JsonObject o = arr.add<JsonObject>();
    o["rfid"] = recs[i].rfid;
    o["timestamp"] = recs[i].timestamp;
    o["device_id"] = recs[i].deviceId;
    o["sync_mode"] = true;
  }
  String payload;
  payload.reserve(measureJson(doc) + 1);
  serializeJson(doc, payload);
  doc.clear();
  esp_task_wdt_reset();
  int code = http.POST(payload);
  esp_task_wdt_reset();
  taskYIELD();
  if (code == 200)
  {
    String body = http.getString();
    esp_task_wdt_reset();
    http.end();
    uint64_t retryMask = 0;
    if (!parseSyncReply(body, validCnt, &retryMask))
    {
      appendFailedLogToSD("BATCH", fn, "RESPONSE_TIDAK_VALID_ATAU_TERPOTONG");
      return SYNC_FILE_HTTP_FAIL;
    }
    isOnline = true;
    int kept = __builtin_popcountll(retryMask);
    if (!acquireSD())
      return SYNC_FILE_HTTP_FAIL;
    selectSD();
    bool removed = (retryMask == 0) ? removeQueueFileSafeLocked(fn, validCnt)
                                    : keepRetryRecordsLocked(fn, validCnt, retryMask);
    deselectSD();
    releaseSD();
    if (!removed)
    {
      appendFailedLogToSD("BATCH", fn, "KARANTINA_GAGAL_FILE_DIPERTAHANKAN");
      return SYNC_FILE_HTTP_FAIL;
    }
    queueMutations = queueMutations + 1;
    pendingCacheDirty = true;
    int done = validCnt - kept;
    if (cachedPendingRecords >= done)
      cachedPendingRecords -= done;
    else
      cachedPendingRecords = 0;
    return SYNC_FILE_OK;
  }
  http.end();
  if (!isWifiConnected())
  {
    syncState.inProgress = false;
    return SYNC_FILE_NO_WIFI;
  }
  return SYNC_FILE_HTTP_FAIL;
}

bool syncQueueFileWithRetry(const char *fn)
{
  bool result = false;
  for (int attempt = 0; attempt <= MAX_SYNC_RETRIES; attempt++)
  {
    esp_task_wdt_reset();
    if (!isWifiConnected())
    {
      syncState.inProgress = false;
      break;
    }
    SyncFileResult r = syncQueueFile(fn);
    if (r == SYNC_FILE_OK || r == SYNC_FILE_EMPTY)
    {
      result = true;
      break;
    }
    if (r == SYNC_FILE_NO_WIFI)
    {
      syncState.inProgress = false;
      break;
    }
    if (attempt < MAX_SYNC_RETRIES)
    {
      char buf[20];
      snprintf(buf, sizeof(buf), "RETRY %d/%d...", attempt + 1, MAX_SYNC_RETRIES);
      showOLED(F("SYNC ULANG"), buf);
      unsigned long t0 = millis(), waitMs = SYNC_RETRY_DELAY_MS * (1UL << attempt);
      while (millis() - t0 < waitMs)
      {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  }
  syncingQueueFile = -1;
  return result;
}

void chunkedSync()
{
  if (!sdCardAvailable || !isWifiConnected())
  {
    syncState.inProgress = false;
    return;
  }
  extendWdtForSync();
  if (!syncState.inProgress)
  {
    syncState.inProgress = true;
    syncState.currentFile = 0;
    syncState.startTime = millis();
    syncState.filesSucceeded = 0;
  }
  syncState.filesProcessed = 0;
  char fn[24];
  bool finished = false;
  while (syncState.filesProcessed < MAX_SYNC_FILES_PER_CYCLE)
  {
    if (!isWifiConnected())
    {
      syncState.inProgress = false;
      break;
    }
    esp_task_wdt_reset();
    taskYIELD();
    if (!acquireSD(pdMS_TO_TICKS(1000)))
      break;
    selectSD();
    int idx;
    if (!findNextQueueFileLocked(syncState.currentFile, &idx))
    {
      deselectSD();
      releaseSD();
      finished = true;
      break;
    }
    syncState.currentFile = idx;
    getQueueFileName(idx, fn, sizeof(fn));
    int nRecs = countValidRecordsInFileLocked(fn);

    if (nRecs == 0)
    {
      if (idx != currentQueueFile)
      {
        if (removeQueueFileSafeLocked(fn, 0))
        {
          pendingCacheDirty = true;
          syncState.filesProcessed++;
        }
      }
      deselectSD();
      releaseSD();
      syncState.currentFile = idx + 1;
      continue;
    }
    deselectSD();
    releaseSD();
    char buf[40];
    snprintf(buf, sizeof(buf), "FILE %d (%d rec)", idx, nRecs);
    showOLED(F("SYNC"), buf);
    bool ok = syncQueueFileWithRetry(fn);
    syncState.filesProcessed++;
    if (ok)
      syncState.filesSucceeded++;
    else if (!isWifiConnected())
    {
      syncState.inProgress = false;
      break;
    }
    syncState.currentFile = idx + 1;
    taskYIELD();
    esp_task_wdt_reset();
  }
  if (finished)
  {
    syncState.inProgress = false;
    syncState.currentFile = 0;
    syncState.filesProcessed = 0;
    refreshPendingCache();
    if (syncState.filesSucceeded > 0)
    {
      char buf[20];
      if (cachedPendingRecords == 0)
      {
        showOLED(F("SYNC"), "SELESAI!");
        playToneSuccess();
      }
      else
      {
        snprintf(buf, sizeof(buf), "SISA %d", cachedPendingRecords);
        showOLED(F("SYNC PARSIAL"), buf);
      }
      delay(500);
    }
    syncState.filesSucceeded = 0;
  }
  restoreWdtNormal();
}
