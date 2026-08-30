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

#include "tabbar.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTabBar
///
/// @brief
/// Draws and manages a simple row of tab labels.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct an empty tab bar.
///
/////////////////////////////////////////////////////////////////////////////
cTabBar::cTabBar(void)
{
    mBounds.row = 0;
    mBounds.col = 0;
    mBounds.rows = 1;
    mBounds.cols = 0;
    mActiveTab = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] tab bar bounds
///
/// @return nothing
///
/// @brief
/// Set tab bar bounds.
///
/////////////////////////////////////////////////////////////////////////////
void cTabBar::SetBounds(const sRect& bounds)
{
    mBounds = bounds;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& title [in] tab title
///
/// @return nothing
///
/// @brief
/// Add a tab to the bar.
///
/////////////////////////////////////////////////////////////////////////////
void cTabBar::AddTab(const std::string& title)
{
    mTabs.push_back(title);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the tab bar.
///
/////////////////////////////////////////////////////////////////////////////
void cTabBar::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle normal = theme.GetStyle(THEME_ROLE_MENU);
    sStyle active = theme.GetStyle(THEME_ROLE_MENU_ACTIVE);
    int col = mBounds.col;

    screen.FillRect(mBounds, " ", normal);

    for (size_t index = 0; index < mTabs.size(); ++index)
    {
        std::string text = " " + mTabs[index] + " ";
        sStyle style;

        if (static_cast<int>(index) == mActiveTab)
        {
            style = active;
        }
        else
        {
            style = normal;
        }

        if ((col + static_cast<int>(text.size())) >= (mBounds.col + mBounds.cols))
        {
            break;
        }

        screen.PutText(mBounds.row, col, text, style);
        col += static_cast<int>(text.size()) + 1;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the active tab changed
///
/// @brief
/// Handle Ctrl-PageUp and Ctrl-PageDown tab switching.
///
/////////////////////////////////////////////////////////////////////////////
bool cTabBar::HandleEvent(const sInputEvent& event)
{
    if (event.type == INPUT_TYPE_MOUSE)
    {
        if (event.mouseAction != MOUSE_ACTION_PRESS)
        {
            return false;
        }

        if (event.mouseButton != MOUSE_BUTTON_LEFT)
        {
            return false;
        }

        if (event.mouseRow != mBounds.row)
        {
            return false;
        }

        int col = mBounds.col;

        for (size_t index = 0; index < mTabs.size(); ++index)
        {
            std::string text = " " + mTabs[index] + " ";
            int endCol = col + static_cast<int>(text.size());

            if ((event.mouseCol >= col) && (event.mouseCol < endCol))
            {
                mActiveTab = static_cast<int>(index);
                return true;
            }

            col = endCol + 1;
        }

        return false;
    }

    if (event.type != INPUT_TYPE_SPECIAL)
    {
        return false;
    }

    if ((event.special == SPECIAL_KEY_PAGE_UP) && (event.ctrl == true))
    {
        if (mActiveTab > 0)
        {
            --mActiveTab;
        }
        return true;
    }

    if ((event.special == SPECIAL_KEY_PAGE_DOWN) && (event.ctrl == true))
    {
        if (mActiveTab < (static_cast<int>(mTabs.size()) - 1))
        {
            ++mActiveTab;
        }
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return active tab index
///
/// @brief
/// Get the active tab index.
///
/////////////////////////////////////////////////////////////////////////////
int cTabBar::GetActiveTab(void) const
{
    return mActiveTab;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int index [in] tab index
///
/// @return nothing
///
/// @brief
/// Set the active tab index.
///
/////////////////////////////////////////////////////////////////////////////
void cTabBar::SetActiveTab(int index)
{
    if ((index >= 0) && (index < static_cast<int>(mTabs.size())))
    {
        mActiveTab = index;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return number of tabs
///
/// @brief
/// Get the number of tabs.
///
/////////////////////////////////////////////////////////////////////////////
int cTabBar::GetTabCount(void) const
{
    return static_cast<int>(mTabs.size());
}

}
