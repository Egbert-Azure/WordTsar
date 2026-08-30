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

#ifndef WORDTSAR_TUI_TERMINALDRIVER_H
#define WORDTSAR_TUI_TERMINALDRIVER_H

#include "tuidefs.h"
#include <string>

namespace wordstartui
{

class cTerminalDriver
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    virtual ~cTerminalDriver(void);

    virtual bool Open(void) = 0;
    virtual void Close(void) = 0;
    virtual sTerminalSize GetSize(void) const = 0;
    virtual sTerminalCapabilities GetCapabilities(void) const = 0;
    virtual bool ReadEvent(sInputEvent& event, int timeoutMs) = 0;
    virtual void Present(const std::vector<sCellRun>& runs) = 0;
    virtual void SetCursor(int row, int col, bool visible) = 0;

    // Change the shape of the hardware cursor (block / underline / bar). Drivers
    // that cannot change the cursor shape leave this as the default no-op.
    virtual void SetCursorShape(eCursorShape shape);

    virtual void Flush(void) = 0;
};

}

#endif
