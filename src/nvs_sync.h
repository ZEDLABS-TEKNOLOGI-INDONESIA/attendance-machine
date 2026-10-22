#pragma once
static bool nvsCompactAfterSync(uint64_t sentMask, int cntAtSend)
{
  if (!lockNvs(pdMS_TO_TICKS(3000)))
    return false;
  prefs.begin(NVS_NAMESPACE, false);
  int total = prefs.getInt(NVS_KEY_COUNT, 0);
  if (total > NVS_MAX_RECORDS)
    total = NVS_MAX_RECORDS;
  char srcKey[16], dstKey[16];
  int dst = 0, dropped = 0;
  bool ok = true;
  for (int src = 0; src < total; src++)
  {
    bool wasSent = (src < cntAtSend && src < 64 && ((sentMask >> src) & 1ULL));
    if (wasSent)
      continue;
    snprintf(srcKey, sizeof(srcKey), "%s%d", NVS_KEY_PREFIX, src);
    if (prefs.getBytesLength(srcKey) != sizeof(OfflineRecord))
    {
      dropped++;
      continue;
    }
    if (src != dst)
    {
      OfflineRecord rec;
      prefs.getBytes(srcKey, &rec, sizeof(OfflineRecord));
      snprintf(dstKey, sizeof(dstKey), "%s%d", NVS_KEY_PREFIX, dst);
      if (prefs.putBytes(dstKey, &rec, sizeof(OfflineRecord)) != sizeof(OfflineRecord))
      {
        ok = false;
        break;
      }
    }
    dst++;
  }
  if (ok && dst != total)
  {
    prefs.putInt(NVS_KEY_COUNT, dst);
    for (int k = dst; k < total; k++)
    {
      snprintf(dstKey, sizeof(dstKey), "%s%d", NVS_KEY_PREFIX, k);
      prefs.remove(dstKey);
    }
  }
  prefs.end();
  unlockNvs();
  LOG_I("[NVS] compact ok=%d kept=%d dropped=%d\n", (int)ok, dst, dropped);
  if (ok && dropped > 0)
    appendFailedLogToSD("NVS", "BUFFER", "RECORD_RUSAK_DIBUANG");
  return ok;
}

static bool parseSyncReply(const String &body, int expected, uint64_t *retryMask)
{
  *retryMask = 0;
  JsonDocument res;
  if (deserializeJson(res, body) != DeserializationError::Ok || !res["data"].is<JsonArray>())
    return false;
  JsonArray arr = res["data"].as<JsonArray>();
  if ((int)arr.size() != expected || expected > 64)
    return false;
  int i = 0;
  for (JsonObject item : arr)
  {
    const char *st = item["status"] | "error";
    if (strcmp(st, "error") == 0)
    {
      if (item["retryable"] | false)
        *retryMask |= (1ULL << i);
      else
        appendFailedLogToSD(item["rfid"] | "unknown", item["timestamp"] | "unknown", item["message"] | "UNKNOWN");
    }
    i++;
  }
  return true;
}

bool nvsSyncToServer()
{
  int cnt = nvsGetCount();
  if (cnt == 0)
    return true;
  if (cnt > NVS_MAX_RECORDS)
    cnt = NVS_MAX_RECORDS;
  if (isSignalCritical())
    return false;
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(30000);
  http.setConnectTimeout(10000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/sync-bulk"))
    return false;
  if (!http.begin(client, url))
    return false;
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  JsonDocument doc;
  JsonArray arr = doc["data"].to<JsonArray>();
  uint64_t sentMask = 0;
  int sentCnt = 0;
  int idxOf[NVS_MAX_RECORDS];
  for (int i = 0; i < cnt; i++)
  {
    OfflineRecord rec;
    if (!nvsLoadRecord(i, rec))
      continue;
    JsonObject o = arr.add<JsonObject>();
    o["rfid"] = rec.rfid;
    o["timestamp"] = rec.timestamp;
    o["device_id"] = rec.deviceId;
    o["sync_mode"] = true;
    idxOf[sentCnt] = i;
    sentMask |= (1ULL << i);
    sentCnt++;
  }
  if (sentCnt == 0)
  {
    http.end();
    return nvsCompactAfterSync(0, cnt);
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
    if (!parseSyncReply(body, sentCnt, &retryMask))
    {
      appendFailedLogToSD("BATCH", "NVS_BUFFER", "RESPONSE_TIDAK_VALID_ATAU_TERPOTONG");
      return false;
    }
    isOnline = true;
    for (int j = 0; j < sentCnt; j++)
      if ((retryMask >> j) & 1ULL)
        sentMask &= ~(1ULL << idxOf[j]);
    return nvsCompactAfterSync(sentMask, cnt);
  }
  http.end();
  return false;
}
