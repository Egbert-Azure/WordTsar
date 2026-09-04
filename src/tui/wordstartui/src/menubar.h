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

#ifndef WORDTSAR_TUI_MENUBAR_H
#define WORDTSAR_TUI_MENUBAR_H

#include "screen.h"
#include "theme.h"
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace wordstartui
{

// One entry inside a pull-down menu. hotKey is the accelerator letter (lower
// case) and hotKeyPos is its index in title, for highlighting. An entry is one
// of: a leaf action (callback), a separator (isSeparator), or a submenu (submenu
// non-empty) that opens a cascading flyout. Disabled entries are drawn dimmed
// and cannot be activated.
struct sMenuEntry
{
    std::string title;
    std::string shortcut;   // right-aligned accelerator text (e.g. "^KS")
    std::function<void(void)> callback;
    char hotKey;
    int hotKeyPos;
    bool isSeparator;
    bool enabled;
    std::vector<sMenuEntry> submenu;
};

// A top-level menu-bar item. When entries is empty the item is a direct action
// (its callback fires immediately). When entries is non-empty the item is a
// pull-down that opens a list of sMenuEntry choices below the bar.
struct sMenuItem
{
    std::string title;
    char hotKey;
    std::function<void(void)> callback;
    std::vector<sMenuEntry> entries;
};

class cMenuBar
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cMenuBar(void);

    void SetBounds(const sRect& bounds);
    void Clear(void);
    void AddItem(const std::string& title, char hotKey, std::function<void(void)> callback);
    int AddMenu(const std::string& title, char hotKey);
    void AddSubItem(int menuIndex, const std::string& title, std::function<void(void)> callback,
                    bool enabled = true);
    void AddSeparator(int menuIndex);

    // Add a cascading submenu to a top-level menu and return a handle used to
    // populate it via AddSubMenuItem / AddSubMenuSeparator.
    int AddSubMenu(int menuIndex, const std::string& title);
    void AddSubMenuItem(int subHandle, const std::string& title,
                        std::function<void(void)> callback, bool enabled = true);
    void AddSubMenuSeparator(int subHandle);

    void Draw(cScreen& screen, const cTheme& theme);
    bool HandleEvent(const sInputEvent& event);
    bool IsOpen(void) const;
    void Close(void);

    const std::vector<sMenuItem>& GetItems(void) const;

private:
    int MenuColumn(int menuIndex) const;
    int MenuTextWidth(int menuIndex) const;
    void OpenMenu(int menuIndex);
    void MoveHighlight(int delta);
    void MoveMenu(int delta);
    void Descend(void);
    void Ascend(void);
    void ActivateHighlighted(void);

    // Parse a '&' accelerator out of a title into an entry; the '&' is stripped
    // and the following character becomes the (lower-case) hotkey.
    sMenuEntry MakeEntry(const std::string& title) const;

    // The list of entries at a given open depth (0 = top pull-down), following
    // mPath through any descended submenus. Returns nullptr if out of range.
    const std::vector<sMenuEntry>* ContainerAtLevel(int level) const;

    int DropWidth(const std::vector<sMenuEntry>& entries) const;
    int FirstSelectable(const std::vector<sMenuEntry>& entries) const;
    sRect LevelRect(int level) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    sRect mBounds;
    std::vector<sMenuItem> mItems;
    std::vector<std::pair<int, int>> mSubHandles;  // handle -> (menuIndex, entryIndex)
    int mOpenMenu;
    std::vector<int> mPath;   // highlight index per open depth (level 0 = top pull-down)
    int mScreenRows;          // last known screen height (for flyout clamping)
};

}

#endif
