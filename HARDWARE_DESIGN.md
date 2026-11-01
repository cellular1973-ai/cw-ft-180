# CW-FT-180 Sürücü Devresi ve Siyah Kanal (Housing) Donanım Rehberi

Bu doküman, **CW-FT-180** (Ters Sarımlı Çift Telli Tam Toroid) bobininin, görseldeki gibi tabanına entegre edilen siyah sürücü kanalı (driver housing) içine yerleştirilecek devre ve mekanik tasarımını içerir.

---

## 1. Fiziksel ve Elektriksel Karşılaştırma

| Özellik | Küçük Torus (CW-FT-50 Kolye) | Büyük Torus (CW-FT-180 Masaüstü) | Avantaj & Etki |
| :--- | :--- | :--- | :--- |
| **Dış Çap (D)** | 50.0 mm | **180.0 mm** | 3.6 kat daha geniş çap |
| **İç Delik Çapı (d)** | 11.0 mm | **40.0 mm** | Merkez vorteks alanı için ferah boşluk |
| **Gövde Yüksekliği (h)**| 23.2 mm | **83.0 mm** | Elips tam kesitli yüksek hacim |
| **Bakır Tel Boyu** | 2.68 m | **9.90 m** | 3.7 kat daha uzun sargı |
| **Bakır Kütlesi** | ~19 g | **~70 g** | 1 mm emaye Cu tel |
| **DC Direnç (R)** | 58.7 mΩ | **218 mΩ (0.22 Ω)** | Düşük kayıp, yüksek iletkenlik |
| **Endüktans (L)** | 8.8 µH | **29.0 µH** | Hızlı yükselme kenarı (τ = 0.13 ms) |
| **Dipol Momenti** | 0.020 A·m² | **0.250 A·m²** | **12.5 kat daha güçlü manyetik penetrasyon!** |
| **1 Metrede B Alanı** | 0.004 µT / A | **0.051 µT / A** | **13 kat daha güçlü uzaktan etki!** |
| **Maksimum Akım** | 1.5 A tepe (Li-Po) | **3.0 A tepe (Harici Besleme)** | Çok daha geniş dinamik aralık |

---

## 2. Siyah Kanal Sürücü Muhafazası (Base Module Housing)

Görselde görüldüğü gibi, torusun alt tabanına oturan siyah modül sürücü devresini ve bağlantı arayüzlerini barındırır.

```
       ┌─────────────────────────────┐
       │   CW-FT-180 Torus Bobini    │
       │    (Ø 180 mm × 83 mm)       │
       └──────────────┬──────────────┘
                      │ 4x Tel Girişi (2 Tel, 4 Uç)
       ┌──────────────┴──────────────┐
       │  SİYAH SÜRÜCÜ KANALI        │
       │  - ESP32-C3 BLE Denetleyici │
       │  - DRV8871 H-Köprüsü (3.6A) │
       │  - Step-Down Buck (12V->3V) │
       │  - INA240 Akım Şöntü        │
       │  - Neon / LED Göstergesi    │
       └──────────────┬──────────────┘
                      │
           [ 12V DC Barrel Jak ] (Harici Güç Girişi)
```

### Mekanik Boyutlar ve 3D Baskı:
- **Kanal Genişliği:** 85 mm  
- **Kanal Derinliği:** 60 mm  
- **Kanal Yüksekliği:** 32 mm  
- **Malzeme:** Siyah PETG veya Mat PLA (bobin ısınmasına karşı PETG tavsiye edilir).
- **Montaj Yuvaları:** Torusun alt oluk kılavuzlarına geçen 2 adet M3 pirinç insert ve vidalı kilitleme tırnağı.
- **Kablo Çıkışı:** Alt merkezde 5.5×2.1 mm vidalı somunlu DC Barrel jak ve kablo gerilim önleyici (strain relief).

---

## 3. Donanım Bileşen Listesi (BOM)

1. **Mikrodenetleyici:** ESP32-C3 SuperMini (Dahili BLE 5.0, 160 MHz, ultra kompakt).
2. **Güç Köprüsü (H-Bridge):** TI DRV8871 (3.6 A tepe akımı, dahili akım sınırlama, PWM girişli) veya BTN8982TA.
3. **Gerilim Regülatörü (Buck):** MP1584EN veya LM2596 DC-DC step-down modülü (12V giriş -> 2.5V – 3.3V VBUS köprü beslemesi).
4. **Akım Ölçüm Katı:** INA240A1 (G=20 V/V) + 10 mΩ 1W SMD şönt direnç (hızlı akım geri beslemesi için).
5. **Giriş Filtresi:** 1000 µF 25V Düşük ESR elektrolitik kapasitör + 10 µF seramik kapasitör.
6. **Güç Girişi:** 12V – 24V DC / min. 2A adaptör girişi (DC Barrel Jack 5.5×2.1 mm).
7. **Durum Göstergesi:** 3 mm amber neon veya yüksek parlaklıklı difüze LED + 1 kΩ direnç.

---

## 4. İki Telin Bağlantı Topolojisi (B Modu vs Skaler Mod)

CW-FT-180 üzerinde iki adet bağımsız 1 mm emaye tel vardır (Tel 1 ve Tel 2):

### A. Manyetik Dipol Modu (B Modu - Standart PEMF):
- Tel 1 ve Tel 2 **ters-seri** bağlanır.
- Poloidal yöndeki zıt akımlar gövde içinde birbirini sıfırlar.
- Toroidal yöndeki akımlar toplanır (Net 22 tur).
- **Sonuç:** Eksen boyunca maksimum dipol manyetik alanı (Merkezde 199 µT/A).

### B. Skaler Enerji / Vorteks Modu (Scalar Cancellation):
- Tel 1 ve Tel 2'nin toroidal manyetik alanları birbirine zıt olacak şekilde sürülür.
- Net manyetik dipol anında söndürülür ($B_{net} \approx 0$).
- Manyetik vektör potansiyeli $\vec{A}$ ve elektrik alan dengelenerek sıfır-noktası skaler dalga ve içe dönük vorteks alanı üretilir (Görseldeki "Rodin coil, Scalar Energy" modu).
- Devrede küçük bir sinyal rölesi veya çift MOSFET anahtar ile bu iki mod yazılımdan tek tıkla değiştirilebilir!
