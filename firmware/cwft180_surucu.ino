/*
  CW-FT-180 Ters Sarimli Cift Telli Tam Toroid - kaide surucusu - ESP32-C3 SuperMini (Arduino-ESP32 core 3.x)
  TASLAK, test edilmedi. Kucuk torus kolye surucusunun (torus_pemf_driver) iki kanalli, sebekeden beslenen surumu.

  Bobin (Biot-Savart / Neumann, girdap_bobin/dogrulama/uc_bobin_180.py, 1 mm Cu):
    tel A (ic, +faz yonunde): 4.94 m, 108 mOhm, 11.2 uH | tel B (dis, ters yonde): 5.00 m, 110 mOhm, 11.3 uH | M = -3.07 uH
  Bag = B kanalinin akiminin A'ya gore iliskisi:
    ters (IB = -IA): iki telin dipolu toplanir -> merkez 199 uT/A, 1 m'de 0.05 uT/A (CST benzetimindeki baglanti)
    ayni (IB = +IA): dipoller birbirini yok eder, alan tupun icinde kapali -> merkez ~1 uT/A, tup ici ~87 uT/A
    tek  (IB = 0):   yalniz tel A -> merkez 100 uT/A
    faz  (IB = -IA, faz kadar gecikmeli): sinuste ters (0) ile ayni (180) arasi; darbede B darbesi faz/360
                     periyot sonra (180 = sirali darbe)
  Donanim (on tasarim):
    USB-C 5 V -> buck ~1.5 V (VBUS) -> 2 x H-koprusu (prototip 2 x DRV8838 PH/EN) -> 22 uH seri bobin -> tel A / tel B
    -> 10 mOhm sont -> INA240A1 (G = 20, REF ortada: 1.65 V + 0.2 V/A). VIN (5 V) / 2 bolucu.
  Zamanlama: 10 kHz adim, 32 bit faz biriktirici; sinus 256 noktali tablo + kanal basina lock-in genlik,
    darbe faz tasinca baslar (kenar 0.1 ms, tepede I*R, kenarda L_etkin*dI/dt).
  Program: en fazla 12 adim (f / dalga / dakika) sirayla calisir; oturum suresi adimlarin toplamidir.

  Komutlar (Seri 115200 ve BLE ayni):
    on | off | a <mA> | t <dakika> | f <Hz> | m sinus | m darbe | d <yuzde> | w <us> (0: doluluk kullan)
    schumann | bag ters|ayni|tek|faz | faz <derece> | p <f>/<s|dNN|wNNN>/<dk>,... | p (programi sil)
    led on | led off | s (durum) | cal (sifir akim kalibrasyonu)
    ornek program: p 7.83/w500/5,10/s/10,100/d25/5
*/
#include <Arduino.h>
#include <math.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "durum_led.h"
#include "kanal.h"

// ---------------- pinler (SuperMini; 2, 8, 9 acilis pinleri - kullanma) ----------------
constexpr int PIN_EN   = 21;  // iki koprunun nSLEEP'i (aktif yuksek), 100k pull-down. IO21 = U0TXD: acilista
                              // kisa darbe gorebilir; PWM pinleri asagida oldugundan kopruler frende kalir
constexpr int PIN_VBUS = 1;   // kopru besleme (~1.5 V, dogrudan ADC1)
constexpr int PIN_VIN  = 0;   // USB 5 V / 2 (ADC1)
static Kanal A{"A", 5, 6, 3};  // PWM, yon (PH), akim (INA240)
static Kanal B{"B", 7, 10, 4};

// ---------------- zamanlama ----------------
constexpr uint32_t TICK_US = 100;               // 10 kHz
constexpr int      N_TAB = 256;                 // sinus tablosu; indeks = faz >> 24
constexpr int      ADC_EVERY = 4;               // sinusta her 4 adimda bir akim ornegi
constexpr float    F_MIN = 0.5f, F_MAX = 200.0f;

// ---------------- sinirlar / ayar ----------------
constexpr float I_SET_MAX = 1.5f;     // A tepe / kanal (ters bagda merkezde ~300 uT)
constexpr float VIN_MIN   = 4.4f, VIN_DIV = 2.0f;
constexpr float VBUS_MIN  = 0.5f;
constexpr float R_INIT    = 0.25f;    // Ohm/kanal: tel 0.11 + seri bobin 0.03 + kopru + sont (ilk tahmin)
constexpr float L_SER = 22e-6f, L_TEL = 11.25e-6f, M_TEL = -3.07e-6f;
constexpr float T_RISE    = TICK_US * 1e-6f;    // darbe kenari: 1 adim = 0.1 ms
constexpr int   DUTY_MIN = 10, DUTY_MAX = 50, N_ON_MIN = 4;
constexpr float UT_PER_A_TEL = 99.4f;           // merkezde tek tel, 1 A
constexpr int   N_PROG = 12;

enum Fault : uint8_t { F_NONE, F_OC_A, F_OC_B, F_VIN_LOW, F_VBUS_LOW, F_OPEN_A, F_OPEN_B };
const char *FAULT_TXT[] = {"yok", "asiri akim A", "asiri akim B", "USB besleme dusuk", "kopru beslemesi yok",
                           "tel A acik devre", "tel B acik devre"};
enum Mode : uint8_t { M_SINUS, M_DARBE };
enum Bag : uint8_t { B_TERS, B_AYNI, B_TEK, B_FAZ };
const char *BAG_TXT[] = {"ters", "ayni", "tek", "faz"};

struct Adim { float f; Mode m; uint8_t duty; uint16_t pw_us; uint16_t sn; };

static float SINE[N_TAB];
static volatile bool  g_run = false;
static volatile Mode  g_mode = M_SINUS;       // istenen (on / yeniden baslatmada etkin olur)
static volatile Bag   g_bag = B_TERS;
static volatile float g_faz = 90.0f;          // derece, yalniz bag=faz
static volatile float g_freq = 100.0f;
static volatile int   g_duty_pct = 25;
static volatile uint32_t g_pw_us = 0;         // 0 -> doluluk
static volatile float g_iset = 1.0f;
static volatile uint32_t g_session_s = 20 * 60;
static volatile Fault g_fault = F_NONE;
// calisan ayar (startOutput'ta kopyalanir; surucu gorevi yalniz bunlari okur)
static Mode  r_mode = M_SINUS;
static float r_sB = -1.0f;                    // B kanal isareti
static uint32_t r_step = 0, r_offB = 0;       // faz adimi ve B gecikmesi (2^32 = bir periyot)
static int   r_n_on = 25;
static float r_LA = 0, r_LB = 0;              // darbe kenari icin etkin enduktans
static volatile bool g_reset = false;
static float g_vbus = 1.5f, g_vin = 5.0f, g_phase_deg = 0;
static uint32_t g_run_started_ms = 0;
static Adim g_prog[N_PROG];
static int g_nprog = 0, g_pstep = -1;         // g_pstep: calisan adim, -1 program yok/durdu
static uint32_t g_step_started_ms = 0;
static TaskHandle_t g_task = nullptr;
static hw_timer_t *g_timer = nullptr;
static BLECharacteristic *g_status_ch = nullptr;
static volatile bool g_ble_conn = false;

static float readVbus() { return analogReadMilliVolts(PIN_VBUS) / 1000.0f; }
static float readVin()  { return analogReadMilliVolts(PIN_VIN) * VIN_DIV / 1000.0f; }

static void outputOff() {
  digitalWrite(PIN_EN, LOW);
  A.off();
  B.off();
}

static void trip(Fault f) {
  g_run = false;
  if (g_timer) timerStop(g_timer);
  outputOff();
  g_fault = f;
}

static float bSign(Bag b) { return b == B_AYNI ? 1.0f : b == B_TEK ? 0.0f : -1.0f; }

static void startOutput() {
  if (g_fault != F_NONE) return;
  g_vbus = readVbus();
  g_vin = readVin();
  if (g_vin < VIN_MIN) { trip(F_VIN_LOW); return; }
  if (g_vbus < VBUS_MIN) { trip(F_VBUS_LOW); return; }
  r_mode = g_mode;
  r_sB = bSign(g_bag);
  r_step = (uint32_t)((double)g_freq * TICK_US * 1e-6 * 4294967296.0);
  r_offB = g_bag == B_FAZ ? (uint32_t)(fmodf(g_faz, 360.0f) / 360.0 * 4294967296.0) : 0;
  uint32_t period_us = (uint32_t)lroundf(1e6f / g_freq);
  uint32_t pw = g_pw_us ? g_pw_us : period_us * g_duty_pct / 100;
  r_n_on = constrain((int)(pw / TICK_US), N_ON_MIN, (int)(period_us / TICK_US / 2));
  // es zamanli darbede ortak enduktans: V_A = L dIA/dt + M dIB/dt, IB = sB IA
  float m = r_offB == 0 ? r_sB * M_TEL : 0.0f;
  r_LA = L_SER + L_TEL + m;
  r_LB = L_SER + L_TEL + m;
  A.ampV = g_iset * R_INIT;                   // ileri besleme
  B.ampV = r_sB != 0 ? g_iset * R_INIT : 0;
  A.sat = B.sat = 0;
  B.imeas = 0;
  g_reset = true;
  digitalWrite(PIN_EN, HIGH);
  g_run = true;
  timerRestart(g_timer);
  timerStart(g_timer);
}

static String restartIfRunning(String reply) {   // mod/frekans degisikligi: oturum suresi korunur
  if (g_run) {
    uint32_t t0 = g_run_started_ms;
    trip(F_NONE);
    startOutput();
    g_run_started_ms = t0;
  }
  return reply;
}

// ---------------- surucu gorevi (10 kHz) ----------------
void IRAM_ATTR onTick() {
  BaseType_t woken = pdFALSE;
  vTaskNotifyGiveFromISR(g_task, &woken);
  portYIELD_FROM_ISR(woken);
}

// darbe gerilimi: k = 0 yukselen kenar, 1..n-2 tepe, n-1 dusen kenar, sonrasi 0
static inline float pulseV(int k, int n, float ampV, float L) {
  float vl = L * g_iset / T_RISE;
  if (k == 0)     return  vl + 0.5f * ampV;
  if (k < n - 1)  return ampV;
  if (k == n - 1) return -vl + 0.5f * ampV;
  return 0.0f;
}

static void periodDone(Kanal &ch, float im, float L, Fault open) {
  g_vbus = 0.9f * g_vbus + 0.1f * readVbus();
  if (g_vbus < VBUS_MIN) { trip(F_VBUS_LOW); return; }
  float vmax = D_MAX * g_vbus - (r_mode == M_DARBE ? L * g_iset / T_RISE : 0.0f);
  if (ch.update(im, g_iset, vmax)) trip(open);
}

// bir kanalin darbe adimi (DarbeSay: kanal.h)
static bool darbeAdim(Kanal &ch, DarbeSay &d, float sgn, float L, Fault oc, Fault open) {
  int n = r_n_on;
  if (d.k > n) { ch.write(0, g_vbus); return true; }
  ch.write(sgn * pulseV(d.k, n, ch.ampV, L), g_vbus);
  bool win = d.k >= n / 2 && d.k <= n - 2;    // tepenin ikinci yarisi
  if (win || d.k == n) {
    float i = sgn * ch.readA();
    if (fabsf(i) > I_TRIP) { trip(oc); return false; }
    if (win) { d.acc += i; d.n_pl++; }
  }
  if (d.k == n) periodDone(ch, d.n_pl ? d.acc / d.n_pl : 0.0f, L, open);
  d.k++;
  return g_run;
}

static void driverTask(void *) {
  uint32_t ph = 0;
  int tick = 0, n_adc = 0;
  float aS = 0, aC = 0, bS = 0, bC = 0;
  DarbeSay dA, dB;
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    if (!g_run) continue;
    if (g_reset) {
      g_reset = false; ph = 0; tick = 0; n_adc = 0; aS = aC = bS = bC = 0;
      dA = DarbeSay(); dB = DarbeSay();
      dA.k = 0;
      if (r_offB == 0) dB.k = 0;
    }
    uint32_t prev = ph;
    uint32_t phB = ph + r_offB;

    if (r_mode == M_DARBE) {
      if (!darbeAdim(A, dA, 1.0f, r_LA, F_OC_A, F_OPEN_A)) continue;
      if (r_sB != 0 && !darbeAdim(B, dB, r_sB, r_LB, F_OC_B, F_OPEN_B)) continue;
      ph += r_step;
      if (ph < prev) { dA = DarbeSay(); dA.k = 0; }                       // A periyodu basladi
      if (r_sB != 0 && ph + r_offB < phB) { dB = DarbeSay(); dB.k = 0; }  // B periyodu basladi
      continue;
    }

    int ia = ph >> 24, ib = phB >> 24;
    float sa = SINE[ia], sb = SINE[ib];
    A.write(A.ampV * sa, g_vbus);
    if (r_sB != 0) B.write(r_sB * B.ampV * sb, g_vbus);
    if (tick++ % ADC_EVERY == 0) {
      float i = A.readA();
      if (fabsf(i) > I_TRIP) { trip(F_OC_A); continue; }
      aS += i * sa;
      aC += i * SINE[(ia + N_TAB / 4) & (N_TAB - 1)];
      if (r_sB != 0) {
        float j = r_sB * B.readA();
        if (fabsf(j) > I_TRIP) { trip(F_OC_B); continue; }
        bS += j * sb;
        bC += j * SINE[(ib + N_TAB / 4) & (N_TAB - 1)];
      }
      n_adc++;
    }
    ph += r_step;
    if (ph < prev && n_adc > 0) {             // periyot tamamlandi: lock-in genlik I = (2/N) sqrt(Ss^2 + Sc^2)
      float is = 2.0f * aS / n_adc, ic = 2.0f * aC / n_adc;
      g_phase_deg = atan2f(-ic, is) * 57.2958f;
      periodDone(A, sqrtf(is * is + ic * ic), 0, F_OPEN_A);
      if (g_run && r_sB != 0) {
        float js = 2.0f * bS / n_adc, jc = 2.0f * bC / n_adc;
        periodDone(B, sqrtf(js * js + jc * jc), 0, F_OPEN_B);
      }
      aS = aC = bS = bC = 0; n_adc = 0;
    }
  }
}

// ---------------- program ----------------
static void applyStep(int k) {
  const Adim &s = g_prog[k];
  g_freq = s.f; g_mode = s.m; g_duty_pct = s.duty; g_pw_us = s.pw_us;
  g_pstep = k;
  g_step_started_ms = millis();
}

static String parseProgram(String arg) {       // "7.83/w500/5,10/s/10,100/d25/5"
  arg.trim();
  g_nprog = 0; g_pstep = -1;
  if (arg.length() == 0 || arg == "sil") return "program=yok";
  int start = 0;
  while (start < (int)arg.length() && g_nprog < N_PROG) {
    int end = arg.indexOf(',', start);
    if (end < 0) end = arg.length();
    char tok[40], md[12];
    arg.substring(start, end).toCharArray(tok, sizeof tok);
    float f = 0, dk = 0;
    if (sscanf(tok, "%f/%11[^/]/%f", &f, md, &dk) != 3 || dk <= 0) { g_nprog = 0; return String("program hatasi: ") + tok; }
    Adim s{fminf(fmaxf(f, F_MIN), F_MAX), M_SINUS, 25, 0, (uint16_t)fminf(dk * 60.0f, 65000.0f)};
    if (md[0] == 'd') { s.m = M_DARBE; s.duty = constrain(atoi(md + 1), DUTY_MIN, DUTY_MAX); }
    else if (md[0] == 'w') { s.m = M_DARBE; s.pw_us = (uint16_t)max(atoi(md + 1), N_ON_MIN * (int)TICK_US); }
    else if (md[0] != 's') { g_nprog = 0; return String("program hatasi: ") + tok; }
    g_prog[g_nprog++] = s;
    start = end + 1;
  }
  uint32_t tot = 0;
  for (int k = 0; k < g_nprog; k++) tot += g_prog[k].sn;
  return "program=" + String(g_nprog) + " adim, " + String(tot / 60.0f, 1) + "dk";
}

static void programTick() {                    // 1 s'de bir: adim suresi doldu mu
  if (!g_run || g_pstep < 0) return;
  if ((millis() - g_step_started_ms) / 1000 < g_prog[g_pstep].sn) return;
  if (g_pstep + 1 >= g_nprog) { trip(F_NONE); g_pstep = -1; return; }
  applyStep(g_pstep + 1);
  restartIfRunning("");
}

// ---------------- komutlar (Seri + BLE) ----------------
static String statusText() {
  char b[330], mod[20], prog[16];
  if (g_mode == M_SINUS) strcpy(mod, "sinus");
  else if (g_pw_us)      snprintf(mod, sizeof mod, "darbe/%luus", (unsigned long)g_pw_us);
  else                   snprintf(mod, sizeof mod, "darbe/%d", g_duty_pct);
  if (g_nprog == 0)      strcpy(prog, "yok");
  else                   snprintf(prog, sizeof prog, "%d/%d", g_pstep + 1, g_nprog);
  // merkez alani: tel A ve (-B) dipolleri, aralarinda bag acisi kadar faz
  float th = g_bag == B_AYNI ? PI : g_bag == B_FAZ ? g_faz * DEG_TO_RAD : 0.0f;
  float ia = A.imeas, ib = g_bag == B_TEK ? 0.0f : B.imeas;
  uint32_t ses = g_session_s;
  float bm = UT_PER_A_TEL * sqrtf(fmaxf(ia * ia + ib * ib + 2 * ia * ib * cosf(th), 0.0f));
  snprintf(b, sizeof b,
           "%s mod=%s bag=%s faz=%.0f f=%.2fHz I_set=%.0fmA IA=%.0fmA IB=%.0fmA VA=%.0fmV VB=%.0fmV "
           "Vbus=%.3fV Vin=%.2fV Bm~%.0fuT kalan=%lus prog=%s led=%s hata=%s",
           g_run ? "ACIK" : "KAPALI", mod, BAG_TXT[g_bag], (float)g_faz, (float)g_freq, g_iset * 1000,
           A.imeas * 1000, ib * 1000, A.ampV * 1000, B.ampV * 1000, g_vbus, g_vin, g_run ? bm : 0.0f,
           g_run ? (unsigned long)(ses - min(ses, (uint32_t)((millis() - g_run_started_ms) / 1000))) : 0UL,
           prog, g_led_en ? "acik" : "kapali", FAULT_TXT[g_fault]);
  return String(b);
}

static String handleCommand(String c) {
  c.trim();
  if (c == "on") {
    g_fault = F_NONE;
    if (g_nprog > 0) {
      uint32_t tot = 0;
      for (int k = 0; k < g_nprog; k++) tot += g_prog[k].sn;
      g_session_s = tot;
      applyStep(0);
    }
    if (g_run) trip(F_NONE);
    startOutput();
    g_run_started_ms = millis();
    return statusText();
  }
  if (c == "off") { trip(F_NONE); g_pstep = -1; return statusText(); }
  if (c == "s")   return statusText();
  if (c == "cal") {
    bool was = g_run; trip(F_NONE); delay(5);
    A.calibrate(); B.calibrate();
    if (was) restartIfRunning("");
    return "ofset A=" + String(A.ioff_mV, 1) + "mV B=" + String(B.ioff_mV, 1) + "mV";
  }
  if (c.startsWith("a ")) {
    g_iset = fminf(fmaxf(c.substring(2).toFloat() / 1000.0f, 0.0f), I_SET_MAX);
    return "I_set=" + String(g_iset * 1000, 0) + "mA";
  }
  if (c.startsWith("t ")) {
    g_session_s = (uint32_t)(fmaxf(c.substring(2).toFloat(), 1.0f) * 60);
    return "oturum=" + String(g_session_s / 60) + "dk";
  }
  if (c.startsWith("f ")) {
    g_freq = fminf(fmaxf(c.substring(2).toFloat(), F_MIN), F_MAX);
    return restartIfRunning("f=" + String(g_freq, 2) + "Hz");
  }
  if (c == "m sinus") { g_mode = M_SINUS; return restartIfRunning("mod=sinus"); }
  if (c == "m darbe") { g_mode = M_DARBE; return restartIfRunning("mod=darbe"); }
  if (c.startsWith("d ")) {
    g_duty_pct = constrain(c.substring(2).toInt(), DUTY_MIN, DUTY_MAX);
    g_pw_us = 0;
    return restartIfRunning("doluluk=%" + String(g_duty_pct));
  }
  if (c.startsWith("w ")) {
    long w = c.substring(2).toInt();
    g_pw_us = w <= 0 ? 0 : (uint32_t)max(w, (long)(N_ON_MIN * TICK_US));
    return restartIfRunning(g_pw_us ? "genislik=" + String(g_pw_us) + "us" : String("genislik=doluluk"));
  }
  if (c == "schumann") {
    g_freq = 7.83f; g_mode = M_DARBE; g_pw_us = 500;
    return restartIfRunning("schumann: 7.83Hz darbe 500us");
  }
  if (c.startsWith("bag ")) {
    String v = c.substring(4);
    for (int k = 0; k < 4; k++)
      if (v == BAG_TXT[k]) { g_bag = (Bag)k; return restartIfRunning("bag=" + v); }
    return "bag: ters | ayni | tek | faz";
  }
  if (c.startsWith("faz ")) {
    g_faz = fminf(fmaxf(c.substring(4).toFloat(), 0.0f), 359.0f);
    return g_bag == B_FAZ ? restartIfRunning("faz=" + String(g_faz, 0)) : "faz=" + String(g_faz, 0);
  }
  if (c == "p" || c.startsWith("p ")) {
    bool was = g_run && g_pstep >= 0;
    if (was) { trip(F_NONE); g_pstep = -1; }
    return parseProgram(c.length() > 2 ? c.substring(2) : String(""));
  }
  if (c == "led on" || c == "led off") { g_led_en = c == "led on"; return g_led_en ? "led=acik" : "led=kapali"; }
  return "komutlar: on | off | a <mA> | t <dk> | f <Hz> | m sinus|darbe | d <yuzde> | w <us> | schumann | "
         "bag ters|ayni|tek|faz | faz <derece> | p <f>/<s|dNN|wNNN>/<dk>,... | led on|off | s | cal";
}

class CmdCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *ch) override {
    String reply = handleCommand(String(ch->getValue().c_str()));
    if (g_status_ch) { g_status_ch->setValue(reply.c_str()); g_status_ch->notify(); }
  }
};

class SrvCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *) override { g_ble_conn = true; }
  void onDisconnect(BLEServer *) override {   // yayini yeniden baslat: yoksa cihaz bir daha bulunamaz
    g_ble_conn = false;
    BLEDevice::startAdvertising();
  }
};

static void setupBLE() {
  BLEDevice::init("CWFT180");
  BLEServer *srv = BLEDevice::createServer();
  srv->setCallbacks(new SrvCallbacks());
  BLEService *svc = srv->createService("7a1c0101-5b4e-4f6a-9c1d-2e3f4a5b6c7d");
  BLECharacteristic *cmd = svc->createCharacteristic("7a1c0102-5b4e-4f6a-9c1d-2e3f4a5b6c7d",
                                                     BLECharacteristic::PROPERTY_WRITE);
  cmd->setCallbacks(new CmdCallbacks());
  g_status_ch = svc->createCharacteristic("7a1c0103-5b4e-4f6a-9c1d-2e3f4a5b6c7d",
                                          BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
#if defined(CONFIG_BLUEDROID_ENABLED)
  g_status_ch->addDescriptor(new BLE2902());   // NimBLE (core 3.3+, C3) bunu kendisi ekler
#endif
  svc->start();
  BLEDevice::getAdvertising()->addServiceUUID(svc->getUUID());
  BLEDevice::startAdvertising();
}

// ---------------- kurulum ----------------
void setup() {
  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, LOW);
  ledInit();
  Serial.begin(115200);
  for (int k = 0; k < N_TAB; k++) SINE[k] = sinf(2.0f * PI * k / N_TAB);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_VBUS, ADC_11db);
  analogSetPinAttenuation(PIN_VIN, ADC_11db);
  A.begin();
  B.begin();
  delay(20);                                  // buck otursun
  A.calibrate();
  B.calibrate();

  xTaskCreate(driverTask, "drv", 4096, nullptr, configMAX_PRIORITIES - 1, &g_task);
  g_timer = timerBegin(1000000);              // 1 MHz zaman tabani
  timerAttachInterrupt(g_timer, &onTick);
  timerAlarm(g_timer, TICK_US, true, 0);
  timerStop(g_timer);                         // yalniz surus sirasinda calisir

  setupBLE();
  Serial.println("CWFT180 hazir. " + handleCommand("?"));
}

void loop() {
  static uint32_t last = 0;
  if (Serial.available()) Serial.println(handleCommand(Serial.readStringUntil('\n')));

  if (millis() - last >= 1000) {               // 1 s: besleme, oturum/program, durum bildirimi
    last = millis();
    g_vin = readVin();
    if (g_run && g_vin < VIN_MIN) trip(F_VIN_LOW);
    if (g_run && (millis() - g_run_started_ms) / 1000 >= g_session_s) { trip(F_NONE); g_pstep = -1; }
    programTick();
    if (g_status_ch) { String st = statusText(); g_status_ch->setValue(st.c_str()); g_status_ch->notify(); }
  }
  ledUpdate(g_fault != F_NONE, g_run, false, g_ble_conn, 5000);
  delay(20);
}
