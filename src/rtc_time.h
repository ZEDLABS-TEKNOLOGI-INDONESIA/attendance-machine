#pragma once
static uint8_t bcd2bin(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

static uint8_t bin2bcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

static bool rtcBusLock()
{
  if (!xDisplayMutex || xSemaphoreTake(xDisplayMutex, pdMS_TO_TICKS(300)) != pdTRUE)
    return false;
  Wire.setClock(100000);
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
  if (r[0] & 0x80)
    return false;
  if (r[2] & 0x40)
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
  Wire.write((uint8_t)(bin2bcd((uint8_t)g.tm_sec) & 0x7F));
  Wire.write(bin2bcd((uint8_t)g.tm_min));
  Wire.write(bin2bcd((uint8_t)g.tm_hour));
  Wire.write(bin2bcd((uint8_t)(g.tm_wday + 1)));
  Wire.write(bin2bcd((uint8_t)g.tm_mday));
  Wire.write(bin2bcd((uint8_t)(g.tm_mon + 1)));
  Wire.write(bin2bcd((uint8_t)(g.tm_year % 100)));
  bool ok = (Wire.endTransmission() == 0);
  rtcBusUnlock();
  return ok;
}

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

bool getEpochWithFallback(time_t *out)
{
  time_t now = time(nullptr);
  if (now >= MIN_VALID_EPOCH)
  {
    *out = now;
    return true;
  }
  if (!timeWasSynced || lastValidTime == 0 || !bootTimeSet)
    return false;

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

bool isTimeTrusted()
{
  if (time(nullptr) >= MIN_VALID_EPOCH)
    return true;
  return !timeFromNvsOnly && isTimeValid();
}

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
    configTzTime(TZ_POSIX, srv);
    unsigned long t0 = millis();
    while (millis() - t0 < NTP_SERVER_TIMEOUT_MS)
    {
      esp_task_wdt_reset();
      if (sntpSyncCount != before)
      {
        time_t now = time(nullptr);
        if (now < MIN_VALID_EPOCH)
          break;
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
