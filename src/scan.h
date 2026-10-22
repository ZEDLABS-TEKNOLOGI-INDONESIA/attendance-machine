#pragma once
void uidToString(uint8_t *uid, uint8_t len, char *out)
{
  if (len >= 4)
  {
    uint32_t v = ((uint32_t)uid[3] << 24) | ((uint32_t)uid[2] << 16) | ((uint32_t)uid[1] << 8) | uid[0];
    sprintf(out, "%010lu", v);
  }
  else
  {
    sprintf(out, "%02X%02X", uid[0], uid[1]);
  }
}

void urlEncode(const char *src, char *dst, size_t dstSize)
{
  size_t di = 0;
  for (size_t i = 0; src[i] != '\0' && di < dstSize - 4; i++)
  {
    char c = src[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~')
    {
      dst[di++] = c;
    }
    else
    {
      snprintf(&dst[di], 4, "%%%02X", (unsigned char)c);
      di += 3;
    }
  }
  dst[di] = '\0';
}

bool kirimLangsung(const char *rfid, const char *ts, char *msg)
{
  if (!isWifiConnected())
    return false;
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(4000);
  http.setConnectTimeout(2000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi"))
  {
    strcpy(msg, "SERVER ERR 0");
    return false;
  }
  if (!http.begin(client, url))
  {
    strcpy(msg, "SERVER ERR 0");
    return false;
  }
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"rfid\":\"%s\",\"timestamp\":\"%s\",\"device_id\":\"%s\",\"sync_mode\":false}",
           rfid, ts, deviceId);
  int code = http.POST(payload);
  String body = (code == 400) ? http.getString() : String();
  http.end();
  if (code == 200)
  {
    strcpy(msg, "PRESENSI OK");
    return true;
  }
  if (code == 400)
  {
    JsonDocument d;
    const char *c = "";
    if (deserializeJson(d, body) == DeserializationError::Ok)
      c = d["code"] | "";
    if (!strcmp(c, "TS_FUTURE") || !strcmp(c, "TS_TOO_OLD"))
    {
      strcpy(msg, "JAM DEVICE SALAH");
      timers.lastTimeSync = millis() - TIME_SYNC_INTERVAL;
      return false;
    }
    if (!strcmp(c, "TS_EXPIRED"))
    {
      strcpy(msg, "DATA KADALUARSA");
      return false;
    }
    strcpy(msg, "CUKUP SEKALI!");
    return false;
  }
  if (code == 403)
  {
    strcpy(msg, "HARI LIBUR!");
    return false;
  }
  if (code == 404)
  {
    strcpy(msg, "RFID NONAKTIF");
    return false;
  }
  snprintf(msg, 32, "SERVER ERR %d", code);
  return false;
}

bool isDuplicateScanRecent(const char *rfid, unsigned long t, const char *sourceTag)
{
  (void)sourceTag;
  bool dup = nvsIsRecentScan(rfid, t);
  return dup;
}

bool kirimPresensi(const char *rfid, char *msg)
{
  msg[0] = '\0';
  if (!isTimeValid())
  {
    strcpy(msg, "WAKTU INVALID");
    return false;
  }
  char ts[20];
  getFormattedTimestamp(ts, sizeof(ts));
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  if (!nowValid)
  {
    strcpy(msg, "WAKTU INVALID");
    return false;
  }
#if REJECT_SCAN_UNTRUSTED_TIME
  if (!isTimeTrusted())
  {
    strcpy(msg, "WAKTU BELUM SYNC");
    return false;
  }
#endif
  const char *pathTag = sdCardAvailable ? "SD" : (isWifiConnected() ? "DIRECT_HTTP" : "OFFLINE_BUFFER");
  if (isDuplicateScanRecent(rfid, (unsigned long)now, pathTag))
  {
    strcpy(msg, "CUKUP SEKALI!");
    return false;
  }
  if (sdCardAvailable)
  {
    RfidLookup lk = rfidLookupCache(rfid);
    if (lk == RFID_BUSY)
    {
      strcpy(msg, "CACHE SIBUK");
      return false;
    }
    if (lk == RFID_NOT_FOUND)
    {
      strcpy(msg, "RFID NONAKTIF");
      return false;
    }
    SaveResult r = saveToQueue(rfid, ts, (unsigned long)now);
    switch (r)
    {
    case SAVE_OK:
      nvsBumpScanCount();
      nvsSaveLastScan(rfid, (unsigned long)now);
      strcpy(msg, cachedQueueFileCount >= QUEUE_WARN_THRESHOLD ? "QUEUE HAMPIR PENUH!" : "DATA TERSIMPAN");
      return true;
    case SAVE_DUPLICATE:
      strcpy(msg, "CUKUP SEKALI!");
      return false;
    case SAVE_QUEUE_FULL:
      strcpy(msg, "QUEUE PENUH!");
      return false;
    default:
      timers.lastSDRedetect = millis() - SD_REDETECT_INTERVAL;
      break;
    }
  }
  if (isWifiConnected())
  {
    if (kirimLangsung(rfid, ts, msg))
    {
      nvsBumpScanCount();
      nvsSaveLastScan(rfid, (unsigned long)now);
      return true;
    }
    if (strcmp(msg, "CUKUP SEKALI!") == 0 || strcmp(msg, "RFID NONAKTIF") == 0 ||
        strcmp(msg, "HARI LIBUR!") == 0 || strcmp(msg, "JAM DEVICE SALAH") == 0 ||
        strcmp(msg, "DATA KADALUARSA") == 0)
    {
      return false;
    }
  }
  if (nvsIsDuplicate(rfid, (unsigned long)now))
  {
    strcpy(msg, "CUKUP SEKALI!");
    return false;
  }
  if (nvsSaveToBuffer(rfid, ts, (unsigned long)now))
  {
    nvsBumpScanCount();
    nvsSaveLastScan(rfid, (unsigned long)now);
    snprintf(msg, 32, "BUFFER %d/%d", nvsGetCount(), NVS_MAX_RECORDS);
    return true;
  }
  strcpy(msg, "BUFFER PENUH!");
  return false;
}
