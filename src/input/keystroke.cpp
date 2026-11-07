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


/////////////////////////////////////////////////////////////////////////////
///
/// @param  keys [in] key sequence
/// @param  mode [in] input mode the keys were typed in
///
/// @return one macro line per key, chord lines combined in INPUT_WORDSTAR
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> KeysToLines(const std::vector<sKeyStroke>& keys, eInputMode mode)
{
    std::vector<std::string> lines;

    for (size_t i = 0; i < keys.size(); i++)
    {
        const sKeyStroke& key = keys[i];

        if ((mode == INPUT_WORDSTAR) && (IsChordPrefix(key) == true) && ((i + 1) < keys.size()))
        {
            std::string follow = ChordFollowName(keys[i + 1]);
            if (follow.empty() == false)
            {
                lines.push_back(KeyToName(key) + follow);
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
                std::string name = KeyToName(one);
                if (name.empty() == false)
                {
                    lines.push_back(name);
                }
            }
            continue;
        }

        std::string name = KeyToName(key);
        if (name.empty() == false)
        {
            lines.push_back(name);
        }
    }

    return lines;
}
