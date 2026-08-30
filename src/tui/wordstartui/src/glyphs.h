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

#ifndef WORDTSAR_TUI_GLYPHS_H
#define WORDTSAR_TUI_GLYPHS_H

namespace wordstartui
{

// Modern Unicode glyphs for widget markers. Single tweak point for the look of
// checkboxes and radio buttons. Stored as UTF-8 byte sequences.
inline constexpr const char* GLYPH_CHECK_ON = "\xe2\x96\xa3";       // U+25A3 filled square
inline constexpr const char* GLYPH_CHECK_OFF = "\xe2\x98\x90";      // U+2610 ballot box
inline constexpr const char* GLYPH_CHECK_INHERIT = "\xe2\x96\xa8";  // U+25A8 hatched square
inline constexpr const char* GLYPH_RADIO_ON = "\xe2\x97\x89";       // U+25C9 fisheye
inline constexpr const char* GLYPH_RADIO_OFF = "\xe2\x97\x8b";      // U+25CB circle
inline constexpr const char* GLYPH_BUTTON_LEFT = "\xe2\x96\xb8";    // U+25B8 focus cap left
inline constexpr const char* GLYPH_BUTTON_RIGHT = "\xe2\x97\x82";   // U+25C2 focus cap right

// Block and shade glyphs for gradients (color picker saturation/value square
// and hue bar). The upper-half block paints two vertical color samples per cell
// via its foreground (top) and background (bottom) colors.
inline constexpr const char* GLYPH_UPPER_HALF = "\xe2\x96\x80";     // U+2580 upper half block
inline constexpr const char* GLYPH_LOWER_HALF = "\xe2\x96\x84";     // U+2584 lower half block
inline constexpr const char* GLYPH_FULL_BLOCK = "\xe2\x96\x88";     // U+2588 full block
inline constexpr const char* GLYPH_SHADE_LIGHT = "\xe2\x96\x91";    // U+2591 light shade
inline constexpr const char* GLYPH_SHADE_MEDIUM = "\xe2\x96\x92";   // U+2592 medium shade
inline constexpr const char* GLYPH_SHADE_DARK = "\xe2\x96\x93";     // U+2593 dark shade

}

#endif // WORDTSAR_TUI_GLYPHS_H
