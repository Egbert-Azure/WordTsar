//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2026 Egbert H. Schroeer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "src/input/keystroke.h"

#include <cctype>
#include <cstddef>


namespace
{

struct sSpecialName
{
    eSpecialKey key;
    const char* name;
};

const sSpecialName SPECIAL_NAMES[] =
{
    { SPECIAL_ENTER,     "Enter" },
    { SPECIAL_TAB,       "Tab" },
    { SPECIAL_ESCAPE,    "Esc" },
    { SPECIAL_BACKSPACE, "Bksp" },
    { SPECIAL_DELETE,    "Del" },
    { SPECIAL_HOME,      "Home" },
    { SPECIAL_END,       "End" },
    { SPECIAL_PAGE_UP,   "PgUp" },
    { SPECIAL_PAGE_DOWN, "PgDn" },
    { SPECIAL_LEFT,      "Left" },
    { SPECIAL_RIGHT,     "Right" },
    { SPECIAL_UP,        "Up" },
    { SPECIAL_DOWN,      "Down" },
    { SPECIAL_F1,        "F1" },
    { SPECIAL_F2,        "F2" },
    { SPECIAL_F3,        "F3" },
    { SPECIAL_F4,        "F4" },
    { SPECIAL_F5,        "F5" },
    { SPECIAL_F6,        "F6" },
    { SPECIAL_F7,        "F7" },
    { SPECIAL_F8,        "F8" },
    { SPECIAL_F9,        "F9" },
    { SPECIAL_F10,       "F10" },
    { SPECIAL_F11,       "F11" },
    { SPECIAL_F12,       "F12" }
};

const char PAUSE_NAME[] = "PAUSE";
const char SPACE_NAME[] = "Space";


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Decode the UTF-8 sequence at text[pos]; advances pos. Invalid bytes
/// decode as themselves.
///
/////////////////////////////////////////////////////////////////////////////
char32_t DecodeUtf8(const std::string& text, size_t& pos)
{
    unsigned char lead = static_cast<unsigned char>(text[pos]);
    size_t length = 1;
    char32_t cp = lead;

    if ((lead & 0xE0U) == 0xC0U)
    {
        length = 2;
        cp = lead & 0x1FU;
    }
    else if ((lead & 0xF0U) == 0xE0U)
    {
        length = 3;
        cp = lead & 0x0FU;
    }
    else if ((lead & 0xF8U) == 0xF0U)
    {
        length = 4;
        cp = lead & 0x07U;
    }

    if ((length == 1) || ((pos + length) > text.size()))
    {
        pos += 1;
        return lead;
    }

    for (size_t i = 1; i < length; i++)
    {
        cp = (cp << 6) | (static_cast<unsigned char>(text[pos + i]) & 0x3FU);
    }

    pos += length;
    return cp;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// True for code points that attach to the character before them
/// (combining marks, variation selectors, joiners, skin tone modifiers).
///
/////////////////////////////////////////////////////////////////////////////
bool IsExtender(char32_t cp)
{
    return ((cp >= 0x0300) && (cp <= 0x036F)) ||
           ((cp >= 0x1AB0) && (cp <= 0x1AFF)) ||
           ((cp >= 0x1DC0) && (cp <= 0x1DFF)) ||
           ((cp >= 0x20D0) && (cp <= 0x20FF)) ||
           ((cp >= 0xFE00) && (cp <= 0xFE0F)) ||
           ((cp >= 0xFE20) && (cp <= 0xFE2F)) ||
           (cp == 0x200C) || (cp == 0x200D) ||
           ((cp >= 0x1F3FB) && (cp <= 0x1F3FF)) ||
           ((cp >= 0xE0020) && (cp <= 0xE007F)) ||
           ((cp >= 0xE0100) && (cp <= 0xE01EF));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Split UTF-8 text into characters, each a base code point with the
/// code points that attach to it.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> SplitCharacters(const std::string& text)
{
    std::vector<std::string> pieces;
    size_t pos = 0;
    bool afterJoiner = false;

    while (pos < text.size())
    {
        size_t start = pos;
        char32_t cp = DecodeUtf8(text, pos);
        std::string bytes = text.substr(start, pos - start);

        if ((pieces.empty() == false) && ((IsExtender(cp) == true) || (afterJoiner == true)))
        {
            pieces.back() += bytes;
        }
        else
        {
            pieces.push_back(bytes);
        }

        afterJoiner = (cp == 0x200D);
    }

    return pieces;
}


bool IsControlText(const std::string& text)
{
    unsigned char first = static_cast<unsigned char>(text[0]);
    return (first < 0x20U) || (first == 0x7FU);
}


bool IsChordPrefix(const sKeyStroke& key)
{
    if ((key.kind != KEYSTROKE_CHAR) || (key.alt == true) || (key.shift == true))
    {
        return false;
    }

    char letter = static_cast<char>(key.ch + '@');
    return (letter == 'K') || (letter == 'Q') || (letter == 'O') || (letter == 'P') || (letter == 'M');
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// The part of a chord line that names the key after the prefix: "^B" for a
/// control key, an upper-case letter or other visible ASCII character for
/// typed text. Empty when the key cannot share the prefix's line.
///
/////////////////////////////////////////////////////////////////////////////
std::string ChordFollowName(const sKeyStroke& key)
{
    if ((key.kind == KEYSTROKE_CHAR) && (key.alt == false) && (key.shift == false) &&
        (key.ch >= 1) && (key.ch <= 26))
    {
        return std::string("^") + static_cast<char>(key.ch + '@');
    }

    if ((key.kind == KEYSTROKE_TEXT) && (key.ctrl == false) && (key.alt == false) &&
        (key.utf8.size() == 1))
    {
        unsigned char c = static_cast<unsigned char>(key.utf8[0]);
        if ((c > 0x20U) && (c < 0x7FU))
        {
            return std::string(1, static_cast<char>(toupper(c)));
        }
    }

    return std::string();
}


bool ParseControlName(const std::string& name, char& ch)
{
    if ((name.size() == 2) && (name[0] == '^') && (isalpha(static_cast<unsigned char>(name[1])) != 0))
    {
        ch = static_cast<char>(toupper(static_cast<unsigned char>(name[1])) - '@');
        return true;
    }
    return false;
}

} // namespace


sKeyStroke CharKeyStroke(char ch, bool shift, bool alt)
{
    sKeyStroke key;
    key.kind = KEYSTROKE_CHAR;
    key.ch = ch;
    key.shift = shift;
    key.ctrl = true;
    key.alt = alt;
    return key;
}


sKeyStroke SpecialKeyStroke(eSpecialKey special, bool shift, bool ctrl, bool alt)
{
    sKeyStroke key;
    key.kind = KEYSTROKE_SPECIAL;
    key.special = special;
    key.shift = shift;
    key.ctrl = ctrl;
    key.alt = alt;
    return key;
}


sKeyStroke TextKeyStroke(const std::string& utf8, bool shift, bool ctrl, bool alt)
{
    sKeyStroke key;
    key.kind = KEYSTROKE_TEXT;
    key.utf8 = utf8;
    key.shift = shift;
    key.ctrl = ctrl;
    key.alt = alt;
    return key;
}


sKeyStroke PauseKeyStroke(void)
{
    sKeyStroke key;
    key.kind = KEYSTROKE_PAUSE;
    return key;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  key [in] key stroke
///
/// @return the key's macro name, or an empty string if it has none
///
/// @brief
/// Modifier prefixes come in the order Shift+, Ctrl+, Alt+. A control key
/// carries no Ctrl+ (the ^ says it). Shift+ on typed text is written only
/// together with Ctrl+ or Alt+, since the character already shows it.
///
/////////////////////////////////////////////////////////////////////////////
std::string KeyToName(const sKeyStroke& key)
{
    std::string base;
    bool shift = key.shift;
    bool ctrl = key.ctrl;
    bool alt = key.alt;

    switch (key.kind)
    {
        case KEYSTROKE_PAUSE:
        {
            return PAUSE_NAME;
        }

        case KEYSTROKE_CHAR:
        {
            if ((key.ch < 1) || (key.ch > 26))
            {
                return std::string();
            }
            base = std::string("^") + static_cast<char>(key.ch + '@');
            ctrl = false;
            break;
        }

        case KEYSTROKE_SPECIAL:
        {
            for (const sSpecialName& entry : SPECIAL_NAMES)
            {
                if (entry.key == key.special)
                {
                    base = entry.name;
                    break;
                }
            }
            break;
        }

        case KEYSTROKE_TEXT:
        {
            if ((key.utf8.empty() == true) || (IsControlText(key.utf8) == true) ||
                (SplitCharacters(key.utf8).size() != 1))
            {
                return std::string();
            }
            base = (key.utf8 == " ") ? std::string(SPACE_NAME) : key.utf8;
            if ((ctrl == false) && (alt == false))
            {
                shift = false;
            }
            break;
        }
    }

    if (base.empty() == true)
    {
        return std::string();
    }

    std::string name;
    if (shift == true)
    {
        name += "Shift+";
    }
    if (ctrl == true)
    {
        name += "Ctrl+";
    }
    if (alt == true)
    {
        name += "Alt+";
    }
    return name + base;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in] one macro line (a trailing CR or LF is ignored)
/// @param  keys [out] the keys are appended here
///
/// @return true if the line names one key or a chord prefix with its key
///
/////////////////////////////////////////////////////////////////////////////
bool NameToKeys(const std::string& line, std::vector<sKeyStroke>& keys)
{
    std::string name = line;
    while ((name.empty() == false) && ((name.back() == '\r') || (name.back() == '\n')))
    {
        name.pop_back();
    }

    if (name.empty() == true)
    {
        return false;
    }

    if (name == PAUSE_NAME)
    {
        keys.push_back(PauseKeyStroke());
        return true;
    }

    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool more = true;

    while (more == true)
    {
        more = false;
        const char* prefixes[] = { "Shift+", "Ctrl+", "Alt+" };
        bool* flags[] = { &shift, &ctrl, &alt };

        for (size_t i = 0; i < 3; i++)
        {
            std::string prefix = prefixes[i];
            if ((name.size() > prefix.size()) && (name.compare(0, prefix.size(), prefix) == 0))
            {
                *flags[i] = true;
                name = name.substr(prefix.size());
                more = true;
            }
        }
    }

    char ch = 0;
    if (ParseControlName(name, ch) == true)
    {
        keys.push_back(CharKeyStroke(ch, shift, alt));
        return true;
    }

    for (const sSpecialName& entry : SPECIAL_NAMES)
    {
        if (name == entry.name)
        {
            keys.push_back(SpecialKeyStroke(entry.key, shift, ctrl, alt));
            return true;
        }
    }

    if (name == SPACE_NAME)
    {
        keys.push_back(TextKeyStroke(" ", shift, ctrl, alt));
        return true;
    }

    // Chord line: prefix ^K ^Q ^O ^P ^M, then ^X or one visible ASCII character.
    if ((shift == false) && (ctrl == false) && (alt == false) && (name.size() >= 3) &&
        (ParseControlName(name.substr(0, 2), ch) == true) &&
        (IsChordPrefix(CharKeyStroke(ch)) == true))
    {
        std::string rest = name.substr(2);
        char follow = 0;

        if (ParseControlName(rest, follow) == true)
        {
            keys.push_back(CharKeyStroke(ch));
            keys.push_back(CharKeyStroke(follow));
            return true;
        }

        unsigned char c = static_cast<unsigned char>(rest[0]);
        if ((rest.size() == 1) && (c > 0x20U) && (c < 0x7FU))
        {
            keys.push_back(CharKeyStroke(ch));
            keys.push_back(TextKeyStroke(rest));
            return true;
        }

        return false;
    }

    if ((IsControlText(name) == false) && (SplitCharacters(name).size() == 1))
    {
        keys.push_back(TextKeyStroke(name, shift, ctrl, alt));
        return true;
    }

    return false;
}


namespace
{

const size_t KEY_LINE_LENGTH = 60;
const char CRLF[] = "\r\n";
const char ENTER_KEY_STRING[] = "{Enter}";
const char PAUSE_LINE[] = "PauseForInput(\"{Enter}\", 'Type, then press Enter')";
const char MODERN_LINE[] = "Modern";
const char MODERN_REM_LINE[] = "REM Modern";

// Editor settings statements of a WordStar 7 recording; played as no-ops.
const char* const SETTING_STATEMENTS[] =
{
    "autorestore", "insert", "cmdtags", "hidedots", "colmode", "colreplace", "sethelplevel", "wsquiet"
};


std::string Lower(const std::string& text)
{
    std::string lower = text;
    for (char& c : lower)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}


std::string Trim(const std::string& text)
{
    const char* blanks = " \t\r\n";
    size_t first = text.find_first_not_of(blanks);
    if (first == std::string::npos)
    {
        return std::string();
    }
    size_t last = text.find_last_not_of(blanks);
    return text.substr(first, last - first + 1);
}


std::string Modifiers(bool shift, bool ctrl, bool alt)
{
    std::string text;
    if (shift == true)
    {
        text += "Shift+";
    }
    if (ctrl == true)
    {
        text += "Ctrl+";
    }
    if (alt == true)
    {
        text += "Alt+";
    }
    return text;
}


// A typed character in a Key("...") string: " { } ^ doubled.
std::string LiteralKeyString(const std::string& piece)
{
    if ((piece == "\"") || (piece == "{") || (piece == "}") || (piece == "^"))
    {
        return piece + piece;
    }
    return piece;
}


std::string SpecialKeyName(eSpecialKey special)
{
    if (special == SPECIAL_BACKSPACE)
    {
        return "BkSp";
    }
    for (const sSpecialName& entry : SPECIAL_NAMES)
    {
        if (entry.key == special)
        {
            return entry.name;
        }
    }
    return std::string();
}


// Key string of a key whose text is at most one character.
std::string OneKeyString(const sKeyStroke& key)
{
    switch (key.kind)
    {
        case KEYSTROKE_PAUSE:
        {
            return std::string();
        }

        case KEYSTROKE_CHAR:
        {
            if ((key.ch < 1) || (key.ch > 26))
            {
                return std::string();
            }
            char letter = static_cast<char>(key.ch + '@');
            if ((key.shift == false) && (key.alt == false))
            {
                return std::string("^") + letter;
            }
            return "{" + Modifiers(key.shift, true, key.alt) + letter + "}";
        }

        case KEYSTROKE_SPECIAL:
        {
            std::string name = SpecialKeyName(key.special);
            if (name.empty() == true)
            {
                return std::string();
            }
            return "{" + Modifiers(key.shift, key.ctrl, key.alt) + name + "}";
        }

        case KEYSTROKE_TEXT:
        {
            if ((key.utf8.empty() == true) || (IsControlText(key.utf8) == true))
            {
                return std::string();
            }
            if ((key.ctrl == false) && (key.alt == false))
            {
                return LiteralKeyString(key.utf8);
            }
            std::string base = (key.utf8 == "\"") ? std::string("\"\"") : key.utf8;
            return "{" + Modifiers(key.shift, key.ctrl, key.alt) + base + "}";
        }
    }
    return std::string();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// The key named between braces: Shift, Ctrl and Alt joined to the key by
/// "+", "-" or " " in any case; a key name, a letter with Ctrl, ^X, or one
/// character. (Name) and [Name] stand for Name.
///
/////////////////////////////////////////////////////////////////////////////
bool BracedKey(const std::string& name, std::vector<sKeyStroke>& keys)
{
    std::string rest = name;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    bool more = true;

    while (more == true)
    {
        more = false;
        const char* prefixes[] = { "shift", "ctrl", "alt" };
        bool* flags[] = { &shift, &ctrl, &alt };
        for (size_t i = 0; i < 3; i++)
        {
            std::string prefix = prefixes[i];
            if ((rest.size() > (prefix.size() + 1)) && (Lower(rest.substr(0, prefix.size())) == prefix) &&
                ((rest[prefix.size()] == '+') || (rest[prefix.size()] == '-') || (rest[prefix.size()] == ' ')))
            {
                *flags[i] = true;
                rest = rest.substr(prefix.size() + 1);
                more = true;
            }
        }
    }

    if ((rest.size() > 2) &&
        (((rest.front() == '(') && (rest.back() == ')')) || ((rest.front() == '[') && (rest.back() == ']'))))
    {
        rest = rest.substr(1, rest.size() - 2);
    }
    if (rest.empty() == true)
    {
        return false;
    }

    char ch = 0;
    if (ParseControlName(rest, ch) == true)
    {
        keys.push_back(CharKeyStroke(ch, shift, alt));
        return true;
    }
    if ((ctrl == true) && (rest.size() == 1) && (isalpha(static_cast<unsigned char>(rest[0])) != 0))
    {
        keys.push_back(CharKeyStroke(static_cast<char>(toupper(static_cast<unsigned char>(rest[0])) - '@'), shift, alt));
        return true;
    }

    std::string lower = Lower(rest);
    for (const sSpecialName& entry : SPECIAL_NAMES)
    {
        if (lower == Lower(entry.name))
        {
            keys.push_back(SpecialKeyStroke(entry.key, shift, ctrl, alt));
            return true;
        }
    }
    if (lower == "backtab")
    {
        keys.push_back(SpecialKeyStroke(SPECIAL_TAB, true, ctrl, alt));
        return true;
    }
    if (lower == "space")
    {
        keys.push_back(TextKeyStroke(" ", shift, ctrl, alt));
        return true;
    }
    if ((IsControlText(rest) == false) && (SplitCharacters(rest).size() == 1))
    {
        keys.push_back(TextKeyStroke(rest, shift, ctrl, alt));
        return true;
    }
    return false;
}


// The string of a "(" string ")" argument list; '' or "" inside stands for
// one delimiter.
bool StringArgument(const std::string& args, std::string& text)
{
    std::string rest = Trim(args);
    if ((rest.size() < 4) || (rest.front() != '(') || (rest.back() != ')'))
    {
        return false;
    }
    rest = Trim(rest.substr(1, rest.size() - 2));
    if (rest.size() < 2)
    {
        return false;
    }

    char quote = rest[0];
    if (((quote != '"') && (quote != '\'')) || (rest.back() != quote))
    {
        return false;
    }

    text.clear();
    for (size_t pos = 1; pos < (rest.size() - 1); pos++)
    {
        if (rest[pos] == quote)
        {
            if ((pos + 1 < (rest.size() - 1)) && (rest[pos + 1] == quote))
            {
                pos++;
            }
            else
            {
                return false;
            }
        }
        text += rest[pos];
    }
    return true;
}

} // namespace


std::string KeyToKeyString(const sKeyStroke& key)
{
    if ((key.kind != KEYSTROKE_TEXT) || (key.utf8.empty() == true))
    {
        return OneKeyString(key);
    }

    std::string text;
    for (const std::string& piece : SplitCharacters(key.utf8))
    {
        sKeyStroke one = key;
        one.utf8 = piece;
        text += OneKeyString(one);
    }
    return text;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in]  key string, delimiters of the Key() argument removed
/// @param  keys [out] the keys are appended here
///
/// @return false on an unknown key name or a stray brace or caret
///
/////////////////////////////////////////////////////////////////////////////
bool KeyStringToKeys(const std::string& text, std::vector<sKeyStroke>& keys)
{
    size_t pos = 0;
    while (pos < text.size())
    {
        char c = text[pos];
        char next = ((pos + 1) < text.size()) ? text[pos + 1] : '\0';

        if (((c == '{') || (c == '}') || (c == '^')) && (next == c))
        {
            keys.push_back(TextKeyStroke(std::string(1, c)));
            pos += 2;
            continue;
        }

        if (c == '{')
        {
            size_t close = text.find('}', pos + 1);
            if (close == std::string::npos)
            {
                return false;
            }
            std::string name = text.substr(pos + 1, close - pos - 1);
            if ((name.empty() == false) && ((name.back() == '+') || (name.back() == '-') || (name.back() == ' ')) &&
                ((close + 1) < text.size()) && (text[close + 1] == '}'))
            {
                name += '}';
                close++;
            }
            if (BracedKey(name, keys) == false)
            {
                return false;
            }
            pos = close + 1;
            continue;
        }

        if (c == '^')
        {
            char ch = 0;
            if (ParseControlName(text.substr(pos, 2), ch) == false)
            {
                return false;
            }
            keys.push_back(CharKeyStroke(ch));
            pos += 2;
            continue;
        }

        if ((c == '}') || (IsControlText(std::string(1, c)) == true))
        {
            return false;
        }

        size_t end = text.find_first_of("{}^", pos);
        if (end == std::string::npos)
        {
            end = text.size();
        }
        for (const std::string& piece : SplitCharacters(text.substr(pos, end - pos)))
        {
            if (IsControlText(piece) == true)
            {
                return false;
            }
            keys.push_back(TextKeyStroke(piece));
        }
        pos = end;
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  keys [in] key sequence
/// @param  mode [in] input mode the keys were typed in
///
/// @return the macro file bytes
///
/////////////////////////////////////////////////////////////////////////////
std::string KeysToMacroSource(const std::vector<sKeyStroke>& keys, eInputMode mode)
{
    std::string source = std::string("Sub MAIN") + CRLF;
    if (mode == INPUT_MODERN)
    {
        source += std::string(MODERN_REM_LINE) + CRLF;
    }

    std::string line;
    auto flush = [&](void)
    {
        if (line.empty() == false)
        {
            source += "Key(\"" + line + "\")" + CRLF;
            line.clear();
        }
    };
    auto add = [&](const std::string& token)
    {
        if (token.empty() == true)
        {
            return;
        }
        if ((line.empty() == false) && ((line.size() + token.size()) > KEY_LINE_LENGTH))
        {
            flush();
        }
        line += token;
        if (token == ENTER_KEY_STRING)
        {
            flush();
        }
    };

    for (size_t i = 0; i < keys.size(); i++)
    {
        const sKeyStroke& key = keys[i];

        if (key.kind == KEYSTROKE_PAUSE)
        {
            flush();
            source += std::string(PAUSE_LINE) + CRLF;
            continue;
        }

        if ((mode == INPUT_WORDSTAR) && (IsChordPrefix(key) == true) && ((i + 1) < keys.size()))
        {
            std::string follow = ChordFollowName(keys[i + 1]);
            if (follow.empty() == false)
            {
                add(OneKeyString(key) + ((follow.size() == 1) ? LiteralKeyString(follow) : follow));
                i++;
                continue;
            }
        }

        if ((key.kind == KEYSTROKE_TEXT) && (key.utf8.empty() == false))
        {
            for (const std::string& piece : SplitCharacters(key.utf8))
            {
                sKeyStroke one = key;
                one.utf8 = piece;
                add(OneKeyString(one));
            }
            continue;
        }

        add(OneKeyString(key));
    }

    flush();
    source += std::string("End Sub") + CRLF;
    source += '\x1A';
    return source;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in]  one line of macro source
/// @param  keys [out] keys of the line are appended here
///
/// @return what the line holds
///
/////////////////////////////////////////////////////////////////////////////
eMacroLine ParseMacroLine(const std::string& line, std::vector<sKeyStroke>& keys)
{
    std::string text = Trim(line);
    std::string lower = Lower(text);
    if ((text.empty() == true) ||
        ((lower.compare(0, 3, "rem") == 0) && ((text.size() == 3) || (text[3] == ' ') || (text[3] == '\t'))))
    {
        return MACRO_LINE_KEYS;
    }

    // Statements separated by ':' outside strings; ' outside parentheses
    // starts a comment.
    std::vector<std::string> statements;
    std::string current;
    char quote = '\0';
    int depth = 0;
    for (size_t i = 0; i < text.size(); i++)
    {
        char c = text[i];
        if (quote != '\0')
        {
            current += c;
            if (c == quote)
            {
                if (((i + 1) < text.size()) && (text[i + 1] == quote))
                {
                    current += c;
                    i++;
                }
                else
                {
                    quote = '\0';
                }
            }
            continue;
        }
        if ((c == '"') || ((c == '\'') && (depth > 0)))
        {
            quote = c;
            current += c;
            continue;
        }
        if (c == '\'')
        {
            break;
        }
        if (c == '(')
        {
            depth++;
        }
        else if ((c == ')') && (depth > 0))
        {
            depth--;
        }
        else if ((c == ':') && (depth == 0))
        {
            statements.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    if (quote != '\0')
    {
        return MACRO_LINE_UNSUPPORTED;
    }
    statements.push_back(current);

    for (const std::string& raw : statements)
    {
        std::string statement = Trim(raw);
        if (statement.empty() == true)
        {
            continue;
        }

        size_t length = 0;
        while ((length < statement.size()) &&
               ((isalnum(static_cast<unsigned char>(statement[length])) != 0) || (statement[length] == '_')))
        {
            length++;
        }
        if (length == 0)
        {
            return MACRO_LINE_UNSUPPORTED;
        }

        std::string word = Lower(statement.substr(0, length));
        std::string rest = Trim(statement.substr(length));
        bool call = (rest.empty() == true) || (rest[0] == '(');

        if (word == "rem")
        {
            break;
        }
        if (word == "sub")
        {
            if (Lower(rest) != "main")
            {
                return MACRO_LINE_UNSUPPORTED;
            }
            continue;
        }
        if (word == "end")
        {
            return (Lower(rest) == "sub") ? MACRO_LINE_END : MACRO_LINE_UNSUPPORTED;
        }
        if (word == "key")
        {
            std::string keyString;
            if (StringArgument(rest, keyString) == false)
            {
                return MACRO_LINE_UNSUPPORTED;
            }
            if (KeyStringToKeys(keyString, keys) == false)
            {
                return MACRO_LINE_BAD_KEY;
            }
            continue;
        }
        if ((word == "pauseforinput") && (call == true))
        {
            keys.push_back(PauseKeyStroke());
            continue;
        }

        bool setting = false;
        for (const char* name : SETTING_STATEMENTS)
        {
            if (word == name)
            {
                setting = true;
            }
        }
        if ((setting == true) && (call == true))
        {
            continue;
        }
        return MACRO_LINE_UNSUPPORTED;
    }

    return MACRO_LINE_KEYS;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  lines  [in]  lines of a one-key-per-line macro
/// @param  source [out] the macro source
///
/// @return false if a line is not a key name
///
/////////////////////////////////////////////////////////////////////////////
bool KeyLinesToMacroSource(const std::vector<std::string>& lines, std::string& source)
{
    std::vector<sKeyStroke> keys;
    eInputMode mode = INPUT_WORDSTAR;

    for (size_t i = 0; i < lines.size(); i++)
    {
        std::string line = lines[i];
        while ((line.empty() == false) && ((line.back() == '\r') || (line.back() == '\n')))
        {
            line.pop_back();
        }
        if ((i == 0) && (line == MODERN_LINE))
        {
            mode = INPUT_MODERN;
            continue;
        }
        if (line.empty() == true)
        {
            continue;
        }
        if (NameToKeys(line, keys) == false)
        {
            return false;
        }
    }

    source = KeysToMacroSource(keys, mode);
    return true;
}
