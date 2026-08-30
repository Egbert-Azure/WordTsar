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

#ifndef WORDTSAR_TUI_COLORPICKER_H
#define WORDTSAR_TUI_COLORPICKER_H

#include "widget.h"
#include <string>
#include <vector>

namespace wordstartui
{

class cColorPicker final : public cWidget
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cColorPicker(const sRect& bounds, const sStyle& style);

    void Draw(cScreen& screen, const cTheme& theme) override;
    bool HandleEvent(const sInputEvent& event) override;
    bool CanFocus(void) const override;
    sStyle GetStyle(void) const;
    void SetStyle(const sStyle& style);

private:
    eColor ColorFromIndex(int index) const;
    std::string ColorName(eColor color) const;
    int ColorToIndex(eColor color) const;
    eColor NearestPaletteColor(const sColor& color) const;
    void MoveSelection(int delta);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    eColor mFg;
    eColor mBg;
    uint32_t mAttrs;
    bool mEditingForeground;
};

}

#endif
