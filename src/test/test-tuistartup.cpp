//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
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
#include "src/tui/dialogs/dialoghost.h"
#include "src/tui/dialogs/dialogs.h"
#include "src/tui/wordstartui/src/dialog.h"
#include "src/tui/wordstartui/src/terminaldriverposix.h"
#include "src/core/utils/abouttext.h"
#include "src/core/utils/config.h"
#include "src/core/include/version.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>


namespace
{

const wordstartui::sColor kBlue = wordstartui::MakeRgb(0, 0, 170);
const wordstartui::sColor kGrey = wordstartui::MakeRgb(170, 170, 170);
const char* const kUpperHalf = "\xe2\x96\x80";   // U+2580
const char* const kSquare = "\xe2\x96\xa0";      // U+25A0

abouttext::sSystemInfo FixedSystem(void)
{
    abouttext::sSystemInfo info;
    info.macosVersion = "15.5";
    info.chip = "Apple M2";
    info.terminal = "Apple_Terminal";
    return info;
}

// An app in the WordStar 7 colour preset with fixed system values.
void PrepareWs7(cWSWordTsar& app)
{
    cConfig config;
    config.ApplyTUIPalette(cConfig::GetTUIPalettes().back());
    REQUIRE(cConfig::GetTUIPalettes().back().name == "WordStar 7");
    app.TestApplyConfig(config);
    app.TestSetSystemInfo(FixedSystem());
}

std::string RowText(const wordstartui::cScreen& screen, int row)
{
    std::string text;
    for (int col = 0; col < screen.GetCols(); ++col)
    {
        text += screen.GetCell(row, col)->textUtf8;
    }
    return text;
}

std::string Trimmed(const std::string& text)
{
    size_t first = text.find_first_not_of(' ');
    if (first == std::string::npos)
    {
        return std::string();
    }
    return text.substr(first, text.find_last_not_of(' ') - first + 1);
}

// Column where text starts on the row, -1 when absent.
int TextCol(const wordstartui::cScreen& screen, int row, const std::string& text)
{
    for (int col = 0; col < screen.GetCols(); ++col)
    {
        std::string rest;
        for (int at = col; at < screen.GetCols(); ++at)
        {
            rest += screen.GetCell(row, at)->textUtf8;
        }
        if (rest.compare(0, text.size(), text) == 0)
        {
            return col;
        }
    }
    return -1;
}

bool IsBar(const wordstartui::cScreen& screen, int row, int col)
{
    return screen.GetCell(row, col)->textUtf8 == kUpperHalf;
}

bool IsSeparator(const wordstartui::cScreen& screen, int row, int col)
{
    return screen.GetCell(row, col)->textUtf8 == kSquare;
}

// Expected rows of the startup screen for one terminal width.
void CheckLayout(int rows, int cols)
{
    CAPTURE(rows);
    CAPTURE(cols);
    cWSWordTsar app;
    PrepareWs7(app);
    wordstartui::cScreen screen;
    screen.Resize(rows, cols);
    app.TestDrawStartup(screen);

    // Row 0: half-block bar with the name centred, one blank either side.
    const int nameCol = (cols - 8) / 2;
    CHECK(TextCol(screen, 0, " WordTsar ") == nameCol - 1);
    CHECK(IsBar(screen, 0, 0) == true);
    CHECK(IsBar(screen, 0, cols - 1) == true);
    CHECK(IsBar(screen, 0, nameCol - 2) == true);
    CHECK(IsBar(screen, 0, nameCol + 9) == true);

    std::vector<std::string> product = abouttext::ProductLines();
    CHECK(Trimmed(RowText(screen, 1)).empty());
    for (int index = 0; index < 4; ++index)
    {
        const std::string& line = product[static_cast<size_t>(index)];
        CHECK(TextCol(screen, 2 + index, line) == (cols - static_cast<int>(line.size())) / 2);
    }
    CHECK(Trimmed(RowText(screen, 6)).empty());

    // Separators: 70 % of the width, centred, black squares.
    const int sepWidth = cols * 7 / 10;
    const int sepCol = (cols - sepWidth) / 2;
    for (int row : {7, 11})
    {
        CAPTURE(row);
        CHECK(IsSeparator(screen, row, sepCol - 1) == false);
        CHECK(IsSeparator(screen, row, sepCol) == true);
        CHECK(IsSeparator(screen, row, sepCol + sepWidth - 1) == true);
        CHECK(IsSeparator(screen, row, sepCol + sepWidth) == false);
    }

    // System block left-aligned three columns inside the separator.
    std::vector<std::string> system = abouttext::SystemLines(FixedSystem(), cols, rows);
    CHECK(TextCol(screen, 8, system[0]) == sepCol + 3);
    CHECK(TextCol(screen, 9, "Printing to PDF") == sepCol + 3);
    CHECK(Trimmed(RowText(screen, 10)).empty());
    CHECK(Trimmed(RowText(screen, 12)).empty());

    std::vector<std::string> technology = abouttext::TechnologyLines();
    for (int index = 0; index < 3; ++index)
    {
        const std::string& line = technology[static_cast<size_t>(index)];
        CHECK(TextCol(screen, 13 + index, line) == (cols - static_cast<int>(line.size())) / 2);
    }
    CHECK(Trimmed(RowText(screen, 16)).empty());

    std::vector<std::string> legal = abouttext::LegalLines();
    for (int index = 0; index < 5; ++index)
    {
        const std::string& line = legal[static_cast<size_t>(index)];
        CHECK(TextCol(screen, 17 + index, line) == (cols - static_cast<int>(line.size())) / 2);
    }
    for (int row = 22; row < rows; ++row)
    {
        CAPTURE(row);
        CHECK(Trimmed(RowText(screen, row)).empty());
    }

    // Colours: bars in the title bar foreground on the body background,
    // everything else grey on blue.
    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const wordstartui::sCell* cell = screen.GetCell(row, col);
            INFO("cell ", row, ",", col, " \"", cell->textUtf8, "\"");
            CHECK(cell->style.bg == kBlue);
            CHECK(cell->style.fg == kGrey);
        }
    }
}

}


TEST_CASE("TUI startup screen - WordStar 7 layout and colours at 80x25")
{
    CheckLayout(25, 80);
}


TEST_CASE("TUI startup screen - WordStar 7 layout and colours at 120x40")
{
    CheckLayout(40, 120);
}


TEST_CASE("TUI startup screen - a 60x20 terminal wraps long lines and drops the blocks that do not fit")
{
    cWSWordTsar app;
    PrepareWs7(app);
    wordstartui::cScreen screen;
    screen.Resize(20, 60);
    app.TestDrawStartup(screen);

    std::string all;
    for (int row = 0; row < 20; ++row)
    {
        std::string text = RowText(screen, row);
        all += text + "\n";
        // One blank column stays on each side.
        if (row > 0)
        {
            CHECK(screen.GetCell(row, 0)->textUtf8 == " ");
            CHECK(screen.GetCell(row, 59)->textUtf8 == " ");
        }
    }
    CHECK(TextCol(screen, 0, " WordTsar ") == 25);
    CHECK(TextCol(screen, 2, std::string("WordTsar for macOS Version ") + FULLVERSION_STRING) >= 1);
    CHECK(TextCol(screen, 3, "macOS edition by Egbert H. Schroeer, based on WordTsar by") >= 1);
    CHECK(TextCol(screen, 4, "Gerald Brandt") >= 1);
    CHECK(TextCol(screen, 5, "Copyright (C) 2018 Gerald Brandt") >= 1);
    CHECK(all.find("Printing to PDF") != std::string::npos);
    CHECK(all.find("Open-source components: see Help > About WordTsar") != std::string::npos);
    // The legal block needs rows 19 to 23 and is left out.
    CHECK(all.find("WARRANTY") == std::string::npos);

    for (int row = 0; row < 20; ++row)
    {
        for (int col = 0; col < 60; ++col)
        {
            CHECK(screen.GetCell(row, col)->style.bg == kBlue);
        }
    }
}


TEST_CASE("TUI startup screen - tiny terminals draw without crashing")
{
    for (int rows : {1, 2, 3, 5, 8, 12})
    {
        for (int cols : {1, 2, 5, 10, 20, 40})
        {
            CAPTURE(rows);
            CAPTURE(cols);
            cWSWordTsar app;
            PrepareWs7(app);
            wordstartui::cScreen screen;
            screen.Resize(rows, cols);
            app.TestDrawStartup(screen);
            CHECK(screen.GetCell(rows - 1, cols - 1)->style.bg == kBlue);
        }
    }
}


namespace
{

// A unique temporary folder, removed with its contents.
class cStartupTempDir
{
public:
    cStartupTempDir(void)
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-startup-XXXXXX").string();
        REQUIRE(mkdtemp(pattern.data()) != nullptr);
        mPath = pattern;
    }

    ~cStartupTempDir(void)
    {
        std::error_code ec;
        std::filesystem::remove_all(mPath, ec);
    }

    std::string WriteText(const std::string& name, const std::string& text)
    {
        std::filesystem::path file = mPath / name;
        std::ofstream out(file, std::ios::binary);
        out << text;
        return file.string();
    }

private:
    std::filesystem::path mPath;
};

wordstartui::sInputEvent Text(const std::string& text, bool alt = false)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_TEXT;
    event.textUtf8 = text;
    event.alt = alt;
    return event;
}

wordstartui::sInputEvent Special(wordstartui::eSpecialKey key)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_SPECIAL;
    event.special = key;
    return event;
}

wordstartui::sInputEvent Function(int number)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_FUNCTION;
    event.functionKey = number;
    return event;
}

wordstartui::sInputEvent Mouse(wordstartui::eMouseAction action, int row, int col)
{
    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_MOUSE;
    event.mouseAction = action;
    event.mouseButton = wordstartui::MOUSE_BUTTON_LEFT;
    event.mouseRow = row;
    event.mouseCol = col;
    return event;
}

// An app started without a file name, showing the startup screen at help
// level 4, drawn once.
void StartShowing(cWSWordTsar& app, wordstartui::cScreen& screen)
{
    PrepareWs7(app);
    app.TestGetEditor()->mHelpLevel = 4;
    app.TestCountModals();
    char name[] = "ws";
    char* argv[] = { name, nullptr };
    app.TestStartSession(1, argv);
    REQUIRE(app.TestInStartup() == true);
    screen.Resize(25, 80);
    app.TestDraw(screen);
    REQUIRE(app.TestInStartup() == true);
}

}


TEST_CASE("TUI startup screen - a key pressed during the startup screen reaches the Opening screen")
{
    cWSWordTsar app;
    wordstartui::cScreen screen;
    StartShowing(app, screen);

    SUBCASE("S starts the editor")
    {
        CHECK(app.TestHandleEvent(Text("s")) == true);
        CHECK(app.TestInEditor() == true);
        CHECK(app.TestFilename().empty() == true);
    }

    SUBCASE("D gives the file list the letters")
    {
        CHECK(app.TestHandleEvent(Text("d")) == true);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestOpeningBrowserFocus() == true);
    }

    SUBCASE("F1 shows the Opening help")
    {
        CHECK(app.TestHandleEvent(Function(1)) == true);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestModalCount() == 1);
    }

    SUBCASE("Escape only ends the startup screen; a second Escape does nothing")
    {
        CHECK(app.TestHandleEvent(Special(wordstartui::SPECIAL_KEY_ESCAPE)) == true);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestQuitRequested() == false);
        CHECK(app.TestOpeningBrowserFocus() == false);
        CHECK(app.TestHandleEvent(Special(wordstartui::SPECIAL_KEY_ESCAPE)) == true);
        CHECK(app.TestQuitRequested() == false);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestOpeningBrowserFocus() == false);
    }

    SUBCASE("Alt+F opens the File menu")
    {
        CHECK(app.TestHandleEvent(Text("f", true)) == true);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestGetOpeningMenu().GetOpenMenu() == 0);
    }

    SUBCASE("a mouse click ends the startup screen and is consumed")
    {
        // Row 9 column 5 is in the help level 4 file list.
        CHECK(app.TestHandleEvent(Mouse(wordstartui::MOUSE_ACTION_PRESS, 9, 5)) == true);
        CHECK(app.TestInOpening() == true);
        CHECK(app.TestOpeningBrowserFocus() == false);
        CHECK(app.TestGetOpeningMenu().IsOpen() == false);
        CHECK(app.TestQuitRequested() == false);
        CHECK(app.TestModalCount() == 0);
    }

    SUBCASE("mouse releases, drags and wheel turns are ignored")
    {
        for (wordstartui::eMouseAction action : {wordstartui::MOUSE_ACTION_RELEASE, wordstartui::MOUSE_ACTION_DRAG,
                                                 wordstartui::MOUSE_ACTION_WHEEL, wordstartui::MOUSE_ACTION_NONE})
        {
            CHECK(app.TestHandleEvent(Mouse(action, 9, 5)) == true);
            CHECK(app.TestInStartup() == true);
        }
    }
}


TEST_CASE("TUI startup screen - shown for 1.5 seconds from the first frame")
{
    cWSWordTsar app;
    CHECK(app.TestStartupDuration() == std::chrono::milliseconds(1500));

    PrepareWs7(app);
    char name[] = "ws";
    char* argv[] = { name, nullptr };
    app.TestStartSession(1, argv);
    REQUIRE(app.TestInStartup() == true);

    SUBCASE("the clock starts with the first frame")
    {
        app.TestSetStartupDuration(std::chrono::milliseconds(0));
        app.TestTickStartup();
        CHECK(app.TestInStartup() == true);
        wordstartui::cScreen screen;
        screen.Resize(25, 80);
        app.TestDraw(screen);
        app.TestTickStartup();
        CHECK(app.TestInOpening() == true);
    }

    SUBCASE("it stays until the duration has passed, then the Opening screen follows")
    {
        app.TestSetStartupDuration(std::chrono::milliseconds(200));
        wordstartui::cScreen screen;
        screen.Resize(25, 80);
        app.TestDraw(screen);
        app.TestTickStartup();
        CHECK(app.TestInStartup() == true);
        usleep(250 * 1000);
        app.TestDraw(screen);
        app.TestTickStartup();
        CHECK(app.TestInOpening() == true);
    }
}


TEST_CASE("TUI startup screen - Show startup screen off goes straight on")
{
    cStartupTempDir dir;
    std::string file = dir.WriteText("letter.txt", "dear reader\n");

    cConfig config;
    config.mTuiShowStartupScreen = false;

    SUBCASE("to the Opening screen")
    {
        cWSWordTsar app;
        app.TestApplyConfig(config);
        app.TestSetSystemInfo(FixedSystem());
        char name[] = "ws";
        char* argv[] = { name, nullptr };
        app.TestStartSession(1, argv);
        CHECK(app.TestInOpening() == true);
    }

    SUBCASE("to the editor with the file opened")
    {
        cWSWordTsar app;
        app.TestApplyConfig(config);
        app.TestSetSystemInfo(FixedSystem());
        char name[] = "ws";
        char* argv[] = { name, file.data(), nullptr };
        app.TestStartSession(2, argv);
        CHECK(app.TestInEditor() == true);
        CHECK(app.TestGetEditor()->mFileName == "letter.txt");
    }
}


TEST_CASE("TUI startup screen - a file name on the command line opens after the startup screen")
{
    cStartupTempDir dir;
    std::string file = dir.WriteText("letter.txt", "dear reader\n");

    cWSWordTsar app;
    PrepareWs7(app);
    char name[] = "ws";
    char* argv[] = { name, file.data(), nullptr };
    app.TestStartSession(2, argv);
    CHECK(app.TestInStartup() == true);
    CHECK(app.TestGetEditor()->mFileName != "letter.txt");

    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDraw(screen);
    CHECK(TextCol(screen, 0, " WordTsar ") == 35);
    CHECK(app.TestGetEditor()->mFileName != "letter.txt");

    app.TestSetStartupDuration(std::chrono::milliseconds(0));
    app.TestTickStartup();
    CHECK(app.TestInEditor() == true);
    CHECK(app.TestGetEditor()->mFileName == "letter.txt");
}


TEST_CASE("TUI About box - shared text with the tagline first and the open-source components")
{
    cWSWordTsar app;
    std::string message = app.TestAboutMessage();
    CHECK(message == abouttext::AboutText());
    CHECK(message.rfind("A WordStar 7 clone for the 21st century.\n", 0) == 0);
    CHECK(message.find("Wordstar") == std::string::npos);
    CHECK(message.find(abouttext::TrademarkText()) != std::string::npos);
    for (const char* component : {"pugixml", "SimpleIni", "cpp-unicodelib", "PicoMath", "zip", "miniz", "chillout"})
    {
        INFO("component: ", component);
        CHECK(message.find(component) != std::string::npos);
    }

    app.TestCountModals();
    app.TestEnterOpening();
    app.TestGetEditor()->mHelpLevel = 3;
    CHECK(app.TestHandleEvent(Text("w")) == true);
    CHECK(app.TestModalCount() == 1);
}


namespace
{

// Dialog host that draws a modal dialog, feeds it a list of events and
// keeps a copy of the screen after the first draw and after each event.
class cScriptHost : public iWSDialogHost
{
public:
    cScriptHost(int rows, int cols, const std::vector<wordstartui::sInputEvent>& events)
        : mRows(rows), mCols(cols), mEvents(events)
    {
    }

    void HostRunModal(wordstartui::cDialog& dialog) override
    {
        dialog.FocusFirst();
        Snapshot([&](wordstartui::cScreen& screen) { dialog.Draw(screen, mTheme); });
        for (const wordstartui::sInputEvent& event : mEvents)
        {
            if (dialog.GetResult() != wordstartui::DIALOG_RESULT_NONE)
            {
                break;
            }
            dialog.HandleEvent(event);
            Snapshot([&](wordstartui::cScreen& screen) { dialog.Draw(screen, mTheme); });
        }
        mResult = dialog.GetResult();
    }

    void HostRunModalRaw(
        const std::function<void(wordstartui::cScreen&, const wordstartui::cTheme&)>& draw,
        const std::function<bool(const wordstartui::sInputEvent&)>& handle) override
    {
        (void)handle;
        Snapshot([&](wordstartui::cScreen& screen) { draw(screen, mTheme); });
    }

    const wordstartui::cTheme& HostTheme(void) override { return mTheme; }

    wordstartui::sTerminalSize HostScreenSize(void) override
    {
        wordstartui::sTerminalSize size{};
        size.rows = mRows;
        size.cols = mCols;
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

    void HostHideLoadProgress(void) override {}

    std::vector<wordstartui::cScreen> mFrames;
    wordstartui::eDialogResult mResult = wordstartui::DIALOG_RESULT_NONE;

private:
    template <typename tDraw>
    void Snapshot(tDraw draw)
    {
        wordstartui::cScreen screen;
        screen.Resize(mRows, mCols);
        draw(screen);
        mFrames.push_back(screen);
    }

    int mRows;
    int mCols;
    std::vector<wordstartui::sInputEvent> mEvents;
    wordstartui::cTheme mTheme;
};

// Box of a dialog on the screen: rows of the top-left and bottom-right corners.
struct sBox
{
    int top = -1;
    int left = -1;
    int bottom = -1;
    int right = -1;
};

sBox FindBox(const wordstartui::cScreen& screen)
{
    sBox box;
    for (int row = 0; row < screen.GetRows(); ++row)
    {
        for (int col = 0; col < screen.GetCols(); ++col)
        {
            const std::string& cell = screen.GetCell(row, col)->textUtf8;
            if ((cell == "┌") && (box.top < 0))
            {
                box.top = row;
                box.left = col;
            }
            if (cell == "┘")
            {
                box.bottom = row;
                box.right = col;
            }
        }
    }
    return box;
}

// Text between the side borders of a box row.
std::string InsideText(const wordstartui::cScreen& screen, const sBox& box, int row)
{
    std::string text;
    for (int col = box.left + 1; col < box.right; ++col)
    {
        text += screen.GetCell(row, col)->textUtf8;
    }
    return text;
}

// Text of a frame inside the box, one line per row.
std::string FrameText(const wordstartui::cScreen& screen)
{
    sBox box = FindBox(screen);
    std::string text;
    for (int row = box.top + 1; row < box.bottom; ++row)
    {
        text += Trimmed(InsideText(screen, box, row)) + "\n";
    }
    return text;
}

// True when the frame shows the end of the About text, the last component
// and its licence; the two may wrap onto separate lines.
bool ShowsLastComponent(const wordstartui::cScreen& screen)
{
    abouttext::sComponent last = abouttext::Components().back();
    std::string text = FrameText(screen);
    return (text.find(last.name) != std::string::npos) &&
           (text.find("(" + last.licence + ").\n") != std::string::npos);
}

// The OK button is the row above the bottom border, alone on its row, and
// the whole box is on the screen.
void CheckAboutFrame(const wordstartui::cScreen& screen)
{
    sBox box = FindBox(screen);
    REQUIRE(box.top >= 0);
    REQUIRE(box.bottom > box.top);
    CHECK(box.left >= 0);
    CHECK(box.right < screen.GetCols());
    CHECK(box.bottom < screen.GetRows());
    CHECK(screen.GetCell(box.bottom, box.left)->textUtf8 == "└");
    CHECK(screen.GetCell(box.top, box.right)->textUtf8 == "┐");
    CHECK(Trimmed(InsideText(screen, box, box.bottom - 1)) == "OK");
    CHECK(Trimmed(InsideText(screen, box, box.bottom - 2)).empty());
}

std::vector<wordstartui::sInputEvent> Repeat(const wordstartui::sInputEvent& event, int count)
{
    return std::vector<wordstartui::sInputEvent>(static_cast<size_t>(count), event);
}

wordstartui::sInputEvent Wheel(int amount, int row, int col)
{
    wordstartui::sInputEvent event = Mouse(wordstartui::MOUSE_ACTION_WHEEL, row, col);
    event.mouseButton = wordstartui::MOUSE_BUTTON_NONE;
    event.mouseWheel = amount;
    return event;
}

}


TEST_CASE("TUI About box - the text scrolls and the OK button stays on screen")
{
    const std::string about = abouttext::AboutText();
    const std::string tagline = abouttext::Tagline();

    for (std::pair<int, int> size : {std::make_pair(25, 80), std::make_pair(20, 60), std::make_pair(40, 120)})
    {
        const int rows = size.first;
        const int cols = size.second;
        CAPTURE(rows);
        CAPTURE(cols);

        // End, then Home, then Down one line at a time, then Enter.
        std::vector<wordstartui::sInputEvent> events;
        events.push_back(Special(wordstartui::SPECIAL_KEY_END));
        events.push_back(Special(wordstartui::SPECIAL_KEY_HOME));
        std::vector<wordstartui::sInputEvent> downs = Repeat(Special(wordstartui::SPECIAL_KEY_ARROW_DOWN), 60);
        events.insert(events.end(), downs.begin(), downs.end());
        events.push_back(Special(wordstartui::SPECIAL_KEY_ENTER));

        cScriptHost host(rows, cols, events);
        wsdialogs::ScrollingMessageBox(&host, "About WordTsar", about);
        REQUIRE(host.mFrames.size() == events.size() + 1);
        CHECK(host.mResult == wordstartui::DIALOG_RESULT_OK);

        std::string all;
        for (const wordstartui::cScreen& frame : host.mFrames)
        {
            CheckAboutFrame(frame);
            all += FrameText(frame);
        }

        // First text line of the box is the tagline.
        sBox box = FindBox(host.mFrames[0]);
        CHECK(Trimmed(InsideText(host.mFrames[0], box, box.top + 2)) == tagline);

        // End shows the last component line, Home the tagline again.
        CHECK(ShowsLastComponent(host.mFrames[1]) == true);
        CHECK(FrameText(host.mFrames[2]).find(tagline) != std::string::npos);

        bool fits = (rows == 40);
        CHECK(ShowsLastComponent(host.mFrames[0]) == fits);

        // Every word of the About text is shown in some frame.
        std::istringstream words(about);
        std::string word;
        while (words >> word)
        {
            INFO("word: ", word);
            CHECK(all.find(word) != std::string::npos);
        }
    }
}


TEST_CASE("TUI About box - Page Down, Page Up and the mouse wheel scroll the text")
{
    const std::string about = abouttext::AboutText();
    const std::string tagline = abouttext::Tagline();

    // Wheel position: the middle of an 80x25 screen is inside the text.
    std::vector<wordstartui::sInputEvent> events = Repeat(Special(wordstartui::SPECIAL_KEY_PAGE_DOWN), 5);
    std::vector<wordstartui::sInputEvent> ups = Repeat(Special(wordstartui::SPECIAL_KEY_PAGE_UP), 5);
    events.insert(events.end(), ups.begin(), ups.end());
    events.push_back(Wheel(3, 12, 40));
    events.push_back(Wheel(-3, 12, 40));
    events.push_back(Special(wordstartui::SPECIAL_KEY_ESCAPE));

    cScriptHost host(25, 80, events);
    wsdialogs::ScrollingMessageBox(&host, "About WordTsar", about);
    REQUIRE(host.mFrames.size() == events.size() + 1);
    CHECK(host.mResult == wordstartui::DIALOG_RESULT_CANCEL);

    CHECK(ShowsLastComponent(host.mFrames[0]) == false);
    CHECK(ShowsLastComponent(host.mFrames[5]) == true);
    CHECK(FrameText(host.mFrames[5]).find(tagline) == std::string::npos);
    CHECK(FrameText(host.mFrames[10]).find(tagline) != std::string::npos);
    CHECK(FrameText(host.mFrames[11]).find(tagline) == std::string::npos);
    CHECK(FrameText(host.mFrames[12]).find(tagline) != std::string::npos);
    for (const wordstartui::cScreen& frame : host.mFrames)
    {
        CheckAboutFrame(frame);
    }
}


TEST_CASE("TUI About box - Help > About WordTsar shows the scrolling box")
{
    cWSWordTsar app;
    app.TestCountModals();
    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestEnterOpening();
    app.TestGetEditor()->mHelpLevel = 3;
    app.TestDraw(screen);
    CHECK(app.TestHandleEvent(Text("w")) == true);
    REQUIRE(app.TestModalCount() == 1);

    const wordstartui::cScreen& modal = app.TestModalScreen();
    REQUIRE(modal.GetRows() == 25);
    CheckAboutFrame(modal);
    sBox box = FindBox(modal);
    CHECK(Trimmed(InsideText(modal, box, box.top + 2)) == abouttext::Tagline());
}


TEST_CASE("TUI startup screen - Escape with a file name ends the startup screen and opens the file")
{
    cStartupTempDir dir;
    std::string file = dir.WriteText("letter.txt", "dear reader\n");

    cWSWordTsar app;
    PrepareWs7(app);
    char name[] = "ws";
    char* argv[] = { name, file.data(), nullptr };
    app.TestStartSession(2, argv);
    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDraw(screen);
    REQUIRE(app.TestInStartup() == true);

    CHECK(app.TestHandleEvent(Special(wordstartui::SPECIAL_KEY_ESCAPE)) == true);
    CHECK(app.TestQuitRequested() == false);
    CHECK(app.TestInEditor() == true);
    CHECK(app.TestGetEditor()->mFileName == "letter.txt");
    CHECK(app.TestGetEditor()->GetDocument()->IsModified() == false);
}


namespace
{

// Events the POSIX driver reads from the given bytes on standard input.
std::vector<wordstartui::sInputEvent> DriverEvents(const std::string& bytes)
{
    int fds[2];
    REQUIRE(pipe(fds) == 0);
    REQUIRE(write(fds[1], bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()));
    close(fds[1]);

    int savedStdin = dup(STDIN_FILENO);
    REQUIRE(savedStdin >= 0);
    REQUIRE(dup2(fds[0], STDIN_FILENO) == STDIN_FILENO);
    close(fds[0]);

    std::vector<wordstartui::sInputEvent> events;
    {
        wordstartui::cTerminalDriverPosix driver;
        for (size_t attempt = 0; attempt < bytes.size() + 2; ++attempt)
        {
            wordstartui::sInputEvent event{};
            if (driver.ReadEvent(event, 10) == true)
            {
                events.push_back(event);
            }
        }
    }

    dup2(savedStdin, STDIN_FILENO);
    close(savedStdin);
    return events;
}

bool IsEscapeEvent(const wordstartui::sInputEvent& event)
{
    return (event.type == wordstartui::INPUT_TYPE_SPECIAL) && (event.special == wordstartui::SPECIAL_KEY_ESCAPE);
}

}


TEST_CASE("TUI terminal driver - an unknown CSI sequence is not an Escape key")
{
    for (const char* sequence : {"\x1b[99~", "\x1b[1;2A", "\x1b[1;5C", "\x1b[Z", "\x1b[200~"})
    {
        INFO("sequence: ESC", std::string(sequence + 1));
        for (const wordstartui::sInputEvent& event : DriverEvents(sequence))
        {
            CHECK(IsEscapeEvent(event) == false);
            CHECK(event.type == wordstartui::INPUT_TYPE_NONE);
        }
    }
}


TEST_CASE("TUI terminal driver - known sequences and a lone Escape still decode")
{
    std::vector<wordstartui::sInputEvent> events = DriverEvents("\x1b[A" "\x1b[99~" "\x1b[5~" "\x1bOP" "x");
    REQUIRE(events.size() == 4);
    CHECK(events[0].type == wordstartui::INPUT_TYPE_SPECIAL);
    CHECK(events[0].special == wordstartui::SPECIAL_KEY_ARROW_UP);
    CHECK(events[1].type == wordstartui::INPUT_TYPE_SPECIAL);
    CHECK(events[1].special == wordstartui::SPECIAL_KEY_PAGE_UP);
    CHECK(events[2].type == wordstartui::INPUT_TYPE_FUNCTION);
    CHECK(events[2].functionKey == 1);
    CHECK(events[3].type == wordstartui::INPUT_TYPE_TEXT);
    CHECK(events[3].textUtf8 == "x");

    std::vector<wordstartui::sInputEvent> escape = DriverEvents("\x1b");
    REQUIRE(escape.size() == 1);
    CHECK(IsEscapeEvent(escape[0]) == true);
}


TEST_CASE("TUI startup screen - an unknown CSI sequence does not exit ws")
{
    cWSWordTsar app;
    wordstartui::cScreen screen;
    StartShowing(app, screen);

    SUBCASE("during the startup screen")
    {
        for (const wordstartui::sInputEvent& event : DriverEvents("\x1b[99~" "\x1b[1;2A"))
        {
            app.TestHandleEvent(event);
        }
        CHECK(app.TestQuitRequested() == false);
        CHECK(app.TestInStartup() == true);
    }

    SUBCASE("on the Opening screen")
    {
        app.TestSetStartupDuration(std::chrono::milliseconds(0));
        app.TestTickStartup();
        REQUIRE(app.TestInOpening() == true);
        for (const wordstartui::sInputEvent& event : DriverEvents("\x1b[99~" "\x1b[1;2A"))
        {
            app.TestHandleEvent(event);
        }
        CHECK(app.TestQuitRequested() == false);
        CHECK(app.TestInOpening() == true);
    }
}


namespace
{

// An app started with a file name, showing the startup screen, drawn once.
std::string StartShowingWithFile(cWSWordTsar& app, cStartupTempDir& dir)
{
    std::string file = dir.WriteText("letter.txt", "dear reader\n");
    PrepareWs7(app);
    app.TestCountModals();
    char name[] = "ws";
    char* argv[] = { name, file.data(), nullptr };
    app.TestStartSession(2, argv);
    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestDraw(screen);
    REQUIRE(app.TestInStartup() == true);
    return file;
}

void CheckLetterUnchanged(cWSWordTsar& app, const std::string& text)
{
    CHECK(app.TestInEditor() == true);
    CHECK(app.TestQuitRequested() == false);
    CHECK(app.TestModalCount() == 0);
    CHECK(app.TestGetEditor()->mFileName == "letter.txt");
    CHECK(app.TestGetEditor()->GetDocument()->IsModified() == false);
    CHECK(app.TestGetEditor()->GetDocument()->GetText() == text);
}

}


TEST_CASE("TUI startup screen - with a file name the keys typed during the startup screen are dropped")
{
    cStartupTempDir dir;
    cWSWordTsar app;
    StartShowingWithFile(app, dir);

    SUBCASE("a letter, Enter or Backspace")
    {
        for (const wordstartui::sInputEvent& event : {Text("x"), Special(wordstartui::SPECIAL_KEY_ENTER),
                                                      Special(wordstartui::SPECIAL_KEY_BACKSPACE)})
        {
            CHECK(app.TestHandleEvent(event) == true);
        }
        REQUIRE(app.TestInEditor() == true);
        std::string text = app.TestGetEditor()->GetDocument()->GetText();
        CHECK(text.find("dear reader") != std::string::npos);
        CheckLetterUnchanged(app, text);
    }

    SUBCASE("each key alone ends the startup screen and is dropped")
    {
        std::string text;
        {
            cStartupTempDir otherDir;
            cWSWordTsar other;
            StartShowingWithFile(other, otherDir);
            other.TestSetStartupDuration(std::chrono::milliseconds(0));
            other.TestTickStartup();
            REQUIRE(other.TestInEditor() == true);
            text = other.TestGetEditor()->GetDocument()->GetText();
        }

        SUBCASE("letter") { CHECK(app.TestHandleEvent(Text("x")) == true); }
        SUBCASE("Enter") { CHECK(app.TestHandleEvent(Special(wordstartui::SPECIAL_KEY_ENTER)) == true); }
        SUBCASE("Backspace") { CHECK(app.TestHandleEvent(Special(wordstartui::SPECIAL_KEY_BACKSPACE)) == true); }
        SUBCASE("Ctrl+Y")
        {
            wordstartui::sInputEvent event{};
            event.type = wordstartui::INPUT_TYPE_CONTROL;
            event.controlCode = 25;
            event.ctrl = true;
            CHECK(app.TestHandleEvent(event) == true);
        }
        SUBCASE("mouse click") { CHECK(app.TestHandleEvent(Mouse(wordstartui::MOUSE_ACTION_PRESS, 10, 10)) == true); }
        CheckLetterUnchanged(app, text);
    }

    SUBCASE("a burst of 20 keys, then the next key after the input went quiet is typed")
    {
        const char* letters = "abcdefghijklmnopqrst";
        for (int index = 0; index < 20; ++index)
        {
            CHECK(app.TestHandleEvent(Text(std::string(1, letters[index]))) == true);
        }
        REQUIRE(app.TestInEditor() == true);
        std::string text = app.TestGetEditor()->GetDocument()->GetText();
        CHECK(text.find("dear reader") != std::string::npos);
        CHECK(text.find("abc") == std::string::npos);
        CHECK(text.find('t') == std::string::npos);
        CheckLetterUnchanged(app, text);

        app.TestIdle();
        CHECK(app.TestHandleEvent(Text("z")) == true);
        CHECK(app.TestGetEditor()->GetDocument()->IsModified() == true);
        CHECK(app.TestGetEditor()->GetDocument()->GetText().find('z') != std::string::npos);
    }

    SUBCASE("keys queued when the startup screen times out are dropped too")
    {
        app.TestSetStartupDuration(std::chrono::milliseconds(0));
        app.TestTickStartup();
        REQUIRE(app.TestInEditor() == true);
        std::string text = app.TestGetEditor()->GetDocument()->GetText();
        CHECK(app.TestHandleEvent(Text("x")) == true);
        CHECK(app.TestHandleEvent(Special(wordstartui::SPECIAL_KEY_ENTER)) == true);
        CheckLetterUnchanged(app, text);
    }
}


TEST_CASE("TUI startup screen - without a file name the next key after the startup screen is not dropped")
{
    cWSWordTsar app;
    wordstartui::cScreen screen;
    StartShowing(app, screen);
    CHECK(app.TestHandleEvent(Text("d")) == true);
    CHECK(app.TestOpeningBrowserFocus() == true);
    CHECK(app.TestHandleEvent(Special(wordstartui::SPECIAL_KEY_ESCAPE)) == true);
    CHECK(app.TestOpeningBrowserFocus() == false);
}


namespace
{

// Standard input replaced by a pipe that stays open until destruction.
class cStdinPipe
{
public:
    cStdinPipe(void)
    {
        int fds[2];
        REQUIRE(pipe(fds) == 0);
        mWrite = fds[1];
        mSavedStdin = dup(STDIN_FILENO);
        REQUIRE(mSavedStdin >= 0);
        REQUIRE(dup2(fds[0], STDIN_FILENO) == STDIN_FILENO);
        close(fds[0]);
    }

    ~cStdinPipe(void)
    {
        close(mWrite);
        dup2(mSavedStdin, STDIN_FILENO);
        close(mSavedStdin);
    }

    void Write(const std::string& bytes)
    {
        REQUIRE(write(mWrite, bytes.data(), bytes.size()) == static_cast<ssize_t>(bytes.size()));
    }

private:
    int mWrite = -1;
    int mSavedStdin = -1;
};

}


TEST_CASE("TUI startup screen - an ignored sequence in the dropped burst does not end the drop")
{
    struct sCase
    {
        const char* name;
        const char* sequence;
    };
    const sCase cases[] = {
        {"ESC [1;2A", "\x1b[1;2A"},
        {"ESC [99~", "\x1b[99~"},
        {"malformed SGR mouse", "\x1b[<0;5M"},
        {"truncated ESC [", "\x1b["},
    };

    for (const sCase& item : cases)
    {
        std::string name = item.name;
        CAPTURE(name);
        cStartupTempDir dir;
        cWSWordTsar app;
        StartShowingWithFile(app, dir);
        std::string text;
        {
            cStdinPipe input;
            wordstartui::cTerminalDriverPosix driver;

            input.Write(std::string("x") + item.sequence);
            app.TestPumpEvent(driver, 10);
            REQUIRE(app.TestInEditor() == true);
            text = app.TestGetEditor()->GetDocument()->GetText();
            app.TestPumpEvent(driver, 10);

            input.Write("y");
            app.TestPumpEvent(driver, 10);
            CheckLetterUnchanged(app, text);

            app.TestPumpEvent(driver, 10);
            input.Write("z");
            app.TestPumpEvent(driver, 10);
        }
        std::string after = app.TestGetEditor()->GetDocument()->GetText();
        CHECK(after.find('y') == std::string::npos);
        CHECK(after.find('z') != std::string::npos);
        CHECK(app.TestGetEditor()->GetDocument()->IsModified() == true);
    }
}


TEST_CASE("TUI terminal driver - SS3 arrows, Home and End decode as the keys")
{
    struct sCase
    {
        const char* sequence;
        wordstartui::eSpecialKey key;
    };
    const sCase cases[] = {
        {"\x1bOA", wordstartui::SPECIAL_KEY_ARROW_UP},
        {"\x1bOB", wordstartui::SPECIAL_KEY_ARROW_DOWN},
        {"\x1bOC", wordstartui::SPECIAL_KEY_ARROW_RIGHT},
        {"\x1bOD", wordstartui::SPECIAL_KEY_ARROW_LEFT},
        {"\x1bOH", wordstartui::SPECIAL_KEY_HOME},
        {"\x1bOF", wordstartui::SPECIAL_KEY_END},
    };

    for (const sCase& item : cases)
    {
        std::string sequence = std::string("ESC ") + (item.sequence + 1);
        CAPTURE(sequence);
        std::vector<wordstartui::sInputEvent> events = DriverEvents(item.sequence);
        REQUIRE(events.size() == 1);
        CHECK(events[0].type == wordstartui::INPUT_TYPE_SPECIAL);
        CHECK(events[0].special == item.key);
    }
}


TEST_CASE("TUI terminal driver - Alt+Backspace and Alt with a UTF-8 character are ignored")
{
    SUBCASE("ESC DEL")
    {
        for (const wordstartui::sInputEvent& event : DriverEvents("\x1b\x7f"))
        {
            CHECK(IsEscapeEvent(event) == false);
            CHECK(event.type == wordstartui::INPUT_TYPE_NONE);
        }
    }

    SUBCASE("ESC followed by a UTF-8 character")
    {
        for (const wordstartui::sInputEvent& event : DriverEvents("\x1b\xc3\xa9"))
        {
            CHECK(IsEscapeEvent(event) == false);
            CHECK(event.type == wordstartui::INPUT_TYPE_NONE);
        }
    }
}


TEST_CASE("TUI terminal driver - ESC ESC is one Escape and a lone Escape is Escape")
{
    std::vector<wordstartui::sInputEvent> twice = DriverEvents("\x1b\x1b");
    REQUIRE(twice.size() == 1);
    CHECK(IsEscapeEvent(twice[0]) == true);

    std::vector<wordstartui::sInputEvent> lone = DriverEvents("\x1b");
    REQUIRE(lone.size() == 1);
    CHECK(IsEscapeEvent(lone[0]) == true);
}


namespace
{

// Cells of the About text area, opened from the Opening screen at 80x25 in
// the given palette, all carry the dialog colours.
void CheckAboutTextColours(const sTUIPalette& palette, wordstartui::sStyle& dialog)
{
    cConfig config;
    config.ApplyTUIPalette(palette);
    cWSWordTsar app;
    app.TestApplyConfig(config);
    app.TestSetSystemInfo(FixedSystem());
    app.TestCountModals();
    wordstartui::cScreen screen;
    screen.Resize(25, 80);
    app.TestEnterOpening();
    app.TestGetEditor()->mHelpLevel = 3;
    app.TestDraw(screen);
    REQUIRE(app.TestHandleEvent(Text("w")) == true);
    REQUIRE(app.TestModalCount() == 1);

    const wordstartui::cScreen& modal = app.TestModalScreen();
    dialog = app.GetTheme().GetStyle(wordstartui::THEME_ROLE_DIALOG);

    sBox box = FindBox(modal);
    REQUIRE(box.top >= 0);
    REQUIRE(box.bottom - box.top > 5);
    int wrong = 0;
    for (int row = box.top + 2; row <= box.bottom - 3; ++row)
    {
        for (int col = box.left + 2; col <= box.right - 2; ++col)
        {
            const wordstartui::sCell* cell = modal.GetCell(row, col);
            if ((cell->style.fg != dialog.fg) || (cell->style.bg != dialog.bg))
            {
                ++wrong;
            }
        }
    }
    CHECK(wrong == 0);
}

}


TEST_CASE("TUI About box - the text area uses the dialog colours")
{
    for (const sTUIPalette& palette : cConfig::GetTUIPalettes())
    {
        CAPTURE(palette.name);
        wordstartui::sStyle dialog;
        CheckAboutTextColours(palette, dialog);
        if (palette.name == "WordStar 7")
        {
            CHECK(dialog.fg == wordstartui::MakeRgb(0, 0, 0));
            CHECK(dialog.bg == kGrey);
        }
    }
}


TEST_CASE("TUI startup screen - the separator bars are black squares, one cell each")
{
    for (std::pair<int, int> size : {std::make_pair(25, 80), std::make_pair(40, 120), std::make_pair(20, 60)})
    {
        const int rows = size.first;
        const int cols = size.second;
        CAPTURE(rows);
        CAPTURE(cols);
        cWSWordTsar app;
        PrepareWs7(app);
        wordstartui::cScreen screen;
        screen.Resize(rows, cols);
        app.TestDrawStartup(screen);

        const int sepWidth = cols * 7 / 10;
        const int sepCol = (cols - sepWidth) / 2;
        int separators = 0;
        for (int row = 1; row < rows; ++row)
        {
            CAPTURE(row);
            CHECK(IsBar(screen, row, sepCol) == false);
            if (IsSeparator(screen, row, sepCol) == false)
            {
                continue;
            }
            ++separators;
            for (int col = 0; col < cols; ++col)
            {
                CAPTURE(col);
                const wordstartui::sCell* cell = screen.GetCell(row, col);
                const bool inside = (col >= sepCol) && (col < sepCol + sepWidth);
                CHECK(IsSeparator(screen, row, col) == inside);
                CHECK(cell->width == 1);
                CHECK(cell->wideTail == false);
            }
        }
        CHECK(separators == 2);

        // The title bar keeps the upper half block.
        CHECK(IsBar(screen, 0, 0) == true);
        CHECK(IsBar(screen, 0, cols - 1) == true);
    }
}
