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

#include "src/tui/wordstartui/src/utf8helper.h"

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
    mWordStar = false;
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

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool on [in] true for the WordStar layout
///
/// @return nothing
///
/// @brief
/// Switch between the adaptive column-major grid and the WordStar layout of
/// 20-cell columns filled across each row first.
///
/////////////////////////////////////////////////////////////////////////////
void cFileBrowser::SetWordStarLayout(bool on)
{
    mWordStar = on;
    mScrollRow = 0;
    EnsureVisible();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::uintmax_t bytes [in] file size
///
/// @return the size as shown in the WordStar layout
///
/// @brief
/// Rounded, in units of 1000: ".3k" below 1k, "1.7k" below 10k, "327k" below
/// 1000k, then "1.2M" below 1000M, then "1.1G".
///
/////////////////////////////////////////////////////////////////////////////
std::string cFileBrowser::FormatSize(std::uintmax_t bytes)
{
    auto tenths = [](std::uintmax_t value) -> std::string
    {
        return std::to_string(value / 10) + "." + std::to_string(value % 10);
    };

    const std::uintmax_t tenthsK = (bytes + 50) / 100;
    if (tenthsK < 10)
    {
        return "." + std::to_string(tenthsK) + "k";
    }
    if (tenthsK < 100)
    {
        return tenths(tenthsK) + "k";
    }

    const std::uintmax_t kilo = (bytes + 500) / 1000;
    if (kilo < 1000)
    {
        return std::to_string(kilo) + "k";
    }

    const std::uintmax_t tenthsM = (bytes + 50000) / 100000;
    if (tenthsM < 10000)
    {
        return tenths(tenthsM) + "M";
    }

    return tenths((bytes + 50000000) / 100000000) + "G";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& name [in] file or folder name
/// @param  int width [in] available columns
///
/// @return the name, cut to the width
///
/// @brief
/// Shorten a name that does not fit: the extension stays and the part before
/// it is cut, the cut marked with an ellipsis. A name whose extension leaves
/// no room is cut at the end.
///
/////////////////////////////////////////////////////////////////////////////
std::string cFileBrowser::ShortenName(const std::string& name, int width)
{
    const std::string ellipsis = "\xe2\x80\xa6";   // U+2026

    if (wordstartui::cUtf8Helper::TextWidth(name) <= width)
    {
        return name;
    }
    if (width < 2)
    {
        return wordstartui::cUtf8Helper::FitWidth(name, width);
    }

    const size_t dot = name.rfind('.');
    if ((dot != std::string::npos) && (dot > 0))
    {
        const std::string extension = name.substr(dot);
        const int baseWidth = width - 1 - wordstartui::cUtf8Helper::TextWidth(extension);
        if (baseWidth >= 1)
        {
            return wordstartui::cUtf8Helper::FitWidth(name.substr(0, dot), baseWidth) + ellipsis + extension;
        }
    }

    return wordstartui::cUtf8Helper::FitWidth(name, width - 1) + ellipsis;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::uintmax_t bytes [in] free bytes
///
/// @return the amount as shown on the Opening screen
///
/// @brief
/// Rounded, in units of 1000: whole megabytes below 1000M ("512M"), then
/// gigabytes with one decimal ("2.0G").
///
/////////////////////////////////////////////////////////////////////////////
std::string cFileBrowser::FormatFreeSpace(std::uintmax_t bytes)
{
    const std::uintmax_t mega = (bytes + 500000) / 1000000;
    if (mega < 1000)
    {
        return std::to_string(mega) + "M";
    }

    const std::uintmax_t tenthsG = (bytes + 50000000) / 100000000;
    return std::to_string(tenthsG / 10) + "." + std::to_string(tenthsG % 10) + "G";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& directory [in] a directory
///
/// @return the free space on its volume, or an empty string if unknown
///
/////////////////////////////////////////////////////////////////////////////
std::string cFileBrowser::FreeSpace(const std::string& directory)
{
    std::error_code ec;
    fs::space_info space = fs::space(directory, ec);
    if ((ec) || (space.available == static_cast<std::uintmax_t>(-1)))
    {
        return std::string();
    }
    return FormatFreeSpace(space.available);
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
    up.size = 0;
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
        entry.size = 0;

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
            std::uintmax_t size = it->file_size(entryEc);
            if (!entryEc)
            {
                entry.size = size;
            }
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
    if (mWordStar == true)
    {
        return 20;
    }

    int longest = 4;
    for (const sEntry& entry : mEntries)
    {
        const int nameWidth = wordstartui::cUtf8Helper::TextWidth(entry.name);
        if (nameWidth > longest)
        {
            longest = nameWidth;
        }
    }

    int width = longest + 2;
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
/// @return the number of grid rows
///
/// @brief
/// Row count of the grid: ceil(entries / columns).
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
/// @param  int index [in] entry index
///
/// @return the grid row of the entry
///
/// @brief
/// Row-major in the WordStar layout, column-major otherwise.
///
/////////////////////////////////////////////////////////////////////////////
int cFileBrowser::GridRow(int index) const
{
    if (mWordStar == true)
    {
        return index / ColumnCount();
    }
    return index % NumRows();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int gridRow [in] grid row
/// @param  int gridCol [in] grid column
///
/// @return the entry index at that grid position (may be past the end)
///
/// @brief
/// Row-major in the WordStar layout, column-major otherwise.
///
/////////////////////////////////////////////////////////////////////////////
int cFileBrowser::IndexAt(int gridRow, int gridCol) const
{
    if (mWordStar == true)
    {
        return (gridRow * ColumnCount()) + gridCol;
    }
    return (gridCol * NumRows()) + gridRow;
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
    int gridRow = GridRow(mSelected);
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
    if (mWordStar == true)
    {
        DrawWordStar(screen, theme, focused);
        return;
    }

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

        std::string name = wordstartui::cUtf8Helper::FitWidth(mEntries[static_cast<size_t>(index)].name, width - 1);

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
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
/// @param  bool focused [in] true to draw the selected entry as the bar
///
/// @return nothing
///
/// @brief
/// Draw the WordStar layout: per 20-cell column the name at cell column 1, a
/// '/' or the size ending at cell column 17, and a vertical bar at cell
/// column 19 between columns. Without focus no entry is marked.
///
/////////////////////////////////////////////////////////////////////////////
void cFileBrowser::DrawWordStar(cScreen& screen, const cTheme& theme, bool focused)
{
    sStyle normal = theme.GetStyle(wordstartui::THEME_ROLE_LIST);
    sStyle selected = theme.GetStyle(wordstartui::THEME_ROLE_LIST_SELECTED);
    selected.attrs = wordstartui::CELL_ATTR_NONE;

    screen.FillRect(mBounds, " ", normal);

    const int width = ColumnWidth();
    const int columns = ColumnCount();
    const int visible = VisibleRows();
    const int count = static_cast<int>(mEntries.size());

    for (int line = 0; line < visible; ++line)
    {
        const int screenRow = mBounds.row + line;
        const int gridRow = mScrollRow + line;

        for (int gridCol = 0; gridCol < columns; ++gridCol)
        {
            const int screenCol = mBounds.col + (gridCol * width);

            if (gridCol < (columns - 1))
            {
                screen.PutCell(screenRow, screenCol + 19, "\xe2\x94\x82", normal);   // U+2502
            }

            const int index = IndexAt(gridRow, gridCol);
            if (index >= count)
            {
                continue;
            }

            const sEntry& entry = mEntries[static_cast<size_t>(index)];

            sStyle style = normal;
            if ((index == mSelected) && (focused == true))
            {
                style = selected;
            }

            std::string name = entry.name;
            std::string suffix = FormatSize(entry.size);
            if (entry.isDirectory == true)
            {
                if ((name.empty() == false) && (name.back() == '/'))
                {
                    name.pop_back();
                }
                suffix = "/";
            }
            const int suffixWidth = wordstartui::cUtf8Helper::TextWidth(suffix);

            sRect cell;
            cell.row = screenRow;
            cell.col = screenCol;
            cell.rows = 1;
            cell.cols = 19;
            screen.FillRect(cell, " ", style);
            screen.PutText(screenRow, screenCol + 1, ShortenName(name, 16 - suffixWidth), style);
            screen.PutText(screenRow, screenCol + 18 - suffixWidth, suffix, style);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return the index of the entry drawn at row, col, or -1
///
/////////////////////////////////////////////////////////////////////////////
int cFileBrowser::EntryAt(int row, int col) const
{
    const int width = ColumnWidth();
    const int localRow = row - mBounds.row;
    const int localCol = col - mBounds.col;
    if ((width <= 0) || (localRow < 0) || (localRow >= VisibleRows()) ||
        (localCol < 0) || (localCol >= mBounds.cols))
    {
        return -1;
    }

    const int gridCol = localCol / width;
    if (gridCol >= ColumnCount())
    {
        return -1;
    }

    const int index = IndexAt(mScrollRow + localRow, gridCol);
    if ((index < 0) || (index >= static_cast<int>(mEntries.size())))
    {
        return -1;
    }
    return index;
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

    if ((event.type == wordstartui::INPUT_TYPE_SPECIAL) && (mWordStar == true))
    {
        // Row-major grid: Right/Left move one entry, Down/Up a whole row.
        const int column = mSelected % columns;
        const int page = VisibleRows() * columns;

        if (event.special == wordstartui::SPECIAL_KEY_ARROW_DOWN)
        {
            if ((mSelected + columns) < count)
            {
                mSelected += columns;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_UP)
        {
            if (mSelected < columns)
            {
                return ACTION_EXIT_TOP;
            }
            mSelected -= columns;
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_RIGHT)
        {
            if (mSelected < (count - 1))
            {
                mSelected++;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_LEFT)
        {
            if (mSelected > 0)
            {
                mSelected--;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_PAGE_DOWN)
        {
            // Same column, a screen of rows down, or its last row.
            int target = mSelected + page;
            if (target >= count)
            {
                target = (((count - 1) / columns) * columns) + column;
                if (target >= count)
                {
                    target -= columns;
                }
                if (target < mSelected)
                {
                    target = mSelected;
                }
            }
            mSelected = target;
            EnsureVisible();
            return ACTION_NONE;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_PAGE_UP)
        {
            // Same column, a screen of rows up, or the first row.
            mSelected -= page;
            if (mSelected < 0)
            {
                mSelected = column;
            }
            EnsureVisible();
            return ACTION_NONE;
        }
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
        int index = EntryAt(event.mouseRow, event.mouseCol);
        if (index >= 0)
        {
            mSelected = index;
            return Activate();
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
