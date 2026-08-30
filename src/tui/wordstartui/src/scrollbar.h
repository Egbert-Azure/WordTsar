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

#ifndef WORDTSAR_TUI_SCROLLBAR_H
#define WORDTSAR_TUI_SCROLLBAR_H

#include "screen.h"
#include "tuidefs.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cScrollBar
///
/// @brief
/// A reusable vertical scrollbar helper (not a focusable widget). A host sets
/// the track geometry and its scroll state (total / visible / top), then draws
/// a proportional thumb and maps mouse rows back to scroll positions for click
/// and drag. Used by the list box, dropdown, and the editor window.
///
/////////////////////////////////////////////////////////////////////////////
class cScrollBar
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cScrollBar(void);

    void SetMetrics(const sRect& track, int total, int visible, int top);
    void SetArrows(bool arrows);
    bool NeedsBar(void) const;
    void Draw(cScreen& screen, const sStyle& trackStyle, const sStyle& thumbStyle) const;
    bool ContainsPoint(int row, int col) const;
    bool IsUpArrow(int row) const;
    bool IsDownArrow(int row) const;
    bool IsAboveThumb(int row) const;
    bool IsBelowThumb(int row) const;
    int TopForRow(int mouseRow) const;

private:
    int ThumbHeight(void) const;
    int ThumbRow(void) const;
    int MaxTop(void) const;
    int InnerTop(void) const;
    int InnerRows(void) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    sRect mTrack;
    int mTotal;
    int mVisible;
    int mTop;
    bool mArrows;
};

}

#endif // WORDTSAR_TUI_SCROLLBAR_H
