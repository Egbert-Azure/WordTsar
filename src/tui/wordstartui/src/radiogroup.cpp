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

#include "radiogroup.h"
#include "glyphs.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sRadioChoice
///
/// @brief
/// Stores one radio button choice.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @class cRadioGroup
///
/// @brief
/// Stores a focusable group of mutually exclusive radio choices.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] group bounds
///
/// @return nothing
///
/// @brief
/// Construct an empty radio group.
///
/////////////////////////////////////////////////////////////////////////////
cRadioGroup::cRadioGroup(const sRect& bounds)
{
    mBounds = bounds;
    mSelectedIndex = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] choice text
/// @param  int value [in] choice value
///
/// @return nothing
///
/// @brief
/// Add a choice to the radio group.
///
/////////////////////////////////////////////////////////////////////////////
void cRadioGroup::AddChoice(const std::string& text, int value)
{
    sRadioChoice choice;

    choice.text = text;
    choice.value = value;

    mChoices.push_back(choice);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw all radio choices.
///
/////////////////////////////////////////////////////////////////////////////
void cRadioGroup::Draw(cScreen& screen, const cTheme& theme)
{
    for (size_t index = 0; index < mChoices.size(); ++index)
    {
        sStyle style;
        std::string mark;

        if ((mHasFocus == true) && (static_cast<int>(index) == mSelectedIndex))
        {
            style = theme.GetStyle(THEME_ROLE_DIALOG_FOCUS);
        }
        else
        {
            style = theme.GetStyle(THEME_ROLE_DIALOG);
        }

        if (static_cast<int>(index) == mSelectedIndex)
        {
            mark = GLYPH_RADIO_ON;
        }
        else
        {
            mark = GLYPH_RADIO_OFF;
        }

        screen.PutText(mBounds.row + static_cast<int>(index), mBounds.col, mark + " " + mChoices[index].text, style);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event changes selection
///
/// @brief
/// Move the selected radio item using arrows, Space, or Enter.
///
/////////////////////////////////////////////////////////////////////////////
bool cRadioGroup::HandleEvent(const sInputEvent& event)
{
    if (mEnabled == false)
    {
        return false;
    }

    if (IsLeftMousePressInside(event) == true)
    {
        int index = event.mouseRow - mBounds.row;

        if ((index >= 0) && (index < static_cast<int>(mChoices.size())))
        {
            mSelectedIndex = index;
        }

        return true;
    }

    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ARROW_UP))
    {
        if (mSelectedIndex > 0)
        {
            --mSelectedIndex;
        }
        return true;
    }

    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ARROW_DOWN))
    {
        if (mSelectedIndex < (static_cast<int>(mChoices.size()) - 1))
        {
            ++mSelectedIndex;
        }
        return true;
    }

    if ((event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ENTER))
    {
        return true;
    }

    if ((event.type == INPUT_TYPE_TEXT) && (event.textUtf8 == " "))
    {
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true because radio groups can receive focus
///
/// @brief
/// Report focus capability.
///
/////////////////////////////////////////////////////////////////////////////
bool cRadioGroup::CanFocus(void) const
{
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return selected choice value or zero
///
/// @brief
/// Get the value stored by the selected choice.
///
/////////////////////////////////////////////////////////////////////////////
int cRadioGroup::GetSelectedValue(void) const
{
    if ((mSelectedIndex >= 0) && (mSelectedIndex < static_cast<int>(mChoices.size())))
    {
        return mChoices[static_cast<size_t>(mSelectedIndex)].value;
    }

    return 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int index [in] selected choice index
///
/// @return nothing
///
/// @brief
/// Set the selected choice index.
///
/////////////////////////////////////////////////////////////////////////////
void cRadioGroup::SetSelectedIndex(int index)
{
    if ((index >= 0) && (index < static_cast<int>(mChoices.size())))
    {
        mSelectedIndex = index;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return selected choice index
///
/// @brief
/// Get the selected choice index.
///
/////////////////////////////////////////////////////////////////////////////
int cRadioGroup::GetSelectedIndex(void) const
{
    return mSelectedIndex;
}

}
