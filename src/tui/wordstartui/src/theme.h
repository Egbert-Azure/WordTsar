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

#ifndef WORDTSAR_TUI_THEME_H
#define WORDTSAR_TUI_THEME_H

#include "tuidefs.h"
#include <array>
#include <string>

namespace wordstartui
{

// A palette of WordTsar source colors from which a full toolkit theme is
// extrapolated. Every eThemeRole is derived from these (see cTheme::LoadPalette).
// The toolkit ships a single base palette (GetDefaultPalette); real alternate
// palettes come from WordTsar's ini colors.
struct sThemePalette
{
    std::string name;
    sColor menuFg, menuBg;              // menu bar
    sColor menuActiveFg, menuActiveBg;  // menu highlight (focus/selection accent)
    sColor menuAccelFg, menuAccelBg;    // menu accelerator (alert accent)
    sColor titleFg, titleBg;            // title bar
    sColor statusFg, statusBg;          // status bar
    sColor helpFg, helpBg;              // help panel
    sColor helpKeyFg, helpKeyBg;        // help keystroke
    sColor rulerFg, rulerBg;            // ruler
    sColor flagFg, flagBg;              // flag column
    sColor scrollFg, scrollBg;          // scrollbar
    sColor editorFg, editorBg;          // editor text / background
    sColor blockFg, blockBg;            // block selection
};

class cTheme
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cTheme(void);

    sStyle GetStyle(eThemeRole role) const;
    void SetStyle(eThemeRole role, const sStyle& style);
    void ResetDefaults(void);

    // Extrapolate every role from a palette's source colors.
    void LoadPalette(const sThemePalette& palette);

    // The single base palette the toolkit ships (WordStar classic colors).
    static sThemePalette GetDefaultPalette(void);

private:
    sStyle MakeStyle(eColor fg, eColor bg, uint32_t attrs) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    std::array<sStyle, THEME_ROLE_COUNT> mStyles;
};

}

#endif
