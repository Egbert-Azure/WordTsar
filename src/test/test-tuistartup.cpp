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
#include "src/core/utils/abouttext.h"
#include "src/core/utils/config.h"
#include "src/core/include/version.h"

#include <string>
#include <vector>


namespace
{

const wordstartui::sColor kBlue = wordstartui::MakeRgb(0, 0, 170);
const wordstartui::sColor kGrey = wordstartui::MakeRgb(170, 170, 170);
const char* const kUpperHalf = "\xe2\x96\x80";   // U+2580

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

    // Separators: 70 % of the width, centred, upper half blocks.
    const int sepWidth = cols * 7 / 10;
    const int sepCol = (cols - sepWidth) / 2;
    for (int row : {7, 11})
    {
        CAPTURE(row);
        CHECK(IsBar(screen, row, sepCol - 1) == false);
        CHECK(IsBar(screen, row, sepCol) == true);
        CHECK(IsBar(screen, row, sepCol + sepWidth - 1) == true);
        CHECK(IsBar(screen, row, sepCol + sepWidth) == false);
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
