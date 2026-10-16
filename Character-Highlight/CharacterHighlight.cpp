// Character Highlight - while the highlight key is held, characters are tinted
// by how they stand to you: your squad, enemies, allies, neutrals.
//
// Original: BFrizzleFoShizzle, KenshiLib_Examples (GPLv3).
//
// LEOPARD fork (16.10.2026): settings in Mod Config Menu, and the per-frame work
// cut down. The original, every frame:
//   * walked every character in the update list (hundreds in a town) and for
//     each one asked isEnemy / areYouGonnaGetMe / isAlly against EVERY player
//     character, plus the faction relation - hundreds x dozens of calls;
//   * wrote two shader constants on every one of them;
//   * with the key NOT held, wrote the "off" constants on every character too -
//     a constant cost even when nothing is highlighted.
// Here:
//   * the relation colour is cached per character and re-evaluated once a
//     second, at most kEvalPerFrame characters a frame;
//   * the constants are written when the colour changes, and again every
//     kReapplyMs in case something else wrote over them;
//   * with the key not held, the characters we tinted are reset once, then
//     nothing is done until the key is held again.
// The portrait fix and the dead-body fix of the original are kept as they were.

#define KLOC_DOMAIN "character_highlight"
#include <Localization.h>
#include <ModConfigMenu.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <IniDefaults.h>
#include <HoldKey.h>

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>

#include <kenshi/Appearance.h>
#include <kenshi/InputHandler.h>
#include <kenshi/Globals.h>
#include <kenshi/Character.h>
#include <kenshi/GameWorld.h>
#include <kenshi/PlayerInterface.h>
#include <kenshi/Faction.h>
#include <kenshi/FactionRelations.h>
#include <kenshi/KingOfRenderThread.h>
#include <kenshi/Renderer.h>
#include <kenshi/RenderToTextureotron.h>
#include <kenshi/util/hand.h>
#include <core/Functions.h>
#include <ogre/OgreMaterial.h>
#include <ogre/OgreTechnique.h>
#include <ogre/OgreEntity.h>
#include <ogre/OgreMaterialManager.h>
#include <ogre/OgreSharedPtr.h>
#include <Debug.h>

extern "C" IMAGE_DOS_HEADER __ImageBase;

namespace
{
    // ---------------------------------------------------------------
    // Settings
    // ---------------------------------------------------------------

    bool g_enabled = true;
    HoldKey::Key g_holdKey;             // ALT (as the game's item highlight); NONE - always on
    bool g_throughWalls = true;
    float g_maxDistance = 0.0f;         // metres from the camera centre, 0 - no limit
    Ogre::ColourValue g_squadColour(0.0f, 0.0f, 1.0f, 1.0f);
    Ogre::ColourValue g_enemyColour(1.0f, 0.0f, 0.0f, 1.0f);
    Ogre::ColourValue g_allyColour(0.0f, 1.0f, 0.0f, 1.0f);
    bool g_neutralByRelation = true;    // the original's red-yellow-green by faction relation
    Ogre::ColourValue g_neutralColour(1.0f, 1.0f, 0.0f, 1.0f);
    bool g_debug = false;

    const int kEvalPerFrame = 40;
    const DWORD kEvalMs = 1000;
    const DWORD kReapplyMs = 500;
    const DWORD kForgetMs = 5000;

    std::string ModulePath(const char* ext)
    {
        char path[MAX_PATH] = {};
        const DWORD length = GetModuleFileNameA(reinterpret_cast<HMODULE>(&__ImageBase), path, MAX_PATH);
        if (length == 0 || length >= MAX_PATH)
            return std::string();
        std::string file(path, length);
        const std::string::size_type dot = file.rfind('.');
        return dot == std::string::npos ? std::string() : file.substr(0, dot) + ext;
    }

    // The player's ini - see shared/IniDefaults.h.
    std::string IniPath() { return IniDefaults::Prepare(ModulePath(".ini")); }

    const char* const kSec = "CharacterHighlight";

    Ogre::ColourValue ReadColour(const std::string& ini, const char* key, const Ogre::ColourValue& def)
    {
        char text[32] = {};
        GetPrivateProfileStringA(kSec, key, "", text, sizeof(text), ini.c_str());
        const char* p = text;
        while (*p == ' ' || *p == '#')
            ++p;
        unsigned int rgb = 0;
        if (strlen(p) < 6 || sscanf_s(p, "%6x", &rgb) != 1)
            return def;
        return Ogre::ColourValue(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f,
                                 (rgb & 0xFF) / 255.0f, 1.0f);
    }

    void ForgetColours();

    // At startup and from MCM after a change.
    void __cdecl LoadSettings()
    {
        const std::string ini = IniPath();
        if (ini.empty())
            return;
        g_enabled = GetPrivateProfileIntA(kSec, "Enabled", 1, ini.c_str()) != 0;
        char key[64] = {};
        GetPrivateProfileStringA(kSec, "HoldKey", "ALT", key, sizeof(key), ini.c_str());
        g_holdKey = HoldKey::Parse(key);
        g_throughWalls = GetPrivateProfileIntA(kSec, "ShowThroughWalls", 1, ini.c_str()) != 0;
        int distance = GetPrivateProfileIntA(kSec, "MaxDistance", 0, ini.c_str());
        g_maxDistance = static_cast<float>(distance < 0 ? 0 : (distance > 5000 ? 5000 : distance));
        g_squadColour = ReadColour(ini, "SquadColour", Ogre::ColourValue(0.0f, 0.0f, 1.0f, 1.0f));
        g_enemyColour = ReadColour(ini, "EnemyColour", Ogre::ColourValue(1.0f, 0.0f, 0.0f, 1.0f));
        g_allyColour = ReadColour(ini, "AllyColour", Ogre::ColourValue(0.0f, 1.0f, 0.0f, 1.0f));
        g_neutralByRelation = GetPrivateProfileIntA(kSec, "NeutralByRelation", 1, ini.c_str()) != 0;
        g_neutralColour = ReadColour(ini, "NeutralColour", Ogre::ColourValue(1.0f, 1.0f, 0.0f, 1.0f));
        g_debug = GetPrivateProfileIntA(kSec, "Debug", 0, ini.c_str()) != 0;
        ForgetColours();                // new colours - re-evaluate everyone
    }

    // ---------------------------------------------------------------
    // Shader constants (as the original writes them)
    // ---------------------------------------------------------------

    void WriteTint(Character* character, const Ogre::ColourValue& colour, bool depth)
    {
        Ogre::MaterialPtr& material = character->getAppearance()->bodyMaterial;
        if (material.isNull())
            return;
        Ogre::Pass* const pass = material->getTechnique(0)->getPass(0);
        pass->getFragmentProgramParameters()->setNamedConstant("coloroverride", colour);
        pass->getVertexProgramParameters()->setNamedConstant("overrideDepth", depth);
    }

    void ClearTint(Character* character)
    {
        WriteTint(character, Ogre::ColourValue(1.0, 1.0, 1.0, 0.0), false);
    }

    bool SafeWrite(Character* character, const Ogre::ColourValue& colour, bool depth)
    {
        __try
        {
            WriteTint(character, colour, depth);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // ---------------------------------------------------------------
    // Relation colour - the original's EnableHighlight, colours from the settings
    // ---------------------------------------------------------------

    Ogre::ColourValue RelationColour(Character* character)
    {
        if (character->isPlayerCharacter())
            return g_squadColour;

        const lektor<Character*>& players = ou->player->playerCharacters;

        // isEnemy if enemy with at least one character
        bool isEnemy = false;
        for (uint32_t i = 0; i < players.size() && !isEnemy; ++i)
            isEnemy = character->isEnemy(players[i], true);
        for (uint32_t i = 0; i < players.size() && !isEnemy; ++i)
            isEnemy = players[i]->areYouGonnaGetMe(character);
        if (isEnemy)
            return g_enemyColour;

        // isAlly if allied with all characters
        bool isAlly = true;
        for (uint32_t i = 0; i < players.size() && isAlly; ++i)
            isAlly = character->isAlly(players[i], true);
        if (isAlly)
            return g_allyColour;

        if (!g_neutralByRelation)
            return g_neutralColour;

        // if neither, colour based on relations, scaled -100..100 to -1..1
        Faction* const faction = character->getFaction();
        if (faction == NULL || faction->relations == NULL)
            return g_neutralColour;
        const float factionRelations = faction->relations->getFactionRelation(ou->player->getFaction()) / 100.0f;
        return Ogre::ColourValue(std::max(0.0f, -factionRelations + 1.0f), factionRelations + 1, 0.0, 1.0);
    }

    bool SafeRelationColour(Character* character, Ogre::ColourValue& out)
    {
        __try
        {
            out = RelationColour(character);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    // ---------------------------------------------------------------
    // Per-character state
    // ---------------------------------------------------------------

    struct Entry
    {
        hand who;
        Ogre::ColourValue colour;
        bool hasColour;
        bool tinted;                    // our constants are on its material now
        DWORD evaluated;
        DWORD applied;
        DWORD seen;
    };
    typedef std::map<unsigned long long, Entry> Entries;
    Entries g_entries;
    bool g_anyTinted = false;
    bool g_forget = false;
    unsigned g_evals = 0, g_writes = 0;

    unsigned long long Key(const hand& h)
    {
        return (static_cast<unsigned long long>(h.index) << 32) | h.serial;
    }

    void ForgetColours() { g_forget = true; }

    void ResetAll()
    {
        for (Entries::iterator it = g_entries.begin(); it != g_entries.end(); ++it)
        {
            if (!it->second.tinted)
                continue;
            Character* const c = it->second.who.getCharacter();
            if (c != NULL)
                SafeWrite(c, Ogre::ColourValue(1.0, 1.0, 1.0, 0.0), false);
            it->second.tinted = false;
        }
        g_anyTinted = false;
    }

    // Own key, changeable in MCM like our other plugins (the original used the
    // game's item highlight control, Alt by default - ALT is the default here).
    bool KeyHeld()
    {
        return HoldKey::Held(g_holdKey);
    }

    bool SafeOutOfRange(Character* c, const Ogre::Vector3& camera)
    {
        __try
        {
            return c->getPosition().squaredDistance(camera) > g_maxDistance * g_maxDistance;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return true;
        }
    }

    bool HasBodyMaterial(Character* c)
    {
        __try
        {
            return c->getAppearance() != NULL && !c->getAppearance()->bodyMaterial.isNull();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    void Tick(GameWorld* world)
    {
        if (g_forget)
        {
            g_forget = false;
            ResetAll();
            g_entries.clear();
        }
        const bool active = g_enabled && KeyHeld() && ou != NULL && ou->player != NULL;
        if (!active)
        {
            if (g_anyTinted)
                ResetAll();
            return;
        }

        const DWORD now = GetTickCount();
        const bool limited = g_maxDistance > 0.0f;
        const Ogre::Vector3 camera = limited ? ou->getCameraCenter() : Ogre::Vector3::ZERO;
        int budget = kEvalPerFrame;
        const ogre_unordered_set<Character*>::type& characters = world->getCharacterUpdateList();
        for (ogre_unordered_set<Character*>::type::const_iterator iter = characters.begin(); iter != characters.end(); ++iter)
        {
            Character* const c = *iter;
            if (c == NULL || !HasBodyMaterial(c))
                continue;
            const hand& h = c->handle;
            Entry& e = g_entries[Key(h)];
            if (e.seen == 0)
            {
                e.who = h;
                e.hasColour = false;
                e.tinted = false;
                e.evaluated = 0;
                e.applied = 0;
            }
            e.seen = now;

            if (limited && SafeOutOfRange(c, camera))
            {
                if (e.tinted && SafeWrite(c, Ogre::ColourValue(1.0, 1.0, 1.0, 0.0), false))
                    e.tinted = false;
                continue;
            }

            bool changed = false;
            if ((!e.hasColour || now - e.evaluated >= kEvalMs) && budget > 0)
            {
                --budget;
                ++g_evals;
                Ogre::ColourValue colour;
                e.evaluated = now;
                if (SafeRelationColour(c, colour))
                {
                    changed = !e.hasColour || colour != e.colour;
                    e.colour = colour;
                    e.hasColour = true;
                }
            }
            if (!e.hasColour)
                continue;               // waits for its turn in the budget
            if (changed || !e.tinted || now - e.applied >= kReapplyMs)
            {
                if (SafeWrite(c, e.colour, g_throughWalls))
                {
                    ++g_writes;
                    e.tinted = true;
                    e.applied = now;
                    g_anyTinted = true;
                }
            }
        }

        // characters gone from the update list - forget (their material was reset on removal)
        for (Entries::iterator it = g_entries.begin(); it != g_entries.end();)
        {
            if (now - it->second.seen >= kForgetMs)
                g_entries.erase(it++);
            else
                ++it;
        }

        static DWORD lastDebug = 0;
        if (g_debug && now - lastDebug >= 10000)
        {
            lastDebug = now;
            char note[160];
            sprintf_s(note, "Character Highlight: %u characters known, %u evaluations, %u writes in 10 s",
                      static_cast<unsigned>(g_entries.size()), g_evals, g_writes);
            DebugLog(note);
            g_evals = g_writes = 0;
        }
    }
}

void (*GameWorld__mainLoop_GPUSensitiveStuff_orig)(GameWorld* thisptr, float time);
void GameWorld__mainLoop_GPUSensitiveStuff_hook(GameWorld* thisptr, float time)
{
    // Before the original, as the original plugin does: Vital Sense tints
    // after the main loop and so stays on top of these colours.
    Tick(thisptr);
    GameWorld__mainLoop_GPUSensitiveStuff_orig(thisptr, time);
}

// fix for dead bodies not updating
void (*GameWorld_removeFromUpdateListMain_orig)(GameWorld* thisptr, Character* character);
void GameWorld_removeFromUpdateListMain_hook(GameWorld* thisptr, Character* character)
{
    if (!character->getAppearance()->bodyMaterial.isNull())
    {
        ClearTint(character);
        g_entries.erase(Key(character->handle));
    }
    GameWorld_removeFromUpdateListMain_orig(thisptr, character);
}

// fix to stop highlight applying to character portraits
void (*RenderToTextureotron_renderCharacter_orig)(RenderToTextureotron* thisptr, Character* character, Ogre::SharedPtr<Ogre::Texture> texture, const iVector2& size, const Ogre::TRect<float>& rect);
void RenderToTextureotron_renderCharacter_hook(RenderToTextureotron* thisptr, Character* character, Ogre::SharedPtr<Ogre::Texture> texture, const iVector2& size, const Ogre::TRect<float>& rect)
{
    Entries::const_iterator it = g_entries.find(Key(character->handle));
    const bool tinted = it != g_entries.end() && it->second.tinted;
    const Ogre::ColourValue colour = tinted ? it->second.colour : Ogre::ColourValue::White;
    if (tinted)
    {
        // disable highlight
        if (!character->getAppearance()->bodyMaterial.isNull())
            ClearTint(character);
    }
    RenderToTextureotron_renderCharacter_orig(thisptr, character, texture, size, rect);
    if (tinted)
    {
        // re-enable highlight
        if (!character->getAppearance()->bodyMaterial.isNull())
            WriteTint(character, colour, g_throughWalls);
    }
}


// The page in Mod Config Menu (Options window, MCM tab), if it is installed.
extern "C" __declspec(dllexport) void MCM_Describe(MCM_Api* api)
{
    static const std::string ini = IniPath();
    api->beginMod(api, "CharacterHighlight", "Character Highlight", ini.c_str(), &LoadSettings);
    if (api->version >= 2)
        api->info(api, Tr("While the highlight key is held, characters are tinted by how they stand to you."));
    api->section(api, Tr("General"));
    api->toggle(api, kSec, "Enabled", Tr("Enabled"), Tr("Tint characters while the highlight key is held."), 1, 0);
    api->hotkey(api, kSec, "HoldKey", Tr("Show key"), Tr("The tint shows while this key is held. NONE (Backspace when assigning) - always on."), "ALT", 0);
    api->toggle(api, kSec, "ShowThroughWalls", Tr("Show through walls"), Tr("Tinted characters are visible through walls and buildings."), 1, 0);
    api->integer(api, kSec, "MaxDistance", Tr("Distance, m"), Tr("Characters farther than this from the centre of the camera are not tinted. 0 - no limit."), 0, 0, 5000, 0);
    api->section(api, Tr("Colours"));
    api->colour(api, kSec, "SquadColour", Tr("Your squad"), Tr("Colour of your characters, #RRGGBB."), "#0000FF", 0);
    api->colour(api, kSec, "EnemyColour", Tr("Enemies"), Tr("Colour of those hostile to you, #RRGGBB."), "#FF0000", 0);
    api->colour(api, kSec, "AllyColour", Tr("Allies"), Tr("Colour of those allied with all your characters, #RRGGBB."), "#00FF00", 0);
    api->toggle(api, kSec, "NeutralByRelation", Tr("Neutrals by relation"), Tr("Neutrals go from red to yellow to green with their faction's relation to you. Off - the neutral colour below."), 1, 0);
    api->colour(api, kSec, "NeutralColour", Tr("Neutrals"), Tr("Colour of neutrals when they are not coloured by relation, #RRGGBB."), "#FFFF00", 0);
    api->section(api, Tr("Diagnostics"));
    api->toggle(api, kSec, "Debug", Tr("Detailed log"), Tr("Every 10 seconds in RE_Kenshi_log.txt: how many characters are known and how much work was done."), 0, 0);
}

__declspec(dllexport) void startPlugin()
{
    LoadSettings();
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::_NV_mainLoop_GPUSensitiveStuff), &GameWorld__mainLoop_GPUSensitiveStuff_hook, &GameWorld__mainLoop_GPUSensitiveStuff_orig))
        ErrorLog("Character Highlight: could not install hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&GameWorld::removeFromUpdateListMain), &GameWorld_removeFromUpdateListMain_hook, &GameWorld_removeFromUpdateListMain_orig))
        ErrorLog("Character Highlight: could not install hook!");
    if (KenshiLib::SUCCESS != KenshiLib::AddHook(KenshiLib::GetRealAddress(&RenderToTextureotron::renderCharacter), &RenderToTextureotron_renderCharacter_hook, &RenderToTextureotron_renderCharacter_orig))
        ErrorLog("Character Highlight: could not install hook!");
}
