#pragma once
bool reinitSDCard()
{
  if (file.isOpen())
    file.close();
  sd.end();
  delay(100);
  selectSD();
  delay(10);
  bool ok = sd.begin(PIN_SD_CS, SD_SCK_MHZ(4));
  deselectSD();
  return ok;
}

void checkSDHealth()
{
  static uint8_t failCount = 0;
  static uint8_t probeBuf[512];
  if (millis() - timers.lastSDRedetect < SD_REDETECT_INTERVAL)
    return;
  timers.lastSDRedetect = millis();
  if (!sdCardAvailable)
  {
    if (!acquireSD(pdMS_TO_TICKS(1000)))
      return;
    bool ok = reinitSDCard();
    releaseSD();
    if (ok)
    {
      failCount = 0;
      sdCardAvailable = true;
      pendingCacheDirty = true;
      showOLED(F("SD CARD"), "TERBACA KEMBALI");
      playToneSuccess();
      delay(800);
      loadRfidCacheFromFile();
      loadAdminRfidList();
    }
    return;
  }
  if (!acquireSD(pdMS_TO_TICKS(500)))
    return;
  selectSD();

  bool healthy = sd.card() && sd.card()->readSector(0, probeBuf);
  deselectSD();
  releaseSD();
  if (healthy)
  {
    failCount = 0;
    return;
  }
  failCount++;
  if (failCount < 3)
  {

    timers.lastSDRedetect = millis() - SD_REDETECT_INTERVAL + 5000UL;
    return;
  }
  failCount = 0;
  sdCardAvailable = false;
  clearRfidCache();
  showOLED(F("SD CARD"), "TERLEPAS!");
  playToneError();
  delay(800);
}

bool flushAllFiles()
{
  if (!sdCardAvailable)
    return true;
  if (!acquireSD(pdMS_TO_TICKS(3000)))
    return false;
  if (file.isOpen())
  {
    file.sync();
    file.close();
  }
  releaseSD();
  return true;
}
