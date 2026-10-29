#pragma once
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
#define MAX_OFFLINE_AGE 2592000UL
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
#define CFG_INTERVAL_MIN_MS 60000UL
#define CFG_OTA_INTERVAL_MIN_MS 300000UL
#define CFG_INTERVAL_MAX_MS 86400000UL
#define RFID_DB_CHECK_INTERVAL 300000UL
#define TELEMETRY_INTERVAL 120000UL
#define REMOTE_CONFIG_INTERVAL 600000UL
#define FACTORY_RESET_HOLD_MS 5000UL
#define FACTORY_RESET_WIPE_QUEUE_MS 10000UL
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
#define FIRMWARE_VERSION "2.3.9"
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
#define RTC_I2C_ADDR 0x68
#define RTC_MIN_VALID_YEAR 2025
#define MIN_VALID_EPOCH 1735689600L
#define NTP_SERVER_TIMEOUT_MS 3000UL
#define TZ_POSIX "WIB-7"
#ifndef LOG_LEVEL
#define LOG_LEVEL 3
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
#define SERIAL_TX_NONBLOCK 0
#define MAX_RC522_RESTARTS 3
#define SLEEP_PARK_TIMEOUT_MS 60000UL
#define SLEEP_RETRY_MS 30000UL
#define SLEEP_GPIO_HOLD 0
#define NVS_KEY_BOOT_CNT "boot_cnt"
#define NVS_KEY_ABN_CNT "abn_cnt"
#define NVS_KEY_LAST_ABN "last_abn"
#define NVS_KEY_RC_FAIL "rc_fail"
#define NVS_KEY_OTA_TRY "ota_try"
#define NVS_KEY_OTA_BAD "ota_bad"
#define REJECT_SCAN_UNTRUSTED_TIME 1
#define UNTRUSTED_TIME_RETRY_INTERVAL 60000UL
#define CRED_DATA_MAX 96
#define CRED_PLAIN_MAX (CRED_DATA_MAX - 1)
#define DEFAULT_API_URL ""
#define QUEUE_HEADER_LINE "rfid,timestamp,device_id,unix_time,crc8"
#define QUARANTINE_FILE "/quarantine.csv"
#define QUARANTINE_OLD "/quarantine.old"
#define QUARANTINE_MAX_BYTES 262144UL
#define LINE_OK 0
#define LINE_EMPTY 1
#define LINE_MALFORMED 2
#define LINE_BAD_CRC 3
#define LINE_EXPIRED 4
