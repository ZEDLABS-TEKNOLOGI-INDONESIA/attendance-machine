#pragma once
void taskRfid(void *param)
{
  (void)param;
  esp_task_wdt_add(nullptr);
  for (;;)
  {
    esp_task_wdt_reset();
    if (sleepRequested)
    {
      parkRfid = true;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    parkRfid = false;
    RfidScanEvent ev;
    if (xQueueReceive(xRfidQueue, &ev, pdMS_TO_TICKS(10)) == pdTRUE)
    {
      char rfidBuf[11];
      uidToString(ev.uid, ev.uidLen, rfidBuf);
      if (strcmp(rfidBuf, lastUID) == 0 && millis() - timers.lastScan < DEBOUNCE_TIME)
        continue;
      strcpy(lastUID, rfidBuf);
      timers.lastScan = millis();
      bool wasOff = !oledIsOn;
      if (wasOff)
        turnOnOLED();
      if (isAdminRfid(rfidBuf))
      {
        handleAdminScan(rfidBuf);
      }
      else
      {
        showOLED(F("RFID"), rfidBuf);
        playToneNotify();
        char msg[32];
        bool ok = kirimPresensi(rfidBuf, msg);
        showOLED(ok ? F("BERHASIL") : F("INFO"), msg);
        ok ? playToneSuccess() : playToneError();
      }
      rfidFeedback.shownAt = millis();
      rfidFeedback.wasOledOff = wasOff;
      __asm__ volatile("" ::: "memory");
      rfidFeedback.active = true;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void taskSync(void *param)
{
  (void)param;
  esp_task_wdt_add(nullptr);
  for (;;)
  {
    esp_task_wdt_reset();
    if (sleepRequested)
    {
      parkSync = true;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    parkSync = false;
    RuntimeConfig cfg = getRuntimeConfigSnapshot();
    processReconnect();
    checkSDHealth();

    unsigned long now = millis();
    if (isWifiConnected())
    {
      bool force = forceSyncRequested;
      if (force)
        forceSyncRequested = false;
      if (!isSignalWeak())
      {
        checkOtaUpdate();
        vTaskDelay(pdMS_TO_TICKS(200));
        checkAndUpdateRfidDb();
        vTaskDelay(pdMS_TO_TICKS(200));
        if (otaState.updateAvailable && !rfidFeedback.active)
          performOtaUpdate();
        sendTelemetry();
        vTaskDelay(pdMS_TO_TICKS(200));
        fetchRemoteConfig();
      }
      if (nvsGetCount() > 0 && (force || now - timers.lastNvsSync >= cfg.syncIntervalMs))
      {
        timers.lastNvsSync = now;
        nvsSyncToServer();
      }
      if (sdCardAvailable)
      {
        if (syncState.inProgress)
          chunkedSync();
        else if (force || now - timers.lastSync >= cfg.syncIntervalMs)
        {
          refreshPendingCache();
          timers.lastSync = now;
          if (cachedPendingRecords > 0)
            chunkedSync();
        }
      }
    }
    anchorTimeFromSystem();
    periodicTimeSync();
    vTaskDelay(pdMS_TO_TICKS(PERIODIC_CHECK_INTERVAL));
  }
}

void taskDisplay(void *param)
{
  (void)param;
  esp_task_wdt_add(nullptr);
  for (;;)
  {
    esp_task_wdt_reset();
    if (sleepRequested)
    {
      parkDisp = true;
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }
    parkDisp = false;
    unsigned long now = millis();

    if (rfidFeedback.active && (long)(now - rfidFeedback.shownAt) >= (long)RFID_FEEDBACK_DISPLAY_MS)
    {
      rfidFeedback.active = false;
      if (rfidFeedback.wasOledOff)
        checkOLEDSchedule();
      memset(previousDisplay.time, 0xFF, sizeof(previousDisplay.time));
      previousDisplay.pendingRecords = -1;
      previousDisplay.isOnline = !currentDisplay.isOnline;
    }
    checkOLEDSchedule();
    if (now - timers.lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)
    {
      timers.lastDisplayUpdate = now;
      updateCurrentDisplayState();
      updateStandbyDisplay();
    }
    checkFactoryReset();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
