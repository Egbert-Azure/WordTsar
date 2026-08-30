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

#include "documentview.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cDocumentView
///
/// @brief
/// Minimal scrollable document pane demonstrating WordStar key routing.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct an empty document view.
///
/////////////////////////////////////////////////////////////////////////////
cDocumentView::cDocumentView(void)
{
    mBounds.row = 0;
    mBounds.col = 0;
    mBounds.rows = 0;
    mBounds.cols = 0;
    mCursorLine = 0;
    mCursorCol = 0;
    mTopLine = 0;
    mLeftCol = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] document pane bounds
///
/// @return nothing
///
/// @brief
/// Set document pane bounds.
///
/////////////////////////////////////////////////////////////////////////////
void cDocumentView::SetBounds(const sRect& bounds)
{
    mBounds = bounds;
    EnsureVisible();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<std::string>& lines [in] document lines
///
/// @return nothing
///
/// @brief
/// Replace displayed document lines.
///
/////////////////////////////////////////////////////////////////////////////
void cDocumentView::SetLines(const std::vector<std::string>& lines)
{
    mLines = lines;
    mCursorLine = 0;
    mCursorCol = 0;
    mTopLine = 0;
    mLeftCol = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw visible document lines.
///
/////////////////////////////////////////////////////////////////////////////
void cDocumentView::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle editor = theme.GetStyle(THEME_ROLE_EDITOR);

    screen.FillRect(mBounds, " ", editor);

    for (int row = 0; row < mBounds.rows; ++row)
    {
        int lineIndex = mTopLine + row;

        if (lineIndex >= static_cast<int>(mLines.size()))
        {
            break;
        }

        std::string visible = mLines[static_cast<size_t>(lineIndex)];
        screen.PutText(mBounds.row + row, mBounds.col, visible, editor);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the document view handled the event
///
/// @brief
/// Handle arrows and classic WordStar diamond control keys.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocumentView::HandleEvent(const sInputEvent& event)
{
    if (event.type == INPUT_TYPE_MOUSE)
    {
        if (ContainsPoint(event.mouseRow, event.mouseCol) == false)
        {
            return false;
        }

        if ((event.mouseAction == MOUSE_ACTION_PRESS) && (event.mouseButton == MOUSE_BUTTON_LEFT))
        {
            SetCursorFromPoint(event.mouseRow, event.mouseCol);
            return true;
        }

        if (event.mouseAction == MOUSE_ACTION_WHEEL)
        {
            MoveCursor(event.mouseWheel, 0);
            return true;
        }

        return false;
    }

    if (event.type == INPUT_TYPE_SPECIAL)
    {
        if (event.special == SPECIAL_KEY_ARROW_UP)
        {
            MoveCursor(-1, 0);
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_DOWN)
        {
            MoveCursor(1, 0);
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_LEFT)
        {
            MoveCursor(0, -1);
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_RIGHT)
        {
            MoveCursor(0, 1);
            return true;
        }
    }

    if (event.type == INPUT_TYPE_CONTROL)
    {
        if (event.controlCode == 5)
        {
            MoveCursor(-1, 0);
            return true;
        }
        else if (event.controlCode == 24)
        {
            MoveCursor(1, 0);
            return true;
        }
        else if (event.controlCode == 19)
        {
            MoveCursor(0, -1);
            return true;
        }
        else if (event.controlCode == 4)
        {
            MoveCursor(0, 1);
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return visible cursor row
///
/// @brief
/// Get cursor row relative to the terminal screen.
///
/////////////////////////////////////////////////////////////////////////////
int cDocumentView::GetCursorRow(void) const
{
    return mBounds.row + (mCursorLine - mTopLine);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return visible cursor column
///
/// @brief
/// Get cursor column relative to the terminal screen.
///
/////////////////////////////////////////////////////////////////////////////
int cDocumentView::GetCursorCol(void) const
{
    return mBounds.col + (mCursorCol - mLeftCol);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int rowDelta [in] row movement
/// @param  int colDelta [in] column movement
///
/// @return nothing
///
/// @brief
/// Move and clamp the document cursor.
///
/////////////////////////////////////////////////////////////////////////////
void cDocumentView::MoveCursor(int rowDelta, int colDelta)
{
    mCursorLine += rowDelta;
    mCursorCol += colDelta;

    if (mCursorLine < 0)
    {
        mCursorLine = 0;
    }

    if (mCursorLine >= static_cast<int>(mLines.size()))
    {
        mCursorLine = static_cast<int>(mLines.size()) - 1;
    }

    if (mCursorLine < 0)
    {
        mCursorLine = 0;
    }

    if (mCursorCol < 0)
    {
        mCursorCol = 0;
    }

    EnsureVisible();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return nothing
///
/// @brief
/// Move the document cursor to a clicked point in the document pane.
///
/////////////////////////////////////////////////////////////////////////////
void cDocumentView::SetCursorFromPoint(int row, int col)
{
    mCursorLine = mTopLine + (row - mBounds.row);
    mCursorCol = mLeftCol + (col - mBounds.col);

    if (mCursorLine < 0)
    {
        mCursorLine = 0;
    }

    if (mCursorLine >= static_cast<int>(mLines.size()))
    {
        mCursorLine = static_cast<int>(mLines.size()) - 1;
    }

    if (mCursorLine < 0)
    {
        mCursorLine = 0;
    }

    if (mCursorCol < 0)
    {
        mCursorCol = 0;
    }

    EnsureVisible();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return true when the position is inside the document pane
///
/// @brief
/// Test whether a point is inside the document view.
///
/////////////////////////////////////////////////////////////////////////////
bool cDocumentView::ContainsPoint(int row, int col) const
{
    if (row < mBounds.row)
    {
        return false;
    }

    if (row >= (mBounds.row + mBounds.rows))
    {
        return false;
    }

    if (col < mBounds.col)
    {
        return false;
    }

    if (col >= (mBounds.col + mBounds.cols))
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scroll the document pane so the cursor remains visible.
///
/////////////////////////////////////////////////////////////////////////////
void cDocumentView::EnsureVisible(void)
{
    if (mCursorLine < mTopLine)
    {
        mTopLine = mCursorLine;
    }

    if (mCursorLine >= (mTopLine + mBounds.rows))
    {
        mTopLine = mCursorLine - mBounds.rows + 1;
    }

    if (mCursorCol < mLeftCol)
    {
        mLeftCol = mCursorCol;
    }

    if (mCursorCol >= (mLeftCol + mBounds.cols))
    {
        mLeftCol = mCursorCol - mBounds.cols + 1;
    }

    if (mTopLine < 0)
    {
        mTopLine = 0;
    }

    if (mLeftCol < 0)
    {
        mLeftCol = 0;
    }
}

}
