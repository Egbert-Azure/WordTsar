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

#include "tuiapplication.h"

#include "terminalfactory.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTuiApplication
///
/// @brief
/// Small reusable event loop for applications using the WordStar TUI layer.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct the application host.
///
/////////////////////////////////////////////////////////////////////////////
cTuiApplication::cTuiApplication(void)
{
    mQuit = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destroy the application host.
///
/////////////////////////////////////////////////////////////////////////////
cTuiApplication::~cTuiApplication(void)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return process exit code
///
/// @brief
/// Run the terminal event loop.
///
/////////////////////////////////////////////////////////////////////////////
int cTuiApplication::Run(void)
{
    if (Initialize() == false)
    {
        return 1;
    }

    while (mQuit == false)
    {
        Draw(mScreen, mTheme);
        mScreen.Present(*mDriver);
        OnAfterDraw(*mDriver);
        mDriver->Flush();

        sInputEvent event;

        if (mDriver->ReadEvent(event, 50) == true)
        {
            if (event.type == INPUT_TYPE_RESIZE)
            {
                mScreen.Resize(event.resizeRows, event.resizeCols);
                OnResize(event.resizeRows, event.resizeCols);
            }
            else
            {
                HandleEvent(event);
            }
        }
    }

    mDriver->Close();

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Request application shutdown.
///
/////////////////////////////////////////////////////////////////////////////
void cTuiApplication::Quit(void)
{
    mQuit = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  draw [in] frame drawer (screen, theme)
/// @param  handle [in] event handler; returns false to end the modal
///
/// @return nothing
///
/// @brief
/// Run a nested modal loop on top of the main loop, reusing the live screen and
/// terminal driver. Draws each frame, presents it, and dispatches one input
/// event (handling resize transparently) until the handler returns false or the
/// application quits. The hardware cursor is hidden; focused widgets draw their
/// own cursor.
///
/////////////////////////////////////////////////////////////////////////////
void cTuiApplication::RunModalLoop(const std::function<void(cScreen&, const cTheme&)>& draw,
                                   const std::function<bool(const sInputEvent&)>& handle)
{
    bool running = true;

    while ((running == true) && (mQuit == false))
    {
        draw(mScreen, mTheme);
        mScreen.Present(*mDriver);
        mDriver->SetCursor(0, 0, false);
        mDriver->Flush();

        sInputEvent event;

        if (mDriver->ReadEvent(event, 50) == true)
        {
            if (event.type == INPUT_TYPE_RESIZE)
            {
                mScreen.Resize(event.resizeRows, event.resizeCols);
                OnResize(event.resizeRows, event.resizeCols);
            }
            else
            {
                running = handle(event);
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return application color theme
///
/// @brief
/// Get the mutable theme used by the application.
///
/////////////////////////////////////////////////////////////////////////////
cTheme& cTuiApplication::GetTheme(void)
{
    return mTheme;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  draw [in] frame drawer (screen, theme)
///
/// @return nothing
///
/// @brief
/// Paint and flush a single frame immediately, outside the event loop. Lets a
/// blocking operation update an overlay (e.g. a progress dialog) on screen.
///
/////////////////////////////////////////////////////////////////////////////
void cTuiApplication::PresentFrame(const std::function<void(cScreen&, const cTheme&)>& draw)
{
    // The driver only exists once Initialize() has run. A host may report
    // load progress for a command-line file before the loop starts; skip
    // presenting rather than dereferencing a null driver.
    if (mDriver == nullptr)
    {
        return;
    }

    draw(mScreen, mTheme);
    mScreen.Present(*mDriver);
    mDriver->SetCursor(0, 0, false);
    mDriver->Flush();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int rows [in] new row count
/// @param  int cols [in] new column count
///
/// @return nothing
///
/// @brief
/// Hook called after the terminal size changes.
///
/////////////////////////////////////////////////////////////////////////////
void cTuiApplication::OnResize(int rows, int cols)
{
    (void)rows;
    (void)cols;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cTerminalDriver& driver [in,out] terminal driver
///
/// @return nothing
///
/// @brief
/// Hook called after drawing so applications can position the cursor.
///
/////////////////////////////////////////////////////////////////////////////
void cTuiApplication::OnAfterDraw(cTerminalDriver& driver)
{
    driver.SetCursor(0, 0, false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when terminal initialization succeeded
///
/// @brief
/// Create the terminal driver and initialize the screen buffer.
///
/////////////////////////////////////////////////////////////////////////////
bool cTuiApplication::Initialize(void)
{
    mDriver = cTerminalFactory::CreateDefault();

    if (mDriver->Open() == false)
    {
        return false;
    }

    ResizeFromTerminal();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Resize the screen from the terminal backend size.
///
/////////////////////////////////////////////////////////////////////////////
void cTuiApplication::ResizeFromTerminal(void)
{
    sTerminalSize size = mDriver->GetSize();

    mScreen.Resize(size.rows, size.cols);
    OnResize(size.rows, size.cols);
}

}
