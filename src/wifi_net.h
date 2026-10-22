#pragma once
static void onWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
{
  if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
    LOG_I("[WIFI] disconnect reason=%d\n", (int)info.wifi_sta_disconnected.reason);
}

bool connectToWifi(int ssidIdx)
{
  if (strlen(wifiCreds[ssidIdx].ssid) == 0)
  {
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.enableIPv6(false);
  WiFi.disconnect(false);
  delay(100);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(WIFI_PS_MIN_MODEM);
  WiFi.begin(wifiCreds[ssidIdx].ssid, wifiCreds[ssidIdx].pass);
  WiFi.setTxPower(WIFI_POWER_15dBm);
  for (int i = 0; i < 20 && !isWifiConnected(); i++)
  {
    esp_task_wdt_reset();
    vTaskDelay(pdMS_TO_TICKS(300));
    if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor((SCREEN_WIDTH - (int)strlen(wifiCreds[ssidIdx].ssid) * 6) / 2, 10);
      display.println(wifiCreds[ssidIdx].ssid);
      display.setCursor(35, 30);
      display.print(F("CONNECTING"));
      for (int j = 0; j < (i % 4); j++)
        display.print('.');
      display.display();
      xSemaphoreGive(xDisplayMutex);
    }
  }
  if (isWifiConnected())
  {
    char buf[20];
    snprintf(buf, sizeof(buf), "RSSI: %d dBm", (int)WiFi.RSSI());
    showOLED(F("WIFI OK"), buf);
    currentSsidIdx = ssidIdx;
    delay(1500);
    return true;
  }
  isOnline = false;
  return false;
}

bool connectToWiFi()
{
  for (int i = 0; i < 3; i++)
    if (connectToWifi(i))
      return true;
  return false;
}

bool pingAPI()
{
  if (isSignalCritical())
    return false;
  vTaskDelay(pdMS_TO_TICKS(1000));
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(15000);
  http.setConnectTimeout(10000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/ping"))
    return false;
  if (!http.begin(client, url))
    return false;
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  esp_task_wdt_reset();
  int code = http.GET();
  esp_task_wdt_reset();
  http.end();
  isOnline = (code == 200);
  return isOnline;
}

void processReconnect()
{
  switch (reconnectState)
  {
  case RECONNECT_IDLE:
    if (!isWifiConnected())
    {
      isOnline = false;
      unsigned long elapsed = millis() - timers.lastReconnect;
      if (elapsed >= RECONNECT_INTERVAL)
      {
        timers.lastReconnect = millis();
        reconnectState = RECONNECT_INIT;
      }
    }
    break;
  case RECONNECT_INIT:
    WiFi.disconnect(false);
    delay(100);
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
    {
      int nextIdx = (currentSsidIdx + 1) % 3;
      for (int i = 0; i < 3; i++)
      {
        int idx = (nextIdx + i) % 3;
        if (strlen(wifiCreds[idx].ssid) > 0)
        {
          WiFi.enableIPv6(false);
          WiFi.begin(wifiCreds[idx].ssid, wifiCreds[idx].pass);
          WiFi.setTxPower(WIFI_POWER_15dBm);
          currentSsidIdx = idx;
          break;
        }
      }
    }
    reconnectStartTime = millis();
    reconnectState = RECONNECT_TRYING;
    break;
  case RECONNECT_TRYING:
    if (isWifiConnected())
      reconnectState = RECONNECT_SUCCESS;
    else if (millis() - reconnectStartTime >= RECONNECT_TIMEOUT)
      reconnectState = RECONNECT_FAILED;
    break;
  case RECONNECT_SUCCESS:
    resetPollBackoff();
    timers.lastTelemetry = 0;
    if (!isSignalCritical())
    {
      syncTimeWithFallback();
      if (nvsGetCount() > 0)
        nvsSyncToServer();
      if (sdCardAvailable && isWifiConnected())
      {
        refreshPendingCache();
        if (cachedPendingRecords > 0)
        {
          char buf[20];
          snprintf(buf, sizeof(buf), "%d RECORDS", cachedPendingRecords);
          showOLED(F("SYNCING"), buf);
          syncState.inProgress = false;
          syncState.currentFile = 0;
          timers.lastSync = millis();
          chunkedSync();
        }
      }
    }
    reconnectState = RECONNECT_IDLE;
    break;
  case RECONNECT_FAILED:
    isOnline = false;
    reconnectState = RECONNECT_IDLE;
    break;
  }
}
