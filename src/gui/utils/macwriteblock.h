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

#ifndef GUI_MACWRITEBLOCK_H
#define GUI_MACWRITEBLOCK_H

#include <string>

/////////////////////////////////////////////////////////////////////////////
///
/// @param  startDir    [in]     - folder the panel opens in, may be empty
/// @param  asciiFormat [in,out] - Copy in ASCII Format box state
/// @param  asciiLocked [in]     - the box shows ticked and cannot be changed
///
/// @return the chosen path, empty if cancelled
///
/// @brief
/// ^KW Write Block: the native macOS save panel with a Copy in ASCII
/// Format check box as its accessory view.
///
/// Implemented in macwriteblock.mm (Objective-C++); this header is plain
/// C++ so it can be included from Qt-side code.
///
/////////////////////////////////////////////////////////////////////////////
std::string ShowMacWriteBlockPanel(const std::string& startDir, bool& asciiFormat, bool asciiLocked);

#endif // GUI_MACWRITEBLOCK_H
