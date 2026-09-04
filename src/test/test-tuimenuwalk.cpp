//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
// Copyright (C) 2026 Egbert H. Schroeer
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

#include "doctest.h"

#include "src/tui/wordtsar.h"
#include "src/tui/wordstartui/src/menubar.h"

#include <string>


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Menu-walking regression test, dispatch-resolution tier only: for every
/// enabled menu entry the real TUI menu tree builds, the entry either opens a
/// submenu or has a real callback -- it never reaches the screen wired to
/// nothing. This does not check that the callback does the right thing (that
/// would require a second maintained copy of the expected behaviour); it only
/// catches an entry added to the tree without being connected to a handler.
///
/////////////////////////////////////////////////////////////////////////////

namespace
{

void CheckEntries(const std::vector<wordstartui::sMenuEntry>& entries, const std::string& path)
{
    for (const wordstartui::sMenuEntry& entry : entries)
    {
        std::string entryPath = path + " > " + entry.title;
        if (entry.isSeparator == true)
        {
            continue;
        }
        if (entry.enabled == false)
        {
            continue;
        }
        if (entry.submenu.empty() == false)
        {
            CheckEntries(entry.submenu, entryPath);
            continue;
        }
        INFO("menu entry: ", entryPath);
        CHECK(static_cast<bool>(entry.callback) == true);
    }
}

}


TEST_CASE("TUI menu walk - every enabled entry resolves to a real target")
{
    cWSWordTsar app;
    app.TestBuildMenus();

    const wordstartui::cMenuBar& menu = app.TestGetMenu();
    const std::vector<wordstartui::sMenuItem>& items = menu.GetItems();
    CHECK(items.empty() == false);

    for (const wordstartui::sMenuItem& item : items)
    {
        std::string path = item.title;
        if (item.entries.empty() == true)
        {
            INFO("top-level menu item: ", path);
            CHECK(static_cast<bool>(item.callback) == true);
            continue;
        }
        CheckEntries(item.entries, path);
    }
}
