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

#include "tuidefs.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sStyle
///
/// @brief
/// Stores the terminal display style for a cell or widget drawing operation.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sCell
///
/// @brief
/// Stores one terminal screen cell. The printable data is a UTF-8 grapheme
/// cluster rather than a single byte or Unicode scalar.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sInputEvent
///
/// @brief
/// Stores one normalized input event while preserving the raw bytes used by
/// WordStar command handling.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  uint8_t r, g, b [in] 24-bit color components
///
/// @return an sColor holding the given RGB triple
///
/// @brief
/// Build a 24-bit RGB cell color.
///
/////////////////////////////////////////////////////////////////////////////
sColor MakeRgb(uint8_t r, uint8_t g, uint8_t b)
{
    sColor color;

    color.isDefault = false;
    color.r = r;
    color.g = g;
    color.b = b;

    return color;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return an sColor that resolves to the terminal's default fg/bg
///
/// @brief
/// Build the "terminal default" cell color (SGR 39/49).
///
/////////////////////////////////////////////////////////////////////////////
sColor MakeDefaultColor(void)
{
    sColor color;

    color.isDefault = true;
    color.r = 0;
    color.g = 0;
    color.b = 0;

    return color;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eColor color [in] a named 16-color palette entry
///
/// @return the RGB sColor for that palette entry (default for COLOR_DEFAULT)
///
/// @brief
/// Convert a named palette color to a 24-bit RGB color using the standard
/// VGA/xterm 16-color values, so the theme and color picker (which speak in
/// named colors) render on truecolor terminals.
///
/////////////////////////////////////////////////////////////////////////////
sColor MakePaletteColor(eColor color)
{
    switch (color)
    {
        case COLOR_BLACK:         return MakeRgb(0, 0, 0);
        case COLOR_BLUE:          return MakeRgb(0, 0, 170);
        case COLOR_GREEN:         return MakeRgb(0, 170, 0);
        case COLOR_CYAN:          return MakeRgb(0, 170, 170);
        case COLOR_RED:           return MakeRgb(170, 0, 0);
        case COLOR_MAGENTA:       return MakeRgb(170, 0, 170);
        case COLOR_BROWN:         return MakeRgb(170, 85, 0);
        case COLOR_LIGHT_GRAY:    return MakeRgb(170, 170, 170);
        case COLOR_DARK_GRAY:     return MakeRgb(85, 85, 85);
        case COLOR_LIGHT_BLUE:    return MakeRgb(85, 85, 255);
        case COLOR_LIGHT_GREEN:   return MakeRgb(85, 255, 85);
        case COLOR_LIGHT_CYAN:    return MakeRgb(85, 255, 255);
        case COLOR_LIGHT_RED:     return MakeRgb(255, 85, 85);
        case COLOR_LIGHT_MAGENTA: return MakeRgb(255, 85, 255);
        case COLOR_YELLOW:        return MakeRgb(255, 255, 85);
        case COLOR_WHITE:         return MakeRgb(255, 255, 255);
        case COLOR_DEFAULT:
        default:                  return MakeDefaultColor();
    }
}

}
