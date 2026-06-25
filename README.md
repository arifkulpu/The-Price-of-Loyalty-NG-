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
    *   **Treacherous:** Cheap to bribe, but **will betray you after a configurable delay (default 1-5 minutes)** — you'll get a warning before the attack.
        *   *Lawless Restriction:* This trait can only be assigned to bandit-like actors (Bandits, Forsworn, Warlocks, Vampires). Regular law-abiding NPCs (Guards, Citizens, Imperial/Stormcloak Soldiers, Merchants) are completely immune to this trait and will never betray you (they will only roll Greedy or Honorable).
    *   **Dynamic Bandit Betrayal:** Bandit-like NPCs (Bandits, Forsworn, etc.) now have a chance to become Treacherous based on your bribe:
        *   **Low Bribe:** **60%** chance of betrayal.
        *   **High Bribe:** **15%** chance of betrayal.
*   **Unique NPC Protections:** Unique NPCs (quest-givers, townsfolk) are too honorable to accept bribes and cannot be recruited (a message plays: *"This unique individual is too honorable to accept bribes."*).
    *   *Merchant Exception:* Unique merchants, blacksmiths, and service providers (like Belethor, Adrienne Avenicci, and even essential traders like **Ri'saad**) are exempt from this restriction and can be bribed to spawn a substitute mercenary bodyguard.
*   **Special Classes (Guards):** Guards have specialized pricing and loyalty checks. Bribing a lawman is expensive but grants high-tier combat support.
*   **Speech Skill Influence:** The higher your Speech skill, the cheaper it is to bribe. At Speech 100, the required gold is reduced by **50%**. Invest in persuasion for maximum gold efficiency.
*   **Bribe Success Probability & Safe Bribery:** Success is not guaranteed. The chance depends on your offer. However, **your gold is only deducted if the bribe attempt succeeds.** If the NPC rejects the offer, you keep your gold!
    *   **Base Chance:** Offering the exact required amount grants a **75%** success rate.
    *   **Risk Factor:** Success is capped at **95%**. There is always a small chance of refusal, even with high bribes!
*   **Save Game Persistence:** All NPC trait assignments are saved with your game and reloaded correctly, so traits are permanent across sessions.

### 3. Combat & Recruitment Mechanics
*   **Surrender Mechanic:** Hostile NPCs (Bandits, Forsworn, etc.) cannot be bribed while at full health. You must reduce their health below **30%** to break their will and make them accept your gold.
*   **Teammate System:** Bribed NPCs become true teammates (`kPlayerTeammate`). They will draw their weapons when you do and fight your enemies. A permanent relationship rank of **3 (Ally)** is established between both parties, ensuring stability and preventing sudden AI-driven combat re-entry.
*   **Dual Faction & Friendly Fire Protection:** NPCs are added to both `PotentialFollowerFaction` (`0x0005C84D`) and `CurrentFollowerFaction` (`0x0005C84E`). This ensures that the engine's built-in follower Friendly Fire dampening rules apply, preventing allies from turning on you due to stray spell/sword hits in combat.
*   **Peaceful Guardian AI (Aggression & Assistance Balance):** Hired allies are set to `Aggression = 0` (Unaggressive) and `Assistance = 2` (Helps Friends & Allies) while following you. This guarantees they will **never pre-emptively attack guards or innocent civilians** in cities (like Riverwood), but will **instantly jump in to defend you and other companions** the second a battle breaks out. On betrayal or dismissal, their aggression is restored to make them hostile again.
*   **Instant Potion Healing:** Upon accepting a bribe, the NPC's **Health, Magicka, and Stamina are instantly restored to 100%**, simulating a healing potion being shared. The notification reads `"NPC is now your loyal teammate. (Healed!)"`.
*   **AI Package Flushing & TDM Compatibility:** Immediately after the bribe, an engine-level `EvaluatePackage(true, true)` and `StopCombat()` is run on all surrounding allies. This flushes their AI combat target list in milliseconds, immediately stopping followers from targeting each other and making them fully compatible with mods like `True Directional Movement (TDM)` and `SmartTargetingNPC`.
*   **Animation Sync:** NPCs will immediately stop sandbox activities (sitting, leaning) upon being bribed to follow you.

### 4. Immersive Combat Classes & Gender-Aware Names
*   **Diverse Combat Classes & Playable Races (Method 2 Spawns):** When bribing unique/quest/merchant NPCs who must remain at their posts, the spawned substitute bodyguard is no longer a generic warrior. They will dynamically spawn across **all of Skyrim's playable races** (including Nords, Imperials, Redguards, Orcs, Bosmer, Dunmer, Altmer, Bretons, Argonians, and Khajiits) as one of four distinct scaled combat archetypes:
    *   **Melee Guardian:** Heavy armor protector wielding a one-handed sword and a shield (Tank) [Spawns as Nord, Imperial, Redguard, Orc, Khajiit, or Argonian].
    *   **Two-Handed Berserker:** Heavy armor warrior wielding a massive battleaxe or greatsword (DPS) [Spawns as Nord, Imperial, Orc, Redguard, or Dunmer].
    *   **Agile Archer:** Swift light-armor marksman wielding bows and daggers (Ranger) [Spawns as Nord, Imperial, Bosmer, Khajiit, or Argonian].
    *   **Destruction Battlemage:** Resilient spellcaster hurling fireballs, ice storms, and lightning bolts (Mage) [Spawns as Altmer, Dunmer, Breton, or Argonian].
*   **Gender-Aware Dynamic Naming (2,200+ Combinations):** Spawning allies or bribing generic hostiles automatically triggers a background gender detection routine (`GetSex()`). The NPC is assigned a lore-friendly first name—including legendary Norse, Imperial, and heroic Turkish names (e.g., Tarkan, Tomris, Mete, Ertugrul, Asena)—and a class-tailored roleplay title (e.g., *"the Spell-Weaver"* for mages, *"Bone-Breaker"* for two-handed warriors, *"Shield-Wall"* for tanks, *"Swift-Arrow"* for archers).
*   **1-Day Dynamic Clone Garbage Collector:** Clones spawned via identity swap or substitution bodyguards do not persist in the world indefinitely once dismissed or when they betray you. The SKSE engine automatically schedules them for total deletion from the game database exactly 1 in-game day (24 hours) after their departure, keeping your savegame clean and memory usage extremely lightweight.
*   **Apprentice vs. Veteran Template Tiers & Safe Stat Scaling:** To prevent the vanilla game engine's stat recalculation bugs (which could cause 1 HP glitches with traditional console commands), the mod uses dynamic vanilla template tiers combined with safe, engine-level ActorValue modifications:
    *   **Low Bribe (Apprentice Tier):** Spawns low-level apprentice templates (Level 1-5 Bandit Melee, Level 4 Imperial Recruits, Level 5 Bandit Thugs, Level 4 Imperial Archers, Level 1 Apprentice Necromancers). Their base Health, Magicka, and Stamina are safely scaled down by **-25%** with completely full health bars.
    *   **High Bribe (Veteran Tier):** Spawns elite-tier veteran templates (Level 10+ Imperial Soldiers, Level 10+ Stormcloak Soldiers, Level 20+ Whiterun Guards, Level 12+ Vigilant of Stendarr Mages, Level 16+ Pyromancers). Their base Health, Magicka, and Stamina are safely scaled up by **+35%** for premium combat capabilities.
*   **Balanced Civil War Spawns:** In High Bribes, Melee, Two-Handed, and Archer classes are fully balanced with Whiterun Guards, Imperial Soldiers, and Stormcloak Soldiers, ensuring a lore-friendly, equal-probability civil war distribution.
*   **Opposing Faction Peace & Brotherhood Harmony:** Even though Stormcloaks, Imperials, and Guards are hostile in vanilla, their original combat faction lists are dynamically stripped upon recruitment. They are added to the player's team and marked as mutual allies, allowing you to build a peaceful, cooperative army of diverse soldiers.
*   **Legendary Easter Egg Names (3% Chance):** There is a rare 3% chance that any newly recruited mercenary or generic NPC will spawn as a legendary mythical figure:
    *   **Male Recruits:** Named **"King Arif"** directly.
    *   **Female Recruits:** Named **"Queen Arif"** directly.
*   **Personalized Dismissal Notifications:** When parting ways with your allies, notifications and message boxes print their actual custom display names (e.g., *"Camilla Swift-Arrow has been dismissed."* or *"Bjorn Shield-Wall is not happy about being dismissed and attacks!"*). You'll always know exactly who you are dismissing.
*   **Peaceful vs Hostile Dismissal:** Only bandit-like NPCs will turn hostile or flee when dismissed. Guards, citizens, and mercenaries will politely return to their duties with a peaceful notification (e.g. *"Lydia leaves peacefully and returns to their duties."*).

### 5. Advanced Engine Fixes & Quality of Life
*   **Engine-Level Kalıcılık (kPersistent Flag):** Hired allies are marked as persistent at the game engine level (`kPersistent`). This prevents the engine's garbage collector from deleting or resetting them when you travel away, fast travel, or load new cells.
*   **Dynamic State Self-Healing:** Whenever you fast travel, change cells, or call allies, the system checks if the engine attempted to reset the actor. If so, it instantly reconstructs their teammate flag, friendly factions, relationship ranks, and unaggressive AI state in milliseconds, preventing them from turning back into hostile bandits.
*   **Dead Ally Purge (No Corpse Teleportation):** If a hired ally dies in combat, the plugin automatically detects their death, purges their state, and erases them from the tracking maps. This prevents their dead bodies from teleporting to you when changing cells or calling allies.
*   **Persistent Relationship Map (Mutual Alliance):** Relationships between allies are managed via a persistent tracking map rather than an active-distance-list. This guarantees that far-away teammates (like archers or mages) are immediately registered as mutual allies of close-range fighters, completely eliminating combat confusion and friendly-fire mutinies.
*   **Call Allies (Teleportation):** Pressing the **'B'** key while not targeting an NPC teleports all active, living bribed allies directly to your position.
*   **Auto-Teleport on Transitions:** Whenever you fast travel, go through loading doors, or enter loaded areas, all your active hired allies automatically teleport to your side.
*   **No "Moaning" Sounds:** Cleaned up actor flags to ensure bribed NPCs don't make reanimation/zombie sounds.

### 6. INI Configuration
The plugin reads settings from `Data/SKSE/Plugins/ThePriceOfLoyalty.ini`. The file is **auto-generated** with defaults on first launch.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `iBribeHotkey` | Integer | `48` | Virtual key code for the bribe/interaction key. Default is **`B`** (keycode 48). |
| `iBaseBribeCost` | Integer | `500` | The base gold cost required to bribe an NPC. |
| `iCostPerLevel` | Integer | `50` | The additional gold cost added per level of the NPC. |
| `iBetrayalMinTime`| Integer | `60` | Minimum time (in seconds) before a treacherous NPC betrays you. |
| `iBetrayalMaxTime`| Integer | `300` | Maximum time (in seconds) before a treacherous NPC betrays you. |
| `iBetrayalChanceLowBribe` | Integer | `60` | The percentage chance (0-100) that a bandit becomes treacherous after a low offer bribe. |
| `iBetrayalChanceHighBribe` | Integer | `15` | The percentage chance (0-100) that a bandit becomes treacherous after a high offer bribe. |
| `fBaseDifficulty` | Float | `1.0` | Global multiplier for bribe costs. `2.0` = twice as expensive, `0.5` = half price. |
| `bEnableBackstab` | Boolean | `1` | Enables the Treacherous betrayal mechanic. Set to `0` to disable all NPC betrayals. |

**Example `ThePriceOfLoyalty.ini`:**
```ini
[General]
iBribeHotkey=48
iBaseBribeCost=500
iCostPerLevel=50
iBetrayalMinTime=60
iBetrayalMaxTime=300
iBetrayalChanceLowBribe=60
iBetrayalChanceHighBribe=15
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
    *   **Hain (Treacherous):** Ucuza rüşvet kabul eder ancak **ayarlanabilir bir süre sonra (varsayılan 1-5 dakika) sizi satar** — saldırıdan önce bir uyarı mesajı alırsınız.
        *   *Kanunsuz Kısıtlaması:* Bu özellik yalnızca haydut tipi karakterlere (Haydutlar, Yeminliler, Kara Büyücüler, Vampirler) atanabilir. Kanunlara uyan normal NPC'ler (Muhafızlar, Vatandaşlar, İmparatorluk/Fırtınapelerin Askerleri, Esnaflar) bu özellikten tamamen muaftır ve size asla ihanet etmezler (yalnızca Açgözlü veya Onurlu olabilirler).
    *   **Dinamik Haydut İhaneti:** Haydut tipi NPC'ler (Haydutlar, Yeminliler vb.) rüşvet miktarına göre Hainleşebilirler:
        *   **Düşük Rüşvet:** **%60** ihanet olasılığı.
        *   **Yüksek Rüşvet:** **%15** ihanet olasılığı.
*   **Benzersiz NPC Korumaları:** Hikayesel ve benzersiz (Unique) karakterler rüşvet kabul etmeyecek kadar onurludur (Sol üstte *"This unique individual is too honorable to accept bribes."* mesajı çıkar).
    *   *Tüccar/Esnaf Muafiyeti:* Belethor, Adrienne Avenicci ve hatta ölümsüz olan **Ri'saad** gibi benzersiz tüccarlar bu engelden muaftır. Rüşvet aldıklarında dükkanlarında kalırlar ama yerlerine dövüşecek bir yedek paralı asker spawn ederler.
*   **Özel Sınıflar (Muhafızlar):** Şehir muhafızları özel fiyatlandırmaya ve sadakat kontrollerine sahiptir. Bir kanun adamını satın almak pahalıdır ancak güçlü savaş desteği sağlar.
*   **Konuşma Becerisi (Speech) Etkisi:** Konuşma beceriniz ne kadar yüksekse rüşvet vermek o kadar ucuza mal olur. Speech **100'de** gereken altın miktarı **%50 azalır**. Dil döndürmeyi bilen karakterler her zaman avantajlıdır.
*   **Rüşvet Başarı Olasılığı & Güvenli Rüşvet:** Rüşvet vermek artık garanti değildir. Başarı şansı teklifinize bağlıdır. Ancak **altınınız sadece rüşvet başarıyla kabul edildiğinde envanterinizden eksilir.** Eğer teklifiniz reddedilirse altın cebinizde kalır!
    *   **Temel Şans:** İstenen miktarı tam ödemek **%75** başarı şansı verir.
    *   **Risk Faktörü:** Başarı şansı **%95** ile sınırlıdır. En yüksek rüşveti verseniz bile her zaman küçük bir reddedilme riski vardır!
*   **Kayıt Oyunu Desteği:** Tüm NPC kişilik atamaları kayıt dosyasına yazılır ve doğru şekilde yüklenir; yani her NPC'nin karakteri kalıcıdır.

### 3. Savaş ve Safa Katma Mekanikleri
*   **Teslim Olma Mekaniği:** Düşman NPC'ler (Haydutlar vb.) canları tamken rüşveti reddeder. Onları safınıza katmak için canlarını **%30'un altına** indirip teslim olmaya zorlamalısınız.
*   **Müttefik Sistemi:** Rüşvet alan NPC'ler gerçek bir takım arkadaşı (`kPlayerTeammate`) olur. Siz silah çektiğinizde onlar da çeker ve düşmanlarınıza saldırırlar. Ayrıca oyuncu ile NPC arasında kalıcı olarak **3 (Müttefik/Ally)** ilişki derecesi kurularak yapay zekâ hatalarından dolayı aniden tekrar saldırmaları kesin olarak engellenir.
*   **Çift Faction ve Dost Ateşi Koruması:** Müttefik yapılan NPC'ler hem `PotentialFollowerFaction` (`0x0005C84D`) hem de oyunun resmi yoldaş grubu olan **`CurrentFollowerFaction` (`0x0005C84E`)** grubuna eklenir. Bu sayede motorun kendi dost ateşi (Friendly Fire) koruması etkinleşir; savaş esnasında kaza ara çarpan büyü/kılıç darbeleri nedeniyle size düşman olmazlar.
*   **Şehir ve Muhafız Dostluğu (Faction & Suç Temizliği):** Müttefik yapılan NPC'ler, oyuncunun kendi grubu olan **`PlayerFaction` (`0x00000013`)** grubuna eklenir. Ayrıca üzerlerindeki düşman suç grupları (**`CrimeFactionBandit`**, **`CrimeFactionForsworn`**, **`CrimeFactionWarlock`**) tamamen silinir. Bu sayede şehirlerde, hanlarda ve yerleşim yerlerinde muhafızlar ve siviller tarafından oyuncunun yandaşı olarak barışçıl algılanırlar; sivil uyarı diyalogları ve muhafız saldırıları kesin olarak önlenir.
*   **Barışçıl Muhafız Yapay Zekası (Aggression & Assistance Dengesi):** Müttefik haydutlar yanınızdayken **`Aggression = 0` (Barışçıl)** ve **`Assistance = 2` (Dostlara Yardım Eder)** olarak ayarlanır. Bu sayede şehirlerde veya yerleşkelerde (Riverwood vb.) muhafızlara ve masum sivillere **asla durup dururken saldırmazlar.** Ancak size veya müttefiklerinize bir düşman saldırdığı an tam sadakatle yardıma koşarlar! İhanet veya kovulma anında agresyonları normale döner.
*   **Anlık İksir İyileştirmesi:** Rüşvet kabul edildiği anda NPC'nin **Can (Health), Büyü (Magicka) ve Kondisyon (Stamina) değerleri anında %100'e doldurulur** (sanki onlara şifa iksiri vermişsiniz gibi). Sol üstte `"NPC is now your loyal teammate. (Healed!)"` bildirimi gösterilir.
*   **Yapay Zeka Paket Sıfırlama & TDM Uyumluluğu:** Rüşvet işleminin hemen ardından çevredeki tüm müttefiklerin üzerinde motor seviyesinde `EvaluatePackage(true, true)` ve `StopCombat()` çalıştırılır. Bu, yapay zekanın savaş hedeflerini milisaniyeler içinde sıfırlayarak yoldaşların birbirleriyle dövüşmesini kesin olarak engeller ve `True Directional Movement (TDM)` ile `SmartTargetingNPC` modlarıyla tam uyumlu çalışmasını sağlar.
*   **Animasyon Senkronizasyonu:** Rüşvet verdiğiniz an NPC yaptığı işi (oturma, yaslanma) anında bırakır ve takibe başlar.

### 4. Çeşitlendirilmiş Sınıflar ve Cinsiyete Duyarlı Dinamik İsimler
*   **Çeşitlendirilmiş Sınıf ve Tüm Oynanabilir Irklar (Yöntem 2 Muhafızları):** Görev yerlerini bırakamayan esnaf veya quest NPC'lerine rüşvet verdiğinizde yerlerine gelen yedek muhafızlar artık tekdüze savaşçılar değildir. Oyundaki **tüm oynanabilir ırklardan (Nord, Imperial, Redguard, Orc, Bosmer, Dunmer, Altmer, Breton, Argonian ve Khajiit)** rastgele ve dinamik olarak 4 farklı gelişmiş savaş sınıfından biriyle spawn olurlar:
    *   **Kalkanlı Muhafız (Melee Guardian):** Tek el silah ve kalkan taşıyan ağır zırhlı koruyucu (Tank) [Nord, Imperial, Redguard, Orc, Khajiit veya Argonian olarak spawn olabilir].
    *   **Çift Edim Savaşçısı (Two-Handed Berserker):** Devasa baltalar veya büyük kılıçlar sallayan ağır zırhlı savaşçı (DPS) [Nord, Imperial, Orc, Redguard veya Dunmer olarak spawn olabilir].
    *   **Keskin Nişancı Okçu (Agile Archer):** Yay kullanan ve hızlı hançerler taşıyan hafif zırhlı nişancı (Okçu) [Nord, Imperial, Bosmer, Khajiit veya Argonian olarak spawn olabilir].
    *   **Yıkım Savaş Büyücüsü (Destruction Battlemage):** Alev fırtınaları ve yıldırımlar saçan dirençli büyücü (Savaş Büyücüsü) [Altmer, Dunmer, Breton veya Argonian olarak spawn olabilir].
*   **Cinsiyete Duyarlı Dinamik İsimlendirme (2.200+ Kombinasyon):** Bir klon çağrıldığında veya sıradan bir düşmana rüşvet verildiğinde sistem arka planda karakterin cinsiyetini tespit eder (`base->GetSex()`). Cinsiyetine tamamen uyumlu lore-friendly ön adlar—erkekler için efsanevi İskandinav, İmparatorluk adlarının yanı sıra kahramanlık dolu **Türkçe adlar (Tarkan, Mete, Ertuğrul, Alp vb.)**, kadınlar için ise **(Tomris, Asena, Sabiha, Nene vb.)** gibi tarihi/mitolojik adlar—ile **kendi savaş sınıfıyla mükemmel uyumlu ünvanlar** (Büyücüler için *"the Spell-Weaver"*, Çift El kullananlar için *"Bone-Breaker"*, Okçular için *"Swift-Arrow"*, Kalkanlılar için *"Shield-Wall"*) birleştirilerek dinamik olarak atanır.
*   **1 Günlük Otomatik Klon Temizliği (Garbage Collector):** Kimlik değişimiyle veya yedek muhafız olarak çağrılan klonlar, yollarınızı ayırdığınızda (kovduğunuzda) veya size ihanet ettiklerinde oyunda sonsuza dek kalıp gereksiz kalabalık ve savegame yükü oluşturmazlar. SKSE altyapısı, azat edildikten tam 1 oyun içi gün (24 saat) sonra bu klonları oyundan tamamen siler (Disable & SetDelete), böylece kayıt dosyanız her zaman hafif ve tertemiz kalır.
*   **Acemi & Kıdemli Şablon Rütbeleri ve Güvenli Nitelik Ölçekleme:** Klasik seviye düşürme komutlarının Skyrim motorunda can barını bozması (1 HP kalması) hatasını tamamen çözmek için mod, dinamik rütbe şablonları ile motor seviyesinde güvenli `SetBaseActorValue` modifikasyonlarını birleştirir:
    *   **Düşük Rüşvet (Acemi Sınıfı):** Doğal olarak düşük seviyeli acemi şablonlar doğar (1-5 lvl arası Haydutlar, Acemi İmparatorluk Erleri, Okçular ve Büyücüler). Can, Büyü ve Kondisyonları **%25 oranında kısılır** (can barları tamamen dolu ve hatasız görünür).
    *   **Yüksek Rüşvet (Kıdemli / Elit Sınıfı):** Güçlü elite veteran şablonlar doğar (Kıdemli İmparatorluk Askerleri, Fırtınapelerin Ağır Zırhlıları, Akçay Muhafızları, Stendarr Büyücüleri). Can, Büyü ve Kondisyonları **%35 oranında artırılır**.
*   **Dengeli İç Savaş Muhafız Havuzu:** Yüksek rüşvetlerde Yakın Dövüş, Çift Elli ve Okçu sınıflarında İmparatorluk Askerleri, Fırtınapelerin Askerleri ve Akçay Muhafızları tamamen dengeli ve adil olasılıklarla doğar.
*   **Düşman Fraksiyon Barışı ve Müttefik Kardeşliği:** Normalde birbirine düşman olan İmparatorluk, Fırtınapelerin, Muhafız ve Haydut yoldaşların tüm eski düşmanlık grupları (faction'lar) rüşvet anında temizlenir. Oyuncunun grubuna eklenip aralarında kalıcı "Müttefik" ilişkisi kurulur; böylece birbirlerine saldırmadan omuz omuza senin için savaşırlar.
*   **Efsanevi Easter Egg İsimleri (%3 Şans):** Yeni çağrılan her muhafızın veya rüşvet alan sıradan haydutun %3 efsanevi ihtimalle kurucunun şerefine özel bir unvan alma şansı vardır:
    *   **Erkek Yoldaşlar:** Doğrudan **"King Arif"** adını alır.
    *   **Kadın Yoldaşlar:** Doğrudan **"Queen Arif"** (Kraliçe Arif) adını alır.
*   **Kişiselleştirilmiş Kovma Bildirimleri:** Yoldaşlarınızı azat ettiğinizde veya size saldırdıklarında ekranda çıkan bildirimlerde ve mesaj kutularında onların gerçek özel isimleri basılır (Örn: *"Camilla Swift-Arrow has been dismissed."* veya *"Bjorn Shield-Wall is not happy about being dismissed and attacks!"*). Kimi azat ettiğinizi her zaman tam olarak bilirsiniz.
*   **Barışçıl veya Saldırgan Kovma:** Sadece haydut kökenli NPC'ler kovulduğunda saldırabilir veya kaçabilir. Korumalar, vatandaşlar ve paralı askerler kovulduklarında sakin bir şekilde görevlerine dönerler (Örn: *"Lydia leaves peacefully and returns to their duties."* bildirimi çıkar).

### 5. Gelişmiş Motor Düzeltmeleri ve Konfor Özellikleri
*   **Motor Seviyesinde Kalıcılık (kPersistent Bayrağı):** Rüşvet verilen tüm haydutlar motor seviyesinde **persistent** olarak işaretlenir. Bu sayede siz uzaklaştığınızda veya hızlı seyahat ettiğinizde oyun motorunun çöp toplayıcısı tarafından silinmeleri veya sıfırlanmaları engellenir.
*   **Dinamik Yapay Zeka Kendini Onarma (Self-Healing):** Işınlanma, hızlı seyahat veya hücre yüklenmelerinde oyun motorunun haydutları sıfırlamaya çalışması durumunda, sistem durumu milisaniyeler içinde teşhis eder; dost yoldaş faction'larını, agresyon değerlerini ve müttefik ilişkilerini otomatik olarak baştan aşağı sessizce onarır.
*   **Ölü Temizleme Sistemi (Ceset Işınlanması Engeli):** Satın aldığınız yoldaş haydutlar savaşta ölürse, sistem onların ölümünü anında tespit eder, yapay zeka durumlarını kapatır ve takip haritamızdan tamamen siler. Böylece ölen yoldaşların cesetleri asla peşinizden ışınlanmaz.
*   **Kalıcı Takip Haritası (Mesafe Bağımsız Müttefiklik):** Yoldaşların birbiriyle olan ilişkileri anlık çevre listesi yerine kalıcı takip haritası üzerinden güncellenir. Bu sayede savaştaki uzaktaki okçular veya büyücüler mesafe nedeniyle gözden kaçmaz; hepsi birbirini müttefik olarak tanır ve takım içi kavgalar/dost ateşi isyanları tamamen engellenir.
*   **Müttefik Çağırma (Işınlanma):** Bir NPC'ye bakmıyorken **'B'** tuşuna basmak, o an hayatta olan tüm aktif müttefiklerinizi anında yanınıza ışınlar.
*   **Otomatik Hücre & Geçiş Temizliği:** Hızlı seyahat (fast travel) yaptığınızda, han/mağara gibi yükleme ekranlı kapılardan geçtiğinizde veya hücre değiştiğinde (mesafe ne olursa olsun) müttefiklerinizin **savaş durumları ve alarmları motor seviyesinde otomatik olarak sıfırlanır, silahları kınına sokulur** ve gerekirse yanınıza ışınlanırlar. Böylece geçişlerde AI takılmaları veya sebepsiz anlık kavgalar tamamen önlenir.
*   **Ses Düzeltmesi:** NPC'lerin rüşvetten sonra "zombi" gibi inleme sesleri çıkarması engellenerek normal insan sesleri korunmuştur.

### 6. INI Ayarları
Eklenti ayarlarını `Data/SKSE/Plugins/ThePriceOfLoyalty.ini` dosyasından okur. Bu dosya ilk çalıştırmada **otomatik oluşturulur**.

| Ayar | Tür | Varsayılan | Açıklama |
|------|-----|-----------|----------|
| `iBribeHotkey` | Tam sayı | `48` | Rüşvet/etkileşim tuşunun sanal tuş kodu. Varsayılan **`B`** tuşudur (kod: 48). |
| `iBaseBribeCost` | Tam sayı | `500` | Bir NPC'ye rüşvet vermek için gereken taban altın maliyeti. |
| `iCostPerLevel` | Tam sayı | `50` | NPC'nin her seviyesi için eklenen ekstra altın maliyeti. |
| `iBetrayalMinTime`| Tam sayı | `60` | Hain NPC'nin size ihanet etmesinden önceki minimum süre (saniye). |
| `iBetrayalMaxTime`| Tam sayı | `300` | Hain NPC'nin size ihanet etmesinden önceki maksimum süre (saniye). |
| `iBetrayalChanceLowBribe` | Tam sayı | `60` | Düşük rüşvet teklif edildiğinde haydutun hain olma şansı (% olarak, 0-100 arası). |
| `iBetrayalChanceHighBribe`| Tam sayı | `15` | Yüksek rüşvet teklif edildiğinde haydutun hain olma şansı (% olarak, 0-100 arası). |
| `fBaseDifficulty` | Ondalık | `1.0` | Rüşvet maliyetleri için global çarpan. `2.0` = iki katı pahalı, `0.5` = yarı fiyat. |
| `bEnableBackstab` | Boolean | `1` | Hain ihanet mekaniğini etkinleştirir. `0` yaparsanız hiçbir NPC ihanet etmez. |

**Örnek `ThePriceOfLoyalty.ini`:**
```ini
[General]
iBribeHotkey=48
iBaseBribeCost=500
iCostPerLevel=50
iBetrayalMinTime=60
iBetrayalMaxTime=300
iBetrayalChanceLowBribe=60
iBetrayalChanceHighBribe=15
fBaseDifficulty=1.0
bEnableBackstab=1
```

---

---

## 🛠 Installation / Kurulum
1.  Place `ThePriceOfLoyalty.dll` in your `Data/SKSE/Plugins` folder.
2.  *(Optional)* Edit `ThePriceOfLoyalty.ini` in the same folder to customize hotkey, difficulty, and backstab settings. The file is **auto-created with defaults** on first launch.
3.  Launch the game.

---

## 📋 Update Notes / Güncelleme Notları

### v1.3.0 — 2026-06-25

---

#### 🇬🇧 English

##### ⚔️ NFF (Nether's Follower Framework) Compatibility
- Recruited NPCs are now fully compatible with NFF out of the box.
- `CurrentFollowerFaction` rank is now set to **1** (actively following) instead of 0, which is the exact rank NFF polls to auto-import new followers into its management system.
- On dismissal, the faction removal automatically signals NFF to cleanly release its tracking — no conflicts or ghost followers.

##### 🧠 AI Behavior Reset System (`ResetActorAI`)
- A new internal `ResetActorAI()` function was added to cleanly flush stale AI state whenever an NPC is recruited or dismissed.
- On **recruitment**: clears any pre-existing combat lock, look-at target, and interaction state so the NPC immediately behaves as a fresh ally.
- On **dismissal**: forces the NPC to fully exit follower AI mode and resume their original sandbox/patrol/shop package instead of freezing in place.
- Uses the engine's `kResetAI` bit flag combined with a two-pass `EvaluatePackage` — one immediate and one deferred via the SKSE task interface — so NFF's Papyrus cleanup scripts get a full frame to run before the engine re-evaluates AI packages. This eliminates race conditions between the C++ plugin and NFF's Papyrus layer.

##### 💪 Player-Level-Aware Health Scaling
- Replaced the old flat-percentage health system with a dynamic, player-level-aware scaling formula.
- Ally health is now computed as: `max(NPC_natural_health, PlayerHP × multiplier, hard_floor)`
  - **Low Bribe (Apprentice):** `PlayerHP × 0.80`, minimum **200 HP**
  - **High Bribe (Veteran):** `PlayerHP × 1.20`, minimum **350 HP**
- This means allies always scale with your character's progression — a level 50 player will have genuinely powerful bodyguards, while a level 5 player still gets meaningful protection.
- Magicka and Stamina follow the same scaling logic with their own multipliers and floors.
- The log tag `[BRIBE_STATS]` now records natural HP, player HP, and final HP for easy debugging.

##### 🩺 Automatic Health Regeneration
- Recruited NPCs now automatically regenerate health, eliminating the need to manually cast healing spells on them between fights.
- **Out of combat:** `HealRate = 3.0` → recovers approximately full health in ~33 seconds.
- **In combat:** `HealRateMult = 35` → slow but steady in-combat regeneration (~1% per second), giving allies a fighting chance in prolonged battles.
- **Stamina:** `StaminaRate = 8.0` → stamina recovers quickly so allies can keep swinging without running out mid-fight.
- *(Vanilla NPC default for reference: `HealRate ≈ 0.7`, meaning full recovery takes ~140 seconds — effectively no regeneration.)*

---

#### 🇹🇷 Türkçe

##### ⚔️ NFF (Nether's Follower Framework) Uyumluluğu
- İşe alınan NPC'ler artık NFF ile kutu açılışında tam uyumlu çalışıyor.
- `CurrentFollowerFaction` rank'ı artık 0 yerine **1** (aktif takip modunda) olarak ayarlanıyor. Bu tam olarak NFF'nin yeni takipçileri otomatik sisteme almak için kontrol ettiği rank.
- Kovulma anında faction kaldırma işlemi NFF'ye otomatik sinyal gönderiyor ve NFF kendi takibini temiz biçimde bırakıyor — hayalet takipçi veya çakışma yok.

##### 🧠 Yapay Zeka Davranış Sıfırlama Sistemi (`ResetActorAI`)
- NPC işe alındığında veya kovulduğunda eski AI durumunu temizlemek için yeni bir dahili `ResetActorAI()` fonksiyonu eklendi.
- **İşe alımda:** Önceki savaş kilidi, bak-at hedefi ve etkileşim durumu temizlenerek NPC anında yeni bir müttefik gibi davranmaya başlıyor.
- **Kovulmada:** NPC'nin takipçi AI modundan tam olarak çıkması ve yerinde donup kalmak yerine özgün sandbox/devriye/dükkan paketine geri dönmesi sağlanıyor.
- Motorun `kResetAI` bit bayrağı ile birlikte iki aşamalı `EvaluatePackage` kullanılıyor — biri anında, diğeri SKSE görev arayüzü üzerinden ertelenerek — NFF'nin Papyrus temizleme scriptleri çalışmadan önce motor AI paketlerini yeniden değerlendirmek zorunda kalmıyor. Bu, C++ eklentisi ile NFF'nin Papyrus katmanı arasındaki yarış koşullarını (race condition) tamamen ortadan kaldırıyor.

##### 💪 Oyuncu Seviyesine Göre Can Ölçekleme
- Eski sabit yüzde tabanlı can sistemi, dinamik ve oyuncu seviyesine duyarlı bir formülle değiştirildi.
- Müttefik canı artık şu şekilde hesaplanıyor: `max(NPC_doğal_canı, OyuncuHP × çarpan, sabit_zemin)`
  - **Düşük Rüşvet (Acemi):** `OyuncuHP × 0.80`, minimum **200 HP**
  - **Yüksek Rüşvet (Kıdemli):** `OyuncuHP × 1.20`, minimum **350 HP**
- Bu sayede müttefikler her zaman karakterinin gelişimiyle orantılı şekilde güçleniyor — 50. seviyedeki oyuncu gerçekten güçlü korumalar alırken, 5. seviyedeki oyuncu yine de anlamlı bir koruma elde ediyor.
- Büyü ve Kondisyon değerleri de aynı mantıkla kendi çarpan ve zemin değerleriyle ölçekleniyor.
- `[BRIBE_STATS]` log etiketi artık doğal HP, oyuncu HP ve nihai HP değerlerini kaydediyor.

##### 🩺 Otomatik Can Yenilenmesi
- İşe alınan NPC'ler artık otomatik olarak iyileşiyor; savaşlar arasında manuel şifa büyüsü atmana gerek kalmıyor.
- **Savaş dışında:** `HealRate = 3.0` → yaklaşık 33 saniyede tam can.
- **Savaş içinde:** `HealRateMult = 35` → yavaş ama istikrarlı savaş içi iyileşme (saniyede ~%1), müttefiklerin uzun çarpışmalarda hayatta kalma şansını artırıyor.
- **Kondisyon:** `StaminaRate = 8.0` → kondisyon hızlı yenileniyor, müttefikler yorulmadan savaşmaya devam edebiliyor.
- *(Referans için: Vanilla NPC varsayılanı `HealRate ≈ 0.7` — tam iyileşme ~140 saniye sürer, pratikte neredeyse hiç iyileşmiyorlar.)*

---

## 📄 License / Lisans
Copyright (c) 2026 Arif KULPU. All Rights Reserved. — Tüm Hakları Saklıdır. See [LICENSE](LICENSE.md) for details.

