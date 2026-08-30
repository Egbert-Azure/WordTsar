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

#ifndef WORDTSAR_TUI_PROGRESSDIALOG_H
#define WORDTSAR_TUI_PROGRESSDIALOG_H

#include <string>

namespace wordstartui
{

class cScreen;
class cTheme;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cProgressDialog
///
/// @brief
/// A small centred dialog showing a title, a one-line message, and a
/// percent-complete bar. It is drawn on demand (no event loop of its own), so
/// hosts can paint it during a blocking operation such as loading a file.
///
/////////////////////////////////////////////////////////////////////////////
class cProgressDialog
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cProgressDialog(void);

    void SetTitle(const std::string& title);
    void SetMessage(const std::string& message);
    void SetPercent(int percent);
    void Draw(cScreen& screen, const cTheme& theme) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    std::string mTitle;
    std::string mMessage;
    int mPercent;
};

}

#endif // WORDTSAR_TUI_PROGRESSDIALOG_H
