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

#include "screen.h"

#include "utf8helper.h"

#include <algorithm>

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cScreen
///
/// @brief
/// Stores a portable UTF-8 terminal cell buffer and presents changed cells to
/// a platform terminal driver.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct an empty screen buffer.
///
/////////////////////////////////////////////////////////////////////////////
cScreen::cScreen(void)
{
    mRows = 0;
    mCols = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int rows [in] new screen rows
/// @param  int cols [in] new screen columns
///
/// @return nothing
///
/// @brief
/// Resize both front and back buffers and invalidate all cells.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::Resize(int rows, int cols)
{
    if (rows < 1)
    {
        rows = 1;
    }

    if (cols < 1)
    {
        cols = 1;
    }

    mRows = rows;
    mCols = cols;

    sStyle style;
    style.fg = MakePaletteColor(COLOR_LIGHT_GRAY);
    style.bg = MakePaletteColor(COLOR_BLACK);
    style.attrs = CELL_ATTR_NONE;

    sCell blank = MakeBlankCell(style);

    mBackBuffer.assign(static_cast<size_t>(rows * cols), blank);
    mFrontBuffer.assign(static_cast<size_t>(rows * cols), blank);

    Invalidate();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return number of screen rows
///
/// @brief
/// Get the current screen row count.
///
/////////////////////////////////////////////////////////////////////////////
int cScreen::GetRows(void) const
{
    return mRows;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return number of screen columns
///
/// @brief
/// Get the current screen column count.
///
/////////////////////////////////////////////////////////////////////////////
int cScreen::GetCols(void) const
{
    return mCols;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sStyle& style [in] style used to fill the screen
///
/// @return nothing
///
/// @brief
/// Clear the logical back buffer.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::Clear(const sStyle& style)
{
    sCell blank = MakeBlankCell(style);

    std::fill(mBackBuffer.begin(), mBackBuffer.end(), blank);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] zero-based row
/// @param  int col [in] zero-based column
/// @param  const std::string& graphemeUtf8 [in] UTF-8 grapheme cluster
/// @param  const sStyle& style [in] style for the cell
///
/// @return nothing
///
/// @brief
/// Put one grapheme cluster into the back buffer.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::PutCell(int row, int col, const std::string& graphemeUtf8, const sStyle& style)
{
    if (IsInsideClip(row, col) == false)
    {
        return;
    }

    int width = cUtf8Helper::GraphemeWidth(graphemeUtf8);

    if (width < 1)
    {
        width = 1;
    }

    if (width > 2)
    {
        width = 2;
    }

    if ((col + width) > mCols)
    {
        return;
    }

    sCell cell;
    cell.textUtf8 = graphemeUtf8;
    cell.style = style;
    cell.width = width;
    cell.wideTail = false;

    SetCell(row, col, cell);

    if (width == 2)
    {
        if (IsInsideClip(row, col + 1) == true)
        {
            sCell tail = MakeBlankCell(style);
            tail.width = 0;
            tail.wideTail = true;
            SetCell(row, col + 1, tail);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] zero-based row
/// @param  int col [in] zero-based column
/// @param  const std::string& textUtf8 [in] UTF-8 text
/// @param  const sStyle& style [in] style for the text
///
/// @return nothing
///
/// @brief
/// Put UTF-8 text into the back buffer after splitting it into graphemes.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::PutText(int row, int col, const std::string& textUtf8, const sStyle& style)
{
    std::vector<std::string> graphemes = cUtf8Helper::SplitGraphemes(textUtf8);
    int currentCol = col;

    for (const std::string& grapheme : graphemes)
    {
        int width = cUtf8Helper::GraphemeWidth(grapheme);

        if ((currentCol + width) > mCols)
        {
            break;
        }

        PutCell(row, currentCol, grapheme, style);
        currentCol += width;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& rect [in] rectangle to fill
/// @param  const std::string& graphemeUtf8 [in] fill grapheme
/// @param  const sStyle& style [in] style for the rectangle
///
/// @return nothing
///
/// @brief
/// Fill a rectangle with a single grapheme.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::FillRect(const sRect& rect, const std::string& graphemeUtf8, const sStyle& style)
{
    for (int row = rect.row; row < (rect.row + rect.rows); ++row)
    {
        for (int col = rect.col; col < (rect.col + rect.cols); ++col)
        {
            PutCell(row, col, graphemeUtf8, style);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& rect [in] box bounds
/// @param  const sStyle& style [in] style for box glyphs
///
/// @return nothing
///
/// @brief
/// Draw a Unicode box in the back buffer.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::DrawBox(const sRect& rect, const sStyle& style)
{
    if ((rect.rows < 2) || (rect.cols < 2))
    {
        return;
    }

    const int top = rect.row;
    const int bottom = rect.row + rect.rows - 1;
    const int left = rect.col;
    const int right = rect.col + rect.cols - 1;

    PutCell(top, left, "┌", style);
    PutCell(top, right, "┐", style);
    PutCell(bottom, left, "└", style);
    PutCell(bottom, right, "┘", style);

    for (int col = left + 1; col < right; ++col)
    {
        PutCell(top, col, "─", style);
        PutCell(bottom, col, "─", style);
    }

    for (int row = top + 1; row < bottom; ++row)
    {
        PutCell(row, left, "│", style);
        PutCell(row, right, "│", style);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& rect [in] clipping rectangle to push
///
/// @return nothing
///
/// @brief
/// Add a clipping rectangle to the clip stack.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::PushClip(const sRect& rect)
{
    sRect clipped = Intersect(CurrentClip(), rect);
    mClipStack.push_back(clipped);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Remove the current clipping rectangle.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::PopClip(void)
{
    if (mClipStack.empty() == false)
    {
        mClipStack.pop_back();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Force the next Present() to rewrite the entire screen.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::Invalidate(void)
{
    for (sCell& cell : mFrontBuffer)
    {
        cell.textUtf8 = "\x01";
        cell.width = 1;
        cell.wideTail = false;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cTerminalDriver& driver [in,out] terminal backend
///
/// @return nothing
///
/// @brief
/// Diff the back buffer against the front buffer and present changed runs.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::Present(cTerminalDriver& driver)
{
    std::vector<sCellRun> runs;

    for (int row = 0; row < mRows; ++row)
    {
        int col = 0;

        while (col < mCols)
        {
            const size_t offset = Index(row, col);

            if (CellsEqual(mBackBuffer[offset], mFrontBuffer[offset]) == true)
            {
                ++col;
                continue;
            }

            sCellRun run;
            run.row = row;
            run.col = col;

            while (col < mCols)
            {
                const size_t innerOffset = Index(row, col);

                if (CellsEqual(mBackBuffer[innerOffset], mFrontBuffer[innerOffset]) == true)
                {
                    break;
                }

                run.cells.push_back(mBackBuffer[innerOffset]);
                mFrontBuffer[innerOffset] = mBackBuffer[innerOffset];
                ++col;
            }

            runs.push_back(run);
        }
    }

    if (runs.empty() == false)
    {
        driver.Present(runs);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] zero-based row
/// @param  int col [in] zero-based column
///
/// @return vector index for the row and column
///
/// @brief
/// Convert row and column into a flat buffer index.
///
/////////////////////////////////////////////////////////////////////////////
size_t cScreen::Index(int row, int col) const
{
    return static_cast<size_t>((row * mCols) + col);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] zero-based row
/// @param  int col [in] zero-based column
///
/// @return true when the coordinate is drawable
///
/// @brief
/// Check screen bounds and the current clipping rectangle.
///
/////////////////////////////////////////////////////////////////////////////
bool cScreen::IsInsideClip(int row, int col) const
{
    if ((row < 0) || (col < 0) || (row >= mRows) || (col >= mCols))
    {
        return false;
    }

    sRect clip = CurrentClip();

    if (row < clip.row)
    {
        return false;
    }

    if (col < clip.col)
    {
        return false;
    }

    if (row >= (clip.row + clip.rows))
    {
        return false;
    }

    if (col >= (clip.col + clip.cols))
    {
        return false;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return active clipping rectangle
///
/// @brief
/// Return the current clipping rectangle or the whole screen if none is set.
///
/////////////////////////////////////////////////////////////////////////////
sRect cScreen::CurrentClip(void) const
{
    if (mClipStack.empty() == false)
    {
        return mClipStack.back();
    }

    sRect rect;
    rect.row = 0;
    rect.col = 0;
    rect.rows = mRows;
    rect.cols = mCols;

    return rect;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& first [in] first rectangle
/// @param  const sRect& second [in] second rectangle
///
/// @return intersection rectangle
///
/// @brief
/// Calculate the intersection of two rectangles.
///
/////////////////////////////////////////////////////////////////////////////
sRect cScreen::Intersect(const sRect& first, const sRect& second) const
{
    sRect result;

    const int top = std::max(first.row, second.row);
    const int left = std::max(first.col, second.col);
    const int bottom = std::min(first.row + first.rows, second.row + second.rows);
    const int right = std::min(first.col + first.cols, second.col + second.cols);

    result.row = top;
    result.col = left;
    result.rows = bottom - top;
    result.cols = right - left;

    if (result.rows < 0)
    {
        result.rows = 0;
    }

    if (result.cols < 0)
    {
        result.cols = 0;
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sStyle& style [in] style for the blank cell
///
/// @return blank cell using the requested style
///
/// @brief
/// Build a blank screen cell.
///
/////////////////////////////////////////////////////////////////////////////
sCell cScreen::MakeBlankCell(const sStyle& style) const
{
    sCell cell;

    cell.textUtf8 = " ";
    cell.style = style;
    cell.width = 1;
    cell.wideTail = false;

    return cell;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] zero-based row
/// @param  int col [in] zero-based column
/// @param  const sCell& cell [in] cell value to store
///
/// @return nothing
///
/// @brief
/// Store a cell in the back buffer after bounds checking.
///
/////////////////////////////////////////////////////////////////////////////
void cScreen::SetCell(int row, int col, const sCell& cell)
{
    if ((row < 0) || (col < 0) || (row >= mRows) || (col >= mCols))
    {
        return;
    }

    mBackBuffer[Index(row, col)] = cell;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sCell& first [in] first cell
/// @param  const sCell& second [in] second cell
///
/// @return true when the cells are identical
///
/// @brief
/// Compare all displayed fields in two cells.
///
/////////////////////////////////////////////////////////////////////////////
bool cScreen::CellsEqual(const sCell& first, const sCell& second) const
{
    if (first.textUtf8 != second.textUtf8)
    {
        return false;
    }

    if (first.style.fg != second.style.fg)
    {
        return false;
    }

    if (first.style.bg != second.style.bg)
    {
        return false;
    }

    if (first.style.attrs != second.style.attrs)
    {
        return false;
    }

    if (first.width != second.width)
    {
        return false;
    }

    if (first.wideTail != second.wideTail)
    {
        return false;
    }

    return true;
}

}
