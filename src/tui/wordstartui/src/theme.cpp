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

#include "theme.h"

namespace wordstartui
{

namespace
{

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Build a style from two colors and an attribute mask.
///
/////////////////////////////////////////////////////////////////////////////
sStyle StyleFrom(sColor fg, sColor bg, uint32_t attrs)
{
    sStyle style;

    style.fg = fg;
    style.bg = bg;
    style.attrs = attrs;

    return style;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Lighten a color toward white by a fixed amount (clamped). Used to derive an
/// input-field surface from the chrome background.
///
/////////////////////////////////////////////////////////////////////////////
sColor Brighten(sColor color, int amount)
{
    if (color.isDefault == true)
    {
        return color;
    }

    int r = static_cast<int>(color.r) + amount;
    int g = static_cast<int>(color.g) + amount;
    int b = static_cast<int>(color.b) + amount;

    if (r > 255)
    {
        r = 255;
    }
    if (g > 255)
    {
        g = 255;
    }
    if (b > 255)
    {
        b = 255;
    }

    sColor result = color;
    result.r = static_cast<uint8_t>(r);
    result.g = static_cast<uint8_t>(g);
    result.b = static_cast<uint8_t>(b);

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Darken a color toward black by a fixed amount (clamped). Used to derive a
/// resting button chip from the focus accent.
///
/////////////////////////////////////////////////////////////////////////////
sColor Darken(sColor color, int amount)
{
    if (color.isDefault == true)
    {
        return color;
    }

    int r = static_cast<int>(color.r) - amount;
    int g = static_cast<int>(color.g) - amount;
    int b = static_cast<int>(color.b) - amount;

    if (r < 0)
    {
        r = 0;
    }
    if (g < 0)
    {
        g = 0;
    }
    if (b < 0)
    {
        b = 0;
    }

    sColor result = color;
    result.r = static_cast<uint8_t>(r);
    result.g = static_cast<uint8_t>(g);
    result.b = static_cast<uint8_t>(b);

    return result;
}

}

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTheme
///
/// @brief
/// Holds all user-editable colors and attributes used by the TUI widgets.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct a theme and initialize it to the default WordStar-like colors.
///
/////////////////////////////////////////////////////////////////////////////
cTheme::cTheme(void)
{
    ResetDefaults();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eThemeRole role [in] theme slot to query
///
/// @return style stored for the requested theme role
///
/// @brief
/// Get the style for a theme role.
///
/////////////////////////////////////////////////////////////////////////////
sStyle cTheme::GetStyle(eThemeRole role) const
{
    if ((role < THEME_ROLE_EDITOR) || (role >= THEME_ROLE_COUNT))
    {
        return MakeStyle(COLOR_LIGHT_GRAY, COLOR_BLACK, CELL_ATTR_NONE);
    }

    return mStyles[role];
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eThemeRole role [in] theme slot to change
/// @param  const sStyle& style [in] new style for the role
///
/// @return nothing
///
/// @brief
/// Set the style for a theme role.
///
/////////////////////////////////////////////////////////////////////////////
void cTheme::SetStyle(eThemeRole role, const sStyle& style)
{
    if ((role >= THEME_ROLE_EDITOR) && (role < THEME_ROLE_COUNT))
    {
        mStyles[role] = style;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Restore the built-in color and attribute defaults.
///
/////////////////////////////////////////////////////////////////////////////
void cTheme::ResetDefaults(void)
{
    LoadPalette(GetDefaultPalette());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eColor fg [in] foreground color
/// @param  eColor bg [in] background color
/// @param  uint32_t attrs [in] attribute bitmask
///
/// @return constructed style
///
/// @brief
/// Build a style value from primitive fields.
///
/////////////////////////////////////////////////////////////////////////////
sStyle cTheme::MakeStyle(eColor fg, eColor bg, uint32_t attrs) const
{
    sStyle style;

    style.fg = MakePaletteColor(fg);
    style.bg = MakePaletteColor(bg);
    style.attrs = attrs;

    return style;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the toolkit's single base palette (WordStar classic colors)
///
/// @brief
/// The one color theme the toolkit ships. Real alternate palettes come from
/// WordTsar's ini colors, fed through LoadPalette.
///
/////////////////////////////////////////////////////////////////////////////
sThemePalette cTheme::GetDefaultPalette(void)
{
    sThemePalette palette;

    palette.name = "WordStar Classic";
    palette.menuFg = MakePaletteColor(COLOR_BLACK);
    palette.menuBg = MakePaletteColor(COLOR_LIGHT_GRAY);
    palette.menuActiveFg = MakePaletteColor(COLOR_WHITE);
    palette.menuActiveBg = MakePaletteColor(COLOR_BLUE);
    palette.menuAccelFg = MakePaletteColor(COLOR_RED);
    palette.menuAccelBg = MakePaletteColor(COLOR_LIGHT_GRAY);
    palette.titleFg = MakePaletteColor(COLOR_WHITE);
    palette.titleBg = MakePaletteColor(COLOR_BLUE);
    palette.statusFg = MakePaletteColor(COLOR_WHITE);
    palette.statusBg = MakePaletteColor(COLOR_BLACK);
    palette.helpFg = MakePaletteColor(COLOR_LIGHT_GRAY);
    palette.helpBg = MakePaletteColor(COLOR_BLUE);
    palette.helpKeyFg = MakePaletteColor(COLOR_LIGHT_CYAN);
    palette.helpKeyBg = MakePaletteColor(COLOR_BLUE);
    palette.rulerFg = MakePaletteColor(COLOR_LIGHT_GRAY);
    palette.rulerBg = MakePaletteColor(COLOR_BLUE);
    palette.flagFg = MakePaletteColor(COLOR_LIGHT_GRAY);
    palette.flagBg = MakePaletteColor(COLOR_BLUE);
    palette.scrollFg = MakePaletteColor(COLOR_LIGHT_GRAY);
    palette.scrollBg = MakePaletteColor(COLOR_BLUE);
    palette.editorFg = MakePaletteColor(COLOR_LIGHT_GRAY);
    palette.editorBg = MakePaletteColor(COLOR_BLUE);
    palette.blockFg = MakePaletteColor(COLOR_BLACK);
    palette.blockBg = MakePaletteColor(COLOR_LIGHT_CYAN);

    return palette;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sThemePalette& p [in] source colors
///
/// @return nothing
///
/// @brief
/// Extrapolate every eThemeRole from the palette's source colors. Dialog,
/// button, list and field roles are derived from the chrome colors; the input
/// field surface is a brightened chrome background, and the warning role reuses
/// the accelerator accent.
///
/////////////////////////////////////////////////////////////////////////////
void cTheme::LoadPalette(const sThemePalette& p)
{
    mStyles[THEME_ROLE_EDITOR] = StyleFrom(p.editorFg, p.editorBg, CELL_ATTR_NONE);
    mStyles[THEME_ROLE_EDITOR_STATUS] = StyleFrom(p.statusFg, p.statusBg, CELL_ATTR_BOLD);
    mStyles[THEME_ROLE_EDITOR_BLOCK] = StyleFrom(p.blockFg, p.blockBg, CELL_ATTR_INVERSE);
    mStyles[THEME_ROLE_MENU] = StyleFrom(p.menuFg, p.menuBg, CELL_ATTR_NONE);
    mStyles[THEME_ROLE_MENU_ACTIVE] = StyleFrom(p.menuActiveFg, p.menuActiveBg, CELL_ATTR_BOLD);
    mStyles[THEME_ROLE_MENU_ACCEL] = StyleFrom(p.menuAccelFg, p.menuAccelBg, CELL_ATTR_BOLD);
    mStyles[THEME_ROLE_DIALOG] = StyleFrom(p.menuFg, p.menuBg, CELL_ATTR_NONE);
    mStyles[THEME_ROLE_DIALOG_TITLE] = StyleFrom(p.titleFg, p.titleBg, CELL_ATTR_BOLD);
    mStyles[THEME_ROLE_DIALOG_FOCUS] = StyleFrom(p.menuActiveFg, p.menuActiveBg, CELL_ATTR_BOLD);
    mStyles[THEME_ROLE_BUTTON] = StyleFrom(p.menuActiveFg, Darken(p.menuActiveBg, 60), CELL_ATTR_NONE);
    mStyles[THEME_ROLE_BUTTON_FOCUS] = StyleFrom(p.menuActiveFg, p.menuActiveBg, CELL_ATTR_BOLD);
    mStyles[THEME_ROLE_FIELD] = StyleFrom(p.menuFg, Brighten(p.menuBg, 85), CELL_ATTR_NONE);
    mStyles[THEME_ROLE_FIELD_FOCUS] = StyleFrom(p.menuActiveFg, p.menuActiveBg, CELL_ATTR_NONE);
    mStyles[THEME_ROLE_LIST] = StyleFrom(p.menuFg, p.menuBg, CELL_ATTR_NONE);
    mStyles[THEME_ROLE_LIST_SELECTED] = StyleFrom(p.menuActiveFg, p.menuActiveBg, CELL_ATTR_BOLD);
    mStyles[THEME_ROLE_HELP] = StyleFrom(p.helpFg, p.helpBg, CELL_ATTR_NONE);
    mStyles[THEME_ROLE_WARNING] = StyleFrom(p.menuActiveFg, p.menuAccelFg, CELL_ATTR_BOLD);
}

}
