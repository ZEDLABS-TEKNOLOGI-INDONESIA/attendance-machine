#pragma once
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
MFRC522 rfidReader(PIN_RFID_SS, PIN_RFID_RST);
SdFat sd;
FsFile file;
Preferences prefs;
WebServer provServer(80);
DNSServer dnsServer;
Timers timers = {};
DisplayState currentDisplay = {false, "00:00", 0, 0};
DisplayState previousDisplay = {false, "--:--", -1, -1};
SyncState syncState = {0, false, 0, 0, 0};
RfidFeedback rfidFeedback = {false, 0, false};
OtaState otaState = {false, "", "", ""};
RuntimeConfig rtCfg = {
    SLEEP_START_HOUR_DEFAULT, SLEEP_END_HOUR_DEFAULT,
    OLED_DIM_START_HOUR_DEFAULT, OLED_DIM_END_HOUR_DEFAULT,
    SYNC_INTERVAL, OTA_CHECK_INTERVAL
  };
char lastUID[11] = "";
char deviceId[20] = "";
char deviceName[DEVICE_NAME_MAX_LEN + 1] = "";
char provApPassword[16] = "";
bool isOnline = false;
bool sdCardAvailable = false;
uint8_t sdInitErr = 0;
volatile bool oledIsOn = true;
bool isProvisioned = false;
bool rtcPresent = false;
bool rfidReady = true;
bool timeFromNvsOnly = false;
volatile bool wdtExtended = false;
portMUX_TYPE wdtMux = portMUX_INITIALIZER_UNLOCKED;
int cachedPendingRecords = 0;
bool pendingCacheDirty = true;
int cachedQueueFileCount = 0;
ReconnectState reconnectState = RECONNECT_IDLE;
unsigned long reconnectStartTime = 0;
int currentSsidIdx = 0;
char rfidCacheFlat[RFID_CACHE_MAX][11];
int rfidCacheCount = 0;
bool rfidCacheLoaded = false;
bool rfidDbValid = false;
int rfidCacheDiscarded = 0;
char adminRfidList[5][11];
int adminRfidCount = 0;
TaskHandle_t hTaskRfid = nullptr;
TaskHandle_t hTaskSync = nullptr;
TaskHandle_t hTaskDisplay = nullptr;
TaskHandle_t hTaskLoop = nullptr;
SemaphoreHandle_t xSdMutex = nullptr;
SemaphoreHandle_t xConfigMutex = nullptr;
SemaphoreHandle_t xNvsMutex = nullptr;
SemaphoreHandle_t xCacheMutex = nullptr;
SemaphoreHandle_t xDisplayMutex = nullptr;
QueueHandle_t xRfidQueue = nullptr;
volatile bool sleepRequested = false;
volatile bool parkRfid = false, parkSync = false, parkDisp = false;
volatile bool otaInProgress = false;
volatile bool forceSyncRequested = false;
volatile uint32_t queueMutations = 0;

char otaRejectedVer[16] = "";

volatile int syncingQueueFile = -1;

extern "C" bool verifyRollbackLater()
{
  return true;
}

static bool lockNvs(TickType_t timeout = pdMS_TO_TICKS(2000))
{
  if (!xNvsMutex)
    return true;
  return xSemaphoreTake(xNvsMutex, timeout) == pdTRUE;
}

static void unlockNvs()
{
  if (xNvsMutex)
    xSemaphoreGive(xNvsMutex);
}

static bool lockCache(TickType_t timeout = pdMS_TO_TICKS(2000))
{
  if (!xCacheMutex)
    return true;
  return xSemaphoreTake(xCacheMutex, timeout) == pdTRUE;
}

static void unlockCache()
{
  if (xCacheMutex)
    xSemaphoreGive(xCacheMutex);
}
