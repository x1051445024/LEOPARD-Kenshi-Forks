// SPDX-License-Identifier: GPL-3.0-only
// Session-long registration for the temporary station-category icon set.

#ifndef KENSHI_JOB_MANAGEMENT_STATION_ASSETS_INL
#define KENSHI_JOB_MANAGEMENT_STATION_ASSETS_INL

    bool g_stationIconLocationRegistered = false;

    const char* const STATION_ICON_RESOURCE_GROUP =
        "KenshiJobManagementStationIcons";
    const char* const STATION_HUD_ICON_FILE = "kjm-hud-icon.png";
    const char* const JOB_ENGINEERING_ICON_FILE = "job-engineering.png";
    const char* const JOB_MEDIC_ICON_FILE = "job-medic.png";
    const char* const JOB_SPLINT_ICON_FILE = "job-splint.png";
    const char* const JOB_ESCORT_OUT_ICON_FILE = "job-escort-out.png";
    const char* const JOB_FORAGING_ANIMALS_ICON_FILE =
        "job-foraging-animals.png";

    const char* const STATION_ICON_FILES[] =
    {
        "station-crafting.png",
        "station-refining.png",
        "station-farming.png",
        "station-mining.png",
        "station-research.png",
        "station-training.png",
        "station-storage.png",
        "station-defense.png",
        "station-other.png"
    };

    const char* const STATION_VISUAL_ICON_FILES[STATION_VISUAL_COUNT] =
    {
        NULL,
        "station-copper-ore.png",
        "station-iron-ore.png",
        "station-copper-plates.png",
        "station-iron-plates.png",
        "station-steel-bars.png",
        "station-copper-alloy-plates.png",
        "station-electronics.png",
        "station-crossbow.png",
        "station-skeleton-limbs.png"
    };

    bool g_stationVisualIconAvailable[STATION_VISUAL_COUNT] = { false };
    bool g_hudManagerIconAvailable = false;
    bool g_jobEngineeringIconAvailable = false;
    bool g_jobMedicIconAvailable = false;
    bool g_jobSplintIconAvailable = false;
    bool g_jobEscortOutIconAvailable = false;
    bool g_jobForagingAnimalsIconAvailable = false;

    struct NativeIconTextureCacheEntry
    {
        std::string textureName;
        bool available;

        NativeIconTextureCacheEntry() : available(false) {}
    };

    // A station type can appear on many cards. Cache both successful and
    // failed native lookups so rebuilding a large board does not repeatedly
    // enter Ogre's synchronous texture loader or exception path.
    std::vector<NativeIconTextureCacheEntry> g_nativeIconTextureCache;

    // Kenshi's native build-menu textures live in the Icons resource group.
    // Keep the load behind both C++ and SEH guards because a missing texture
    // can make Ogre throw or fault while resolving the resource name.
    __declspec(noinline) bool CallEnsureNativeIconTexture(
        const std::string* textureName)
    {
        if (textureName == NULL || textureName->empty())
        {
            return false;
        }
        try
        {
            Ogre::TextureManager* textures =
                Ogre::TextureManager::getSingletonPtr();
            if (textures == NULL)
            {
                return false;
            }
            const Ogre::TexturePtr texture = textures->load(
                *textureName, "Icons");
            return !texture.isNull();
        }
        catch (...)
        {
            return false;
        }
    }

    __declspec(noinline) bool TryLoadNativeIconTextureGuarded(
        const std::string* textureName)
    {
        bool loaded = false;
        __try
        {
            loaded = CallEnsureNativeIconTexture(textureName);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            loaded = false;
        }
        return loaded;
    }

    bool TryEnsureNativeIconTexture(
        const std::string* textureName)
    {
        if (textureName == NULL || textureName->empty())
        {
            return false;
        }
        for (size_t index = 0;
             index < g_nativeIconTextureCache.size(); ++index)
        {
            if (g_nativeIconTextureCache[index].textureName == *textureName)
            {
                return g_nativeIconTextureCache[index].available;
            }
        }

        NativeIconTextureCacheEntry entry;
        entry.textureName = *textureName;
        entry.available = TryLoadNativeIconTextureGuarded(textureName);
        g_nativeIconTextureCache.push_back(entry);
        return entry.available;
    }

    const char* GetStationCategoryIconResource(StationCategory category)
    {
        if (!g_stationIconLocationRegistered)
        {
            return NULL;
        }
        switch (category)
        {
        case STATION_CRAFTING: return "station-crafting.png";
        case STATION_REFINING: return "station-refining.png";
        case STATION_FARMING: return "station-farming.png";
        case STATION_MINING: return "station-mining.png";
        case STATION_RESEARCH: return "station-research.png";
        case STATION_TRAINING: return "station-training.png";
        case STATION_STORAGE_HAULING: return "station-storage.png";
        case STATION_DEFENSE: return "station-defense.png";
        default: return "station-other.png";
        }
    }

    const char* GetStationVisualIconResource(
        StationCategory category,
        StationVisualSubtype subtype)
    {
        if (g_stationIconLocationRegistered &&
            subtype > STATION_VISUAL_DEFAULT &&
            subtype < STATION_VISUAL_COUNT &&
            g_stationVisualIconAvailable[static_cast<int>(subtype)])
        {
            return STATION_VISUAL_ICON_FILES[static_cast<int>(subtype)];
        }
        return GetStationCategoryIconResource(category);
    }

    const char* GetGlobalJobIconResource(TaskType taskType)
    {
        if (!g_stationIconLocationRegistered)
        {
            return NULL;
        }
        switch (taskType)
        {
        case JOB_BUILDER:
            return g_jobEngineeringIconAvailable ?
                JOB_ENGINEERING_ICON_FILE :
                GetStationCategoryIconResource(STATION_OTHER);
        case JOB_REPAIR_ROBOT:
            return g_stationVisualIconAvailable[
                static_cast<int>(STATION_VISUAL_SKELETON_LIMBS)] ?
                STATION_VISUAL_ICON_FILES[
                    static_cast<int>(STATION_VISUAL_SKELETON_LIMBS)] : NULL;
        case JOB_MEDIC:
        case FIND_AND_RESCUE:
        case FIND_BED_AND_PUT_IN:
        case FIND_AND_RESCUE_IF_THERES_BEDS:
            return g_jobMedicIconAvailable ? JOB_MEDIC_ICON_FILE : NULL;
        case SPLINT_JOB:
            return g_jobSplintIconAvailable ? JOB_SPLINT_ICON_FILE : NULL;
        case PICKUP_INTRUDERS_BUILDING:
        case TAKE_INTRUDER_OUTSIDE:
        case PICKUP_INTRUDERS_TOWN:
        case TAKE_INTRUDER_OUTSIDE_TOWN:
        case SHOO_STRANGERS_OUT_OF_MY_BUILDING:
        case SHOO_STRANGERS_OUT_OF_MY_BUILDING_IF_PRIVATE:
            return g_jobEscortOutIconAvailable ?
                JOB_ESCORT_OUT_ICON_FILE : NULL;
        case LOOT_ANIMALS_JOB:
        case ANIMAL_FETCH_A_LIMB:
        case PLAY_BECAUSE_I_HAVE_A_LIMB_IN_MOUTH:
        case CHASE_ALLY_DOGS_WITH_MOUTH_LIMBS:
            return g_jobForagingAnimalsIconAvailable ?
                JOB_FORAGING_ANIMALS_ICON_FILE : NULL;
        default:
            return NULL;
        }
    }

    bool EnsureStationIconResources()
    {
        if (g_stationIconLocationRegistered)
        {
            return true;
        }

        HMODULE module = NULL;
        char modulePath[MAX_PATH] = { 0 };
        if (GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&g_stationIconLocationRegistered),
                &module) == FALSE ||
            module == NULL ||
            GetModuleFileNameA(module, modulePath, MAX_PATH) == 0)
        {
            ErrorLog("[KenshiJobManagement] Could not locate the station icon directory.");
            return false;
        }

        char* separator = std::strrchr(modulePath, '\\');
        if (separator == NULL)
        {
            ErrorLog("[KenshiJobManagement] Could not resolve the station icon path.");
            return false;
        }
        *(separator + 1) = '\0';
        std::string iconDirectory(modulePath);
        iconDirectory += "gui";

        try
        {
            Ogre::ResourceGroupManager* resources =
                Ogre::ResourceGroupManager::getSingletonPtr();
            Ogre::TextureManager* textures =
                Ogre::TextureManager::getSingletonPtr();
            if (resources == NULL || textures == NULL)
            {
                ErrorLog("[KenshiJobManagement] Ogre is not ready for station icons.");
                return false;
            }

            if (!resources->resourceGroupExists(STATION_ICON_RESOURCE_GROUP))
            {
                resources->createResourceGroup(STATION_ICON_RESOURCE_GROUP, true);
            }
            if (!resources->resourceLocationExists(
                    iconDirectory,
                    STATION_ICON_RESOURCE_GROUP))
            {
                resources->addResourceLocation(
                    iconDirectory,
                    "FileSystem",
                    STATION_ICON_RESOURCE_GROUP,
                    false,
                    true);
            }

            // These are raw PNGs, not Ogre resource scripts.  Loading the
            // nine exact files is both cheaper and safer than initialising a
            // resource group, which scans every file in the location.
            const size_t iconCount =
                sizeof(STATION_ICON_FILES) / sizeof(STATION_ICON_FILES[0]);
            for (size_t index = 0; index < iconCount; ++index)
            {
                Ogre::TexturePtr texture = textures->load(
                    STATION_ICON_FILES[index],
                    STATION_ICON_RESOURCE_GROUP);
                if (texture.isNull())
                {
                    ErrorLog("[KenshiJobManagement] A station icon could not be loaded.");
                    return false;
                }
            }

            // Variant icons are optional.  A missing new asset must not take
            // down the established category artwork.  Attempt each exact
            // texture directly: this resource group intentionally is not
            // initialised or indexed, so resourceExists() cannot reliably
            // discover files added through the raw FileSystem location.
            for (int subtype =
                    static_cast<int>(STATION_VISUAL_DEFAULT) + 1;
                 subtype < static_cast<int>(STATION_VISUAL_COUNT);
                 ++subtype)
            {
                const char* file = STATION_VISUAL_ICON_FILES[subtype];
                if (file == NULL)
                {
                    continue;
                }
                try
                {
                    Ogre::TexturePtr texture = textures->load(
                        file, STATION_ICON_RESOURCE_GROUP);
                    g_stationVisualIconAvailable[subtype] = !texture.isNull();
                }
                catch (...)
                {
                    g_stationVisualIconAvailable[subtype] = false;
                }
            }

            // Global-job role art is optional and independent from station
            // target classification. Missing files keep those cards
            // text-only without disabling the established station icons.
            try
            {
                Ogre::TexturePtr texture = textures->load(
                    JOB_ENGINEERING_ICON_FILE,
                    STATION_ICON_RESOURCE_GROUP);
                g_jobEngineeringIconAvailable = !texture.isNull();
            }
            catch (...)
            {
                g_jobEngineeringIconAvailable = false;
            }
            try
            {
                Ogre::TexturePtr texture = textures->load(
                    JOB_MEDIC_ICON_FILE,
                    STATION_ICON_RESOURCE_GROUP);
                g_jobMedicIconAvailable = !texture.isNull();
            }
            catch (...)
            {
                g_jobMedicIconAvailable = false;
            }
            try
            {
                Ogre::TexturePtr texture = textures->load(
                    JOB_SPLINT_ICON_FILE,
                    STATION_ICON_RESOURCE_GROUP);
                g_jobSplintIconAvailable = !texture.isNull();
            }
            catch (...)
            {
                g_jobSplintIconAvailable = false;
            }
            try
            {
                Ogre::TexturePtr texture = textures->load(
                    JOB_ESCORT_OUT_ICON_FILE,
                    STATION_ICON_RESOURCE_GROUP);
                g_jobEscortOutIconAvailable = !texture.isNull();
            }
            catch (...)
            {
                g_jobEscortOutIconAvailable = false;
            }
            try
            {
                Ogre::TexturePtr texture = textures->load(
                    JOB_FORAGING_ANIMALS_ICON_FILE,
                    STATION_ICON_RESOURCE_GROUP);
                g_jobForagingAnimalsIconAvailable = !texture.isNull();
            }
            catch (...)
            {
                g_jobForagingAnimalsIconAvailable = false;
            }

            // The manager entry icon is optional.  A package without this
            // new file must still keep every mandatory station category
            // resource usable; HudButton.inl keeps its JM caption fallback.
            try
            {
                Ogre::TexturePtr texture = textures->load(
                    STATION_HUD_ICON_FILE, STATION_ICON_RESOURCE_GROUP);
                g_hudManagerIconAvailable = !texture.isNull();
            }
            catch (...)
            {
                g_hudManagerIconAvailable = false;
            }

            g_stationIconLocationRegistered = true;
            DebugLog("[KenshiJobManagement] Station category icons loaded.");
            return true;
        }
        catch (...)
        {
            ErrorLog("[KenshiJobManagement] Ogre rejected the station icon directory.");
            return false;
        }
    }

    __declspec(noinline) bool CallEnsureStationIconResourcesSafely()
    {
        try
        {
            return EnsureStationIconResources();
        }
        catch (...)
        {
            return false;
        }
    }

    __declspec(noinline) bool TryEnsureStationIconResourcesGuarded()
    {
        bool result = false;
        bool faulted = false;
        __try
        {
            result = CallEnsureStationIconResourcesSafely();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            faulted = true;
        }
        if (faulted || !result)
        {
            ErrorLog("[KenshiJobManagement] Station icons failed safely; using text labels.");
            return false;
        }
        return true;
    }

    bool EnsureHudManagerIconResource()
    {
        if (!g_stationIconLocationRegistered)
        {
            return false;
        }
        if (g_hudManagerIconAvailable)
        {
            return true;
        }

        try
        {
            Ogre::TextureManager* textures =
                Ogre::TextureManager::getSingletonPtr();
            if (textures == NULL)
            {
                return false;
            }
            Ogre::TexturePtr texture = textures->load(
                STATION_HUD_ICON_FILE, STATION_ICON_RESOURCE_GROUP);
            g_hudManagerIconAvailable = !texture.isNull();
        }
        catch (...)
        {
            g_hudManagerIconAvailable = false;
        }
        return g_hudManagerIconAvailable;
    }

    __declspec(noinline) bool TryEnsureHudManagerIconResourceGuarded()
    {
        bool result = false;
        bool faulted = false;
        __try
        {
            result = EnsureHudManagerIconResource();
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            faulted = true;
        }
        if (faulted)
        {
            g_hudManagerIconAvailable = false;
            return false;
        }
        return result;
    }

#endif
