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

#include "scrolltextview.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cScrollTextView
///
/// @brief
/// Scrollable read-only text widget for help screens.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] view bounds
///
/// @return nothing
///
/// @brief
/// Construct an empty scroll text view.
///
/////////////////////////////////////////////////////////////////////////////
cScrollTextView::cScrollTextView(const sRect& bounds)
{
    mBounds = bounds;
    mTopLine = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<std::string>& lines [in] lines to show
///
/// @return nothing
///
/// @brief
/// Set the displayed text lines.
///
/////////////////////////////////////////////////////////////////////////////
void cScrollTextView::SetLines(const std::vector<std::string>& lines)
{
    mLines = lines;
    mTopLine = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw visible help text lines.
///
/////////////////////////////////////////////////////////////////////////////
void cScrollTextView::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle style = theme.GetStyle(THEME_ROLE_HELP);

    screen.FillRect(mBounds, " ", style);

    for (int row = 0; row < mBounds.rows; ++row)
    {
        int line = mTopLine + row;

        if (line >= static_cast<int>(mLines.size()))
        {
            break;
        }

        screen.PutText(mBounds.row + row, mBounds.col, mLines[static_cast<size_t>(line)], style);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when scrolling occurred
///
/// @brief
/// Handle text scrolling keys.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollTextView::HandleEvent(const sInputEvent& event)
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
            Scroll(event.mouseWheel);
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
        Scroll(-1);
        return true;
    }
    else if (event.special == SPECIAL_KEY_ARROW_DOWN)
    {
        Scroll(1);
        return true;
    }
    else if (event.special == SPECIAL_KEY_PAGE_UP)
    {
        Scroll(-mBounds.rows);
        return true;
    }
    else if (event.special == SPECIAL_KEY_PAGE_DOWN)
    {
        Scroll(mBounds.rows);
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because scroll text views can receive focus
///
/// @brief
/// Report focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollTextView::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int topLine [in] new top line index
///
/// @return nothing
///
/// @brief
/// Set top line index.
///
/////////////////////////////////////////////////////////////////////////////
void cScrollTextView::SetTopLine(int topLine)
{
    mTopLine = topLine;
    Scroll(0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return current top line index
///
/// @brief
/// Get top line index.
///
/////////////////////////////////////////////////////////////////////////////
int cScrollTextView::GetTopLine(void) const
{
    return mTopLine;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int amount [in] signed scroll amount
///
/// @return nothing
///
/// @brief
/// Scroll and clamp the top line.
///
/////////////////////////////////////////////////////////////////////////////
void cScrollTextView::Scroll(int amount)
{
    mTopLine += amount;

    if (mTopLine < 0)
    {
        mTopLine = 0;
    }

    int maxTop = static_cast<int>(mLines.size()) - mBounds.rows;

    if (maxTop < 0)
    {
        maxTop = 0;
    }

    if (mTopLine > maxTop)
    {
        mTopLine = maxTop;
    }
}

}
