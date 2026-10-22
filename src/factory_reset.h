#pragma once
static int wipeQueueFiles()
{
  if (!sdCardAvailable)
    return 0;
  int removed = 0;
  int from = 0;
  for (int guard = 0; guard < MAX_QUEUE_FILES; guard++)
  {
    esp_task_wdt_reset();
    if (!acquireSD(pdMS_TO_TICKS(3000)))
      break;
    selectSD();
    int idx = 0;
    bool found = findNextQueueFileLocked(from, &idx);
    if (found)
    {
      char fn[24];
      getQueueFileName(idx, fn, sizeof(fn));
      if (sd.remove(fn))
        removed++;
    }
    deselectSD();
    releaseSD();
    if (!found)
      break;
    from = idx + 1;
  }
  return removed;
}

void checkFactoryReset()
{
  if (digitalRead(PIN_BOOT) != LOW)
    return;
  esp_task_wdt_reset();
  vTaskDelay(pdMS_TO_TICKS(100));
  if (digitalRead(PIN_BOOT) != LOW)
    return;
  esp_task_wdt_reset();
  vTaskDelay(pdMS_TO_TICKS(100));
  if (digitalRead(PIN_BOOT) != LOW)
    return;
  esp_task_wdt_reset();
  unsigned long held = millis();
  bool armed = false;
  bool wipeQueue = false;
  showOLED(F("TAHAN UNTUK"), "FACTORY RESET");
  while (digitalRead(PIN_BOOT) == LOW)
  {
    esp_task_wdt_reset();
    unsigned long elapsed = millis() - held;
    if (!wipeQueue && elapsed >= FACTORY_RESET_WIPE_QUEUE_MS)
    {
      wipeQueue = true;
      showOLED(F("HAPUS ANTREAN"), "LEPAS UNTUK LANJUT");
      playToneNotify();
    }
    else if (!armed && elapsed >= FACTORY_RESET_HOLD_MS)
    {
      armed = true;
      showOLED(F("LEPAS = RESET"), "TAHAN = +ANTREAN");
      playToneNotify();
    }
    delay(100);
  }

  if (!armed)
  {

    memset(previousDisplay.time, 0xFF, sizeof(previousDisplay.time));
    return;
  }

  showOLED(F("FACTORY RESET"), "MENGHAPUS...");
  playToneError();
  delay(500);

  WiFi.mode(WIFI_STA);
  esp_wifi_restore();

  if (lockNvs(pdMS_TO_TICKS(3000)))
  {
    prefs.begin(NVS_NS_CONFIG, false);
    prefs.clear();
    prefs.end();
    prefs.begin(NVS_NAMESPACE, false);
    prefs.clear();
    prefs.end();
    unlockNvs();
  }

  if (sdCardAvailable)
  {
    if (acquireSD(pdMS_TO_TICKS(3000)))
    {
      selectSD();
      sd.remove(RFID_DB_FILE);
      sd.remove(RFID_DB_BAK);
      sd.remove(METADATA_FILE);
      sd.remove("/failed_log.csv");
      sd.remove("/failed_log.old");
      deselectSD();
      releaseSD();
    }
    if (wipeQueue)
    {
      showOLED(F("HAPUS ANTREAN"), "MOHON TUNGGU...");
      int n = wipeQueueFiles();
      char buf[24];
      snprintf(buf, sizeof(buf), "%d FILE", n);
      showOLED(F("ANTREAN TERHAPUS"), buf);
      delay(1000);
    }
  }

  rtcQueueFileValid = false;
  currentQueueFile = 0;

  showOLED(F("RESET SELESAI"), "RESTART...");
  delay(2000);
  ESP.restart();
}
