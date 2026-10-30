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

#include <string>
#include <vector>


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
    cGridDriver(void)
        : mCells(SCREEN_ROWS, std::vector<std::string>(SCREEN_COLS, " "))
    {
    }

    bool Open(void) override { return true; }
    void Close(void) override {}

    wordstartui::sTerminalSize GetSize(void) const override
    {
        wordstartui::sTerminalSize size;
        size.rows = SCREEN_ROWS;
        size.cols = SCREEN_COLS;
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
                if ((run.row >= 0) && (run.row < SCREEN_ROWS) && (col >= 0) && (col < SCREEN_COLS))
                {
                    mCells[static_cast<size_t>(run.row)][static_cast<size_t>(col)] = run.cells[index].textUtf8;
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

    std::vector<std::vector<std::string>> mCells;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @brief Dialog host that renders the modal dialog once and cancels it.
///
/////////////////////////////////////////////////////////////////////////////
class cRenderHost : public iWSDialogHost
{
public:
    void HostRunModal(wordstartui::cDialog& dialog) override
    {
        wordstartui::cScreen screen;
        screen.Resize(SCREEN_ROWS, SCREEN_COLS);
        dialog.Draw(screen, mTheme);
        screen.Present(mDriver);
        dialog.SetResult(wordstartui::DIALOG_RESULT_CANCEL);
    }

    void HostRunModalRaw(
        const std::function<void(wordstartui::cScreen&, const wordstartui::cTheme&)>& draw,
        const std::function<bool(const wordstartui::sInputEvent&)>& handle) override
    {
        (void)draw;
        (void)handle;
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

    // True when every row between the top and bottom border keeps its right border
    bool RightBorderIntact(void) const
    {
        int top = -1;
        int bottom = -1;
        int right = -1;
        for (int row = 0; row < SCREEN_ROWS; ++row)
        {
            for (int col = 0; col < SCREEN_COLS; ++col)
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
