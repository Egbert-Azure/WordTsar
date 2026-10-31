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

#include "src/tui/dialogs/dialoghost.h"
#include "src/tui/dialogs/dialogs.h"
#include "src/tui/wordstartui/src/dialog.h"
#include "src/tui/wordstartui/src/screen.h"
#include "src/tui/wordstartui/src/terminaldriver.h"
#include "src/tui/wordstartui/src/theme.h"
#include "src/tui/wordstartui/src/tuidefs.h"
#include "src/tui/wordstartui/src/utf8helper.h"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>


namespace
{

const int SCREEN_ROWS = 40;
const int SCREEN_COLS = 120;

/////////////////////////////////////////////////////////////////////////////
///
/// @brief Terminal driver that stores presented cells in a grid.
///
/////////////////////////////////////////////////////////////////////////////
class cGridDriver : public wordstartui::cTerminalDriver
{
public:
    explicit cGridDriver(int rows = SCREEN_ROWS, int cols = SCREEN_COLS)
        : mRows(rows),
          mCols(cols),
          mCells(static_cast<size_t>(rows), std::vector<std::string>(static_cast<size_t>(cols), " ")),
          mWidths(static_cast<size_t>(rows), std::vector<int>(static_cast<size_t>(cols), 1)),
          mTails(static_cast<size_t>(rows), std::vector<bool>(static_cast<size_t>(cols), false))
    {
    }

    bool Open(void) override { return true; }
    void Close(void) override {}

    wordstartui::sTerminalSize GetSize(void) const override
    {
        wordstartui::sTerminalSize size;
        size.rows = mRows;
        size.cols = mCols;
        return size;
    }

    wordstartui::sTerminalCapabilities GetCapabilities(void) const override
    {
        wordstartui::sTerminalCapabilities caps{};
        return caps;
    }

    bool ReadEvent(wordstartui::sInputEvent& event, int timeoutMs) override
    {
        (void)event;
        (void)timeoutMs;
        return false;
    }

    void Present(const std::vector<wordstartui::sCellRun>& runs) override
    {
        for (const wordstartui::sCellRun& run : runs)
        {
            for (size_t index = 0; index < run.cells.size(); ++index)
            {
                int col = run.col + static_cast<int>(index);
                if ((run.row >= 0) && (run.row < mRows) && (col >= 0) && (col < mCols))
                {
                    mCells[static_cast<size_t>(run.row)][static_cast<size_t>(col)] = run.cells[index].textUtf8;
                    mWidths[static_cast<size_t>(run.row)][static_cast<size_t>(col)] = run.cells[index].width;
                    mTails[static_cast<size_t>(run.row)][static_cast<size_t>(col)] = run.cells[index].wideTail;
                }
                else
                {
                    ++mOutside;
                }
            }
        }
    }

    void SetCursor(int row, int col, bool visible) override
    {
        (void)row;
        (void)col;
        (void)visible;
    }

    void Flush(void) override {}

    int mRows;
    int mCols;
    std::vector<std::vector<std::string>> mCells;
    std::vector<std::vector<int>> mWidths;
    std::vector<std::vector<bool>> mTails;
    int mOutside = 0;                                   // cells presented outside the grid
};

/////////////////////////////////////////////////////////////////////////////
///
/// @brief Dialog host that renders the modal dialog once and cancels it.
///
/////////////////////////////////////////////////////////////////////////////
class cRenderHost : public iWSDialogHost
{
public:
    explicit cRenderHost(int rows = SCREEN_ROWS, int cols = SCREEN_COLS)
        : mDriver(rows, cols)
    {
    }

    void HostRunModal(wordstartui::cDialog& dialog) override
    {
        wordstartui::cScreen screen;
        screen.Resize(mDriver.mRows, mDriver.mCols);
        dialog.Draw(screen, mTheme);
        screen.Present(mDriver);
        dialog.SetResult(wordstartui::DIALOG_RESULT_CANCEL);
    }

    void HostRunModalRaw(
        const std::function<void(wordstartui::cScreen&, const wordstartui::cTheme&)>& draw,
        const std::function<bool(const wordstartui::sInputEvent&)>& handle) override
    {
        (void)handle;
        wordstartui::cScreen screen;
        screen.Resize(mDriver.mRows, mDriver.mCols);
        draw(screen, mTheme);
        screen.Present(mDriver);
    }

    const wordstartui::cTheme& HostTheme(void) override
    {
        return mTheme;
    }

    wordstartui::sTerminalSize HostScreenSize(void) override
    {
        return mDriver.GetSize();
    }

    wordstartui::sTerminalCapabilities HostCapabilities(void) override
    {
        return mDriver.GetCapabilities();
    }

    void HostShowLoadProgress(const std::string& message, int percent) override
    {
        (void)message;
        (void)percent;
    }

    void HostHideLoadProgress(void) override
    {
    }

    // The rendered screen, one string per row
    std::vector<std::string> Rows(void) const
    {
        std::vector<std::string> rows;
        for (const std::vector<std::string>& cells : mDriver.mCells)
        {
            std::string row;
            for (const std::string& cell : cells)
            {
                row += cell;
            }
            rows.push_back(row);
        }
        return rows;
    }

    // True when the text appears on any row with the tail cells of wide characters left out
    bool ShowsText(const std::string& text) const
    {
        for (size_t row = 0; row < mDriver.mCells.size(); ++row)
        {
            std::string line;
            for (size_t col = 0; col < mDriver.mCells[row].size(); ++col)
            {
                if (mDriver.mTails[row][col] == false)
                {
                    line += mDriver.mCells[row][col];
                }
            }
            if (line.find(text) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    // True when every presented cell is valid UTF-8 and none is "?"
    bool AllCellsValid(void) const;

    // True when the text appears on any row
    bool Shows(const std::string& text) const
    {
        for (const std::string& row : Rows())
        {
            if (row.find(text) != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    // True when no presented cell holds a C0 control byte or DEL
    bool NoControlBytes(void) const
    {
        for (const std::vector<std::string>& cells : mDriver.mCells)
        {
            for (const std::string& cell : cells)
            {
                for (char byte : cell)
                {
                    unsigned char value = static_cast<unsigned char>(byte);
                    if ((value < 0x20U) || (value == 0x7FU))
                    {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    // True when every row between the top and bottom border keeps its right border
    bool RightBorderIntact(void) const
    {
        int top = -1;
        int bottom = -1;
        int right = -1;
        for (int row = 0; row < mDriver.mRows; ++row)
        {
            for (int col = 0; col < mDriver.mCols; ++col)
            {
                const std::string& cell = mDriver.mCells[static_cast<size_t>(row)][static_cast<size_t>(col)];
                if (cell == "┐")
                {
                    top = row;
                    right = col;
                }
                if (cell == "┘")
                {
                    bottom = row;
                }
            }
        }
        if ((top < 0) || (bottom <= top))
        {
            return false;
        }
        for (int row = top + 1; row < bottom; ++row)
        {
            if (mDriver.mCells[static_cast<size_t>(row)][static_cast<size_t>(right)] != "│")
            {
                return false;
            }
        }
        return true;
    }

    // The cell at a screen position
    const std::string& Cell(int row, int col) const
    {
        return mDriver.mCells[static_cast<size_t>(row)][static_cast<size_t>(col)];
    }

    // Number of cells presented outside the grid
    int OutsideWrites(void) const
    {
        return mDriver.mOutside;
    }

    // Screen positions of the box corner glyphs
    std::vector<std::pair<int, int>> Corners(void) const
    {
        std::vector<std::pair<int, int>> corners;
        for (int row = 0; row < mDriver.mRows; ++row)
        {
            for (int col = 0; col < mDriver.mCols; ++col)
            {
                const std::string& cell = Cell(row, col);
                if ((cell == "┌") || (cell == "┐") || (cell == "└") || (cell == "┘"))
                {
                    corners.emplace_back(row, col);
                }
            }
        }
        return corners;
    }

private:
    cGridDriver mDriver;
    wordstartui::cTheme mTheme;
};

}


TEST_CASE("TUI Find dialog - WordStar 7 wording")
{
    cRenderHost host;
    wsdialogs::sFindOptions initial;
    wsdialogs::FindDialog(&host, initial);

    CHECK(host.Shows(" Find "));
    CHECK(host.Shows("Find:"));
    CHECK(host.Shows(" OK "));
    CHECK(host.Shows(" Cancel "));
    CHECK(host.Shows("For Next Occurrence"));
    CHECK(host.Shows("Entire File/Global"));
    CHECK(host.Shows("Whole Words"));
    CHECK(host.Shows("Ignore Case (U)"));
    CHECK(host.Shows("Backward"));
    CHECK(host.Shows("Use ? Wildcard Character"));
    CHECK(host.RightBorderIntact());
}


TEST_CASE("TUI Find and Replace dialog - WordStar 7 wording")
{
    cRenderHost host;
    wsdialogs::sReplaceOptions initial;
    wsdialogs::ReplaceDialog(&host, initial);

    CHECK(host.Shows(" Find and Replace "));
    CHECK(host.Shows("Find:"));
    CHECK(host.Shows("Replace:"));
    CHECK(host.Shows(" OK "));
    CHECK(host.Shows(" Cancel "));
    CHECK(host.Shows("For Next Occurrence"));
    CHECK(host.Shows("Entire File/Global"));
    CHECK(host.Shows("Rest of File"));
    CHECK(host.Shows("Don't Ask"));
    CHECK(host.Shows("Whole Words"));
    CHECK(host.Shows("Ignore Case (U)"));
    CHECK(host.Shows("Backward"));
    CHECK(host.Shows("Use ? Wildcard Character"));
    CHECK(host.RightBorderIntact());
}


TEST_CASE("TUI Save File dialog - a control character in a file name reaches no screen cell")
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                ("wstest-savefile-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "alpha.ws").put('x');
    std::ofstream(dir / "Icon\r").put('x');
    std::ofstream(dir / "zulu.ws").put('x');

    cRenderHost host;
    std::string result;
    wsdialogs::FileBrowser(&host, "Save File", true, dir.string(), result);

    CHECK(host.Shows("alpha.ws"));
    CHECK(host.Shows("zulu.ws"));
    CHECK(host.Shows("Icon?"));
    CHECK(host.NoControlBytes());
    CHECK(host.RightBorderIntact());

    std::filesystem::remove_all(dir);
}


TEST_CASE("TUI Open and Save File dialogs - the box covers the whole screen")
{
    const int sizes[][2] = { { SCREEN_ROWS, SCREEN_COLS }, { 25, 80 } };
    for (const auto& size : sizes)
    {
        for (bool saveMode : { false, true })
        {
            CAPTURE(size[0]);
            CAPTURE(size[1]);
            CAPTURE(saveMode);
            cRenderHost host(size[0], size[1]);
            std::string result;
            wsdialogs::FileBrowser(&host, saveMode ? "Save File" : "Open File", saveMode,
                                   std::filesystem::temp_directory_path().string(), result);

            CHECK(host.Cell(0, 0) == "┌");
            CHECK(host.Cell(0, size[1] - 1) == "┐");
            CHECK(host.Cell(size[0] - 1, 0) == "└");
            CHECK(host.Cell(size[0] - 1, size[1] - 1) == "┘");
            CHECK(host.RightBorderIntact());
        }
    }
}


namespace
{

// True when a cell holds a C0 control, DEL or a C1 control (UTF-8 C2 80..C2 9F)
bool CellHasControl(const std::string& cell)
{
    for (size_t index = 0; index < cell.size(); ++index)
    {
        unsigned char value = static_cast<unsigned char>(cell[index]);
        if ((value < 0x20U) || (value == 0x7FU))
        {
            return true;
        }
        if ((value == 0xC2U) && ((index + 1) < cell.size()))
        {
            unsigned char next = static_cast<unsigned char>(cell[index + 1]);
            if ((next >= 0x80U) && (next <= 0x9FU))
            {
                return true;
            }
        }
    }
    return false;
}

// Render one text run at row 0, column 0 and return the presented row
std::vector<std::string> PresentText(const std::string& text)
{
    cGridDriver driver;
    wordstartui::cScreen screen;
    screen.Resize(SCREEN_ROWS, SCREEN_COLS);
    wordstartui::sStyle style{};
    screen.PutText(0, 0, text, style);
    screen.Present(driver);
    return driver.mCells[0];
}

}


TEST_CASE("TUI screen - a C1 control character reaches no screen cell")
{
    // "A" CSI "B": U+009B is CSI for UTF-8 terminals
    std::vector<std::string> cells = PresentText("A\xC2\x9B" "B");

    CHECK(cells[0] == "A");
    CHECK(cells[1] == "?");
    CHECK(cells[2] == "B");
    for (const std::string& cell : cells)
    {
        CHECK(CellHasControl(cell) == false);
    }
}


TEST_CASE("TUI screen - a control character after a ZWJ reaches no screen cell")
{
    // One grapheme "A" ZWJ CR: the control is not the first codepoint
    cGridDriver driver;
    wordstartui::cScreen screen;
    screen.Resize(SCREEN_ROWS, SCREEN_COLS);
    wordstartui::sStyle style{};
    screen.PutCell(0, 0, "A\xE2\x80\x8D\r", style);
    screen.PutCell(0, 1, "B", style);
    screen.Present(driver);
    const std::vector<std::string>& cells = driver.mCells[0];

    CHECK(cells[0] == "?");
    CHECK(cells[1] == "B");
    for (const std::string& cell : cells)
    {
        CHECK(CellHasControl(cell) == false);
    }
}


namespace
{

// True when the text is well-formed UTF-8 (no overlongs, surrogates or values above U+10FFFF)
bool IsValidUtf8(const std::string& text)
{
    size_t index = 0;
    while (index < text.size())
    {
        unsigned char lead = static_cast<unsigned char>(text[index]);
        size_t count = 0;
        char32_t value = 0;
        if (lead < 0x80U)
        {
            ++index;
            continue;
        }
        else if ((lead >= 0xC2U) && (lead <= 0xDFU))
        {
            count = 2;
            value = lead & 0x1FU;
        }
        else if ((lead & 0xF0U) == 0xE0U)
        {
            count = 3;
            value = lead & 0x0FU;
        }
        else if ((lead >= 0xF0U) && (lead <= 0xF4U))
        {
            count = 4;
            value = lead & 0x07U;
        }
        else
        {
            return false;
        }
        if ((index + count) > text.size())
        {
            return false;
        }
        for (size_t i = 1; i < count; ++i)
        {
            unsigned char next = static_cast<unsigned char>(text[index + i]);
            if ((next & 0xC0U) != 0x80U)
            {
                return false;
            }
            value = (value << 6U) | (next & 0x3FU);
        }
        if (((count == 3) && (value < 0x800U)) || ((count == 4) && (value < 0x10000U)) ||
            ((value >= 0xD800U) && (value <= 0xDFFFU)) || (value > 0x10FFFFU))
        {
            return false;
        }
        index += count;
    }
    return true;
}

}


TEST_CASE("TUI screen - invalid UTF-8 reaches no screen cell")
{
    // Lone 0x9B, a 3-byte sequence cut after two bytes, and a cut at the end
    std::vector<std::string> cells = PresentText("A\x9B" "B\xE6\xBC" "C\xE6\xBC");

    CHECK(cells[0] == "A");
    CHECK(cells[1] == "?");
    CHECK(cells[2] == "B");
    CHECK(cells[3] == "?");
    CHECK(cells[4] == "?");
    CHECK(cells[5] == "C");
    CHECK(cells[6] == "?");
    for (const std::string& cell : cells)
    {
        CHECK(IsValidUtf8(cell) == true);
    }

    // An encoded U+FFFD stays as it is
    std::vector<std::string> replacement = PresentText("\xEF\xBF\xBD");
    CHECK(replacement[0] == "\xEF\xBF\xBD");
}


TEST_CASE("TUI screen - a wide grapheme shown as \"?\" takes one column plus a blank")
{
    // U+6F22 followed by CR in one grapheme
    cGridDriver driver;
    wordstartui::cScreen screen;
    screen.Resize(SCREEN_ROWS, SCREEN_COLS);
    wordstartui::sStyle style{};
    screen.PutCell(0, 0, "\xE6\xBC\xA2\r", style);
    screen.PutCell(0, 2, "B", style);
    screen.Present(driver);

    CHECK(driver.mCells[0][0] == "?");
    CHECK(driver.mWidths[0][0] == 1);
    CHECK(driver.mTails[0][0] == false);
    CHECK(driver.mCells[0][1] == " ");
    CHECK(driver.mWidths[0][1] == 1);
    CHECK(driver.mTails[0][1] == false);
    CHECK(driver.mCells[0][2] == "B");
}


TEST_CASE("TUI screen - characters U+00A0..U+00FF reach the screen unchanged")
{
    for (char32_t codepoint = 0xA0U; codepoint <= 0xFFU; ++codepoint)
    {
        std::string text = wordstartui::cUtf8Helper::Encode(codepoint);
        std::vector<std::string> cells = PresentText(text);
        CHECK(cells[0] == text);
    }
}


TEST_CASE("TUI utf8 - a codepoint after a ZWJ joins the grapheme")
{
    // "A" ZWJ "B"
    std::vector<std::string> joined = wordstartui::cUtf8Helper::SplitGraphemes("A\xE2\x80\x8D" "B");
    REQUIRE(joined.size() == 1);
    CHECK(joined[0] == "A\xE2\x80\x8D" "B");

    // U+1F468 ZWJ U+1F469 ZWJ U+1F467, then "C"
    const std::string family = "\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA7";
    std::vector<std::string> emoji = wordstartui::cUtf8Helper::SplitGraphemes(family + "C");
    REQUIRE(emoji.size() == 2);
    CHECK(emoji[0] == family);
    CHECK(emoji[1] == "C");
}


TEST_CASE("TUI screen - a control character after a ZWJ in text reaches no screen cell")
{
    // "A" ZWJ CR is one grapheme, then "B"
    std::vector<std::string> cells = PresentText("A\xE2\x80\x8D\r" "B");

    CHECK(cells[0] == "?");
    CHECK(cells[1] == "B");
    for (const std::string& cell : cells)
    {
        CHECK(CellHasControl(cell) == false);
    }
}


bool cRenderHost::AllCellsValid(void) const
{
    for (const std::vector<std::string>& cells : mDriver.mCells)
    {
        for (const std::string& cell : cells)
        {
            if ((IsValidUtf8(cell) == false) || (cell == "?"))
            {
                return false;
            }
        }
    }
    return true;
}


TEST_CASE("TUI utf8 - text width and fitting by whole graphemes")
{
    using wordstartui::cUtf8Helper;

    const std::string wide = "\xE6\xBC\xA2";            // U+6F22, 2 columns
    const std::string umlaut = "\xC3\xA4";              // U+00E4, 1 column
    const std::string accented = "e\xCC\x81";           // "e" U+0301, 1 column

    CHECK(cUtf8Helper::TextWidth("") == 0);
    CHECK(cUtf8Helper::TextWidth("abc") == 3);
    CHECK(cUtf8Helper::TextWidth(wide + wide) == 4);
    CHECK(cUtf8Helper::TextWidth(umlaut + umlaut) == 2);
    CHECK(cUtf8Helper::TextWidth(accented + "x") == 2);

    // A wide grapheme at the limit is left out, not split
    CHECK(cUtf8Helper::FitWidth("a" + wide + "b", 2) == "a");
    CHECK(cUtf8Helper::FitWidth("a" + wide + "b", 3) == "a" + wide);
    CHECK(cUtf8Helper::FitWidth("a" + wide + "b", 4) == "a" + wide + "b");
    CHECK(cUtf8Helper::FitWidth("a" + wide + "b", 9) == "a" + wide + "b");
    CHECK(cUtf8Helper::FitWidth(accented + "x", 1) == accented);
    CHECK(cUtf8Helper::FitWidth("abc", 0) == "");
    CHECK(cUtf8Helper::FitWidth("abc", -1) == "");

    CHECK(cUtf8Helper::FitWidthTail("a" + wide + "b", 2) == "b");
    CHECK(cUtf8Helper::FitWidthTail("a" + wide + "b", 3) == wide + "b");
    CHECK(cUtf8Helper::FitWidthTail("a" + wide + "b", 9) == "a" + wide + "b");
    CHECK(cUtf8Helper::FitWidthTail("x" + accented, 1) == accented);
    CHECK(cUtf8Helper::FitWidthTail("abc", 0) == "");
}


TEST_CASE("TUI Open File dialog - long UTF-8 file names are cut by columns, not bytes")
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                ("wstest-utf8names-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    std::string wideName;                               // 40 x U+6F22: 120 bytes, 80 columns
    for (int i = 0; i < 40; ++i)
    {
        wideName += "\xE6\xBC\xA2";
    }
    std::string umlautName;                             // 60 x U+00E4: 120 bytes, 60 columns
    for (int i = 0; i < 60; ++i)
    {
        umlautName += "\xC3\xA4";
    }
    std::ofstream(dir / wideName).put('x');
    std::ofstream(dir / umlautName).put('x');

    cRenderHost host;
    std::string result;
    wsdialogs::FileBrowser(&host, "Open File", false, dir.string(), result);

    CHECK(host.ShowsText(wideName));
    CHECK(host.ShowsText(umlautName));
    CHECK(host.AllCellsValid());
    CHECK(host.RightBorderIntact());

    std::filesystem::remove_all(dir);
}


TEST_CASE("TUI Open File dialog - a long UTF-8 folder line is cut by columns, not bytes")
{
    std::string umlautName;                             // 60 x U+00E4: 120 bytes, 60 columns
    for (int i = 0; i < 60; ++i)
    {
        umlautName += "\xC3\xA4";
    }
    std::filesystem::path base = std::filesystem::temp_directory_path() /
                                 ("wstest-utf8folder-" + std::to_string(getpid()) +
                                  "-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    std::filesystem::path dir = base / umlautName;
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(dir);

    cRenderHost host;
    std::string result;
    wsdialogs::FileBrowser(&host, "Open File", false, dir.string(), result);

    // The folder line is wider than the dialog; its last 116 columns hold "/" and the full name
    REQUIRE(wordstartui::cUtf8Helper::TextWidth(dir.string()) > 116);
    CHECK(host.ShowsText("x/" + umlautName));
    CHECK(host.AllCellsValid());
    CHECK(host.RightBorderIntact());

    std::filesystem::remove_all(base);
}


TEST_CASE("TUI Open and Save File dialogs - a tiny terminal keeps the box inside the screen")
{
    // 30x8 is the smallest box; smaller screens clip it
    const int sizes[][2] = { { 8, 30 }, { 5, 20 }, { 1, 1 } };
    for (const auto& size : sizes)
    {
        for (bool saveMode : { false, true })
        {
            CAPTURE(size[0]);
            CAPTURE(size[1]);
            CAPTURE(saveMode);
            cRenderHost host(size[0], size[1]);
            std::string result;
            wsdialogs::FileBrowser(&host, saveMode ? "Save File" : "Open File", saveMode,
                                   std::filesystem::temp_directory_path().string(), result);

            CHECK(host.OutsideWrites() == 0);
            CHECK(host.Cell(0, 0) == "┌");
            if ((size[0] >= 8) && (size[1] >= 30))
            {
                CHECK(host.Cell(0, size[1] - 1) == "┐");
                CHECK(host.Cell(size[0] - 1, 0) == "└");
                CHECK(host.Cell(size[0] - 1, size[1] - 1) == "┘");
                CHECK(host.Corners().size() == 4);
                CHECK(host.RightBorderIntact());
            }
            else
            {
                CHECK(host.Corners().size() == 1);
            }
            for (int row = 1; row < size[0]; ++row)
            {
                CHECK(host.Cell(row, 0) == ((row == 7) ? "└" : "│"));
            }
        }
    }
}


TEST_CASE("TUI Open File dialog - a file name wider than the screen is cut by columns")
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() /
                                ("wstest-widename-" + std::to_string(getpid()));
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    std::string wideName;                               // 60 x U+6F22: 120 columns
    for (int i = 0; i < 60; ++i)
    {
        wideName += "\xE6\xBC\xA2";
    }
    std::ofstream(dir / wideName).put('x');

    cRenderHost host(25, 80);
    std::string result;
    wsdialogs::FileBrowser(&host, "Open File", false, dir.string(), result);

    // The grid is 76 columns, a name gets 75: 37 wide characters fit
    std::string shown;
    for (int i = 0; i < 37; ++i)
    {
        shown += "\xE6\xBC\xA2";
    }
    CHECK(host.ShowsText(shown));
    CHECK_FALSE(host.ShowsText(shown + "\xE6\xBC\xA2"));
    CHECK(host.OutsideWrites() == 0);
    CHECK(host.AllCellsValid());
    CHECK(host.RightBorderIntact());

    std::filesystem::remove_all(dir);
}


TEST_CASE("TUI Open File dialog - a wide folder line cut at an odd width splits no character")
{
    std::string wideName;                               // 40 x U+6F22 then "x": 81 columns
    for (int i = 0; i < 40; ++i)
    {
        wideName += "\xE6\xBC\xA2";
    }
    wideName += "x";
    std::filesystem::path base = std::filesystem::temp_directory_path() /
                                 ("wstest-widefolder-" + std::to_string(getpid()));
    std::filesystem::path dir = base / wideName;
    std::filesystem::remove_all(base);
    std::filesystem::create_directories(dir);

    cRenderHost host(25, 80);
    std::string result;
    wsdialogs::FileBrowser(&host, "Open File", false, dir.string(), result);

    // The folder line gets 76 columns: 37 wide characters and "x" take 75
    std::string shown;
    for (int i = 0; i < 37; ++i)
    {
        shown += "\xE6\xBC\xA2";
    }
    CHECK(host.ShowsText(shown + "x"));
    CHECK_FALSE(host.ShowsText("\xE6\xBC\xA2" + shown + "x"));
    CHECK(host.Cell(1, 2) == "\xE6\xBC\xA2");
    CHECK(host.OutsideWrites() == 0);
    CHECK(host.AllCellsValid());
    CHECK(host.RightBorderIntact());

    std::filesystem::remove_all(base);
}
