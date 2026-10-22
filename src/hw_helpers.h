#pragma once
inline bool acquireSD(TickType_t timeout = pdMS_TO_TICKS(SD_MUTEX_TIMEOUT_MS))
{
  return xSemaphoreTake(xSdMutex, timeout) == pdTRUE;
}

inline void releaseSD()
{
  xSemaphoreGive(xSdMutex);
}

inline void selectSD()
{
  digitalWrite(PIN_RFID_SS, HIGH);
  digitalWrite(PIN_SD_CS, LOW);
}

inline void deselectSD()
{
  digitalWrite(PIN_SD_CS, HIGH);
}

bool isWifiConnected()
{
  return WiFi.status() == WL_CONNECTED;
}

bool isSignalWeak()
{
  return !isWifiConnected() || WiFi.RSSI() < SIGNAL_THRESHOLD_WEAK;
}

bool isSignalCritical()
{
  return !isWifiConnected() || WiFi.RSSI() < SIGNAL_THRESHOLD_CRITICAL;
}
