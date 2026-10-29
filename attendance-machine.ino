#include <SPI.h>
#include <MFRC522.h>

constexpr uint8_t PIN_SCK = 4;
constexpr uint8_t PIN_MISO = 5;
constexpr uint8_t PIN_MOSI = 6;
constexpr uint8_t PIN_SS = 7;
constexpr uint8_t PIN_RST = 3;
constexpr uint8_t PIN_LED = 8;

constexpr bool UID_LITTLE_ENDIAN = true;
constexpr uint32_t DEBOUNCE_MS = 500;
constexpr uint32_t HEALTH_CHECK_MS = 5000;
constexpr uint32_t LED_MS = 60;

MFRC522 rfid(PIN_SS, PIN_RST);

uint32_t lastUid = 0xFFFFFFFF;
uint32_t lastTapMs = 0;
uint32_t lastCheckMs = 0;
uint32_t ledOffAt = 0;
bool ledOn = false;
bool rc522Healthy = false;

static bool rc522Present() {
  byte v = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  return v != 0x00 && v != 0xFF;
}

// LED aktif rendah; dimatikan oleh loop() tanpa menahan pemindaian
static void ledPulse(uint32_t now) {
  digitalWrite(PIN_LED, LOW);
  ledOn = true;
  ledOffAt = now + LED_MS;
}

static uint32_t uidToNumber(const MFRC522::Uid &uid) {
  byte b[4] = { 0, 0, 0, 0 };
  for (uint8_t i = 0; i < 4 && i < uid.size; i++)
    b[i] = uid.uidByte[i];
  if (UID_LITTLE_ENDIAN) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
  }
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static void initReader() {
  rfid.PCD_Init();
  delay(10);
  if (rc522Present()) {
    rfid.PCD_SetAntennaGain(rfid.RxGain_max);
    if (!rc522Healthy)
      Serial.println("READY");
    rc522Healthy = true;
  } else {
    if (rc522Healthy || lastCheckMs == 0)
      Serial.println("ERR:RC522");
    rc522Healthy = false;
  }
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000)
    delay(10);

  SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI, PIN_SS);
  initReader();
  lastCheckMs = millis();
}

void loop() {
  uint32_t now = millis();

  if (ledOn && (int32_t)(now - ledOffAt) >= 0) {
    digitalWrite(PIN_LED, HIGH);
    ledOn = false;
  }

  if (now - lastCheckMs >= HEALTH_CHECK_MS) {
    lastCheckMs = now;
    if (!rc522Present() || !rc522Healthy)
      initReader();
  }
  if (!rc522Healthy) {
    delay(50);
    return;
  }

  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;

  uint32_t uid = uidToNumber(rfid.uid);

  bool duplicate = (uid == lastUid) && (now - lastTapMs < DEBOUNCE_MS);
  if (!duplicate) {
    char line[16];
    snprintf(line, sizeof(line), "%010lu", (unsigned long)uid);
    Serial.println(line);
    Serial.flush();
    lastUid = uid;
    lastTapMs = now;
    ledPulse(now);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
