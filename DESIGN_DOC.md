# The Price of Loyalty - Technical Design Document

## 1. Overview
**The Price of Loyalty** is a comprehensive Social and Economic Overhaul for Skyrim. It leverages SKSE and C++ to introduce a deep, dynamic bribery and interaction system that goes beyond simple dialogue options, transforming how players interact with the world's inhabitants—both friend and foe.

---

## 2. Core Systems

### 2.1 Dynamic Bribery & Interaction (Hotkey System)
- **Feature**: A dedicated hotkey triggers the interaction, independent of the standard 'E' (Activate) key.
- **Custom UI**: A modern, sleek overlay built using SKSE/C++ (potentially integrating with Scaleform or a custom rendering layer like ImGui).
- **Time Dilation**: When the hotkey is pressed while looking at an NPC, time slows down (e.g., 0.1x speed) to allow for tactical decision-making.
- **NPC Insights**: The UI displays:
    - NPC Class and Level.
    - Social Status (Citizen, Guard, Bandit Leader, etc.).
    - Estimated Success Probability.

### 2.2 Combat Interception & "True Surrender"
- **Engine Hook**: Intercept the `Bleedout` state logic.
- **Resolution**: Prevents the vanilla "recover and attack" loop. NPCs in bleedout become receptive to the interaction hotkey.
- **Permanent Alignment**: Successful bribery in bleedout sets `RelationshipRank` to 3 (Ally) and clears combat state permanently.

### 2.3 Social & Economic Hierarchy
- **Scaling Difficulty**:
    - **Bandits**: Low gold requirement, low speech check.
    - **Guards**: High gold requirement, high risk of "Report Crime" if failed.
    - **Fanatics (Thalmor/Silver Hand)**: Near-zero success rate, high cost.
- **World Interaction**: NPCs can bribe each other (simulated or active hooks), creating a living, corrupt ecosystem.

### 2.4 Personality & Risk Factors (Hidden Traits)
NPCs are assigned hidden traits upon first encounter (or via persistent storage):
- **Greedy**: Lower gold requirement, 100% loyalty after payment.
- **Honorable**: Requires high Speech skill and significant gold; once bought, stays bought.
- **Treacherous**: Might take the gold and flee, or worse, backstab the player later.

---

## 3. Technical Implementation (SKSE / C++)

### 3.1 Performance & Stability
- **Language**: C++ (CommonLibSSE-NG).
- **Execution**: Logic runs on the main engine thread or via task delegates, avoiding the overhead and "script lag" of Papyrus.
- **Persistence**: Data (Relationship changes, hidden traits) is serialized into the savegame using `SKSE::Serialization`.

### 3.2 Hooks & Logic
- **Input Hook**: Monitor for the custom hotkey.
- **Combat Hook**: Monitor NPC health and `IsBleedingOut()` status.
- **UI Hook**: Render the interaction panel.

### 3.3 Visuals & Audio
- **Animations**: Trigger standard "Give Item" or custom animation events via `PlayAnimation`.
- **Audio**: Utilize `VoiceType` to play appropriate lines for acceptance or rejection.

---

## 4. Development Phases

### Phase 1: Foundation
- Project setup (CommonLibSSE-NG).
- Hotkey detection and NPC targeting logic.
- Basic "Time Slow" effect implementation.

### Phase 2: Combat & Surrender
- Implementing the bleedout intercept.
- Combat state management (forcing NPCs to yield).

### Phase 3: Social Engine
- Probability calculation logic based on Class/Status/Traits.
- Hidden trait assignment system.

### Phase 4: UI & Polish
- Developing the custom UI overlay.
- Sound and animation integration.
- Save/Load serialization.
