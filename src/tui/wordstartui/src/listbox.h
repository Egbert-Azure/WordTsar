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

#ifndef WORDTSAR_TUI_LISTBOX_H
#define WORDTSAR_TUI_LISTBOX_H

#include "widget.h"
#include <string>
#include <vector>

namespace wordstartui
{

class cListBox final : public cWidget
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cListBox(const sRect& bounds);

    void SetItems(const std::vector<std::string>& items);
    void Draw(cScreen& screen, const cTheme& theme) override;
    bool HandleEvent(const sInputEvent& event) override;
    bool CanFocus(void) const override;
    int GetSelectedIndex(void) const;
    std::string GetSelectedText(void) const;
    void SetSelectedIndex(int index);

private:
    void MoveSelection(int delta);
    void EnsureVisible(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    std::vector<std::string> mItems;
    int mSelectedIndex;
    int mTopIndex;
};

}

#endif
