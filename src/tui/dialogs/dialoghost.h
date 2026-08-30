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

#ifndef WORDTSAR_WSTUI_DIALOGHOST_H
#define WORDTSAR_WSTUI_DIALOGHOST_H

#include <functional>
#include <string>

namespace wordstartui
{
class cDialog;
class cScreen;
class cTheme;
struct sInputEvent;
struct sTerminalSize;
struct sTerminalCapabilities;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @class iWSDialogHost
///
/// @brief
/// Services the application provides to the editor control so it can run modal
/// dialogs synchronously (find, replace, message boxes, etc.) on top of the
/// running event loop, and query the screen geometry to centre them.
///
/////////////////////////////////////////////////////////////////////////////
class iWSDialogHost
{
public:
    virtual ~iWSDialogHost(void) = default;

    // Run a plain cDialog modally over the current screen; returns when the
    // dialog sets a result (OK/Cancel/etc.).
    virtual void HostRunModal(wordstartui::cDialog& dialog) = 0;

    // Run a custom modal (drawer + event handler) for dialogs that are not a
    // plain cDialog (spell check list, file browser, print preview). The
    // handler returns false to close the modal.
    virtual void HostRunModalRaw(
        const std::function<void(wordstartui::cScreen&, const wordstartui::cTheme&)>& draw,
        const std::function<bool(const wordstartui::sInputEvent&)>& handle) = 0;

    // The application's colour theme (for building dialog widgets).
    virtual const wordstartui::cTheme& HostTheme(void) = 0;

    // Current terminal size (rows, cols) for centring dialogs.
    virtual wordstartui::sTerminalSize HostScreenSize(void) = 0;

    // The terminal's color capabilities (16 / 256 / truecolor) so dialogs such
    // as the color picker can default to the depth the terminal supports.
    virtual wordstartui::sTerminalCapabilities HostCapabilities(void) = 0;

    // Show / update a load-progress overlay during a blocking file load, and
    // remove it when done. percent is 0..100. These paint one frame each
    // (no event loop), so they can be called from inside a synchronous load.
    virtual void HostShowLoadProgress(const std::string& message, int percent) = 0;
    virtual void HostHideLoadProgress(void) = 0;
};

#endif // WORDTSAR_WSTUI_DIALOGHOST_H
