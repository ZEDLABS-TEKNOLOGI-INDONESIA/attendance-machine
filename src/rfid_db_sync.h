#pragma once
unsigned long checkRfidDbVersion()
{
  if (isSignalWeak())
    return 0;
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/rfid-list/version"))
  {
    return 0;
  }
  if (!http.begin(client, url))
    return 0;
  http.addHeader(F("X-API-KEY"), apiKey);
  int code = http.GET();
  if (code != 200)
  {
    http.end();
    return 0;
  }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return 0;
  return doc["ver"] | 0UL;
}

static bool dbWriteChunk(FsFile &dbf, const char *data, size_t len)
{
  if (len == 0)
    return true;
  if (!acquireSD(pdMS_TO_TICKS(3000)))
    return false;
  selectSD();
  size_t w = dbf.write(data, len);
  deselectSD();
  releaseSD();
  return w == len;
}

bool downloadRfidDb()
{
  if (isSignalWeak() || !sdCardAvailable)
    return false;

  showOLED(F("RFID DB"), "MENGUNDUH...");
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(30000);
  http.setConnectTimeout(10000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/rfid-list"))
    return false;
  if (!http.begin(client, url))
    return false;
  http.useHTTP10(true);
  http.addHeader(F("X-API-KEY"), apiKey);

  int code = http.GET();
  if (code != 200)
  {
    http.end();
    showOLED(F("RFID DB"), "GAGAL UNDUH");
    playToneError();
    delay(800);
    return false;
  }

  const char *tmpPath = "/rfid_db.tmp";
  FsFile dbf;
  if (!acquireSD(pdMS_TO_TICKS(3000)))
  {
    http.end();
    return false;
  }
  selectSD();
  if (sd.exists(tmpPath))
    sd.remove(tmpPath);
  bool opened = dbf.open(tmpPath, O_WRONLY | O_CREAT | O_TRUNC);
  deselectSD();
  releaseSD();
  if (!opened)
  {
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  int total = http.getSize();
  long bytesRead = 0;
  unsigned long lastDataAt = millis();
  unsigned long serverVer = 0;
  bool firstLine = true;
  bool receivedEndMarker = false;
  bool writeOk = true;
  int written = 0;

  char lineBuf[32];
  int lbPos = 0;
  bool lineTruncated = false;
  char outBuf[512];
  size_t outLen = 0;
  uint8_t chunk[256];

  while (http.connected() && (total <= 0 || bytesRead < total) && !receivedEndMarker && writeOk)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int avail = stream->available();
    if (!avail)
    {
      if (millis() - lastDataAt > 120000UL)
        break;
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    lastDataAt = millis();
    int rd = stream->readBytes(chunk, min(avail, (int)sizeof(chunk)));
    bytesRead += rd;

    for (int i = 0; i < rd && !receivedEndMarker; i++)
    {
      char c = (char)chunk[i];
      if (c == '\r')
        continue;
      if (c != '\n')
      {
        if (lbPos < (int)sizeof(lineBuf) - 1)
          lineBuf[lbPos++] = c;
        else
          lineTruncated = true;
        continue;
      }

      lineBuf[lbPos] = '\0';
      lbPos = 0;
      bool truncated = lineTruncated;
      lineTruncated = false;

      if (firstLine)
      {
        firstLine = false;
        if (strncmp(lineBuf, "ver:", 4) == 0)
        {
          serverVer = strtoul(lineBuf + 4, nullptr, 10);
          continue;
        }
      }
      if (strcmp(lineBuf, "END") == 0)
      {
        receivedEndMarker = true;
        break;
      }
      if (truncated || strlen(lineBuf) != 10)
        continue;

      bool ok = true;
      for (int j = 0; j < 10 && ok; j++)
        ok = isdigit((unsigned char)lineBuf[j]);
      if (!ok)
        continue;

      if (outLen + 11 > sizeof(outBuf))
      {
        if (!dbWriteChunk(dbf, outBuf, outLen))
        {
          writeOk = false;
          break;
        }
        outLen = 0;
      }
      memcpy(outBuf + outLen, lineBuf, 10);
      outBuf[outLen + 10] = '\n';
      outLen += 11;
      written++;
    }
  }
  http.end();

  if (writeOk && outLen > 0)
    writeOk = dbWriteChunk(dbf, outBuf, outLen);

  if (acquireSD(pdMS_TO_TICKS(5000)))
  {
    selectSD();
    dbf.sync();
    dbf.close();
    deselectSD();
    releaseSD();
  }
  else
  {
    writeOk = false;
  }

  bool failed = !writeOk || !receivedEndMarker;
  if (failed)
  {
    if (acquireSD(pdMS_TO_TICKS(3000)))
    {
      selectSD();
      sd.remove(tmpPath);
      deselectSD();
      releaseSD();
    }
    showOLED(F("RFID DB"), "UNDUH TERPUTUS");
    playToneError();
    delay(800);
    appendFailedLogToSD("RFID_DB", "download", "TERPUTUS_ATAU_KOSONG");
    return false;
  }

  if (!acquireSD(pdMS_TO_TICKS(5000)))
    return false;
  selectSD();
  if (sd.exists(RFID_DB_BAK))
    sd.remove(RFID_DB_BAK);
  bool hadOld = sd.exists(RFID_DB_FILE);
  bool swapped = true;
  if (hadOld && !sd.rename(RFID_DB_FILE, RFID_DB_BAK))
    swapped = false;
  else if (!sd.rename(tmpPath, RFID_DB_FILE))
  {
    swapped = false;
    if (hadOld)
      sd.rename(RFID_DB_BAK, RFID_DB_FILE);
  }
  if (swapped)
  {
    if (hadOld)
      sd.remove(RFID_DB_BAK);
    loadRfidCacheFromFileLocked();
  }
  deselectSD();
  releaseSD();
  if (!swapped)
  {
    showOLED(F("RFID DB"), "GAGAL GANTI FILE");
    playToneError();
    delay(800);
    return false;
  }

  if (serverVer == 0 && written > 0)
  {
    unsigned long fallbackVer = checkRfidDbVersion();
    if (fallbackVer > 0)
      serverVer = fallbackVer;
  }
  if (serverVer > 0)
    nvsSetRfidDbVer(serverVer);

  char buf[20];
  snprintf(buf, sizeof(buf), "%d RFID", written);
  showOLED(F("RFID DB"), buf);
  playToneSuccess();
  delay(800);
  return true;
}

void checkAndUpdateRfidDb()
{
  if (!sdCardAvailable || isSignalWeak())
    return;
  if (millis() - timers.lastRfidDbCheck < backoffInterval(RFID_DB_CHECK_INTERVAL, failDb))
    return;
  timers.lastRfidDbCheck = millis();
  unsigned long local = nvsGetRfidDbVer(), server = checkRfidDbVersion();
  if (server == 0)
  {
    backoffFail(failDb);
    return;
  }
  failDb = 0;
  if (server <= local)
  {
    return;
  }
  if (!downloadRfidDb())
    backoffFail(failDb);
}
