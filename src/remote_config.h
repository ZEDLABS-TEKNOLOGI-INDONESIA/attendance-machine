#pragma once
static const RuntimeConfig kRuntimeDefaults = {
    SLEEP_START_HOUR_DEFAULT, SLEEP_END_HOUR_DEFAULT,
    OLED_DIM_START_HOUR_DEFAULT, OLED_DIM_END_HOUR_DEFAULT,
    SYNC_INTERVAL, OTA_CHECK_INTERVAL};

static int jsonHourOrDefault(JsonVariantConst v, int def)
{
  if (v.isNull() || !v.is<int>())
    return def;
  int h = v.as<int>();
  return (h >= 0 && h <= 23) ? h : def;
}

static unsigned long jsonIntervalOrDefault(JsonVariantConst v, unsigned long def, unsigned long minMs)
{
  if (v.isNull() || !v.is<unsigned long>())
    return def;
  unsigned long x = v.as<unsigned long>();
  return (x >= minMs && x <= CFG_INTERVAL_MAX_MS) ? x : def;
}

static bool sameRuntimeConfig(const RuntimeConfig &a, const RuntimeConfig &b)
{
  return a.sleepStartHour == b.sleepStartHour &&
         a.sleepEndHour == b.sleepEndHour &&
         a.dimStartHour == b.dimStartHour &&
         a.dimEndHour == b.dimEndHour &&
         a.syncIntervalMs == b.syncIntervalMs &&
         a.otaCheckIntervalMs == b.otaCheckIntervalMs;
}

void fetchRemoteConfig()
{
  if (isSignalWeak())
    return;
  if (millis() - timers.lastRemoteConfig < backoffInterval(REMOTE_CONFIG_INTERVAL, failConfig))
    return;
  timers.lastRemoteConfig = millis();

  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  char encodedDeviceId[64];
  urlEncode(deviceId, encodedDeviceId, sizeof(encodedDeviceId));
  char url[200];
  snprintf(url, sizeof(url), "%s/api/presensi/config?device_id=%s", apiBaseUrl, encodedDeviceId);
  if (!http.begin(client, url))
    return;
  http.addHeader(F("X-API-KEY"), apiKey);
  int code = http.GET();
  if (code != 200)
  {
    http.end();
    backoffFail(failConfig);
    return;
  }
  String body = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
  {
    backoffFail(failConfig);
    return;
  }
  if (!doc.is<JsonObject>())
  {
    backoffFail(failConfig);
    return;
  }
  failConfig = 0;

  RuntimeConfig next;
  next.sleepStartHour = jsonHourOrDefault(doc["sleep_start"], kRuntimeDefaults.sleepStartHour);
  next.sleepEndHour = jsonHourOrDefault(doc["sleep_end"], kRuntimeDefaults.sleepEndHour);
  next.dimStartHour = jsonHourOrDefault(doc["oled_dim_start"], kRuntimeDefaults.dimStartHour);
  next.dimEndHour = jsonHourOrDefault(doc["oled_dim_end"], kRuntimeDefaults.dimEndHour);
  next.syncIntervalMs = jsonIntervalOrDefault(doc["sync_interval_ms"], kRuntimeDefaults.syncIntervalMs, CFG_INTERVAL_MIN_MS);
  next.otaCheckIntervalMs = jsonIntervalOrDefault(doc["ota_check_interval_ms"], kRuntimeDefaults.otaCheckIntervalMs, CFG_OTA_INTERVAL_MIN_MS);
  if (next.sleepStartHour == next.sleepEndHour)
    LOG_W("[CFG] jendela sleep kosong (mulai == selesai): sleep nonaktif\n");
  if (next.dimStartHour == next.dimEndHour)
    LOG_W("[CFG] jendela dim kosong (mulai == selesai): dim nonaktif\n");
  if (!xConfigMutex || xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(500)) != pdTRUE)
    return;
  bool changed = !sameRuntimeConfig(rtCfg, next);
  rtCfg = next;
  xSemaphoreGive(xConfigMutex);

  if (changed)
    persistRuntimeConfigToNvs(next);
}
