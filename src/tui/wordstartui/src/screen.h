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

#ifndef WORDTSAR_TUI_SCREEN_H
#define WORDTSAR_TUI_SCREEN_H

#include "terminaldriver.h"
#include "tuidefs.h"
#include <vector>

namespace wordstartui
{

class cScreen
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cScreen(void);

    void Resize(int rows, int cols);
    int GetRows(void) const;
    int GetCols(void) const;
    void Clear(const sStyle& style);
    void PutCell(int row, int col, const std::string& graphemeUtf8, const sStyle& style);
    void PutText(int row, int col, const std::string& textUtf8, const sStyle& style);
    void FillRect(const sRect& rect, const std::string& graphemeUtf8, const sStyle& style);
    void DrawBox(const sRect& rect, const sStyle& style);
    void PushClip(const sRect& rect);
    void PopClip(void);
    void Invalidate(void);
    void Present(cTerminalDriver& driver);

private:
    size_t Index(int row, int col) const;
    bool IsInsideClip(int row, int col) const;
    sRect CurrentClip(void) const;
    sRect Intersect(const sRect& first, const sRect& second) const;
    sCell MakeBlankCell(const sStyle& style) const;
    void SetCell(int row, int col, const sCell& cell);
    bool CellsEqual(const sCell& first, const sCell& second) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    int mRows;
    int mCols;
    std::vector<sCell> mBackBuffer;
    std::vector<sCell> mFrontBuffer;
    std::vector<sRect> mClipStack;
};

}

#endif
