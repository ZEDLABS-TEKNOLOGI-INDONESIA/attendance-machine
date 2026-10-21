/*
 * ATTENDANCE MACHINE
 * Device           : ESP32-C3 Super Mini
 * Tools            : Arduino IDE 2.3.7
 * Environtment     : v3.3.12
 * Schema Partition : Minimal SPIFFS (1.9MB APP with OTA/128KB SPIFFS)
 * Author           : Yahya Zulfikri
 * Created          : Juli 2025
 * Updated          : Agustus 2026
 * Version          : 2.3.7
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <MFRC522.h>
#include <SPI.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <SdFat.h>
#include <esp_mac.h>
#include <esp_efuse_table.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <mbedtls/aes.h>
#include <mbedtls/md.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <nvs_flash.h>
#include <esp_sntp.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <driver/gpio.h>
#include "pure_logic.h"

#define PIN_SPI_SCK 4
#define PIN_SPI_MOSI 6
#define PIN_SPI_MISO 5
#define PIN_RFID_SS 7
#define PIN_RFID_RST 3
#define PIN_SD_CS 1
#define PIN_OLED_SDA 8
#define PIN_OLED_SCL 9
#define PIN_BUZZER 10
#define PIN_BOOT 0
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define DEBOUNCE_TIME 150UL
#define SYNC_INTERVAL 300000UL
#define MAX_OFFLINE_AGE 2592000UL // 30 hari = MAX_DATA_AGE di server (PresensiController)
#define MIN_REPEAT_INTERVAL 1800UL
#define TIME_SYNC_INTERVAL 1800000UL
#define RECONNECT_INTERVAL 60000UL
#define RECONNECT_TIMEOUT 20000UL
#define DISPLAY_UPDATE_INTERVAL 1000UL
#define PERIODIC_CHECK_INTERVAL 1000UL
#define OLED_SCHEDULE_CHECK_INTERVAL 60000UL
#define RFID_FEEDBACK_DISPLAY_MS 1800UL
#define SD_REDETECT_INTERVAL 30000UL
#define MAX_TIME_ESTIMATE_AGE 43200UL
#define OTA_CHECK_INTERVAL 600000UL
#define CFG_INTERVAL_MIN_MS 5000UL
#define CFG_INTERVAL_MAX_MS 86400000UL // 24 jam; jauh di bawah wrap millis (±49 hari)
#define RFID_DB_CHECK_INTERVAL 300000UL
#define TELEMETRY_INTERVAL 120000UL
#define REMOTE_CONFIG_INTERVAL 600000UL
#define FACTORY_RESET_HOLD_MS 5000UL
#define FACTORY_RESET_WIPE_QUEUE_MS 10000UL // tahan selama ini = reset + hapus antrean SD
#define PROVISIONING_TIMEOUT_MS 300000UL

#define WDT_SYNC_TIMEOUT_MS 180000UL
#define WDT_NORMAL_TIMEOUT_MS 90000UL
#define SD_MUTEX_TIMEOUT_MS 5000UL
#define MAX_RECORDS_PER_FILE 25
#define MAX_DUPLICATE_CHECK_FILES 3
#define MAX_DUPLICATE_CHECK_LINES (MAX_RECORDS_PER_FILE + 1)
#define QUEUE_WARN_THRESHOLD 48000
#define METADATA_FILE "/queue_meta.txt"
#define MAX_SYNC_FILES_PER_CYCLE 5
#define MAX_SYNC_RETRIES 2
#define QUEUE_SLOT_SEARCH_LIMIT 50
#define SYNC_RETRY_DELAY_MS 2000UL
#define FAILED_LOG_MAX_LINES 500
#define NVS_MAX_RECORDS 40
#define NVS_NAMESPACE "presensi"
#define NVS_KEY_COUNT "nvs_count"
#define NVS_KEY_PREFIX "rec_"
#define NVS_KEY_LAST_TIME "last_time"
#define NVS_KEY_RFID_VER "rfid_db_ver"
#define NVS_KEY_SCAN_DATE "scan_date"
#define NVS_KEY_SCAN_COUNT "scan_count"
#define NVS_KEY_BOOT_RETRY "boot_retry"
#define NVS_NS_CONFIG "cfg"
#define NVS_KEY_SSID1 "ssid1"
#define NVS_KEY_PASS1 "pass1"
#define NVS_KEY_SSID2 "ssid2"
#define NVS_KEY_PASS2 "pass2"
#define NVS_KEY_SSID3 "ssid3"
#define NVS_KEY_PASS3 "pass3"
#define NVS_KEY_APIKEY "apikey"
#define NVS_KEY_DEVNAME "devname"
#define NVS_KEY_APIURL "apiurl"
#define NVS_KEY_CFG_SLP_S "slp_s"
#define NVS_KEY_CFG_SLP_E "slp_e"
#define NVS_KEY_CFG_DIM_S "dim_s"
#define NVS_KEY_CFG_DIM_E "dim_e"
#define NVS_KEY_CFG_SYNCIV "sync_iv"
#define NVS_KEY_CFG_OTAIV "ota_iv"
#define NVS_KEY_PROVISIONED "prov"
#define NVS_KEY_LAST_RFID "last_rfid"
#define NVS_KEY_LAST_SCAN_T "last_scan_t"
#define RFID_DB_FILE "/rfid_db.txt"
#define RFID_DB_BAK "/rfid_db.bak"
#define RFID_CACHE_MAX 5000
#define ADMIN_RFID_FILE "/admin_rfid.txt"
#define SLEEP_START_HOUR_DEFAULT 18
#define SLEEP_END_HOUR_DEFAULT 1
#define OLED_DIM_START_HOUR_DEFAULT 8
#define OLED_DIM_END_HOUR_DEFAULT 10

#define SIGNAL_THRESHOLD_WEAK -85
#define SIGNAL_THRESHOLD_CRITICAL -90
#define FIRMWARE_VERSION "2.3.7"
#define PROV_AP_SSID "ATTENDANCE MACHINE"
#define PROV_DNS_PORT 53
#define TASK_RFID_STACK 8192
#define TASK_SYNC_STACK 8192
#define TASK_DISPLAY_STACK 12288
#define TASK_RFID_PRIORITY 3
#define TASK_SYNC_PRIORITY 2
#define TASK_DISPLAY_PRIORITY 1
#define RFID_QUEUE_LEN 8
#define DEVICE_NAME_MAX_LEN 19
#define MAX_BOOT_TIME_SYNC_RETRIES 5
#define RTC_I2C_ADDR 0x68 // DS1307 / DS3231, bus I2C sama dengan OLED (GPIO 8/9)
#define RTC_MIN_VALID_YEAR 2025
#define MIN_VALID_EPOCH 1735689600L // 2025-01-01 00:00:00 UTC. HARUS selaras dengan RTC_MIN_VALID_YEAR
#define NTP_SERVER_TIMEOUT_MS 3000UL
#define TZ_POSIX "WIB-7"
#ifndef LOG_LEVEL
#define LOG_LEVEL 3 // 0=mati 1=E 2=W 3=I 4=D. Rilis hemat: -DLOG_LEVEL=0
#endif
#define LOG_E(...)                \
  do                              \
  {                               \
    if (LOG_LEVEL >= 1)           \
      Serial.printf(__VA_ARGS__); \
  } while (0)
#define LOG_W(...)                \
  do                              \
  {                               \
    if (LOG_LEVEL >= 2)           \
      Serial.printf(__VA_ARGS__); \
  } while (0)
#define LOG_I(...)                \
  do                              \
  {                               \
    if (LOG_LEVEL >= 3)           \
      Serial.printf(__VA_ARGS__); \
  } while (0)
#define LOG_D(...)                \
  do                              \
  {                               \
    if (LOG_LEVEL >= 4)           \
      Serial.printf(__VA_ARGS__); \
  } while (0)
#define SERIAL_TX_NONBLOCK 0 // D2: set 1 hanya setelah uji 3.6 membuktikan pemblokiran
#define MAX_RC522_RESTARTS 3
#define SLEEP_PARK_TIMEOUT_MS 60000UL
#define SLEEP_RETRY_MS 30000UL
#define SLEEP_GPIO_HOLD 0 // TIDAK TERUJI: aktifkan hanya setelah arus diukur
#define NVS_KEY_BOOT_CNT "boot_cnt"
#define NVS_KEY_ABN_CNT "abn_cnt"
#define NVS_KEY_LAST_ABN "last_abn"
#define NVS_KEY_RC_FAIL "rc_fail"
#define NVS_KEY_OTA_TRY "ota_try"
#define NVS_KEY_OTA_BAD "ota_bad"
// Jam hasil estimasi NVS (cold boot tanpa RTC) tidak menghitung lama listrik mati.
// 1 = scan ditolak sampai NTP/RTC berhasil. 0 = scan tetap diterima dengan jam estimasi.
#define REJECT_SCAN_UNTRUSTED_TIME 1
#define UNTRUSTED_TIME_RETRY_INTERVAL 60000UL
#define CRED_DATA_MAX 96 // kelipatan 16
#define CRED_PLAIN_MAX (CRED_DATA_MAX - 1)
// Kosong = wajib diisi lewat provisioning. Jangan isi domain satu sekolah di firmware generik.
#define DEFAULT_API_URL ""
#define QUEUE_HEADER_LINE "rfid,timestamp,device_id,unix_time,crc8"
#define QUARANTINE_FILE "/quarantine.csv"
#define QUARANTINE_OLD "/quarantine.old"
#define QUARANTINE_MAX_BYTES 262144UL // ±1500 baris, lalu rotasi ke .old (menimpa .old lama)
#define LINE_OK 0
#define LINE_EMPTY 1
#define LINE_MALFORMED 2
#define LINE_BAD_CRC 3
#define LINE_EXPIRED 4
// Penanda versi di dalam image, dibaca saat OTA untuk mencocokkan versi yang ditawarkan server.
// `retain` mencegah linker membuang variabel ini (gc-sections). Referensi runtime di setup()
// menjadi pengaman kedua, karena pada toolchain tertentu `retain` diabaikan.
__attribute__((used, retain)) static const char FW_MARKER[] = "FWVER:" FIRMWARE_VERSION ";";
RTC_DATA_ATTR bool bootTimeSyncFailed = false;
static const char NTP_SERVER_1[] PROGMEM = "pool.ntp.org";
static const char NTP_SERVER_2[] PROGMEM = "time.google.com";
static const char NTP_SERVER_3[] PROGMEM = "id.pool.ntp.org";
RTC_DATA_ATTR time_t lastValidTime = 0;
RTC_DATA_ATTR bool timeWasSynced = false;
RTC_DATA_ATTR unsigned long bootTime = 0;
RTC_DATA_ATTR bool bootTimeSet = false;
RTC_DATA_ATTR int currentQueueFile = 0;
RTC_DATA_ATTR bool rtcQueueFileValid = false;
RTC_DATA_ATTR uint64_t sleepDurationSeconds = 0;
enum ReconnectState
{
  RECONNECT_IDLE,
  RECONNECT_INIT,
  RECONNECT_TRYING,
  RECONNECT_SUCCESS,
  RECONNECT_FAILED
};
enum SaveResult
{
  SAVE_OK,
  SAVE_DUPLICATE,
  SAVE_QUEUE_FULL,
  SAVE_SD_ERROR
};
enum SyncFileResult
{
  SYNC_FILE_OK,
  SYNC_FILE_EMPTY,
  SYNC_FILE_HTTP_FAIL,
  SYNC_FILE_NO_WIFI
};

enum RfidLookup
{
  RFID_FOUND,
  RFID_NOT_FOUND,
  RFID_BUSY,
  RFID_NO_DB // belum pernah ada DB lokal (file belum diunduh)
};

struct Timers
{
  unsigned long lastScan, lastSync, lastTimeSync, lastReconnect;
  unsigned long lastDisplayUpdate, lastPeriodicCheck, lastOLEDScheduleCheck;
  unsigned long lastSDRedetect, lastNvsSync, lastOtaCheck, lastRfidDbCheck;
  unsigned long lastTelemetry, lastRemoteConfig;
};
struct DisplayState
{
  bool isOnline;
  char time[6];
  int pendingRecords;
  int wifiSignal;
};
struct OfflineRecord
{
  char rfid[11];
  char timestamp[20];
  char deviceId[20];
  unsigned long unixTime;
};
struct SyncState
{
  int currentFile;
  bool inProgress;
  unsigned long startTime;
  int filesProcessed;
  int filesSucceeded;
};
struct RfidFeedback
{
  bool active;
  unsigned long shownAt;
  bool wasOledOff;
};
struct OtaState
{
  bool updateAvailable;
  char version[16];
  char url[128];
  char md5[36];
};

struct RfidScanEvent
{
  uint8_t uid[10];
  uint8_t uidLen;
};
struct RuntimeConfig
{
  int sleepStartHour;
  int sleepEndHour;
  int dimStartHour;
  int dimEndHour;
  unsigned long syncIntervalMs;
  unsigned long otaCheckIntervalMs;
};

struct EncryptedCredential
{
  uint8_t iv[16];
  uint8_t data[CRED_DATA_MAX];
  uint8_t len;
};
// Format lama (blob 65 byte) tetap bisa dibaca agar device lama tidak minta provisioning ulang.
struct EncryptedCredentialV1
{
  uint8_t iv[16];
  uint8_t data[48];
  uint8_t len;
};
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
    SYNC_INTERVAL, OTA_CHECK_INTERVAL};
char lastUID[11] = "";
char deviceId[20] = "";
char deviceName[DEVICE_NAME_MAX_LEN + 1] = "";
char provApPassword[16] = "";
bool isOnline = false;
bool sdCardAvailable = false;
uint8_t sdInitErr = 0; // sd.sdErrorCode() saat sd.begin gagal; 0 + gagal = FS tidak valid
volatile bool oledIsOn = true;
bool isProvisioned = false;
bool rtcPresent = false; // RTC eksternal terdeteksi saat boot (opsional)
bool rfidReady = true;   // false = RC522 tidak merespons (mode terdegradasi)
// true hanya jika jam dipulihkan dari NVS setelah cold boot. Tidak mencakup lama listrik mati.
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
int rfidCacheDiscarded = 0; // jumlah kartu yang tidak muat di RFID_CACHE_MAX
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
volatile bool parkRfid = false, parkSync = false, parkDisp = false; // true = task berhenti di titik aman
volatile bool otaInProgress = false;
volatile bool forceSyncRequested = false; // diset taskRfid (kartu admin), dikonsumsi taskSync
// Naik setiap kali isi antrean SD berubah (record ditambah / file dihapus setelah sync).
// refreshPendingCache() memakainya untuk mendeteksi perubahan selama pemindaian.
volatile uint32_t queueMutations = 0;
char otaRejectedVer[16] = ""; // versi OTA yang gagal verifikasi, tidak dicoba lagi sampai reboot
// Indeks file antrean yang sedang di-sync (-1 = tidak ada). Writer tidak boleh memakai file ini.

volatile int syncingQueueFile = -1;
// Tahan penandaan "valid" otomatis dari core Arduino. Tanpa ini core sudah menandai valid
// sebelum setup(), sehingga rollback tidak pernah berlaku. Penandaan dilakukan manual di setup()
// setelah inisialisasi perangkat keras kritis berhasil.
// PENTING: letakkan SETELAH semua enum/struct. Fungsi ini tidak boleh menjadi fungsi pertama
// di file, karena arduino-cli menyisipkan prototipe otomatis tepat sebelum fungsi pertama.
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

// Mutex untuk cache RFID (rfidCacheFlat, rfidCacheCount, rfidCacheLoaded, rfidDbValid).
// Urutan kunci: xSdMutex dulu, lalu xCacheMutex. Fungsi lookup tidak boleh mengambil SD.
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

static void deriveAesKey(uint8_t key[16])
{
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);
  uint8_t seed[22];
  memcpy(seed, mac, 6);
  const char *salt = "ZEDLABS_PRESENSI";
  memcpy(seed + 6, salt, 16);
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, seed, 22);
  uint8_t hash[32];
  mbedtls_md_finish(&ctx, hash);
  mbedtls_md_free(&ctx);
  memcpy(key, hash, 16);
}

static bool encryptString(const char *plain, EncryptedCredential &out)
{
  size_t plen = strlen(plain);
  if (plen > CRED_PLAIN_MAX)
    return false;
  uint8_t key[16];
  deriveAesKey(key);
  uint8_t buf[CRED_DATA_MAX] = {};
  memcpy(buf, plain, plen);
  out.len = (uint8_t)plen;
  esp_fill_random(out.iv, 16);
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_enc(&aes, key, 128);
  uint8_t iv[16];
  memcpy(iv, out.iv, 16);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_ENCRYPT, CRED_DATA_MAX, iv, buf, out.data);
  mbedtls_aes_free(&aes);
  return true;
}

static bool decryptBytes(const uint8_t *ivIn, const uint8_t *data, size_t dataLen,
                         uint8_t len, char *plain, size_t maxLen)
{
  // Validasi: panjang blob, dan len tidak boleh melebihi isi blob (NVS korup).
  if (maxLen == 0 || dataLen == 0 || dataLen > CRED_DATA_MAX || (dataLen % 16) != 0 || len > dataLen)
    return false;
  uint8_t key[16];
  deriveAesKey(key);
  uint8_t buf[CRED_DATA_MAX];
  uint8_t iv[16];
  memcpy(iv, ivIn, 16);
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  mbedtls_aes_setkey_dec(&aes, key, 128);
  mbedtls_aes_crypt_cbc(&aes, MBEDTLS_AES_DECRYPT, dataLen, iv, data, buf);
  mbedtls_aes_free(&aes);
  size_t copyLen = (len < maxLen - 1) ? len : maxLen - 1;
  memcpy(plain, buf, copyLen);
  plain[copyLen] = '\0';
  return true;
}

static bool saveEncryptedNvs(const char *ns, const char *key, const char *plain)
{
  EncryptedCredential ec;
  if (!encryptString(plain, ec))
    return false;
  prefs.begin(ns, false);
  size_t w = prefs.putBytes(key, &ec, sizeof(EncryptedCredential));
  prefs.end();
  return w == sizeof(EncryptedCredential);
}

static bool loadEncryptedNvs(const char *ns, const char *key, char *plain, size_t maxLen)
{
  bool ok = false;
  prefs.begin(ns, true);
  size_t len = prefs.getBytesLength(key);
  if (len == sizeof(EncryptedCredential))
  {
    EncryptedCredential ec;
    prefs.getBytes(key, &ec, sizeof(EncryptedCredential));
    ok = decryptBytes(ec.iv, ec.data, sizeof(ec.data), ec.len, plain, maxLen);
  }
  else if (len == sizeof(EncryptedCredentialV1))
  {
    EncryptedCredentialV1 ec;
    prefs.getBytes(key, &ec, sizeof(EncryptedCredentialV1));
    ok = decryptBytes(ec.iv, ec.data, sizeof(ec.data), ec.len, plain, maxLen);
  }
  prefs.end();
  return ok;
}

struct WifiCredential
{
  char ssid[33]; // SSID maks 32 karakter + NUL
  char pass[64]; // WPA2 maks 63 karakter + NUL
};
static WifiCredential wifiCreds[3];
static char apiKey[CRED_DATA_MAX] = "";
static char apiBaseUrl[80] = DEFAULT_API_URL;
#define API_URL_BUF 160
// Gabung base URL + path dengan batas aman. false jika terpotong.
static bool buildApiUrl(char *out, size_t outSz, const char *path)
{
  int n = snprintf(out, outSz, "%s%s", apiBaseUrl, path);
  return n > 0 && (size_t)n < outSz;
}
static void urlEncode(const char *src, char *dst, size_t dstSize);

// Karakter aman untuk CSV, JSON manual, dan URL.
static bool isSafeDeviceNameChar(char c)
{
  return isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' || c == '.';
}
// Untuk nama lama yang sudah tersimpan di NVS: ganti karakter tidak aman dengan '_'.
static void sanitizeDeviceName(char *s)
{
  for (; *s; s++)
    if (!isSafeDeviceNameChar(*s))
      *s = '_';
}
static void loadCredentials()
{
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_SSID1, wifiCreds[0].ssid, sizeof(wifiCreds[0].ssid));
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_PASS1, wifiCreds[0].pass, sizeof(wifiCreds[0].pass));
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_SSID2, wifiCreds[1].ssid, sizeof(wifiCreds[1].ssid));
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_PASS2, wifiCreds[1].pass, sizeof(wifiCreds[1].pass));
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_SSID3, wifiCreds[2].ssid, sizeof(wifiCreds[2].ssid));
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_PASS3, wifiCreds[2].pass, sizeof(wifiCreds[2].pass));
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_APIKEY, apiKey, sizeof(apiKey));
  loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_DEVNAME, deviceName, sizeof(deviceName));
  sanitizeDeviceName(deviceName);
  char tmpUrl[80] = "";
  if (loadEncryptedNvs(NVS_NS_CONFIG, NVS_KEY_APIURL, tmpUrl, sizeof(tmpUrl)))
  {
    if (strlen(tmpUrl) > 0)
    {
      int ul = strlen(tmpUrl);
      while (ul > 0 && tmpUrl[ul - 1] == '/')
        tmpUrl[--ul] = '\0';
      strncpy(apiBaseUrl, tmpUrl, sizeof(apiBaseUrl) - 1);
      apiBaseUrl[sizeof(apiBaseUrl) - 1] = '\0';
    }
  }
  prefs.begin(NVS_NS_CONFIG, true);
  int slpS = prefs.getInt(NVS_KEY_CFG_SLP_S, SLEEP_START_HOUR_DEFAULT);
  int slpE = prefs.getInt(NVS_KEY_CFG_SLP_E, SLEEP_END_HOUR_DEFAULT);
  int dimS = prefs.getInt(NVS_KEY_CFG_DIM_S, OLED_DIM_START_HOUR_DEFAULT);
  int dimE = prefs.getInt(NVS_KEY_CFG_DIM_E, OLED_DIM_END_HOUR_DEFAULT);
  unsigned long syncIv = prefs.getULong(NVS_KEY_CFG_SYNCIV, SYNC_INTERVAL);
  unsigned long otaIv = prefs.getULong(NVS_KEY_CFG_OTAIV, OTA_CHECK_INTERVAL);
  prefs.end();
  auto clampHour = [](int h, int def)
  {
    return (h >= 0 && h <= 23) ? h : def;
  };
  auto clampInterval = [](unsigned long v, unsigned long def)
  {
    return (v >= CFG_INTERVAL_MIN_MS && v <= CFG_INTERVAL_MAX_MS) ? v : def;
  };
  rtCfg.sleepStartHour = clampHour(slpS, SLEEP_START_HOUR_DEFAULT);
  rtCfg.sleepEndHour = clampHour(slpE, SLEEP_END_HOUR_DEFAULT);
  rtCfg.dimStartHour = clampHour(dimS, OLED_DIM_START_HOUR_DEFAULT);
  rtCfg.dimEndHour = clampHour(dimE, OLED_DIM_END_HOUR_DEFAULT);
  rtCfg.syncIntervalMs = clampInterval(syncIv, SYNC_INTERVAL);
  rtCfg.otaCheckIntervalMs = clampInterval(otaIv, OTA_CHECK_INTERVAL);
}
RuntimeConfig getRuntimeConfigSnapshot()
{
  RuntimeConfig snap;
  if (xConfigMutex && xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(200)) == pdTRUE)
  {
    snap = rtCfg;
    xSemaphoreGive(xConfigMutex);
  }
  else
  {
    snap = rtCfg;
  }
  return snap;
}
static void persistRuntimeConfigToNvs(const RuntimeConfig &cfg)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NS_CONFIG, false);
  prefs.putInt(NVS_KEY_CFG_SLP_S, cfg.sleepStartHour);
  prefs.putInt(NVS_KEY_CFG_SLP_E, cfg.sleepEndHour);
  prefs.putInt(NVS_KEY_CFG_DIM_S, cfg.dimStartHour);
  prefs.putInt(NVS_KEY_CFG_DIM_E, cfg.dimEndHour);
  prefs.putULong(NVS_KEY_CFG_SYNCIV, cfg.syncIntervalMs);
  prefs.putULong(NVS_KEY_CFG_OTAIV, cfg.otaCheckIntervalMs);
  prefs.end();
  unlockNvs();
}
static bool saveCredential(const char *key, const char *val)
{
  return saveEncryptedNvs(NVS_NS_CONFIG, key, val);
}
static void markProvisioned()
{
  prefs.begin(NVS_NS_CONFIG, false);
  prefs.putBool(NVS_KEY_PROVISIONED, true);
  prefs.end();
  isProvisioned = true;
}
static bool checkProvisioned()
{
  prefs.begin(NVS_NS_CONFIG, true);
  bool v = prefs.getBool(NVS_KEY_PROVISIONED, false);
  prefs.end();
  return v;
}

#define TLS_VERIFY_CERT 1
#if TLS_VERIFY_CERT
static const char TLS_ROOT_CA[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIICCTCCAY6gAwIBAgINAgPlwGjvYxqccpBQUjAKBggqhkjOPQQDAzBHMQswCQYD
VQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEUMBIG
A1UEAxMLR1RTIFJvb3QgUjQwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAwMDAw
WjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2Vz
IExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjQwdjAQBgcqhkjOPQIBBgUrgQQAIgNi
AATzdHOnaItgrkO4NcWBMHtLSZ37wWHO5t5GvWvVYRg1rkDdc/eJkTBa6zzuhXyi
QHY7qca4R9gq55KRanPpsXI5nymfopjTX15YhmUPoYRlBtHci8nHc8iMai/lxKvR
HYqjQjBAMA4GA1UdDwEB/wQEAwIBhjAPBgNVHRMBAf8EBTADAQH/MB0GA1UdDgQW
BBSATNbrdP9JNqPV2Py1PsVq8JQdjDAKBggqhkjOPQQDAwNpADBmAjEA6ED/g94D
9J+uHXqnLrmvT/aDHQ4thQEd0dlq7A/Cr8deVl5c1RxYIigL9zC2L7F8AjEA8GE8
p/SgguMh1YQdc4acLa/KNJvxn7kjNuK8YAOdgLOaVsjh4rsUecrNIdSUtUlD
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";
#endif

// Setiap request membuat WiFiClientSecure sendiri (objeknya kecil, sesi TLS ada di heap),
// sehingga taskRfid dan taskSync tidak saling merusak sesi TLS.
static void applyTls(WiFiClientSecure &c)
{
#if TLS_VERIFY_CERT
  c.setCACert(TLS_ROOT_CA);
#else
  c.setInsecure();
#endif
  c.setHandshakeTimeout(10);
}
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
static bool wdtApply(uint32_t ms)
{
  const esp_task_wdt_config_t cfg = {
      .timeout_ms = ms,
      .idle_core_mask = 0,
      .trigger_panic = true};
  esp_err_t e = esp_task_wdt_reconfigure(&cfg); // task tetap terdaftar: tidak ada jendela tanpa pengawasan
  if (e != ESP_OK)
    LOG_E("[WDT] reconfigure %lu ms gagal err=%d\n", (unsigned long)ms, (int)e);
  else
    LOG_I("[WDT] timeout %lu ms\n", (unsigned long)ms);
  return e == ESP_OK;
}
void extendWdtForSync()
{
  portENTER_CRITICAL(&wdtMux);
  bool already = wdtExtended;
  wdtExtended = true;
  portEXIT_CRITICAL(&wdtMux);
  if (already)
    return;
  if (!wdtApply(WDT_SYNC_TIMEOUT_MS))
    wdtExtended = false;
}
void restoreWdtNormal()
{
  portENTER_CRITICAL(&wdtMux);
  bool was = wdtExtended;
  wdtExtended = false;
  portEXIT_CRITICAL(&wdtMux);
  if (!was)
    return;
  if (!wdtApply(WDT_NORMAL_TIMEOUT_MS))
    wdtExtended = true;
}
void turnOffOLED()
{
  if (!oledIsOn)
    return;
  if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    display.clearDisplay();
    display.display();
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    oledIsOn = false;
    xSemaphoreGive(xDisplayMutex);
  }
}
void turnOnOLED()
{
  if (oledIsOn)
    return;
  if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
    oledIsOn = true;
    memset(previousDisplay.time, 0xFF, sizeof(previousDisplay.time));
    previousDisplay.pendingRecords = -1;
    previousDisplay.wifiSignal = -1;
    previousDisplay.isOnline = !currentDisplay.isOnline;
    xSemaphoreGive(xDisplayMutex);
  }
}
void showOLED(const __FlashStringHelper *l1, const char *l2)
{
  if (!oledIsOn)
    return;
  if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(200)) != pdTRUE)
    return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(l1, 0, 0, &x, &y, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 10);
  display.println(l1);
  display.getTextBounds(l2, 0, 0, &x, &y, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 30);
  display.println(l2);
  display.display();
  xSemaphoreGive(xDisplayMutex);
}
void showOLED(const __FlashStringHelper *l1, const __FlashStringHelper *l2)
{
  char buf[32];
  strncpy_P(buf, (const char *)l2, 31);
  buf[31] = '\0';
  showOLED(l1, buf);
}
void showProgress(const __FlashStringHelper *msg, int ms)
{
  if (!oledIsOn)
    return;
  if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(200)) != pdTRUE)
    return;
  const int step = 8, total = 80;
  int perStep = ms / (total / step);
  int startX = (SCREEN_WIDTH - total) / 2;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(msg, 0, 0, &x, &y, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 20);
  display.println(msg);
  display.display();
  xSemaphoreGive(xDisplayMutex);
  for (int i = 0; i <= total; i += step)
  {
    esp_task_wdt_reset();
    if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(50)) == pdTRUE)
    {
      display.fillRect(startX, 40, i, 4, WHITE);
      display.display();
      xSemaphoreGive(xDisplayMutex);
    }
    vTaskDelay(pdMS_TO_TICKS(perStep));
  }
  esp_task_wdt_reset();
  vTaskDelay(pdMS_TO_TICKS(300));
}
void playToneSuccess()
{
  for (int i = 0; i < 2; i++)
  {
    tone(PIN_BUZZER, 3000, 100);
    delay(150);
  }
  noTone(PIN_BUZZER);
}
void playToneError()
{
  for (int i = 0; i < 3; i++)
  {
    tone(PIN_BUZZER, 3000, 150);
    delay(200);
  }
  noTone(PIN_BUZZER);
}
void playToneNotify()
{
  tone(PIN_BUZZER, 3000, 100);
  delay(120);
  noTone(PIN_BUZZER);
}
void playStartupMelody()
{
  static const int mel[] = {2500, 3000, 2500, 3000};
  for (int i = 0; i < 4; i++)
  {
    tone(PIN_BUZZER, mel[i], 100);
    delay(150);
  }
  noTone(PIN_BUZZER);
}
void nvsSaveLastTime(time_t t)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putULong(NVS_KEY_LAST_TIME, (unsigned long)t);
  prefs.end();
  unlockNvs();
}

time_t nvsLoadLastTime()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  unsigned long t = prefs.getULong(NVS_KEY_LAST_TIME, 0);
  prefs.end();
  unlockNvs();
  return (time_t)t;
}

void nvsBumpScanCount()
{
  struct tm ti;
  if (!getTimeWithFallback(&ti))
    return;
  char today[9];
  snprintf(today, sizeof(today), "%04u%02u%02u",
           (unsigned)(ti.tm_year + 1900) % 10000u,
           (unsigned)(ti.tm_mon + 1) % 100u,
           (unsigned)ti.tm_mday % 100u);
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  char stored[9] = "";
  prefs.getString(NVS_KEY_SCAN_DATE, stored, sizeof(stored));
  int cnt = strcmp(stored, today) == 0 ? prefs.getInt(NVS_KEY_SCAN_COUNT, 0) : 0;
  prefs.putString(NVS_KEY_SCAN_DATE, today);
  prefs.putInt(NVS_KEY_SCAN_COUNT, cnt + 1);
  prefs.end();
  unlockNvs();
}

int nvsGetScanCount()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int c = prefs.getInt(NVS_KEY_SCAN_COUNT, 0);
  prefs.end();
  unlockNvs();
  return c;
}

int nvsGetBootRetry()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int c = prefs.getInt(NVS_KEY_BOOT_RETRY, 0);
  prefs.end();
  unlockNvs();
  return c;
}

// Menulis hanya jika nilai berubah, supaya flash tidak aus saat boot normal (nilai tetap 0).
void nvsSetBootRetry(int v)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  if (prefs.getInt(NVS_KEY_BOOT_RETRY, 0) != v)
    prefs.putInt(NVS_KEY_BOOT_RETRY, v);
  prefs.end();
  unlockNvs();
}
static int nvsGetI(const char *key)
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int v = prefs.getInt(key, 0);
  prefs.end();
  unlockNvs();
  return v;
}
static void nvsSetIChanged(const char *key, int v)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  if (prefs.getInt(key, 0) != v)
    prefs.putInt(key, v);
  prefs.end();
  unlockNvs();
}

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
  return h ? (uint32_t)uxTaskGetStackHighWaterMark(h) : 0; // ASUMSI: byte di ESP-IDF
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

int nvsGetCount()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  int c = prefs.getInt(NVS_KEY_COUNT, 0);
  prefs.end();
  unlockNvs();
  return c;
}

void nvsSetCount(int count)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putInt(NVS_KEY_COUNT, count);
  prefs.end();
  unlockNvs();
}

bool nvsLoadRecord(int idx, OfflineRecord &rec)
{
  char key[16];
  snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, idx);
  if (!lockNvs())
    return false;
  prefs.begin(NVS_NAMESPACE, true);
  size_t len = prefs.getBytesLength(key);
  if (len != sizeof(OfflineRecord))
  {
    prefs.end();
    unlockNvs();
    return false;
  }
  prefs.getBytes(key, &rec, sizeof(OfflineRecord));
  prefs.end();
  unlockNvs();
  return true;
}

bool nvsSaveRecord(int idx, const OfflineRecord &rec)
{
  char key[16];
  snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, idx);
  if (!lockNvs())
    return false;
  prefs.begin(NVS_NAMESPACE, false);
  size_t w = prefs.putBytes(key, &rec, sizeof(OfflineRecord));
  prefs.end();
  unlockNvs();
  return w == sizeof(OfflineRecord);
}

void nvsDeleteRecord(int idx)
{
  char key[16];
  snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, idx);
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.remove(key);
  prefs.end();
  unlockNvs();
}

bool nvsIsDuplicate(const char *rfid, unsigned long t)
{
  int cnt = nvsGetCount();
  for (int i = 0; i < cnt; i++)
  {
    OfflineRecord rec;
    if (!nvsLoadRecord(i, rec))
      continue;
    if (strcmp(rec.rfid, rfid) == 0 && t >= rec.unixTime && (t - rec.unixTime) < MIN_REPEAT_INTERVAL)
      return true;
  }
  return false;
}

bool nvsSaveToBuffer(const char *rfid, const char *ts, unsigned long t)
{
  if (!lockNvs(pdMS_TO_TICKS(2000)))
    return false;

  prefs.begin(NVS_NAMESPACE, false);
  int cnt = prefs.getInt(NVS_KEY_COUNT, 0);
  bool ok = false;

  if (cnt < NVS_MAX_RECORDS)
  {
    OfflineRecord rec = {};
    strncpy(rec.rfid, rfid, sizeof(rec.rfid) - 1);
    strncpy(rec.timestamp, ts, sizeof(rec.timestamp) - 1);
    strncpy(rec.deviceId, deviceId, sizeof(rec.deviceId) - 1);
    rec.unixTime = t;

    char key[16];
    snprintf(key, sizeof(key), "%s%d", NVS_KEY_PREFIX, cnt);
    if (prefs.putBytes(key, &rec, sizeof(OfflineRecord)) == sizeof(OfflineRecord))
    {
      prefs.putInt(NVS_KEY_COUNT, cnt + 1);
      ok = true;
    }
  }

  prefs.end();
  unlockNvs();
  return ok;
}

unsigned long nvsGetRfidDbVer()
{
  if (!lockNvs())
    return 0;
  prefs.begin(NVS_NAMESPACE, true);
  unsigned long v = prefs.getULong(NVS_KEY_RFID_VER, 0);
  prefs.end();
  unlockNvs();
  return v;
}

void nvsSetRfidDbVer(unsigned long ver)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putULong(NVS_KEY_RFID_VER, ver);
  prefs.end();
  unlockNvs();
}

void nvsSaveLastScan(const char *rfid, unsigned long t)
{
  if (!lockNvs())
    return;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString(NVS_KEY_LAST_RFID, rfid);
  prefs.putULong(NVS_KEY_LAST_SCAN_T, t);
  prefs.end();
  unlockNvs();
}

bool nvsIsRecentScan(const char *rfid, unsigned long t)
{
  char stored[11] = "";
  if (!lockNvs())
    return false;
  prefs.begin(NVS_NAMESPACE, true);
  prefs.getString(NVS_KEY_LAST_RFID, stored, sizeof(stored));
  unsigned long storedT = prefs.getULong(NVS_KEY_LAST_SCAN_T, 0);
  prefs.end();
  unlockNvs();
  if (stored[0] == '\0' || storedT == 0)
    return false;
  if (strcmp(stored, rfid) != 0)
    return false;
  return (t >= storedT && (t - storedT) < MIN_REPEAT_INTERVAL);
}

// Versi mentah: pemanggil HARUS sudah memegang xCacheMutex (atau berada di jalur tanpa task lain).
static void clearRfidCacheRaw()
{
  memset(rfidCacheFlat, 0, sizeof(rfidCacheFlat));
  rfidCacheCount = 0;
  rfidCacheLoaded = false;
  rfidDbValid = false;
}

// Versi aman untuk dipanggil dari mana saja.
void clearRfidCache()
{
  if (!lockCache(pdMS_TO_TICKS(3000)))
    return;
  clearRfidCacheRaw();
  unlockCache();
}

static bool loadRfidCacheImpl()
{
  clearRfidCacheRaw();
  rfidCacheDiscarded = 0;
  // Pemulihan: listrik mati di tengah penggantian file meninggalkan .bak tanpa DB aktif.
  if (!sd.exists(RFID_DB_FILE) && sd.exists(RFID_DB_BAK))
    sd.rename(RFID_DB_BAK, RFID_DB_FILE);
  if (!sd.exists(RFID_DB_FILE))
  {
    return false;
  }
  FsFile f;
  if (!f.open(RFID_DB_FILE, O_RDONLY))
  {
    return false;
  }
  char line[12];
  int idx = 0;
  long discarded = 0;
  while (f.fgets(line, sizeof(line)) > 0 && idx < RFID_CACHE_MAX)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    if (len != 10)
      continue;
    bool ok = true;
    for (int j = 0; j < 10 && ok; j++)
      ok = isdigit((unsigned char)line[j]);
    if (!ok)
      continue;
    memcpy(rfidCacheFlat[idx], line, 10);
    rfidCacheFlat[idx][10] = '\0';
    idx++;
  }
  if (idx >= RFID_CACHE_MAX)
  {
    while (f.fgets(line, sizeof(line)) > 0)
    {
      esp_task_wdt_reset();
      taskYIELD();
      int len = strlen(line);
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
      if (len == 10)
        discarded++;
    }
  }
  f.close();
  rfidCacheCount = idx;
  rfidCacheDiscarded = (int)discarded;
  // File ada dan terbaca = DB sah, walau kosong (semua kartu nonaktif).
  rfidCacheLoaded = true;
  rfidDbValid = true;
  return true;
}

// Dipanggil saat xSdMutex dipegang. Urutan kunci: SD lalu cache.
bool loadRfidCacheFromFileLocked()
{
  if (!lockCache(pdMS_TO_TICKS(3000)))
    return false;
  bool ok = loadRfidCacheImpl();
  unlockCache();
  return ok;
}

bool loadRfidCacheFromFile()
{
  if (!sdCardAvailable)
    return false;
  if (!acquireSD())
    return false;
  selectSD();
  bool ok = loadRfidCacheImpl();
  deselectSD();
  releaseSD();
  return ok;
}

// Dipanggil dari taskRfid. Tidak mengambil xSdMutex.
// Timeout lebih panjang dari durasi reload cache (beberapa ratus ms sampai ±2 detik).
static RfidLookup rfidLookupCache(const char *rfid)
{
  if (!lockCache(pdMS_TO_TICKS(2500)))
    return RFID_BUSY;

  RfidLookup result = RFID_NOT_FOUND;
  if (!rfidDbValid || !rfidCacheLoaded)
  {
    result = RFID_NO_DB;
  }
  else
  {
    for (int i = 0; i < rfidCacheCount; i++)
    {
      if (strcmp(rfidCacheFlat[i], rfid) == 0)
      {
        result = RFID_FOUND;
        break;
      }
    }
  }
  unlockCache();
  return result;
}

void loadAdminRfidList()
{
  char tmp[5][11];
  int cnt = 0;
  if (!sdCardAvailable)
    return; // daftar lama dipertahankan
  if (!acquireSD())
    return; // gagal ambil mutex: jangan kosongkan daftar lama
  selectSD();
  FsFile f;
  if (sd.exists(ADMIN_RFID_FILE) && f.open(ADMIN_RFID_FILE, O_RDONLY))
  {
    char line[12];
    while (cnt < 5 && f.fgets(line, sizeof(line)) > 0)
    {
      int len = strlen(line);
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
      if (len == 10)
      {
        memcpy(tmp[cnt], line, 11);
        cnt++;
      }
    }
    f.close();
  }
  deselectSD();
  releaseSD();
  portENTER_CRITICAL(&wdtMux); // tukar sangat singkat; pembaca tidak pernah melihat daftar kosong sementara
  memcpy(adminRfidList, tmp, sizeof(adminRfidList));
  adminRfidCount = cnt;
  portEXIT_CRITICAL(&wdtMux);
}
bool isAdminRfid(const char *rfid)
{
  bool found = false;
  portENTER_CRITICAL(&wdtMux);
  for (int i = 0; i < adminRfidCount; i++)
    if (strcmp(adminRfidList[i], rfid) == 0)
    {
      found = true;
      break;
    }
  portEXIT_CRITICAL(&wdtMux);
  return found;
}
void handleAdminScan(const char *rfid)
{
  (void)rfid;
  showOLED(F("ADMIN MODE"), "SYNC + STATUS");
  playToneNotify();
  char buf[24];
  snprintf(buf, sizeof(buf), "Q:%d SC:%d", cachedPendingRecords + nvsGetCount(), nvsGetScanCount());
  showOLED(F("STATUS"), buf);
  delay(2000);
  if (isWifiConnected())
  {
    pendingCacheDirty = true;
    forceSyncRequested = true; // taskSync yang menjalankan; syncState tidak disentuh dari sini
  }
}

// ---- RTC eksternal opsional (DS1307 / DS3231). Menyimpan waktu UTC. ----
static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

// Bus I2C dibagi dengan OLED, jadi dilindungi mutex yang sama.
static bool rtcBusLock()
{
  if (!xDisplayMutex || xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(300)) != pdTRUE)
    return false;
  Wire.setClock(100000); // DS1307 maksimal 100 kHz
  return true;
}
static void rtcBusUnlock()
{
  if (xDisplayMutex)
    xSemaphoreGive(xDisplayMutex);
}

static bool rtcDetect()
{
  if (!rtcBusLock())
    return false;
  Wire.beginTransmission(RTC_I2C_ADDR);
  bool ok = (Wire.endTransmission() == 0);
  rtcBusUnlock();
  return ok;
}

// false jika RTC tidak ada, osilator berhenti (baterai habis / belum pernah di-set), atau data tidak masuk akal.
static bool rtcReadEpoch(time_t *out)
{
  if (!rtcPresent || !rtcBusLock())
    return false;
  uint8_t r[7];
  bool ok = false;
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write((uint8_t)0x00);
  if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)RTC_I2C_ADDR, (uint8_t)7) == 7)
  {
    for (int i = 0; i < 7; i++)
      r[i] = (uint8_t)Wire.read();
    ok = true;
  }
  rtcBusUnlock();
  if (!ok)
    return false;
  if (r[0] & 0x80) // bit CH: osilator berhenti
    return false;
  if (r[2] & 0x40) // mode 12 jam: bukan format yang kita tulis
    return false;
  int sec = bcd2bin(r[0] & 0x7F);
  int mn = bcd2bin(r[1] & 0x7F);
  int hr = bcd2bin(r[2] & 0x3F);
  int day = bcd2bin(r[4] & 0x3F);
  int mon = bcd2bin(r[5] & 0x1F);
  int year = 2000 + bcd2bin(r[6]);
  if (sec > 59 || mn > 59 || hr > 23 || day < 1 || day > 31 || mon < 1 || mon > 12 || year < RTC_MIN_VALID_YEAR)
    return false;
  *out = (time_t)(daysFromCivil(year, (unsigned)mon, (unsigned)day) * 86400LL + hr * 3600L + mn * 60L + sec);
  return true;
}

static bool rtcWriteEpoch(time_t t)
{
  if (!rtcPresent)
    return false;
  struct tm g;
  gmtime_r(&t, &g);
  if (!rtcBusLock())
    return false;
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write((uint8_t)0x00);
  Wire.write((uint8_t)(bin2bcd((uint8_t)g.tm_sec) & 0x7F)); // CH = 0: osilator jalan
  Wire.write(bin2bcd((uint8_t)g.tm_min));
  Wire.write(bin2bcd((uint8_t)g.tm_hour)); // mode 24 jam
  Wire.write(bin2bcd((uint8_t)(g.tm_wday + 1)));
  Wire.write(bin2bcd((uint8_t)g.tm_mday));
  Wire.write(bin2bcd((uint8_t)(g.tm_mon + 1)));
  Wire.write(bin2bcd((uint8_t)(g.tm_year % 100)));
  bool ok = (Wire.endTransmission() == 0);
  rtcBusUnlock();
  return ok;
}

// Dipanggil sekali di setup(). RTC yang valid menjadi sumber waktu awal,
// menggantikan estimasi dari NVS yang tidak menghitung lama listrik mati.
static void rtcInitAndApply()
{
  rtcPresent = rtcDetect();
  if (!rtcPresent)
  {
    LOG_I("[RTC] tidak terdeteksi (opsional)\n");
    return;
  }
  time_t e;
  if (!rtcReadEpoch(&e))
  {
    LOG_W("[RTC] terdeteksi, waktu belum valid; menunggu NTP\n");
    showOLED(F("RTC"), "BELUM DI-SET");
    delay(800);
    return;
  }
  struct timeval tv;
  tv.tv_sec = e;
  tv.tv_usec = 0;
  settimeofday(&tv, nullptr);
  lastValidTime = e;
  timeWasSynced = true;
  bootTime = millis();
  bootTimeSet = true;
  struct tm ti;
  localtime_r(&e, &ti);
  char buf[20];
  snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
  LOG_I("[RTC] dipakai sebagai waktu awal: %s\n", buf);
  showOLED(F("RTC OK"), buf);
  delay(800);
}

// Dipanggil setelah NTP berhasil. Menulis hanya jika RTC belum valid atau selisih > 1 detik.
static void rtcSyncFromSystem()
{
  if (!rtcPresent)
    return;
  time_t now = time(nullptr);
  if (now < MIN_VALID_EPOCH)
    return;
  time_t cur;
  if (rtcReadEpoch(&cur))
  {
    time_t diff = (cur > now) ? (cur - now) : (now - cur);
    if (diff <= 1)
      return;
  }
  if (rtcWriteEpoch(now))
    LOG_I("[RTC] diperbarui dari NTP\n");
}

// Satu sumber epoch untuk SEMUA tempat (simpan, duplikat, scan count, filter umur).
// Tidak memblokir (tanpa getLocalTime).
bool getEpochWithFallback(time_t *out)
{
  time_t now = time(nullptr);
  if (now >= MIN_VALID_EPOCH) // jam sistem valid
  {
    *out = now;
    return true;
  }
  if (!timeWasSynced || lastValidTime == 0 || !bootTimeSet)
    return false;
  // Selisih unsigned aman terhadap wrap millis, asal bootTime berasal dari boot ini (lihat setup()).
  unsigned long elapsed = (millis() - bootTime) / 1000UL;
  if (elapsed > MAX_TIME_ESTIMATE_AGE)
    return false;
  *out = lastValidTime + (time_t)elapsed;
  return true;
}

bool getTimeWithFallback(struct tm *ti)
{
  time_t e;
  if (!getEpochWithFallback(&e))
    return false;
  localtime_r(&e, ti);
  return true;
}

// Record dianggap kedaluwarsa hanya jika jam valid.
static bool isRecordExpired(unsigned long recT, time_t nowEpoch, bool nowValid)
{
  if (!nowValid)
    return false;
  if (recT > (unsigned long)nowEpoch)
    return false;
  return ((unsigned long)nowEpoch - recT) > MAX_OFFLINE_AGE;
}

bool isTimeValid()
{
  struct tm ti;
  return getTimeWithFallback(&ti);
}

// Jam tepercaya = jam sistem sudah diset (NTP / RTC / jam RTC internal yang bertahan saat reset),
// atau estimasi yang bukan berasal dari NVS-saja (mis. bangun dari deep sleep, durasi diketahui).
// Estimasi NVS setelah cold boot tidak tepercaya karena lama mati tidak terhitung.
bool isTimeTrusted()
{
  if (time(nullptr) >= MIN_VALID_EPOCH)
    return true;
  return !timeFromNvsOnly && isTimeValid();
}

// Segarkan jangkar di RTC memory HANYA dari jam sistem sungguhan, tidak pernah dari estimasi,
// supaya batas MAX_TIME_ESTIMATE_AGE tidak diperpanjang oleh estimasi itu sendiri.
// Pengecualian kedua (setelah isTimeTrusted) terhadap aturan "semua epoch lewat getEpochWithFallback".
static void anchorTimeFromSystem()
{
  static unsigned long last = 0;
  unsigned long n = millis();
  if (last != 0 && n - last < 60000UL)
    return;
  last = n ? n : 1;
  time_t t = time(nullptr);
  if (t < MIN_VALID_EPOCH)
    return;
  lastValidTime = t;
  timeWasSynced = true;
  bootTime = n;
  bootTimeSet = true;
}

void getFormattedTimestamp(char *buf, size_t sz)
{
  struct tm ti;
  if (!getTimeWithFallback(&ti))
  {
    buf[0] = '\0';
    return;
  }
  strftime(buf, sz, "%Y-%m-%d %H:%M:%S", &ti);
}

// Naik setiap SNTP benar-benar mengatur jam. Tidak terpengaruh jam sistem yang sudah valid.
static volatile uint32_t sntpSyncCount = 0;
static void onSntpSync(struct timeval *tv)
{
  (void)tv;
  sntpSyncCount = sntpSyncCount + 1;
}

bool syncTimeWithFallback()
{
  if (isSignalCritical())
  {
    return false;
  }
  static bool cbSet = false;
  if (!cbSet)
  {
    sntp_set_time_sync_notification_cb(onSntpSync);
    cbSet = true;
  }
  const char *servers[] = {NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3};
  for (int i = 0; i < 3; i++)
  {
    char srv[32];
    strcpy_P(srv, servers[i]);
    uint32_t before = sntpSyncCount;
    configTzTime(TZ_POSIX, srv); // TZ tetap WIB-7, tidak ditimpa "UTC-7"
    unsigned long t0 = millis();
    while (millis() - t0 < NTP_SERVER_TIMEOUT_MS)
    {
      esp_task_wdt_reset();
      if (sntpSyncCount != before)
      {
        time_t now = time(nullptr);
        if (now < MIN_VALID_EPOCH)
          break; // callback terpanggil tetapi jam tidak masuk akal: server berikutnya
        lastValidTime = now;
        nvsSaveLastTime(lastValidTime);
        timeWasSynced = true;
        bootTime = millis();
        bootTimeSet = true;
        bootTimeSyncFailed = false;
        rtcSyncFromSystem();
        struct tm ti;
        localtime_r(&now, &ti);
        char buf[6];
        snprintf(buf, sizeof(buf), "%02d:%02d", ti.tm_hour, ti.tm_min);
        LOG_I("[TIME] SNTP OK via %s epoch=%lld %02d:%02d\n", srv, (long long)now, ti.tm_hour, ti.tm_min);
        showOLED(F("WAKTU TERSYNC"), buf);
        delay(1000);
        return true;
      }
      delay(100);
    }
    LOG_I("[TIME] SNTP timeout via %s\n", srv);
  }
  return false;
}
void periodicTimeSync()
{
  // Selama jam belum tepercaya, coba lebih sering agar scan tidak lama tertolak.
  unsigned long interval = isTimeTrusted() ? TIME_SYNC_INTERVAL : UNTRUSTED_TIME_RETRY_INTERVAL;
  if (millis() - timers.lastTimeSync < interval)
    return;
  timers.lastTimeSync = millis();
  if (!isSignalCritical())
    syncTimeWithFallback();
}
void checkOLEDSchedule()
{
  if (millis() - timers.lastOLEDScheduleCheck < OLED_SCHEDULE_CHECK_INTERVAL)
    return;
  timers.lastOLEDScheduleCheck = millis();
  struct tm ti;
  if (!getTimeWithFallback(&ti))
    return;
  RuntimeConfig cfg = getRuntimeConfigSnapshot();
  int h = ti.tm_hour;
  if (isInWindow(h, cfg.dimStartHour, cfg.dimEndHour))
    turnOffOLED();
  else
    turnOnOLED();
}
// Panggil saat xSdMutex dipegang dan f dibuka O_RDWR|O_APPEND.
// size==0: tulis header (file ada tapi kosong akibat daya putus setelah O_CREAT).
// byte terakhir bukan '\n': tutup baris parsial agar record berikutnya tidak menempel.
static bool prepareAppendLocked(FsFile &f, const char *header)
{
  uint64_t sz = f.size();
  if (sz == 0)
  {
    if (header && f.println(header) == 0)
      return false;
    return true;
  }
  if (!f.seekSet(sz - 1))
    return false;
  int last = f.read();
  if (last < 0)
    return false;
  if (last != '\n')
  {
    if (f.write((uint8_t)'\n') != 1) // O_APPEND: selalu menulis di akhir file
      return false;
  }
  return true;
}
void appendFailedLogToSD(const char *rfid, const char *ts, const char *reason)
{
  if (!sdCardAvailable)
    return;
  if (!acquireSD(pdMS_TO_TICKS(2000)))
    return;
  selectSD();

  int lineCount = 0;
  if (sd.exists("/failed_log.csv"))
  {
    FsFile countFile;
    if (countFile.open("/failed_log.csv", O_RDONLY))
    {
      char ln[128];
      while (countFile.fgets(ln, sizeof(ln)) > 0)
      {
        size_t l = strlen(ln);
        if (l > 0 && ln[l - 1] == '\n')
          lineCount++; // hanya hitung baris yang sudah lengkap
      }
      countFile.close();
    }
  }

  if (lineCount >= FAILED_LOG_MAX_LINES)
  {
    if (sd.exists("/failed_log.old"))
      sd.remove("/failed_log.old");
    sd.rename("/failed_log.csv", "/failed_log.old");
    lineCount = 0;
  }
  if (lineCount < FAILED_LOG_MAX_LINES)
  {
    FsFile logFile;
    if (logFile.open("/failed_log.csv", O_RDWR | O_CREAT | O_APPEND))
    {
      if (prepareAppendLocked(logFile, "rfid,timestamp,reason"))
      {
        logFile.print(rfid);
        logFile.print(',');
        logFile.print(ts);
        logFile.print(',');
        logFile.println(reason);
        logFile.sync();
      }
      logFile.close();
    }
    else
    {
      showOLED(F("ERROR"), "GAGAL MENYIMPAN LOG");
    }
  }

  deselectSD();
  releaseSD();
}
void getQueueFileName(int idx, char *buf, size_t sz)
{
  snprintf(buf, sz, "/queue_%d.csv", idx);
}

// Cari file antrean dengan indeks terkecil >= fromIdx lewat iterasi direktori (tahan celah indeks).
// Panggil saat mutex SD dipegang dan selectSD() aktif.
static bool findNextQueueFileLocked(int fromIdx, int *outIdx)
{
  FsFile root, entry;
  if (!root.open("/"))
    return false;
  int best = -1;
  char name[40];
  while (entry.openNext(&root, O_RDONLY))
  {
    esp_task_wdt_reset();
    taskYIELD();
    bool isFile = entry.isFile();
    if (isFile)
      entry.getName(name, sizeof(name));
    entry.close();
    if (!isFile)
      continue;
    int idx;
    if (parseQueueIndex(name, &idx) && idx >= fromIdx && (best < 0 || idx < best))
      best = idx;
  }
  root.close();
  if (best < 0)
    return false;
  *outIdx = best;
  return true;
}
int countRecordsInFileLocked(const char *filename)
{
  if (!file.open(filename, O_RDONLY))
    return 0;
  int cnt = 0;
  char line[128];
  if (file.available())
    file.fgets(line, sizeof(line));
  while (file.fgets(line, sizeof(line)) > 0)
  {
    esp_task_wdt_reset();
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    if (len > 10)
      cnt++;
  }
  file.close();
  return cnt;
}
void saveMetadataLocked()
{
  selectSD();
  if (file.open(METADATA_FILE, O_WRONLY | O_CREAT | O_TRUNC))
  {
    file.print(cachedPendingRecords);
    file.print(',');
    file.println(currentQueueFile);
    file.sync();
    file.close();
  }
  deselectSD();
}
void loadMetadataLocked()
{
  selectSD();
  if (!sd.exists(METADATA_FILE))
  {
    deselectSD();
    return;
  }
  if (!file.open(METADATA_FILE, O_RDONLY))
  {
    deselectSD();
    return;
  }
  char line[32];
  if (file.fgets(line, sizeof(line)) > 0)
  {
    char *comma = strchr(line, ',');
    if (comma)
    {
      *comma = '\0';
      cachedPendingRecords = atoi(line);
      currentQueueFile = atoi(comma + 1);
      rtcQueueFileValid = true;
      pendingCacheDirty = false;
    }
  }
  file.close();
  deselectSD();
}
bool reinitSDCard()
{
  if (file.isOpen())
    file.close();
  sd.end();
  delay(100);
  selectSD();
  delay(10);
  bool ok = sd.begin(PIN_SD_CS, SD_SCK_MHZ(10));
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
  // Baca sektor 0 = akses nyata ke kartu (fatType() hanya cache hasil mount).
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
    // ulangi cek lebih cepat (5 detik) sebelum memutuskan SD hilang
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
bool fileHasValidRecords(const char *fn)
{
  if (!file.open(fn, O_RDONLY))
    return false;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  char line[144];
  if (file.available())
    file.fgets(line, sizeof(line));
  bool found = false;
  while (file.available())
  {
    esp_task_wdt_reset();
    if (file.fgets(line, sizeof(line)) <= 0)
      break;
    int len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
      line[--len] = '\0';
    char *c1 = strchr(line, ',');
    char *c2 = c1 ? strchr(c1 + 1, ',') : nullptr;
    char *c3 = c2 ? strchr(c2 + 1, ',') : nullptr;
    if (!c3)
      continue;
    unsigned long recT = strtoul(c3 + 1, nullptr, 10);
    if (recT > 0 && !isRecordExpired(recT, now, nowValid))
    {
      found = true;
      break;
    }
  }
  file.close();
  return found;
}
static int classifyQueueLine(char *line, OfflineRecord *out, time_t now, bool nowValid);

int countValidRecordsInFileLocked(const char *fn)
{
  if (!file.open(fn, O_RDONLY))
    return 0;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  int cnt = 0;
  char line[144];
  if (file.available())
    file.fgets(line, sizeof(line)); // header
  while (file.available())
  {
    esp_task_wdt_reset();
    taskYIELD();
    if (file.fgets(line, sizeof(line)) <= 0)
      break;
    if (classifyQueueLine(line, nullptr, now, nowValid) == LINE_OK)
      cnt++;
  }
  file.close();
  return cnt;
}

// Return -1 jika gagal (mutex), supaya cache tidak salah di-set 0.
// Mutex SD diambil dan dilepas per file, sehingga saveToQueue() dan pembacaan RFID
// bisa menyela di antara dua file. Memakai findNextQueueFileLocked() agar tahan perubahan
// direktori di sela pemindaian (file dihapus / dibuat oleh task lain).
int countAllOfflineRecords()
{
  if (!sdCardAvailable)
    return 0;
  int total = 0;
  int files = 0;
  int from = 0;
  char fn[24];
  for (;;)
  {
    esp_task_wdt_reset();
    vTaskDelay(1); // melepas CPU ke prioritas lebih rendah (loop), bukan taskYIELD()
    if (!acquireSD(pdMS_TO_TICKS(2000)))
      return -1;
    selectSD();
    int idx;
    if (!findNextQueueFileLocked(from, &idx))
    {
      deselectSD();
      releaseSD();
      break;
    }
    getQueueFileName(idx, fn, sizeof(fn));
    int v = countValidRecordsInFileLocked(fn);
    deselectSD();
    releaseSD();
    if (v > 0)
    {
      files++;
      total += v;
    }
    from = idx + 1;
  }
  cachedQueueFileCount = files;
  return total;
}

void refreshPendingCache()
{
  if (!pendingCacheDirty)
    return;
  pendingCacheDirty = false; // dibersihkan SEBELUM menghitung
  uint32_t before = queueMutations;
  int n = countAllOfflineRecords();
  if (n < 0 || queueMutations != before)
  {
    pendingCacheDirty = true; // gagal atau hasil basi
    return;
  }
  cachedPendingRecords = n;
  if (!acquireSD(pdMS_TO_TICKS(1000)))
    return;
  saveMetadataLocked();
  releaseSD();
}

bool isDuplicateLocked(const char *rfid, unsigned long t)
{
  char fn[20];
  for (int offset = 0; offset < MAX_DUPLICATE_CHECK_FILES; offset++)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int idx = (currentQueueFile - offset + MAX_QUEUE_FILES) % MAX_QUEUE_FILES;
    getQueueFileName(idx, fn, sizeof(fn));
    if (!sd.exists(fn))
      continue;
    if (!file.open(fn, O_RDONLY))
      continue;
    char line[128];
    if (file.available())
      file.fgets(line, sizeof(line));
    int read = 0;
    bool found = false;
    while (read < MAX_DUPLICATE_CHECK_LINES && file.fgets(line, sizeof(line)) > 0)
    {
      esp_task_wdt_reset();
      int len = strlen(line);
      while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        line[--len] = '\0';
      char tmp[128];
      strncpy(tmp, line, sizeof(tmp) - 1);
      tmp[sizeof(tmp) - 1] = '\0';
      char *c1 = strchr(tmp, ',');
      if (!c1)
      {
        read++;
        continue;
      }
      char *c2 = strchr(c1 + 1, ',');
      if (!c2)
      {
        read++;
        continue;
      }
      char *c3 = strchr(c2 + 1, ',');
      if (!c3)
      {
        read++;
        continue;
      }
      char *c4 = strchr(c3 + 1, ',');
      *c1 = '\0';
      unsigned long ft;
      if (c4)
      {
        *c4 = '\0';
        ft = strtoul(c3 + 1, nullptr, 10);
      }
      else
      {
        ft = strtoul(c3 + 1, nullptr, 10);
      }
      if (strcmp(tmp, rfid) == 0 && ft > 0 && t >= ft && (t - ft) < MIN_REPEAT_INTERVAL)
      {
        found = true;
        break;
      }
      read++;
    }
    file.close();
    if (found)
      return true;
  }
  return false;
}
// "YYYY-MM-DD HH:MM:SS" tepat 19 karakter. Bentuk lain pasti ditolak server.
static bool looksLikeTimestamp(const char *s)
{
  for (int i = 0; i < 19; i++)
  {
    char c = s[i];
    if (i == 4 || i == 7)
    {
      if (c != '-')
        return false;
    }
    else if (i == 10)
    {
      if (c != ' ')
        return false;
    }
    else if (i == 13 || i == 16)
    {
      if (c != ':')
        return false;
    }
    else if (c < '0' || c > '9')
      return false;
  }
  return true;
}
// Klasifikasi satu baris data antrean (header sudah dilewati pemanggil). Mengubah `line` hanya
// dengan membuang CR/LF. out boleh nullptr. Mengembalikan LINE_*.
static int classifyQueueLine(char *line, OfflineRecord *out, time_t now, bool nowValid)
{
  int len = strlen(line);
  while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
    line[--len] = '\0';
  if (len == 0)
    return LINE_EMPTY;
  if (len < 10)
    return LINE_MALFORMED;
  char *c1 = strchr(line, ',');
  char *c2 = c1 ? strchr(c1 + 1, ',') : nullptr;
  char *c3 = c2 ? strchr(c2 + 1, ',') : nullptr;
  char *c4 = c3 ? strchr(c3 + 1, ',') : nullptr;
  if (!c1 || !c2 || !c3)
    return LINE_MALFORMED;
  int rl = c1 - line, tl = c2 - c1 - 1, dl = c3 - c2 - 1;
  // Server menolak SELURUH batch bila satu item rfid != 10 karakter atau timestamp tidak valid,
  // sehingga file macet selamanya. Tolak di sini agar baris itu masuk karantina.
  if (rl != 10 || tl != 19 || dl > 19 || !looksLikeTimestamp(c1 + 1))
    return LINE_MALFORMED;
  unsigned long recT = strtoul(c3 + 1, nullptr, 10);
  if (recT == 0)
    return LINE_MALFORMED;
  if (c4)
  {
    char rfidTmp[11];
    memset(rfidTmp, 0, sizeof(rfidTmp));
    memcpy(rfidTmp, line, rl);
    char expected[3];
    snprintf(expected, sizeof(expected), "%02X", (unsigned)recordCrc8(rfidTmp, recT));
    if (strncmp(c4 + 1, expected, 2) != 0 || c4[3] != '\0')
      return LINE_BAD_CRC;
  }
  if (isRecordExpired(recT, now, nowValid))
    return LINE_EXPIRED;
  if (out)
  {
    memset(out, 0, sizeof(OfflineRecord));
    memcpy(out->rfid, line, rl);
    memcpy(out->timestamp, c1 + 1, tl);
    memcpy(out->deviceId, c2 + 1, dl);
    out->unixTime = recT;
  }
  return LINE_OK;
}

static bool openQuarantineLocked(FsFile &q)
{
  if (sd.exists(QUARANTINE_FILE))
  {
    FsFile t;
    if (t.open(QUARANTINE_FILE, O_RDONLY))
    {
      uint64_t sz = t.size();
      t.close();
      if (sz > QUARANTINE_MAX_BYTES)
      {
        if (sd.exists(QUARANTINE_OLD))
          sd.remove(QUARANTINE_OLD);
        sd.rename(QUARANTINE_FILE, QUARANTINE_OLD);
      }
    }
  }
  if (!q.open(QUARANTINE_FILE, O_RDWR | O_CREAT | O_APPEND))
    return false;
  if (!prepareAppendLocked(q, "reason,source,raw_line"))
  {
    q.close();
    return false;
  }
  return true;
}

// Salin semua baris yang TIDAK akan terkirim ke karantina. Panggil dengan xSdMutex + selectSD().
// sentCnt = jumlah record sah yang memang dikirim; record sah berikutnya (>= sentCnt) dicatat "UNSENT".
// false = karantina gagal ditulis: pemanggil JANGAN menghapus file.
static bool quarantineRejectedLocked(const char *fn, int sentCnt, uint64_t keepMask)
{
  FsFile src;
  if (!src.open(fn, O_RDONLY))
    return false;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  FsFile q;
  bool qOpen = false, ok = true;
  int okOrd = 0;
  char line[144];
  if (src.available())
    src.fgets(line, sizeof(line)); // header
  while (src.available())
  {
    esp_task_wdt_reset();
    if (src.fgets(line, sizeof(line)) <= 0)
      break;
    int rc = classifyQueueLine(line, nullptr, now, nowValid);
    const char *reason = nullptr;
    if (rc == LINE_OK)
    {
      int ord = okOrd++;
      if (ord >= sentCnt)
        reason = "UNSENT";
      else if (ord < 64 && ((keepMask >> ord) & 1ULL))
        continue; // retryable: dipertahankan di file, bukan dikarantina
    }
    else if (rc == LINE_EXPIRED)
      reason = "EXPIRED";
    else if (rc == LINE_BAD_CRC)
      reason = "BAD_CRC";
    else if (rc == LINE_MALFORMED)
      reason = "MALFORMED";
    if (!reason)
      continue;
    if (!qOpen)
    {
      if (!openQuarantineLocked(q))
      {
        ok = false;
        break;
      }
      qOpen = true;
    }
    char outLn[200];
    int n = snprintf(outLn, sizeof(outLn), "%s,%s,%s\r\n", reason, fn, line);
    if (n <= 0 || n >= (int)sizeof(outLn) || q.write(outLn, (size_t)n) != (size_t)n)
    {
      ok = false;
      break;
    }
  }
  src.close();
  if (qOpen)
  {
    if (ok)
      q.sync();
    q.close();
  }
  return ok;
}

// Satu-satunya cara menghapus file antrean di jalur normal.
static bool removeQueueFileSafeLocked(const char *fn, int sentCnt)
{
  if (!quarantineRejectedLocked(fn, sentCnt, 0))
    return false;
  return sd.remove(fn);
}

// Server menerima sebagian; record retryable (bit di keepMask) dipertahankan, sisanya dikarantina/dibuang.
// Tulis ke tmp lalu ganti. Jendela remove->rename sangat kecil; bila daya putus di situ, record ada
// di /queue_keep.tmp (tidak dipindai sebagai antrean) untuk pemulihan manual.
static bool keepRetryRecordsLocked(const char *fn, int sentCnt, uint64_t keepMask)
{
  if (!quarantineRejectedLocked(fn, sentCnt, keepMask))
    return false;
  const char *tmp = "/queue_keep.tmp";
  FsFile src, dst;
  if (!src.open(fn, O_RDONLY))
    return false;
  if (sd.exists(tmp))
    sd.remove(tmp);
  if (!dst.open(tmp, O_WRONLY | O_CREAT | O_TRUNC))
  {
    src.close();
    return false;
  }
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  bool ok = dst.println(QUEUE_HEADER_LINE) > 0;
  int okOrd = 0;
  char line[144];
  if (src.available())
    src.fgets(line, sizeof(line)); // header
  while (ok && src.available())
  {
    esp_task_wdt_reset();
    if (src.fgets(line, sizeof(line)) <= 0)
      break;
    if (classifyQueueLine(line, nullptr, now, nowValid) != LINE_OK)
      continue; // line sudah bebas CR/LF
    int ord = okOrd++;
    if (ord < 64 && ((keepMask >> ord) & 1ULL))
    {
      char out[160];
      int n = snprintf(out, sizeof(out), "%s\r\n", line);
      if (n <= 0 || n >= (int)sizeof(out) || dst.write(out, (size_t)n) != (size_t)n)
        ok = false;
    }
  }
  src.close();
  if (ok)
    dst.sync();
  dst.close();
  if (!ok)
  {
    sd.remove(tmp);
    return false; // file asli utuh; akan dikirim ulang (idempoten di server)
  }
  if (!sd.remove(fn))
    return false;
  return sd.rename(tmp, fn);
}

// Sisa dari keepRetryRecordsLocked(): daya putus antara remove(fn) dan rename(tmp, fn).
// Nama asli tidak tersimpan, jadi cari indeks bebas terkecil dan pakai itu.
// Panggil dengan selectSD() aktif dan sebelum penulisan antrean dimulai.
static void recoverKeepTmpLocked()
{
  const char *tmp = "/queue_keep.tmp";
  if (!sd.exists(tmp))
    return;
  char fn[24];
  for (int i = 0; i < MAX_QUEUE_FILES; i++)
  {
    esp_task_wdt_reset();
    getQueueFileName(i, fn, sizeof(fn));
    if (sd.exists(fn))
      continue;
    if (sd.rename(tmp, fn))
      LOG_I("[SD] pulihkan %s dari queue_keep.tmp\n", fn);
    return;
  }
}

bool initSDCard()
{
  pinMode(PIN_SD_CS, OUTPUT);
  pinMode(PIN_RFID_SS, OUTPUT);
  deselectSD();
  digitalWrite(PIN_RFID_SS, HIGH);
  selectSD();
  delay(10);
  if (!sd.begin(PIN_SD_CS, SD_SCK_MHZ(10)))
  {
    sdInitErr = sd.sdErrorCode();
    deselectSD();
    return false;
  }
  loadMetadataLocked();
  recoverKeepTmpLocked();
  pendingCacheDirty = true;
  if (!rtcQueueFileValid)
  {
    currentQueueFile = -1;
    char fn[20];
    for (int i = 0; i < MAX_QUEUE_FILES; i++)
    {
      esp_task_wdt_reset();
      getQueueFileName(i, fn, sizeof(fn));
      if (!sd.exists(fn))
      {
        if (file.open(fn, O_WRONLY | O_CREAT))
        {
          file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
          file.close();
          currentQueueFile = i;
          break;
        }
      }
      else
      {
        int cnt = countValidRecordsInFileLocked(fn);
        if (cnt == 0)
        {
          if (removeQueueFileSafeLocked(fn, 0) && file.open(fn, O_WRONLY | O_CREAT))
          {
            file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
            file.close();
            currentQueueFile = i;
            break;
          }
        }
        else if (cnt < MAX_RECORDS_PER_FILE)
        {
          currentQueueFile = i;
          break;
        }
      }
    }
    if (currentQueueFile == -1)
    {
      currentQueueFile = 0;
    }
    rtcQueueFileValid = true;
  }
  deselectSD();
  return true;
}
bool findAvailableQueueSlotLocked(int startIdx, int *outIdx)
{
  char fn[20];
  for (int offset = 0; offset < QUEUE_SLOT_SEARCH_LIMIT; offset++)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int idx = (startIdx + offset) % MAX_QUEUE_FILES;
    if (idx == syncingQueueFile)
      continue; // jangan pilih file yang sedang di-sync
    getQueueFileName(idx, fn, sizeof(fn));
    if (!sd.exists(fn))
    {
      *outIdx = idx;
      return true;
    }
    int cnt = countValidRecordsInFileLocked(fn);
    if (cnt == 0)
    {
      if (removeQueueFileSafeLocked(fn, 0)) // sisa baris buruk masuk karantina dulu
      {
        *outIdx = idx;
        return true;
      }
      continue; // karantina gagal: file dibiarkan, coba indeks lain
    }
    if (cnt < MAX_RECORDS_PER_FILE)
    {
      *outIdx = idx;
      return true;
    }
  }
  return false;
}
SaveResult saveToQueue(const char *rfid, const char *ts, unsigned long t)
{
  if (!sdCardAvailable)
  {
    return SAVE_SD_ERROR;
  }
  if (!acquireSD())
  {
    return SAVE_SD_ERROR;
  }
  selectSD();
  bool dup = isDuplicateLocked(rfid, t);
  if (dup)
  {
    deselectSD();
    releaseSD();
    return SAVE_DUPLICATE;
  }
  bool fileSwitched = false;
  if (currentQueueFile < 0 || currentQueueFile >= MAX_QUEUE_FILES)
  {
    currentQueueFile = 0;
    fileSwitched = true;
  }
  char curFn[20];
  getQueueFileName(currentQueueFile, curFn, sizeof(curFn));
  if (!sd.exists(curFn))
  {
    if (file.open(curFn, O_WRONLY | O_CREAT))
    {
      file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
      file.close();
    }
  }
  int curCnt = countRecordsInFileLocked(curFn);
  if (curCnt >= MAX_RECORDS_PER_FILE)
  {
    int nextIdx;
    int startSearch = (currentQueueFile + 1) % MAX_QUEUE_FILES;
    if (!findAvailableQueueSlotLocked(startSearch, &nextIdx))
    {
      deselectSD();
      releaseSD();
      return SAVE_QUEUE_FULL;
    }
    currentQueueFile = nextIdx;
    fileSwitched = true;
    getQueueFileName(currentQueueFile, curFn, sizeof(curFn));
    if (!file.open(curFn, O_WRONLY | O_CREAT))
    {
      deselectSD();
      releaseSD();
      return SAVE_SD_ERROR;
    }
    file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
    file.close();
  }
  if (!file.open(curFn, O_RDWR | O_APPEND))
  {
    deselectSD();
    releaseSD();
    return SAVE_SD_ERROR;
  }
  // 1.1: tutup baris parsial / tulis header bila file kosong sebelum menambah record.
  if (!prepareAppendLocked(file, QUEUE_HEADER_LINE))
  {
    file.close();
    deselectSD();
    releaseSD();
    return SAVE_SD_ERROR;
  }
  uint8_t crc = recordCrc8(rfid, t);
  // Satu write() per baris (format CSV sama persis dengan sebelumnya, termasuk CRLF).
  char lineBuf[96];
  int n = snprintf(lineBuf, sizeof(lineBuf), "%s,%s,%s,%lu,%02X\r\n", rfid, ts, deviceId, t, (unsigned)crc);
  if (n <= 0 || n >= (int)sizeof(lineBuf) || file.write(lineBuf, (size_t)n) != (size_t)n)
  {
    file.close();
    deselectSD();
    releaseSD();
    return SAVE_SD_ERROR;
  }
  file.sync();
  file.close();
  deselectSD();
  cachedPendingRecords++;
  if (cachedPendingRecords < 0)
    cachedPendingRecords = 0;
  queueMutations = queueMutations + 1;
  if (fileSwitched)
    saveMetadataLocked();
  releaseSD();
  return SAVE_OK;
}

// sentMask bit i = record indeks i benar-benar dimasukkan ke payload (i < cntAtSend).
// Record tak terkirim (gagal dibaca karena mutex, atau ditulis saat HTTP) DIPERTAHANKAN.
// Blob hilang/rusak (panjang salah) tidak bisa dikirim: dibuang dan dicatat.
// Urutan aman power-loss: (1) salin ke bawah (hanya menimpa record yang sudah terkirim/rusak),
// (2) putInt(count) = titik atomik, (3) hapus ekor. Daya putus di antara langkah = duplikat, bukan hilang.
static bool nvsCompactAfterSync(uint64_t sentMask, int cntAtSend)
{
  if (!lockNvs(pdMS_TO_TICKS(3000)))
    return false;
  prefs.begin(NVS_NAMESPACE, false);
  int total = prefs.getInt(NVS_KEY_COUNT, 0);
  if (total > NVS_MAX_RECORDS)
    total = NVS_MAX_RECORDS;
  char srcKey[16], dstKey[16];
  int dst = 0, dropped = 0;
  bool ok = true;
  for (int src = 0; src < total; src++)
  {
    bool wasSent = (src < cntAtSend && src < 64 && ((sentMask >> src) & 1ULL));
    if (wasSent)
      continue;
    snprintf(srcKey, sizeof(srcKey), "%s%d", NVS_KEY_PREFIX, src);
    if (prefs.getBytesLength(srcKey) != sizeof(OfflineRecord))
    {
      dropped++;
      continue;
    }
    if (src != dst)
    {
      OfflineRecord rec;
      prefs.getBytes(srcKey, &rec, sizeof(OfflineRecord));
      snprintf(dstKey, sizeof(dstKey), "%s%d", NVS_KEY_PREFIX, dst);
      if (prefs.putBytes(dstKey, &rec, sizeof(OfflineRecord)) != sizeof(OfflineRecord))
      {
        ok = false; // NVS penuh/gagal: count tidak diubah, tidak ada yang hilang
        break;
      }
    }
    dst++;
  }
  if (ok && dst != total)
  {
    prefs.putInt(NVS_KEY_COUNT, dst);
    for (int k = dst; k < total; k++)
    {
      snprintf(dstKey, sizeof(dstKey), "%s%d", NVS_KEY_PREFIX, k);
      prefs.remove(dstKey);
    }
  }
  prefs.end();
  unlockNvs();
  LOG_I("[NVS] compact ok=%d kept=%d dropped=%d\n", (int)ok, dst, dropped);
  if (ok && dropped > 0)
    appendFailedLogToSD("NVS", "BUFFER", "RECORD_RUSAK_DIBUANG");
  return ok;
}

// Parse respons sync-bulk. true = JSON valid dan jumlah item == expected.
// Error final dicatat ke failed_log. Error dengan "retryable":true ditandai di retryMask
// (bit i = item ke-i sesuai urutan request) dan TIDAK dicatat; pemanggil mempertahankannya.
static bool parseSyncReply(const String &body, int expected, uint64_t *retryMask)
{
  *retryMask = 0;
  JsonDocument res;
  if (deserializeJson(res, body) != DeserializationError::Ok || !res["data"].is<JsonArray>())
    return false;
  JsonArray arr = res["data"].as<JsonArray>();
  if ((int)arr.size() != expected || expected > 64)
    return false;
  int i = 0;
  for (JsonObject item : arr)
  {
    const char *st = item["status"] | "error";
    if (strcmp(st, "error") == 0)
    {
      if (item["retryable"] | false)
        *retryMask |= (1ULL << i);
      else
        appendFailedLogToSD(item["rfid"] | "unknown", item["timestamp"] | "unknown", item["message"] | "UNKNOWN");
    }
    i++;
  }
  return true;
}

bool nvsSyncToServer()
{
  int cnt = nvsGetCount();
  if (cnt == 0)
    return true;
  if (cnt > NVS_MAX_RECORDS)
    cnt = NVS_MAX_RECORDS;
  if (isSignalCritical())
    return false;
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(30000);
  http.setConnectTimeout(10000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/sync-bulk"))
    return false;
  if (!http.begin(client, url))
    return false;
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  JsonDocument doc;
  JsonArray arr = doc["data"].to<JsonArray>();
  uint64_t sentMask = 0;
  int sentCnt = 0;
  int idxOf[NVS_MAX_RECORDS];
  for (int i = 0; i < cnt; i++)
  {
    OfflineRecord rec;
    if (!nvsLoadRecord(i, rec))
      continue;
    JsonObject o = arr.add<JsonObject>();
    o["rfid"] = rec.rfid;
    o["timestamp"] = rec.timestamp;
    o["device_id"] = rec.deviceId;
    o["sync_mode"] = true;
    idxOf[sentCnt] = i;
    sentMask |= (1ULL << i);
    sentCnt++;
  }
  if (sentCnt == 0)
  {
    http.end();
    return nvsCompactAfterSync(0, cnt);
  }
  String payload;
  payload.reserve(measureJson(doc) + 1); // satu alokasi
  serializeJson(doc, payload);
  doc.clear();
  esp_task_wdt_reset();
  int code = http.POST(payload);
  esp_task_wdt_reset();
  taskYIELD();
  if (code == 200)
  {
    String body = http.getString();
    esp_task_wdt_reset();
    http.end();
    uint64_t retryMask = 0;
    if (!parseSyncReply(body, sentCnt, &retryMask))
    {
      appendFailedLogToSD("BATCH", "NVS_BUFFER", "RESPONSE_TIDAK_VALID_ATAU_TERPOTONG");
      return false;
    }
    isOnline = true;
    for (int j = 0; j < sentCnt; j++)
      if ((retryMask >> j) & 1ULL)
        sentMask &= ~(1ULL << idxOf[j]);
    return nvsCompactAfterSync(sentMask, cnt);
  }
  http.end();
  return false;
}

// Penghitung gagal berurutan per endpoint polling. Hanya diakses dari taskSync (dan reset saat reconnect).
static uint8_t failOta = 0;
static uint8_t failDb = 0;
static uint8_t failTelemetry = 0;
static uint8_t failConfig = 0;

static void backoffFail(uint8_t &f)
{
  if (f < 255)
    f++;
}

static void resetPollBackoff()
{
  failOta = 0;
  failDb = 0;
  failTelemetry = 0;
  failConfig = 0;
}
unsigned long checkRfidDbVersion()
{
  if (isSignalWeak())
    return 0;
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/rfid-list/version"))
  {
    return 0;
  }
  if (!http.begin(client, url))
    return 0;
  http.addHeader(F("X-API-KEY"), apiKey);
  int code = http.GET();
  if (code != 200)
  {
    http.end();
    return 0;
  }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
    return 0;
  return doc["ver"] | 0UL;
}

// Tulis satu blok ke file tmp. Mutex SD dipegang hanya selama penulisan.
static bool dbWriteChunk(FsFile &dbf, const char *data, size_t len)
{
  if (len == 0)
    return true;
  if (!acquireSD(pdMS_TO_TICKS(3000)))
    return false;
  selectSD();
  size_t w = dbf.write(data, len);
  deselectSD();
  releaseSD();
  return w == len;
}

bool downloadRfidDb()
{
  if (isSignalWeak() || !sdCardAvailable)
    return false;

  showOLED(F("RFID DB"), "MENGUNDUH...");
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(30000);
  http.setConnectTimeout(10000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/rfid-list"))
    return false;
  if (!http.begin(client, url))
    return false;
  http.useHTTP10(true); // hindari chunked encoding pada stream mentah
  http.addHeader(F("X-API-KEY"), apiKey);

  int code = http.GET();
  if (code != 200)
  {
    http.end();
    showOLED(F("RFID DB"), "GAGAL UNDUH");
    playToneError();
    delay(800);
    return false;
  }

  // Siapkan file tmp. Mutex SD hanya untuk operasi ini.
  const char *tmpPath = "/rfid_db.tmp";
  FsFile dbf;
  if (!acquireSD(pdMS_TO_TICKS(3000)))
  {
    http.end();
    return false;
  }
  selectSD();
  if (sd.exists(tmpPath))
    sd.remove(tmpPath);
  bool opened = dbf.open(tmpPath, O_WRONLY | O_CREAT | O_TRUNC);
  deselectSD();
  releaseSD();
  if (!opened)
  {
    http.end();
    return false;
  }

  // Streaming tanpa memegang mutex SD.
  WiFiClient *stream = http.getStreamPtr();
  int total = http.getSize();
  long bytesRead = 0;
  unsigned long lastDataAt = millis();
  unsigned long serverVer = 0;
  bool firstLine = true;
  bool receivedEndMarker = false;
  bool writeOk = true;
  int written = 0;

  char lineBuf[32];
  int lbPos = 0;
  bool lineTruncated = false;
  char outBuf[512];
  size_t outLen = 0;
  uint8_t chunk[256];

  while (http.connected() && (total <= 0 || bytesRead < total) && !receivedEndMarker && writeOk)
  {
    esp_task_wdt_reset();
    taskYIELD();
    int avail = stream->available();
    if (!avail)
    {
      if (millis() - lastDataAt > 120000UL)
        break;
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    lastDataAt = millis();
    int rd = stream->readBytes(chunk, min(avail, (int)sizeof(chunk)));
    bytesRead += rd;

    for (int i = 0; i < rd && !receivedEndMarker; i++)
    {
      char c = (char)chunk[i];
      if (c == '\r')
        continue;
      if (c != '\n')
      {
        if (lbPos < (int)sizeof(lineBuf) - 1)
          lineBuf[lbPos++] = c;
        else
          lineTruncated = true;
        continue;
      }

      // Akhir baris
      lineBuf[lbPos] = '\0';
      lbPos = 0;
      bool truncated = lineTruncated;
      lineTruncated = false;

      if (firstLine)
      {
        firstLine = false;
        if (strncmp(lineBuf, "ver:", 4) == 0)
        {
          serverVer = strtoul(lineBuf + 4, nullptr, 10);
          continue;
        }
      }
      if (strcmp(lineBuf, "END") == 0)
      {
        receivedEndMarker = true;
        break;
      }
      if (truncated || strlen(lineBuf) != 10)
        continue;

      bool ok = true;
      for (int j = 0; j < 10 && ok; j++)
        ok = isdigit((unsigned char)lineBuf[j]);
      if (!ok)
        continue;

      if (outLen + 11 > sizeof(outBuf))
      {
        if (!dbWriteChunk(dbf, outBuf, outLen))
        {
          writeOk = false;
          break;
        }
        outLen = 0;
      }
      memcpy(outBuf + outLen, lineBuf, 10);
      outBuf[outLen + 10] = '\n';
      outLen += 11;
      written++;
    }
  }
  http.end();

  if (writeOk && outLen > 0)
    writeOk = dbWriteChunk(dbf, outBuf, outLen);

  // Tutup file di bawah mutex SD.
  if (acquireSD(pdMS_TO_TICKS(5000)))
  {
    selectSD();
    dbf.sync();
    dbf.close();
    deselectSD();
    releaseSD();
  }
  else
  {
    writeOk = false;
  }

  // Daftar kosong yang sah (END diterima, 0 kartu) bukan kegagalan.
  bool failed = !writeOk || !receivedEndMarker;
  if (failed)
  {
    if (acquireSD(pdMS_TO_TICKS(3000)))
    {
      selectSD();
      sd.remove(tmpPath);
      deselectSD();
      releaseSD();
    }
    showOLED(F("RFID DB"), "UNDUH TERPUTUS");
    playToneError();
    delay(800);
    appendFailedLogToSD("RFID_DB", "download", "TERPUTUS_ATAU_KOSONG");
    return false;
  }

  // Ganti file aktif secara aman: DB lama dipindah ke .bak dan dikembalikan jika rename gagal.
  // Urutan kunci: SD lalu cache (di dalam loadRfidCacheFromFileLocked).
  if (!acquireSD(pdMS_TO_TICKS(5000)))
    return false; // file tmp tetap ada, akan diganti pada unduhan berikutnya
  selectSD();
  if (sd.exists(RFID_DB_BAK))
    sd.remove(RFID_DB_BAK);
  bool hadOld = sd.exists(RFID_DB_FILE);
  bool swapped = true;
  if (hadOld && !sd.rename(RFID_DB_FILE, RFID_DB_BAK))
    swapped = false; // DB lama utuh
  else if (!sd.rename(tmpPath, RFID_DB_FILE))
  {
    swapped = false;
    if (hadOld)
      sd.rename(RFID_DB_BAK, RFID_DB_FILE); // kembalikan DB lama
  }
  if (swapped)
  {
    if (hadOld)
      sd.remove(RFID_DB_BAK);
    loadRfidCacheFromFileLocked();
  }
  deselectSD();
  releaseSD();
  if (!swapped)
  {
    showOLED(F("RFID DB"), "GAGAL GANTI FILE");
    playToneError();
    delay(800);
    return false; // versi lokal tidak diubah, dicoba lagi pada pengecekan berikutnya
  }

  if (serverVer == 0 && written > 0)
  {
    unsigned long fallbackVer = checkRfidDbVersion();
    if (fallbackVer > 0)
      serverVer = fallbackVer;
  }
  if (serverVer > 0)
    nvsSetRfidDbVer(serverVer);

  char buf[20];
  snprintf(buf, sizeof(buf), "%d RFID", written);
  showOLED(F("RFID DB"), buf);
  playToneSuccess();
  delay(800);
  return true;
}
void checkAndUpdateRfidDb()
{
  if (!sdCardAvailable || isSignalWeak())
    return;
  if (millis() - timers.lastRfidDbCheck < backoffInterval(RFID_DB_CHECK_INTERVAL, failDb))
    return;
  timers.lastRfidDbCheck = millis();
  unsigned long local = nvsGetRfidDbVer(), server = checkRfidDbVersion();
  if (server == 0)
  {
    backoffFail(failDb); // server tidak merespons atau respons tidak valid
    return;
  }
  failDb = 0;
  if (server <= local)
  {
    return;
  }
  if (!downloadRfidDb())
    backoffFail(failDb);
}

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

// Default firmware untuk remote config. Dipakai saat server tidak mengirim field atau mengirim null.
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

static unsigned long jsonIntervalOrDefault(JsonVariantConst v, unsigned long def)
{
  if (v.isNull() || !v.is<unsigned long>())
    return def;
  unsigned long x = v.as<unsigned long>();
  return (x >= CFG_INTERVAL_MIN_MS && x <= CFG_INTERVAL_MAX_MS) ? x : def;
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
    return; // JSON rusak: pertahankan konfigurasi lama
  }
  if (!doc.is<JsonObject>())
  {
    backoffFail(failConfig);
    return; // 200 tetapi bukan objek (mis. array/string): jangan reset ke default
  }
  failConfig = 0;
  // Respons valid: setiap field dihitung ulang. Field yang tidak ada atau null kembali ke default.
  RuntimeConfig next;
  next.sleepStartHour = jsonHourOrDefault(doc["sleep_start"], kRuntimeDefaults.sleepStartHour);
  next.sleepEndHour = jsonHourOrDefault(doc["sleep_end"], kRuntimeDefaults.sleepEndHour);
  next.dimStartHour = jsonHourOrDefault(doc["oled_dim_start"], kRuntimeDefaults.dimStartHour);
  next.dimEndHour = jsonHourOrDefault(doc["oled_dim_end"], kRuntimeDefaults.dimEndHour);
  next.syncIntervalMs = jsonIntervalOrDefault(doc["sync_interval_ms"], kRuntimeDefaults.syncIntervalMs);
  next.otaCheckIntervalMs = jsonIntervalOrDefault(doc["ota_check_interval_ms"], kRuntimeDefaults.otaCheckIntervalMs);
  if (next.sleepStartHour == next.sleepEndHour)
    LOG_W("[CFG] jendela sleep kosong (mulai == selesai): sleep nonaktif\n");
  if (next.dimStartHour == next.dimEndHour)
    LOG_W("[CFG] jendela dim kosong (mulai == selesai): dim nonaktif\n");
  if (!xConfigMutex || xSemaphoreTake(xConfigMutex, pdMS_TO_TICKS(500)) != pdTRUE)
    return;
  bool changed = !sameRuntimeConfig(rtCfg, next);
  rtCfg = next;
  xSemaphoreGive(xConfigMutex);

  // Tulis NVS hanya jika ada perubahan, agar flash tidak aus karena polling berkala.
  if (changed)
    persistRuntimeConfigToNvs(next);
}

// Ambil host dari URL (huruf kecil, tanpa port/path). false jika bukan format scheme://host.
static bool extractUrlHost(const char *url, char *out, size_t outSz)
{
  const char *p = strstr(url, "://");
  if (!p)
    return false;
  p += 3;
  size_t i = 0;
  while (*p && *p != '/' && *p != ':' && *p != '?' && *p != '#')
  {
    if (i + 1 >= outSz)
      return false;
    out[i++] = (char)tolower((unsigned char)*p++);
  }
  out[i] = '\0';
  return i > 0;
}
// URL OTA harus https dan satu host dengan API. Mencegah API key dikirim ke host lain.
static bool isOtaUrlAllowed(const char *otaUrl)
{
  if (strncmp(otaUrl, "https://", 8) != 0)
    return false;
  char h1[64], h2[64];
  if (!extractUrlHost(otaUrl, h1, sizeof(h1)) || !extractUrlHost(apiBaseUrl, h2, sizeof(h2)))
    return false;
  return strcmp(h1, h2) == 0;
}
void checkOtaUpdate()
{
  if (isSignalWeak())
    return;
  RuntimeConfig cfg = getRuntimeConfigSnapshot();
  if (millis() - timers.lastOtaCheck < backoffInterval(cfg.otaCheckIntervalMs, failOta))
    return;
  timers.lastOtaCheck = millis();
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(8000);
  http.setConnectTimeout(5000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/firmware/check"))
  {
    return;
  }
  if (!http.begin(client, url))
  {
    return;
  }
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  char payload[80];
  snprintf(payload, sizeof(payload), "{\"version\":\"%s\",\"device_id\":\"%s\"}", FIRMWARE_VERSION, deviceId);
  int code = http.POST(payload);
  if (code != 200)
  {
    LOG_I("[OTA] check HTTP %d\n", code);
    http.end();
    backoffFail(failOta);
    return;
  }
  String body = http.getString();
  http.end();
  JsonDocument doc;
  if (deserializeJson(doc, body) != DeserializationError::Ok)
  {
    backoffFail(failOta);
    return;
  }
  failOta = 0; // server merespons dengan benar; penolakan update di bawah bukan kegagalan server
  bool hasUpdate = doc["update"] | false;
  const char *ver = doc["version"] | "";
  const char *burl = doc["url"] | "";
  const char *md5 = doc["md5"] | "";
  LOG_I("[OTA] update=%d ver=%s md5len=%u url=%s\n", (int)hasUpdate, ver, (unsigned)strlen(md5), burl);
  if (!hasUpdate || !strlen(ver) || !strlen(burl))
    return;
  if (!isValidMd5Hex(md5))
  {
    LOG_W("[OTA] ditolak: md5 tidak valid\n");
    return;
  }
  if (strlen(burl) >= sizeof(otaState.url) || !isOtaUrlAllowed(burl))
  {
    Serial.println("[OTA] ditolak: URL terlalu panjang atau host beda dari API");
    return;
  }
  if (otaRejectedVer[0] != '\0' && strcmp(ver, otaRejectedVer) == 0)
  {
    LOG_W("[OTA] dilewati: versi ini pernah gagal verifikasi\n");
    return;
  }
  if (compareFirmwareVersion(ver, FIRMWARE_VERSION) <= 0)
  {
    LOG_W("[OTA] ditolak: versi tidak lebih baru\n");
    return;
  }
  strncpy(otaState.version, ver, sizeof(otaState.version) - 1);
  otaState.version[sizeof(otaState.version) - 1] = '\0';
  strncpy(otaState.url, burl, sizeof(otaState.url) - 1);
  otaState.url[sizeof(otaState.url) - 1] = '\0';
  strncpy(otaState.md5, md5, sizeof(otaState.md5) - 1);
  otaState.md5[sizeof(otaState.md5) - 1] = '\0';
  otaState.updateAvailable = true;
  char buf[32];
  snprintf(buf, sizeof(buf), "v%s TERSEDIA", otaState.version);
  showOLED(F("UPDATE"), buf);
  playToneNotify();
  delay(2000);
}

// Gagal OTA: tampilkan pesan, reset status, pulihkan WDT normal.
static void otaAbort(const char *detail, bool startedUpdate)
{
  if (startedUpdate)
    Update.abort();
  showOLED(F("UPDATE GAGAL"), detail);
  playToneError();
  otaState.updateAvailable = false;
  otaInProgress = false;
  restoreWdtNormal();
}

void performOtaUpdate()
{
  if (!otaState.updateAvailable || isSignalWeak())
    return;
  if (!isValidMd5Hex(otaState.md5))
  {
    otaAbort("MD5 TIDAK ADA", false);
    return;
  }
  char buf[20];
  snprintf(buf, sizeof(buf), "v%s", otaState.version);
  showOLED(F("UPDATE OTA"), buf);
  delay(500);
  showOLED(F("MENGUNDUH"), "MOHON TUNGGU...");
  extendWdtForSync();
  otaInProgress = true;

  WiFiClientSecure otaClient;
  applyTls(otaClient);
  HTTPClient http;
  http.setTimeout(60000);
  http.useHTTP10(true);
  if (!http.begin(otaClient, otaState.url))
  {
    otaAbort("URL ERR", false);
    return;
  }
  http.addHeader(F("X-API-KEY"), apiKey);
  int code = http.GET();
  if (code != 200)
  {
    snprintf(buf, sizeof(buf), "HTTP ERR %d", code);
    http.end();
    otaAbort(buf, false);
    return;
  }
  int total = http.getSize();
  size_t updateSize = (total > 0) ? (size_t)total : UPDATE_SIZE_UNKNOWN;
  if (!Update.begin(updateSize))
  {
    http.end();
    otaAbort("NO SPACE", false);
    return;
  }
  Update.setMD5(otaState.md5);

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buff[1024];
  size_t written = 0;
  unsigned long lastDataAt = millis();
  bool ioError = false;
  OtaVerScan verScan;
  while (http.connected() && (total <= 0 || written < (size_t)total))
  {
    esp_task_wdt_reset();
    int avail = stream->available();
    if (avail > 0)
    {
      int rd = stream->readBytes(buff, min((int)sizeof(buff), avail));
      if (rd <= 0 || Update.write(buff, rd) != (size_t)rd)
      {
        ioError = true;
        break;
      }
      otaVerFeed(verScan, buff, (size_t)rd);
      written += (size_t)rd;
      lastDataAt = millis();
    }
    else
    {
      if (millis() - lastDataAt > 30000UL)
      {
        ioError = true;
        break;
      }
      vTaskDelay(pdMS_TO_TICKS(5));
    }
  }
  http.end();

  if (ioError || (total > 0 && written != (size_t)total))
  {
    otaAbort("UNDUH TERPUTUS", true);
    return;
  }
  if (!verScan.found)
  {
    strncpy(otaRejectedVer, otaState.version, sizeof(otaRejectedVer) - 1);
    otaAbort("VERSI TAK ADA", true);
    return;
  }
  if (strcmp(verScan.ver, otaState.version) != 0)
  {
    LOG_W("[OTA] versi image %s != server %s\n", verScan.ver, otaState.version);
    strncpy(otaRejectedVer, otaState.version, sizeof(otaRejectedVer) - 1);
    otaAbort("VERSI BEDA", true);
    return;
  }
  if (!Update.end() || !Update.isFinished())
  {
    if (Update.getError() == UPDATE_ERROR_MD5)
      otaAbort("MD5 SALAH", false);
    else
    {
      snprintf(buf, sizeof(buf), "ERR %d", Update.getError());
      otaAbort(buf, false);
    }
    return;
  }
  otaTrackAttempt(otaState.version); // dicatat sebelum restart; dibandingkan saat boot (deteksi rollback)
  showOLED(F("UPDATE OK"), "RESTART...");
  playToneSuccess();
  delay(2000);
  restoreWdtNormal();
  ESP.restart();
}

bool readQueueFileLocked(const char *fn, OfflineRecord *recs, int *cnt, int maxCnt)
{
  *cnt = 0;
  if (!file.open(fn, O_RDONLY))
    return false;
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  char line[144];
  if (file.available())
    file.fgets(line, sizeof(line)); // header
  while (file.available() && *cnt < maxCnt)
  {
    esp_task_wdt_reset();
    if (file.fgets(line, sizeof(line)) <= 0)
      break;
    OfflineRecord tmp;
    // Baris yang ditolak TIDAK dibuang di sini; quarantineRejectedLocked() mencatatnya sebelum file dihapus.
    if (classifyQueueLine(line, &tmp, now, nowValid) == LINE_OK)
      recs[(*cnt)++] = tmp;
  }
  file.close();
  return *cnt > 0;
}
// Dipanggil saat mutex SD dipegang. Jika file yang akan di-sync adalah file tulis aktif,
// alihkan writer ke file lain dulu, sehingga file ini tidak akan ditambah record lagi.
static bool detachFromWriterLocked(const char *fn)
{
  int idx;
  if (!parseQueueIndex(fn + 1, &idx)) // lewati "/" di depan
    return false;
  syncingQueueFile = idx;
  if (idx != currentQueueFile)
    return true;
  int nextIdx;
  if (!findAvailableQueueSlotLocked((currentQueueFile + 1) % MAX_QUEUE_FILES, &nextIdx) || nextIdx == idx)
  {
    syncingQueueFile = -1;
    return false;
  }
  char nfn[20];
  getQueueFileName(nextIdx, nfn, sizeof(nfn));
  if (!sd.exists(nfn))
  {
    if (!file.open(nfn, O_WRONLY | O_CREAT))
    {
      syncingQueueFile = -1;
      return false;
    }
    file.println(F("rfid,timestamp,device_id,unix_time,crc8"));
    file.close();
  }
  currentQueueFile = nextIdx;
  saveMetadataLocked();
  selectSD(); // saveMetadataLocked() memanggil deselectSD() di akhir
  return true;
}

SyncFileResult syncQueueFile(const char *fn)
{
  if (!sdCardAvailable || !isWifiConnected())
    return SYNC_FILE_NO_WIFI;
  OfflineRecord recs[MAX_RECORDS_PER_FILE];
  int validCnt = 0;
  if (!acquireSD())
    return SYNC_FILE_HTTP_FAIL;
  selectSD();
  if (!sd.exists(fn))
  {
    deselectSD();
    releaseSD();
    pendingCacheDirty = true;
    return SYNC_FILE_EMPTY;
  }
  FsFile probe;
  if (!probe.open(fn, O_RDONLY))
  {
    deselectSD();
    releaseSD();
    return SYNC_FILE_HTTP_FAIL;
  }
  probe.close();
  if (!detachFromWriterLocked(fn))
  {
    deselectSD();
    releaseSD();
    return SYNC_FILE_HTTP_FAIL;
  }
  bool hasData = readQueueFileLocked(fn, recs, &validCnt, MAX_RECORDS_PER_FILE);
  if (!hasData || validCnt == 0)
  {
    bool removed = removeQueueFileSafeLocked(fn, 0);
    deselectSD();
    releaseSD();
    if (!removed)
      return SYNC_FILE_HTTP_FAIL;
    pendingCacheDirty = true;
    return SYNC_FILE_EMPTY;
  }
  deselectSD();
  releaseSD();
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(45000);
  http.setConnectTimeout(15000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi/sync-bulk"))
    return SYNC_FILE_HTTP_FAIL;
  if (!http.begin(client, url))
    return SYNC_FILE_HTTP_FAIL;
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  JsonDocument doc;
  JsonArray arr = doc["data"].to<JsonArray>();
  for (int i = 0; i < validCnt; i++)
  {
    JsonObject o = arr.add<JsonObject>();
    o["rfid"] = recs[i].rfid;
    o["timestamp"] = recs[i].timestamp;
    o["device_id"] = recs[i].deviceId;
    o["sync_mode"] = true;
  }
  String payload;
  payload.reserve(measureJson(doc) + 1);
  serializeJson(doc, payload);
  doc.clear();
  esp_task_wdt_reset();
  int code = http.POST(payload);
  esp_task_wdt_reset();
  taskYIELD();
  if (code == 200)
  {
    String body = http.getString();
    esp_task_wdt_reset();
    http.end();
    uint64_t retryMask = 0;
    if (!parseSyncReply(body, validCnt, &retryMask))
    {
      appendFailedLogToSD("BATCH", fn, "RESPONSE_TIDAK_VALID_ATAU_TERPOTONG");
      return SYNC_FILE_HTTP_FAIL;
    }
    isOnline = true;
    int kept = __builtin_popcountll(retryMask);
    if (!acquireSD())
      return SYNC_FILE_HTTP_FAIL;
    selectSD();
    bool removed = (retryMask == 0) ? removeQueueFileSafeLocked(fn, validCnt)
                                    : keepRetryRecordsLocked(fn, validCnt, retryMask);
    deselectSD();
    releaseSD();
    if (!removed)
    {
      appendFailedLogToSD("BATCH", fn, "KARANTINA_GAGAL_FILE_DIPERTAHANKAN");
      return SYNC_FILE_HTTP_FAIL;
    }
    queueMutations = queueMutations + 1;
    pendingCacheDirty = true;
    int done = validCnt - kept;
    if (cachedPendingRecords >= done)
      cachedPendingRecords -= done;
    else
      cachedPendingRecords = 0;
    return SYNC_FILE_OK;
  }
  http.end();
  if (!isWifiConnected())
  {
    syncState.inProgress = false;
    return SYNC_FILE_NO_WIFI;
  }
  return SYNC_FILE_HTTP_FAIL;
}

bool syncQueueFileWithRetry(const char *fn)
{
  bool result = false;
  for (int attempt = 0; attempt <= MAX_SYNC_RETRIES; attempt++)
  {
    esp_task_wdt_reset();
    if (!isWifiConnected())
    {
      syncState.inProgress = false;
      break;
    }
    SyncFileResult r = syncQueueFile(fn);
    if (r == SYNC_FILE_OK || r == SYNC_FILE_EMPTY)
    {
      result = true;
      break;
    }
    if (r == SYNC_FILE_NO_WIFI)
    {
      syncState.inProgress = false;
      break;
    }
    if (attempt < MAX_SYNC_RETRIES)
    {
      char buf[20];
      snprintf(buf, sizeof(buf), "RETRY %d/%d...", attempt + 1, MAX_SYNC_RETRIES);
      showOLED(F("SYNC ULANG"), buf);
      unsigned long t0 = millis(), waitMs = SYNC_RETRY_DELAY_MS * (1UL << attempt);
      while (millis() - t0 < waitMs) // aman terhadap wrap millis
      {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
  }
  syncingQueueFile = -1;
  return result;
}
void chunkedSync()
{
  if (!sdCardAvailable || !isWifiConnected())
  {
    syncState.inProgress = false;
    return;
  }
  extendWdtForSync();
  if (!syncState.inProgress)
  {
    syncState.inProgress = true;
    syncState.currentFile = 0;
    syncState.startTime = millis();
    syncState.filesSucceeded = 0;
  }
  syncState.filesProcessed = 0; // kuota per siklus, bukan per sesi
  char fn[24];
  bool finished = false;
  while (syncState.filesProcessed < MAX_SYNC_FILES_PER_CYCLE)
  {
    if (!isWifiConnected())
    {
      syncState.inProgress = false;
      break;
    }
    esp_task_wdt_reset();
    taskYIELD();
    if (!acquireSD(pdMS_TO_TICKS(1000)))
      break; // coba lagi di siklus berikutnya, jangan lompati file
    selectSD();
    int idx;
    if (!findNextQueueFileLocked(syncState.currentFile, &idx))
    {
      deselectSD();
      releaseSD();
      finished = true;
      break;
    }
    syncState.currentFile = idx;
    getQueueFileName(idx, fn, sizeof(fn));
    int nRecs = countValidRecordsInFileLocked(fn);

    if (nRecs == 0)
    {
      if (idx != currentQueueFile) // file tulis aktif yang masih kosong dibiarkan
      {
        if (removeQueueFileSafeLocked(fn, 0))
        {
          pendingCacheDirty = true;
          syncState.filesProcessed++;
        }
      }
      deselectSD();
      releaseSD();
      syncState.currentFile = idx + 1;
      continue;
    }
    deselectSD();
    releaseSD();
    char buf[40];
    snprintf(buf, sizeof(buf), "FILE %d (%d rec)", idx, nRecs);
    showOLED(F("SYNC"), buf);
    bool ok = syncQueueFileWithRetry(fn);
    syncState.filesProcessed++;
    if (ok)
      syncState.filesSucceeded++;
    else if (!isWifiConnected())
    {
      syncState.inProgress = false;
      break;
    }
    syncState.currentFile = idx + 1;
    taskYIELD();
    esp_task_wdt_reset();
  }
  if (finished)
  {
    syncState.inProgress = false;
    syncState.currentFile = 0;
    syncState.filesProcessed = 0;
    refreshPendingCache();
    if (syncState.filesSucceeded > 0)
    {
      char buf[20];
      if (cachedPendingRecords == 0)
      {
        showOLED(F("SYNC"), "SELESAI!");
        playToneSuccess();
      }
      else
      {
        snprintf(buf, sizeof(buf), "SISA %d", cachedPendingRecords);
        showOLED(F("SYNC PARSIAL"), buf);
      }
      delay(500);
    }
    syncState.filesSucceeded = 0;
  }
  restoreWdtNormal();
}
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
  WiFi.disconnect(true);
  delay(100);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
  WiFi.setSleep(WIFI_PS_MAX_MODEM);
  WiFi.persistent(false); // kredensial hanya di NVS terenkripsi aplikasi, bukan nvs.net80211
  WiFi.setAutoReconnect(true);
  WiFi.begin(wifiCreds[ssidIdx].ssid, wifiCreds[ssidIdx].pass);
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
      isOnline = false; // tanpa WiFi, server pasti tak terjangkau
      unsigned long elapsed = millis() - timers.lastReconnect;
      if (elapsed >= RECONNECT_INTERVAL)
      {
        timers.lastReconnect = millis();
        reconnectState = RECONNECT_INIT;
      }
    }
    break;
  case RECONNECT_INIT:
    WiFi.disconnect(true);
    delay(100);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.setSleep(WIFI_PS_MAX_MODEM);
    {
      int nextIdx = (currentSsidIdx + 1) % 3;
      for (int i = 0; i < 3; i++)
      {
        int idx = (nextIdx + i) % 3;
        if (strlen(wifiCreds[idx].ssid) > 0)
        {
          WiFi.enableIPv6(false);
          WiFi.begin(wifiCreds[idx].ssid, wifiCreds[idx].pass);
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
    resetPollBackoff();       // jaringan baru pulih: kembali ke interval normal
    timers.lastTelemetry = 0; // heartbeat segera, agar status server cepat terisi
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
void uidToString(uint8_t *uid, uint8_t len, char *out)
{
  if (len >= 4)
  {
    uint32_t v = ((uint32_t)uid[3] << 24) | ((uint32_t)uid[2] << 16) | ((uint32_t)uid[1] << 8) | uid[0];
    sprintf(out, "%010lu", v);
  }
  else
  {
    sprintf(out, "%02X%02X", uid[0], uid[1]);
  }
}
void urlEncode(const char *src, char *dst, size_t dstSize)
{
  size_t di = 0;
  for (size_t i = 0; src[i] != '\0' && di < dstSize - 4; i++)
  {
    char c = src[i];
    if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~')
    {
      dst[di++] = c;
    }
    else
    {
      snprintf(&dst[di], 4, "%%%02X", (unsigned char)c);
      di += 3;
    }
  }
  dst[di] = '\0';
}
bool kirimLangsung(const char *rfid, const char *ts, char *msg)
{
  if (!isWifiConnected())
    return false;
  WiFiClientSecure client;
  applyTls(client);
  HTTPClient http;
  http.setTimeout(4000);
  http.setConnectTimeout(2000);
  char url[API_URL_BUF];
  if (!buildApiUrl(url, sizeof(url), "/api/presensi"))
  {
    strcpy(msg, "SERVER ERR 0");
    return false;
  }
  if (!http.begin(client, url))
  {
    strcpy(msg, "SERVER ERR 0");
    return false;
  }
  http.addHeader(F("Content-Type"), F("application/json"));
  http.addHeader(F("X-API-KEY"), apiKey);
  char payload[128];
  snprintf(payload, sizeof(payload),
           "{\"rfid\":\"%s\",\"timestamp\":\"%s\",\"device_id\":\"%s\",\"sync_mode\":false}",
           rfid, ts, deviceId);
  int code = http.POST(payload);
  String body = (code == 400) ? http.getString() : String();
  http.end();
  if (code == 200)
  {
    strcpy(msg, "PRESENSI OK");
    return true;
  }
  if (code == 400)
  {
    JsonDocument d;
    const char *c = "";
    if (deserializeJson(d, body) == DeserializationError::Ok)
      c = d["code"] | "";
    if (!strcmp(c, "TS_FUTURE") || !strcmp(c, "TS_TOO_OLD"))
    {
      strcpy(msg, "JAM DEVICE SALAH");
      timers.lastTimeSync = millis() - TIME_SYNC_INTERVAL; // jatuh tempo sekarang, aman terhadap wrap
      return false;
    }
    if (!strcmp(c, "TS_EXPIRED"))
    {
      strcpy(msg, "DATA KADALUARSA");
      return false;
    }
    strcpy(msg, "CUKUP SEKALI!");
    return false;
  }
  if (code == 403)
  {
    strcpy(msg, "HARI LIBUR!");
    return false;
  }
  if (code == 404)
  {
    strcpy(msg, "RFID NONAKTIF");
    return false;
  }
  snprintf(msg, 32, "SERVER ERR %d", code);
  return false;
}
bool isDuplicateScanRecent(const char *rfid, unsigned long t, const char *sourceTag)
{
  bool dup = nvsIsRecentScan(rfid, t);
  return dup;
}

bool kirimPresensi(const char *rfid, char *msg)
{
  msg[0] = '\0';
  if (!isTimeValid())
  {
    strcpy(msg, "WAKTU INVALID");
    return false;
  }
  char ts[20];
  getFormattedTimestamp(ts, sizeof(ts));
  time_t now = 0;
  bool nowValid = getEpochWithFallback(&now);
  if (!nowValid)
  {
    strcpy(msg, "WAKTU INVALID");
    return false;
  }
#if REJECT_SCAN_UNTRUSTED_TIME
  if (!isTimeTrusted())
  {
    strcpy(msg, "WAKTU BELUM SYNC");
    return false;
  }
#endif
  const char *pathTag = sdCardAvailable ? "SD" : (isWifiConnected() ? "DIRECT_HTTP" : "OFFLINE_BUFFER");
  if (isDuplicateScanRecent(rfid, (unsigned long)now, pathTag))
  {
    strcpy(msg, "CUKUP SEKALI!");
    return false;
  }
  if (sdCardAvailable)
  {
    RfidLookup lk = rfidLookupCache(rfid);
    if (lk == RFID_BUSY)
    {
      strcpy(msg, "CACHE SIBUK");
      return false;
    }
    if (lk == RFID_NOT_FOUND)
    {
      strcpy(msg, "RFID NONAKTIF");
      return false;
    }
    SaveResult r = saveToQueue(rfid, ts, (unsigned long)now);
    switch (r)
    {
    case SAVE_OK:
      nvsBumpScanCount();
      nvsSaveLastScan(rfid, (unsigned long)now);
      strcpy(msg, cachedQueueFileCount >= QUEUE_WARN_THRESHOLD ? "QUEUE HAMPIR PENUH!" : "DATA TERSIMPAN");
      return true;
    case SAVE_DUPLICATE:
      strcpy(msg, "CUKUP SEKALI!");
      return false;
    case SAVE_QUEUE_FULL:
      strcpy(msg, "QUEUE PENUH!");
      return false;
    default:                                                   // SAVE_SD_ERROR: lanjut ke HTTP/NVS
      timers.lastSDRedetect = millis() - SD_REDETECT_INTERVAL; // cek kesehatan SD jatuh tempo sekarang
      break;
    }
  }
  if (isWifiConnected())
  {
    if (kirimLangsung(rfid, ts, msg))
    {
      nvsBumpScanCount();
      nvsSaveLastScan(rfid, (unsigned long)now);
      return true;
    }
    if (strcmp(msg, "CUKUP SEKALI!") == 0 || strcmp(msg, "RFID NONAKTIF") == 0 ||
        strcmp(msg, "HARI LIBUR!") == 0 || strcmp(msg, "JAM DEVICE SALAH") == 0 ||
        strcmp(msg, "DATA KADALUARSA") == 0)
    {
      return false;
    }
  }
  if (nvsIsDuplicate(rfid, (unsigned long)now))
  {
    strcpy(msg, "CUKUP SEKALI!");
    return false;
  }
  if (nvsSaveToBuffer(rfid, ts, (unsigned long)now))
  {
    nvsBumpScanCount();
    nvsSaveLastScan(rfid, (unsigned long)now);
    snprintf(msg, 32, "BUFFER %d/%d", nvsGetCount(), NVS_MAX_RECORDS);
    return true;
  }
  strcpy(msg, "BUFFER PENUH!");
  return false;
}
bool displayStateChanged()
{
  return currentDisplay.isOnline != previousDisplay.isOnline || currentDisplay.pendingRecords != previousDisplay.pendingRecords || currentDisplay.wifiSignal != previousDisplay.wifiSignal || strncmp(currentDisplay.time, previousDisplay.time, 5) != 0;
}
void updateCurrentDisplayState()
{
  currentDisplay.isOnline = isWifiConnected() && isOnline;
  struct tm ti;
  if (!isTimeTrusted())
    strcpy(currentDisplay.time, "--:--"); // penanda: jam belum tepercaya
  else if (getTimeWithFallback(&ti))
    snprintf(currentDisplay.time, sizeof(currentDisplay.time), "%02d:%02d", ti.tm_hour, ti.tm_min);
  if (pendingCacheDirty)
    refreshPendingCache();
  currentDisplay.pendingRecords = cachedPendingRecords + nvsGetCount();
  if (currentDisplay.pendingRecords < 0)
    currentDisplay.pendingRecords = 0;
  if (isWifiConnected())
  {
    long r = WiFi.RSSI();
    currentDisplay.wifiSignal = r > -67 ? 4 : r > -70 ? 3
                                          : r > -80   ? 2
                                          : r > -90   ? 1
                                                      : 0;
  }
  else
  {
    currentDisplay.wifiSignal = 0;
  }
}
void updateStandbyDisplay()
{
  if (!oledIsOn)
    return;
  if (!displayStateChanged())
    return;
  if (xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(50)) != pdTRUE)
    return;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(2, 2);
  if (reconnectState == RECONNECT_INIT || reconnectState == RECONNECT_TRYING)
  {
    display.print(F("CONNECTING"));
    for (int i = 0; i < (int)((millis() / 500) % 4); i++)
      display.print('.');
  }
  else if (syncState.inProgress)
  {
    display.print(F("SYNCING"));
    for (int i = 0; i < (int)((millis() / 500) % 4); i++)
      display.print('.');
  }
  else
  {
    display.print(currentDisplay.isOnline ? F("ONLINE") : F("OFFLINE"));
  }
  const char *tap = rfidReady ? "TAP KARTU" : "RC522 RUSAK";
  int16_t x, y;
  uint16_t w, h;
  display.getTextBounds(tap, 0, 0, &x, &y, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 20);
  display.print(tap);
  display.getTextBounds(currentDisplay.time, 0, 0, &x, &y, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, 35);
  display.print(currentDisplay.time);
  if (currentDisplay.pendingRecords > 0)
  {
    char buf[16];
    snprintf(buf, sizeof(buf), "Q:%d", currentDisplay.pendingRecords);
    display.getTextBounds(buf, 0, 0, &x, &y, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, 50);
    display.print(buf);
  }
  if (currentDisplay.wifiSignal > 0)
  {
    for (int i = 0; i < 4; i++)
    {
      int bh = 2 + i * 2, bx = SCREEN_WIDTH - 18 + i * 5;
      if (i < currentDisplay.wifiSignal)
        display.fillRect(bx, 10 - bh, 3, bh, WHITE);
      else
        display.drawRect(bx, 10 - bh, 3, bh, WHITE);
    }
  }
  display.display();
  xSemaphoreGive(xDisplayMutex);
  memcpy(&previousDisplay, &currentDisplay, sizeof(DisplayState));
}

void showStartupAnimation()
{
  static const char title[] = "ZEDLABS";
  static const char sub1[] = "INNOVATE BEYOND";
  static const char sub2[] = "LIMITS";
  static const char version[] = "v" FIRMWARE_VERSION;
  const int tX = (SCREEN_WIDTH - 7 * 12) / 2;
  const int s1X = (SCREEN_WIDTH - 15 * 6) / 2;
  const int s2X = (SCREEN_WIDTH - 6 * 6) / 2;
  const int vX = (SCREEN_WIDTH - (int)strlen(version) * 6) / 2;
  display.clearDisplay();
  display.setTextColor(WHITE);
  for (int x = -80; x <= tX; x += 4)
  {
    esp_task_wdt_reset();
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(x, 5);
    display.println(title);
    display.setTextSize(1);
    display.setCursor(s1X, 30);
    display.println(sub1);
    display.setCursor(s2X, 40);
    display.println(sub2);
    display.display();
    delay(30);
  }
  esp_task_wdt_reset();
  delay(300);
  display.setTextSize(1);
  display.setCursor(vX, 55);
  display.print(version);
  display.display();
  for (int i = 0; i < 3; i++)
  {
    delay(300);
    display.print('.');
    display.display();
  }
  esp_task_wdt_reset();
  delay(500);
}

// Hapus semua queue_N.csv. Mutex SD diambil per file agar task lain tidak terblokir lama.
// Memakai findNextQueueFileLocked() karena tahan terhadap celah indeks.
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
  bool armed = false;     // sudah melewati 5 detik
  bool wipeQueue = false; // sudah melewati 10 detik
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
    // Dilepas sebelum 5 detik: batal.
    memset(previousDisplay.time, 0xFF, sizeof(previousDisplay.time));
    return;
  }

  showOLED(F("FACTORY RESET"), "MENGHAPUS...");
  playToneError();
  delay(500);
  // Hapus kredensial WiFi lama yang mungkin tersimpan driver (firmware sebelumnya memakai persistent(true)).
  WiFi.mode(WIFI_STA);
  esp_wifi_restore();

  // Bersihkan NVS di bawah mutex agar tidak bertabrakan dengan task lain.
  // Catatan: buffer offline NVS (rec_N) ikut terhapus di sini karena satu namespace.
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
  // Indeks file antrean di RTC memory tidak boleh dipercaya lagi; boot berikutnya memindai ulang.
  rtcQueueFileValid = false;
  currentQueueFile = 0;

  showOLED(F("RESET SELESAI"), "RESTART...");
  delay(2000);
  ESP.restart();
}

static String provHtmlPage()
{
  String html;
  html.reserve(7500);
  html += F("<!DOCTYPE html><html lang='id'><head>"
            "<meta charset='utf-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1'>"
            "<title>Device Setup - Attendance Machine</title>"
            "<style>"
            "*{box-sizing:border-box;margin:0;padding:0}"
            "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;"
            "background:#0a0f1e;min-height:100vh;display:flex;flex-direction:column;"
            "align-items:center;justify-content:center;padding:16px}"
            ".card{background:#111827;border:1px solid #1f2937;border-radius:20px;"
            "width:100%;max-width:480px;overflow:hidden;box-shadow:0 25px 50px rgba(0,0,0,.5)}"
            ".header{padding:32px 32px 28px;border-bottom:1px solid #1f2937;"
            "background:linear-gradient(160deg,#111827 0%,#0f172a 100%)}"
            ".header-title h1{font-size:26px;font-weight:800;letter-spacing:-.5px;"
            "background:linear-gradient(90deg,#f8fafc,#94a3b8);"
            "-webkit-background-clip:text;-webkit-text-fill-color:transparent}"
            ".header-sub{color:#475569;font-size:12px;letter-spacing:2px;"
            "text-transform:uppercase;font-weight:500;margin-top:4px}"
            ".body{padding:28px 32px}"
            ".section{margin-bottom:22px}"
            ".section-label{color:#374151;font-size:10px;font-weight:700;"
            "letter-spacing:2px;text-transform:uppercase;margin-bottom:12px;"
            "display:flex;align-items:center;gap:10px}"
            ".section-label::after{content:'';flex:1;height:1px;background:#1f2937}"
            ".field{margin-bottom:10px}"
            "label{display:block;color:#6b7280;font-size:12px;font-weight:500;margin-bottom:5px}"
            "input,select{width:100%;background:#0a0f1e;border:1px solid #1f2937;border-radius:8px;"
            "color:#e5e7eb;font-size:14px;padding:10px 14px;outline:none;"
            "transition:border .2s,box-shadow .2s}"
            "input:focus,select:focus{border-color:#1d4ed8;box-shadow:0 0 0 3px rgba(29,78,216,.15)}"
            "input::placeholder{color:#374151}"
            "select option{background:#111827}"
            ".row{display:grid;grid-template-columns:1fr 1fr;gap:10px}"
            ".row4{display:grid;grid-template-columns:1fr 1fr 1fr 1fr;gap:8px}"
            ".hint{color:#4b5563;font-size:11px;margin-top:4px}"
            ".btn{width:100%;background:#1d4ed8;color:#fff;border:none;border-radius:10px;"
            "padding:13px;font-size:14px;font-weight:600;cursor:pointer;margin-top:4px;"
            "letter-spacing:.5px;transition:background .2s,transform .1s;"
            "display:flex;align-items:center;justify-content:center;gap:8px}"
            ".btn:hover{background:#2563eb}.btn:active{transform:scale(.99)}"
            ".footer{text-align:center;padding:16px 32px;border-top:1px solid #1f2937}"
            ".footer p{color:#374151;font-size:11px;line-height:1.8}"
            ".footer strong{color:#4b5563}"
            "</style></head><body>"
            "<div class='card'>"
            "<div class='header'>"
            "<div class='header-title'><h1>DEVICE SETUP</h1></div>"
            "<div class='header-sub'>Attendance Machine</div>"
            "</div>"
            "<div class='body'><form method='POST' action='/save'>"
            "<div class='section'><div class='section-label'>WiFi Utama</div>"
            "<div class='row'>"
            "<div class='field'><label>SSID</label>"
            "<input name='ssid1' placeholder='Nama Jaringan' required></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='pass1' placeholder='&#9679;&#9679;&#9679;&#9679;&#9679;&#9679;'></div>"
            "</div></div>"
            "<div class='section'><div class='section-label'>WiFi Cadangan 1</div>"
            "<div class='row'>"
            "<div class='field'><label>SSID</label>"
            "<input name='ssid2' placeholder='Nama Jaringan'></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='pass2' placeholder='&#9679;&#9679;&#9679;&#9679;&#9679;&#9679;'></div>"
            "</div></div>"
            "<div class='section'><div class='section-label'>WiFi Cadangan 2</div>"
            "<div class='row'>"
            "<div class='field'><label>SSID</label>"
            "<input name='ssid3' placeholder='Nama Jaringan'></div>"
            "<div class='field'><label>Password</label>"
            "<input type='password' name='pass3' placeholder='&#9679;&#9679;&#9679;&#9679;&#9679;&#9679;'></div>"
            "</div></div>"
            "<div class='section'><div class='section-label'>Konfigurasi Perangkat</div>"
            "<div class='field'><label>API URL Backend</label>"
            "<input name='apiurl' placeholder='https://domain.sch.id' value='" DEFAULT_API_URL "' required></div>"
            "<div class='hint'>Tanpa trailing slash. Contoh: https://presensi.sekolah.sch.id</div>"
            "<div class='field' style='margin-top:10px'><label>API Key</label>"
            "<input name='apikey' placeholder='Masukkan API Key' required></div>"
            "<div class='field'><label>Nama Perangkat</label>"
            "<input name='devname' placeholder='Contoh: GERBANG UTAMA' maxlength='19' "
            "pattern='[A-Za-z0-9 ._-]*' title='Huruf, angka, spasi, - _ .'></div>"
            "</div>"
            "<div class='section'><div class='section-label'>Jadwal Sleep Mode</div>"
            "<div class='row'>"
            "<div class='field'><label>Mulai Sleep (jam)</label>"
            "<select name='slp_s'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == SLEEP_START_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "<div class='field'><label>Selesai Sleep (jam)</label>"
            "<select name='slp_e'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == SLEEP_END_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "</div>"
            "<p class='hint'>Perangkat akan deep sleep dari jam Mulai hingga jam Selesai. "
            "Jika Mulai sama dengan Selesai, sleep dinonaktifkan.</p>"
            "</div>"
            "<div class='section'><div class='section-label'>Jadwal Dim OLED</div>"
            "<div class='row'>"
            "<div class='field'><label>Mulai Dim (jam)</label>"
            "<select name='dim_s'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == OLED_DIM_START_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "<div class='field'><label>Selesai Dim (jam)</label>"
            "<select name='dim_e'>");
  for (int h = 0; h <= 23; h++)
  {
    html += "<option value='";
    html += h;
    html += "'";
    if (h == OLED_DIM_END_HOUR_DEFAULT)
      html += " selected";
    html += ">";
    if (h < 10)
      html += "0";
    html += h;
    html += ":00</option>";
  }
  html += F("</select></div>"
            "</div>"
            "<p class='hint'>OLED akan dimatikan sementara pada rentang jam tersebut. "
            "Jika Mulai sama dengan Selesai, dim dinonaktifkan.</p>"
            "</div>"
            "<button type='submit' class='btn'>Simpan &amp; Restart</button>"
            "</form></div>"
            "<div class='footer'>"
            "<p><strong>&copy; 2022 - <script>document.write(new Date().getFullYear())</script>"
            " ZEDLABS TEKNOLOGI INDONESIA</strong></p>"
            "<p>Attendance Machine <strong>v" FIRMWARE_VERSION "</strong></p>"
            "</div></div></body></html>");
  return html;
}
// Password AP acak setiap masuk mode setup, ditampilkan di OLED. Tidak bisa ditebak dari MAC.
static void deriveProvisioningPassword(char *out, size_t outSz)
{
  static const char cs[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // 32 karakter, tanpa yang mirip
  const size_t n = 10;
  if (outSz < n + 1)
  {
    if (outSz > 0)
      out[0] = '\0';
    return;
  }
  for (size_t i = 0; i < n; i++)
    out[i] = cs[esp_random() % 32];
  out[n] = '\0';
}

void startProvisioningMode()
{
  deriveProvisioningPassword(provApPassword, sizeof(provApPassword));
  showOLED(F("PROVISIONING"), PROV_AP_SSID);
  delay(1500);
  showOLED(F("AP PASSWORD"), provApPassword);
  delay(2500);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(PROV_AP_SSID, provApPassword);
  dnsServer.start(PROV_DNS_PORT, "*", WiFi.softAPIP());
  provServer.on("/", HTTP_GET, []()
                { provServer.send(200, "text/html", provHtmlPage()); });
  provServer.on("/save", HTTP_POST, []()
                {
    String s1 = provServer.arg("ssid1"), p1 = provServer.arg("pass1");
    String s2 = provServer.arg("ssid2"), p2 = provServer.arg("pass2");
    String s3 = provServer.arg("ssid3"), p3 = provServer.arg("pass3");
    String ak = provServer.arg("apikey");
    String dn = provServer.arg("devname");
    String aurl = provServer.arg("apiurl");
    String slpS = provServer.arg("slp_s");
    String slpE = provServer.arg("slp_e");
    String dimS = provServer.arg("dim_s");
    String dimE = provServer.arg("dim_e");
    if (s1.length() == 0 || ak.length() == 0 || aurl.length() == 0) {
      provServer.send(400, "text/plain", "SSID 1, API Key, dan API URL wajib diisi.");
      return;
    }
    if (!aurl.startsWith("https://"))
    {
      provServer.send(400, "text/plain", "API URL harus diawali https://");
      return;
    }
    while (aurl.endsWith("/")) aurl.remove(aurl.length() - 1);
    if (aurl.length() > 79) {
      provServer.send(400, "text/plain", "API URL terlalu panjang (max 79 karakter).");
      return;
    }
    if (s1.length() > 32 || s2.length() > 32 || s3.length() > 32)
    {
      provServer.send(400, "text/plain", "SSID maksimal 32 karakter.");
      return;
    }
    if (p1.length() > 63 || p2.length() > 63 || p3.length() > 63)
    {
      provServer.send(400, "text/plain", "Password WiFi maksimal 63 karakter.");
      return;
    }
    if (ak.length() > CRED_PLAIN_MAX)
    {
      provServer.send(400, "text/plain", "API Key terlalu panjang.");
      return;
    }
    if (dn.length() > DEVICE_NAME_MAX_LEN)
    {
      provServer.send(400, "text/plain", "Nama perangkat maksimal 19 karakter.");
      return;
    }
    for (size_t i = 0; i < dn.length(); i++)
    {
      if (!isSafeDeviceNameChar(dn[i]))
      {
        provServer.send(400, "text/plain", "Nama perangkat hanya boleh huruf, angka, spasi, '-', '_' dan '.'.");
        return;
      }
    }
    // Kosong = default. Selain angka 0-23 ditolak, bukan diganti default diam-diam.
    bool hourErr = false;
    auto parseHour = [&hourErr](const String &s, int def) -> int
    {
      if (s.length() == 0)
        return def;
      for (size_t i = 0; i < s.length(); i++)
      {
        if (!isdigit((unsigned char)s[i]))
        {
          hourErr = true;
          return def;
        }
      }
      int v = s.toInt();
      if (v < 0 || v > 23)
      {
        hourErr = true;
        return def;
      }
      return v;
    };
    int iSlpS = parseHour(slpS, SLEEP_START_HOUR_DEFAULT);
    int iSlpE = parseHour(slpE, SLEEP_END_HOUR_DEFAULT);
    int iDimS = parseHour(dimS, OLED_DIM_START_HOUR_DEFAULT);
    int iDimE = parseHour(dimE, OLED_DIM_END_HOUR_DEFAULT);
    if (hourErr)
    {
      provServer.send(400, "text/plain", "Jam sleep/dim harus angka 0-23.");
      return;
    }
    bool saved = saveCredential(NVS_KEY_SSID1, s1.c_str());
    saved = saveCredential(NVS_KEY_PASS1, p1.c_str()) && saved;
    saved = saveCredential(NVS_KEY_SSID2, s2.c_str()) && saved;
    saved = saveCredential(NVS_KEY_PASS2, p2.c_str()) && saved;
    saved = saveCredential(NVS_KEY_SSID3, s3.c_str()) && saved;
    saved = saveCredential(NVS_KEY_PASS3, p3.c_str()) && saved;
    saved = saveCredential(NVS_KEY_APIKEY, ak.c_str()) && saved;
    saved = saveCredential(NVS_KEY_DEVNAME, dn.c_str()) && saved;
    saved = saveCredential(NVS_KEY_APIURL, aurl.c_str()) && saved;
    if (!saved)
    {
      provServer.send(500, "text/plain", "Gagal menyimpan konfigurasi. Periksa panjang input lalu coba lagi.");
      return;
    }
    prefs.begin(NVS_NS_CONFIG, false);
    prefs.putInt(NVS_KEY_CFG_SLP_S, iSlpS);
    prefs.putInt(NVS_KEY_CFG_SLP_E, iSlpE);
    prefs.putInt(NVS_KEY_CFG_DIM_S, iDimS);
    prefs.putInt(NVS_KEY_CFG_DIM_E, iDimE);
    prefs.end();
    markProvisioned();
    provServer.send(200, "text/html",
                    "<html><body style='font-family:sans-serif;text-align:center;padding:40px'>"
                    "<h2>&#10003; Tersimpan!</h2><p>Device akan restart dalam 2 detik...</p>"
                    "</body></html>");
    delay(2000);
    ESP.restart(); });
  provServer.onNotFound([]()
                        { provServer.send(200, "text/html", provHtmlPage()); });
  provServer.begin();
  unsigned long t0 = millis();
  while (millis() - t0 < PROVISIONING_TIMEOUT_MS)
  {
    dnsServer.processNextRequest();
    provServer.handleClient();
    esp_task_wdt_reset();
    delay(10);
  }
  showOLED(F("TIMEOUT"), "RESTART...");
  delay(2000);
  ESP.restart();
}
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
      __asm__ volatile("" ::: "memory"); // `active` ditulis terakhir
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
    // Diambil SETELAH processReconnect(): ia mengisi timers.lastSync = millis(),
    // dan (now - lastSync) akan underflow bila now lebih lama.
    unsigned long now = millis();
    if (isWifiConnected())
    {
      bool force = forceSyncRequested;
      if (force)
        forceSyncRequested = false;
      if (!isSignalWeak())
      {
        checkOtaUpdate();
        checkAndUpdateRfidDb();
        if (otaState.updateAvailable && !rfidFeedback.active)
          performOtaUpdate();
        sendTelemetry();
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
    // Selisih bertanda: shownAt bisa lebih baru dari `now` bila taskRfid menyela.
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
  Serial.printf("[FW] %s\n", FW_MARKER); // sengaja BUKAN LOG_I: referensi runtime marker, tetap ada saat LOG_LEVEL=0
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
    rfidReady = false; // mode terdegradasi: WiFi, OTA, sync antrean tetap jalan
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
      int sleepSec = calcSleepSeconds(cfg); // dihitung ulang tepat sebelum tidur
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
