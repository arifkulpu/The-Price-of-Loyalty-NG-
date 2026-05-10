# The Price of Loyalty (NG)

A comprehensive SKSE plugin for Skyrim Anniversary Edition (v1.6.1170) that introduces a dynamic bribery and loyalty system. Optimized for maximum stability in heavy modded environments.

---

## 🚀 Full Feature List (English)

### 1. Dynamic Interaction System
*   **Native UI Integration:** Uses Skyrim's native message box system. No ImGui dependencies, ensuring 100% compatibility with UI mods and high-stability.
*   **Context-Aware Menus:** The menu automatically switches between "Bribe" and "Dismiss" options based on whether the NPC is already your ally.

### 2. NPC Traits & Logic
*   **Personality System:** NPCs are dynamically assigned traits:
    *   **Honorable:** Harder to bribe, requires more gold, but extremely loyal.
    *   **Greedy:** Loves gold, easy to bribe, but might ask for more later.
    *   **Treacherous:** Cheap to bribe, but highly likely to betray you in combat.
*   **Special Classes (Guards):** Guards have specialized pricing and loyalty checks. Bribing a lawman is expensive but grants high-tier combat support.

### 3. Combat & Recruitment Mechanics
*   **Surrender Mechanic:** Hostile NPCs (Bandits, Forsworn, etc.) cannot be bribed while at full health. You must reduce their health below **30%** to break their will and make them accept your gold.
*   **Teammate System:** Bribed NPCs become true teammates (`kPlayerTeammate`). They will draw their weapons when you do and fight your enemies.
*   **Animation Sync:** NPCs will immediately stop sandbox activities (sitting, leaning) upon being bribed to follow you.

### 4. Utility & Quality of Life
*   **Call Allies (Teleportation):** Pressing the **'B'** key while not targeting an NPC teleports all active bribed allies directly to your position.
*   **Hard Reset Dismissal:** Dismissing an NPC uses a "Hard Reset" (AI state flush) to ensure they return to their original AI packages and stop following you forever.
*   **No "Moaning" Sounds:** Cleaned up actor flags to ensure bribed NPCs don't make reanimation/zombie sounds.

---

## 🚀 Tüm Özellikler Listesi (Türkçe)

### 1. Dinamik Etkileşim Sistemi
*   **Yerel Arayüz (Native UI):** Skyrim'in kendi mesaj kutusu sistemini kullanır. ImGui bağımlılığı yoktur, bu da 3000+ modlu listelerde bile %100 stabilite sağlar.
*   **Bağlamsal Menüler:** NPC müttefikiniz olup olmamasına göre menü otomatik olarak "Rüşvet Ver" veya "Azat Et" seçenekleri arasında geçiş yapar.

### 2. NPC Kişilik ve Mantık Sistemi
*   **Kişilik Sistemi:** NPC'lere dinamik olarak karakter özellikleri atanır:
    *   **Onurlu (Honorable):** Rüşvet vermesi zordur, daha fazla altın ister ama sonuna kadar sadıktır.
    *   **Açgözlü (Greedy):** Altını sever, kolay rüşvet alır ama ileride daha fazlasını isteyebilir.
    *   **Hain (Treacherous):** Ucuza rüşvet kabul eder ancak savaşın ortasında sizi satma ihtimali yüksektir.
*   **Özel Sınıflar (Muhafızlar):** Şehir muhafızları özel fiyatlandırmaya ve sadakat kontrollerine sahiptir. Bir kanun adamını satın almak pahalıdır ancak güçlü savaş desteği sağlar.

### 3. Savaş ve Safa Katma Mekanikleri
*   **Teslim Olma Mekaniği:** Düşman NPC'ler (Haydutlar vb.) canları tamken rüşveti reddeder. Onları safınıza katmak için canlarını **%30'un altına** indirip teslim olmaya zorlamalısınız.
*   **Müttefik Sistemi:** Rüşvet alan NPC'ler gerçek bir takım arkadaşı (`kPlayerTeammate`) olur. Siz silah çektiğinizde onlar da çeker ve düşmanlarınıza saldırırlar.
*   **Animasyon Senkronizasyonu:** Rüşvet verdiğiniz an NPC yaptığı işi (oturma, yaslanma) anında bırakır ve takibe başlar.

### 4. Kullanıcı Deneyimi ve Kolaylıklar
*   **Müttefik Çağırma (Işınlanma):** Bir NPC'ye bakmıyorken **'B'** tuşuna basmak, tüm aktif müttefiklerinizi anında yanınıza ışınlar.
*   **Kesin Kovma (Hard Reset):** Bir NPC'yi azat ettiğinizde AI verileri tamamen sıfırlanır, böylece peşinizi kesin olarak bırakıp eski rutinlerine (evine veya kampına) dönerler.
*   **Ses Düzeltmesi:** NPC'lerin rüşvetten sonra "zombi" gibi inleme sesleri çıkarması engellenerek normal insan sesleri korunmuştur.

---

## 🛠 Installation / Kurulum
1.  Place `ThePriceOfLoyalty.dll` in your `Data/SKSE/Plugins` folder.
2.  Launch the game.

## 📄 License / Lisans
Copyright (c) 2026 Arif KULPU. All Rights Reserved. — Tüm Hakları Saklıdır. See [LICENSE](LICENSE.md) for details.
