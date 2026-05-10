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
    *   **Treacherous:** Cheap to bribe, but **will betray you after 5-10 seconds** — you'll get a warning before the attack.
*   **Special Classes (Guards):** Guards have specialized pricing and loyalty checks. Bribing a lawman is expensive but grants high-tier combat support.
*   **Speech Skill Influence:** The higher your Speech skill, the cheaper it is to bribe. At Speech 100, the required gold is reduced by **50%**. Invest in persuasion for maximum gold efficiency.
*   **Save Game Persistence:** All NPC trait assignments are saved with your game and reloaded correctly, so traits are permanent across sessions.

### 3. Combat & Recruitment Mechanics
*   **Surrender Mechanic:** Hostile NPCs (Bandits, Forsworn, etc.) cannot be bribed while at full health. You must reduce their health below **30%** to break their will and make them accept your gold.
*   **Teammate System:** Bribed NPCs become true teammates (`kPlayerTeammate`). They will draw their weapons when you do and fight your enemies.
*   **Animation Sync:** NPCs will immediately stop sandbox activities (sitting, leaning) upon being bribed to follow you.

### 4. Utility & Quality of Life
*   **Call Allies (Teleportation):** Pressing the **'B'** key while not targeting an NPC teleports all active bribed allies directly to your position.
*   **Hard Reset Dismissal:** Dismissing an NPC uses a "Hard Reset" (AI state flush) to ensure they return to their original AI packages and stop following you forever.
*   **No "Moaning" Sounds:** Cleaned up actor flags to ensure bribed NPCs don't make reanimation/zombie sounds.

### 5. INI Configuration
The plugin reads settings from `Data/SKSE/Plugins/ThePriceOfLoyalty.ini`. The file is **auto-generated** with defaults on first launch.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `iBribeHotkey` | Integer | `48` | Virtual key code for the bribe/interaction key. Default is **`B`** (keycode 48). |
| `fBaseDifficulty` | Float | `1.0` | Global multiplier for bribe costs. `2.0` = twice as expensive, `0.5` = half price. |
| `bEnableBackstab` | Boolean | `1` | Enables the Treacherous betrayal mechanic. Set to `0` to disable all NPC betrayals. |

**Example `ThePriceOfLoyalty.ini`:**
```ini
[General]
iBribeHotkey=48
fBaseDifficulty=1.0
bEnableBackstab=1
```

---

## 🚀 Tüm Özellikler Listesi (Türkçe)

### 1. Dinamik Etkileşim Sistemi
*   **Yerel Arayüz (Native UI):** Skyrim'in kendi mesaj kutusu sistemini kullanır. ImGui bağımlılığı yoktur, bu da 3000+ modlu listelerde bile %100 stabilite sağlar.
*   **Bağlamsal Menüler:** NPC müttefikiniz olup olmamasına göre menü otomatik olarak "Rüşvet Ver" veya "Azat Et" seçenekleri arasında geçiş yapar.

### 2. NPC Kişilik ve Mantık Sistemi
*   **Kişilik Sistemi:** NPC'lere dinamik olarak karakter özellikleri atanır:
    *   **Onurlu (Honorable):** Rüşvet vermesi zordur, daha fazla altın ister ama sonuna kadar sadıktır.
    *   **Açgözlü (Greedy):** Altını sever, kolay rüşvet alır ama ileride daha fazlasını isteyebilir.
    *   **Hain (Treacherous):** Ucuza rüşvet kabul eder ancak **5-10 saniye sonra sizi satar** — saldırıdan önce bir uyarı mesajı alırsınız.
*   **Özel Sınıflar (Muhafızlar):** Şehir muhafızları özel fiyatlandırmaya ve sadakat kontrollerine sahiptir. Bir kanun adamını satın almak pahalıdır ancak güçlü savaş desteği sağlar.
*   **Konuşma Becerisi (Speech) Etkisi:** Konuşma beceriniz ne kadar yüksekse rüşvet vermek o kadar ucuza mal olur. Speech **100'de** gereken altın miktarı **%50 azalır**. Dil döndürmeyi bilen karakterler her zaman avantajlıdır.
*   **Kayıt Oyunu Desteği:** Tüm NPC kişilik atamaları kayıt dosyasına yazılır ve doğru şekilde yüklenir; yani her NPC'nin karakteri kalıcıdır.

### 3. Savaş ve Safa Katma Mekanikleri
*   **Teslim Olma Mekaniği:** Düşman NPC'ler (Haydutlar vb.) canları tamken rüşveti reddeder. Onları safınıza katmak için canlarını **%30'un altına** indirip teslim olmaya zorlamalısınız.
*   **Müttefik Sistemi:** Rüşvet alan NPC'ler gerçek bir takım arkadaşı (`kPlayerTeammate`) olur. Siz silah çektiğinizde onlar da çeker ve düşmanlarınıza saldırırlar.
*   **Animasyon Senkronizasyonu:** Rüşvet verdiğiniz an NPC yaptığı işi (oturma, yaslanma) anında bırakır ve takibe başlar.

### 4. Kullanıcı Deneyimi ve Kolaylıklar
*   **Müttefik Çağırma (Işınlanma):** Bir NPC'ye bakmıyorken **'B'** tuşuna basmak, tüm aktif müttefiklerinizi anında yanınıza ışınlar.
*   **Kesin Kovma (Hard Reset):** Bir NPC'yi azat ettiğinizde AI verileri tamamen sıfırlanır, böylece peşinizi kesin olarak bırakıp eski rutinlerine (evine veya kampına) dönerler.
*   **Ses Düzeltmesi:** NPC'lerin rüşvetten sonra "zombi" gibi inleme sesleri çıkarması engellenerek normal insan sesleri korunmuştur.

### 5. INI Ayarları
Eklenti ayarlarını `Data/SKSE/Plugins/ThePriceOfLoyalty.ini` dosyasından okur. Bu dosya ilk çalıştırmada **otomatik oluşturulur**.

| Ayar | Tür | Varsayılan | Açıklama |
|------|-----|-----------|----------|
| `iBribeHotkey` | Tam sayı | `48` | Rüşvet/etkileşim tuşunun sanal tuş kodu. Varsayılan **`B`** tuşudur (kod: 48). |
| `fBaseDifficulty` | Ondalık | `1.0` | Rüşvet maliyetleri için global çarpan. `2.0` = iki katı pahalı, `0.5` = yarı fiyat. |
| `bEnableBackstab` | Boolean | `1` | Hain ihanet mekaniğini etkinleştirir. `0` yaparsanız hiçbir NPC ihanet etmez. |

**Örnek `ThePriceOfLoyalty.ini`:**
```ini
[General]
iBribeHotkey=48
fBaseDifficulty=1.0
bEnableBackstab=1
```

---

## 🛠 Installation / Kurulum
1.  Place `ThePriceOfLoyalty.dll` in your `Data/SKSE/Plugins` folder.
2.  *(Optional)* Edit `ThePriceOfLoyalty.ini` in the same folder to customize hotkey, difficulty, and backstab settings. The file is **auto-created with defaults** on first launch.
3.  Launch the game.

## 📄 License / Lisans
Copyright (c) 2026 Arif KULPU. All Rights Reserved. — Tüm Hakları Saklıdır. See [LICENSE](LICENSE.md) for details.
