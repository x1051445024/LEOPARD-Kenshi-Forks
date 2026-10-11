// SPDX-License-Identifier: GPL-3.0-only
// Plug-in-owned UTF-8 localization. English defaults remain in code so a
// missing or damaged catalog never makes the manager unusable.

#ifndef KENSHI_JOB_MANAGEMENT_LOCALIZATION_INL
#define KENSHI_JOB_MANAGEMENT_LOCALIZATION_INL

    // LEOPARD (12.10.2026): translations are .po files read by the shared
    // Localization.h (locale/<lang>/LC_MESSAGES/kenshi_job_management.po; the
    // msgid is the English text in the code), like our other plug-ins. The
    // original keys stay in the calls as labels only; the plug-in's own .lang
    // catalogs and the locale polling are gone. Languages: en, ru, zh.
    void TickLocalization()
    {
    }

    void PrepareLocalizationForWindow()
    {
    }

    std::string Localize(const char* key, const char* englishDefault)
    {
        (void)key;
        return englishDefault != NULL ? std::string(Tr(englishDefault)) : std::string();
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
