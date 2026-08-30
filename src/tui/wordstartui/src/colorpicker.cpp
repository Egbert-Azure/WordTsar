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

#include "colorpicker.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cColorPicker
///
/// @brief
/// Provides a simple 16-color foreground/background selector.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] picker bounds
/// @param  const sStyle& style [in] initial selected style
///
/// @return nothing
///
/// @brief
/// Construct a color picker.
///
/////////////////////////////////////////////////////////////////////////////
cColorPicker::cColorPicker(const sRect& bounds, const sStyle& style)
{
    mBounds = bounds;
    mFg = NearestPaletteColor(style.fg);
    mBg = NearestPaletteColor(style.bg);
    mAttrs = style.attrs;
    mEditingForeground = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw foreground/background names, color grid, and sample text.
///
/////////////////////////////////////////////////////////////////////////////
void cColorPicker::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle dialog = theme.GetStyle(THEME_ROLE_DIALOG);
    sStyle focus = theme.GetStyle(THEME_ROLE_DIALOG_FOCUS);

    screen.FillRect(mBounds, " ", dialog);

    std::string mode;

    if (mEditingForeground == true)
    {
        mode = "Editing foreground";
    }
    else
    {
        mode = "Editing background";
    }

    screen.PutText(mBounds.row, mBounds.col, mode, focus);
    screen.PutText(mBounds.row + 1, mBounds.col, "FG: " + ColorName(mFg) + "  BG: " + ColorName(mBg), dialog);

    for (int index = 0; index < 16; ++index)
    {
        eColor color = ColorFromIndex(index);
        sStyle cellStyle;

        cellStyle.fg = MakePaletteColor(COLOR_WHITE);
        cellStyle.bg = MakePaletteColor(color);
        cellStyle.attrs = CELL_ATTR_BOLD;

        if (color == COLOR_WHITE || color == COLOR_YELLOW || color == COLOR_LIGHT_GRAY || color == COLOR_LIGHT_CYAN)
        {
            cellStyle.fg = MakePaletteColor(COLOR_BLACK);
        }

        int row = mBounds.row + 3 + (index / 4);
        int col = mBounds.col + ((index % 4) * 10);
        std::string label = "  " + std::to_string(index) + "  ";
        screen.PutText(row, col, label, cellStyle);
    }

    screen.PutText(mBounds.row + 8, mBounds.col, "Tab switches FG/BG, arrows change color", dialog);
    screen.PutText(mBounds.row + 10, mBounds.col, "Sample: WordTsar display color", GetStyle());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the picker state changed
///
/// @brief
/// Handle color picker navigation.
///
/////////////////////////////////////////////////////////////////////////////
bool cColorPicker::HandleEvent(const sInputEvent& event)
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

        if ((event.mouseAction == MOUSE_ACTION_PRESS) && (event.mouseButton == MOUSE_BUTTON_LEFT))
        {
            if (event.mouseRow == mBounds.row)
            {
                if (mEditingForeground == true)
                {
                    mEditingForeground = false;
                }
                else
                {
                    mEditingForeground = true;
                }
                return true;
            }

            int gridRow = event.mouseRow - (mBounds.row + 3);
            int gridCol = (event.mouseCol - mBounds.col) / 10;

            if ((gridRow >= 0) && (gridRow < 4) && (gridCol >= 0) && (gridCol < 4))
            {
                int index = (gridRow * 4) + gridCol;

                if (mEditingForeground == true)
                {
                    mFg = ColorFromIndex(index);
                }
                else
                {
                    mBg = ColorFromIndex(index);
                }

                return true;
            }
        }

        return false;
    }

    if (event.type == INPUT_TYPE_SPECIAL)
    {
        if (event.special == SPECIAL_KEY_TAB)
        {
            if (mEditingForeground == true)
            {
                mEditingForeground = false;
            }
            else
            {
                mEditingForeground = true;
            }
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_LEFT)
        {
            MoveSelection(-1);
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_RIGHT)
        {
            MoveSelection(1);
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_UP)
        {
            MoveSelection(-4);
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_DOWN)
        {
            MoveSelection(4);
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because the color picker can receive focus
///
/// @brief
/// Report focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cColorPicker::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return selected style
///
/// @brief
/// Get selected foreground, background, and attributes.
///
/////////////////////////////////////////////////////////////////////////////
sStyle cColorPicker::GetStyle(void) const
{
    sStyle style;

    style.fg = MakePaletteColor(mFg);
    style.bg = MakePaletteColor(mBg);
    style.attrs = mAttrs;

    return style;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sStyle& style [in] style to select
///
/// @return nothing
///
/// @brief
/// Set selected style.
///
/////////////////////////////////////////////////////////////////////////////
void cColorPicker::SetStyle(const sStyle& style)
{
    mFg = NearestPaletteColor(style.fg);
    mBg = NearestPaletteColor(style.bg);
    mAttrs = style.attrs;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sColor& color [in] a 24-bit or default cell color
///
/// @return the nearest named palette entry (COLOR_LIGHT_GRAY for default)
///
/// @brief
/// Map a 24-bit color back to the closest of the 16 named palette entries so
/// this 16-color picker can be seeded from an arbitrary RGB style.
///
/////////////////////////////////////////////////////////////////////////////
eColor cColorPicker::NearestPaletteColor(const sColor& color) const
{
    if (color.isDefault == true)
    {
        return COLOR_LIGHT_GRAY;
    }

    eColor best = COLOR_BLACK;
    long bestDistance = -1;

    for (int index = 0; index < 16; ++index)
    {
        eColor candidate = ColorFromIndex(index);
        sColor rgb = MakePaletteColor(candidate);

        long dr = static_cast<long>(color.r) - static_cast<long>(rgb.r);
        long dg = static_cast<long>(color.g) - static_cast<long>(rgb.g);
        long db = static_cast<long>(color.b) - static_cast<long>(rgb.b);
        long distance = (dr * dr) + (dg * dg) + (db * db);

        if ((bestDistance < 0) || (distance < bestDistance))
        {
            bestDistance = distance;
            best = candidate;
        }
    }

    return best;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int index [in] color index from 0 to 15
///
/// @return color enum
///
/// @brief
/// Convert a picker index to a color.
///
/////////////////////////////////////////////////////////////////////////////
eColor cColorPicker::ColorFromIndex(int index) const
{
    switch (index)
    {
        case 0:
        {
            return COLOR_BLACK;
        }
        case 1:
        {
            return COLOR_BLUE;
        }
        case 2:
        {
            return COLOR_GREEN;
        }
        case 3:
        {
            return COLOR_CYAN;
        }
        case 4:
        {
            return COLOR_RED;
        }
        case 5:
        {
            return COLOR_MAGENTA;
        }
        case 6:
        {
            return COLOR_BROWN;
        }
        case 7:
        {
            return COLOR_LIGHT_GRAY;
        }
        case 8:
        {
            return COLOR_DARK_GRAY;
        }
        case 9:
        {
            return COLOR_LIGHT_BLUE;
        }
        case 10:
        {
            return COLOR_LIGHT_GREEN;
        }
        case 11:
        {
            return COLOR_LIGHT_CYAN;
        }
        case 12:
        {
            return COLOR_LIGHT_RED;
        }
        case 13:
        {
            return COLOR_LIGHT_MAGENTA;
        }
        case 14:
        {
            return COLOR_YELLOW;
        }
        case 15:
        {
            return COLOR_WHITE;
        }
        default:
        {
            return COLOR_LIGHT_GRAY;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eColor color [in] color enum
///
/// @return human-readable color name
///
/// @brief
/// Get a color name for display.
///
/////////////////////////////////////////////////////////////////////////////
std::string cColorPicker::ColorName(eColor color) const
{
    switch (color)
    {
        case COLOR_BLACK:
        {
            return "Black";
        }
        case COLOR_BLUE:
        {
            return "Blue";
        }
        case COLOR_GREEN:
        {
            return "Green";
        }
        case COLOR_CYAN:
        {
            return "Cyan";
        }
        case COLOR_RED:
        {
            return "Red";
        }
        case COLOR_MAGENTA:
        {
            return "Magenta";
        }
        case COLOR_BROWN:
        {
            return "Brown";
        }
        case COLOR_LIGHT_GRAY:
        {
            return "Light gray";
        }
        case COLOR_DARK_GRAY:
        {
            return "Dark gray";
        }
        case COLOR_LIGHT_BLUE:
        {
            return "Light blue";
        }
        case COLOR_LIGHT_GREEN:
        {
            return "Light green";
        }
        case COLOR_LIGHT_CYAN:
        {
            return "Light cyan";
        }
        case COLOR_LIGHT_RED:
        {
            return "Light red";
        }
        case COLOR_LIGHT_MAGENTA:
        {
            return "Light magenta";
        }
        case COLOR_YELLOW:
        {
            return "Yellow";
        }
        case COLOR_WHITE:
        {
            return "White";
        }
        default:
        {
            return "Default";
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eColor color [in] color enum
///
/// @return picker index
///
/// @brief
/// Convert a color to its picker index.
///
/////////////////////////////////////////////////////////////////////////////
int cColorPicker::ColorToIndex(eColor color) const
{
    for (int index = 0; index < 16; ++index)
    {
        if (ColorFromIndex(index) == color)
        {
            return index;
        }
    }

    return 7;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int delta [in] movement through the color grid
///
/// @return nothing
///
/// @brief
/// Change foreground or background by a signed delta.
///
/////////////////////////////////////////////////////////////////////////////
void cColorPicker::MoveSelection(int delta)
{
    eColor current;

    if (mEditingForeground == true)
    {
        current = mFg;
    }
    else
    {
        current = mBg;
    }

    int index = ColorToIndex(current);
    index += delta;

    if (index < 0)
    {
        index = 0;
    }

    if (index > 15)
    {
        index = 15;
    }

    if (mEditingForeground == true)
    {
        mFg = ColorFromIndex(index);
    }
    else
    {
        mBg = ColorFromIndex(index);
    }
}

}
