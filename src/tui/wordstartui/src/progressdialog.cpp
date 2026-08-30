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

#include "progressdialog.h"
#include "screen.h"
#include "theme.h"

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct an empty progress dialog at 0%.
///
/////////////////////////////////////////////////////////////////////////////
cProgressDialog::cProgressDialog(void)
{
    mPercent = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& title [in] title shown on the top border
///
/// @return nothing
///
/// @brief
/// Set the dialog title.
///
/////////////////////////////////////////////////////////////////////////////
void cProgressDialog::SetTitle(const std::string& title)
{
    mTitle = title;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& message [in] one-line message (e.g. a filename)
///
/// @return nothing
///
/// @brief
/// Set the message line.
///
/////////////////////////////////////////////////////////////////////////////
void cProgressDialog::SetMessage(const std::string& message)
{
    mMessage = message;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int percent [in] progress 0..100 (clamped)
///
/// @return nothing
///
/// @brief
/// Set the percent-complete value.
///
/////////////////////////////////////////////////////////////////////////////
void cProgressDialog::SetPercent(int percent)
{
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }
    mPercent = percent;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] active colour theme
///
/// @return nothing
///
/// @brief
/// Draw the dialog centred on the screen: a bordered box with the title on the
/// top edge, the message line, and a percent bar.
///
/////////////////////////////////////////////////////////////////////////////
void cProgressDialog::Draw(cScreen& screen, const cTheme& theme) const
{
    sStyle dialog = theme.GetStyle(THEME_ROLE_DIALOG);
    sStyle title = theme.GetStyle(THEME_ROLE_DIALOG_TITLE);
    sStyle filled = theme.GetStyle(THEME_ROLE_LIST_SELECTED);
    sStyle track = theme.GetStyle(THEME_ROLE_FIELD);

    int screenRows = screen.GetRows();
    int screenCols = screen.GetCols();

    int innerWidth = static_cast<int>(mMessage.size());
    if (innerWidth < 30)
    {
        innerWidth = 30;
    }
    int maxInner = screenCols - 6;
    if ((maxInner > 0) && (innerWidth > maxInner))
    {
        innerWidth = maxInner;
    }
    if (innerWidth < 1)
    {
        innerWidth = 1;
    }

    int boxCols = innerWidth + 4;   // border + one pad column on each side
    int boxRows = 4;                // top border, message, bar, bottom border

    int boxCol = (screenCols - boxCols) / 2;
    int boxRow = (screenRows - boxRows) / 2;
    if (boxCol < 0)
    {
        boxCol = 0;
    }
    if (boxRow < 0)
    {
        boxRow = 0;
    }

    sRect box;
    box.row = boxRow;
    box.col = boxCol;
    box.rows = boxRows;
    box.cols = boxCols;
    screen.FillRect(box, " ", dialog);
    screen.DrawBox(box, dialog);

    if (mTitle.empty() == false)
    {
        screen.PutText(boxRow, boxCol + 2, " " + mTitle + " ", title);
    }

    int textCol = boxCol + 2;

    std::string msg = mMessage;
    if (static_cast<int>(msg.size()) > innerWidth)
    {
        msg = msg.substr(0, static_cast<size_t>(innerWidth));
    }
    screen.PutText(boxRow + 1, textCol, msg, dialog);

    // Percent bar: filled cells proportional to mPercent, followed by "NN%".
    std::string pctText = " " + std::to_string(mPercent) + "%";
    int barWidth = innerWidth - static_cast<int>(pctText.size());
    if (barWidth < 1)
    {
        barWidth = 1;
    }
    int filledCells = (barWidth * mPercent) / 100;

    int barRow = boxRow + 2;
    for (int i = 0; i < barWidth; ++i)
    {
        if (i < filledCells)
        {
            screen.PutCell(barRow, textCol + i, "\xe2\x96\x88", filled);   // full block
        }
        else
        {
            screen.PutCell(barRow, textCol + i, "\xe2\x96\x91", track);    // light shade
        }
    }
    screen.PutText(barRow, textCol + barWidth, pctText, dialog);
}

}
