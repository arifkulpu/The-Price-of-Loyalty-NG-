#pragma once
#include <Windows.h>
#include <string>

namespace Loyalty {
    class Settings {
    public:
        static Settings* GetSingleton() {
            static Settings singleton;
            return &singleton;
        }

        void Load() {
            const char* iniPath = ".\\Data\\SKSE\\Plugins\\ThePriceOfLoyalty.ini";
            
            bribeHotkey = GetPrivateProfileIntA("General", "iBribeHotkey", 48, iniPath);
            baseBribeCost = GetPrivateProfileIntA("General", "iBaseBribeCost", 500, iniPath);
            costPerLevel = GetPrivateProfileIntA("General", "iCostPerLevel", 50, iniPath);
            betrayalMinTime = GetPrivateProfileIntA("General", "iBetrayalMinTime", 60, iniPath);
            betrayalMaxTime = GetPrivateProfileIntA("General", "iBetrayalMaxTime", 300, iniPath);
            betrayalChanceLowBribe = GetPrivateProfileIntA("General", "iBetrayalChanceLowBribe", 60, iniPath);
            betrayalChanceHighBribe = GetPrivateProfileIntA("General", "iBetrayalChanceHighBribe", 15, iniPath);

            char buf[64];
            GetPrivateProfileStringA("General", "fBaseDifficulty", "1.0", buf, sizeof(buf), iniPath);
            baseDifficulty = std::stof(buf);

            enableBackstab = GetPrivateProfileIntA("General", "bEnableBackstab", 1, iniPath) != 0;

            // Write defaults if not present
            if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) {
                WritePrivateProfileStringA("General", "iBribeHotkey", "48", iniPath);
                WritePrivateProfileStringA("General", "iBaseBribeCost", "500", iniPath);
                WritePrivateProfileStringA("General", "iCostPerLevel", "50", iniPath);
                WritePrivateProfileStringA("General", "iBetrayalMinTime", "60", iniPath);
                WritePrivateProfileStringA("General", "iBetrayalMaxTime", "300", iniPath);
                WritePrivateProfileStringA("General", "iBetrayalChanceLowBribe", "60", iniPath);
                WritePrivateProfileStringA("General", "iBetrayalChanceHighBribe", "15", iniPath);
                WritePrivateProfileStringA("General", "fBaseDifficulty", "1.0", iniPath);
                WritePrivateProfileStringA("General", "bEnableBackstab", "1", iniPath);
            }
        }

        int bribeHotkey = 48;
        int baseBribeCost = 500;
        int costPerLevel = 50;
        int betrayalMinTime = 60;
        int betrayalMaxTime = 300;
        int betrayalChanceLowBribe = 60;
        int betrayalChanceHighBribe = 15;
        float baseDifficulty = 1.0f;
        bool enableBackstab = true;

    private:
        Settings() = default;
    };
}
