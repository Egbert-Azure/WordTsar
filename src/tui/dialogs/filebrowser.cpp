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

#include "src/tui/dialogs/filebrowser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#endif

using wordstartui::cScreen;
using wordstartui::cTheme;
using wordstartui::sInputEvent;
using wordstartui::sRect;
using wordstartui::sStyle;

namespace fs = std::filesystem;

namespace wsui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct a browser rooted at the current working directory.
///
/////////////////////////////////////////////////////////////////////////////
cFileBrowser::cFileBrowser(void)
{
    mSelected = 0;
    mScrollRow = 0;
    mBounds.row = 0;
    mBounds.col = 0;
    mBounds.rows = 1;
    mBounds.cols = 1;

    std::error_code ec;
    mDirectory = fs::current_path(ec).string();
    Rebuild();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& path [in] a directory (or a path within one)
///
/// @return nothing
///
/// @brief
/// Change to a directory, normalizing the path, and reload its contents.
///
/////////////////////////////////////////////////////////////////////////////
void cFileBrowser::SetDirectory(const std::string& path)
{
    std::error_code ec;
    fs::path target(path);

    if (fs::is_directory(target, ec) == false)
    {
        target = target.parent_path();
    }

    fs::path canonical = fs::weakly_canonical(target, ec);
    if (ec)
    {
        canonical = target;
    }

    if (canonical.empty() == true)
    {
        canonical = fs::current_path(ec);
    }

    mDirectory = canonical.string();
    mSelected = 0;
    mScrollRow = 0;
    Rebuild();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] the render area
///
/// @return nothing
///
/// @brief
/// Set the screen area the grid is drawn in.
///
/////////////////////////////////////////////////////////////////////////////
void cFileBrowser::SetBounds(const sRect& bounds)
{
    mBounds = bounds;
    EnsureVisible();
}

std::string cFileBrowser::GetSelectedFile(void) const
{
    return mSelectedFile;
}

std::string cFileBrowser::GetCurrentDirectory(void) const
{
    return mDirectory;
}

std::string cFileBrowser::GetSelectedName(void) const
{
    if ((mSelected >= 0) && (mSelected < static_cast<int>(mEntries.size())))
    {
        return mEntries[static_cast<size_t>(mSelected)].name;
    }
    return std::string();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Read the current directory: ".." first, then sorted directories, then files.
///
/////////////////////////////////////////////////////////////////////////////
void cFileBrowser::Rebuild(void)
{
    mEntries.clear();

    fs::path current(mDirectory);

    sEntry up;
    up.name = "../";
    up.path = current.parent_path().string();
    if (up.path.empty() == true)
    {
        up.path = mDirectory;
    }
    up.isDirectory = true;
    mEntries.push_back(up);

    std::vector<sEntry> dirs;
    std::vector<sEntry> files;
    std::error_code ec;

    for (fs::directory_iterator it(current, ec); it != fs::directory_iterator(); it.increment(ec))
    {
        if (ec)
        {
            break;
        }

        std::error_code entryEc;
        std::string filename = it->path().filename().string();

        // Skip hidden files on any OS: dot-files (Unix/macOS) and, on Windows,
        // anything carrying the hidden or system attribute.
        if ((filename.empty() == false) && (filename[0] == '.'))
        {
            continue;
        }
#ifdef _WIN32
        {
            DWORD attributes = GetFileAttributesA(it->path().string().c_str());
            if ((attributes != INVALID_FILE_ATTRIBUTES) &&
                ((attributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) != 0))
            {
                continue;
            }
        }
#endif

        sEntry entry;
        entry.path = it->path().string();

        if (it->is_directory(entryEc) == true)
        {
            entry.name = it->path().filename().string() + "/";
            entry.isDirectory = true;
            dirs.push_back(entry);
        }
        else
        {
            entry.name = it->path().filename().string();
            entry.isDirectory = false;
            files.push_back(entry);
        }
    }

    // Sort alphabetically, case-insensitive.
    auto byName = [](const sEntry& a, const sEntry& b) -> bool
    {
        std::string la = a.name;
        std::string lb = b.name;
        std::transform(la.begin(), la.end(), la.begin(), ::tolower);
        std::transform(lb.begin(), lb.end(), lb.begin(), ::tolower);
        return la < lb;
    };
    std::sort(dirs.begin(), dirs.end(), byName);
    std::sort(files.begin(), files.end(), byName);

    for (const sEntry& dir : dirs)
    {
        mEntries.push_back(dir);
    }
    for (const sEntry& file : files)
    {
        mEntries.push_back(file);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the width in columns of one grid cell
///
/// @brief
/// Cell width from the longest entry name (clamped), so the column count adapts
/// to the file names.
///
/////////////////////////////////////////////////////////////////////////////
int cFileBrowser::ColumnWidth(void) const
{
    size_t longest = 4;
    for (const sEntry& entry : mEntries)
    {
        if (entry.name.size() > longest)
        {
            longest = entry.name.size();
        }
    }

    int width = static_cast<int>(longest) + 2;
    if (width < 12)
    {
        width = 12;
    }
    if (width > mBounds.cols)
    {
        width = mBounds.cols;
    }
    if (width < 1)
    {
        width = 1;
    }
    return width;
}

int cFileBrowser::ColumnCount(void) const
{
    int columns = mBounds.cols / ColumnWidth();
    if (columns < 1)
    {
        columns = 1;
    }
    return columns;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the number of grid rows (entries fill columns top-down)
///
/// @brief
/// Row count for the column-major grid: ceil(entries / columns).
///
/////////////////////////////////////////////////////////////////////////////
int cFileBrowser::NumRows(void) const
{
    int total = static_cast<int>(mEntries.size());
    int columns = ColumnCount();
    int rows = (total + columns - 1) / columns;
    if (rows < 1)
    {
        rows = 1;
    }
    return rows;
}

int cFileBrowser::VisibleRows(void) const
{
    int rows = mBounds.rows;
    if (rows < 1)
    {
        rows = 1;
    }
    return rows;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scroll so the selected entry's grid row is visible.
///
/////////////////////////////////////////////////////////////////////////////
void cFileBrowser::EnsureVisible(void)
{
    // Column-major grid: an entry's on-screen row is its index modulo the row
    // count (entries fill down each column first).
    int numRows = NumRows();
    int gridRow = mSelected % numRows;
    int visible = VisibleRows();

    if (gridRow < mScrollRow)
    {
        mScrollRow = gridRow;
    }
    else if (gridRow >= (mScrollRow + visible))
    {
        mScrollRow = gridRow - visible + 1;
    }

    if (mScrollRow < 0)
    {
        mScrollRow = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
/// @param  bool focused [in] true if the browser has focus
///
/// @return nothing
///
/// @brief
/// Draw the file grid. The current directory path is drawn on the first row.
///
/////////////////////////////////////////////////////////////////////////////
void cFileBrowser::Draw(cScreen& screen, const cTheme& theme, bool focused)
{
    sStyle normal = theme.GetStyle(wordstartui::THEME_ROLE_LIST);
    sStyle selected = theme.GetStyle(wordstartui::THEME_ROLE_LIST_SELECTED);

    screen.FillRect(mBounds, " ", normal);

    int numRows = NumRows();
    int width = ColumnWidth();
    int visible = VisibleRows();
    int count = static_cast<int>(mEntries.size());

    for (int index = 0; index < count; ++index)
    {
        int gridRow = index % numRows;
        int gridCol = index / numRows;

        if (gridRow < mScrollRow)
        {
            continue;
        }
        if (gridRow >= (mScrollRow + visible))
        {
            continue;
        }

        int screenRow = mBounds.row + (gridRow - mScrollRow);
        int screenCol = mBounds.col + (gridCol * width);

        sStyle style = normal;
        if (index == mSelected)
        {
            if (focused == true)
            {
                style = selected;
            }
            else
            {
                style.attrs = style.attrs | wordstartui::CELL_ATTR_UNDERLINE;
            }
        }

        std::string name = mEntries[static_cast<size_t>(index)].name;
        if (static_cast<int>(name.size()) > (width - 1))
        {
            name = name.substr(0, static_cast<size_t>(width - 1));
        }

        sRect cell;
        cell.row = screenRow;
        cell.col = screenCol;
        cell.rows = 1;
        cell.cols = width - 1;
        screen.FillRect(cell, " ", style);
        screen.PutText(screenRow, screenCol, name, style);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the action resulting from opening the selected entry
///
/// @brief
/// Open the selected entry: navigate into a directory, or choose a file.
///
/////////////////////////////////////////////////////////////////////////////
cFileBrowser::eAction cFileBrowser::Activate(void)
{
    if ((mSelected < 0) || (mSelected >= static_cast<int>(mEntries.size())))
    {
        return ACTION_NONE;
    }

    const sEntry& entry = mEntries[static_cast<size_t>(mSelected)];

    if (entry.isDirectory == true)
    {
        SetDirectory(entry.path);
        return ACTION_NONE;
    }

    mSelectedFile = entry.path;
    return ACTION_FILE;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return the resulting action
///
/// @brief
/// Grid navigation: arrows move, a letter jumps, Enter or a click opens.
///
/////////////////////////////////////////////////////////////////////////////
cFileBrowser::eAction cFileBrowser::HandleEvent(const sInputEvent& event)
{
    int columns = ColumnCount();
    int numRows = NumRows();
    int count = static_cast<int>(mEntries.size());
    if (count == 0)
    {
        return ACTION_NONE;
    }

    if (event.type == wordstartui::INPUT_TYPE_SPECIAL)
    {
        // Column-major grid: Down/Up move one entry (entries fill down each
        // column first); Right/Left jump a whole column (numRows entries).
        // Column-major fill order (down each column, then across).
        if (event.special == wordstartui::SPECIAL_KEY_ARROW_DOWN)
        {
            if (mSelected < (count - 1))
            {
                mSelected++;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_UP)
        {
            if (mSelected == 0)
            {
                return ACTION_EXIT_TOP;
            }
            mSelected--;
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_RIGHT)
        {
            if ((mSelected + numRows) < count)
            {
                mSelected += numRows;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_LEFT)
        {
            if ((mSelected - numRows) >= 0)
            {
                mSelected -= numRows;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_PAGE_DOWN)
        {
            mSelected += VisibleRows();
            if (mSelected >= count)
            {
                mSelected = count - 1;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_PAGE_UP)
        {
            mSelected -= VisibleRows();
            if (mSelected < 0)
            {
                mSelected = 0;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_HOME)
        {
            mSelected = 0;
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_END)
        {
            mSelected = count - 1;
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ENTER)
        {
            return Activate();
        }
    }

    if ((event.type == wordstartui::INPUT_TYPE_MOUSE) &&
        (event.mouseAction == wordstartui::MOUSE_ACTION_PRESS))
    {
        int width = ColumnWidth();
        int localRow = event.mouseRow - mBounds.row;
        int localCol = event.mouseCol - mBounds.col;

        if ((localRow >= 0) && (localRow < VisibleRows()) && (localCol >= 0))
        {
            int gridRow = mScrollRow + localRow;
            int gridCol = localCol / width;
            if (gridCol < columns)
            {
                int index = (gridCol * numRows) + gridRow;
                if ((index >= 0) && (index < count))
                {
                    mSelected = index;
                    return Activate();
                }
            }
        }
        return ACTION_NONE;
    }

    if ((event.type == wordstartui::INPUT_TYPE_TEXT) && (event.textUtf8.empty() == false))
    {
        char wanted = static_cast<char>(std::tolower(static_cast<unsigned char>(event.textUtf8[0])));

        for (int step = 1; step <= count; ++step)
        {
            int index = (mSelected + step) % count;
            const std::string& name = mEntries[static_cast<size_t>(index)].name;
            if (name.empty() == false)
            {
                char first = static_cast<char>(std::tolower(static_cast<unsigned char>(name[0])));
                if (first == wanted)
                {
                    mSelected = index;
                    EnsureVisible();
                    return ACTION_NONE;
                }
            }
        }
    }

    return ACTION_NONE;
}

}
