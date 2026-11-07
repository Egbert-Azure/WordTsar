#ifndef KEYSTROKE_H
#define KEYSTROKE_H

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

#include <string>
#include <vector>

#include "src/core/include/config.h"
#include "src/input/inputhandler.h"

/// @ingroup Input
/// @{


/////////////////////////////////////////////////////////////////////////////
///
/// @enum eKeyStrokeKind
///
/// @brief
/// What a sKeyStroke holds.
///
/////////////////////////////////////////////////////////////////////////////
enum eKeyStrokeKind
{
    KEYSTROKE_CHAR,         ///< control character 1..26 (^A..^Z) in ch
    KEYSTROKE_SPECIAL,      ///< named key in special
    KEYSTROKE_TEXT,         ///< typed text in utf8
    KEYSTROKE_PAUSE         ///< macro pause marker, never sent to an input handler
};


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sKeyStroke
///
/// @brief
/// One key in front-end neutral form, as fed to cEditorBase::FeedKey.
///
/////////////////////////////////////////////////////////////////////////////
struct sKeyStroke
{
    eKeyStrokeKind kind = KEYSTROKE_TEXT;
    char ch = 0;                            ///< KEYSTROKE_CHAR: 1..26
    eSpecialKey special = SPECIAL_ESCAPE;   ///< KEYSTROKE_SPECIAL
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
    std::string utf8;                       ///< KEYSTROKE_TEXT
};


sKeyStroke CharKeyStroke(char ch, bool shift = false, bool alt = false);
sKeyStroke SpecialKeyStroke(eSpecialKey key, bool shift = false, bool ctrl = false, bool alt = false);
sKeyStroke TextKeyStroke(const std::string& utf8, bool shift = false, bool ctrl = false, bool alt = false);
sKeyStroke PauseKeyStroke(void);

// Name of one key ("^K", "Enter", "Ctrl+Left", "Alt+k", "a", "Space", "PAUSE").
// Empty when the key has no name.
std::string KeyToName(const sKeyStroke& key);

// Parse one line of a one-key-per-line macro into keys. A Classic chord line
// such as "^KB" or "^P^B" gives two keys. Returns false when the line is not a
// key name.
bool NameToKeys(const std::string& line, std::vector<sKeyStroke>& keys);

// WordStar 7 key string of one key ("^K", "{Enter}", "{Alt+t}", "a", "{{");
// text of several characters gives one entry per character. Empty when the
// key has none (a pause, a control character as text).
std::string KeyToKeyString(const sKeyStroke& key);

// Keys of a WordStar 7 key string: characters, ^X control keys, ^[ (Esc),
// ^\ and ^], {Name} keys with Shift/Ctrl/Alt and "+", "-" or " ", {Insert}
// as ^V, ESC c FS for the character c, and the doubled {{ }} ^^ for a
// literal brace or caret. Returns false on an unknown name or a stray brace,
// caret or control character; bad, if not nullptr, receives that part.
bool KeyStringToKeys(const std::string& text, std::vector<sKeyStroke>& keys, std::string* bad = nullptr);

// WordStar 7 macro source: Sub MAIN, REM Modern for INPUT_MODERN, Key("...")
// lines, PauseForInput("{Enter}", 'Type, then press Enter') for a pause,
// End Sub, CRLF line ends, one 0x1A at the end. In INPUT_WORDSTAR a chord
// prefix (^K ^Q ^O ^P ^M) is followed by its key in upper case. A Key line
// ends after {Enter} and before it would pass 60 characters.
std::string KeysToMacroSource(const std::vector<sKeyStroke>& keys, eInputMode mode);

// Macro source for a one-key-per-line macro (first line "Modern" for a
// Modern recording). Returns false on a line that is not a key name.
bool KeyLinesToMacroSource(const std::vector<std::string>& lines, std::string& source);

/// @}

#endif // KEYSTROKE_H
