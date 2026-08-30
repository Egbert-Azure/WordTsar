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

#ifndef WORDTSAR_TUI_DOCUMENTVIEW_H
#define WORDTSAR_TUI_DOCUMENTVIEW_H

#include "screen.h"
#include "theme.h"
#include <string>
#include <vector>

namespace wordstartui
{

class cDocumentView
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cDocumentView(void);

    void SetBounds(const sRect& bounds);
    void SetLines(const std::vector<std::string>& lines);
    void Draw(cScreen& screen, const cTheme& theme);
    bool HandleEvent(const sInputEvent& event);
    int GetCursorRow(void) const;
    int GetCursorCol(void) const;

private:
    void MoveCursor(int rowDelta, int colDelta);
    void SetCursorFromPoint(int row, int col);
    bool ContainsPoint(int row, int col) const;
    void EnsureVisible(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    sRect mBounds;
    std::vector<std::string> mLines;
    int mCursorLine;
    int mCursorCol;
    int mTopLine;
    int mLeftCol;
};

}

#endif
