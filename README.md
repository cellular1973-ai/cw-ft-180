# CW-FT-180 Torus PEMF & Skaler Alan Kontrol Sistemi

🌐 **Canlı Web Uygulaması (GitHub Pages):** [https://cellular1973-ai.github.io/cw-ft-180/](https://cellular1973-ai.github.io/cw-ft-180/)  
*(veya alternatif doğrudan link: [https://cellular1973-ai.github.io/cw-ft-180/cw_ft_180.html](https://cellular1973-ai.github.io/cw-ft-180/cw_ft_180.html))*

Bu proje, **CW-FT-180** (180 mm dış çap, 83 mm yükseklik, Ters Sarımlı Çift Telli Tam Toroid) bobinini profesyonel bir masaüstü PEMF ve Skaler Enerji cihazına dönüştürmek için geliştirilmiş Web BLE kontrol paneli, gömülü yazılımı (ESP32) ve donanım tasarımını içerir.

---

## 🚀 Öne Çıkan Özellikler

1. **Genişletilmiş Web Bluetooth Kontrol Paneli (`index.html` / `cw_ft_180.html`):**
   - **Modern 2 Sütunlu Responsive Arayüz:** Masaüstü ve tabletler için ferah ve detaylı göstergeler ("daha geniş alan").
   - **3B Dinamik Canvas Simülatörü:** CW-FT-180 torusunun 11x12 çift ters sarımını ve çalışma frekansıyla eşzamanlı atan manyetik/skaler akı çizgilerini canlı çizer.
   - **CST Studio Simülasyon Kalibrasyonu:**
     - Bobin direnci: `218 mΩ`, Endüktans: `29 µH`
     - Merkez manyetik akı: `199 µT / A` (tepe)
     - Dipol momenti: `0.25 A·m² / A` (Küçük kolyeden 12.5 kat daha güçlü)
     - 1 Metre uzak alan: `0.051 µT / A` (Küçük kolyeden 13 kat daha güçlü)
   - **Mesafe Bazlı B Alanı Hesaplayıcı:** Kullanıcının seçtiği mesafede (0 cm, 5 cm, 10 cm, 25 cm, 50 cm, 100 cm) manyetik alan zayıflamasını anlık gösterir.
   - **Topoloji Seçimi:**
     - *B-Dipol Modu:* Manyetik odaklı maksimum alan (Standart PEMF).
     - *Skaler Mod:* Manyetik dipol söndürme ($B \approx 0$), sıfır-noktası skaler enerji vorteksi.
   - **Gelişmiş Frekans Protokolleri:**
     - 100 Hz Klasik PEMF Sinüs
     - 100 Hz Darbe (%25 duty, 0.1 ms kenar ile 16 kat yüksek dB/dt)
     - 7.83 Hz Schumann Rezonansı
     - 40 Hz Gamma Beyin Dalgası
     - 432 Hz Doğal Harmonik
     - 528 Hz Solfeggio / DNA Onarım Frekansı
   - **Canlı Telemetri:** Ölçülen akım, gerilim, güç dağılımı (W/mW), dB/dt (T/s), doku E alanı (µV/m), sıcaklık ve seans geri sayımı.
   - **Dahili Demo Modu:** Donanıma bağlanmadan da tarayıcıda tam test ve simülasyon.
   - **PWA Desteği:** Masaüstü veya telefona uygulama gibi yüklenebilir, çevrimdışı çalışır.

2. **Gömülü Yazılım (`firmware/cw_ft_180_driver.ino`):**
   - ESP32-C3 / ESP32-S3 ile tam uyumlu.
   - 3.0 A tepe akım kapasitesi (DRV8871 güç katı).
   - BLE GATT servisi, anlık durum yayını ve aşırı akım koruması.

3. **Donanım & Siyah Kanal Sürücü Tasarımı (`HARDWARE_DESIGN.md`):**
   - Görseldeki gibi torusun tabanına entegre edilen siyah sürücü muhafazası.
   - 12V DC harici adaptör beslemesi ve kablo jak girişi.
