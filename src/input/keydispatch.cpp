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

#include "src/input/keydispatch.h"
#include "src/core/editor/editorbase.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor [in] editor that receives text the handler does not take
/// @param  input  [in] active input handler
/// @param  key    [in] key stroke
///
/// @return true if the key was handled
///
/// @brief
/// While a chord is pending, Delete and Backspace complete it as their
/// character equivalents (0x7F and ^H). Control keys go to HandleKey, named
/// keys to HandleSpecialKey. A typed ASCII character goes to HandleKey
/// first; typed text the handler does not take is inserted if neither Ctrl
/// nor Alt is held.
///
/////////////////////////////////////////////////////////////////////////////
bool DispatchKey(cEditorBase& editor, IInputHandler& input, const sKeyStroke& key)
{
    switch (key.kind)
    {
        case KEYSTROKE_PAUSE:
        {
            return false ;
        }

        case KEYSTROKE_SPECIAL:
        {
            if (input.CheckControlMode() == true)
            {
                if (key.special == SPECIAL_DELETE)
                {
                    input.HandleKey(static_cast<char>(0x7F), key.shift) ;
                    return true ;
                }
                if (key.special == SPECIAL_BACKSPACE)
                {
                    input.HandleKey(static_cast<char>(0x08), key.shift) ;
                    return true ;
                }
            }
            return input.HandleSpecialKey(key.special, key.shift, key.ctrl, key.alt) ;
        }

        case KEYSTROKE_CHAR:
        {
            return input.HandleKey(key.ch, key.shift, key.alt) ;
        }

        case KEYSTROKE_TEXT:
        {
            if (key.utf8.empty() == true)
            {
                return false ;
            }

            // Only a single ASCII character can be a command key.
            bool ascii = (key.utf8.size() == 1) && (static_cast<unsigned char>(key.utf8[0]) < 0x80U) ;
            if ((ascii == true) && (input.HandleKey(key.utf8[0], key.shift, key.alt) == true))
            {
                return true ;
            }

            if ((key.ctrl == true) || (key.alt == true))
            {
                return false ;
            }

            editor.InsertText(key.utf8) ;
            return true ;
        }
    }

    return false ;
}
