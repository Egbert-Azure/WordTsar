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

#ifndef GUI_MACLOOKUP_H
#define GUI_MACLOOKUP_H

#include <string>

/////////////////////////////////////////////////////////////////////////////
///
/// @param  nsViewPtr [in] - the NSView backing the calling QWidget, i.e.
///                          reinterpret_cast<void*>(widget->winId())
/// @param  word [in] - UTF-8 word to look up
/// @param  x [in] - anchor point X, in the view's own coordinate system
/// @param  y [in] - anchor point Y, in the view's own coordinate system
///
/// @return nothing
///
/// @brief
/// Shows macOS's native Look Up popover (the same one Safari/TextEdit/Notes
/// show for right-click "Look Up") for a word, anchored at a point in the
/// given view. Backed by whatever dictionary/thesaurus sources are enabled
/// in Dictionary.app -- no custom UI or synonym parsing needed. Implements
/// WordTsar's ^QJ Thesaurus command on the GUI.
///
/// Implemented in maclookup.mm (Objective-C++); this header is plain C++
/// so it can be included from Qt-side code without pulling in Objective-C.
///
/////////////////////////////////////////////////////////////////////////////
void ShowMacDefinitionPopover(void* nsViewPtr, const std::string& word, double x, double y);

#endif // GUI_MACLOOKUP_H
