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

#include "menubar.h"

#include <cctype>

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cMenuBar
///
/// @brief
/// A WordStar-style top menu bar with cascading pull-downs. Top items are direct
/// actions or pull-downs. A pull-down entry is a leaf action, a separator, or a
/// submenu that opens a flyout to the side. The whole bar (bar + open cascade)
/// is drawn in Draw(), so it should be drawn after the window content so its
/// pull-downs overlay neighbouring widgets.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct an empty, closed menu bar.
///
/////////////////////////////////////////////////////////////////////////////
cMenuBar::cMenuBar(void)
{
    mBounds.row = 0;
    mBounds.col = 0;
    mBounds.rows = 1;
    mBounds.cols = 0;
    mOpenMenu = -1;
    mScreenRows = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] menu bar bounds
///
/// @return nothing
///
/// @brief
/// Set menu bar bounds.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::SetBounds(const sRect& bounds)
{
    mBounds = bounds;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Remove all menus and entries (so the bar can be rebuilt, e.g. when the
/// keyboard mode changes). Bounds are preserved.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::Clear(void)
{
    mItems.clear();
    mSubHandles.clear();
    mOpenMenu = -1;
    mPath.clear();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& title [in] entry title (may contain a '&' accel)
///
/// @return a leaf/plain entry with the accelerator parsed out
///
/// @brief
/// Build an entry from a title, stripping a '&' accelerator marker and recording
/// the hotkey letter and its display position (falling back to the first
/// alphanumeric character when no '&' is given).
///
/////////////////////////////////////////////////////////////////////////////
sMenuEntry cMenuBar::MakeEntry(const std::string& title) const
{
    sMenuEntry entry;
    entry.hotKey = 0;
    entry.hotKeyPos = -1;
    entry.isSeparator = false;
    entry.enabled = true;

    // Split a trailing shortcut off the label: the text after the last run of
    // two-or-more spaces is the (right-aligned) shortcut. This keeps every
    // pull-down's shortcut column aligned regardless of label length.
    std::string trimmed = title;
    while ((trimmed.empty() == false) && (trimmed.back() == ' '))
    {
        trimmed.pop_back();
    }

    std::string namePart = trimmed;
    size_t gap = trimmed.rfind("  ");
    if (gap == std::string::npos)
    {
        // No wide gap: split off a trailing token only when it clearly looks
        // like a shortcut (^chord, .dot-command, or an F-key), so multi-word
        // names without a shortcut (e.g. "Word Count") are left intact.
        size_t sp = trimmed.rfind(' ');
        if ((sp != std::string::npos) && ((sp + 1) < trimmed.size()))
        {
            char lead = trimmed[sp + 1];
            bool looksLikeShortcut = (lead == '^') || (lead == '.') ||
                                     ((lead == 'F') && ((sp + 2) < trimmed.size()) &&
                                      (std::isdigit(static_cast<unsigned char>(trimmed[sp + 2])) != 0));
            if (looksLikeShortcut == true)
            {
                gap = sp;
            }
        }
    }

    if (gap != std::string::npos)
    {
        size_t after = gap;
        while ((after < trimmed.size()) && (trimmed[after] == ' '))
        {
            ++after;
        }
        entry.shortcut = trimmed.substr(after);

        namePart = trimmed.substr(0, gap);
        while ((namePart.empty() == false) && (namePart.back() == ' '))
        {
            namePart.pop_back();
        }
    }

    std::string display;
    for (size_t index = 0; index < namePart.size(); ++index)
    {
        bool isAccelerator = (namePart[index] == '&') && ((index + 1) < namePart.size()) &&
                             (entry.hotKey == 0) &&
                             (std::isalnum(static_cast<unsigned char>(namePart[index + 1])) != 0);
        if (isAccelerator == true)
        {
            entry.hotKey = static_cast<char>(std::tolower(static_cast<unsigned char>(namePart[index + 1])));
            entry.hotKeyPos = static_cast<int>(display.size());
            continue;
        }
        display.push_back(namePart[index]);
    }
    entry.title = display;

    if (entry.hotKey == 0)
    {
        for (size_t index = 0; index < display.size(); ++index)
        {
            if (std::isalnum(static_cast<unsigned char>(display[index])) != 0)
            {
                entry.hotKey = static_cast<char>(std::tolower(static_cast<unsigned char>(display[index])));
                entry.hotKeyPos = static_cast<int>(index);
                break;
            }
        }
    }

    return entry;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& title [in] menu title
/// @param  char hotKey [in] hotkey letter
/// @param  std::function<void(void)> callback [in] activation callback
///
/// @return nothing
///
/// @brief
/// Add a direct-action top-level item (no pull-down).
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::AddItem(const std::string& title, char hotKey, std::function<void(void)> callback)
{
    sMenuItem item;

    item.title = title;
    item.hotKey = static_cast<char>(std::tolower(static_cast<unsigned char>(hotKey)));
    item.callback = callback;

    mItems.push_back(item);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& title [in] menu title
/// @param  char hotKey [in] hotkey letter
///
/// @return the index of the new menu (use it with AddSubItem)
///
/// @brief
/// Add a pull-down top-level menu with no entries yet.
///
/////////////////////////////////////////////////////////////////////////////
int cMenuBar::AddMenu(const std::string& title, char hotKey)
{
    sMenuItem item;

    item.title = title;
    item.hotKey = static_cast<char>(std::tolower(static_cast<unsigned char>(hotKey)));

    mItems.push_back(item);

    return static_cast<int>(mItems.size()) - 1;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int menuIndex [in] menu returned by AddMenu
/// @param  const std::string& title [in] sub-item title
/// @param  std::function<void(void)> callback [in] activation callback
/// @param  bool enabled [in] false to draw the item dimmed and non-activatable
///
/// @return nothing
///
/// @brief
/// Add one leaf entry to a pull-down menu.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::AddSubItem(int menuIndex, const std::string& title,
                          std::function<void(void)> callback, bool enabled)
{
    if ((menuIndex < 0) || (menuIndex >= static_cast<int>(mItems.size())))
    {
        return;
    }

    sMenuEntry entry = MakeEntry(title);
    entry.callback = callback;
    entry.enabled = enabled;

    mItems[static_cast<size_t>(menuIndex)].entries.push_back(entry);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int menuIndex [in] menu returned by AddMenu
///
/// @return nothing
///
/// @brief
/// Add a horizontal separator line to a pull-down menu.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::AddSeparator(int menuIndex)
{
    if ((menuIndex < 0) || (menuIndex >= static_cast<int>(mItems.size())))
    {
        return;
    }

    sMenuEntry entry;
    entry.hotKey = 0;
    entry.hotKeyPos = -1;
    entry.isSeparator = true;
    entry.enabled = false;

    mItems[static_cast<size_t>(menuIndex)].entries.push_back(entry);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int menuIndex [in] menu returned by AddMenu
/// @param  const std::string& title [in] submenu title
///
/// @return a handle used with AddSubMenuItem / AddSubMenuSeparator
///
/// @brief
/// Add a cascading submenu entry to a top-level pull-down.
///
/////////////////////////////////////////////////////////////////////////////
int cMenuBar::AddSubMenu(int menuIndex, const std::string& title)
{
    if ((menuIndex < 0) || (menuIndex >= static_cast<int>(mItems.size())))
    {
        return -1;
    }

    sMenuEntry entry = MakeEntry(title);
    entry.enabled = true;

    std::vector<sMenuEntry>& entries = mItems[static_cast<size_t>(menuIndex)].entries;
    entries.push_back(entry);

    int entryIndex = static_cast<int>(entries.size()) - 1;
    mSubHandles.push_back(std::make_pair(menuIndex, entryIndex));

    return static_cast<int>(mSubHandles.size()) - 1;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int subHandle [in] handle returned by AddSubMenu
/// @param  const std::string& title [in] sub-item title
/// @param  std::function<void(void)> callback [in] activation callback
/// @param  bool enabled [in] false to draw dimmed and non-activatable
///
/// @return nothing
///
/// @brief
/// Add a leaf entry to a submenu.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::AddSubMenuItem(int subHandle, const std::string& title,
                              std::function<void(void)> callback, bool enabled)
{
    if ((subHandle < 0) || (subHandle >= static_cast<int>(mSubHandles.size())))
    {
        return;
    }

    int menuIndex = mSubHandles[static_cast<size_t>(subHandle)].first;
    int entryIndex = mSubHandles[static_cast<size_t>(subHandle)].second;

    sMenuEntry entry = MakeEntry(title);
    entry.callback = callback;
    entry.enabled = enabled;

    mItems[static_cast<size_t>(menuIndex)].entries[static_cast<size_t>(entryIndex)].submenu.push_back(entry);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int subHandle [in] handle returned by AddSubMenu
///
/// @return nothing
///
/// @brief
/// Add a horizontal separator line to a submenu.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::AddSubMenuSeparator(int subHandle)
{
    if ((subHandle < 0) || (subHandle >= static_cast<int>(mSubHandles.size())))
    {
        return;
    }

    int menuIndex = mSubHandles[static_cast<size_t>(subHandle)].first;
    int entryIndex = mSubHandles[static_cast<size_t>(subHandle)].second;

    sMenuEntry entry;
    entry.hotKey = 0;
    entry.hotKeyPos = -1;
    entry.isSeparator = true;
    entry.enabled = false;

    mItems[static_cast<size_t>(menuIndex)].entries[static_cast<size_t>(entryIndex)].submenu.push_back(entry);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int menuIndex [in] top-level menu index
///
/// @return the left screen column of that menu's " title " box
///
/// @brief
/// Compute where a top-level menu is drawn.
///
/////////////////////////////////////////////////////////////////////////////
int cMenuBar::MenuColumn(int menuIndex) const
{
    int col = mBounds.col + 1;

    for (int index = 0; index < menuIndex; ++index)
    {
        col += MenuTextWidth(index) + 2;
    }

    return col;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int menuIndex [in] top-level menu index
///
/// @return the width of the title label in columns
///
/// @brief
/// Width of a top-level menu label.
///
/////////////////////////////////////////////////////////////////////////////
int cMenuBar::MenuTextWidth(int menuIndex) const
{
    if ((menuIndex < 0) || (menuIndex >= static_cast<int>(mItems.size())))
    {
        return 0;
    }

    return static_cast<int>(mItems[static_cast<size_t>(menuIndex)].title.size());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<sMenuEntry>& entries [in] a pull-down's entries
///
/// @return the inner width (excluding the box border) in columns
///
/// @brief
/// Width of a pull-down, sized to the longest entry plus padding, leaving room
/// for the submenu indicator.
///
/////////////////////////////////////////////////////////////////////////////
int cMenuBar::DropWidth(const std::vector<sMenuEntry>& entries) const
{
    int width = 8;

    for (const sMenuEntry& entry : entries)
    {
        if (entry.isSeparator == true)
        {
            continue;
        }

        int nameLen = static_cast<int>(entry.title.size());
        int shortLen = static_cast<int>(entry.shortcut.size());
        int candidate;

        if (shortLen > 0)
        {
            candidate = nameLen + shortLen + 4;   // name + gap + shortcut + pads
        }
        else if (entry.submenu.empty() == false)
        {
            candidate = nameLen + 4;              // name + gap + indicator + pad
        }
        else
        {
            candidate = nameLen + 2;
        }

        if (candidate > width)
        {
            width = candidate;
        }
    }

    return width;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<sMenuEntry>& entries [in] a pull-down's entries
///
/// @return the first non-separator index, or 0
///
/// @brief
/// The first selectable (non-separator) entry to highlight when a menu opens.
///
/////////////////////////////////////////////////////////////////////////////
int cMenuBar::FirstSelectable(const std::vector<sMenuEntry>& entries) const
{
    for (int index = 0; index < static_cast<int>(entries.size()); ++index)
    {
        if (entries[static_cast<size_t>(index)].isSeparator == false)
        {
            return index;
        }
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int level [in] open depth (0 = top pull-down)
///
/// @return the entries at that depth following mPath, or nullptr
///
/// @brief
/// Walk mPath from the open top-level menu down through descended submenus to
/// the entry list shown at the given depth.
///
/////////////////////////////////////////////////////////////////////////////
const std::vector<sMenuEntry>* cMenuBar::ContainerAtLevel(int level) const
{
    if ((mOpenMenu < 0) || (mOpenMenu >= static_cast<int>(mItems.size())))
    {
        return nullptr;
    }

    const std::vector<sMenuEntry>* entries = &mItems[static_cast<size_t>(mOpenMenu)].entries;

    for (int lvl = 0; lvl < level; ++lvl)
    {
        if (lvl >= static_cast<int>(mPath.size()))
        {
            return nullptr;
        }

        int index = mPath[static_cast<size_t>(lvl)];

        if ((index < 0) || (index >= static_cast<int>(entries->size())))
        {
            return nullptr;
        }

        entries = &(*entries)[static_cast<size_t>(index)].submenu;
    }

    return entries;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int level [in] open depth (0 = top pull-down)
///
/// @return the bordered box rectangle for that depth's pull-down
///
/// @brief
/// Compute the on-screen box for each open cascade level. Level 0 sits below its
/// top-level label; deeper levels flyout to the right of the highlighted parent
/// entry (or to the left when there is no room), aligned with that entry.
///
/////////////////////////////////////////////////////////////////////////////
sRect cMenuBar::LevelRect(int level) const
{
    sRect rect;
    rect.row = 0;
    rect.col = 0;
    rect.rows = 0;
    rect.cols = 0;

    const std::vector<sMenuEntry>* entries = ContainerAtLevel(0);
    if (entries == nullptr)
    {
        return rect;
    }

    int screenRight = mBounds.col + mBounds.cols;

    int innerWidth = DropWidth(*entries);
    int boxWidth = innerWidth + 2;
    int startCol = MenuColumn(mOpenMenu);
    if ((startCol + boxWidth) > screenRight)
    {
        startCol = screenRight - boxWidth;
    }
    if (startCol < mBounds.col)
    {
        startCol = mBounds.col;
    }

    sRect cur;
    cur.row = mBounds.row + 1;
    cur.col = startCol;
    cur.cols = boxWidth;
    cur.rows = static_cast<int>(entries->size()) + 2;

    for (int lvl = 1; lvl <= level; ++lvl)
    {
        const std::vector<sMenuEntry>* child = ContainerAtLevel(lvl);
        if (child == nullptr)
        {
            break;
        }

        int parentIndex = mPath[static_cast<size_t>(lvl - 1)];
        int parentRow = cur.row + 1 + parentIndex;

        int cInner = DropWidth(*child);
        int cWidth = cInner + 2;

        sRect next;
        next.cols = cWidth;
        next.rows = static_cast<int>(child->size()) + 2;

        int rightEdge = cur.col + cur.cols;
        if ((rightEdge + cWidth) <= screenRight)
        {
            next.col = rightEdge;
        }
        else
        {
            next.col = cur.col - cWidth;
        }
        if (next.col < mBounds.col)
        {
            next.col = mBounds.col;
        }

        // Align the flyout's first item with the parent entry (border one above).
        next.row = parentRow - 1;
        if (next.row < (mBounds.row + 1))
        {
            next.row = mBounds.row + 1;
        }
        if ((mScreenRows > 0) && ((next.row + next.rows) > mScreenRows))
        {
            next.row = mScreenRows - next.rows;
            if (next.row < (mBounds.row + 1))
            {
                next.row = mBounds.row + 1;
            }
        }

        cur = next;
    }

    return cur;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the bar and, when a menu is open, its cascade of pull-downs.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::Draw(cScreen& screen, const cTheme& theme)
{
    mScreenRows = screen.GetRows();

    sStyle style = theme.GetStyle(THEME_ROLE_MENU);
    sStyle active = theme.GetStyle(THEME_ROLE_MENU_ACTIVE);
    sStyle accel = theme.GetStyle(THEME_ROLE_MENU_ACCEL);

    screen.FillRect(mBounds, " ", style);

    for (int menuIndex = 0; menuIndex < static_cast<int>(mItems.size()); ++menuIndex)
    {
        const sMenuItem& item = mItems[static_cast<size_t>(menuIndex)];
        int col = MenuColumn(menuIndex);
        int width = static_cast<int>(item.title.size());

        if ((col + width) > (mBounds.col + mBounds.cols))
        {
            break;
        }

        sStyle labelStyle = style;

        if (menuIndex == mOpenMenu)
        {
            labelStyle = active;
        }

        screen.PutText(mBounds.row, col, item.title, labelStyle);

        if (menuIndex != mOpenMenu)
        {
            for (size_t index = 0; index < item.title.size(); ++index)
            {
                char current = static_cast<char>(std::tolower(static_cast<unsigned char>(item.title[index])));

                if (current == item.hotKey)
                {
                    screen.PutText(mBounds.row, col + static_cast<int>(index), item.title.substr(index, 1), accel);
                    break;
                }
            }
        }
    }

    if (mOpenMenu < 0)
    {
        return;
    }

    // Draw each open cascade level, shallow first so flyouts overlay parents.
    for (int level = 0; level < static_cast<int>(mPath.size()); ++level)
    {
        const std::vector<sMenuEntry>* entries = ContainerAtLevel(level);
        if ((entries == nullptr) || (entries->empty() == true))
        {
            continue;
        }

        sRect box = LevelRect(level);
        int innerWidth = box.cols - 2;
        int highlight = mPath[static_cast<size_t>(level)];

        screen.FillRect(box, " ", style);
        screen.DrawBox(box, style);

        for (int index = 0; index < static_cast<int>(entries->size()); ++index)
        {
            const sMenuEntry& entry = (*entries)[static_cast<size_t>(index)];
            int entryRow = box.row + 1 + index;

            if (entry.isSeparator == true)
            {
                for (int c = box.col + 1; c < (box.col + box.cols - 1); ++c)
                {
                    screen.PutCell(entryRow, c, "\xe2\x94\x80", style);   // horizontal
                }
                screen.PutCell(entryRow, box.col, "\xe2\x94\x9c", style);              // left tee
                screen.PutCell(entryRow, box.col + box.cols - 1, "\xe2\x94\xa4", style); // right tee
                continue;
            }

            sStyle rowStyle = style;
            if (index == highlight)
            {
                rowStyle = active;
            }
            if (entry.enabled == false)
            {
                rowStyle.attrs = rowStyle.attrs | CELL_ATTR_DIM;
            }

            sRect lineRect;
            lineRect.row = entryRow;
            lineRect.col = box.col + 1;
            lineRect.rows = 1;
            lineRect.cols = innerWidth;
            screen.FillRect(lineRect, " ", rowStyle);

            screen.PutText(entryRow, box.col + 2, entry.title, rowStyle);

            if ((entry.enabled == true) && (entry.hotKeyPos >= 0) &&
                (entry.hotKeyPos < static_cast<int>(entry.title.size())))
            {
                sStyle hotStyle = rowStyle;
                hotStyle.fg = accel.fg;
                hotStyle.attrs = hotStyle.attrs | CELL_ATTR_BOLD;
                std::string letter = entry.title.substr(static_cast<size_t>(entry.hotKeyPos), 1);
                screen.PutCell(entryRow, box.col + 2 + entry.hotKeyPos, letter, hotStyle);
            }

            if (entry.shortcut.empty() == false)
            {
                int scCol = box.col + box.cols - 1 - static_cast<int>(entry.shortcut.size());
                screen.PutText(entryRow, scCol, entry.shortcut, rowStyle);
            }

            if (entry.submenu.empty() == false)
            {
                screen.PutCell(entryRow, box.col + box.cols - 2, "\xe2\x96\xb6", rowStyle);   // right triangle
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event was consumed
///
/// @brief
/// Drive menu navigation: open/close, move within a pull-down, descend/ascend
/// submenus, and activate entries.
///
/////////////////////////////////////////////////////////////////////////////
bool cMenuBar::HandleEvent(const sInputEvent& event)
{
    // ----- Mouse -----
    if (event.type == INPUT_TYPE_MOUSE)
    {
        if ((event.mouseAction != MOUSE_ACTION_PRESS) || (event.mouseButton != MOUSE_BUTTON_LEFT))
        {
            return false;
        }

        // Click on the bar row: activate/toggle a top-level menu.
        if (event.mouseRow == mBounds.row)
        {
            for (int menuIndex = 0; menuIndex < static_cast<int>(mItems.size()); ++menuIndex)
            {
                int col = MenuColumn(menuIndex);
                int endCol = col + MenuTextWidth(menuIndex);

                if ((event.mouseCol >= col) && (event.mouseCol < endCol))
                {
                    if (mItems[static_cast<size_t>(menuIndex)].entries.empty() == true)
                    {
                        Close();
                        if (mItems[static_cast<size_t>(menuIndex)].callback)
                        {
                            mItems[static_cast<size_t>(menuIndex)].callback();
                        }
                    }
                    else if (mOpenMenu == menuIndex)
                    {
                        Close();
                    }
                    else
                    {
                        OpenMenu(menuIndex);
                    }

                    return true;
                }
            }

            return false;
        }

        // Click inside one of the open cascade boxes (deepest first).
        if (mOpenMenu >= 0)
        {
            for (int level = static_cast<int>(mPath.size()) - 1; level >= 0; --level)
            {
                const std::vector<sMenuEntry>* entries = ContainerAtLevel(level);
                if ((entries == nullptr) || (entries->empty() == true))
                {
                    continue;
                }

                sRect box = LevelRect(level);
                int row = event.mouseRow - (box.row + 1);
                bool inBox = (row >= 0) && (row < static_cast<int>(entries->size())) &&
                             (event.mouseCol > box.col) && (event.mouseCol < (box.col + box.cols - 1));

                if (inBox == false)
                {
                    continue;
                }

                const sMenuEntry& entry = (*entries)[static_cast<size_t>(row)];
                if (entry.isSeparator == true)
                {
                    return true;
                }

                mPath.resize(static_cast<size_t>(level) + 1);
                mPath[static_cast<size_t>(level)] = row;
                ActivateHighlighted();
                return true;
            }

            // A click outside every open box dismisses the menu.
            Close();
            return false;
        }

        return false;
    }

    // ----- F10 opens the first pull-down -----
    if (event.type == INPUT_TYPE_FUNCTION)
    {
        if ((event.functionKey == 10) && (mItems.empty() == false))
        {
            if (mOpenMenu >= 0)
            {
                Close();
            }
            else
            {
                OpenMenu(0);
            }
            return true;
        }

        return false;
    }

    // ----- Keyboard while a menu is open -----
    if (mOpenMenu >= 0)
    {
        if (event.type == INPUT_TYPE_SPECIAL)
        {
            if (event.special == SPECIAL_KEY_ARROW_UP)
            {
                MoveHighlight(-1);
                return true;
            }
            else if (event.special == SPECIAL_KEY_ARROW_DOWN)
            {
                MoveHighlight(1);
                return true;
            }
            else if (event.special == SPECIAL_KEY_ARROW_LEFT)
            {
                Ascend();
                return true;
            }
            else if (event.special == SPECIAL_KEY_ARROW_RIGHT)
            {
                Descend();
                return true;
            }
            else if (event.special == SPECIAL_KEY_ENTER)
            {
                ActivateHighlighted();
                return true;
            }
            else if (event.special == SPECIAL_KEY_ESCAPE)
            {
                Close();
                return true;
            }
        }

        // A plain letter activates the entry with that accelerator at the
        // current depth. Other letters are consumed so they do not leak to the
        // editor while the menu is open.
        if ((event.type == INPUT_TYPE_TEXT) && (event.alt == false) && (event.textUtf8.empty() == false))
        {
            char pressed = static_cast<char>(std::tolower(static_cast<unsigned char>(event.textUtf8[0])));
            const std::vector<sMenuEntry>* entries = ContainerAtLevel(static_cast<int>(mPath.size()) - 1);

            if (entries != nullptr)
            {
                for (int index = 0; index < static_cast<int>(entries->size()); ++index)
                {
                    const sMenuEntry& entry = (*entries)[static_cast<size_t>(index)];
                    if ((entry.isSeparator == false) && (entry.enabled == true) && (entry.hotKey == pressed))
                    {
                        mPath.back() = index;
                        ActivateHighlighted();
                        return true;
                    }
                }
            }
            return true;
        }
    }

    // ----- Alt-letter activates a top-level menu (open or switch) -----
    if ((event.type == INPUT_TYPE_TEXT) && (event.alt == true) && (event.textUtf8.empty() == false))
    {
        char pressed = static_cast<char>(std::tolower(static_cast<unsigned char>(event.textUtf8[0])));

        for (int menuIndex = 0; menuIndex < static_cast<int>(mItems.size()); ++menuIndex)
        {
            if (pressed == mItems[static_cast<size_t>(menuIndex)].hotKey)
            {
                if (mItems[static_cast<size_t>(menuIndex)].entries.empty() == true)
                {
                    Close();
                    if (mItems[static_cast<size_t>(menuIndex)].callback)
                    {
                        mItems[static_cast<size_t>(menuIndex)].callback();
                    }
                }
                else
                {
                    OpenMenu(menuIndex);
                }

                return true;
            }
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when a pull-down is showing
///
/// @brief
/// Report whether a menu is currently open (so the host can route keys here).
///
/////////////////////////////////////////////////////////////////////////////
bool cMenuBar::IsOpen(void) const
{
    return (mOpenMenu >= 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Close any open pull-down.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::Close(void)
{
    mOpenMenu = -1;
    mPath.clear();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int menuIndex [in] menu to open
///
/// @return nothing
///
/// @brief
/// Open a pull-down menu with its first selectable entry highlighted.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::OpenMenu(int menuIndex)
{
    if ((menuIndex < 0) || (menuIndex >= static_cast<int>(mItems.size())))
    {
        return;
    }

    if (mItems[static_cast<size_t>(menuIndex)].entries.empty() == true)
    {
        return;
    }

    mOpenMenu = menuIndex;
    mPath.clear();
    mPath.push_back(FirstSelectable(mItems[static_cast<size_t>(menuIndex)].entries));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int delta [in] signed row movement
///
/// @return nothing
///
/// @brief
/// Move the highlight within the deepest open pull-down, skipping separators.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::MoveHighlight(int delta)
{
    if (mPath.empty() == true)
    {
        return;
    }

    const std::vector<sMenuEntry>* entries = ContainerAtLevel(static_cast<int>(mPath.size()) - 1);
    if ((entries == nullptr) || (entries->empty() == true))
    {
        return;
    }

    int count = static_cast<int>(entries->size());
    int index = mPath.back();
    int step = (delta >= 0) ? 1 : -1;

    for (int guard = 0; guard < count; ++guard)
    {
        index += step;

        if (index < 0)
        {
            index = count - 1;
        }
        if (index >= count)
        {
            index = 0;
        }

        if ((*entries)[static_cast<size_t>(index)].isSeparator == false)
        {
            mPath.back() = index;
            return;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int delta [in] signed menu movement (left/right)
///
/// @return nothing
///
/// @brief
/// Switch to the previous/next top-level pull-down menu, skipping direct-action
/// items.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::MoveMenu(int delta)
{
    if (mItems.empty() == true)
    {
        return;
    }

    int count = static_cast<int>(mItems.size());
    int index = mOpenMenu;

    for (int step = 0; step < count; ++step)
    {
        index += delta;

        if (index < 0)
        {
            index = count - 1;
        }
        if (index >= count)
        {
            index = 0;
        }

        if (mItems[static_cast<size_t>(index)].entries.empty() == false)
        {
            OpenMenu(index);
            return;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Enter the highlighted submenu, or (at the top level) move to the next menu.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::Descend(void)
{
    if (mPath.empty() == true)
    {
        return;
    }

    const std::vector<sMenuEntry>* entries = ContainerAtLevel(static_cast<int>(mPath.size()) - 1);
    if (entries == nullptr)
    {
        return;
    }

    int index = mPath.back();
    if ((index >= 0) && (index < static_cast<int>(entries->size())) &&
        ((*entries)[static_cast<size_t>(index)].submenu.empty() == false))
    {
        const std::vector<sMenuEntry>& child = (*entries)[static_cast<size_t>(index)].submenu;
        mPath.push_back(FirstSelectable(child));
        return;
    }

    if (mPath.size() == 1)
    {
        MoveMenu(1);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Leave the current submenu, or (at the top level) move to the previous menu.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::Ascend(void)
{
    if (mPath.size() > 1)
    {
        mPath.pop_back();
        return;
    }

    MoveMenu(-1);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Act on the deepest highlighted entry: descend into a submenu, or fire a leaf
/// entry's callback and close the menu. Separators and disabled entries do
/// nothing.
///
/////////////////////////////////////////////////////////////////////////////
void cMenuBar::ActivateHighlighted(void)
{
    if (mPath.empty() == true)
    {
        return;
    }

    const std::vector<sMenuEntry>* entries = ContainerAtLevel(static_cast<int>(mPath.size()) - 1);
    if (entries == nullptr)
    {
        return;
    }

    int index = mPath.back();
    if ((index < 0) || (index >= static_cast<int>(entries->size())))
    {
        return;
    }

    const sMenuEntry& entry = (*entries)[static_cast<size_t>(index)];

    if (entry.isSeparator == true)
    {
        return;
    }

    if (entry.submenu.empty() == false)
    {
        Descend();
        return;
    }

    if (entry.enabled == false)
    {
        return;
    }

    std::function<void(void)> callback = entry.callback;

    Close();

    if (callback)
    {
        callback();
    }
}

}
