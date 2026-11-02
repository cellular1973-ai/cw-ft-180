// Kopru kanali: PWM/yon pinleri, akim olcumu ve periyot basina genlik duzeltmesi (torus_pemf_driver ile ayni yasa)
#pragma once
#include <Arduino.h>
#include <math.h>

constexpr bool     BRIDGE_PHEN = true;   // true: DRV8838 PH/EN (prototip); false: ayrik kopru (A kolu PWM / B kolu yon)
constexpr uint32_t PWM_FREQ = 100000;     // Hz
constexpr uint8_t  PWM_BITS = 9;          // 80 MHz / 100 kHz = 800 -> 9 bit
constexpr uint32_t PWM_MAX  = (1u << PWM_BITS) - 1;
constexpr float SENSE_V_PER_A = 0.010f * 20.0f;  // 10 mOhm x INA240A1 (20) = 0.2 V/A
constexpr float D_MAX    = 0.95f;         // en yuksek doluluk
constexpr float KI       = 0.25f;         // periyot basina genlik duzeltme kazanci
constexpr float STEP_MAX = 0.05f;         // periyot basina en fazla %5 degisim
constexpr float I_TRIP   = 2.2f;          // A anlik - aninda kapat

struct Kanal {
  const char *ad;
  int pin_pwm, pin_dir, pin_is;
  float ioff_mV = 1650.0f;  // sifir akim ofseti
  float ampV = 0;           // kopru gerilim genligi komutu (V)
  float imeas = 0;          // olculen tepe akimi (A)
  bool neg = false;
  uint16_t sat = 0;         // ust uste doyum sayaci (acik devre algisi)

  void begin() {
    pinMode(pin_dir, OUTPUT);
    digitalWrite(pin_dir, LOW);
    if (!ledcAttach(pin_pwm, PWM_FREQ, PWM_BITS)) Serial.printf("LEDC %s baslatilamadi\n", ad);
    ledcWrite(pin_pwm, 0);
    analogSetPinAttenuation(pin_is, ADC_11db);
  }

  void off() {
    ledcWrite(pin_pwm, 0);
    digitalWrite(pin_dir, LOW);
    neg = false;
  }

  float readA() const { return (analogReadMilliVolts(pin_is) - ioff_mV) / (SENSE_V_PER_A * 1000.0f); }

  void calibrate() {                      // kopru kapaliyken cagrilmali
    uint32_t acc = 0;
    for (int k = 0; k < 64; k++) acc += analogReadMilliVolts(pin_is);
    ioff_mV = acc / 64.0f;
  }

  void write(float v, float vbus) {
    float d = fminf(fabsf(v) / vbus, D_MAX);
    bool want_neg = v < 0;
    if (want_neg != neg) { neg = want_neg; digitalWrite(pin_dir, neg ? HIGH : LOW); }
    ledcWrite(pin_pwm, (uint32_t)lroundf(((neg && !BRIDGE_PHEN) ? 1.0f - d : d) * PWM_MAX));
  }

  // periyot sonu: carpimsal, sinirli genlik duzeltmesi. true -> doyumda ve akim cok dusuk (tel kopuk)
  bool update(float im, float iset, float vmax) {
    imeas = im;
    if (iset <= 0) { ampV = 0; sat = 0; return false; }
    float err = (iset - im) / fmaxf(iset, 0.05f);
    ampV *= 1.0f + fminf(fmaxf(KI * err, -STEP_MAX), STEP_MAX);
    if (ampV > vmax) {
      ampV = vmax;
      sat = (im < 0.3f * iset) ? sat + 1 : 0;
      return sat > 50;
    }
    sat = 0;
    return false;
  }
};

// darbe sayaci (kanal basina): k = periyot basindan beri adim, k > n_on bosta
struct DarbeSay { int k = 1 << 30; int n_pl = 0; float acc = 0; };
