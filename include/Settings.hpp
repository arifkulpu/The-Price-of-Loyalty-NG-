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
            
            char buf[64];
            GetPrivateProfileStringA("General", "fBaseDifficulty", "1.0", buf, sizeof(buf), iniPath);
            baseDifficulty = std::stof(buf);

            enableBackstab = GetPrivateProfileIntA("General", "bEnableBackstab", 1, iniPath) != 0;

            // Write defaults if not present
            if (GetFileAttributesA(iniPath) == INVALID_FILE_ATTRIBUTES) {
                WritePrivateProfileStringA("General", "iBribeHotkey", "48", iniPath);
                WritePrivateProfileStringA("General", "fBaseDifficulty", "1.0", iniPath);
                WritePrivateProfileStringA("General", "bEnableBackstab", "1", iniPath);
            }
        }

        int bribeHotkey = 48;
        float baseDifficulty = 1.0f;
        bool enableBackstab = true;

    private:
        Settings() = default;
    };
}
