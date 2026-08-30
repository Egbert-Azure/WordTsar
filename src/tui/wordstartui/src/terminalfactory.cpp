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

#include "terminalfactory.h"

#ifdef _WIN32
#include "terminaldriverwin32.h"
#else
#include "terminaldriverposix.h"
#endif

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTerminalFactory
///
/// @brief
/// Creates the correct terminal backend for the current platform.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return terminal driver instance for the current platform
///
/// @brief
/// Create the default non-serial terminal backend.
///
/////////////////////////////////////////////////////////////////////////////
std::unique_ptr<cTerminalDriver> cTerminalFactory::CreateDefault(void)
{
#ifdef _WIN32
    return std::make_unique<cTerminalDriverWin32>();
#else
    return std::make_unique<cTerminalDriverPosix>();
#endif
}

}
