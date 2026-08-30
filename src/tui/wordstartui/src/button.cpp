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

#include "button.h"
#include "glyphs.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cButton
///
/// @brief
/// Focusable push button with an activation callback.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] button bounds
/// @param  const std::string& text [in] button label
/// @param  std::function<void(void)> callback [in] activation callback
///
/// @return nothing
///
/// @brief
/// Construct a button.
///
/////////////////////////////////////////////////////////////////////////////
cButton::cButton(const sRect& bounds, const std::string& text, std::function<void(void)> callback)
{
    mBounds = bounds;
    mText = text;
    mCallback = callback;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the button.
///
/////////////////////////////////////////////////////////////////////////////
void cButton::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle style;

    if (mHasFocus == true)
    {
        style = theme.GetStyle(THEME_ROLE_BUTTON_FOCUS);
    }
    else
    {
        style = theme.GetStyle(THEME_ROLE_BUTTON);
    }

    // Filled chip: label centered with padding; focus adds accent caps.
    std::string label = " " + mText + " ";
    int width = static_cast<int>(mText.size()) + 2;

    if (mHasFocus == true)
    {
        label = std::string(GLYPH_BUTTON_LEFT) + " " + mText + " " + GLYPH_BUTTON_RIGHT;
        width = static_cast<int>(mText.size()) + 4;
    }

    int col = mBounds.col + ((mBounds.cols - width) / 2);

    if (col < mBounds.col)
    {
        col = mBounds.col;
    }

    screen.FillRect(mBounds, " ", style);
    screen.PutText(mBounds.row, col, label, style);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event activates the button
///
/// @brief
/// Handle Enter or Space as button activation.
///
/////////////////////////////////////////////////////////////////////////////
bool cButton::HandleEvent(const sInputEvent& event)
{
    if (mEnabled == false)
    {
        return false;
    }

    if (IsLeftMousePressInside(event) == true)
    {
        Press();
        return true;
    }

    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ENTER))
    {
        Press();
        return true;
    }

    if ((event.type == INPUT_TYPE_TEXT) && (event.textUtf8 == " "))
    {
        Press();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because buttons can receive focus
///
/// @brief
/// Report button focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cButton::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Activate the button callback.
///
/////////////////////////////////////////////////////////////////////////////
void cButton::Press(void)
{
    if (mCallback)
    {
        mCallback();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] new button label
///
/// @return nothing
///
/// @brief
/// Change the button label.
///
/////////////////////////////////////////////////////////////////////////////
void cButton::SetText(const std::string& text)
{
    mText = text;
}

}
