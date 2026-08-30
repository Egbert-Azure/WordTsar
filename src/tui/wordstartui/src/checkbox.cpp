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

#include "checkbox.h"
#include "glyphs.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cCheckBox
///
/// @brief
/// Focusable checkbox with WordStar-friendly tri-state support.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] checkbox bounds
/// @param  const std::string& text [in] display text
/// @param  eTriState state [in] initial state
///
/// @return nothing
///
/// @brief
/// Construct a checkbox.
///
/////////////////////////////////////////////////////////////////////////////
cCheckBox::cCheckBox(const sRect& bounds, const std::string& text, eTriState state)
{
    mBounds = bounds;
    mText = text;
    mState = state;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the checkbox.
///
/////////////////////////////////////////////////////////////////////////////
void cCheckBox::Draw(cScreen& screen, const cTheme& theme)
{
    sStyle style;
    std::string mark;

    if (mHasFocus == true)
    {
        style = theme.GetStyle(THEME_ROLE_DIALOG_FOCUS);
    }
    else
    {
        style = theme.GetStyle(THEME_ROLE_DIALOG);
    }

    if (mState == TRI_STATE_ON)
    {
        mark = GLYPH_CHECK_ON;
    }
    else if (mState == TRI_STATE_INHERIT)
    {
        mark = GLYPH_CHECK_INHERIT;
    }
    else
    {
        mark = GLYPH_CHECK_OFF;
    }

    screen.PutText(mBounds.row, mBounds.col, mark + " " + mText, style);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event toggles the checkbox
///
/// @brief
/// Toggle the checkbox on Space or Enter.
///
/////////////////////////////////////////////////////////////////////////////
bool cCheckBox::HandleEvent(const sInputEvent& event)
{
    if (mEnabled == false)
    {
        return false;
    }

    if (IsLeftMousePressInside(event) == true)
    {
        Toggle();
        return true;
    }

    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ENTER))
    {
        Toggle();
        return true;
    }

    if ((event.type == INPUT_TYPE_TEXT) && (event.textUtf8 == " "))
    {
        Toggle();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because checkboxes can receive focus
///
/// @brief
/// Report checkbox focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cCheckBox::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eTriState state [in] new checkbox state
///
/// @return nothing
///
/// @brief
/// Set the checkbox state.
///
/////////////////////////////////////////////////////////////////////////////
void cCheckBox::SetState(eTriState state)
{
    mState = state;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return current checkbox state
///
/// @brief
/// Get the checkbox state.
///
/////////////////////////////////////////////////////////////////////////////
eTriState cCheckBox::GetState(void) const
{
    return mState;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Cycle through Off, On, and Inherit.
///
/////////////////////////////////////////////////////////////////////////////
void cCheckBox::Toggle(void)
{
    if (mState == TRI_STATE_OFF)
    {
        mState = TRI_STATE_ON;
    }
    else if (mState == TRI_STATE_ON)
    {
        mState = TRI_STATE_INHERIT;
    }
    else
    {
        mState = TRI_STATE_OFF;
    }
}

}
