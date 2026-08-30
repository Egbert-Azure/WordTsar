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

#include "colorfield2d.h"
#include "colorutils.h"
#include "glyphs.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cColorField2D
///
/// @brief
/// A saturation/value square for a fixed hue. Columns sweep saturation left to
/// right; rows sweep value top (bright) to bottom (dark). Each character cell
/// paints two vertical samples using the upper-half block glyph, doubling the
/// vertical resolution.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  double value [in] the value to clamp
///
/// @return the value constrained to the range 0-1
///
/// @brief
/// Clamp a normalized color component to the unit range.
///
/////////////////////////////////////////////////////////////////////////////
static double ClampUnit(double value)
{
    if (value < 0.0)
    {
        return 0.0;
    }
    if (value > 1.0)
    {
        return 1.0;
    }
    return value;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] widget bounds
///
/// @return nothing
///
/// @brief
/// Construct the saturation/value square.
///
/////////////////////////////////////////////////////////////////////////////
cColorField2D::cColorField2D(const sRect& bounds)
{
    mBounds = bounds;
    mHue = 0.0;
    mSaturation = 1.0;
    mValue = 1.0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme (unused)
///
/// @return nothing
///
/// @brief
/// Paint the saturation/value gradient and the selection crosshair.
///
/////////////////////////////////////////////////////////////////////////////
void cColorField2D::Draw(cScreen& screen, const cTheme& theme)
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

        double topValue = 1.0 - (static_cast<double>(topSample) / static_cast<double>(totalSamples - 1));
        double bottomValue = 1.0 - (static_cast<double>(bottomSample) / static_cast<double>(totalSamples - 1));

        for (int c = 0; c < cols; ++c)
        {
            double saturation = 0.0;
            if (cols > 1)
            {
                saturation = static_cast<double>(c) / static_cast<double>(cols - 1);
            }

            sHsv topHsv;
            topHsv.h = mHue;
            topHsv.s = saturation;
            topHsv.v = topValue;

            sHsv bottomHsv;
            bottomHsv.h = mHue;
            bottomHsv.s = saturation;
            bottomHsv.v = bottomValue;

            sStyle style;
            style.fg = HsvToRgb(topHsv);
            style.bg = HsvToRgb(bottomHsv);
            style.attrs = CELL_ATTR_NONE;

            screen.PutCell(mBounds.row + r, mBounds.col + c, GLYPH_UPPER_HALF, style);
        }
    }

    int markerCol = 0;
    if (cols > 1)
    {
        markerCol = static_cast<int>((mSaturation * static_cast<double>(cols - 1)) + 0.5);
    }

    int markerRow = 0;
    if (rows > 1)
    {
        markerRow = static_cast<int>(((1.0 - mValue) * static_cast<double>(rows - 1)) + 0.5);
    }

    sHsv selectedHsv;
    selectedHsv.h = mHue;
    selectedHsv.s = mSaturation;
    selectedHsv.v = mValue;

    sStyle markerStyle;
    markerStyle.bg = HsvToRgb(selectedHsv);
    markerStyle.attrs = CELL_ATTR_NONE;
    if (mValue > 0.5)
    {
        markerStyle.fg = MakeRgb(0, 0, 0);
    }
    else
    {
        markerStyle.fg = MakeRgb(255, 255, 255);
    }

    screen.PutCell(mBounds.row + markerRow, mBounds.col + markerCol, "+", markerStyle);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event moved the selection
///
/// @brief
/// Move the selection with the mouse or the arrow keys.
///
/////////////////////////////////////////////////////////////////////////////
bool cColorField2D::HandleEvent(const sInputEvent& event)
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
        double satStep = 0.02;
        if (mBounds.cols > 1)
        {
            satStep = 1.0 / static_cast<double>(mBounds.cols - 1);
        }

        double valStep = 0.02;
        if (mBounds.rows > 1)
        {
            valStep = 1.0 / static_cast<double>(mBounds.rows - 1);
        }

        bool handled = false;

        if (event.special == SPECIAL_KEY_ARROW_LEFT)
        {
            mSaturation = ClampUnit(mSaturation - satStep);
            handled = true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_RIGHT)
        {
            mSaturation = ClampUnit(mSaturation + satStep);
            handled = true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_UP)
        {
            mValue = ClampUnit(mValue + valStep);
            handled = true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_DOWN)
        {
            mValue = ClampUnit(mValue - valStep);
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
/// @return true because the square can receive focus
///
/// @brief
/// Report focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cColorField2D::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  double hue [in] the hue (0-360) whose square to display
///
/// @return nothing
///
/// @brief
/// Set the hue whose saturation/value plane is shown.
///
/////////////////////////////////////////////////////////////////////////////
void cColorField2D::SetHue(double hue)
{
    mHue = hue;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  double saturation [in] saturation 0-1
/// @param  double value [in] value 0-1
///
/// @return nothing
///
/// @brief
/// Set the current selection point.
///
/////////////////////////////////////////////////////////////////////////////
void cColorField2D::SetSaturationValue(double saturation, double value)
{
    mSaturation = ClampUnit(saturation);
    mValue = ClampUnit(value);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the current saturation (0-1)
///
/// @brief
/// Get the selected saturation.
///
/////////////////////////////////////////////////////////////////////////////
double cColorField2D::GetSaturation(void) const
{
    return mSaturation;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the current value (0-1)
///
/// @brief
/// Get the selected value.
///
/////////////////////////////////////////////////////////////////////////////
double cColorField2D::GetValue(void) const
{
    return mValue;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::function<void(void)> callback [in] change notification
///
/// @return nothing
///
/// @brief
/// Set the callback invoked when the selection changes.
///
/////////////////////////////////////////////////////////////////////////////
void cColorField2D::SetOnChange(std::function<void(void)> callback)
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
/// Update the selection from a mouse position and notify listeners.
///
/////////////////////////////////////////////////////////////////////////////
void cColorField2D::UpdateFromMouse(const sInputEvent& event)
{
    if (mBounds.cols > 1)
    {
        double saturation = static_cast<double>(event.mouseCol - mBounds.col) / static_cast<double>(mBounds.cols - 1);
        mSaturation = ClampUnit(saturation);
    }

    if (mBounds.rows > 1)
    {
        double value = 1.0 - (static_cast<double>(event.mouseRow - mBounds.row) / static_cast<double>(mBounds.rows - 1));
        mValue = ClampUnit(value);
    }

    if (mOnChange)
    {
        mOnChange();
    }
}

}
