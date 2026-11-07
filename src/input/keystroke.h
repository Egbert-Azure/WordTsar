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

// Parse one macro line into keys. A Classic chord line such as "^KB" or "^P^B"
// gives two keys. Returns false when the line is not a key name.
bool NameToKeys(const std::string& line, std::vector<sKeyStroke>& keys);

// Macro lines for a key sequence, one key per line. In INPUT_WORDSTAR a chord
// prefix (^K ^Q ^O ^P ^M) shares its line with the key that follows it.
// Text of several characters gives one line per character. Keys without a
// name are left out.
std::vector<std::string> KeysToLines(const std::vector<sKeyStroke>& keys, eInputMode mode);

/// @}

#endif // KEYSTROKE_H
