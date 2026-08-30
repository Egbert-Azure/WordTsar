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

#include "label.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cLabel
///
/// @brief
/// Displays static text in a dialog or window.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sRect& bounds [in] label bounds
/// @param  const std::string& text [in] label text
///
/// @return nothing
///
/// @brief
/// Construct a label.
///
/////////////////////////////////////////////////////////////////////////////
cLabel::cLabel(const sRect& bounds, const std::string& text)
{
    mBounds = bounds;
    mText = text;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the label.
///
/////////////////////////////////////////////////////////////////////////////
void cLabel::Draw(cScreen& screen, const cTheme& theme)
{
    screen.PutText(mBounds.row, mBounds.col, mText, theme.GetStyle(THEME_ROLE_DIALOG));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return false because labels do not handle input
///
/// @brief
/// Ignore input.
///
/////////////////////////////////////////////////////////////////////////////
bool cLabel::HandleEvent(const sInputEvent& event)
{
    (void)event;
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] new label text
///
/// @return nothing
///
/// @brief
/// Change the label text.
///
/////////////////////////////////////////////////////////////////////////////
void cLabel::SetText(const std::string& text)
{
    mText = text;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return current label text
///
/// @brief
/// Get the label text.
///
/////////////////////////////////////////////////////////////////////////////
std::string cLabel::GetText(void) const
{
    return mText;
}

}
