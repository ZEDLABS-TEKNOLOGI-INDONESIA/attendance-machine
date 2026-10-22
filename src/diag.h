#pragma once
static int diagBootCount = 0;

static int diagAbnormalCount = 0;

static int diagResetReason = 0;

static int diagLastAbnormal = 0;

static const char *resetReasonName(esp_reset_reason_t r)
{
  switch (r)
  {
  case ESP_RST_UNKNOWN:
    return "NONE";
  case ESP_RST_POWERON:
    return "POWERON";
  case ESP_RST_EXT:
    return "EXT";
  case ESP_RST_SW:
    return "SW";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
    return "INT_WDT";
  case ESP_RST_TASK_WDT:
    return "TASK_WDT";
  case ESP_RST_WDT:
    return "WDT";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  default:
    return "OTHER";
  }
}

static bool isAbnormalReset(esp_reset_reason_t r)
{
  return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
         r == ESP_RST_WDT || r == ESP_RST_BROWNOUT;
}

static void bootDiagRecord()
{
  esp_reset_reason_t rr = esp_reset_reason();
  diagResetReason = (int)rr;
  diagBootCount = nvsGetI(NVS_KEY_BOOT_CNT);
  diagAbnormalCount = nvsGetI(NVS_KEY_ABN_CNT);
  diagLastAbnormal = nvsGetI(NVS_KEY_LAST_ABN);
  if (rr != ESP_RST_DEEPSLEEP)
  {
    diagBootCount++;
    nvsSetIChanged(NVS_KEY_BOOT_CNT, diagBootCount);
  }
  if (isAbnormalReset(rr))
  {
    diagAbnormalCount++;
    diagLastAbnormal = (int)rr;
    nvsSetIChanged(NVS_KEY_ABN_CNT, diagAbnormalCount);
    nvsSetIChanged(NVS_KEY_LAST_ABN, diagLastAbnormal);
  }
  LOG_I("[BOOT] reset=%s boot=%d abnormal=%d last_abn=%s heap=%u\n",
        resetReasonName(rr), diagBootCount, diagAbnormalCount,
        resetReasonName((esp_reset_reason_t)diagLastAbnormal), (unsigned)esp_get_free_heap_size());
}

static uint32_t taskStackHwm(TaskHandle_t h)
{
  return h ? (uint32_t)uxTaskGetStackHighWaterMark(h) : 0;
}

static void otaTrackAttempt(const char *ver)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_OTA_TRY, ver);
  prefs.end();
  unlockNvs();
}

static void otaResolveAtBoot()
{
  String bad;
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  String t = prefs.getString(NVS_KEY_OTA_TRY, "");
  bad = prefs.getString(NVS_KEY_OTA_BAD, "");
  if (t.length() > 0)
  {
    if (!t.equals(FIRMWARE_VERSION))
    {
      bad = t;
      prefs.putString(NVS_KEY_OTA_BAD, bad);
      LOG_W("[OTA] v%s tidak berjalan (rollback?), ditandai buruk\n", t.c_str());
    }
    prefs.remove(NVS_KEY_OTA_TRY);
  }
  prefs.end();
  unlockNvs();
  strncpy(otaRejectedVer, bad.c_str(), sizeof(otaRejectedVer) - 1);
  otaRejectedVer[sizeof(otaRejectedVer) - 1] = '\0';
}
