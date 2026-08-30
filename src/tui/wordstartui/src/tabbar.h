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

#ifndef WORDTSAR_TUI_TABBAR_H
#define WORDTSAR_TUI_TABBAR_H

#include "screen.h"
#include "theme.h"
#include <string>
#include <vector>

namespace wordstartui
{

class cTabBar
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cTabBar(void);

    void SetBounds(const sRect& bounds);
    void AddTab(const std::string& title);
    void Draw(cScreen& screen, const cTheme& theme);
    bool HandleEvent(const sInputEvent& event);
    int GetActiveTab(void) const;
    void SetActiveTab(int index);
    int GetTabCount(void) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    sRect mBounds;
    std::vector<std::string> mTabs;
    int mActiveTab;
};

}

#endif
