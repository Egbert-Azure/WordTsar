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

#include "textfield.h"

#include "utf8helper.h"
#include <vector>

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTextField
///
/// @brief
/// Simple single-line UTF-8 text entry widget.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] field bounds
/// @param  const std::string& text [in] initial text
///
/// @return nothing
///
/// @brief
/// Construct a text field.
///
/////////////////////////////////////////////////////////////////////////////
cTextField::cTextField(const sRect& bounds, const std::string& text)
{
    mBounds = bounds;
    mText = text;
    mCursor = static_cast<int>(cUtf8Helper::SplitGraphemes(text).size());
    mScroll = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the visible text field contents.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle style;

    if (mHasFocus == true)
    {
        style = theme.GetStyle(THEME_ROLE_FIELD_FOCUS);
    }
    else
    {
        style = theme.GetStyle(THEME_ROLE_FIELD);
    }

    screen.FillRect(mBounds, " ", style);

    std::vector<std::string> graphemes = cUtf8Helper::SplitGraphemes(mText);
    int displayCol = 0;

    for (int index = mScroll; index < static_cast<int>(graphemes.size()); ++index)
    {
        if (displayCol >= mBounds.cols)
        {
            break;
        }

        screen.PutCell(mBounds.row, mBounds.col + displayCol, graphemes[static_cast<size_t>(index)], style);
        displayCol += cUtf8Helper::GraphemeWidth(graphemes[static_cast<size_t>(index)]);
    }

    // Paint a visible block cursor when focused: modal dialogs run without a
    // hardware cursor, so the field draws its own.
    if (mHasFocus == true)
    {
        int cursorCol = mCursor - mScroll;

        if ((cursorCol >= 0) && (cursorCol < mBounds.cols))
        {
            std::string under = " ";
            if (mCursor < static_cast<int>(graphemes.size()))
            {
                under = graphemes[static_cast<size_t>(mCursor)];
            }

            sStyle cursorStyle = style;
            cursorStyle.attrs = cursorStyle.attrs | CELL_ATTR_INVERSE;
            screen.PutCell(mBounds.row, mBounds.col + cursorCol, under, cursorStyle);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event edits the field
///
/// @brief
/// Handle basic single-line editing keys.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextField::HandleEvent(const sInputEvent& event)
{
    if (mEnabled == false)
    {
        return false;
    }

    if (event.type == INPUT_TYPE_MOUSE)
    {
        if (IsLeftMousePressInside(event) == true)
        {
            SetCursorFromColumn(event.mouseCol - mBounds.col);
            return true;
        }

        return false;
    }

    if (event.type == INPUT_TYPE_TEXT)
    {
        InsertText(event.textUtf8);
        return true;
    }

    if (event.type == INPUT_TYPE_SPECIAL)
    {
        if (event.special == SPECIAL_KEY_ARROW_LEFT)
        {
            MoveLeft();
            return true;
        }
        else if (event.special == SPECIAL_KEY_ARROW_RIGHT)
        {
            MoveRight();
            return true;
        }
        else if (event.special == SPECIAL_KEY_BACKSPACE)
        {
            Backspace();
            return true;
        }
        else if (event.special == SPECIAL_KEY_DELETE)
        {
            Delete();
            return true;
        }
        else if (event.special == SPECIAL_KEY_HOME)
        {
            mCursor = 0;
            ClampCursor();
            return true;
        }
        else if (event.special == SPECIAL_KEY_END)
        {
            mCursor = DisplayLength();
            ClampCursor();
            return true;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because text fields can receive focus
///
/// @brief
/// Report focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cTextField::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] new text
///
/// @return nothing
///
/// @brief
/// Replace the field text.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::SetText(const std::string& text)
{
    mText = text;
    mCursor = DisplayLength();
    ClampCursor();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return field text
///
/// @brief
/// Get the current field text.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextField::GetText(void) const
{
    return mText;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return cursor column relative to the field
///
/// @brief
/// Get the cursor column used by the application cursor.
///
/////////////////////////////////////////////////////////////////////////////
int cTextField::GetCursorColumn(void) const
{
    int column = mCursor - mScroll;

    if (column < 0)
    {
        column = 0;
    }

    if (column >= mBounds.cols)
    {
        column = mBounds.cols - 1;
    }

    return column;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move the insertion cursor left by one grapheme.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::MoveLeft(void)
{
    if (mCursor > 0)
    {
        --mCursor;
    }

    ClampCursor();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move the insertion cursor right by one grapheme.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::MoveRight(void)
{
    if (mCursor < DisplayLength())
    {
        ++mCursor;
    }

    ClampCursor();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Remove the grapheme before the insertion cursor.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::Backspace(void)
{
    if (mCursor <= 0)
    {
        return;
    }

    std::vector<std::string> graphemes = cUtf8Helper::SplitGraphemes(mText);
    graphemes.erase(graphemes.begin() + (mCursor - 1));
    --mCursor;

    mText.clear();

    for (const std::string& grapheme : graphemes)
    {
        mText += grapheme;
    }

    ClampCursor();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Remove the grapheme under the insertion cursor.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::Delete(void)
{
    std::vector<std::string> graphemes = cUtf8Helper::SplitGraphemes(mText);

    if (mCursor >= static_cast<int>(graphemes.size()))
    {
        return;
    }

    graphemes.erase(graphemes.begin() + mCursor);
    mText.clear();

    for (const std::string& grapheme : graphemes)
    {
        mText += grapheme;
    }

    ClampCursor();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] text to insert
///
/// @return nothing
///
/// @brief
/// Insert text at the cursor position.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::InsertText(const std::string& text)
{
    std::vector<std::string> graphemes = cUtf8Helper::SplitGraphemes(mText);
    std::vector<std::string> inserted = cUtf8Helper::SplitGraphemes(text);

    graphemes.insert(graphemes.begin() + mCursor, inserted.begin(), inserted.end());
    mCursor += static_cast<int>(inserted.size());

    mText.clear();

    for (const std::string& grapheme : graphemes)
    {
        mText += grapheme;
    }

    ClampCursor();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int column [in] clicked column relative to the field
///
/// @return nothing
///
/// @brief
/// Move the insertion cursor near a clicked field column.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::SetCursorFromColumn(int column)
{
    if (column < 0)
    {
        column = 0;
    }

    int targetColumn = mScroll + column;
    std::vector<std::string> graphemes = cUtf8Helper::SplitGraphemes(mText);
    int displayColumn = 0;
    mCursor = static_cast<int>(graphemes.size());

    for (size_t index = 0; index < graphemes.size(); ++index)
    {
        int width = cUtf8Helper::GraphemeWidth(graphemes[index]);

        if (targetColumn < (displayColumn + width))
        {
            mCursor = static_cast<int>(index);
            break;
        }

        displayColumn += width;
    }

    ClampCursor();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return number of graphemes in the field
///
/// @brief
/// Count field graphemes.
///
/////////////////////////////////////////////////////////////////////////////
int cTextField::DisplayLength(void) const
{
    return static_cast<int>(cUtf8Helper::SplitGraphemes(mText).size());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Keep cursor and scroll offsets inside the field.
///
/////////////////////////////////////////////////////////////////////////////
void cTextField::ClampCursor(void)
{
    const int length = DisplayLength();

    if (mCursor < 0)
    {
        mCursor = 0;
    }

    if (mCursor > length)
    {
        mCursor = length;
    }

    if (mCursor < mScroll)
    {
        mScroll = mCursor;
    }

    if (mCursor >= (mScroll + mBounds.cols))
    {
        mScroll = mCursor - mBounds.cols + 1;
    }

    if (mScroll < 0)
    {
        mScroll = 0;
    }
}

}
