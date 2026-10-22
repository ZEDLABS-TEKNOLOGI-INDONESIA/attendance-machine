/*
 * ATTENDANCE MACHINE
 * Device           : ESP32-C3 Super Mini
 * Tools            : Arduino IDE 2.3.6
 * Environtment     : v3.3.12
 * Schema Partition : Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)
 * Author           : Yahya Zulfikri
 * Created          : Juli 2025
 * Updated          : Oktober 2026
 * Version          : 2.3.8
 */
#include "src/config.h"
#include "src/types.h"
#include "src/globals.h"
#include "src/forward.h"
#include "src/credentials.h"
#include "src/tls.h"
#include "src/hw_helpers.h"
#include "src/watchdog.h"
#include "src/ui_feedback.h"
#include "src/nvs_store.h"
#include "src/diag.h"
#include "src/nvs_records.h"
#include "src/rfid_db.h"
#include "src/rtc_time.h"
#include "src/sd_log.h"
#include "src/queue_files.h"
#include "src/sd_health.h"
#include "src/queue_scan.h"
#include "src/quarantine.h"
#include "src/queue_write.h"
#include "src/nvs_sync.h"
#include "src/backoff.h"
#include "src/rfid_db_sync.h"
#include "src/telemetry.h"
#include "src/remote_config.h"
#include "src/ota.h"
#include "src/queue_sync.h"
#include "src/wifi_net.h"
#include "src/scan.h"
#include "src/ui_standby.h"
#include "src/factory_reset.h"
#include "src/provisioning.h"
#include "src/tasks.h"

static void logNvsStats()
{
  nvs_stats_t st;
  if (nvs_get_stats(nullptr, &st) == ESP_OK)
    LOG_I("[NVS] used=%u free=%u avail=%u total=%u ns=%u\n",
          (unsigned)st.used_entries, (unsigned)st.free_entries,
          (unsigned)st.available_entries, (unsigned)st.total_entries,
          (unsigned)st.namespace_count);
  else
    LOG_E("[NVS] nvs_get_stats gagal\n");
}

void setup()
{
#if SLEEP_GPIO_HOLD
  gpio_deep_sleep_hold_dis();
  gpio_hold_dis((gpio_num_t)PIN_SD_CS);
  gpio_hold_dis((gpio_num_t)PIN_RFID_SS);
#endif
  Serial.begin(115200);
#if SERIAL_TX_NONBLOCK
  Serial.setTxTimeoutMs(0);
#endif
  Serial.printf("[FW] %s\n", FW_MARKER);
  WiFi.onEvent(onWifiEvent);
  setenv("TZ", TZ_POSIX, 1);
  tzset();
  esp_task_wdt_deinit();
  const esp_task_wdt_config_t wdtCfg = {
      .timeout_ms = WDT_NORMAL_TIMEOUT_MS,
      .idle_core_mask = 0,
      .trigger_panic = true};
  esp_task_wdt_init(&wdtCfg);
  esp_task_wdt_add(nullptr);

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BOOT, INPUT_PULLUP);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  showStartupAnimation();
  playStartupMelody();
  esp_task_wdt_reset();
  xSdMutex = xSemaphoreCreateMutex();
  xDisplayMutex = xSemaphoreCreateMutex();
  xConfigMutex = xSemaphoreCreateMutex();
  xNvsMutex = xSemaphoreCreateMutex();
  xCacheMutex = xSemaphoreCreateMutex();
  logNvsStats();
  otaResolveAtBoot();
  bootDiagRecord();
  xRfidQueue = xQueueCreate(RFID_QUEUE_LEN, sizeof(RfidScanEvent));
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(deviceId, sizeof(deviceId), "ESP32_%02X%02X", mac[4], mac[5]);
  for (int i = 0; deviceId[i]; i++)
    deviceId[i] = toupper(deviceId[i]);

  esp_reset_reason_t rr = esp_reset_reason();
  if (rr == ESP_RST_DEEPSLEEP && timeWasSynced && lastValidTime > 0 && sleepDurationSeconds > 0)
  {
    lastValidTime += (time_t)sleepDurationSeconds;
    nvsSaveLastTime(lastValidTime);
  }
  sleepDurationSeconds = 0;
  if (timeWasSynced && lastValidTime > 0)
  {
    bootTime = millis();
    bootTimeSet = true;
  }

  if (!timeWasSynced || lastValidTime == 0)
  {
    time_t saved = nvsLoadLastTime();
    if (saved > 0)
    {
      lastValidTime = saved;
      timeWasSynced = true;
      bootTime = millis();
      bootTimeSet = true;
      timeFromNvsOnly = true;
    }
  }
  rtcInitAndApply();

  {
    time_t e = 0;
    bool eok = getEpochWithFallback(&e);
    LOG_I("[TIME] boot rr=%d sys=%lld lastValid=%lld est_ok=%d est=%lld\n",
          (int)rr, (long long)time(nullptr), (long long)lastValidTime, (int)eok, (long long)e);
  }
  isProvisioned = checkProvisioned();
  if (!isProvisioned)
  {
    showOLED(F("BELUM DIKONFIGURASI"), "MASUK SETUP MODE");
    delay(2000);
    startProvisioningMode();
    return;
  }
  loadCredentials();
  if (strlen(apiKey) == 0 || strlen(wifiCreds[0].ssid) == 0 || strlen(apiBaseUrl) == 0)
  {
    showOLED(F("CONFIG ERROR"), "MASUK SETUP MODE");
    delay(2000);
    startProvisioningMode();
    return;
  }
  if (strlen(deviceName) > 0)
  {
    if (strlen(deviceName) > DEVICE_NAME_MAX_LEN)
      deviceName[DEVICE_NAME_MAX_LEN] = '\0';
    snprintf(deviceId, sizeof(deviceId), "%s", deviceName);
  }
  SPI.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
  showProgress(F("INIT SD CARD"), 1500);
  sdCardAvailable = initSDCard();
  if (sdCardAvailable)
  {
    showOLED(F("SD CARD"), "TERSEDIA");
    playToneSuccess();
    delay(800);
    refreshPendingCache();
    if (cachedPendingRecords > 0)
    {
      char buf[20];
      snprintf(buf, sizeof(buf), "%d TERSISA", cachedPendingRecords);
      showOLED(F("DATA OFFLINE"), buf);
      delay(1000);
    }
    showProgress(F("LOAD RFID DB"), 500);
    if (loadRfidCacheFromFile())
    {
      char buf[20];
      snprintf(buf, sizeof(buf), "%d RFID", rfidCacheCount);
      showOLED(F("RFID DB"), buf);
      delay(600);
      if (rfidCacheDiscarded > 0)
      {
        snprintf(buf, sizeof(buf), "%d TERBUANG", rfidCacheDiscarded);
        showOLED(F("DB MELEBIHI BATAS"), buf);
        playToneError();
        delay(1500);
      }
    }
    loadAdminRfidList();
  }
  else
  {
    LOG_E("[SD] init gagal, sdErrorCode=0x%02X\n", (unsigned)sdInitErr);
    if (sdInitErr == 0)
      showOLED(F("SD FS RUSAK"), "FORMAT FAT32 DI PC");
    else
      showOLED(F("SD CARD"), "TIDAK TERBACA");
    playToneError();
    delay(1500);
    int nc = nvsGetCount();
    if (nc > 0)
    {
      char buf[20];
      snprintf(buf, sizeof(buf), "%d TERSISA", nc);
      showOLED(F("NVS BUFFER"), buf);
      delay(1000);
    }
  }
  showProgress(F("INIT RFID"), 1000);
  rfidReader.PCD_Init();
  delay(100);
  digitalWrite(PIN_RFID_SS, HIGH);
  byte ver = rfidReader.PCD_ReadRegister(rfidReader.VersionReg);
  if (ver == 0x00 || ver == 0xFF)
  {
    int fails = nvsGetI(NVS_KEY_RC_FAIL) + 1;
    if (fails > MAX_RC522_RESTARTS + 1)
      fails = MAX_RC522_RESTARTS + 1;
    nvsSetIChanged(NVS_KEY_RC_FAIL, fails);
    LOG_E("[RC522] versi=0x%02X gagal, percobaan %d\n", (unsigned)ver, fails);
    if (fails <= MAX_RC522_RESTARTS)
    {
      showOLED(F("RC522 GAGAL"), "RESTART...");
      playToneError();
      delay(3000UL * fails);
      ESP.restart();
    }
    rfidReady = false;
    showOLED(F("RC522 RUSAK"), "MODE TERBATAS");
    playToneError();
    delay(2000);
  }
  else
  {
    rfidReady = true;
    nvsSetIChanged(NVS_KEY_RC_FAIL, 0);
  }
  esp_ota_mark_app_valid_cancel_rollback();
  showProgress(F("CONNECTING WIFI"), 1500);
  bool wifiOk = connectToWiFi();
  esp_task_wdt_reset();
  if (!wifiOk)
  {
    showOLED(F("NO WIFI"), "OFFLINE MODE");
    playToneError();
    delay(1500);
  }
  else
  {
    showOLED(F("SYNCING TIME"), "MOHON TUNGGU...");
    syncTimeWithFallback();
    esp_task_wdt_reset();
    showProgress(F("PING API"), 500);
    int apiRetry = 0;
    while (!pingAPI() && apiRetry < 3)
    {
      apiRetry++;
      char buf[12];
      snprintf(buf, sizeof(buf), "Retry %d/3", apiRetry);
      showOLED(F("API GAGAL"), buf);
      playToneError();
      delay(800);
      esp_task_wdt_reset();
    }
    if (isOnline && !isSignalCritical())
    {
      showOLED(F("API OK"), "ONLINE");
      playToneSuccess();
      delay(500);
      esp_task_wdt_reset();
      int nc = nvsGetCount();
      if (nc > 0)
      {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d NVS RECORDS", nc);
        showOLED(F("SYNC NVS"), buf);
        delay(800);
        nvsSyncToServer();
        esp_task_wdt_reset();
      }
      if (sdCardAvailable && isWifiConnected())
      {
        refreshPendingCache();
        if (cachedPendingRecords > 0)
        {
          char buf[20];
          snprintf(buf, sizeof(buf), "%d records", cachedPendingRecords);
          showOLED(F("SYNC DATA"), buf);
          delay(1000);
          chunkedSync();
          esp_task_wdt_reset();
        }
        showProgress(F("SYNC RFID DB"), 500);
        unsigned long lv = nvsGetRfidDbVer(), sv = checkRfidDbVersion();
        if (sv > lv)
          downloadRfidDb();
        else
          showOLED(F("RFID DB"), "UP TO DATE");
        delay(600);
        esp_task_wdt_reset();
      }
    }
    else
    {
      showOLED(F("API GAGAL"), "OFFLINE MODE");
      playToneError();
      delay(1500);
    }
  }
  if (!isTimeTrusted())
  {
    int retry = nvsGetBootRetry() + 1;
    if (retry <= MAX_BOOT_TIME_SYNC_RETRIES)
    {
      nvsSetBootRetry(retry);
      char buf[24];
      snprintf(buf, sizeof(buf), "RETRY %d/%d", retry, MAX_BOOT_TIME_SYNC_RETRIES);
      showOLED(F("WAKTU GAGAL SYNC"), buf);
      playToneError();
      delay(2000);
      ESP.restart();
    }
    else
    {
      showOLED(F("WAKTU TETAP GAGAL"), "LANJUT OFFLINE");
      playToneError();
      delay(2000);
      nvsSetBootRetry(0);
      bootTimeSyncFailed = true;
    }
  }
  else
  {
    nvsSetBootRetry(0);
    bootTimeSyncFailed = false;
  }
  showOLED(F("SISTEM SIAP"), isOnline ? "ONLINE" : "OFFLINE");
  playToneSuccess();
  if (!bootTimeSet)
  {
    bootTime = millis();
    bootTimeSet = true;
  }
  unsigned long now = millis();
  timers.lastSync = now;
  timers.lastTimeSync = now;
  timers.lastReconnect = isWifiConnected() ? now : 0;
  timers.lastDisplayUpdate = now;
  timers.lastPeriodicCheck = now;
  timers.lastOLEDScheduleCheck = now;
  timers.lastSDRedetect = now;
  timers.lastNvsSync = now;
  timers.lastOtaCheck = 0;
  timers.lastRfidDbCheck = now;
  timers.lastTelemetry = now;
  timers.lastRemoteConfig = now;
  delay(1000);
  checkOLEDSchedule();
  hTaskLoop = xTaskGetCurrentTaskHandle();
  xTaskCreatePinnedToCore(taskRfid, "rfid", TASK_RFID_STACK, nullptr, TASK_RFID_PRIORITY, &hTaskRfid, 0);
  xTaskCreatePinnedToCore(taskSync, "sync", TASK_SYNC_STACK, nullptr, TASK_SYNC_PRIORITY, &hTaskSync, 0);
  xTaskCreatePinnedToCore(taskDisplay, "disp", TASK_DISPLAY_STACK, nullptr, TASK_DISPLAY_PRIORITY, &hTaskDisplay, 0);
}

static void pollRfidReader()
{
  if (!rfidReady)
    return;
  static uint32_t skips = 0;
  static unsigned long lastLog = 0;
  if (!acquireSD(pdMS_TO_TICKS(30)))
  {
    skips++;
    if (millis() - lastLog >= 60000UL)
    {
      lastLog = millis();
      LOG_W("[RFID] poll dilewati total=%lu (mutex SD sibuk)\n", (unsigned long)skips);
    }
    return;
  }
  deselectSD();
  bool got = false;
  RfidScanEvent ev = {};
  if (rfidReader.PICC_IsNewCardPresent() && rfidReader.PICC_ReadCardSerial())
  {
    uint8_t n = rfidReader.uid.size;
    if (n > sizeof(ev.uid))
      n = sizeof(ev.uid);
    memcpy(ev.uid, rfidReader.uid.uidByte, n);
    ev.uidLen = n;
    rfidReader.PICC_HaltA();
    rfidReader.PCD_StopCrypto1();
    got = true;
  }
  releaseSD();
  if (got)
    xQueueSend(xRfidQueue, &ev, 0);
}

static int calcSleepSeconds(const RuntimeConfig &cfg)
{
  time_t t;
  if (!getEpochWithFallback(&t))
    return 60;
  struct tm tn;
  localtime_r(&t, &tn);
  int h = tn.tm_hour;
  int nowSec = tn.tm_hour * 3600 + tn.tm_min * 60 + tn.tm_sec;
  int endSec;
  if (cfg.sleepStartHour < cfg.sleepEndHour)
    endSec = cfg.sleepEndHour * 3600;
  else
    endSec = (h >= cfg.sleepStartHour) ? cfg.sleepEndHour * 3600 + 86400 : cfg.sleepEndHour * 3600;
  int s = endSec - nowSec;
  if (s < 60)
    s = 60;
  if (s > 43200)
    s = 43200;
  return s;
}

void loop()
{
  esp_task_wdt_reset();
  pollRfidReader();
  struct tm ti;
  if (getTimeWithFallback(&ti) && !bootTimeSyncFailed && isTimeTrusted())
  {
    RuntimeConfig cfg = getRuntimeConfigSnapshot();
    int h = ti.tm_hour;
    if (isInWindow(h, cfg.sleepStartHour, cfg.sleepEndHour))
    {
      static bool sleepFailed = false;
      static unsigned long lastSleepFail = 0;
      if ((sleepFailed && millis() - lastSleepFail < SLEEP_RETRY_MS) || syncState.inProgress || otaInProgress)
      {
        vTaskDelay(pdMS_TO_TICKS(10));
        return;
      }
      sleepRequested = true;
      unsigned long waitStart = millis();
      while (millis() - waitStart < SLEEP_PARK_TIMEOUT_MS && !(parkRfid && parkSync && parkDisp))
      {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100));
      }
      if (!(parkRfid && parkSync && parkDisp) || !flushAllFiles())
      {
        LOG_W("[SLEEP] dibatalkan: task belum berhenti atau flush SD gagal\n");
        sleepRequested = false;
        sleepFailed = true;
        lastSleepFail = millis();
        return;
      }
      sleepFailed = false;
      showOLED(F("SLEEP MODE"), "...");
      delay(1000);
      char buf[24];
      int shown = calcSleepSeconds(cfg);
      snprintf(buf, sizeof(buf), "%dj %dm", shown / 3600, (shown % 3600) / 60);
      showOLED(F("SLEEP FOR"), buf);
      delay(2000);
      if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(500)) == pdTRUE)
      {
        display.clearDisplay();
        display.display();
        display.ssd1306_command(SSD1306_DISPLAYOFF);
        xSemaphoreGive(xDisplayMutex);
      }
      int sleepSec = calcSleepSeconds(cfg);
      {
        time_t t;
        if (getEpochWithFallback(&t))
        {
          lastValidTime = t;
          bootTime = millis();
          bootTimeSet = true;
          timeWasSynced = true;
          nvsSaveLastTime(t);
        }
      }
      sleepDurationSeconds = (uint64_t)sleepSec;
      if (acquireSD(pdMS_TO_TICKS(500)))
      {
        deselectSD();
        rfidReader.PCD_SoftPowerDown();
        releaseSD();
      }
      restoreWdtNormal();
      if (hTaskLoop)
        esp_task_wdt_delete(hTaskLoop);
      if (hTaskRfid)
        esp_task_wdt_delete(hTaskRfid);
      if (hTaskSync)
        esp_task_wdt_delete(hTaskSync);
      if (hTaskDisplay)
        esp_task_wdt_delete(hTaskDisplay);
      esp_task_wdt_deinit();
#if SLEEP_GPIO_HOLD
      digitalWrite(PIN_SD_CS, HIGH);
      digitalWrite(PIN_RFID_SS, HIGH);
      gpio_hold_en((gpio_num_t)PIN_SD_CS);
      gpio_hold_en((gpio_num_t)PIN_RFID_SS);
      gpio_deep_sleep_hold_en();
#endif
      esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
      esp_deep_sleep_start();
    }
  }
  vTaskDelay(pdMS_TO_TICKS(10));
}
