# CW-FT-180 · Ters Sarımlı Çift Telli Tam Toroid

**Uygulama:** https://cellular1973-ai.github.io/cw-ft-180/cw_ft_180.html
(kök adres ve `cwft180.html` aynı sayfayı açar)

CW-FT-180, iki ayrı telle ters yönlerde sarılmış Ø180 mm'lik bir tam toroid bobindir. Sürücü kartı bobinin oturduğu siyah kaidenin içindedir. Telefon veya bilgisayardan Web Bluetooth ile kontrol edilir.

> Tıbbi cihaz değildir; herhangi bir hastalığın tanı veya tedavisi için tasarlanmamıştır. Kalp pili, defibrilatör veya başka bir aktif implant taşıyanlar kullanmamalıdır.

## Bobin

| | Değer |
|---|---|
| Gövde | Ø180 mm dış, Ø40 mm delik, 83 mm yükseklik, elips kesit |
| Sargı | 2 tel × (11 toroidal × 12 poloidal), Ø1 mm bakır |
| Tel A (iç) | 4,94 m · 108 mΩ · 11,2 µH |
| Tel B (dış, ters yönde) | 5,00 m · 110 mΩ · 11,3 µH |
| Teller arası ortak endüktans | −3,07 µH |

Değerler Biot-Savart / Neumann hesabındandır (CST benzetimiyle eksende %1 içinde uyumlu). Ölçümle doğrulanmadı.

## Tel bağlantısı

İki tel ayrı köprülerle sürüldüğü için alanın biçimi yazılımdan seçilir:

| Bağlantı | Akımlar | Merkezde dış alan | Tüp içinde |
|---|---|---|---|
| **Ters** | I_B = −I_A | 199 µT/A | — |
| **Aynı** | I_B = +I_A | ≈ 1 µT/A | ≈ 87 µT/A |
| **Tek tel** | yalnız A | 99 µT/A | — |
| **Faz** | B, A'ya göre faz kaymalı | ters ile aynı arası | ters ile aynı arası |

- **Ters** bağlantıda iki telin dipol alanları toplanır; alan bobinin deliğinden ve dışına yayılır (1 m'de 0,05 µT/A).
- **Aynı** bağlantıda dipol alanları birbirini söndürür, alan tüpün içinde kapalı kalır; bobinin dışında neredeyse alan yoktur.

## Uygulama (`cw_ft_180.html`)

- Bluetooth bağlantısı (cihaz adı `CWFT180`), Demo modu (cihazsız deneme)
- Tel bağlantısı: ters / aynı / tek tel / faz (0–355°)
- Sinyal: sinüs, darbe (%10–50 doluluk), 7,83 Hz dar darbe; frekans 1–200 Hz
- Şiddet: tel başına 100–1500 mA tepe
- Oturum: 10 / 20 / 30 / 60 dk veya en fazla 12 adımlı program (hazır 3 program + kendi adımlarınız)
- Durum: tel A ve B akımı, köprü gerilimleri, USB gerilimi, kalan süre, tahmini merkez ve tüp içi alan, dB/dt
- Telefona kurulabilir (PWA), çevrimdışı açılır

Web Bluetooth: Android'de Chrome, iPhone'da Bluefy tarayıcısı; sayfa HTTPS üzerinden açılmalıdır.

## Firmware (`firmware/cwft180_surucu/`)

ESP32-C3 SuperMini, Arduino-ESP32 core 3.x. Taslaktır, donanımda henüz denenmedi.

```bash
arduino-cli compile -b "esp32:esp32:esp32c3:CDCOnBoot=cdc,PartitionScheme=huge_app" --upload -p COMx firmware/cwft180_surucu
```

Komutlar (seri port 115200 ve Bluetooth aynı):

```
on | off | a <mA> | t <dakika> | f <Hz> | m sinus | m darbe | d <yüzde> | w <us>
schumann | bag ters|ayni|tek|faz | faz <derece> | led on|off | s | cal
p <f>/<s|dNN|wNNN>/<dk>,...     örnek: p 7.83/w500/5,10/s/10,100/d25/5
```

Bluetooth servisi `7a1c0101-5b4e-4f6a-9c1d-2e3f4a5b6c7d` (komut `…0102`, durum `…0103`).

## Donanım

Kaide kartı ve muhafaza: [HARDWARE_DESIGN.md](HARDWARE_DESIGN.md)

## Depodaki diğer dosyalar

`torus_kolye.html` ve `manifest.webmanifest` küçük torus kolyenin (CW-FT-50) uygulamasıdır; asıl adresi https://cellular1973-ai.github.io/torus-kolye/.
