// SPDX-License-Identifier: GPL-3.0-only
// LEOPARD fork: plug-in settings edited in Mod Config Menu (MCM tab of the
// game's Options window). They live in KenshiJobManagement.ini next to the
// DLL; the mod ships KenshiJobManagement.defaults.ini, and the player's ini is
// created from it and only gains new keys on updates (shared/IniDefaults.h).
// The manager's own window state (station filters, collapsed groups) stays in
// %LOCALAPPDATA%\KenshiJobManagement\settings.ini as in the original.

#ifndef KENSHI_JOB_MANAGEMENT_MCM_SETTINGS_INL
#define KENSHI_JOB_MANAGEMENT_MCM_SETTINGS_INL

    bool g_settingHudButton = true;      // the "JM" button on the game's panel
    HoldKey::Key g_hotkey;               // opens / closes the manager
    std::string g_hotkeyText = "CTRL+J";

    void RestoreHudManagerButton();      // HudButton.inl

    std::string LeopardIniPath()
    {
        char path[MAX_PATH] = { 0 };
        const DWORD length = GetModuleFileNameA(
            reinterpret_cast<HMODULE>(&__ImageBase), path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
        {
            return std::string();
        }
        std::string file(path, length);
        const std::string::size_type dot = file.rfind('.');
        if (dot == std::string::npos)
        {
            return std::string();
        }
        return IniDefaults::Prepare(file.substr(0, dot) + ".ini");
    }

    bool LeopardEnabledSetting()
    {
        const std::string ini = LeopardIniPath();
        return ini.empty() ||
            GetPrivateProfileIntA("General", "Enabled", 1, ini.c_str()) != 0;
    }

    void LoadLeopardSettings()
    {
        const std::string ini = LeopardIniPath();
        if (ini.empty())
        {
            return;
        }
        char key[64] = { 0 };
        GetPrivateProfileStringA("General", "Hotkey", "CTRL+J", key, sizeof(key), ini.c_str());
        g_hotkey = HoldKey::Parse(key);
        g_hotkeyText = g_hotkey.vk != 0 ? std::string(key) : std::string();

        const bool hud = GetPrivateProfileIntA("General", "HudButton", 1, ini.c_str()) != 0;
        if (g_settingHudButton && !hud && g_enabled)
        {
            RestoreHudManagerButton();   // turned off in MCM - take it away now
        }
        g_settingHudButton = hud;
    }

    // Tooltip of the panel button: the current key, not a hard-coded Ctrl+J.
    std::string HudButtonTooltip()
    {
        std::string text = Localize("ui.hud.open_manager_tooltip", "Open Job Manager");
        if (!g_hotkeyText.empty())
        {
            text += " (" + g_hotkeyText + ")";
        }
        return text;
    }

#endif
