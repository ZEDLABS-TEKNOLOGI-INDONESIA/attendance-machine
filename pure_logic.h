#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifndef CRC8_POLY
#define CRC8_POLY 0x07
#endif

#ifndef BACKOFF_MAX_SHIFT
#define BACKOFF_MAX_SHIFT 4
#endif

#ifndef BACKOFF_MAX_INTERVAL_MS
#define BACKOFF_MAX_INTERVAL_MS 3600000UL
#endif

#ifndef MAX_QUEUE_FILES
#define MAX_QUEUE_FILES 60000
#endif

struct OtaVerScan
{
    uint8_t st = 0;
    uint8_t vl = 0;
    bool found = false;
    char ver[16] = "";
};

static inline uint8_t crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x80) ? ((crc << 1) ^ CRC8_POLY) : (crc << 1);
    }
    return crc;
}

static inline uint8_t recordCrc8(const char *rfid, uint32_t t)
{
    uint8_t buf[14] = {0};
    size_t n = strnlen(rfid, 10);
    memcpy(buf, rfid, n);
    buf[10] = (t >> 24) & 0xFF;
    buf[11] = (t >> 16) & 0xFF;
    buf[12] = (t >> 8) & 0xFF;
    buf[13] = (t) & 0xFF;
    return crc8(buf, 14);
}

static inline uint32_t backoffInterval(uint32_t base, uint8_t fails)
{
    uint8_t s = (fails > BACKOFF_MAX_SHIFT) ? BACKOFF_MAX_SHIFT : fails;
    uint32_t cap = (base > BACKOFF_MAX_INTERVAL_MS) ? base : (uint32_t)BACKOFF_MAX_INTERVAL_MS;
    if (base > (cap >> s))
        return cap;
    return base << s;
}

static inline bool isInWindow(int h, int start, int end)
{
    if (start == end)
        return false;
    if (start < end)
        return h >= start && h < end;
    return h >= start || h < end;
}

static inline int compareFirmwareVersion(const char *a, const char *b)
{
    int aMaj = 0, aMin = 0, aPat = 0;
    int bMaj = 0, bMin = 0, bPat = 0;
    sscanf(a, "%d.%d.%d", &aMaj, &aMin, &aPat);
    sscanf(b, "%d.%d.%d", &bMaj, &bMin, &bPat);
    if (aMaj != bMaj)
        return (aMaj < bMaj) ? -1 : 1;
    if (aMin != bMin)
        return (aMin < bMin) ? -1 : 1;
    if (aPat != bPat)
        return (aPat < bPat) ? -1 : 1;
    return 0;
}

static inline bool isValidMd5Hex(const char *s)
{
    if (s == nullptr || strlen(s) != 32)
        return false;
    for (int i = 0; i < 32; i++)
        if (!isxdigit((unsigned char)s[i]))
            return false;
    return true;
}

static inline int64_t daysFromCivil(int y, unsigned m, unsigned d)
{
    y -= (m <= 2);
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + (int64_t)doe - 719468;
}

static inline bool parseQueueIndex(const char *name, int *idx)
{
    int n = -1;
    if (sscanf(name, "queue_%d.csv", &n) != 1 || n < 0 || n >= MAX_QUEUE_FILES)
        return false;
    char expect[24];
    snprintf(expect, sizeof(expect), "queue_%d.csv", n);
    if (strcmp(expect, name) != 0)
        return false;
    *idx = n;
    return true;
}

static inline void otaVerFeed(OtaVerScan &s, const uint8_t *d, size_t n)
{
    static const char pre[] = "FWVER:";
    for (size_t i = 0; i < n && !s.found; i++)
    {
        char c = (char)d[i];
        if (s.st < 6)
        {
            if (c == pre[s.st])
                s.st++;
            else
                s.st = (c == 'F') ? 1 : 0;
            s.vl = 0;
            continue;
        }
        if (c == ';')
        {
            s.ver[s.vl] = '\0';
            int a, b, p;
            if (s.vl > 0 && sscanf(s.ver, "%d.%d.%d", &a, &b, &p) == 3)
                s.found = true;
            else
            {
                s.st = 0;
                s.vl = 0;
            }
        }
        else if ((isdigit((unsigned char)c) || c == '.') && s.vl < sizeof(s.ver) - 1)
            s.ver[s.vl++] = c;
        else
        {
            s.st = 0;
            s.vl = 0;
        }
    }
}
