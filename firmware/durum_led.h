// Durum LED'i (GPIO20 -> 1k -> LED -> GND, aktif yuksek). Desenler kisa cakmalar: ortalama akim < 0.2 mA.
//   hata: 5 Hz | calisiyor: 1 s'de 80 ms
//   durdu + BLE bagli: 2 s'de 30 ms | durdu: 4 s'de 30 ms
#pragma once
#include <Arduino.h>
#include <driver/gpio.h>

constexpr int PIN_LED = 20;  // SuperMini IO20 (U0RXD; seri port USB CDC'de oldugu icin bos)

static bool g_led_en = true;

static inline void ledInit() {
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);
}

static inline bool ledPattern(uint32_t now, bool fault, bool run, bool sleeping, bool conn, uint32_t adv_every) {
  if (fault)    return (now / 100) % 2 == 0;
  if (sleeping) return now % adv_every < 60;         // CPU bu pencerede zaten uyanik
  if (run)      return now % 1000 < 80;
  return now % (conn ? 2000 : 4000) < 30;
}

static inline void ledUpdate(bool fault, bool run, bool sleeping, bool conn, uint32_t adv_every) {
  digitalWrite(PIN_LED, g_led_en && ledPattern(millis(), fault, run, sleeping, conn, adv_every) ? HIGH : LOW);
}

static inline void ledBeforeSleep() {                 // uykuda sonuk kalsin
  digitalWrite(PIN_LED, LOW);
  gpio_hold_en((gpio_num_t)PIN_LED);
}

static inline void ledAfterSleep() { gpio_hold_dis((gpio_num_t)PIN_LED); }
