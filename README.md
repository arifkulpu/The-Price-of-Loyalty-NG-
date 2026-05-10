# The Price of Loyalty (NG)

[Türkçe](#türkçe) | [English](#english)

---

<a name="türkçe"></a>
## 🇹🇷 Türkçe
Skyrim Anniversary Edition (v1.6.1170) için geliştirilmiş, NPC'lere rüşvet verme ve sadakatlerini sınama modudur. Bu sürüm, ağır mod listelerinde (3000+ plugin) bile %100 stabilite sağlamak için **Skyrim Native UI** (Yerli Arayüz) kullanacak şekilde optimize edilmiştir.

### 🚀 Özellikler
*   **Dinamik Rüşvet:** Hemen hemen her NPC ile rüşvet etkileşimine girme.
*   **Native Arayüz:** ImGui çakışmalarını önlemek için Skyrim'in kendi şık mesaj kutularını kullanır.
*   **Sadakat Sistemi:** Rüşvet verdiğiniz NPC'ler size sadık kalabilir veya rüşveti alıp size ihanet edebilir.
*   **Kolay Yapılandırma:** Ayarlarınızı doğrudan `Data/SKSE/Plugins/ThePriceOfLoyalty.ini` dosyasından yapabilirsiniz.

### 👥 Kapsanan Karakterler
*   **Kapsamda:** Vatandaşlar, Muhafızlar, Haydutlar ve diğer standart NPC'ler.
*   **İstisnalar:** Takipçileriniz (Followers), Önemli (Essential/Ölümsüz) karakterler ve ölüler rüşvet etkileşimine giremez.

### 🛠 Kurulum
1.  `ThePriceOfLoyalty.dll` dosyasını `Data/SKSE/Plugins` klasörüne kopyalayın.
2.  Oyunu çalıştırın. İlk açılışta `ThePriceOfLoyalty.ini` dosyası otomatik olarak oluşacaktır.

### 🎮 Kullanım
*   Bir NPC'nin karşısına geçin ve **'B'** tuşuna basın.

---

<a name="english"></a>
## 🇺🇸 English
A mod for Skyrim Anniversary Edition (v1.6.1170) that allows you to bribe NPCs and test their loyalty. This version is optimized using **Skyrim Native UI** to ensure 100% stability even in heavy mod lists (3000+ plugins).

### 🚀 Features
*   **Dynamic Bribing:** Interact and bribe almost any NPC in the game.
*   **Native UI:** Uses Skyrim's built-in message boxes to prevent ImGui crashes or conflicts.
*   **Loyalty Mechanics:** Bribed NPCs may remain loyal or decide to betray you after taking your gold.

### 👥 Covered NPCs
*   **Supported:** Citizens, Guards, Bandits, and other standard NPCs.
*   **Exceptions:** Followers, Essential (Invulnerable) NPCs, and dead bodies are excluded from bribery.

### 🛠 Installation
1.  Copy `ThePriceOfLoyalty.dll` to `Data/SKSE/Plugins`.
2.  Launch the game. The `ThePriceOfLoyalty.ini` file will be created automatically.

### 🎮 Usage
*   Face an NPC and press the **'B'** key to open the bribe menu.

### ⚙️ Settings (INI File)
Edit `Data/SKSE/Plugins/ThePriceOfLoyalty.ini` to change:
*   `iBribeHotkey`: Key code for interaction (Default 48 = B).
*   `fBaseDifficulty`: Cost multiplier for bribes.
*   `bEnableBackstab`: Toggle betrayal mechanics.

## 📋 Requirements
*   **SKSE64** (v2.2.6+)
*   **Address Library for SKSE Plugins**

> [!IMPORTANT]
> This mod no longer requires **SKSE Menu Framework** or **imgui.dll**. It is fully standalone.
