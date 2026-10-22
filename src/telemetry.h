#pragma once
void sendTelemetry()
{
  if (isSignalWeak())
    return;
  if (millis() - timers.lastTelemetry < backoffInterval(TELEMETRY_INTERVAL, failTelemetry))
    return;
  timers.lastTelemetry = millis();
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/heartbeat"))
    return;
  if (!http.begin(client, url))
    return;
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  JsonDocument doc;
  doc["device_id"] = deviceId;
  doc["device_name"] = deviceName;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["uptime_sec"] = millis() / 1000;
  doc["heap_free"] = esp_get_free_heap_size();
  doc["pending_records"] = cachedPendingRecords + nvsGetCount();
  doc["scan_today"] = nvsGetScanCount();
  doc["rssi"] = isWifiConnected() ? (int)WiFi.RSSI() : 0;
  doc["sd_ok"] = sdCardAvailable;
  doc["rfid_db_entries"] = rfidCacheCount;
  doc["rfid_db_discarded"] = rfidCacheDiscarded;
  doc["online"] = isOnline;
  doc["reset_reason"] = resetReasonName((esp_reset_reason_t)diagResetReason);
  doc["last_abnormal"] = resetReasonName((esp_reset_reason_t)diagLastAbnormal);
  doc["boot_count"] = diagBootCount;
  doc["abnormal_count"] = diagAbnormalCount;
  doc["heap_min"] = (uint32_t)esp_get_minimum_free_heap_size();
  doc["heap_max_block"] = (uint32_t)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  doc["stk_loop"] = taskStackHwm(hTaskLoop);
  doc["stk_rfid"] = taskStackHwm(hTaskRfid);
  doc["stk_sync"] = taskStackHwm(hTaskSync);
  doc["stk_disp"] = taskStackHwm(hTaskDisplay);
  doc["rc522_ok"] = rfidReady;
  LOG_I("[DIAG] heap=%u min=%u blk=%u stk L/R/S/D=%u/%u/%u/%u\n",
        (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT),
        (unsigned)taskStackHwm(hTaskLoop), (unsigned)taskStackHwm(hTaskRfid),
        (unsigned)taskStackHwm(hTaskSync), (unsigned)taskStackHwm(hTaskDisplay));
  String payload;
  payload.reserve(measureJson(doc) + 1);
  serializeJson(doc, payload);
  int code = http.POST(payload);
  http.end();
  if (code >= 200 && code < 300)
  {
    failTelemetry = 0;
    isOnline = true;
  }
  else
  {
    backoffFail(failTelemetry);
    isOnline = false;
  }
}
