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

#include "scrollbar.h"

namespace wordstartui
{

namespace
{
const char* SCROLLBAR_TRACK = "\xe2\x94\x82";   // U+2502 light vertical
const char* SCROLLBAR_THUMB = "\xe2\x96\x88";   // U+2588 full block
}

/////////////////////////////////////////////////////////////////////////////
///
/// @class cScrollBar
///
/// @brief
/// Reusable vertical scrollbar helper shared by list, dropdown, and editor.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct an empty scrollbar.
///
/////////////////////////////////////////////////////////////////////////////
cScrollBar::cScrollBar(void)
{
    mTrack.row = 0;
    mTrack.col = 0;
    mTrack.rows = 0;
    mTrack.cols = 1;
    mTotal = 0;
    mVisible = 0;
    mTop = 0;
    mArrows = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool arrows [in] true to draw clickable up/down arrows
///
/// @return nothing
///
/// @brief
/// Enable the up/down arrow buttons at the ends of the track.
///
/////////////////////////////////////////////////////////////////////////////
void cScrollBar::SetArrows(bool arrows)
{
    mArrows = arrows;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the first row of the thumb travel area (below the up arrow)
///
/// @brief
/// The thumb/track area, excluding the arrow rows when arrows are shown.
///
/////////////////////////////////////////////////////////////////////////////
int cScrollBar::InnerTop(void) const
{
    if (mArrows == true)
    {
        return mTrack.row + 1;
    }

    return mTrack.row;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the number of rows in the thumb travel area
///
/// @brief
/// Track rows minus the two arrow rows when arrows are shown.
///
/////////////////////////////////////////////////////////////////////////////
int cScrollBar::InnerRows(void) const
{
    if (mArrows == true)
    {
        int rows = mTrack.rows - 2;

        if (rows < 0)
        {
            rows = 0;
        }

        return rows;
    }

    return mTrack.rows;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& track [in] the scrollbar column area
/// @param  int total [in] total item count
/// @param  int visible [in] number of visible rows
/// @param  int top [in] index of the first visible item
///
/// @return nothing
///
/// @brief
/// Set the track geometry and scroll state.
///
/////////////////////////////////////////////////////////////////////////////
void cScrollBar::SetMetrics(const sRect& track, int total, int visible, int top)
{
    mTrack = track;
    mTotal = total;
    mVisible = visible;
    mTop = top;

    if (mTop < 0)
    {
        mTop = 0;
    }

    if (mTop > MaxTop())
    {
        mTop = MaxTop();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the largest valid top index
///
/// @brief
/// The maximum first-visible index (never negative).
///
/////////////////////////////////////////////////////////////////////////////
int cScrollBar::MaxTop(void) const
{
    int max = mTotal - mVisible;

    if (max < 0)
    {
        max = 0;
    }

    return max;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when there is more content than fits
///
/// @brief
/// Report whether a scrollbar is needed.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollBar::NeedsBar(void) const
{
    return (mTotal > mVisible) && (mTrack.rows > 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the thumb height in rows
///
/// @brief
/// A proportional thumb height, at least one row.
///
/////////////////////////////////////////////////////////////////////////////
int cScrollBar::ThumbHeight(void) const
{
    int inner = InnerRows();

    if (mTotal <= 0)
    {
        return inner;
    }

    int height = (mVisible * inner) / mTotal;

    if (height < 1)
    {
        height = 1;
    }

    if (height > inner)
    {
        height = inner;
    }

    return height;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the top row of the thumb
///
/// @brief
/// Position the thumb within the track from the current top index.
///
/////////////////////////////////////////////////////////////////////////////
int cScrollBar::ThumbRow(void) const
{
    int travel = InnerRows() - ThumbHeight();
    int range = MaxTop();

    if ((travel <= 0) || (range <= 0))
    {
        return InnerTop();
    }

    return InnerTop() + ((mTop * travel) / range);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const sStyle& trackStyle [in] style for the track
/// @param  const sStyle& thumbStyle [in] style for the thumb
///
/// @return nothing
///
/// @brief
/// Draw the track and a proportional thumb down the scrollbar column.
///
/////////////////////////////////////////////////////////////////////////////
void cScrollBar::Draw(cScreen& screen, const sStyle& trackStyle, const sStyle& thumbStyle) const
{
    if (mTrack.rows <= 0)
    {
        return;
    }

    if (mArrows == true)
    {
        screen.PutCell(mTrack.row, mTrack.col, "\xe2\x96\xb2", thumbStyle);                       // up arrow
        screen.PutCell(mTrack.row + mTrack.rows - 1, mTrack.col, "\xe2\x96\xbc", thumbStyle);     // down arrow
    }

    int thumbTop = ThumbRow();
    int thumbBottom = thumbTop + ThumbHeight();
    int innerTop = InnerTop();
    int innerRows = InnerRows();

    for (int row = 0; row < innerRows; ++row)
    {
        int screenRow = innerTop + row;

        if ((screenRow >= thumbTop) && (screenRow < thumbBottom))
        {
            screen.PutCell(screenRow, mTrack.col, SCROLLBAR_THUMB, thumbStyle);
        }
        else
        {
            screen.PutCell(screenRow, mTrack.col, SCROLLBAR_TRACK, trackStyle);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
///
/// @return true when the row is the up-arrow button
///
/// @brief
/// Hit test for the top arrow.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollBar::IsUpArrow(int row) const
{
    return (mArrows == true) && (row == mTrack.row);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
///
/// @return true when the row is the down-arrow button
///
/// @brief
/// Hit test for the bottom arrow.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollBar::IsDownArrow(int row) const
{
    return (mArrows == true) && (row == (mTrack.row + mTrack.rows - 1));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
///
/// @return true when the row is on the track above the thumb
///
/// @brief
/// Hit test for a page-up click: on the track, above the thumb, excluding the
/// up arrow.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollBar::IsAboveThumb(int row) const
{
    return (row >= InnerTop()) && (row < ThumbRow());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
///
/// @return true when the row is on the track below the thumb
///
/// @brief
/// Hit test for a page-down click: on the track, below the thumb, excluding the
/// down arrow.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollBar::IsBelowThumb(int row) const
{
    return (row >= (ThumbRow() + ThumbHeight())) &&
           (row < (InnerTop() + InnerRows()));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return true when the point is on the scrollbar column
///
/// @brief
/// Hit test for routing clicks and drags to the scrollbar.
///
/////////////////////////////////////////////////////////////////////////////
bool cScrollBar::ContainsPoint(int row, int col) const
{
    if (col != mTrack.col)
    {
        return false;
    }

    if (row < mTrack.row)
    {
        return false;
    }

    if (row >= (mTrack.row + mTrack.rows))
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int mouseRow [in] the clicked or dragged screen row
///
/// @return the top index that positions the scroll at that row
///
/// @brief
/// Map a mouse row on the track to a first-visible index (clamped).
///
/////////////////////////////////////////////////////////////////////////////
int cScrollBar::TopForRow(int mouseRow) const
{
    int range = MaxTop();
    int inner = InnerRows();

    if ((range <= 0) || (inner <= 1))
    {
        return 0;
    }

    int rel = mouseRow - InnerTop();

    if (rel < 0)
    {
        rel = 0;
    }

    if (rel > (inner - 1))
    {
        rel = inner - 1;
    }

    int top = (rel * range) / (inner - 1);

    if (top < 0)
    {
        top = 0;
    }

    if (top > range)
    {
        top = range;
    }

    return top;
}

}
