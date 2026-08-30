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

#include "colorswatch.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cColorSwatch
///
/// @brief
/// A non-interactive preview rectangle filled with a solid color. Used by the
/// color picker to show the currently selected color.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] widget bounds
///
/// @return nothing
///
/// @brief
/// Construct the swatch with an initial black color.
///
/////////////////////////////////////////////////////////////////////////////
cColorSwatch::cColorSwatch(const sRect& bounds)
{
    mBounds = bounds;
    mColor = MakeRgb(0, 0, 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme (unused)
///
/// @return nothing
///
/// @brief
/// Fill the swatch area with the current color.
///
/////////////////////////////////////////////////////////////////////////////
void cColorSwatch::Draw(cScreen& screen, const cTheme& theme)
{
    (void)theme;

    sStyle style;
    style.fg = MakeRgb(255, 255, 255);
    style.bg = mColor;
    style.attrs = CELL_ATTR_NONE;

    screen.FillRect(mBounds, " ", style);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event (ignored)
///
/// @return false because the swatch never consumes input
///
/// @brief
/// The swatch is display only.
///
/////////////////////////////////////////////////////////////////////////////
bool cColorSwatch::HandleEvent(const sInputEvent& event)
{
    (void)event;
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sColor& color [in] the color to display
///
/// @return nothing
///
/// @brief
/// Set the previewed color.
///
/////////////////////////////////////////////////////////////////////////////
void cColorSwatch::SetColor(const sColor& color)
{
    mColor = color;
}

}
