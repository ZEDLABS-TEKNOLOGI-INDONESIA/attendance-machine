#include "../pure_logic.h"
static int fails = 0, total = 0;
#define CHECK(c)                                                \
    do                                                          \
    {                                                           \
        total++;                                                \
        if (!(c))                                               \
        {                                                       \
            fails++;                                            \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); \
        }                                                       \
    } while (0)

static void feed(OtaVerScan &s, const char *p, size_t n) { otaVerFeed(s, (const uint8_t *)p, n); }

int main()
{
    // crc8: vektor baku CRC-8 poly 0x07
    CHECK(crc8((const uint8_t *)"123456789", 9) == 0xF4);
    uint8_t ref[14] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 0x69, 0x1A, 0x2B, 0x3C};
    CHECK(recordCrc8("1234567890", 0x691A2B3Cu) == crc8(ref, 14));
    uint8_t ref2[14] = {'A', 'B', 'C', 'D'}; // UID pendek: sisa nol
    ref2[10] = 0;
    ref2[11] = 0;
    ref2[12] = 0;
    ref2[13] = 5;
    CHECK(recordCrc8("ABCD", 5) == crc8(ref2, 14));

    // backoff: 10 mnt -> 10,20,40,60,60
    const uint32_t b[] = {600000, 1200000, 2400000, 3600000, 3600000, 3600000, 3600000};
    for (int f = 0; f < 7; f++)
        CHECK(backoffInterval(600000u, f) == b[f]);
    CHECK(backoffInterval(5000u, 4) == 80000u);
    CHECK(backoffInterval(5000u, 200) == 80000u);
    CHECK(backoffInterval(4000000000u, 4) == 4000000000u); // tanpa overflow 32-bit
    CHECK(backoffInterval(86400000u, 3) == 86400000u);     // base >= 1 jam: tidak membesar

    // jendela sleep
    CHECK(isInWindow(18, 18, 1) && isInWindow(23, 18, 1) && isInWindow(0, 18, 1));
    CHECK(!isInWindow(1, 18, 1) && !isInWindow(12, 18, 1) && !isInWindow(17, 18, 1));
    CHECK(isInWindow(8, 8, 10) && isInWindow(9, 8, 10) && !isInWindow(10, 8, 10) && !isInWindow(7, 8, 10));
    for (int h = 0; h < 24; h++)
        CHECK(!isInWindow(h, 5, 5));

    // versi
    CHECK(compareFirmwareVersion("2.3.6", "2.3.7") < 0);
    CHECK(compareFirmwareVersion("2.10.0", "2.9.0") > 0);
    CHECK(compareFirmwareVersion("2.3", "2.3.0") == 0);
    CHECK(compareFirmwareVersion("2.3.7", "2.3.7") == 0);

    // md5
    CHECK(isValidMd5Hex("d41d8cd98f00b204e9800998ecf8427e"));
    CHECK(!isValidMd5Hex("d41d8cd98f00b204e9800998ecf8427"));
    CHECK(!isValidMd5Hex("d41d8cd98f00b204e9800998ecf8427ee"));
    CHECK(!isValidMd5Hex("g41d8cd98f00b204e9800998ecf8427e"));
    CHECK(!isValidMd5Hex(nullptr));

    // kalender
    CHECK(daysFromCivil(1970, 1, 1) == 0);
    CHECK(daysFromCivil(2000, 3, 1) == 11017);
    CHECK(daysFromCivil(2024, 2, 29) == 19782);
    CHECK(daysFromCivil(2025, 1, 1) * 86400LL == 1735689600LL); // = MIN_VALID_EPOCH

    // nama file antrean
    int i = -1;
    CHECK(parseQueueIndex("queue_0.csv", &i) && i == 0);
    CHECK(parseQueueIndex("queue_59999.csv", &i) && i == 59999);
    CHECK(!parseQueueIndex("queue_60000.csv", &i));
    CHECK(!parseQueueIndex("queue_007.csv", &i));
    CHECK(!parseQueueIndex("queue_-1.csv", &i));
    CHECK(!parseQueueIndex("queue_1.csv.bak", &i));
    CHECK(!parseQueueIndex("queue_keep.tmp", &i));

    // marker versi di image
    {
        OtaVerScan s;
        feed(s, "xxFWVER:2.3.7;yy", 16);
        CHECK(s.found && !strcmp(s.ver, "2.3.7"));
    }
    {
        OtaVerScan s;
        feed(s, "AAFWV", 5);
        feed(s, "ER:2.3", 6);
        feed(s, ".7;zz", 5);
        CHECK(s.found && !strcmp(s.ver, "2.3.7"));
    } // terpotong antar-chunk
    {
        OtaVerScan s;
        feed(s, "FWVER:\0FWVER:2.3.7;", 19);
        CHECK(s.found);
    } // literal prefiks + NUL
    {
        OtaVerScan s;
        feed(s, "FFWVER:1.0.0;", 13);
        CHECK(s.found && !strcmp(s.ver, "1.0.0"));
    }
    {
        OtaVerScan s;
        feed(s, "FWVER:;FWVER:2.3.6;", 19);
        CHECK(s.found && !strcmp(s.ver, "2.3.6"));
    }
    {
        OtaVerScan s;
        feed(s, "FWVER:2.3;", 10);
        CHECK(!s.found);
    }
    {
        OtaVerScan s;
        feed(s, "FWVER:1111111111111111111;", 26);
        CHECK(!s.found);
    }

    // pola wrap millis (menguji ekspresinya, bukan fungsi firmware)
    uint32_t t0 = 0xFFFFFF00u, now = t0 + 0x200u; // wrap terjadi
    CHECK((uint32_t)(now - t0) == 0x200u);
    CHECK(!((uint32_t)(now - t0) < 100u));
    CHECK((int32_t)((uint32_t)5u - (uint32_t)0x10u) < 0); // shownAt lebih baru dari now

    printf("%s: %d/%d lulus\n", fails ? "GAGAL" : "OK", total - fails, total);
    return fails ? 1 : 0;
}
