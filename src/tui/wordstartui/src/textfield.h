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

#ifndef WORDTSAR_TUI_TEXTFIELD_H
#define WORDTSAR_TUI_TEXTFIELD_H

#include "widget.h"
#include <string>

namespace wordstartui
{

class cTextField final : public cWidget
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cTextField(const sRect& bounds, const std::string& text);

    void Draw(cScreen& screen, const cTheme& theme) override;
    bool HandleEvent(const sInputEvent& event) override;
    bool CanFocus(void) const override;
    void SetText(const std::string& text);
    std::string GetText(void) const;
    int GetCursorColumn(void) const;

private:
    void MoveLeft(void);
    void MoveRight(void);
    void Backspace(void);
    void Delete(void);
    void InsertText(const std::string& text);
    void SetCursorFromColumn(int column);
    int DisplayLength(void) const;
    void ClampCursor(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    std::string mText;
    int mCursor;
    int mScroll;
};

}

#endif
