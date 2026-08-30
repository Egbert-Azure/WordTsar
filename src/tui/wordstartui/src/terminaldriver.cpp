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

#include "terminaldriver.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTerminalDriver
///
/// @brief
/// Defines the platform-specific terminal input and output backend interface.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destroy the terminal driver interface.
///
/////////////////////////////////////////////////////////////////////////////
cTerminalDriver::~cTerminalDriver(void)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eCursorShape shape [in] requested cursor shape (unused in base)
///
/// @return nothing
///
/// @brief
/// Default cursor-shape handler: do nothing. Drivers that support DECSCUSR
/// override this.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriver::SetCursorShape(eCursorShape shape)
{
    (void)shape;
}

}
