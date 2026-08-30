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

#ifndef WORDTSAR_TUI_COLORFIELD2D_H
#define WORDTSAR_TUI_COLORFIELD2D_H

#include "widget.h"

#include <functional>

namespace wordstartui
{

class cColorField2D final : public cWidget
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cColorField2D(const sRect& bounds);

    void Draw(cScreen& screen, const cTheme& theme) override;
    bool HandleEvent(const sInputEvent& event) override;
    bool CanFocus(void) const override;

    void SetHue(double hue);
    void SetSaturationValue(double saturation, double value);
    double GetSaturation(void) const;
    double GetValue(void) const;
    void SetOnChange(std::function<void(void)> callback);

private:
    void UpdateFromMouse(const sInputEvent& event);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    double mHue;
    double mSaturation;
    double mValue;
    std::function<void(void)> mOnChange;
};

}

#endif
