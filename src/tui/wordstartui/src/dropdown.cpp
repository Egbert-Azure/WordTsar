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

#include "dropdown.h"
#include "scrollbar.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cDropdown
///
/// @brief
/// A single-line control that shows the current selection and expands into an
/// overlay list of choices when opened. The open list is drawn directly below
/// the control, so callers should leave room beneath it (or draw the focused
/// dropdown last so its list overlays neighbouring widgets).
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] the closed control area (row is the box)
///
/// @return nothing
///
/// @brief
/// Construct a closed, empty dropdown.
///
/////////////////////////////////////////////////////////////////////////////
cDropdown::cDropdown(const sRect& bounds)
{
    mBounds = bounds;
    mSelectedIndex = -1;
    mHighlightIndex = 0;
    mTopIndex = 0;
    mOpen = false;
    mOpenUp = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<std::string>& items [in] choices
///
/// @return nothing
///
/// @brief
/// Replace the list of choices and select the first when none was selected.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::SetItems(const std::vector<std::string>& items)
{
    mItems = items;

    if ((mSelectedIndex < 0) && (mItems.empty() == false))
    {
        mSelectedIndex = 0;
    }

    if (mSelectedIndex >= static_cast<int>(mItems.size()))
    {
        mSelectedIndex = static_cast<int>(mItems.size()) - 1;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the closed control, plus the overlay list when open.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle field = theme.GetStyle(THEME_ROLE_FIELD);

    if (HasFocus() == true)
    {
        field = theme.GetStyle(THEME_ROLE_FIELD_FOCUS);
    }

    sStyle list = theme.GetStyle(THEME_ROLE_LIST);
    sStyle selected = theme.GetStyle(THEME_ROLE_LIST_SELECTED);

    // Closed control: one row with the selected text and a state arrow.
    sRect boxRect;
    boxRect.row = mBounds.row;
    boxRect.col = mBounds.col;
    boxRect.rows = 1;
    boxRect.cols = mBounds.cols;
    screen.FillRect(boxRect, " ", field);

    std::string label;
    if ((mSelectedIndex >= 0) && (mSelectedIndex < static_cast<int>(mItems.size())))
    {
        label = mItems[static_cast<size_t>(mSelectedIndex)];
    }
    screen.PutText(mBounds.row, mBounds.col, label, field);

    if (mBounds.cols >= 1)
    {
        std::string arrow = "\xe2\x96\xbc";   // down triangle

        if (mOpen == true)
        {
            arrow = "\xe2\x96\xb2";   // up triangle
        }

        screen.PutCell(mBounds.row, mBounds.col + mBounds.cols - 1, arrow, field);
    }

    if (mOpen == false)
    {
        return;
    }

    // Open overlay list, drawn directly below (or above) the control with a
    // side-and-bottom border so the list stands out from the background. The
    // control's own row acts as the fourth (top) edge.
    int rows = VisibleRows();
    int leftCol = mBounds.col;
    int rightCol = mBounds.col + mBounds.cols - 1;
    int contentCol = leftCol + 1;
    int contentCols = mBounds.cols - 2;
    if (contentCols < 0)
    {
        contentCols = 0;
    }

    for (int row = 0; row < rows; ++row)
    {
        int itemIndex = mTopIndex + row;

        if (itemIndex >= static_cast<int>(mItems.size()))
        {
            break;
        }

        sStyle style = list;

        if (itemIndex == mHighlightIndex)
        {
            style = selected;
        }

        int screenRow = OverlayTop() + row;

        sRect lineRect;
        lineRect.row = screenRow;
        lineRect.col = contentCol;
        lineRect.rows = 1;
        lineRect.cols = contentCols;
        screen.FillRect(lineRect, " ", style);
        screen.PutText(screenRow, contentCol, mItems[static_cast<size_t>(itemIndex)], style);

        // Side borders drawn last so long item text cannot overwrite them.
        screen.PutCell(screenRow, leftCol, "\xe2\x94\x82", list);    // vertical bar
        screen.PutCell(screenRow, rightCol, "\xe2\x94\x82", list);   // vertical bar
    }

    // Closing edge: a bottom border below the list, or a top border above it
    // when the list opens upward.
    int borderRow = BorderRow();
    for (int c = leftCol + 1; c < rightCol; ++c)
    {
        screen.PutCell(borderRow, c, "\xe2\x94\x80", list);          // horizontal bar
    }

    if (mOpenUp == true)
    {
        screen.PutCell(borderRow, leftCol, "\xe2\x94\x8c", list);    // top-left corner
        screen.PutCell(borderRow, rightCol, "\xe2\x94\x90", list);   // top-right corner
    }
    else
    {
        screen.PutCell(borderRow, leftCol, "\xe2\x94\x94", list);    // bottom-left corner
        screen.PutCell(borderRow, rightCol, "\xe2\x94\x98", list);   // bottom-right corner
    }

    // Scrollbar when the list is taller than the visible window. It sits just
    // inside the right border.
    if (static_cast<int>(mItems.size()) > rows)
    {
        cScrollBar scrollbar;
        sRect track;
        track.row = OverlayTop();
        track.col = rightCol - 1;
        track.rows = rows;
        track.cols = 1;
        scrollbar.SetMetrics(track, static_cast<int>(mItems.size()), rows, mTopIndex);
        scrollbar.Draw(screen, list, selected);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true if the event was consumed
///
/// @brief
/// Toggle open/closed, move the highlight, and commit a selection.
///
/////////////////////////////////////////////////////////////////////////////
bool cDropdown::HandleEvent(const sInputEvent& event)
{
    if (mEnabled == false)
    {
        return false;
    }

    if (event.type == INPUT_TYPE_MOUSE)
    {
        if ((event.mouseAction == MOUSE_ACTION_WHEEL) && (mOpen == true))
        {
            MoveHighlight(event.mouseWheel);
            return true;
        }

        int scrollCol = mBounds.col + mBounds.cols - 2;
        bool pressOrDrag = (event.mouseAction == MOUSE_ACTION_PRESS) ||
                           (event.mouseAction == MOUSE_ACTION_DRAG);

        // Press or drag on the overlay scrollbar column scrolls the open list.
        if ((mOpen == true) && (pressOrDrag == true) && (event.mouseButton == MOUSE_BUTTON_LEFT) &&
            (event.mouseCol == scrollCol) && (event.mouseRow >= OverlayTop()) &&
            (static_cast<int>(mItems.size()) > VisibleRows()))
        {
            int rows = VisibleRows();
            cScrollBar scrollbar;
            sRect track;
            track.row = OverlayTop();
            track.col = scrollCol;
            track.rows = rows;
            track.cols = 1;
            scrollbar.SetMetrics(track, static_cast<int>(mItems.size()), rows, mTopIndex);
            mTopIndex = scrollbar.TopForRow(event.mouseRow);

            if (mHighlightIndex < mTopIndex)
            {
                mHighlightIndex = mTopIndex;
            }
            if (mHighlightIndex > (mTopIndex + rows - 1))
            {
                mHighlightIndex = mTopIndex + rows - 1;
            }

            return true;
        }

        if ((event.mouseAction == MOUSE_ACTION_PRESS) && (event.mouseButton == MOUSE_BUTTON_LEFT))
        {
            bool onControl = (event.mouseRow == mBounds.row) &&
                             (event.mouseCol >= mBounds.col) &&
                             (event.mouseCol < (mBounds.col + mBounds.cols));

            if (onControl == true)
            {
                if (mOpen == true)
                {
                    Close();
                }
                else
                {
                    Open();
                }
                return true;
            }

            if (mOpen == true)
            {
                int rows = VisibleRows();
                int row = event.mouseRow - OverlayTop();
                bool inList = (row >= 0) && (row < rows) &&
                              (event.mouseCol >= mBounds.col) &&
                              (event.mouseCol < (mBounds.col + mBounds.cols));

                if (inList == true)
                {
                    int itemIndex = mTopIndex + row;

                    if ((itemIndex >= 0) && (itemIndex < static_cast<int>(mItems.size())))
                    {
                        mSelectedIndex = itemIndex;
                    }

                    Close();
                    return true;
                }

                // A click on the border frame is claimed without dismissing.
                if (ContainsEventPoint(event.mouseRow, event.mouseCol) == true)
                {
                    return true;
                }

                // A click elsewhere dismisses the open list.
                Close();
            }
        }

        return false;
    }

    if (mOpen == true)
    {
        if (event.type == INPUT_TYPE_SPECIAL)
        {
            if (event.special == SPECIAL_KEY_ARROW_UP)
            {
                MoveHighlight(-1);
                return true;
            }
            else if (event.special == SPECIAL_KEY_ARROW_DOWN)
            {
                MoveHighlight(1);
                return true;
            }
            else if (event.special == SPECIAL_KEY_PAGE_UP)
            {
                MoveHighlight(-VisibleRows());
                return true;
            }
            else if (event.special == SPECIAL_KEY_PAGE_DOWN)
            {
                MoveHighlight(VisibleRows());
                return true;
            }
            else if (event.special == SPECIAL_KEY_HOME)
            {
                mHighlightIndex = 0;
                EnsureVisible();
                return true;
            }
            else if (event.special == SPECIAL_KEY_END)
            {
                if (mItems.empty() == false)
                {
                    mHighlightIndex = static_cast<int>(mItems.size()) - 1;
                    EnsureVisible();
                }
                return true;
            }
            else if (event.special == SPECIAL_KEY_ENTER)
            {
                mSelectedIndex = mHighlightIndex;
                Close();
                return true;
            }
            else if (event.special == SPECIAL_KEY_ESCAPE)
            {
                Close();
                return true;
            }
        }

        return false;
    }

    // Closed: Enter, Down arrow, or Space opens the list.
    if (event.type == INPUT_TYPE_SPECIAL)
    {
        if ((event.special == SPECIAL_KEY_ENTER) || (event.special == SPECIAL_KEY_ARROW_DOWN))
        {
            Open();
            return true;
        }
    }

    if ((event.type == INPUT_TYPE_TEXT) && (event.textUtf8 == " "))
    {
        Open();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true (a dropdown is always focusable)
///
/// @brief
/// Report that the dropdown accepts keyboard focus.
///
/////////////////////////////////////////////////////////////////////////////
bool cDropdown::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return true when the position is on the box or the open overlay list
///
/// @brief
/// Claim the closed box and, when open, the overlay list rows drawn below it,
/// so the host routes clicks on the expanded list to this widget.
///
/////////////////////////////////////////////////////////////////////////////
bool cDropdown::ContainsEventPoint(int row, int col) const
{
    if (ContainsPoint(row, col) == true)
    {
        return true;
    }

    if (mOpen == false)
    {
        return false;
    }

    int rows = VisibleRows();
    int regionTop = OverlayTop();
    if (mOpenUp == true)
    {
        regionTop = OverlayTop() - 1;   // top border sits above the items
    }
    int regionRows = rows + 1;          // items plus the closing border row

    bool inOverlay = (row >= regionTop) && (row < (regionTop + regionRows)) &&
                     (col >= mBounds.col) && (col < (mBounds.col + mBounds.cols));

    return inOverlay;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the selected item index, or -1 when empty
///
/// @brief
/// Get the current selection index.
///
/////////////////////////////////////////////////////////////////////////////
int cDropdown::GetSelectedIndex(void) const
{
    return mSelectedIndex;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the selected item text, or an empty string when none
///
/// @brief
/// Get the current selection text.
///
/////////////////////////////////////////////////////////////////////////////
std::string cDropdown::GetSelectedText(void) const
{
    if ((mSelectedIndex >= 0) && (mSelectedIndex < static_cast<int>(mItems.size())))
    {
        return mItems[static_cast<size_t>(mSelectedIndex)];
    }

    return std::string();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int index [in] new selection index
///
/// @return nothing
///
/// @brief
/// Set the current selection, clamped to the valid range.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::SetSelectedIndex(int index)
{
    if (mItems.empty() == true)
    {
        mSelectedIndex = -1;
        return;
    }

    if (index < 0)
    {
        index = 0;
    }

    if (index >= static_cast<int>(mItems.size()))
    {
        index = static_cast<int>(mItems.size()) - 1;
    }

    mSelectedIndex = index;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the overlay list is showing
///
/// @brief
/// Report whether the dropdown is currently expanded.
///
/////////////////////////////////////////////////////////////////////////////
bool cDropdown::IsOpen(void) const
{
    return mOpen;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the list is expanded
///
/// @brief
/// An open dropdown paints its list past its bounds, so the host must draw it
/// after neighbouring controls.
///
/////////////////////////////////////////////////////////////////////////////
bool cDropdown::HasOpenOverlay(void) const
{
    return mOpen;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool up [in] true to open the overlay above the control
///
/// @return nothing
///
/// @brief
/// Choose whether the open list drops down or rises up.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::SetOpenUpward(bool up)
{
    mOpenUp = up;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the first screen row of the open overlay list
///
/// @brief
/// The overlay sits below the control by default, or above it when opening
/// upward.
///
/////////////////////////////////////////////////////////////////////////////
int cDropdown::OverlayTop(void) const
{
    if (mOpenUp == true)
    {
        return mBounds.row - VisibleRows();
    }

    return mBounds.row + 1;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the screen row of the list's closing border
///
/// @brief
/// The border sits below the list when it drops down, or above the list when
/// it opens upward.
///
/////////////////////////////////////////////////////////////////////////////
int cDropdown::BorderRow(void) const
{
    if (mOpenUp == true)
    {
        return OverlayTop() - 1;
    }

    return OverlayTop() + VisibleRows();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Expand the list and highlight the current selection.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::Open(void)
{
    if (mItems.empty() == true)
    {
        return;
    }

    mOpen = true;

    if ((mSelectedIndex >= 0) && (mSelectedIndex < static_cast<int>(mItems.size())))
    {
        mHighlightIndex = mSelectedIndex;
    }
    else
    {
        mHighlightIndex = 0;
    }

    EnsureVisible();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Collapse the list.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::Close(void)
{
    mOpen = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int delta [in] highlight movement (signed)
///
/// @return nothing
///
/// @brief
/// Move the highlighted row within the open list.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::MoveHighlight(int delta)
{
    if (mItems.empty() == true)
    {
        return;
    }

    mHighlightIndex += delta;

    if (mHighlightIndex < 0)
    {
        mHighlightIndex = 0;
    }

    if (mHighlightIndex >= static_cast<int>(mItems.size()))
    {
        mHighlightIndex = static_cast<int>(mItems.size()) - 1;
    }

    EnsureVisible();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scroll the open list so the highlighted row is visible.
///
/////////////////////////////////////////////////////////////////////////////
void cDropdown::EnsureVisible(void)
{
    int rows = VisibleRows();

    if (mHighlightIndex < mTopIndex)
    {
        mTopIndex = mHighlightIndex;
    }

    if (mHighlightIndex >= (mTopIndex + rows))
    {
        mTopIndex = mHighlightIndex - rows + 1;
    }

    if (mTopIndex < 0)
    {
        mTopIndex = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the number of list rows drawn when open (capped)
///
/// @brief
/// Compute the open list height, capped so a long list does not fill the
/// whole screen.
///
/////////////////////////////////////////////////////////////////////////////
int cDropdown::VisibleRows(void) const
{
    int cap = 8;
    int count = static_cast<int>(mItems.size());

    if (count < cap)
    {
        return count;
    }

    return cap;
}

}
