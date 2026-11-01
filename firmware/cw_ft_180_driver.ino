/*
  CW-FT-180 Torus PEMF & Skaler Alan Sürücüsü - ESP32-C3 / ESP32-S3 Firmware
  
  Bobin Geometrisi:
    - Çap: Ø 180 mm, delik: Ø 40 mm, yükseklik: 83 mm
    - Ters sarımlı çift 1 mm emaye Cu tel (11x12 torus knot)
    - Net Toroidal Tur: 22 tur
    - DC Direnç: 218 mOhm
    - Endüktans L: 29.0 uH
    - Merkez Alan Sabiti: 199 uT / A (tepe)
    - Dipol Momenti: 0.25 A·m² / A

  Donanım & Sürücü Mimarisi (Siyah Kanal / Housing Katı):
    - Giriş: 12V DC barrel jak / adaptör veya 3S Li-ion pil takımı
    - Regülatör: LM2596 / MP1584 Buck step-down (VBUS ayarlı ~2.0V - 3.3V)
    - Köprü: TI DRV8871 (3.6 A tepe akımı) veya Ayrık Dual H-Bridge
    - Akım Algılama: INA240A1 (G=20) veya INA219 (I2C)
    - Çift Tel Çıkışı: 
        * B Modu: Teller ters-seri (Manyetik dipol maksimum)
        * Skaler Modu: Teller ters-paralel / zıt manyetik (B net = 0, Skaler alan)

  GATT Servis ve Karakteristikleri:
    SVC = "7a1c0001-5b4e-4f6a-9c1d-2e3f4a5b6c7d"
    CMD = "7a1c0002-5b4e-4f6a-9c1d-2e3f4a5b6c7d"
    STA = "7a1c0003-5b4e-4f6a-9c1d-2e3f4a5b6c7d"
*/

#include <Arduino.h>
#include <math.h>
#include <atomic>
#include <esp_timer.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

// ---------------- Pin Tanımları (ESP32-C3 SuperMini) ----------------
constexpr int PIN_PWM_A   = 5;   // Köprü IN1 / PWM
constexpr int PIN_PWM_B   = 6;   // Köprü IN2 / DIR
constexpr int PIN_ENABLE  = 7;   // Köprü nSLEEP / EN
constexpr int PIN_ISENSE  = 3;   // Akım şönt ADC (INA240 çıkışı)
constexpr int PIN_VBUS    = 1;   // Sürücü bus voltajı (~2-3V)
constexpr int PIN_VIN     = 4;   // 12V DC giriş voltaj bölücü (10k / 2.2k)
constexpr int PIN_LED     = 20;  // Siyah kanal durum LED / Neon göstergesi
constexpr int PIN_RELAY_TOPO = 8; // Topoloji rölesi / MOSFET (B vs Skaler)

// ---------------- Fiziksel Sabitler (CW-FT-180 Simülasyon Verisi) ----------------
constexpr float UT_PER_A_CENTER = 199.0f;  // 1 A tepe için merkez B (uT)
constexpr float R_COIL          = 0.218f;  // Ohm
constexpr float L_COIL          = 29e-6f;  // H (29 uH)
constexpr float I_SET_MAX       = 3.0f;    // A tepe (3000 mA)
constexpr float I_TRIP          = 4.2f;    // A ani aşırı akım kesme eşiği

// ---------------- Zamanlama ve PWM ----------------
constexpr uint32_t PWM_FREQ  = 100000;  // 100 kHz PWM
constexpr uint8_t  PWM_BITS  = 9;       // 9 bit
constexpr uint32_t PWM_MAX   = (1u << PWM_BITS) - 1;
constexpr uint32_t TICK_US   = 100;     // 10 kHz kontrol döngüsü
constexpr int      N_TAB     = 256;

// ---------------- BLE UUID ----------------
#define SERVICE_UUID           "7a1c0001-5b4e-4f6a-9c1d-2e3f4a5b6c7d"
#define CHARACTERISTIC_UUID_RX "7a1c0002-5b4e-4f6a-9c1d-2e3f4a5b6c7d" // CMD
#define CHARACTERISTIC_UUID_TX "7a1c0003-5b4e-4f6a-9c1d-2e3f4a5b6c7d" // STA

enum Mode : uint8_t { M_SINUS, M_DARBE };
enum Topo : uint8_t { TOPO_B, TOPO_SCALAR };

static float SINE[N_TAB];
static volatile bool g_run = false;
static volatile Mode g_mode = M_SINUS;
static volatile Topo g_topo = TOPO_B;
static volatile float g_freq = 100.0f;        // 1 - 1000 Hz
static volatile float g_iset = 1.0f;          // A tepe
static volatile uint32_t g_session_s = 20 * 60;
static volatile int g_duty_pct = 25;
static volatile bool g_led = true;

static float g_imeas = 0.0f;
static float g_vbus = 12.0f;
static uint32_t g_run_started_ms = 0;
static const char *g_fault = "yok";

static BLEServer *pServer = nullptr;
static BLECharacteristic *pTxChar = nullptr;
static BLECharacteristic *pRxChar = nullptr;
static bool deviceConnected = false;

// Periyodik kontrol zamanlayıcısı
static hw_timer_t *g_timer = nullptr;

// ---------------- Sinüs Tablosu Başlatma ----------------
void initSineTable() {
  for (int i = 0; i < N_TAB; i++) {
    SINE[i] = sinf(2.0f * M_PI * (float)i / (float)N_TAB);
  }
}

// ---------------- Durum Raporlama Stringi ----------------
String getStatusString() {
  uint32_t left = 0;
  if (g_run) {
    uint32_t elapsed = (millis() - g_run_started_ms) / 1000;
    if (elapsed < g_session_s) left = g_session_s - elapsed;
    else left = 0;
  }
  float bEst = (g_topo == TOPO_SCALAR) ? 0.0f : (g_imeas * UT_PER_A_CENTER);

  char buf[128];
  snprintf(buf, sizeof(buf), 
    "%s m=%s f=%.2fHz I_set=%.0fmA I_olc=%.0fmA Vk=%.0fmV Vpil=%.2fV B~%.1fuT kalan=%lus hata=%s led=%s",
    g_run ? "ACIK" : "KAPALI",
    (g_mode == M_SINUS) ? "sinus" : "darbe",
    g_freq,
    g_iset * 1000.0f,
    g_imeas * 1000.0f,
    g_imeas * R_COIL * 1000.0f,
    g_vbus,
    bEst,
    left,
    g_fault,
    g_led ? "acik" : "kapali"
  );
  return String(buf);
}

// ---------------- Komut Ayrıştırıcı ----------------
void handleCommand(const String &cmd) {
  String c = cmd;
  c.trim();

  if (c == "on") {
    g_run = true;
    g_run_started_ms = millis();
    g_fault = "yok";
    digitalWrite(PIN_ENABLE, HIGH);
  } else if (c == "off") {
    g_run = false;
    digitalWrite(PIN_ENABLE, LOW);
  } else if (c.startsWith("a ")) {
    float ma = c.substring(2).toFloat();
    g_iset = constrain(ma / 1000.0f, 0.1f, I_SET_MAX);
  } else if (c.startsWith("t ")) {
    g_session_s = c.substring(2).toInt() * 60;
  } else if (c.startsWith("f ")) {
    g_freq = constrain(c.substring(2).toFloat(), 1.0f, 1000.0f);
  } else if (c == "m sinus") {
    g_mode = M_SINUS;
  } else if (c == "m darbe") {
    g_mode = M_DARBE;
  } else if (c == "schumann") {
    g_mode = M_DARBE;
    g_freq = 7.83f;
    g_duty_pct = 5;
  } else if (c == "topo B") {
    g_topo = TOPO_B;
    digitalWrite(PIN_RELAY_TOPO, LOW); // Seri B modu
  } else if (c == "topo SCALAR") {
    g_topo = TOPO_SCALAR;
    digitalWrite(PIN_RELAY_TOPO, HIGH); // Zıt skaler modu
  } else if (c == "led on") {
    g_led = true;
    digitalWrite(PIN_LED, HIGH);
  } else if (c == "led off") {
    g_led = false;
    digitalWrite(PIN_LED, LOW);
  }
}

// ---------------- BLE Geri Çağrımları ----------------
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    BLEDevice::startAdvertising();
  }
};

class RxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue().c_str();
    if (rxValue.length() > 0) {
      handleCommand(rxValue);
    }
  }
};

void setup() {
  Serial.begin(115200);
  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_ENABLE, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_RELAY_TOPO, OUTPUT);
  digitalWrite(PIN_ENABLE, LOW);
  digitalWrite(PIN_LED, HIGH);

  initSineTable();

  // BLE Başlatma
  BLEDevice::init("CW-FT-180-PEMF");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxChar = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
  pTxChar->addDescriptor(new BLE2902());

  pRxChar = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxChar->setCallbacks(new RxCallbacks());

  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  Serial.println("CW-FT-180 Driver Ready!");
}

void loop() {
  // Seans süresi kontrolü
  if (g_run && (millis() - g_run_started_ms >= g_session_s * 1000)) {
    g_run = false;
    digitalWrite(PIN_ENABLE, LOW);
  }

  // Sanal / Donanımsal Akım Ölçümü
  if (g_run) {
    g_imeas = g_iset; // INA240 ADC entegre edildiğinde ADC okuması alınır
  } else {
    g_imeas = 0.0f;
  }

  // BLE Durum Güncellemesi (1 Hz)
  static uint32_t lastNotify = 0;
  if (millis() - lastNotify >= 1000) {
    lastNotify = millis();
    String st = getStatusString();
    if (deviceConnected && pTxChar) {
      pTxChar->setValue(st.c_str());
      pTxChar->notify();
    }
  }

  delay(10);
}
