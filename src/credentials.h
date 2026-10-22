#pragma once
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
  char ssid[33];
  char pass[64];
};

static WifiCredential wifiCreds[3];

static char apiKey[CRED_DATA_MAX] = "";

static char apiBaseUrl[80] = DEFAULT_API_URL;

#define API_URL_BUF 160

static bool buildApiUrl(char *out, size_t outSz, const char *path)
{
  int n = snprintf(out, outSz, "%s%s", apiBaseUrl, path);
  return n > 0 && (size_t)n < outSz;
}

static void urlEncode(const char *src, char *dst, size_t dstSize);

static bool isSafeDeviceNameChar(char c)
{
  return isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_' || c == '.';
}

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
  auto clampInterval = [](unsigned long v, unsigned long def, unsigned long minMs)
  {
    return (v >= minMs && v <= CFG_INTERVAL_MAX_MS) ? v : def;
  };
  rtCfg.sleepStartHour = clampHour(slpS, SLEEP_START_HOUR_DEFAULT);
  rtCfg.sleepEndHour = clampHour(slpE, SLEEP_END_HOUR_DEFAULT);
  rtCfg.dimStartHour = clampHour(dimS, OLED_DIM_START_HOUR_DEFAULT);
  rtCfg.dimEndHour = clampHour(dimE, OLED_DIM_END_HOUR_DEFAULT);
  rtCfg.syncIntervalMs = clampInterval(syncIv, SYNC_INTERVAL, CFG_INTERVAL_MIN_MS);
  rtCfg.otaCheckIntervalMs = clampInterval(otaIv, OTA_CHECK_INTERVAL, CFG_OTA_INTERVAL_MIN_MS);
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
