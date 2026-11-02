# CW-FT-180 · Kaide sürücü kartı ve muhafaza

Bobin kenarı üstünde dik durur (ekseni yatay) ve siyah kaidenin üstündeki kanala oturur. Sürücü kartı kaidenin içindedir. Besleme ve programlama SuperMini'nin USB-C girişinden yapılır.

## Blok şeması

```
USB-C 5 V ─┬─ ESP32-C3 SuperMini (Bluetooth, 10 kHz sürüş döngüsü)
           │
           ├─ TLV62569 buck 1,5 V ─ DRV8838 köprü ─ 22 µH ─ Tel A ─ 10 mΩ şönt ─┐
           │                                                      INA240A1 ─┘→ IO3
           │
           └─ TLV62569 buck 1,5 V ─ DRV8838 köprü ─ 22 µH ─ Tel B ─ 10 mΩ şönt ─┐
                                                                  INA240A1 ─┘→ IO4
```

Her tel için ayrı buck kullanılır: iki tel aynı anda 1,5 A tepe çektiğinde tek bir 2 A buck yetmez.

## Kart

| | |
|---|---|
| Boyut | 70 × 42 mm, 2 katman, köşeler 2 mm pahlı |
| Denetleyici | ESP32-C3 SuperMini, 2,5 mm plastikli pin başlığında; USB-C sol kenarda |
| Köprü | 2 × DRV8838 (PH/EN, ölü zaman entegre içinde) |
| Akım ölçümü | 2 × INA240A1 (G = 20) + 10 mΩ %1 şönt → 1,65 V + 0,2 V/A |
| Sınırlar | Tel başına 1,5 A tepe ayar, 2,2 A anlık kesme; USB < 4,4 V'ta durur |
| Bobin bağlantısı | 4'lü vidalı klemens (Phoenix MKDS 1,5, 5 mm), sağ kenarda |
| Montaj | 2 × M3 delik (sol köşeler) |

Klemens pinleri, yukarıdan aşağı: **4 A−, 3 A+, 2 B−, 1 B+**

### SuperMini pinleri

| Pin | Görev |
|---|---|
| IO5 / IO6 | Kanal A PWM (EN) / yön (PH) |
| IO7 / IO10 | Kanal B PWM (EN) / yön (PH) |
| IO21 | İki köprünün nSLEEP'i (100 k aşağı çekme) |
| IO3 / IO4 | Akım A / akım B (INA240) |
| IO1 | Köprü A beslemesi (VBR_A) |
| IO0 | USB 5 V / 2 |
| IO20 | Durum LED'i |

Kart DRC: 0 hata, 0 açık bağlantı, şema ile eşleşiyor. Üretim dosyaları (Gerber, BOM, dizgi konumları) KiCad projesinden üretilir.

## Muhafaza

| | |
|---|---|
| Gövde | 124 × 90 × 37,4 mm, altı açık kabuk, PLA ~203 g |
| Kanal | Torusun elips kesitine 0,5 mm boşlukla, 12 mm gömülme |
| Taban plakası | 2 mm, gövdeye 4 × M3 havşa vida; kart 4 mm ayaklarda |
| Delikler | Sol duvarda USB-C; kanal dibinde klemensin üstünde 8 × 20 mm tel yarığı; üst yüzde Ø3,2 ışık borusu (LED) |

Bobin telleri torusun alt noktasından yarıktan doğrudan klemense iner.

## Doğrulanmamış noktalar

- Kaide ve torus ölçüleri benzetim modelinden; gerçek parçayla karşılaştırılmadı.
- Klemensin tel giriş yönü ve SuperMini pin sırası satın alınan parçayla kontrol edilmeli.
- Kart, muhafaza ve firmware henüz birlikte denenmedi.
