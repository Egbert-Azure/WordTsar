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

#ifndef WORDTSAR_TUI_TUIAPPLICATION_H
#define WORDTSAR_TUI_TUIAPPLICATION_H

#include "screen.h"
#include "terminaldriver.h"
#include "theme.h"
#include <memory>
#include <functional>

namespace wordstartui
{

class cTuiApplication
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cTuiApplication(void);
    virtual ~cTuiApplication(void);

    int Run(void);
    void Quit(void);
    cTheme& GetTheme(void);

protected:
    virtual void Draw(cScreen& screen, const cTheme& theme) = 0;
    virtual bool HandleEvent(const sInputEvent& event) = 0;
    virtual void OnResize(int rows, int cols);
    virtual void OnAfterDraw(cTerminalDriver& driver);

    // Run a nested modal loop over the live screen/driver: repeatedly draw and
    // feed events to the handler until it returns false (dialog finished) or the
    // application quits. Resize is handled transparently. Enables synchronous
    // modal dialogs on top of the main event loop.
    void RunModalLoop(const std::function<void(cScreen&, const cTheme&)>& draw,
                      const std::function<bool(const sInputEvent&)>& handle);

    // Paint and present a single frame synchronously (no event loop). Used to
    // update an overlay -- such as a progress dialog -- during a blocking
    // operation that cannot pump the main loop.
    void PresentFrame(const std::function<void(cScreen&, const cTheme&)>& draw);

private:
    bool Initialize(void);
    void ResizeFromTerminal(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    std::unique_ptr<cTerminalDriver> mDriver;
    cScreen mScreen;
    cTheme mTheme;
    bool mQuit;
};

}

#endif
