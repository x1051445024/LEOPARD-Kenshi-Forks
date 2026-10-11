// SPDX-License-Identifier: GPL-3.0-only
// Plug-in-owned UTF-8 localization. English defaults remain in code so a
// missing or damaged catalog never makes the manager unusable.

#ifndef KENSHI_JOB_MANAGEMENT_LOCALIZATION_INL
#define KENSHI_JOB_MANAGEMENT_LOCALIZATION_INL

    typedef std::map<std::string, std::string> LocalizationCatalog;

    LocalizationCatalog g_localizationCatalog;
    std::string g_localizationLocaleId;
    bool g_localizationInitialized = false;
    DWORD g_lastLocalizationProbeTick = 0;
    char g_localizationModuleAnchor = 0;

    bool IsSupportedLocalizationLocale(const std::string& localeId)
    {
        return localeId == "de_DE" || localeId == "ja_JP" ||
            localeId == "ru_RU" || localeId == "zh_CN";
    }

    __declspec(noinline) bool TryCopyCurrentLocaleIdGuarded(
        char* buffer,
        size_t capacity)
    {
        if (buffer == NULL || capacity < 2)
        {
            return false;
        }
        buffer[0] = '\0';
        __try
        {
            LocaleManager* manager = LocaleManager::getInstance();
            LocaleInfo* locale = manager != NULL ?
                manager->getCurrentLocale() : NULL;
            if (locale == NULL || locale->id.empty() ||
                locale->id.size() >= capacity)
            {
                return false;
            }
            memcpy(buffer, locale->id.c_str(), locale->id.size() + 1);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            buffer[0] = '\0';
            return false;
        }
    }

    bool TryGetCurrentLocalizationLocale(std::string* localeIdOut)
    {
        if (localeIdOut == NULL)
        {
            return false;
        }
        char localeId[64];
        if (!TryCopyCurrentLocaleIdGuarded(localeId, sizeof(localeId)))
        {
            return false;
        }
        *localeIdOut = localeId;
        return true;
    }

    bool TryGetLocalizationDirectory(std::wstring* directoryOut)
    {
        if (directoryOut == NULL)
        {
            return false;
        }

        HMODULE module = NULL;
        const DWORD flags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;
        if (!GetModuleHandleExW(
                flags,
                reinterpret_cast<LPCWSTR>(&g_localizationModuleAnchor),
                &module) || module == NULL)
        {
            return false;
        }

        std::vector<wchar_t> path(32768, L'\0');
        const DWORD length = GetModuleFileNameW(
            module, &path[0], static_cast<DWORD>(path.size()));
        if (length == 0 || length >= path.size())
        {
            return false;
        }

        std::wstring directory(&path[0], length);
        const std::wstring::size_type slash = directory.find_last_of(L"\\/");
        if (slash == std::wstring::npos)
        {
            return false;
        }
        directory.erase(slash);
        *directoryOut = directory;
        return true;
    }

    std::wstring LocalizationCatalogPath(const std::string& localeId)
    {
        std::wstring directory;
        if (!TryGetLocalizationDirectory(&directory))
        {
            return std::wstring();
        }
        std::wstring wideLocale;
        for (size_t index = 0; index < localeId.size(); ++index)
        {
            const unsigned char value =
                static_cast<unsigned char>(localeId[index]);
            if (value > 0x7f)
            {
                return std::wstring();
            }
            wideLocale.push_back(static_cast<wchar_t>(value));
        }
        return directory + L"\\locale\\" + wideLocale + L".lang";
    }

    bool TryReadLocalizationBytes(
        const std::wstring& path,
        std::string* bytesOut)
    {
        if (path.empty() || bytesOut == NULL)
        {
            return false;
        }

        HANDLE file = CreateFileW(
            path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        LARGE_INTEGER size;
        const bool sizeAvailable = GetFileSizeEx(file, &size) != FALSE;
        if (!sizeAvailable || size.QuadPart < 0 ||
            size.QuadPart > 1024 * 1024)
        {
            CloseHandle(file);
            return false;
        }

        std::vector<char> bytes(
            static_cast<size_t>(size.QuadPart) + 1, '\0');
        DWORD bytesRead = 0;
        const bool read = size.QuadPart == 0 ||
            ReadFile(
                file, &bytes[0], static_cast<DWORD>(size.QuadPart),
                &bytesRead, NULL) != FALSE;
        CloseHandle(file);
        if (!read || bytesRead != static_cast<DWORD>(size.QuadPart))
        {
            return false;
        }

        bytesOut->assign(&bytes[0], bytesRead);
        return true;
    }

    bool IsValidUtf8LocalizationText(const std::string& text)
    {
        if (text.empty())
        {
            return true;
        }
        return MultiByteToWideChar(
            CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
            static_cast<int>(text.size()), NULL, 0) > 0;
    }

    bool IsLocalizationKeyCharacter(char value)
    {
        return (value >= 'a' && value <= 'z') ||
            (value >= 'A' && value <= 'Z') ||
            (value >= '0' && value <= '9') ||
            value == '.' || value == '_' || value == '-';
    }

    bool UnescapeLocalizationValue(
        const std::string& encoded,
        std::string* decodedOut)
    {
        if (decodedOut == NULL)
        {
            return false;
        }
        decodedOut->clear();
        for (size_t index = 0; index < encoded.size(); ++index)
        {
            const char value = encoded[index];
            if (value != '\\')
            {
                decodedOut->push_back(value);
                continue;
            }
            if (++index >= encoded.size())
            {
                return false;
            }
            const char escaped = encoded[index];
            if (escaped == 'n')
            {
                decodedOut->push_back('\n');
            }
            else if (escaped == '\\' || escaped == '=')
            {
                decodedOut->push_back(escaped);
            }
            else
            {
                return false;
            }
        }
        return true;
    }

    bool TryParseLocalizationCatalog(
        const std::string& sourceBytes,
        LocalizationCatalog* catalogOut)
    {
        if (catalogOut == NULL || !IsValidUtf8LocalizationText(sourceBytes))
        {
            return false;
        }

        LocalizationCatalog parsed;
        size_t position = 0;
        bool firstLine = true;
        while (position <= sourceBytes.size())
        {
            const size_t newline = sourceBytes.find('\n', position);
            const size_t end = newline == std::string::npos ?
                sourceBytes.size() : newline;
            std::string line = sourceBytes.substr(position, end - position);
            if (!line.empty() && line[line.size() - 1] == '\r')
            {
                line.erase(line.size() - 1);
            }
            if (firstLine && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf)
            {
                line.erase(0, 3);
            }
            firstLine = false;

            if (!line.empty() && line[0] != '#')
            {
                const size_t equals = line.find('=');
                if (equals == std::string::npos || equals == 0)
                {
                    return false;
                }
                const std::string key = line.substr(0, equals);
                for (size_t index = 0; index < key.size(); ++index)
                {
                    if (!IsLocalizationKeyCharacter(key[index]))
                    {
                        return false;
                    }
                }
                if (parsed.find(key) != parsed.end())
                {
                    return false;
                }
                std::string value;
                if (!UnescapeLocalizationValue(
                        line.substr(equals + 1), &value) || value.empty())
                {
                    return false;
                }
                parsed[key] = value;
            }

            if (newline == std::string::npos)
            {
                break;
            }
            position = newline + 1;
        }

        catalogOut->swap(parsed);
        return true;
    }

    bool LoadLocalizationCatalog(const std::string& localeId)
    {
        LocalizationCatalog nextCatalog;
        if (IsSupportedLocalizationLocale(localeId))
        {
            const std::wstring path = LocalizationCatalogPath(localeId);
            std::string bytes;
            if (!TryReadLocalizationBytes(path, &bytes) ||
                !TryParseLocalizationCatalog(bytes, &nextCatalog))
            {
                std::ostringstream warning;
                warning << "[KenshiJobManagement] Could not load locale/"
                        << localeId
                        << ".lang; using built-in English text.";
                DebugLog(warning.str().c_str());
                nextCatalog.clear();
            }
        }

        g_localizationCatalog.swap(nextCatalog);
        g_localizationLocaleId = localeId;
        g_localizationInitialized = true;
        return true;
    }

    void TickLocalization()
    {
        const DWORD now = GetTickCount();
        if (g_lastLocalizationProbeTick != 0 &&
            static_cast<DWORD>(now - g_lastLocalizationProbeTick) < 1000)
        {
            return;
        }
        g_lastLocalizationProbeTick = now;

        std::string localeId;
        if (!TryGetCurrentLocalizationLocale(&localeId))
        {
            return;
        }
        if (g_localizationInitialized &&
            localeId == g_localizationLocaleId)
        {
            return;
        }

        // Do not mix two languages in an already-open manager. Kenshi applies
        // language changes at a restart boundary in normal use; if another
        // plug-in changes it live, the next manager open loads the new locale.
        if (g_window != NULL)
        {
            return;
        }
        LoadLocalizationCatalog(localeId);
    }

    void PrepareLocalizationForWindow()
    {
        std::string localeId;
        if (TryGetCurrentLocalizationLocale(&localeId) &&
            (!g_localizationInitialized ||
             localeId != g_localizationLocaleId))
        {
            LoadLocalizationCatalog(localeId);
        }
    }

    std::string Localize(const char* key, const char* englishDefault)
    {
        if (key != NULL)
        {
            const LocalizationCatalog::const_iterator found =
                g_localizationCatalog.find(key);
            if (found != g_localizationCatalog.end())
            {
                return found->second;
            }
        }
        return englishDefault != NULL ? englishDefault : "";
    }

    bool HasRequiredLocalizationToken(
        const std::string& localized,
        const char* englishDefault,
        const char* token)
    {
        if (englishDefault == NULL || token == NULL || token[0] == '\0')
        {
            return true;
        }
        const std::string fallback(englishDefault);
        return fallback.find(token) == std::string::npos ||
            localized.find(token) != std::string::npos;
    }

    void ReplaceLocalizationToken(
        std::string* text,
        const char* token,
        const std::string& value)
    {
        if (text == NULL || token == NULL || token[0] == '\0')
        {
            return;
        }
        const std::string marker(token);
        size_t position = 0;
        while ((position = text->find(marker, position)) != std::string::npos)
        {
            text->replace(position, marker.size(), value);
            position += value.size();
        }
    }

    std::string LocalizeFormat(
        const char* key,
        const char* englishDefault,
        const char* token1,
        const std::string& value1)
    {
        std::string result = Localize(key, englishDefault);
        if (!HasRequiredLocalizationToken(result, englishDefault, token1))
        {
            result = englishDefault != NULL ? englishDefault : "";
        }
        ReplaceLocalizationToken(&result, token1, value1);
        return result;
    }

    std::string LocalizeFormat(
        const char* key,
        const char* englishDefault,
        const char* token1,
        const std::string& value1,
        const char* token2,
        const std::string& value2)
    {
        std::string result = Localize(key, englishDefault);
        if (!HasRequiredLocalizationToken(result, englishDefault, token1) ||
            !HasRequiredLocalizationToken(result, englishDefault, token2))
        {
            result = englishDefault != NULL ? englishDefault : "";
        }
        ReplaceLocalizationToken(&result, token1, value1);
        ReplaceLocalizationToken(&result, token2, value2);
        return result;
    }

    std::string LocalizeFormat(
        const char* key,
        const char* englishDefault,
        const char* token1,
        const std::string& value1,
        const char* token2,
        const std::string& value2,
        const char* token3,
        const std::string& value3)
    {
        std::string result = Localize(key, englishDefault);
        if (!HasRequiredLocalizationToken(result, englishDefault, token1) ||
            !HasRequiredLocalizationToken(result, englishDefault, token2) ||
            !HasRequiredLocalizationToken(result, englishDefault, token3))
        {
            result = englishDefault != NULL ? englishDefault : "";
        }
        ReplaceLocalizationToken(&result, token1, value1);
        ReplaceLocalizationToken(&result, token2, value2);
        ReplaceLocalizationToken(&result, token3, value3);
        return result;
    }

    std::string LocalizeFormat(
        const char* key,
        const char* englishDefault,
        const char* token1,
        const std::string& value1,
        const char* token2,
        const std::string& value2,
        const char* token3,
        const std::string& value3,
        const char* token4,
        const std::string& value4)
    {
        std::string result = Localize(key, englishDefault);
        if (!HasRequiredLocalizationToken(result, englishDefault, token1) ||
            !HasRequiredLocalizationToken(result, englishDefault, token2) ||
            !HasRequiredLocalizationToken(result, englishDefault, token3) ||
            !HasRequiredLocalizationToken(result, englishDefault, token4))
        {
            result = englishDefault != NULL ? englishDefault : "";
        }
        ReplaceLocalizationToken(&result, token1, value1);
        ReplaceLocalizationToken(&result, token2, value2);
        ReplaceLocalizationToken(&result, token3, value3);
        ReplaceLocalizationToken(&result, token4, value4);
        return result;
    }

    std::string LocalizeFormat(
        const char* key,
        const char* englishDefault,
        const char* token1,
        const std::string& value1,
        const char* token2,
        const std::string& value2,
        const char* token3,
        const std::string& value3,
        const char* token4,
        const std::string& value4,
        const char* token5,
        const std::string& value5)
    {
        std::string result = Localize(key, englishDefault);
        if (!HasRequiredLocalizationToken(result, englishDefault, token1) ||
            !HasRequiredLocalizationToken(result, englishDefault, token2) ||
            !HasRequiredLocalizationToken(result, englishDefault, token3) ||
            !HasRequiredLocalizationToken(result, englishDefault, token4) ||
            !HasRequiredLocalizationToken(result, englishDefault, token5))
        {
            result = englishDefault != NULL ? englishDefault : "";
        }
        ReplaceLocalizationToken(&result, token1, value1);
        ReplaceLocalizationToken(&result, token2, value2);
        ReplaceLocalizationToken(&result, token3, value3);
        ReplaceLocalizationToken(&result, token4, value4);
        ReplaceLocalizationToken(&result, token5, value5);
        return result;
    }

    std::string LocalizeFormat(
        const char* key,
        const char* englishDefault,
        const char* token1,
        const std::string& value1,
        const char* token2,
        const std::string& value2,
        const char* token3,
        const std::string& value3,
        const char* token4,
        const std::string& value4,
        const char* token5,
        const std::string& value5,
        const char* token6,
        const std::string& value6)
    {
        std::string result = Localize(key, englishDefault);
        if (!HasRequiredLocalizationToken(result, englishDefault, token1) ||
            !HasRequiredLocalizationToken(result, englishDefault, token2) ||
            !HasRequiredLocalizationToken(result, englishDefault, token3) ||
            !HasRequiredLocalizationToken(result, englishDefault, token4) ||
            !HasRequiredLocalizationToken(result, englishDefault, token5) ||
            !HasRequiredLocalizationToken(result, englishDefault, token6))
        {
            result = englishDefault != NULL ? englishDefault : "";
        }
        ReplaceLocalizationToken(&result, token1, value1);
        ReplaceLocalizationToken(&result, token2, value2);
        ReplaceLocalizationToken(&result, token3, value3);
        ReplaceLocalizationToken(&result, token4, value4);
        ReplaceLocalizationToken(&result, token5, value5);
        ReplaceLocalizationToken(&result, token6, value6);
        return result;
    }

    size_t LocalizationCodePointColumns(
        MyGUI::UString::code_point codePoint)
    {
        const unsigned int value = static_cast<unsigned int>(codePoint);
        // Kenshi's tooltip width estimate is based on narrow Latin glyphs.
        // CJK and full-width glyphs occupy roughly twice that space, so count
        // them as two columns when choosing safe UTF-8 wrap positions.
        if ((value >= 0x1100 && value <= 0x115f) ||
            (value >= 0x2e80 && value <= 0xa4cf) ||
            (value >= 0xac00 && value <= 0xd7a3) ||
            (value >= 0xf900 && value <= 0xfaff) ||
            (value >= 0xfe10 && value <= 0xfe6f) ||
            (value >= 0xff00 && value <= 0xff60) ||
            (value >= 0xffe0 && value <= 0xffe6) ||
            (value >= 0x1f300 && value <= 0x1faff) ||
            (value >= 0x20000 && value <= 0x3fffd))
        {
            return 2;
        }
        return 1;
    }

    size_t LocalizationTextColumns(const MyGUI::UString& text)
    {
        size_t columns = 0;
        for (size_t index = 0; index < text.size(); ++index)
        {
            columns += LocalizationCodePointColumns(text[index]);
        }
        return columns;
    }

    size_t LocalizationSplitPosition(
        const MyGUI::UString& text,
        size_t maximumColumns)
    {
        size_t columns = 0;
        size_t index = 0;
        while (index < text.size())
        {
            const size_t width = LocalizationCodePointColumns(text[index]);
            if (index != 0 && columns + width > maximumColumns)
            {
                break;
            }
            columns += width;
            ++index;
        }
        return index == 0 && !text.empty() ? 1 : index;
    }

    std::string WrapUtf8Text(
        const std::string& source,
        size_t maximumCharacters,
        size_t* lineCountOut)
    {
        if (maximumCharacters == 0)
        {
            if (lineCountOut != NULL)
            {
                *lineCountOut = source.empty() ? 0 : 1;
            }
            return source;
        }

        const MyGUI::UString unicodeSource(source);
        MyGUI::UString wrapped;
        size_t lineCount = 0;
        size_t sourcePosition = 0;
        while (sourcePosition <= unicodeSource.size())
        {
            const size_t newline = unicodeSource.find('\n', sourcePosition);
            const size_t sourceEnd = newline == MyGUI::UString::npos ?
                unicodeSource.size() : newline;
            MyGUI::UString line = unicodeSource.substr(
                sourcePosition, sourceEnd - sourcePosition);

            while (LocalizationTextColumns(line) > maximumCharacters)
            {
                const size_t hardSplit = LocalizationSplitPosition(
                    line, maximumCharacters);
                size_t split = hardSplit;
                for (size_t index = hardSplit; index > 0; --index)
                {
                    const MyGUI::UString::code_point value = line[index - 1];
                    if (value == ' ' || value == '\t')
                    {
                        split = index - 1;
                        break;
                    }
                }
                if (split == 0)
                {
                    split = hardSplit;
                }

                wrapped.append(line.substr(0, split));
                wrapped.push_back('\n');
                ++lineCount;

                size_t next = split;
                while (next < line.size() &&
                    (line[next] == ' ' || line[next] == '\t'))
                {
                    ++next;
                }
                line = line.substr(next);
            }

            wrapped.append(line);
            ++lineCount;
            if (newline == MyGUI::UString::npos)
            {
                break;
            }
            wrapped.push_back('\n');
            sourcePosition = newline + 1;
        }

        if (lineCountOut != NULL)
        {
            *lineCountOut = lineCount;
        }
        return wrapped.asUTF8();
    }

#endif
