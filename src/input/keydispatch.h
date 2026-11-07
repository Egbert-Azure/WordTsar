#ifndef KEYDISPATCH_H
#define KEYDISPATCH_H

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

#include "src/input/inputhandler.h"
#include "src/input/keystroke.h"

class cEditorBase;

/// @ingroup Input
/// @{

// Send one key to the input handler; typed text the handler does not take
// is inserted into the document. Returns true if the key was handled.
bool DispatchKey(cEditorBase& editor, IInputHandler& input, const sKeyStroke& key);

/// @}

#endif // KEYDISPATCH_H
