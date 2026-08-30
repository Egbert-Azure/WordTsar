#ifndef INPUTHANDLER_H
#define INPUTHANDLER_H

//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
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

#include "src/core/include/config.h"

// Forward declaration
class cEditorBase;

/// @ingroup Input
/// @{


/////////////////////////////////////////////////////////////////////////////
///
/// @enum eSpecialKey
///
/// @brief
/// Non-character keys that cannot be represented as a single ASCII byte.
/// Used by HandleSpecialKey() to route function keys, navigation keys,
/// and editing keys through the input handler instead of handling them
/// directly in platform-specific event handlers (keyPressEvent / OnEvent).
///
/////////////////////////////////////////////////////////////////////////////
enum eSpecialKey
{
    SPECIAL_F1,
    SPECIAL_F2,
    SPECIAL_F3,
    SPECIAL_F4,
    SPECIAL_F5,
    SPECIAL_F6,
    SPECIAL_F7,
    SPECIAL_F8,
    SPECIAL_F9,
    SPECIAL_F10,
    SPECIAL_F11,
    SPECIAL_F12,
    SPECIAL_HOME,
    SPECIAL_END,
    SPECIAL_UP,
    SPECIAL_DOWN,
    SPECIAL_LEFT,
    SPECIAL_RIGHT,
    SPECIAL_PAGE_UP,
    SPECIAL_PAGE_DOWN,
    SPECIAL_DELETE,
    SPECIAL_BACKSPACE,
    SPECIAL_TAB,
    SPECIAL_ENTER,
    SPECIAL_ESCAPE
};


/////////////////////////////////////////////////////////////////////////////
///
/// @class IInputHandler
///
/// @brief
/// Abstract base class for keyboard input handlers.
/// Implements Strategy pattern for swappable input modes.
///
/// This interface allows the editor to support different keyboard command
/// schemes (WordStar, Modern/CUA, WordPerfect, Emacs, etc.) through
/// polymorphism. Each concrete implementation interprets keystrokes
/// according to its own command set and invokes appropriate editor methods.
///
/// Platform Independence:
/// - Uses char-based API (not Qt-specific) for portability
/// - Can work with any UI framework (Qt, ncurses, terminal, web)
/// - Editor adapter layer (editorctrl) translates platform keys to chars
///
/// Key Routing:
/// - HandleKey(): Ctrl+letter (values 1-26), Alt+letter, printable chars
/// - HandleSpecialKey(): F-keys, navigation, editing keys (arrows, etc.)
///
/////////////////////////////////////////////////////////////////////////////
class IInputHandler
{
public:
    virtual ~IInputHandler(void) = default;

    // Handle a keyboard character input. Returns true if handled as command, false to insert as text.
    // ch: character value (1-26 for Ctrl+A through Ctrl+Z, or printable ASCII)
    // shift: true if Shift modifier is held
    // alt: true if Alt modifier is held (GUI: Qt::AltModifier, TUI: ESC prefix)
    virtual bool HandleKey(char ch, bool shift = false, bool alt = false) = 0;

    // Handle a special (non-character) key. Returns true if handled, false otherwise.
    // key: the special key identifier (F-keys, navigation, editing)
    // shift/ctrl/alt: modifier key state
    virtual bool HandleSpecialKey(eSpecialKey key, bool shift = false,
                                  bool ctrl = false, bool alt = false) = 0;

    // Check if currently in a command sequence mode (e.g., after Ctrl+K in WordStar mode).
    virtual bool CheckControlMode(void) = 0;

    // Get the current help display status for the active command mode.
    virtual eHelpDisplay GetHelpStatus(void) = 0;
};

/// @}

#endif // INPUTHANDLER_H
