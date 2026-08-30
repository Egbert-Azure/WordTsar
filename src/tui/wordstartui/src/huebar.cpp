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

#include "huebar.h"
#include "colorutils.h"
#include "glyphs.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cHueBar
///
/// @brief
/// A vertical hue slider running 0 degrees at the top to 360 degrees at the
/// bottom. Each cell paints two vertical hue samples with the upper-half block
/// glyph.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  double hue [in] a hue value
///
/// @return the hue wrapped into the range 0 to 360
///
/// @brief
/// Constrain a hue to a single turn.
///
/////////////////////////////////////////////////////////////////////////////
static double ClampHue(double hue)
{
    if (hue < 0.0)
    {
        return 0.0;
    }
    if (hue > 360.0)
    {
        return 360.0;
    }
    return hue;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] widget bounds
///
/// @return nothing
///
/// @brief
/// Construct the hue bar.
///
/////////////////////////////////////////////////////////////////////////////
cHueBar::cHueBar(const sRect& bounds)
{
    mBounds = bounds;
    mHue = 0.0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme (unused)
///
/// @return nothing
///
/// @brief
/// Paint the hue gradient and the current-hue marker.
///
/////////////////////////////////////////////////////////////////////////////
void cHueBar::Draw(cScreen& screen, const cTheme& theme)
{
    (void)theme;

    int rows = mBounds.rows;
    int cols = mBounds.cols;

    if ((rows <= 0) || (cols <= 0))
    {
        return;
    }

    int totalSamples = rows * 2;

    for (int r = 0; r < rows; ++r)
    {
        int topSample = 2 * r;
        int bottomSample = (2 * r) + 1;

        double topHue = (static_cast<double>(topSample) / static_cast<double>(totalSamples - 1)) * 360.0;
        double bottomHue = (static_cast<double>(bottomSample) / static_cast<double>(totalSamples - 1)) * 360.0;

        sHsv topHsv;
        topHsv.h = topHue;
        topHsv.s = 1.0;
        topHsv.v = 1.0;

        sHsv bottomHsv;
        bottomHsv.h = bottomHue;
        bottomHsv.s = 1.0;
        bottomHsv.v = 1.0;

        sStyle style;
        style.fg = HsvToRgb(topHsv);
        style.bg = HsvToRgb(bottomHsv);
        style.attrs = CELL_ATTR_NONE;

        for (int c = 0; c < cols; ++c)
        {
            screen.PutCell(mBounds.row + r, mBounds.col + c, GLYPH_UPPER_HALF, style);
        }
    }

    int markerRow = 0;
    if (rows > 1)
    {
        markerRow = static_cast<int>(((mHue / 360.0) * static_cast<double>(rows - 1)) + 0.5);
    }

    sStyle markerStyle;
    markerStyle.fg = MakeRgb(255, 255, 255);
    markerStyle.bg = MakeRgb(0, 0, 0);
    markerStyle.attrs = CELL_ATTR_NONE;

    screen.PutText(mBounds.row + markerRow, mBounds.col, "<", markerStyle);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event changed the hue
///
/// @brief
/// Change the hue with the mouse or the up/down arrow keys.
///
/////////////////////////////////////////////////////////////////////////////
bool cHueBar::HandleEvent(const sInputEvent& event)
{
    if (mEnabled == false)
    {
        return false;
    }

    if ((event.type == INPUT_TYPE_MOUSE) && (event.mouseButton == MOUSE_BUTTON_LEFT))
    {
        if ((event.mouseAction == MOUSE_ACTION_PRESS) || (event.mouseAction == MOUSE_ACTION_DRAG))
        {
            if (ContainsPoint(event.mouseRow, event.mouseCol) == true)
            {
                UpdateFromMouse(event);
                return true;
            }
        }
    }

    if (event.type == INPUT_TYPE_SPECIAL)
    {
        double step = 6.0;

        bool handled = false;

        if (event.special == SPECIAL_KEY_ARROW_UP)
        {
            mHue = ClampHue(mHue - step);
            handled = true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_DOWN)
        {
            mHue = ClampHue(mHue + step);
            handled = true;
        }

        if (handled == true)
        {
            if (mOnChange)
            {
                mOnChange();
            }
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because the hue bar can receive focus
///
/// @brief
/// Report focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cHueBar::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  double hue [in] the new hue (0-360)
///
/// @return nothing
///
/// @brief
/// Set the current hue.
///
/////////////////////////////////////////////////////////////////////////////
void cHueBar::SetHue(double hue)
{
    mHue = ClampHue(hue);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the current hue (0-360)
///
/// @brief
/// Get the current hue.
///
/////////////////////////////////////////////////////////////////////////////
double cHueBar::GetHue(void) const
{
    return mHue;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::function<void(void)> callback [in] change notification
///
/// @return nothing
///
/// @brief
/// Set the callback invoked when the hue changes.
///
/////////////////////////////////////////////////////////////////////////////
void cHueBar::SetOnChange(std::function<void(void)> callback)
{
    mOnChange = callback;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] mouse event inside the widget
///
/// @return nothing
///
/// @brief
/// Update the hue from a mouse position and notify listeners.
///
/////////////////////////////////////////////////////////////////////////////
void cHueBar::UpdateFromMouse(const sInputEvent& event)
{
    if (mBounds.rows > 1)
    {
        double hue = (static_cast<double>(event.mouseRow - mBounds.row) / static_cast<double>(mBounds.rows - 1)) * 360.0;
        mHue = ClampHue(hue);
    }

    if (mOnChange)
    {
        mOnChange();
    }
}

}
