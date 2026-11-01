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
#include "src/tui/wordstartui/src/dialog.h"
#include "src/tui/dialogs/dialoghost.h"
#include "src/input/wordtsarinput.h"
#include "src/tui/dialogs/filebrowser.h"
#include "src/core/utils/config.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
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


namespace
{

wordstartui::sInputEvent OpeningAltText(const std::string& text)
{
    wordstartui::sInputEvent event = OpeningText(text);
    event.alt = true;
    return event;
}

wordstartui::sInputEvent OpeningFunction(int key)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_FUNCTION;
    event.functionKey = key;
    return event;
}

}


TEST_CASE("TUI Opening screen - help level 4 keys open the Opening menus")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 4;
    app.TestEnterOpening();
    const wordstartui::cMenuBar& menu = app.TestGetOpeningMenu();
    REQUIRE(menu.IsOpen() == false);

    SUBCASE("F10 opens File and Escape closes it")
    {
        CHECK(app.TestHandleEvent(OpeningFunction(10)) == true);
        CHECK(menu.GetOpenMenu() == 0);
        CHECK(app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ESCAPE)) == true);
        CHECK(menu.IsOpen() == false);
        CHECK(app.TestQuitRequested() == false);
        CHECK(app.TestInOpening() == true);
    }

    SUBCASE("plain F, U, A, H open their menus")
    {
        const char* letters[] = {"f", "u", "a", "h"};
        for (int index = 0; index < 4; ++index)
        {
            INFO("letter: ", letters[index]);
            CHECK(app.TestHandleEvent(OpeningText(letters[index])) == true);
            CHECK(menu.GetOpenMenu() == index);
            app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ESCAPE));
            CHECK(menu.IsOpen() == false);
        }
        CHECK(app.TestQuitRequested() == false);
    }

    SUBCASE("upper-case letters open their menus")
    {
        CHECK(app.TestHandleEvent(OpeningText("U")) == true);
        CHECK(menu.GetOpenMenu() == 1);
    }

    SUBCASE("Alt+F, U, A, H open their menus")
    {
        const char* letters[] = {"f", "u", "a", "h"};
        for (int index = 0; index < 4; ++index)
        {
            INFO("Alt+", letters[index]);
            CHECK(app.TestHandleEvent(OpeningAltText(letters[index])) == true);
            CHECK(menu.GetOpenMenu() == index);
            app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ESCAPE));
        }
    }

    SUBCASE("Left and Right move between menus")
    {
        app.TestHandleEvent(OpeningText("f"));
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_RIGHT));
        CHECK(menu.GetOpenMenu() == 1);
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_RIGHT));
        CHECK(menu.GetOpenMenu() == 2);
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_LEFT));
        CHECK(menu.GetOpenMenu() == 1);
        CHECK(app.TestOpeningSelection() == 0);
    }

    SUBCASE("other classic letters still run their actions")
    {
        CHECK(app.TestHandleEvent(OpeningText("d")) == true);
        CHECK(menu.IsOpen() == false);
        CHECK(app.TestOpeningBrowserFocus() == true);
    }

    SUBCASE("plain letters go to the file list when it has focus")
    {
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_TAB));
        REQUIRE(app.TestOpeningBrowserFocus() == true);
        app.TestHandleEvent(OpeningText("f"));
        CHECK(menu.IsOpen() == false);
        CHECK(app.TestHandleEvent(OpeningFunction(10)) == true);
        CHECK(menu.GetOpenMenu() == 0);
    }
}


TEST_CASE("TUI Opening screen - help level 3 keys do not open the Opening menus")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 3;
    app.TestEnterOpening();
    const wordstartui::cMenuBar& menu = app.TestGetOpeningMenu();

    for (const char* letter : {"f", "u", "h"})
    {
        INFO("letter: ", letter);
        app.TestHandleEvent(OpeningText(letter));
        CHECK(menu.IsOpen() == false);
        app.TestHandleEvent(OpeningAltText(letter));
        CHECK(menu.IsOpen() == false);
    }
    app.TestHandleEvent(OpeningFunction(10));
    CHECK(menu.IsOpen() == false);
    CHECK(app.TestInOpening() == true);
    CHECK(app.TestQuitRequested() == false);
    CHECK(app.TestOpeningBrowserFocus() == false);
}


TEST_CASE("TUI menu walk - every enabled Opening menu entry resolves to a real target")
{
    cWSWordTsar app;
    const std::vector<wordstartui::sMenuItem>& items = app.TestGetOpeningMenu().GetItems();
    REQUIRE(items.size() == 4);

    for (const wordstartui::sMenuItem& item : items)
    {
        INFO("Opening menu: ", item.title);
        CHECK(item.entries.empty() == false);
        CheckEntries(item.entries, "Opening " + item.title);
    }
}


namespace
{

struct sExpectedEntry
{
    const char* title;      // nullptr marks a separator
    const char* shortcut;
    bool enabled;
};

void CheckMenuContents(const wordstartui::sMenuItem& item, const std::vector<sExpectedEntry>& expected)
{
    INFO("Opening menu: ", item.title);
    REQUIRE(item.entries.size() == expected.size());
    for (size_t index = 0; index < expected.size(); ++index)
    {
        const wordstartui::sMenuEntry& entry = item.entries[index];
        INFO("entry ", index);
        if (expected[index].title == nullptr)
        {
            CHECK(entry.isSeparator == true);
            continue;
        }
        CHECK(entry.isSeparator == false);
        CHECK(entry.title == expected[index].title);
        CHECK(entry.shortcut == expected[index].shortcut);
        CHECK(entry.enabled == expected[index].enabled);
        CHECK(entry.submenu.empty() == true);
    }
}

}


TEST_CASE("TUI Opening screen - Opening menu contents")
{
    cWSWordTsar app;
    const std::vector<wordstartui::sMenuItem>& items = app.TestGetOpeningMenu().GetItems();
    REQUIRE(items.size() == 4);

    CheckMenuContents(items[0], {
        {"New", "S", true},
        {"Open Document...", "D", true},
        {"Open Nondocument...", "N", false},
        {"Recent Files...", "", true},
        {nullptr, "", false},
        {"Print...", "P", true},
        {"Print from Keyboard...", "K", false},
        {"Fax...", "\\", false},
        {nullptr, "", false},
        {"Copy...", "O", false},
        {"Delete...", "Y", false},
        {"Rename...", "E", false},
        {"Protect/Unprotect...", "C", false},
        {nullptr, "", false},
        {"Change Drive/Directory...", "L", true},
        {"Change Filename Display...", "", false},
        {"Run DOS Command...", "R", false},
        {"Status", "?", true},
        {nullptr, "", false},
        {"Exit WordTsar", "X", true},
    });

    CheckMenuContents(items[1], {
        {"Index...", "I", true},
        {"TOC...", "T", true},
        {"Macros", "\xe2\x96\xba", false},
        {nullptr, "", false},
        {"System Preferences...", "", true},
    });

    CheckMenuContents(items[2], {
        {"MailList", "AM", false},
        {"TelMerge", "AT", false},
        {"Star Exchange", "AS", false},
    });

    REQUIRE(items[3].entries.size() == 4);
    CHECK(items[3].entries[0].title == "Help");
    CHECK(items[3].entries[0].shortcut == "F1");
    CHECK(items[3].entries[1].title == "Change Help Level...");
    CHECK(items[3].entries[2].isSeparator == true);
    CHECK(items[3].entries[3].title == "About WordTsar");
    CHECK(items[3].entries[3].enabled == true);

    SUBCASE("hotkey letters follow the reference")
    {
        std::string keys;
        for (const wordstartui::sMenuEntry& entry : items[0].entries)
        {
            if (entry.isSeparator == false)
            {
                keys.push_back(entry.hotKey);
            }
        }
        CHECK(keys == "wdnipkfotecvhrux");
    }
}


TEST_CASE("TUI Opening screen - Opening menu entries run the panel actions")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 4;
    app.TestEnterOpening();
    const wordstartui::cMenuBar& menu = app.TestGetOpeningMenu();

    SUBCASE("File > New starts a new document")
    {
        app.TestHandleEvent(OpeningText("f"));
        app.TestHandleEvent(OpeningText("w"));
        CHECK(menu.IsOpen() == false);
        CHECK(app.TestInEditor() == true);
        CHECK(app.TestFilename().empty() == true);
    }

    SUBCASE("File > Open Document opens the file list")
    {
        app.TestHandleEvent(OpeningText("f"));
        app.TestHandleEvent(OpeningText("d"));
        CHECK(menu.IsOpen() == false);
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == false);
    }

    SUBCASE("File > Print opens the file list to print")
    {
        app.TestHandleEvent(OpeningText("f"));
        app.TestHandleEvent(OpeningText("p"));
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == true);
    }

    SUBCASE("File > Change Drive/Directory opens the file list")
    {
        app.TestHandleEvent(OpeningText("f"));
        app.TestHandleEvent(OpeningText("v"));
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantPrint() == false);
    }

    SUBCASE("File > Exit WordTsar exits")
    {
        app.TestHandleEvent(OpeningText("f"));
        app.TestHandleEvent(OpeningText("x"));
        CHECK(app.TestQuitRequested() == true);
    }

    SUBCASE("Utilities > Index opens the file list to index")
    {
        app.TestHandleEvent(OpeningText("u"));
        app.TestHandleEvent(OpeningText("i"));
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantIndex() == true);
    }

    SUBCASE("Utilities > TOC opens the file list for a table of contents")
    {
        app.TestHandleEvent(OpeningText("u"));
        app.TestHandleEvent(OpeningText("t"));
        CHECK(app.TestOpeningBrowserFocus() == true);
        CHECK(app.TestOpeningWantTOC() == true);
    }

    SUBCASE("greyed File entries do nothing")
    {
        app.TestHandleEvent(OpeningText("f"));
        for (const char* letter : {"n", "k", "f", "o", "t", "e", "c", "h", "r"})
        {
            INFO("letter: ", letter);
            CHECK(app.TestHandleEvent(OpeningText(letter)) == true);
            CHECK(menu.GetOpenMenu() == 0);
        }
        CHECK(app.TestOpeningBrowserFocus() == false);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestQuitRequested() == false);
    }

    SUBCASE("Enter on a greyed Additional entry does nothing")
    {
        app.TestHandleEvent(OpeningText("a"));
        REQUIRE(menu.GetOpenMenu() == 2);
        CHECK(app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ENTER)) == true);
        CHECK(menu.GetOpenMenu() == 2);
        CHECK(app.TestOpeningBrowserFocus() == false);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestQuitRequested() == false);
    }
}


namespace
{

/////////////////////////////////////////////////////////////////////////////
///
/// @brief Dialog host that feeds queued events to modal dialogs, cancels
///        when the queue is empty, and counts the dialogs it runs.
///
/////////////////////////////////////////////////////////////////////////////
class cCountingHost : public iWSDialogHost
{
public:
    void QueueText(const std::string& text)
    {
        wordstartui::sInputEvent event{};
        event.type = wordstartui::INPUT_TYPE_TEXT;
        event.textUtf8 = text;
        mEvents.push_back(event);
    }

    void QueueEnter(void)
    {
        wordstartui::sInputEvent event{};
        event.type = wordstartui::INPUT_TYPE_SPECIAL;
        event.special = wordstartui::SPECIAL_KEY_ENTER;
        mEvents.push_back(event);
    }

    void HostRunModal(wordstartui::cDialog& dialog) override
    {
        mModals = mModals + 1;
        dialog.FocusFirst();
        while (dialog.GetResult() == wordstartui::DIALOG_RESULT_NONE)
        {
            if (mEvents.empty() == true)
            {
                dialog.SetResult(wordstartui::DIALOG_RESULT_CANCEL);
                break;
            }
            dialog.HandleEvent(mEvents.front());
            mEvents.pop_front();
        }
    }

    void HostRunModalRaw(
        const std::function<void(wordstartui::cScreen&, const wordstartui::cTheme&)>& draw,
        const std::function<bool(const wordstartui::sInputEvent&)>& handle) override
    {
        (void)draw;
        (void)handle;
        mModals = mModals + 1;
    }

    const wordstartui::cTheme& HostTheme(void) override
    {
        return mTheme;
    }

    wordstartui::sTerminalSize HostScreenSize(void) override
    {
        wordstartui::sTerminalSize size;
        size.rows = 25;
        size.cols = 80;
        return size;
    }

    wordstartui::sTerminalCapabilities HostCapabilities(void) override
    {
        wordstartui::sTerminalCapabilities caps{};
        return caps;
    }

    void HostShowLoadProgress(const std::string& message, int percent) override
    {
        (void)message;
        (void)percent;
    }

    void HostHideLoadProgress(void) override
    {
    }

    int mModals = 0;

private:
    std::deque<wordstartui::sInputEvent> mEvents;
    wordstartui::cTheme mTheme;
};

}


TEST_CASE("TUI help level - F1 F1 prompts for a level below 4 only")
{
    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    cCountingHost host;
    editor->SetDialogHost(&host);
    cWordStarInput input(editor);

    SUBCASE("level 4")
    {
        editor->mHelpLevel = 4;
        input.HandleSpecialKey(SPECIAL_F1);
        input.HandleSpecialKey(SPECIAL_F1);
        CHECK(host.mModals == 0);
        CHECK(editor->mHelpLevel == 4);
    }

    SUBCASE("level 3")
    {
        editor->mHelpLevel = 3;
        input.HandleSpecialKey(SPECIAL_F1);
        input.HandleSpecialKey(SPECIAL_F1);
        CHECK(host.mModals == 1);
        CHECK(editor->mHelpLevel == 3);
    }
}


TEST_CASE("TUI Opening screen - Help menu entries")
{
    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    cCountingHost host;
    editor->SetDialogHost(&host);
    editor->mHelpLevel = 4;
    app.TestEnterOpening();

    const std::vector<wordstartui::sMenuItem>& items = app.TestGetOpeningMenu().GetItems();
    REQUIRE(items.size() == 4);
    REQUIRE(items[3].entries.size() == 4);
    CHECK(items[3].entries[0].enabled == true);
    CHECK(static_cast<bool>(items[3].entries[0].callback) == true);
    CHECK(items[3].entries[1].enabled == true);

    SUBCASE("Change Help Level sets the level")
    {
        host.QueueText("3");
        host.QueueEnter();
        app.TestHandleEvent(OpeningText("h"));
        app.TestHandleEvent(OpeningText("c"));
        CHECK(host.mModals == 1);
        CHECK(editor->mHelpLevel == 3);
        CHECK(app.TestGetOpeningMenu().IsOpen() == false);
        CHECK(app.TestInOpening() == true);
    }
}


TEST_CASE("TUI edit screen - Help menu has Help and Change Help Level")
{
    cWSWordTsar app;
    app.TestBuildMenus();
    const std::vector<wordstartui::sMenuItem>& items = app.TestGetMenu().GetItems();
    REQUIRE(items.empty() == false);
    const wordstartui::sMenuItem& help = items.back();
    REQUIRE(help.title == "Help");
    REQUIRE(help.entries.size() == 4);
    CHECK(help.entries[0].title == "Help");
    CHECK(help.entries[0].shortcut == "F1");
    CHECK(help.entries[0].enabled == true);
    CHECK(help.entries[1].title == "Change Help Level...");
    CHECK(help.entries[1].enabled == true);
    CHECK(help.entries[2].isSeparator == true);
    CHECK(help.entries[3].title == "About WordTsar");

    SUBCASE("Change Help Level sets the level")
    {
        cWSEditorCtrl* editor = app.TestGetEditor();
        cCountingHost host;
        editor->SetDialogHost(&host);
        editor->mHelpLevel = 3;
        app.TestEnterOpening();
        app.TestHandleEvent(OpeningText("s"));
        REQUIRE(app.TestInEditor() == true);

        host.QueueText("4");
        host.QueueEnter();
        app.TestHandleEvent(OpeningAltText("h"));
        app.TestHandleEvent(OpeningText("c"));
        CHECK(host.mModals == 1);
        CHECK(editor->mHelpLevel == 4);
    }
}


namespace
{

// A scratch directory removed again when the test ends.
struct sBrowserDir
{
    std::filesystem::path path;

    explicit sBrowserDir(const std::string& tag)
    {
        const long long stamp = static_cast<long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        path = std::filesystem::temp_directory_path() /
               ("wt-browser-" + tag + "-" + std::to_string(stamp));
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }

    ~sBrowserDir(void)
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }

    void Folder(const std::string& name) const
    {
        std::filesystem::create_directories(path / name);
    }

    void File(const std::string& name, size_t bytes) const
    {
        std::ofstream out(path / name, std::ios::binary);
        out << std::string(bytes, 'x');
    }
};

// Cells [col, col + count) of a row, joined.
std::string CellRun(const wordstartui::cScreen& screen, int row, int col, int count)
{
    std::string text;
    for (int index = 0; index < count; ++index)
    {
        const wordstartui::sCell* cell = screen.GetCell(row, col + index);
        text += (cell != nullptr) ? cell->textUtf8 : std::string("?");
    }
    return text;
}

wordstartui::sRect BrowserRect(int row, int rows)
{
    wordstartui::sRect rect{};
    rect.row = row;
    rect.col = 0;
    rect.rows = rows;
    rect.cols = 80;
    return rect;
}

wordstartui::sInputEvent BrowserClick(int row, int col)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_MOUSE;
    event.mouseAction = wordstartui::MOUSE_ACTION_PRESS;
    event.mouseButton = wordstartui::MOUSE_BUTTON_LEFT;
    event.mouseRow = row;
    event.mouseCol = col;
    return event;
}

const char* const kBar = "\xe2\x94\x82";   // U+2502

}


TEST_CASE("TUI file browser - size text")
{
    CHECK(wsui::cFileBrowser::FormatSize(6) == ".0k");
    CHECK(wsui::cFileBrowser::FormatSize(57) == ".1k");
    CHECK(wsui::cFileBrowser::FormatSize(307) == ".3k");
    CHECK(wsui::cFileBrowser::FormatSize(838) == ".8k");
    CHECK(wsui::cFileBrowser::FormatSize(1024) == "1.0k");
    CHECK(wsui::cFileBrowser::FormatSize(1664) == "1.7k");
    CHECK(wsui::cFileBrowser::FormatSize(4872) == "4.9k");
    CHECK(wsui::cFileBrowser::FormatSize(9949) == "9.9k");
    CHECK(wsui::cFileBrowser::FormatSize(9950) == "10k");
    CHECK(wsui::cFileBrowser::FormatSize(13696) == "14k");
    CHECK(wsui::cFileBrowser::FormatSize(41856) == "42k");
    CHECK(wsui::cFileBrowser::FormatSize(327403) == "327k");
    CHECK(wsui::cFileBrowser::FormatSize(999499) == "999k");
    CHECK(wsui::cFileBrowser::FormatSize(999500) == "1.0M");
    CHECK(wsui::cFileBrowser::FormatSize(1234567) == "1.2M");
    CHECK(wsui::cFileBrowser::FormatSize(999949999) == "999.9M");
    CHECK(wsui::cFileBrowser::FormatSize(999950000) == "1.0G");
    CHECK(wsui::cFileBrowser::FormatSize(1100000000ULL) == "1.1G");
}


TEST_CASE("TUI file browser - a long name keeps its extension")
{
    CHECK(wsui::cFileBrowser::ShortenName("short.txt", 12) == "short.txt");
    CHECK(wsui::cFileBrowser::ShortenName("ABCDEFGHIJKLMNOPQRSTUVWXYZ.txt", 12) == "ABCDEFG\xe2\x80\xa6.txt");
    CHECK(wsui::cFileBrowser::ShortenName("abcdefghijklmnopqrstuvwxyz0123", 15) == "abcdefghijklmn\xe2\x80\xa6");
    CHECK(wsui::cFileBrowser::ShortenName("name.averylongextension", 12) == "name.averyl\xe2\x80\xa6");
}


TEST_CASE("TUI file browser - WordStar layout fills each row across four columns")
{
    sBrowserDir dir("layout");
    dir.Folder("alpha");
    dir.Folder("beta");
    dir.File("big.dat", 327403);
    dir.File("small.txt", 307);
    dir.File("tiny.bat", 6);
    dir.File("zeta.ws", 1664);

    wsui::cFileBrowser browser;
    browser.SetWordStarLayout(true);
    browser.SetDirectory(dir.path.string());
    browser.SetBounds(BrowserRect(8, 16));

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    wordstartui::cTheme theme;
    browser.Draw(screen, theme, false);

    // Row 0: "..", alpha, beta, big.dat.
    CHECK(CellRun(screen, 8, 0, 1) == " ");
    CHECK(CellRun(screen, 8, 1, 3) == ".. ");
    CHECK(CellRun(screen, 8, 17, 1) == "/");
    CHECK(CellRun(screen, 8, 18, 1) == " ");
    CHECK(CellRun(screen, 8, 19, 1) == kBar);
    CHECK(CellRun(screen, 8, 21, 6) == "alpha ");
    CHECK(CellRun(screen, 8, 37, 1) == "/");
    CHECK(CellRun(screen, 8, 39, 1) == kBar);
    CHECK(CellRun(screen, 8, 41, 5) == "beta ");
    CHECK(CellRun(screen, 8, 57, 1) == "/");
    CHECK(CellRun(screen, 8, 59, 1) == kBar);
    CHECK(CellRun(screen, 8, 61, 7) == "big.dat");
    CHECK(CellRun(screen, 8, 73, 5) == " 327k");
    CHECK(CellRun(screen, 8, 78, 2) == "  ");

    // Row 1: the remaining files, sizes ending at column 17 + 20k.
    CHECK(CellRun(screen, 9, 1, 9) == "small.txt");
    CHECK(CellRun(screen, 9, 14, 4) == " .3k");
    CHECK(CellRun(screen, 9, 19, 1) == kBar);
    CHECK(CellRun(screen, 9, 21, 8) == "tiny.bat");
    CHECK(CellRun(screen, 9, 34, 4) == " .0k");
    CHECK(CellRun(screen, 9, 41, 7) == "zeta.ws");
    CHECK(CellRun(screen, 9, 54, 4) == "1.7k");

    SUBCASE("long names are cut before the extension")
    {
        sBrowserDir longDir("long");
        longDir.Folder("abcdefghijklmnopqrstuvwxyz0123");
        longDir.File("ABCDEFGHIJKLMNOPQRSTUVWXYZ.txt", 1664);
        browser.SetDirectory(longDir.path.string());
        browser.Draw(screen, theme, false);

        CHECK(CellRun(screen, 8, 21, 16) == "abcdefghijklmn\xe2\x80\xa6 ");
        CHECK(CellRun(screen, 8, 37, 1) == "/");
        CHECK(CellRun(screen, 8, 41, 13) == "ABCDEFG\xe2\x80\xa6.txt ");
        CHECK(CellRun(screen, 8, 54, 4) == "1.7k");
    }
}


TEST_CASE("TUI file browser - the Open and Save layout is unchanged")
{
    // "..", two folders and ten files: 13 entries, 6 columns of 12, 3 rows.
    sBrowserDir dir("classic");
    dir.Folder("alpha");
    dir.Folder("beta");
    for (int index = 0; index < 10; ++index)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "f%02d.txt", index);
        dir.File(name, 100);
    }

    wsui::cFileBrowser browser;
    browser.SetDirectory(dir.path.string());
    browser.SetBounds(BrowserRect(8, 3));

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    wordstartui::cTheme theme;
    browser.Draw(screen, theme, false);

    // Column-major: entries fill down each column first, names at column 0.
    CHECK(CellRun(screen, 8, 0, 3) == "../");
    CHECK(CellRun(screen, 9, 0, 6) == "alpha/");
    CHECK(CellRun(screen, 10, 0, 5) == "beta/");
    CHECK(CellRun(screen, 8, 12, 7) == "f00.txt");
    CHECK(CellRun(screen, 9, 12, 7) == "f01.txt");
    CHECK(CellRun(screen, 8, 24, 7) == "f03.txt");
    CHECK(CellRun(screen, 8, 19, 1) != kBar);

    browser.HandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_DOWN));
    CHECK(browser.GetSelectedName() == "alpha/");
    browser.HandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_RIGHT));
    CHECK(browser.GetSelectedName() == "f01.txt");
}


TEST_CASE("TUI file browser - WordStar layout navigation")
{
    // "..", d1-d3 and f00-f25: 30 entries in 8 rows of 4; 3 rows visible.
    sBrowserDir dir("nav");
    dir.Folder("d1");
    dir.Folder("d2");
    dir.Folder("d3");
    for (int index = 0; index < 26; ++index)
    {
        char name[16];
        std::snprintf(name, sizeof(name), "f%02d.txt", index);
        dir.File(name, 100);
    }

    wsui::cFileBrowser browser;
    browser.SetWordStarLayout(true);
    browser.SetDirectory(dir.path.string());
    browser.SetBounds(BrowserRect(10, 3));

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    wordstartui::cTheme theme;

    auto key = [&](wordstartui::eSpecialKey special)
    {
        return browser.HandleEvent(OpeningSpecial(special));
    };

    REQUIRE(browser.GetSelectedName() == "../");

    SUBCASE("arrow keys move across and down the rows")
    {
        key(wordstartui::SPECIAL_KEY_ARROW_RIGHT);
        CHECK(browser.GetSelectedName() == "d1/");
        key(wordstartui::SPECIAL_KEY_ARROW_DOWN);
        CHECK(browser.GetSelectedName() == "f01.txt");
        key(wordstartui::SPECIAL_KEY_ARROW_LEFT);
        CHECK(browser.GetSelectedName() == "f00.txt");
        key(wordstartui::SPECIAL_KEY_ARROW_UP);
        CHECK(browser.GetSelectedName() == "../");
        CHECK(key(wordstartui::SPECIAL_KEY_ARROW_UP) == wsui::cFileBrowser::ACTION_EXIT_TOP);
        key(wordstartui::SPECIAL_KEY_ARROW_LEFT);
        CHECK(browser.GetSelectedName() == "../");
    }

    SUBCASE("Page Down and Page Up move a screen of rows and scroll")
    {
        key(wordstartui::SPECIAL_KEY_PAGE_DOWN);
        CHECK(browser.GetSelectedName() == "f08.txt");
        browser.Draw(screen, theme, true);
        CHECK(CellRun(screen, 12, 1, 7) == "f08.txt");
        CHECK(CellRun(screen, 10, 1, 7) == "f00.txt");

        key(wordstartui::SPECIAL_KEY_PAGE_DOWN);
        CHECK(browser.GetSelectedName() == "f20.txt");
        key(wordstartui::SPECIAL_KEY_PAGE_DOWN);
        CHECK(browser.GetSelectedName() == "f24.txt");
        key(wordstartui::SPECIAL_KEY_PAGE_UP);
        CHECK(browser.GetSelectedName() == "f12.txt");
        key(wordstartui::SPECIAL_KEY_PAGE_UP);
        CHECK(browser.GetSelectedName() == "f00.txt");
        key(wordstartui::SPECIAL_KEY_PAGE_UP);
        CHECK(browser.GetSelectedName() == "../");
    }

    SUBCASE("Home and End go to the first and last entry and scroll")
    {
        key(wordstartui::SPECIAL_KEY_END);
        CHECK(browser.GetSelectedName() == "f25.txt");
        browser.Draw(screen, theme, true);
        CHECK(CellRun(screen, 12, 21, 7) == "f25.txt");
        CHECK(CellRun(screen, 12, 1, 7) == "f24.txt");

        key(wordstartui::SPECIAL_KEY_HOME);
        CHECK(browser.GetSelectedName() == "../");
        browser.Draw(screen, theme, true);
        CHECK(CellRun(screen, 10, 1, 2) == "..");
    }

    SUBCASE("a click picks the entry under the mouse")
    {
        browser.Draw(screen, theme, true);
        CHECK(browser.HandleEvent(BrowserClick(11, 45)) == wsui::cFileBrowser::ACTION_FILE);
        CHECK(std::filesystem::path(browser.GetSelectedFile()).filename() == "f02.txt");
    }

    SUBCASE("a click after scrolling picks the scrolled entry")
    {
        key(wordstartui::SPECIAL_KEY_END);
        browser.Draw(screen, theme, true);
        CHECK(browser.HandleEvent(BrowserClick(10, 1)) == wsui::cFileBrowser::ACTION_FILE);
        CHECK(std::filesystem::path(browser.GetSelectedFile()).filename() == "f16.txt");
    }

    SUBCASE("a typed letter jumps to the next name")
    {
        browser.HandleEvent(OpeningText("f"));
        CHECK(browser.GetSelectedName() == "f00.txt");
        browser.HandleEvent(OpeningText("d"));
        CHECK(browser.GetSelectedName() == "d1/");
    }
}


TEST_CASE("TUI Opening screen - the file list uses the WordStar layout")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 4;
    app.TestEnterOpening();

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    CHECK(CellRun(screen, 8, 0, 4) == " .. ");
    CHECK(CellRun(screen, 8, 17, 1) == "/");
    CHECK(CellRun(screen, 8, 19, 1) == kBar);
    CHECK(CellRun(screen, 8, 39, 1) == kBar);
    CHECK(CellRun(screen, 8, 59, 1) == kBar);
}


TEST_CASE("TUI file browser - free space text")
{
    CHECK(wsui::cFileBrowser::FormatFreeSpace(512345678) == "512M");
    CHECK(wsui::cFileBrowser::FormatFreeSpace(999499999) == "999M");
    CHECK(wsui::cFileBrowser::FormatFreeSpace(999500000) == "1.0G");
    CHECK(wsui::cFileBrowser::FormatFreeSpace(2013000000ULL) == "2.0G");
    CHECK(wsui::cFileBrowser::FormatFreeSpace(1234567890123ULL) == "1234.6G");

    CHECK(wsui::cFileBrowser::FreeSpace("/no/such/wordtsar/folder") == "");
    std::error_code ec;
    std::filesystem::space_info space = std::filesystem::space("/", ec);
    REQUIRE(!ec);
    CHECK(wsui::cFileBrowser::FreeSpace("/") == wsui::cFileBrowser::FormatFreeSpace(space.available));
}


namespace
{

// True when every cell of the row holds the glyph.
bool RowIsFilledWith(const wordstartui::cScreen& screen, int row, const std::string& glyph)
{
    for (int col = 0; col < screen.GetCols(); ++col)
    {
        if (CellRun(screen, row, col, 1) != glyph)
        {
            return false;
        }
    }
    return true;
}

const char* const kUpperHalf = "\xe2\x96\x80";   // U+2580
const char* const kLowerHalf = "\xe2\x96\x84";   // U+2584

void CheckOpeningHeader(const wordstartui::cScreen& screen, int row, const std::string& directory)
{
    CHECK(CellRun(screen, row, 0, 11) == " Filenames:");
    std::string path = "Path: " + directory + "  " + wsui::cFileBrowser::FreeSpace(directory) + " free";
    CHECK(CellRun(screen, row, 21, static_cast<int>(path.size())) == path);
    CHECK(CellRun(screen, row, 71, 1) == "\xe2\x96\xb2");   // U+25B2
    CHECK(CellRun(screen, row, 73, 1) == "\xe2\x96\xbc");   // U+25BC
    CHECK(CellRun(screen, row, 75, 1) == "\xe2\x86\x91");   // U+2191
    CHECK(CellRun(screen, row, 77, 1) == "\xe2\x86\x93");   // U+2193
}

}


TEST_CASE("TUI Opening screen - help level 4 header and bars")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 4;
    app.TestEnterOpening();
    app.TestSetOpeningDirectory("/");

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    CHECK(RowIsFilledWith(screen, 6, kUpperHalf) == true);
    CheckOpeningHeader(screen, 7, "/");
    CHECK(CellRun(screen, 8, 1, 2) == "..");
    CHECK(RowIsFilledWith(screen, 24, kLowerHalf) == true);

    SUBCASE("a long path is cut at its start and stops before the arrows")
    {
        sBrowserDir dir("a-folder-with-a-long-name-for-the-path-line");
        app.TestSetOpeningDirectory(dir.path.string());
        app.TestDrawOpening(screen);

        std::string row = CellRun(screen, 7, 21, 50);
        CHECK(row.rfind("Path: \xe2\x80\xa6", 0) == 0);
        CHECK(row.find(" free") != std::string::npos);
        CHECK(CellRun(screen, 7, 70, 2) == " \xe2\x96\xb2");
    }
}


TEST_CASE("TUI Opening screen - help level 3 header and bars")
{
    cWSWordTsar app;
    app.TestGetEditor()->mHelpLevel = 3;
    app.TestEnterOpening();
    app.TestSetOpeningDirectory("/");

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    // Row 13 is a single line, row 14 the header, rows 15 to 23 the list.
    CHECK(RowIsFilledWith(screen, 13, "\xe2\x94\x80") == true);   // U+2500
    CheckOpeningHeader(screen, 14, "/");
    CHECK(CellRun(screen, 15, 1, 2) == "..");
    CHECK(CellRun(screen, 15, 19, 1) == kBar);
    CHECK(RowIsFilledWith(screen, 24, kLowerHalf) == true);
}


namespace
{

wordstartui::cScreen DrawLevel3Opening(cWSWordTsar& app)
{
    app.TestGetEditor()->mHelpLevel = 3;
    app.TestEnterOpening();
    app.TestSetOpeningDirectory("/");

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);
    return screen;
}

}


TEST_CASE("TUI Opening screen - help level 3 C label reads protect/unprotect a file")
{
    cWSWordTsar app;
    wordstartui::cScreen screen = DrawLevel3Opening(app);
    CHECK(ScreenHasText(screen, "C protect/unprotect a file") == true);
}


TEST_CASE("TUI Opening screen - help level 3 own entries sit on row 12 with letters")
{
    cWSWordTsar app;
    wordstartui::cScreen screen = DrawLevel3Opening(app);

    std::string row12 = MenuRowText(screen, 12);
    CHECK(row12.find("G recent files") != std::string::npos);
    CHECK(row12.find("V preferences") != std::string::npos);
    CHECK(ScreenHasText(screen, "  recent files") == false);
    CHECK(ScreenHasText(screen, "  preferences") == false);

    eOpeningAction action{};
    CHECK(app.TestOpeningKeyAction('g', action) == true);
    CHECK(action == eOpeningAction::RECENT_FILES);
    CHECK(app.TestOpeningKeyAction('V', action) == true);
    CHECK(action == eOpeningAction::PREFERENCES);

    SUBCASE("the right column ends with display status")
    {
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_RIGHT));
        for (int step = 0; step < 9; ++step)
        {
            app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_DOWN));
        }
        REQUIRE(app.TestOpeningSelection() == 18);
        app.TestHandleEvent(OpeningSpecial(wordstartui::SPECIAL_KEY_ARROW_DOWN));
        CHECK(app.TestOpeningBrowserFocus() == true);
    }

    SUBCASE("help level 4 leaves G and V unbound")
    {
        app.TestGetEditor()->mHelpLevel = 4;
        CHECK(app.TestOpeningKeyAction('g', action) == false);
        CHECK(app.TestOpeningKeyAction('v', action) == false);
    }
}


TEST_CASE("TUI Opening screen - help level 3 A is additional and W is about WordTsar")
{
    cWSWordTsar app;
    wordstartui::cScreen screen = DrawLevel3Opening(app);

    CHECK(ScreenHasText(screen, "A additional") == true);
    CHECK(ScreenHasText(screen, "A about") == false);
    CHECK(MenuRowText(screen, 12).find("W about WordTsar") != std::string::npos);

    eOpeningAction action{};
    CHECK(app.TestOpeningKeyAction('w', action) == true);
    CHECK(action == eOpeningAction::ABOUT);

    // The Additional programs are not available: A does nothing.
    REQUIRE(app.TestOpeningKeyAction('a', action) == false);
    CHECK(app.TestHandleEvent(OpeningText("a")) == true);
    CHECK(app.TestInOpening() == true);
    CHECK(app.TestOpeningBrowserFocus() == false);
    CHECK(app.TestOpeningSelection() == 0);
    CHECK(app.TestQuitRequested() == false);

    SUBCASE("help level 4 leaves W unbound")
    {
        app.TestGetEditor()->mHelpLevel = 4;
        CHECK(app.TestOpeningKeyAction('w', action) == false);
    }
}


TEST_CASE("TUI Opening screen - help level 3 columns follow the WordStar layout")
{
    cWSWordTsar app;
    wordstartui::cScreen screen = DrawLevel3Opening(app);

    // Letters at columns 6 and 43, labels at 8 and 45, F1 at 5 and 6.
    CHECK(CellRun(screen, 2, 4, 19) == "  D open a document");
    CHECK(CellRun(screen, 10, 4, 17) == "  X exit WordTsar");
    CHECK(CellRun(screen, 11, 4, 8) == " F1 help");
    CHECK(CellRun(screen, 2, 41, 26) == "  L change drive/directory");
    CHECK(CellRun(screen, 10, 41, 14) == "  A additional");
    CHECK(CellRun(screen, 11, 41, 18) == "  ? display status");
}


TEST_CASE("TUI Opening screen - help level 3 title row and box")
{
    cWSWordTsar app;
    wordstartui::cScreen screen = DrawLevel3Opening(app);
    const std::string line = "\xe2\x94\x80";   // U+2500
    const std::string side = "\xe2\x94\x82";   // U+2502

    // Row 0: the name centred in a fill of upper half blocks.
    CHECK(CellRun(screen, 0, 0, 1) == kUpperHalf);
    CHECK(CellRun(screen, 0, 35, 10) == " WordTsar ");
    CHECK(CellRun(screen, 0, 79, 1) == kUpperHalf);

    // Row 1: top border with the menu name in it.
    CHECK(CellRun(screen, 1, 0, 1) == "\xe2\x94\x8c");    // U+250C
    CHECK(CellRun(screen, 1, 1, 1) == line);
    CHECK(CellRun(screen, 1, 26, 27) == line + " O P E N I N G   M E N U " + line);
    CHECK(CellRun(screen, 1, 79, 1) == "\xe2\x94\x90");   // U+2510

    for (int row = 2; row <= 11; ++row)
    {
        INFO("row ", row);
        CHECK(CellRun(screen, row, 0, 1) == side);
        CHECK(CellRun(screen, row, 79, 1) == side);
    }

    // Row 12: bottom border carrying WordTsar's own entries.
    CHECK(CellRun(screen, 12, 0, 2) == "\xe2\x94\x94" + line);   // U+2514
    CHECK(CellRun(screen, 12, 78, 2) == line + "\xe2\x94\x98");  // U+2518
    std::string bottom = MenuRowText(screen, 12);
    CHECK(bottom.find(line + " G recent files " + line + line + " V preferences " +
                      line + line + " W about WordTsar " + line) != std::string::npos);
}



namespace
{

wordstartui::sInputEvent MenuAlt(char letter)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_TEXT;
    event.alt = true;
    event.textUtf8 = std::string(1, letter);
    return event;
}

// Draw a copy of the menu bar on row 1 of a 25x80 screen with one menu open.
wordstartui::cScreen DrawOpenMenu(wordstartui::cMenuBar menu, char letter)
{
    wordstartui::sRect bounds{};
    bounds.row = 1;
    bounds.col = 0;
    bounds.rows = 1;
    bounds.cols = 80;
    menu.SetBounds(bounds);
    menu.HandleEvent(MenuAlt(letter));
    REQUIRE(menu.IsOpen() == true);

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    wordstartui::cTheme theme;
    menu.Draw(screen, theme);
    return screen;
}

}


TEST_CASE("TUI menu bar - a shortcut is placed by its display width")
{
    wordstartui::cMenuBar menu;
    int util = menu.AddMenu("Utilities", 'u');
    menu.AddSubItem(util, "&Index...  I", [](void) {});
    menu.AddSubItem(util, "&Macros  \xe2\x96\xba", [](void) {});   // U+25BA

    wordstartui::cScreen screen = DrawOpenMenu(menu, 'u');

    // Box columns 1 to 15; both shortcuts end directly before the border.
    CHECK(CellRun(screen, 2, 15, 1) == "\xe2\x94\x90");
    CHECK(CellRun(screen, 3, 13, 3) == " I\xe2\x94\x82");
    CHECK(CellRun(screen, 4, 13, 3) == " \xe2\x96\xba\xe2\x94\x82");
}


TEST_CASE("TUI Opening screen - pull-down boxes follow the WordStar layout")
{
    cWSWordTsar app;
    const wordstartui::cMenuBar& opening = app.TestGetOpeningMenu();
    const std::string corner = "\xe2\x94\x8c";   // U+250C
    const std::string side = "\xe2\x94\x82";     // U+2502

    SUBCASE("File")
    {
        wordstartui::cScreen screen = DrawOpenMenu(opening, 'f');
        CHECK(CellRun(screen, 2, 0, 1) == corner);
        CHECK(CellRun(screen, 2, 31, 1) == "\xe2\x94\x90");
        CHECK(CellRun(screen, 3, 0, 5) == side + " New");
        CHECK(CellRun(screen, 3, 28, 4) == " S " + side);
        CHECK(CellRun(screen, 17, 28, 4) == " L " + side);
        CHECK(CellRun(screen, 18, 2, 26) == "Change Filename Display...");
        CHECK(CellRun(screen, 18, 28, 4) == "   " + side);
        CHECK(CellRun(screen, 22, 28, 4) == " X " + side);
    }

    SUBCASE("Utilities")
    {
        wordstartui::cScreen screen = DrawOpenMenu(opening, 'u');
        CHECK(CellRun(screen, 2, 7, 1) == corner);
        CHECK(CellRun(screen, 2, 33, 1) == "\xe2\x94\x90");
        CHECK(CellRun(screen, 3, 9, 8) == "Index...");
        CHECK(CellRun(screen, 3, 30, 4) == " I " + side);
        CHECK(CellRun(screen, 4, 30, 4) == " T " + side);
        CHECK(CellRun(screen, 5, 30, 4) == " \xe2\x96\xba " + side);
        CHECK(CellRun(screen, 7, 9, 21) == "System Preferences...");
    }

    SUBCASE("Additional")
    {
        wordstartui::cScreen screen = DrawOpenMenu(opening, 'a');
        CHECK(CellRun(screen, 2, 19, 1) == corner);
        CHECK(CellRun(screen, 2, 38, 1) == "\xe2\x94\x90");
        CHECK(CellRun(screen, 3, 21, 8) == "MailList");
        CHECK(CellRun(screen, 3, 34, 5) == " AM " + side);
        CHECK(CellRun(screen, 5, 21, 13) == "Star Exchange");
        CHECK(CellRun(screen, 5, 34, 5) == " AS " + side);
    }

    SUBCASE("Help")
    {
        wordstartui::cScreen screen = DrawOpenMenu(opening, 'h');
        CHECK(CellRun(screen, 2, 53, 1) == corner);
        CHECK(CellRun(screen, 2, 79, 1) == "\xe2\x94\x90");
        CHECK(CellRun(screen, 3, 55, 4) == "Help");
        CHECK(CellRun(screen, 3, 75, 5) == " F1 " + side);
    }
}


TEST_CASE("TUI menu bar - edit-screen pull-downs keep the shortcut at the border")
{
    cWSWordTsar app;
    app.TestBuildMenus();
    const wordstartui::cMenuBar& menu = app.TestGetMenu();
    const std::vector<wordstartui::sMenuItem>& items = menu.GetItems();

    for (const wordstartui::sMenuItem& item : items)
    {
        if (item.entries.empty() == true)
        {
            continue;
        }
        INFO("menu: ", item.title);
        wordstartui::cScreen screen = DrawOpenMenu(menu, item.hotKey);

        // The box opens at the title's column unless it is pushed left by
        // the right edge of the screen.
        int left = -1;
        int right = -1;
        for (int col = 0; col < 80; ++col)
        {
            if (CellRun(screen, 2, col, 1) == "\xe2\x94\x8c")
            {
                left = col;
            }
            if (CellRun(screen, 2, col, 1) == "\xe2\x94\x90")
            {
                right = col;
            }
        }
        REQUIRE(left >= 0);
        REQUIRE(right > left);
        const int titleCol = static_cast<int>(MenuRowText(screen, 1).find(item.title));
        CHECK(((left == titleCol) || (right == 79)) == true);

        for (size_t index = 0; index < item.entries.size(); ++index)
        {
            const wordstartui::sMenuEntry& entry = item.entries[index];
            if (entry.shortcut.empty() == true)
            {
                continue;
            }
            const int width = static_cast<int>(entry.shortcut.size());
            CHECK(CellRun(screen, 3 + static_cast<int>(index), right - width, width) == entry.shortcut);
        }
    }
}


namespace
{

wordstartui::cScreen DrawOpenMenuWithTheme(wordstartui::cMenuBar menu, char letter, const wordstartui::cTheme& theme)
{
    wordstartui::sRect bounds{};
    bounds.row = 1;
    bounds.col = 0;
    bounds.rows = 1;
    bounds.cols = 80;
    menu.SetBounds(bounds);
    menu.HandleEvent(MenuAlt(letter));
    REQUIRE(menu.IsOpen() == true);

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    menu.Draw(screen, theme);
    return screen;
}

const wordstartui::sCell& CellAt(const wordstartui::cScreen& screen, int row, int col)
{
    const wordstartui::sCell* cell = screen.GetCell(row, col);
    REQUIRE(cell != nullptr);
    return *cell;
}

}


TEST_CASE("TUI menu bar - the highlighted entry's hotkey uses the highlighted hotkey colours")
{
    wordstartui::sThemePalette palette = wordstartui::cTheme::GetDefaultPalette();
    palette.menuFg = wordstartui::MakeRgb(0, 0, 0);
    palette.menuBg = wordstartui::MakeRgb(170, 170, 170);
    palette.menuAccelFg = wordstartui::MakeRgb(85, 85, 255);
    palette.menuAccelBg = wordstartui::MakeRgb(170, 170, 170);
    palette.menuActiveFg = wordstartui::MakeRgb(170, 170, 170);
    palette.menuActiveBg = wordstartui::MakeRgb(0, 0, 0);
    palette.menuActiveAccelFg = wordstartui::MakeRgb(11, 12, 13);
    palette.menuActiveAccelBg = wordstartui::MakeRgb(14, 15, 16);
    wordstartui::cTheme theme;
    theme.LoadPalette(palette);

    wordstartui::cMenuBar menu;
    int file = menu.AddMenu("File", 'f');
    menu.AddSubItem(file, "&Alpha", [](void) {});
    menu.AddSubItem(file, "&Beta", [](void) {});

    wordstartui::cScreen screen = DrawOpenMenuWithTheme(menu, 'f', theme);

    // Highlighted entry on row 3: its letter in the new pair, the rest in the
    // menu highlight colours.
    const wordstartui::sCell& hot = CellAt(screen, 3, 3);
    CHECK(hot.textUtf8 == "A");
    CHECK(hot.style.fg == wordstartui::MakeRgb(11, 12, 13));
    CHECK(hot.style.bg == wordstartui::MakeRgb(14, 15, 16));
    const wordstartui::sCell& rest = CellAt(screen, 3, 4);
    CHECK(rest.style.fg == wordstartui::MakeRgb(170, 170, 170));
    CHECK(rest.style.bg == wordstartui::MakeRgb(0, 0, 0));

    // Other entry on row 4: its letter in the accelerator colour on the menu.
    const wordstartui::sCell& other = CellAt(screen, 4, 3);
    CHECK(other.textUtf8 == "B");
    CHECK(other.style.fg == wordstartui::MakeRgb(85, 85, 255));
    CHECK(other.style.bg == wordstartui::MakeRgb(170, 170, 170));
}


TEST_CASE("TUI Opening screen - WordStar 7 colours for the highlighted hotkey in a pull-down")
{
    cWSWordTsar app;
    cConfig config;
    config.ApplyTUIPalette(cConfig::GetTUIPalettes().back());
    app.TestApplyConfig(config);
    app.TestGetEditor()->mHelpLevel = 4;
    app.TestEnterOpening();
    REQUIRE(app.TestHandleEvent(OpeningText("f")) == true);
    REQUIRE(app.TestGetOpeningMenu().IsOpen() == true);

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    // "New" is highlighted; its hotkey letter "w" sits at column 4.
    const wordstartui::sCell& hot = CellAt(screen, 3, 4);
    CHECK(hot.textUtf8 == "w");
    CHECK(hot.style.fg == wordstartui::MakeRgb(170, 170, 170));
    CHECK(hot.style.bg == wordstartui::MakeRgb(0, 0, 170));
}


namespace
{

const wordstartui::sColor kWs7Black = wordstartui::MakeRgb(0, 0, 0);
const wordstartui::sColor kWs7Blue = wordstartui::MakeRgb(0, 0, 170);
const wordstartui::sColor kWs7Grey = wordstartui::MakeRgb(170, 170, 170);
const wordstartui::sColor kWs7LightBlue = wordstartui::MakeRgb(85, 85, 255);

// An app in the WordStar 7 colour preset on the Opening screen.
void EnterWs7Opening(cWSWordTsar& app, int helpLevel)
{
    cConfig config;
    config.ApplyTUIPalette(cConfig::GetTUIPalettes().back());
    REQUIRE(config.mTuiMenuHighlightAcceleratorBackground.b == 170);
    app.TestApplyConfig(config);
    app.TestGetEditor()->mHelpLevel = helpLevel;
    app.TestEnterOpening();
}

void CheckCellColours(const wordstartui::cScreen& screen, int row, int col,
                      const wordstartui::sColor& fg, const wordstartui::sColor& bg)
{
    const wordstartui::sCell& cell = CellAt(screen, row, col);
    INFO("cell ", row, ",", col, " \"", cell.textUtf8, "\"");
    CHECK(cell.style.fg == fg);
    CHECK(cell.style.bg == bg);
}

}


TEST_CASE("TUI Opening screen - help level 4 uses the palette colours")
{
    cWSWordTsar app;
    EnterWs7Opening(app, 4);

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    // Row 0: title bar colours.
    CheckCellColours(screen, 0, 0, kWs7Grey, kWs7Black);
    CheckCellColours(screen, 0, 37, kWs7Grey, kWs7Black);

    // Row 1: menu bar, hotkey letters in the accelerator colour.
    CheckCellColours(screen, 1, 1, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 1, 2, kWs7Black, kWs7Grey);
    CheckCellColours(screen, 1, 40, kWs7Black, kWs7Grey);

    // Rows 2 to 5: the editor background as an empty strip.
    for (int row = 2; row <= 5; ++row)
    {
        CHECK(CellAt(screen, row, 0).style.bg == kWs7Blue);
        CHECK(CellAt(screen, row, 79).style.bg == kWs7Blue);
    }

    // Rows 6 and 24: half-block bars in the title bar background on the panel.
    CHECK(CellAt(screen, 6, 10).textUtf8 == "\xe2\x96\x80");
    CheckCellColours(screen, 6, 10, kWs7Black, kWs7Grey);
    CHECK(CellAt(screen, 24, 10).textUtf8 == "\xe2\x96\x84");
    CheckCellColours(screen, 24, 10, kWs7Black, kWs7Grey);

    // Row 7 and the file list: black on grey.
    CheckCellColours(screen, 7, 1, kWs7Black, kWs7Grey);
    CheckCellColours(screen, 7, 18, kWs7Black, kWs7Grey);
    CheckCellColours(screen, 7, 71, kWs7Black, kWs7Grey);
    CheckCellColours(screen, 8, 1, kWs7Black, kWs7Grey);
    CheckCellColours(screen, 23, 79, kWs7Black, kWs7Grey);
}


TEST_CASE("TUI Opening screen - help level 4 open menu title")
{
    cWSWordTsar app;
    EnterWs7Opening(app, 4);
    REQUIRE(app.TestHandleEvent(OpeningText("u")) == true);
    REQUIRE(app.TestGetOpeningMenu().IsOpen() == true);

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    // Utilities sits at columns 8 to 16; the highlight covers 7 to 17.
    CheckCellColours(screen, 1, 6, kWs7Black, kWs7Grey);
    CheckCellColours(screen, 1, 7, kWs7Grey, kWs7Black);
    CHECK(CellAt(screen, 1, 8).textUtf8 == "U");
    CheckCellColours(screen, 1, 8, kWs7Grey, kWs7Blue);
    CheckCellColours(screen, 1, 9, kWs7Grey, kWs7Black);
    CheckCellColours(screen, 1, 17, kWs7Grey, kWs7Black);
    CheckCellColours(screen, 1, 18, kWs7Black, kWs7Grey);

    // The pull-down: black on grey with light blue hotkeys below the
    // highlighted first entry.
    CHECK(CellAt(screen, 4, 9).textUtf8 == "T");
    CheckCellColours(screen, 4, 9, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 4, 10, kWs7Black, kWs7Grey);
}


TEST_CASE("TUI Opening screen - help level 3 uses the palette colours")
{
    cWSWordTsar app;
    EnterWs7Opening(app, 3);

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDrawOpening(screen);

    // Row 0: title bar colours.
    CheckCellColours(screen, 0, 37, kWs7Grey, kWs7Black);

    // The box: black lines on grey, the menu name in the accelerator colour.
    CHECK(CellAt(screen, 1, 0).textUtf8 == "\xe2\x94\x8c");
    CheckCellColours(screen, 1, 0, kWs7Black, kWs7Grey);
    CHECK(CellAt(screen, 1, 28).textUtf8 == "O");
    CheckCellColours(screen, 1, 28, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 5, 79, kWs7Black, kWs7Grey);
    CheckCellColours(screen, 5, 30, kWs7Black, kWs7Grey);

    // Letters in the accelerator colour, labels black on grey.
    CHECK(CellAt(screen, 3, 6).textUtf8 == "S");
    CheckCellColours(screen, 3, 6, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 3, 8, kWs7Black, kWs7Grey);
    CHECK(CellAt(screen, 2, 43).textUtf8 == "L");
    CheckCellColours(screen, 2, 43, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 2, 45, kWs7Black, kWs7Grey);
    CHECK(CellAt(screen, 11, 5).textUtf8 == "F");
    CheckCellColours(screen, 11, 5, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 11, 6, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 11, 8, kWs7Black, kWs7Grey);

    // An entry without an Opening action keeps the panel colours.
    CHECK(CellAt(screen, 4, 6).textUtf8 == "N");
    CheckCellColours(screen, 4, 6, kWs7LightBlue, kWs7Grey);
    CHECK(CellAt(screen, 4, 8).style.bg == kWs7Grey);

    // The selected entry: menu highlight colours, its letter in the
    // highlighted hotkey colours.
    REQUIRE(app.TestOpeningSelection() == 0);
    CHECK(CellAt(screen, 2, 6).textUtf8 == "D");
    CheckCellColours(screen, 2, 6, kWs7Grey, kWs7Blue);
    CheckCellColours(screen, 2, 4, kWs7Grey, kWs7Black);
    CheckCellColours(screen, 2, 8, kWs7Grey, kWs7Black);

    // WordTsar's own entries in the bottom border.
    std::string bottom = MenuRowText(screen, 12);
    size_t recent = bottom.find(" G recent files ");
    REQUIRE(recent != std::string::npos);
    int letterCol = 0;
    {
        // Column of "G" counted in cells.
        int col = 0;
        for (; col < 80; ++col)
        {
            if ((CellAt(screen, 12, col).textUtf8 == "G") && (CellAt(screen, 12, col + 2).textUtf8 == "r"))
            {
                break;
            }
        }
        letterCol = col;
    }
    REQUIRE(letterCol < 80);
    CheckCellColours(screen, 12, letterCol, kWs7LightBlue, kWs7Grey);
    CheckCellColours(screen, 12, letterCol + 2, kWs7Black, kWs7Grey);
}


namespace
{

// True when no cell in rows first to last carries the bold attribute.
bool NoBoldInRows(const wordstartui::cScreen& screen, int first, int last)
{
    bool clean = true;
    for (int row = first; row <= last; ++row)
    {
        for (int col = 0; col < 80; ++col)
        {
            const wordstartui::sCell& cell = CellAt(screen, row, col);
            if ((cell.style.attrs & wordstartui::CELL_ATTR_BOLD) != 0U)
            {
                INFO("bold cell ", row, ",", col, " \"", cell.textUtf8, "\"");
                CHECK(false);
                clean = false;
            }
        }
    }
    return clean;
}

}


TEST_CASE("TUI menu bar - menu titles, highlighted entries and hotkeys carry no bold")
{
    cWSWordTsar app;
    app.TestBuildMenus();
    const wordstartui::cMenuBar& menu = app.TestGetMenu();

    for (const wordstartui::sMenuItem& item : menu.GetItems())
    {
        if (item.entries.empty() == true)
        {
            continue;
        }
        INFO("edit-screen menu: ", item.title);
        wordstartui::cScreen screen = DrawOpenMenu(menu, item.hotKey);
        CHECK(NoBoldInRows(screen, 1, 24) == true);
    }

    for (const wordstartui::sMenuItem& item : app.TestGetOpeningMenu().GetItems())
    {
        INFO("Opening menu: ", item.title);
        wordstartui::cScreen screen = DrawOpenMenu(app.TestGetOpeningMenu(), item.hotKey);
        CHECK(NoBoldInRows(screen, 1, 24) == true);
    }
}


TEST_CASE("TUI menu bar - the edit-screen open title is highlighted one column wider each side")
{
    wordstartui::sThemePalette palette = wordstartui::cTheme::GetDefaultPalette();
    palette.menuFg = wordstartui::MakeRgb(0, 0, 0);
    palette.menuBg = wordstartui::MakeRgb(170, 170, 170);
    palette.menuAccelFg = wordstartui::MakeRgb(85, 85, 255);
    palette.menuAccelBg = wordstartui::MakeRgb(170, 170, 170);
    palette.menuActiveFg = wordstartui::MakeRgb(170, 170, 170);
    palette.menuActiveBg = wordstartui::MakeRgb(0, 0, 0);
    palette.menuActiveAccelFg = wordstartui::MakeRgb(11, 12, 13);
    palette.menuActiveAccelBg = wordstartui::MakeRgb(14, 15, 16);
    wordstartui::cTheme theme;
    theme.LoadPalette(palette);

    cWSWordTsar app;
    app.TestBuildMenus();
    wordstartui::cScreen screen = DrawOpenMenuWithTheme(app.TestGetMenu(), 'u', theme);

    // Utilities sits at columns 42 to 50; the highlight covers 41 to 51 and
    // the hotkey letter U is in the highlighted hotkey colours.
    const int titleCol = static_cast<int>(MenuRowText(screen, 1).find("Utilities"));
    REQUIRE(titleCol == 42);
    CheckCellColours(screen, 1, 40, wordstartui::MakeRgb(0, 0, 0), wordstartui::MakeRgb(170, 170, 170));
    CheckCellColours(screen, 1, 41, wordstartui::MakeRgb(170, 170, 170), wordstartui::MakeRgb(0, 0, 0));
    CHECK(CellAt(screen, 1, 42).textUtf8 == "U");
    CheckCellColours(screen, 1, 42, wordstartui::MakeRgb(11, 12, 13), wordstartui::MakeRgb(14, 15, 16));
    CheckCellColours(screen, 1, 43, wordstartui::MakeRgb(170, 170, 170), wordstartui::MakeRgb(0, 0, 0));
    CheckCellColours(screen, 1, 51, wordstartui::MakeRgb(170, 170, 170), wordstartui::MakeRgb(0, 0, 0));
    CheckCellColours(screen, 1, 52, wordstartui::MakeRgb(0, 0, 0), wordstartui::MakeRgb(170, 170, 170));

    // The highlighted first entry: its hotkey letter in the highlighted
    // hotkey colours, the rest of the bar in the menu highlight colours.
    CHECK(CellAt(screen, 3, 44).textUtf8 == "S");
    CheckCellColours(screen, 3, 44, wordstartui::MakeRgb(11, 12, 13), wordstartui::MakeRgb(14, 15, 16));
    CheckCellColours(screen, 3, 43, wordstartui::MakeRgb(170, 170, 170), wordstartui::MakeRgb(0, 0, 0));
    CheckCellColours(screen, 3, 45, wordstartui::MakeRgb(170, 170, 170), wordstartui::MakeRgb(0, 0, 0));
}
