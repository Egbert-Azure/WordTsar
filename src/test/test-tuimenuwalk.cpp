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
#include "src/tui/editor/editorctrl.h"
#include "src/tui/wordstartui/src/menubar.h"

#include <cctype>
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


namespace
{

// Text of one screen row from column 0, one character per cell.
std::string MenuRowText(const wordstartui::cScreen& screen, int row)
{
    std::string text;
    for (int col = 0; col < screen.GetCols(); ++col)
    {
        const wordstartui::sCell* cell = screen.GetCell(row, col);
        text += (cell != nullptr) ? cell->textUtf8 : std::string("?");
    }
    return text;
}

wordstartui::cMenuBar OpeningStyleMenu(void)
{
    wordstartui::cMenuBar menu;
    for (const char* title : {"File", "Utilities", "Additional", "Help"})
    {
        int index = menu.AddMenu(title, static_cast<char>(std::tolower(title[0])));
        menu.AddSubItem(index, "&Entry", [](void) {});
    }
    wordstartui::sRect bounds{};
    bounds.row = 0;
    bounds.col = 0;
    bounds.rows = 1;
    bounds.cols = 80;
    menu.SetBounds(bounds);
    return menu;
}

}


TEST_CASE("TUI menu bar - default layout puts two columns between menus")
{
    wordstartui::cScreen screen;
    screen.Resize(24, 80);
    wordstartui::cTheme theme;

    wordstartui::cMenuBar menu = OpeningStyleMenu();
    menu.Draw(screen, theme);

    CHECK(MenuRowText(screen, 0) ==
          " File  Utilities  Additional  Help                                              ");
}


TEST_CASE("TUI menu bar - gap 3 and a right-aligned last menu")
{
    wordstartui::cScreen screen;
    screen.Resize(24, 80);
    wordstartui::cTheme theme;

    wordstartui::cMenuBar menu = OpeningStyleMenu();
    menu.SetMenuGap(3);
    menu.SetLastMenuRightAligned(true);
    menu.Draw(screen, theme);

    std::string row = MenuRowText(screen, 0);
    CHECK(row.find("File") == 1);
    CHECK(row.find("Utilities") == 8);
    CHECK(row.find("Additional") == 20);
    CHECK(row.find("Help") == 74);
    CHECK(row.substr(78) == "  ");

    SUBCASE("a click on the right-aligned title opens its menu")
    {
        wordstartui::sInputEvent click{};
        click.type = wordstartui::INPUT_TYPE_MOUSE;
        click.mouseAction = wordstartui::MOUSE_ACTION_PRESS;
        click.mouseButton = wordstartui::MOUSE_BUTTON_LEFT;
        click.mouseRow = 0;
        click.mouseCol = 75;
        CHECK(menu.HandleEvent(click) == true);
        CHECK(menu.IsOpen() == true);
    }

    SUBCASE("a click where Help sits by default opens nothing")
    {
        wordstartui::sInputEvent click{};
        click.type = wordstartui::INPUT_TYPE_MOUSE;
        click.mouseAction = wordstartui::MOUSE_ACTION_PRESS;
        click.mouseButton = wordstartui::MOUSE_BUTTON_LEFT;
        click.mouseRow = 0;
        click.mouseCol = 32;
        menu.HandleEvent(click);
        CHECK(menu.IsOpen() == false);
    }
}


TEST_CASE("TUI menu bar - the edit-screen menu keeps the default layout")
{
    cWSWordTsar app;
    app.TestBuildMenus();
    wordstartui::cMenuBar menu = app.TestGetMenu();
    wordstartui::sRect bounds{};
    bounds.row = 0;
    bounds.col = 0;
    bounds.rows = 1;
    bounds.cols = 80;
    menu.SetBounds(bounds);

    wordstartui::cScreen screen;
    screen.Resize(24, 80);
    wordstartui::cTheme theme;
    menu.Draw(screen, theme);

    CHECK(MenuRowText(screen, 0) ==
          " File  Edit  View  Insert  Style  Layout  Utilities  Help                       ");
}


namespace
{

// True when any row of the screen contains the text.
bool ScreenHasText(const wordstartui::cScreen& screen, const std::string& text)
{
    for (int row = 0; row < screen.GetRows(); ++row)
    {
        if (MenuRowText(screen, row).find(text) != std::string::npos)
        {
            return true;
        }
    }
    return false;
}

}


TEST_CASE("TUI Opening screen - help level 4 draws the title and menu bars")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 4;
    app.TestEnterOpening();

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    // Row 0: the name centred in a fill of upper half blocks.
    const wordstartui::sCell* first = screen.GetCell(0, 0);
    REQUIRE(first != nullptr);
    CHECK(first->textUtf8 == "\xe2\x96\x80");
    const wordstartui::sCell* last = screen.GetCell(0, 79);
    REQUIRE(last != nullptr);
    CHECK(last->textUtf8 == "\xe2\x96\x80");
    std::string name;
    for (int col = 36; col < 44; ++col)
    {
        name += screen.GetCell(0, col)->textUtf8;
    }
    CHECK(name == "WordTsar");

    // Row 1: the Opening menu bar.
    std::string bar = MenuRowText(screen, 1);
    CHECK(bar.find("File") == 1);
    CHECK(bar.find("Utilities") == 8);
    CHECK(bar.find("Additional") == 20);
    CHECK(bar.find("Help") == 74);

    // No classic command panel.
    CHECK(ScreenHasText(screen, "O P E N I N G") == false);
    CHECK(ScreenHasText(screen, "open a document") == false);
    CHECK(ScreenHasText(screen, "speed write") == false);
}


TEST_CASE("TUI Opening screen - help level 3 draws the classic panel")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 3;
    app.TestEnterOpening();

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    CHECK(ScreenHasText(screen, "O P E N I N G   M E N U") == true);
    CHECK(ScreenHasText(screen, "open a document") == true);
    CHECK(MenuRowText(screen, 1).find("Utilities") == std::string::npos);
}


TEST_CASE("TUI Opening screen - each Opening menu opens")
{
    cWSWordTsar app;
    wordstartui::cMenuBar menu = app.TestGetOpeningMenu();
    const std::vector<wordstartui::sMenuItem>& items = menu.GetItems();
    REQUIRE(items.size() == 4);
    CHECK(items[0].title == "File");
    CHECK(items[1].title == "Utilities");
    CHECK(items[2].title == "Additional");
    CHECK(items[3].title == "Help");

    for (const char* letter : {"f", "u", "a", "h"})
    {
        INFO("Alt+", letter);
        wordstartui::sInputEvent event{};
        event.type = wordstartui::INPUT_TYPE_TEXT;
        event.alt = true;
        event.textUtf8 = letter;
        menu.Close();
        CHECK(menu.HandleEvent(event) == true);
        CHECK(menu.IsOpen() == true);
    }
}
