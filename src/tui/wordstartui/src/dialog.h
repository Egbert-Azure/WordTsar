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

#ifndef WORDTSAR_TUI_DIALOG_H
#define WORDTSAR_TUI_DIALOG_H

#include "button.h"
#include "widget.h"
#include <memory>
#include <string>
#include <vector>

namespace wordstartui
{

class cDialog
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cDialog(const sRect& bounds, const std::string& title);

    void AddWidget(std::unique_ptr<cWidget> widget);
    void Draw(cScreen& screen, const cTheme& theme);
    bool HandleEvent(const sInputEvent& event);
    void SetResult(eDialogResult result);
    eDialogResult GetResult(void) const;
    void FocusFirst(void);
    cWidget* GetFocusedWidget(void);

private:
    void FocusNext(void);
    void FocusPrevious(void);
    void SetFocusIndex(int index);
    int FindNextFocusable(int start, int step) const;
    int FindWidgetAt(int row, int col) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    sRect mBounds;
    std::string mTitle;
    std::vector<std::unique_ptr<cWidget>> mWidgets;
    int mFocusIndex;
    eDialogResult mResult;
};

}

#endif
