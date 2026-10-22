#pragma once
static bool extractUrlHost(const char *url, char *out, size_t outSz)
{
  const char *p = strstr(url, "://");
  if (!p)
    return false;
  p += 3;
  size_t i = 0;
  while (*p && *p != '/' && *p != ':' && *p != '?' && *p != '#')
  {
    if (i + 1 >= outSz)
      return false;
    out[i++] = (char)tolower((unsigned char)*p++);
  }
  out[i] = '\0';
  return i > 0;
}

static bool isOtaUrlAllowed(const char *otaUrl)
{
  if (strncmp(otaUrl, "https://", 8) != 0)
    return false;
  char h1[64], h2[64];
  if (!extractUrlHost(otaUrl, h1, sizeof(h1)) || !extractUrlHost(apiBaseUrl, h2, sizeof(h2)))
    return false;
  return strcmp(h1, h2) == 0;
}

void checkOtaUpdate()
{
  if (isSignalWeak())
    return;
  RuntimeConfig cfg = getRuntimeConfigSnapshot();
  if (millis() - timers.lastOtaCheck < backoffInterval(cfg.otaCheckIntervalMs, failOta))
    return;
  timers.lastOtaCheck = millis();
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/firmware/check"))
  {
    return;
  }
  if (!http.begin(client, url))
  {
    return;
  }
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  char payload[80];
  snprintf(payload, sizeof(payload), "{\"version\":\"%s\",\"device_id\":\"%s\"}", FIRMWARE_VERSION, deviceId);
  int code = http.POST(payload);
  if (code != 200)
  {
    LOG_I("[OTA] check HTTP %d\n", code);
    http.end();
    backoffFail(failOta);
    return;
  }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
  {
    backoffFail(failOta);
    return;
  }
  failOta = 0;
  bool hasUpdate = doc["update"] | false;
  const char *ver = doc["version"] | "";
  const char *burl = doc["url"] | "";
  const char *md5 = doc["md5"] | "";
  LOG_I("[OTA] update=%d ver=%s md5len=%u url=%s\n", (int)hasUpdate, ver, (unsigned)strlen(md5), burl);
  if (!hasUpdate || !strlen(ver) || !strlen(burl))
    return;
  if (!isValidMd5Hex(md5))
  {
    LOG_W("[OTA] ditolak: md5 tidak valid\n");
    return;
  }
  if (strlen(burl) >= sizeof(otaState.url) || !isOtaUrlAllowed(burl))
  {
    LOG_W("[OTA] ditolak: URL terlalu panjang atau host beda dari API\n");
    return;
  }
  if (otaRejectedVer[0] != '\0' && strcmp(ver, otaRejectedVer) == 0)
  {
    LOG_W("[OTA] dilewati: versi ini pernah gagal verifikasi\n");
    return;
  }
  if (compareFirmwareVersion(ver, FIRMWARE_VERSION) <= 0)
  {
    LOG_W("[OTA] ditolak: versi tidak lebih baru\n");
    return;
  }
  strncpy(otaState.version, ver, sizeof(otaState.version) - 1);
  otaState.version[sizeof(otaState.version) - 1] = '\0';
  strncpy(otaState.url, burl, sizeof(otaState.url) - 1);
  otaState.url[sizeof(otaState.url) - 1] = '\0';
  strncpy(otaState.md5, md5, sizeof(otaState.md5) - 1);
  otaState.md5[sizeof(otaState.md5) - 1] = '\0';
  otaState.updateAvailable = true;
  char buf[32];
  snprintf(buf, sizeof(buf), "v%s TERSEDIA", otaState.version);
  showOLED(F("UPDATE"), buf);
  playToneNotify();
  delay(2000);
}

static void otaAbort(const char *detail, bool startedUpdate)
{
  if (startedUpdate)
    Update.abort();
  showOLED(F("UPDATE GAGAL"), detail);
  playToneError();
  otaState.updateAvailable = false;
  otaInProgress = false;
  restoreWdtNormal();
}

void performOtaUpdate()
{
  if (!otaState.updateAvailable || isSignalWeak())
    return;
  if (!isValidMd5Hex(otaState.md5))
  {
    otaAbort("MD5 TIDAK ADA", false);
    return;
  }
  char buf[20];
  snprintf(buf, sizeof(buf), "v%s", otaState.version);
  showOLED(F("UPDATE OTA"), buf);
  delay(500);
  showOLED(F("MENGUNDUH"), "MOHON TUNGGU...");
  extendWdtForSync();
  otaInProgress = true;

  WiFiClientSecure otaClient;
  applyTls(otaClient);
  HTTPClient http;
  http.setTimeout(60000);
  http.useHTTP10(true);
  if (!http.begin(otaClient, otaState.url))
  {
    otaAbort("URL ERR", false);
    return;
  }
  http.addHeader(F("X-API-KEY"), apiKey);
  int code = http.GET();
  if (code != 200)
  {
    snprintf(buf, sizeof(buf), "HTTP ERR %d", code);
    http.end();
    otaAbort(buf, false);
    return;
  }
  int total = http.getSize();
  size_t updateSize = (total > 0) ? (size_t)total : UPDATE_SIZE_UNKNOWN;
  if (!Update.begin(updateSize))
  {
    http.end();
    otaAbort("NO SPACE", false);
    return;
  }
  Update.setMD5(otaState.md5);

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buff[1024];
  size_t written = 0;
  unsigned long lastDataAt = millis();
  bool ioError = false;
  OtaVerScan verScan;
  while (http.connected() && (total <= 0 || written < (size_t)total))
  {
    esp_task_wdt_reset();
    int avail = stream->available();
    if (avail > 0)
    {
      int rd = stream->readBytes(buff, min((int)sizeof(buff), avail));
      if (rd <= 0 || Update.write(buff, rd) != (size_t)rd)
      {
        ioError = true;
        break;
      }
      otaVerFeed(verScan, buff, (size_t)rd);
      written += (size_t)rd;
      lastDataAt = millis();
    }
    else
    {
      if (millis() - lastDataAt > 30000UL)
      {
        ioError = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  http.end();

  if (ioError || (total > 0 && written != (size_t)total))
  {
    otaAbort("UNDUH TERPUTUS", true);
    return;
  }
  if (!verScan.found)
  {
    strncpy(otaRejectedVer, otaState.version, sizeof(otaRejectedVer) - 1);
    otaAbort("VERSI TAK ADA", true);
    return;
  }
  if (strcmp(verScan.ver, otaState.version) != 0)
  {
    LOG_W("[OTA] versi image %s != server %s\n", verScan.ver, otaState.version);
    strncpy(otaRejectedVer, otaState.version, sizeof(otaRejectedVer) - 1);
    otaAbort("VERSI BEDA", true);
    return;
  }
  if (!Update.end() || !Update.isFinished())
  {
    if (Update.getError() == UPDATE_ERROR_MD5)
      otaAbort("MD5 SALAH", false);
    else
    {
      snprintf(buf, sizeof(buf), "ERR %d", Update.getError());
      otaAbort(buf, false);
    }
    return;
  }
  otaTrackAttempt(otaState.version);
  showOLED(F("UPDATE OK"), "RESTART...");
  playToneSuccess();
  delay(2000);
  restoreWdtNormal();
  ESP.restart();
}
