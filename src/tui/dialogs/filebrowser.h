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

#ifndef WORDTSAR_WSTUI_FILEBROWSER_H
#define WORDTSAR_WSTUI_FILEBROWSER_H

#include "src/tui/wordstartui/src/screen.h"
#include "src/tui/wordstartui/src/theme.h"
#include "src/tui/wordstartui/src/tuidefs.h"

#include <string>
#include <vector>

namespace wsui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cFileBrowser
///
/// @brief
/// A reusable multi-column file browser. Directories are listed first (with a
/// trailing '/'), then files. The column count adapts to the widest entry and
/// the available width. Arrow keys move within the grid, a letter jumps to the
/// next matching entry, Enter (or a mouse click) opens a directory or selects a
/// file. Used both by the standalone open/save dialog and the start screen.
///
/////////////////////////////////////////////////////////////////////////////
class cFileBrowser
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    // The result of handling an event.
    enum eAction
    {
        ACTION_NONE,       // consumed; nothing further
        ACTION_FILE,       // a file was chosen (see GetSelectedFile)
        ACTION_EXIT_TOP    // moved up past the first row (host should take focus)
    };

    cFileBrowser(void);

    void SetDirectory(const std::string& path);
    void SetBounds(const wordstartui::sRect& bounds);
    void Draw(wordstartui::cScreen& screen, const wordstartui::cTheme& theme, bool focused);
    eAction HandleEvent(const wordstartui::sInputEvent& event);

    std::string GetSelectedFile(void) const;
    std::string GetCurrentDirectory(void) const;
    std::string GetSelectedName(void) const;

private:
    struct sEntry
    {
        std::string name;
        std::string path;
        bool isDirectory;
    };

    void Rebuild(void);
    int ColumnWidth(void) const;
    int ColumnCount(void) const;
    int NumRows(void) const;
    int VisibleRows(void) const;
    void EnsureVisible(void);
    eAction Activate(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    std::string mDirectory;
    std::vector<sEntry> mEntries;
    int mSelected;
    int mScrollRow;                 // first visible grid row
    wordstartui::sRect mBounds;
    std::string mSelectedFile;
};

}

#endif // WORDTSAR_WSTUI_FILEBROWSER_H
