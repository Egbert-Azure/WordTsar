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

#include "listbox.h"
#include "scrollbar.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cListBox
///
/// @brief
/// Scrollable list widget used by pickers, file lists, and suggestions.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] list bounds
///
/// @return nothing
///
/// @brief
/// Construct an empty list box.
///
/////////////////////////////////////////////////////////////////////////////
cListBox::cListBox(const sRect& bounds)
{
    mBounds = bounds;
    mSelectedIndex = 0;
    mTopIndex = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<std::string>& items [in] list items
///
/// @return nothing
///
/// @brief
/// Replace list items.
///
/////////////////////////////////////////////////////////////////////////////
void cListBox::SetItems(const std::vector<std::string>& items)
{
    mItems = items;
    mSelectedIndex = 0;
    mTopIndex = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw visible list items and a simple scrollbar.
///
/////////////////////////////////////////////////////////////////////////////
void cListBox::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle normal = theme.GetStyle(THEME_ROLE_LIST);
    sStyle selected = theme.GetStyle(THEME_ROLE_LIST_SELECTED);

    screen.FillRect(mBounds, " ", normal);

    for (int row = 0; row < mBounds.rows; ++row)
    {
        int itemIndex = mTopIndex + row;

        if (itemIndex >= static_cast<int>(mItems.size()))
        {
            break;
        }

        sStyle style;

        if (itemIndex == mSelectedIndex)
        {
            style = selected;
        }
        else
        {
            style = normal;
        }

        sRect lineRect;
        lineRect.row = mBounds.row + row;
        lineRect.col = mBounds.col;
        lineRect.rows = 1;
        lineRect.cols = mBounds.cols;
        screen.FillRect(lineRect, " ", style);
        screen.PutText(mBounds.row + row, mBounds.col, mItems[static_cast<size_t>(itemIndex)], style);
    }

    cScrollBar scrollbar;
    sRect track;
    track.row = mBounds.row;
    track.col = mBounds.col + mBounds.cols - 1;
    track.rows = mBounds.rows;
    track.cols = 1;
    scrollbar.SetMetrics(track, static_cast<int>(mItems.size()), mBounds.rows, mTopIndex);

    if (scrollbar.NeedsBar() == true)
    {
        scrollbar.Draw(screen, normal, selected);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when selection or scroll position changed
///
/// @brief
/// Handle list navigation keys.
///
/////////////////////////////////////////////////////////////////////////////
bool cListBox::HandleEvent(const sInputEvent& event)
{
    if (mEnabled == false)
    {
        return false;
    }

    if (event.type == INPUT_TYPE_MOUSE)
    {
        if (IsMouseInside(event) == false)
        {
            return false;
        }

        if (event.mouseAction == MOUSE_ACTION_WHEEL)
        {
            MoveSelection(event.mouseWheel);
            return true;
        }

        bool leftButton = (event.mouseButton == MOUSE_BUTTON_LEFT);
        bool pressOrDrag = (event.mouseAction == MOUSE_ACTION_PRESS) ||
                           (event.mouseAction == MOUSE_ACTION_DRAG);
        int scrollCol = mBounds.col + mBounds.cols - 1;

        // Press or drag on the scrollbar column scrolls the list.
        if ((pressOrDrag == true) && (leftButton == true) && (event.mouseCol == scrollCol) &&
            (static_cast<int>(mItems.size()) > mBounds.rows))
        {
            cScrollBar scrollbar;
            sRect track;
            track.row = mBounds.row;
            track.col = scrollCol;
            track.rows = mBounds.rows;
            track.cols = 1;
            scrollbar.SetMetrics(track, static_cast<int>(mItems.size()), mBounds.rows, mTopIndex);
            mTopIndex = scrollbar.TopForRow(event.mouseRow);
            return true;
        }

        if ((event.mouseAction == MOUSE_ACTION_PRESS) && (leftButton == true))
        {
            int index = mTopIndex + (event.mouseRow - mBounds.row);

            if ((index >= 0) && (index < static_cast<int>(mItems.size())))
            {
                mSelectedIndex = index;
                EnsureVisible();
            }

            return true;
        }

        return false;
    }

    if (event.type != INPUT_TYPE_SPECIAL)
    {
        return false;
    }

    if (event.special == SPECIAL_KEY_ARROW_UP)
    {
        MoveSelection(-1);
        return true;
    }
    else if (event.special == SPECIAL_KEY_ARROW_DOWN)
    {
        MoveSelection(1);
        return true;
    }
    else if (event.special == SPECIAL_KEY_PAGE_UP)
    {
        MoveSelection(-mBounds.rows);
        return true;
    }
    else if (event.special == SPECIAL_KEY_PAGE_DOWN)
    {
        MoveSelection(mBounds.rows);
        return true;
    }
    else if (event.special == SPECIAL_KEY_HOME)
    {
        mSelectedIndex = 0;
        EnsureVisible();
        return true;
    }
    else if (event.special == SPECIAL_KEY_END)
    {
        if (mItems.empty() == false)
        {
            mSelectedIndex = static_cast<int>(mItems.size()) - 1;
            EnsureVisible();
        }
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because list boxes can receive focus
///
/// @brief
/// Report focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cListBox::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return selected item index
///
/// @brief
/// Get selected item index.
///
/////////////////////////////////////////////////////////////////////////////
int cListBox::GetSelectedIndex(void) const
{
    return mSelectedIndex;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return selected item text or empty string
///
/// @brief
/// Get selected item text.
///
/////////////////////////////////////////////////////////////////////////////
std::string cListBox::GetSelectedText(void) const
{
    if ((mSelectedIndex >= 0) && (mSelectedIndex < static_cast<int>(mItems.size())))
    {
        return mItems[static_cast<size_t>(mSelectedIndex)];
    }

    return std::string();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int index [in] selected item index
///
/// @return nothing
///
/// @brief
/// Set selected item index.
///
/////////////////////////////////////////////////////////////////////////////
void cListBox::SetSelectedIndex(int index)
{
    if ((index >= 0) && (index < static_cast<int>(mItems.size())))
    {
        mSelectedIndex = index;
        EnsureVisible();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int delta [in] selection movement amount
///
/// @return nothing
///
/// @brief
/// Move selected item by a signed delta.
///
/////////////////////////////////////////////////////////////////////////////
void cListBox::MoveSelection(int delta)
{
    if (mItems.empty() == true)
    {
        return;
    }

    mSelectedIndex += delta;

    if (mSelectedIndex < 0)
    {
        mSelectedIndex = 0;
    }

    if (mSelectedIndex >= static_cast<int>(mItems.size()))
    {
        mSelectedIndex = static_cast<int>(mItems.size()) - 1;
    }

    EnsureVisible();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scroll the list so the selected item is visible.
///
/////////////////////////////////////////////////////////////////////////////
void cListBox::EnsureVisible(void)
{
    if (mSelectedIndex < mTopIndex)
    {
        mTopIndex = mSelectedIndex;
    }

    if (mSelectedIndex >= (mTopIndex + mBounds.rows))
    {
        mTopIndex = mSelectedIndex - mBounds.rows + 1;
    }

    if (mTopIndex < 0)
    {
        mTopIndex = 0;
    }
}

}
