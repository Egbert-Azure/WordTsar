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

#ifndef GUI_MACEDITMENU_H
#define GUI_MACEDITMENU_H

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Registers the AppKit defaults that keep Dictation and Emoji & Symbols
/// out of the Edit menu. Registration domain only: nothing is written to
/// the user's preferences. Call before the QApplication is created, since
/// AppKit reads these when it builds the menu bar.
///
/// Implemented in maceditmenu.mm (Objective-C++); this header is plain
/// C++ so it can be included from Qt-side code.
///
/////////////////////////////////////////////////////////////////////////////
void RegisterMacEditMenuDefaults();

#endif // GUI_MACEDITMENU_H
