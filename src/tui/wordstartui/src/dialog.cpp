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

#include "dialog.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cDialog
///
/// @brief
/// Modal dialog container with focus traversal and simple default keys.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] dialog bounds
/// @param  const std::string& title [in] dialog title
///
/// @return nothing
///
/// @brief
/// Construct an empty dialog.
///
/////////////////////////////////////////////////////////////////////////////
cDialog::cDialog(const sRect& bounds, const std::string& title)
{
    mBounds = bounds;
    mTitle = title;
    mFocusIndex = -1;
    mResult = DIALOG_RESULT_NONE;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::unique_ptr<cWidget> widget [in] widget owned by the dialog
///
/// @return nothing
///
/// @brief
/// Add a widget to the dialog.
///
/////////////////////////////////////////////////////////////////////////////
void cDialog::AddWidget(std::unique_ptr<cWidget> widget)
{
    mWidgets.push_back(std::move(widget));

    if (mFocusIndex < 0)
    {
        FocusFirst();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw dialog frame and contained widgets.
///
/////////////////////////////////////////////////////////////////////////////
void cDialog::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle dialog = theme.GetStyle(THEME_ROLE_DIALOG);
    sStyle title = theme.GetStyle(THEME_ROLE_DIALOG_TITLE);

    screen.FillRect(mBounds, " ", dialog);
    screen.DrawBox(mBounds, dialog);
    screen.PutText(mBounds.row, mBounds.col + 2, " " + mTitle + " ", title);

    screen.PushClip(mBounds);

    // Draw the focused widget last so an expanded control (e.g. an open
    // dropdown) overlays its neighbours rather than being drawn under them.
    for (int index = 0; index < static_cast<int>(mWidgets.size()); ++index)
    {
        if (index == mFocusIndex)
        {
            continue;
        }

        mWidgets[static_cast<size_t>(index)]->Draw(screen, theme);
    }

    if ((mFocusIndex >= 0) && (mFocusIndex < static_cast<int>(mWidgets.size())))
    {
        mWidgets[static_cast<size_t>(mFocusIndex)]->Draw(screen, theme);
    }

    screen.PopClip();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the dialog handled the event
///
/// @brief
/// Route input to focus traversal, cancellation, or the focused widget.
///
/////////////////////////////////////////////////////////////////////////////
bool cDialog::HandleEvent(const sInputEvent& event)
{
    if (event.type == INPUT_TYPE_MOUSE)
    {
        // A focused overlay widget (e.g. an open dropdown) claims clicks in its
        // expanded area first, even where it overlaps later widgets.
        cWidget* focused = GetFocusedWidget();

        if ((focused != nullptr) && (focused->ContainsEventPoint(event.mouseRow, event.mouseCol) == true))
        {
            if (focused->HandleEvent(event) == true)
            {
                return true;
            }
        }

        int widgetIndex = FindWidgetAt(event.mouseRow, event.mouseCol);

        if (widgetIndex >= 0)
        {
            if (mWidgets[static_cast<size_t>(widgetIndex)]->CanFocus() == true)
            {
                SetFocusIndex(widgetIndex);
            }

            if (mWidgets[static_cast<size_t>(widgetIndex)]->HandleEvent(event) == true)
            {
                return true;
            }
        }

        if (event.mouseRow < mBounds.row)
        {
            return true;
        }

        if (event.mouseRow >= (mBounds.row + mBounds.rows))
        {
            return true;
        }

        if (event.mouseCol < mBounds.col)
        {
            return true;
        }

        if (event.mouseCol >= (mBounds.col + mBounds.cols))
        {
            return true;
        }

        return false;
    }

    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ESCAPE))
    {
        mResult = DIALOG_RESULT_CANCEL;
        return true;
    }

    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_TAB))
    {
        if (event.shift == true)
        {
            FocusPrevious();
        }
        else
        {
            FocusNext();
        }
        return true;
    }

    cWidget* widget = GetFocusedWidget();

    if (widget != nullptr)
    {
        if (widget->HandleEvent(event) == true)
        {
            return true;
        }
    }

    // Enter confirms the dialog (default OK) when the focused widget -- a text
    // field or label -- did not consume it. A focused button handles its own
    // Enter above, so this only fires for non-activating widgets.
    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ENTER))
    {
        mResult = DIALOG_RESULT_OK;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eDialogResult result [in] new dialog result
///
/// @return nothing
///
/// @brief
/// Set the modal result.
///
/////////////////////////////////////////////////////////////////////////////
void cDialog::SetResult(eDialogResult result)
{
    mResult = result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return current dialog result
///
/// @brief
/// Get the modal result.
///
/////////////////////////////////////////////////////////////////////////////
eDialogResult cDialog::GetResult(void) const
{
    return mResult;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Focus the first focusable widget.
///
/////////////////////////////////////////////////////////////////////////////
void cDialog::FocusFirst(void)
{
    int index = FindNextFocusable(0, 1);
    SetFocusIndex(index);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return focused widget or nullptr
///
/// @brief
/// Get the currently focused widget.
///
/////////////////////////////////////////////////////////////////////////////
cWidget* cDialog::GetFocusedWidget(void)
{
    if ((mFocusIndex >= 0) && (mFocusIndex < static_cast<int>(mWidgets.size())))
    {
        return mWidgets[static_cast<size_t>(mFocusIndex)].get();
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move focus to the next focusable widget.
///
/////////////////////////////////////////////////////////////////////////////
void cDialog::FocusNext(void)
{
    int start = mFocusIndex + 1;
    int index = FindNextFocusable(start, 1);

    if (index < 0)
    {
        index = FindNextFocusable(0, 1);
    }

    SetFocusIndex(index);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move focus to the previous focusable widget.
///
/////////////////////////////////////////////////////////////////////////////
void cDialog::FocusPrevious(void)
{
    int start = mFocusIndex - 1;
    int index = FindNextFocusable(start, -1);

    if (index < 0)
    {
        index = FindNextFocusable(static_cast<int>(mWidgets.size()) - 1, -1);
    }

    SetFocusIndex(index);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int index [in] focus index
///
/// @return nothing
///
/// @brief
/// Change focus to a specific widget index.
///
/////////////////////////////////////////////////////////////////////////////
void cDialog::SetFocusIndex(int index)
{
    if ((mFocusIndex >= 0) && (mFocusIndex < static_cast<int>(mWidgets.size())))
    {
        mWidgets[static_cast<size_t>(mFocusIndex)]->SetFocus(false);
    }

    mFocusIndex = index;

    if ((mFocusIndex >= 0) && (mFocusIndex < static_cast<int>(mWidgets.size())))
    {
        mWidgets[static_cast<size_t>(mFocusIndex)]->SetFocus(true);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int start [in] starting index
/// @param  int step [in] search direction
///
/// @return focusable widget index or -1
///
/// @brief
/// Search for a focusable widget.
///
/////////////////////////////////////////////////////////////////////////////
int cDialog::FindNextFocusable(int start, int step) const
{
    int index = start;

    while ((index >= 0) && (index < static_cast<int>(mWidgets.size())))
    {
        if (mWidgets[static_cast<size_t>(index)]->CanFocus() == true)
        {
            return index;
        }

        index += step;
    }

    return -1;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] screen row
/// @param  int col [in] screen column
///
/// @return widget index or -1
///
/// @brief
/// Find the topmost widget containing a screen position.
///
/////////////////////////////////////////////////////////////////////////////
int cDialog::FindWidgetAt(int row, int col) const
{
    for (int index = static_cast<int>(mWidgets.size()) - 1; index >= 0; --index)
    {
        if (mWidgets[static_cast<size_t>(index)]->ContainsEventPoint(row, col) == true)
        {
            return index;
        }
    }

    return -1;
}

}
