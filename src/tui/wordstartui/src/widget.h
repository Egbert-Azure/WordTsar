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

#ifndef WORDTSAR_TUI_WIDGET_H
#define WORDTSAR_TUI_WIDGET_H

#include "screen.h"
#include "theme.h"
#include "tuidefs.h"

namespace wordstartui
{

class cWidget
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cWidget(void);
    virtual ~cWidget(void);

    virtual void Draw(cScreen& screen, const cTheme& theme) = 0;
    virtual bool HandleEvent(const sInputEvent& event) = 0;
    virtual bool CanFocus(void) const;

    void SetBounds(const sRect& bounds);
    sRect GetBounds(void) const;
    void SetFocus(bool focus);
    bool HasFocus(void) const;
    void SetEnabled(bool enabled);
    bool IsEnabled(void) const;

    // Hit test for mouse routing. Defaults to the widget bounds; overlay widgets
    // (e.g. an open dropdown) override this to also claim their expanded area.
    virtual bool ContainsEventPoint(int row, int col) const;

    // True when the widget is currently painting an overlay that extends past
    // its bounds (e.g. an open dropdown list). Hosts draw such widgets last so
    // the overlay is not overwritten by neighbouring controls.
    virtual bool HasOpenOverlay(void) const;

protected:
    bool ContainsPoint(int row, int col) const;
    bool IsMouseInside(const sInputEvent& event) const;
    bool IsLeftMousePressInside(const sInputEvent& event) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

protected:
    sRect mBounds;
    bool mHasFocus;
    bool mEnabled;
};

}

#endif
