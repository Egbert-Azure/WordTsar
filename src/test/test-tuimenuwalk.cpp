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


namespace
{

wordstartui::sInputEvent OpeningText(const std::string& text)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_TEXT;
    event.textUtf8 = text;
    return event;
}

wordstartui::sInputEvent OpeningSpecial(wordstartui::eSpecialKey key)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_SPECIAL;
    event.special = key;
    return event;
}

}


TEST_CASE("TUI Opening screen - letter keys run their actions")
{
    cWSWordTsar app;
    app.TestEnterOpening();
    REQUIRE(app.TestInOpening() == true);
    REQUIRE(app.TestOpeningBrowserFocus() == false);

    SUBCASE("D opens the file list for a document")
    {
        CHECK(app.TestHandleEvent(OpeningText("d")) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == false);
        CHECK(app.TestOpeningWantTOC() == false);
        CHECK(app.TestOpeningWantIndex() == false);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestOpeningSelection() == 0);
    }

    SUBCASE("L opens the file list")
    {
        CHECK(app.TestHandleEvent(OpeningText("L")) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == false);
        CHECK(app.TestOpeningSelection() == 9);
    }

    SUBCASE("P opens the file list to print")
    {
        CHECK(app.TestHandleEvent(OpeningText("p")) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == true);
        CHECK(app.TestOpeningWantTOC() == false);
        CHECK(app.TestOpeningWantIndex() == false);
    }

    SUBCASE("T opens the file list for a table of contents")
    {
        CHECK(app.TestHandleEvent(OpeningText("t")) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantTOC() == true);
        CHECK(app.TestOpeningWantPrint() == false);
    }

    SUBCASE("I opens the file list to index a document")
    {
        CHECK(app.TestHandleEvent(OpeningText("i")) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantIndex() == true);
        CHECK(app.TestOpeningWantPrint() == false);
    }

    SUBCASE("S starts a new untitled document")
    {
        CHECK(app.TestHandleEvent(OpeningText("s")) == true);
        CHECK(app.TestInEditor() == true);
        CHECK(app.TestFilename().empty() == true);
        CHECK(app.TestQuitRequested() == false);
    }

    SUBCASE("X exits")
    {
        CHECK(app.TestHandleEvent(OpeningText("x")) == true);
        CHECK(app.TestQuitRequested() == true);
    }

    SUBCASE("Escape exits")
    {
        CHECK(app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ESCAPE)) == true);
        CHECK(app.TestQuitRequested() == true);
    }

    SUBCASE("letters of greyed-out items do nothing")
    {
        for (const char* letter : {"n", "\\", "k", "c", "e", "o", "y", "f", "m", "r"})
        {
            INFO("letter: ", letter);
            CHECK(app.TestHandleEvent(OpeningText(letter)) == true);
            CHECK(app.TestOpeningBrowserFocus() == false);
            CHECK(app.TestInOpening() == true);
            CHECK(app.TestQuitRequested() == false);
            CHECK(app.TestOpeningSelection() == 0);
        }
    }
}


TEST_CASE("TUI Opening screen - Enter runs the selected item")
{
    cWSWordTsar app;
    app.TestEnterOpening();

    SUBCASE("Enter on the first item opens the file list")
    {
        CHECK(app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ENTER)) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
    }

    SUBCASE("Enter on a greyed-out item does nothing")
    {
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_DOWN));
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_DOWN));
        REQUIRE(app.TestOpeningSelection() == 2);
        CHECK(app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ENTER)) == true);
        CHECK(app.TestOpeningBrowserFocus() == false);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestQuitRequested() == false);
    }

    SUBCASE("Enter on print a file opens the file list to print")
    {
        for (int step = 0; step < 3; ++step)
        {
            app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_DOWN));
        }
        REQUIRE(app.TestOpeningSelection() == 3);
        CHECK(app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ENTER)) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == true);
    }

    SUBCASE("Enter on change drive/directory opens the file list")
    {
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_RIGHT));
        REQUIRE(app.TestOpeningSelection() == 9);
        CHECK(app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ENTER)) == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == false);
    }
}


TEST_CASE("TUI Opening screen - Escape and D clear pending file-list requests")
{
    cWSWordTsar app;
    app.TestEnterOpening();

    app.TestHandleEvent(OpeningText("p"));
    REQUIRE(app.TestOpeningWantPrint() == true);
    app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ESCAPE));
    CHECK(app.TestOpeningBrowserFocus() == false);
    CHECK(app.TestOpeningWantPrint() == false);
    CHECK(app.TestQuitRequested() == false);

    app.TestHandleEvent(OpeningText("t"));
    REQUIRE(app.TestOpeningWantTOC() == true);
    app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_TAB));
    REQUIRE(app.TestOpeningBrowserFocus() == false);
    app.TestHandleEvent(OpeningText("d"));
    CHECK(app.TestOpeningBrowserFocus() == true);
    CHECK(app.TestOpeningWantPrint() == false);
    CHECK(app.TestOpeningWantTOC() == false);
    CHECK(app.TestOpeningWantIndex() == false);
}
