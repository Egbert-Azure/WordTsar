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

#include "dialogs.h"

#include "src/tui/dialogs/dialoghost.h"
#include "src/tui/dialogs/filebrowser.h"

#include "src/core/utils/config.h"

#include "src/tui/wordstartui/src/button.h"
#include "src/tui/wordstartui/src/checkbox.h"
#include "src/tui/wordstartui/src/colorfield2d.h"
#include "src/tui/wordstartui/src/colorswatch.h"
#include "src/tui/wordstartui/src/colorutils.h"
#include "src/tui/wordstartui/src/dialog.h"
#include "src/tui/wordstartui/src/dropdown.h"
#include "src/tui/wordstartui/src/huebar.h"
#include "src/tui/wordstartui/src/label.h"
#include "src/tui/wordstartui/src/listbox.h"
#include "src/tui/wordstartui/src/radiogroup.h"
#include "src/tui/wordstartui/src/screen.h"
#include "src/tui/wordstartui/src/scrolltextview.h"
#include "src/tui/wordstartui/src/tabbar.h"
#include "src/tui/wordstartui/src/textfield.h"
#include "src/tui/wordstartui/src/theme.h"
#include "src/tui/wordstartui/src/tuidefs.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

using namespace wordstartui;

namespace wsdialogs
{

namespace
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] source text
///
/// @return the text split into lines on newline characters
///
/// @brief
/// Split a message into individual lines for display in stacked labels.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> SplitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string current;

    for (char ch : text)
    {
        if (ch == '\n')
        {
            lines.push_back(current);
            current.clear();
        }
        else if (ch != '\r')
        {
            current.push_back(ch);
        }
    }

    lines.push_back(current);
    return lines;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  int rows [in] desired dialog height
/// @param  int cols [in] desired dialog width
///
/// @return a rectangle centred on the current screen
///
/// @brief
/// Compute a centred rectangle sized to fit within the terminal.
///
/////////////////////////////////////////////////////////////////////////////
sRect CenteredRect(iWSDialogHost* host, int rows, int cols)
{
    sTerminalSize size = host->HostScreenSize();

    if (cols > size.cols)
    {
        cols = size.cols;
    }

    if (rows > size.rows)
    {
        rows = size.rows;
    }

    sRect rect;
    rect.rows = rows;
    rect.cols = cols;
    rect.row = (size.rows - rows) / 2;
    rect.col = (size.cols - cols) / 2;

    if (rect.row < 0)
    {
        rect.row = 0;
    }

    if (rect.col < 0)
    {
        rect.col = 0;
    }

    return rect;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<std::string>& lines [in] text lines
///
/// @return the length of the longest line
///
/// @brief
/// Return the widest line length used to size a dialog.
///
/////////////////////////////////////////////////////////////////////////////
int WidestLine(const std::vector<std::string>& lines)
{
    int widest = 0;

    for (const std::string& line : lines)
    {
        int len = static_cast<int>(line.size());

        if (len > widest)
        {
            widest = len;
        }
    }

    return widest;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] numeric text
///
/// @return the parsed integer, or 0 when parsing fails
///
/// @brief
/// Parse a signed integer from user text without throwing.
///
/////////////////////////////////////////////////////////////////////////////
int ParseInt(const std::string& text)
{
    int value = 0;
    bool negative = false;
    bool started = false;

    for (char ch : text)
    {
        if ((started == false) && (ch == '-'))
        {
            negative = true;
            started = true;
        }
        else if ((ch >= '0') && (ch <= '9'))
        {
            value = (value * 10) + (ch - '0');
            started = true;
        }
        else if (ch == ' ')
        {
            continue;
        }
        else
        {
            break;
        }
    }

    if (negative == true)
    {
        value = -value;
    }

    return value;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int value [in] value to clamp
/// @param  int low [in] lower bound
/// @param  int high [in] upper bound
///
/// @return the value clamped to the inclusive range
///
/// @brief
/// Clamp an integer to a range.
///
/////////////////////////////////////////////////////////////////////////////
int Clamp(int value, int low, int high)
{
    if (value < low)
    {
        return low;
    }

    if (value > high)
    {
        return high;
    }

    return value;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool flag [in] flag value
///
/// @return TRI_STATE_ON when true, TRI_STATE_OFF otherwise
///
/// @brief
/// Convert a boolean to a checkbox tri-state.
///
/////////////////////////////////////////////////////////////////////////////
eTriState BoolToTri(bool flag)
{
    if (flag == true)
    {
        return TRI_STATE_ON;
    }

    return TRI_STATE_OFF;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const cCheckBox* box [in] checkbox
///
/// @return true when the checkbox is on
///
/// @brief
/// Read a checkbox state as a boolean.
///
/////////////////////////////////////////////////////////////////////////////
bool TriToBool(const cCheckBox* box)
{
    return box->GetState() == TRI_STATE_ON;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event is the Escape key
///
/// @brief
/// Test whether an input event is the Escape key.
///
/////////////////////////////////////////////////////////////////////////////
bool IsEscape(const sInputEvent& event)
{
    return (event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ESCAPE);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true when the event is the Enter key
///
/// @brief
/// Test whether an input event is the Enter key.
///
/////////////////////////////////////////////////////////////////////////////
bool IsEnter(const sInputEvent& event)
{
    return (event.type == INPUT_TYPE_SPECIAL) && (event.special == SPECIAL_KEY_ENTER);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] numeric text
///
/// @return the parsed double, or 0.0 when parsing fails
///
/// @brief
/// Parse a double from user text without throwing.
///
/////////////////////////////////////////////////////////////////////////////
double ParseDouble(const std::string& text)
{
    double value = 0.0;

    if (std::sscanf(text.c_str(), "%lf", &value) != 1)
    {
        return 0.0;
    }

    return value;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  short value [in] channel value
///
/// @return the value clamped to 0-255
///
/// @brief
/// Clamp a color channel to the 0-255 range as a short.
///
/////////////////////////////////////////////////////////////////////////////
short ClampChannel(int value)
{
    return static_cast<short>(Clamp(value, 0, 255));
}

}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::string& title [in] dialog title
/// @param  const std::string& message [in] message text
///
/// @return nothing
///
/// @brief
/// Display a one-button informational message box.
///
/////////////////////////////////////////////////////////////////////////////
void MessageBox(iWSDialogHost* host, const std::string& title, const std::string& message)
{
    std::vector<std::string> lines = SplitLines(message);

    int contentWidth = WidestLine(lines);
    int titleWidth = static_cast<int>(title.size());

    if (titleWidth > contentWidth)
    {
        contentWidth = titleWidth;
    }

    int width = contentWidth + 6;

    if (width < 20)
    {
        width = 20;
    }

    int height = static_cast<int>(lines.size()) + 5;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, title);

    int row = 2;

    for (const std::string& line : lines)
    {
        sRect labelRect;
        labelRect.row = rect.row + row;
        labelRect.col = rect.col + 2;
        labelRect.rows = 1;
        labelRect.cols = rect.cols - 4;
        dialog.AddWidget(std::make_unique<cLabel>(labelRect, line));
        row = row + 1;
    }

    sRect buttonRect;
    buttonRect.rows = 1;
    buttonRect.cols = 8;
    buttonRect.row = rect.row + rect.rows - 2;
    buttonRect.col = rect.col + ((rect.cols - buttonRect.cols) / 2);

    cDialog* dialogPtr = &dialog;
    dialog.AddWidget(std::make_unique<cButton>(buttonRect, "OK", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::string& title [in] dialog title
/// @param  const std::string& question [in] question text
///
/// @return true when Yes was pressed
///
/// @brief
/// Display a Yes/No confirmation dialog.
///
/////////////////////////////////////////////////////////////////////////////
bool YesNo(iWSDialogHost* host, const std::string& title, const std::string& question)
{
    std::vector<std::string> lines = SplitLines(question);

    int contentWidth = WidestLine(lines);
    int titleWidth = static_cast<int>(title.size());

    if (titleWidth > contentWidth)
    {
        contentWidth = titleWidth;
    }

    int width = contentWidth + 6;

    if (width < 24)
    {
        width = 24;
    }

    int height = static_cast<int>(lines.size()) + 5;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, title);

    int row = 2;

    for (const std::string& line : lines)
    {
        sRect labelRect;
        labelRect.row = rect.row + row;
        labelRect.col = rect.col + 2;
        labelRect.rows = 1;
        labelRect.cols = rect.cols - 4;
        dialog.AddWidget(std::make_unique<cLabel>(labelRect, line));
        row = row + 1;
    }

    cDialog* dialogPtr = &dialog;
    bool yes = false;
    bool* yesPtr = &yes;

    sRect yesRect;
    yesRect.rows = 1;
    yesRect.cols = 9;
    yesRect.row = rect.row + rect.rows - 2;
    yesRect.col = rect.col + ((rect.cols - 20) / 2);

    dialog.AddWidget(std::make_unique<cButton>(yesRect, "Yes", [dialogPtr, yesPtr]() {
        *yesPtr = true;
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect noRect;
    noRect.rows = 1;
    noRect.cols = 9;
    noRect.row = yesRect.row;
    noRect.col = yesRect.col + 11;

    dialog.AddWidget(std::make_unique<cButton>(noRect, "No", [dialogPtr, yesPtr]() {
        *yesPtr = false;
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    return yes;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::string& title [in] dialog title
/// @param  const std::string& message [in] message text
/// @param  const std::string& button0 [in] first button label
/// @param  const std::string& button1 [in] second button label
/// @param  const std::string& button2 [in] third button label
///
/// @return the index of the pressed button, or -1 on cancel
///
/// @brief
/// Display a three-button choice dialog.
///
/////////////////////////////////////////////////////////////////////////////
int ThreeChoice(iWSDialogHost* host, const std::string& title, const std::string& message,
                const std::string& button0, const std::string& button1, const std::string& button2)
{
    std::vector<std::string> lines = SplitLines(message);

    int buttonWidth = 12;
    int buttonsWidth = (buttonWidth * 3) + 4;

    int contentWidth = WidestLine(lines);

    if (buttonsWidth > contentWidth)
    {
        contentWidth = buttonsWidth;
    }

    int width = contentWidth + 6;
    int height = static_cast<int>(lines.size()) + 5;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, title);

    int row = 2;

    for (const std::string& line : lines)
    {
        sRect labelRect;
        labelRect.row = rect.row + row;
        labelRect.col = rect.col + 2;
        labelRect.rows = 1;
        labelRect.cols = rect.cols - 4;
        dialog.AddWidget(std::make_unique<cLabel>(labelRect, line));
        row = row + 1;
    }

    cDialog* dialogPtr = &dialog;
    int choice = -1;
    int* choicePtr = &choice;

    int totalButtons = (buttonWidth * 3) + 4;
    int startCol = rect.col + ((rect.cols - totalButtons) / 2);
    int buttonRow = rect.row + rect.rows - 2;

    sRect button0Rect;
    button0Rect.rows = 1;
    button0Rect.cols = buttonWidth;
    button0Rect.row = buttonRow;
    button0Rect.col = startCol;
    dialog.AddWidget(std::make_unique<cButton>(button0Rect, button0, [dialogPtr, choicePtr]() {
        *choicePtr = 0;
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect button1Rect;
    button1Rect.rows = 1;
    button1Rect.cols = buttonWidth;
    button1Rect.row = buttonRow;
    button1Rect.col = startCol + buttonWidth + 2;
    dialog.AddWidget(std::make_unique<cButton>(button1Rect, button1, [dialogPtr, choicePtr]() {
        *choicePtr = 1;
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect button2Rect;
    button2Rect.rows = 1;
    button2Rect.cols = buttonWidth;
    button2Rect.row = buttonRow;
    button2Rect.col = startCol + (2 * (buttonWidth + 2));
    dialog.AddWidget(std::make_unique<cButton>(button2Rect, button2, [dialogPtr, choicePtr]() {
        *choicePtr = 2;
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_CANCEL)
    {
        return -1;
    }

    return choice;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::string& title [in] dialog title
/// @param  const std::string& prompt [in] prompt text
/// @param  std::string& value [in,out] initial and returned text
///
/// @return true when OK was pressed
///
/// @brief
/// Display a single-line text input dialog.
///
/////////////////////////////////////////////////////////////////////////////
bool InputBox(iWSDialogHost* host, const std::string& title, const std::string& prompt,
              std::string& value)
{
    int promptWidth = static_cast<int>(prompt.size());
    int width = promptWidth + 8;

    if (width < 40)
    {
        width = 40;
    }

    int height = 8;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, title);

    sRect promptRect;
    promptRect.row = rect.row + 2;
    promptRect.col = rect.col + 2;
    promptRect.rows = 1;
    promptRect.cols = rect.cols - 4;
    dialog.AddWidget(std::make_unique<cLabel>(promptRect, prompt));

    sRect fieldRect;
    fieldRect.row = rect.row + 3;
    fieldRect.col = rect.col + 2;
    fieldRect.rows = 1;
    fieldRect.cols = rect.cols - 4;

    auto field = std::make_unique<cTextField>(fieldRect, value);
    cTextField* fieldPtr = field.get();
    dialog.AddWidget(std::move(field));

    cDialog* dialogPtr = &dialog;

    sRect okRect;
    okRect.rows = 1;
    okRect.cols = 10;
    okRect.row = rect.row + rect.rows - 2;
    okRect.col = rect.col + ((rect.cols - 24) / 2);
    dialog.AddWidget(std::make_unique<cButton>(okRect, "OK", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = okRect.row;
    cancelRect.col = okRect.col + 12;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        value = fieldPtr->GetText();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::string& title [in] dialog title
/// @param  bool saveMode [in] true to show a filename field
/// @param  const std::string& startPath [in] initial directory
/// @param  std::string& result [out] chosen path
///
/// @return true when a file was chosen
///
/// @brief
/// Display a directory-navigating file browser.
///
/////////////////////////////////////////////////////////////////////////////
bool FileBrowser(iWSDialogHost* host, const std::string& title, bool saveMode,
                 const std::string& startPath, std::string& result)
{
    sTerminalSize size = host->HostScreenSize();

    int width = (size.cols * 9) / 10;
    if (width > (size.cols - 2))
    {
        width = size.cols - 2;
    }
    if (width < 30)
    {
        width = 30;
    }

    int height = (size.rows * 8) / 10;
    if (height > (size.rows - 2))
    {
        height = size.rows - 2;
    }
    if (height < 8)
    {
        height = 8;
    }

    sRect rect;
    rect.rows = height;
    rect.cols = width;
    rect.row = (size.rows - height) / 2;
    rect.col = (size.cols - width) / 2;
    if (rect.row < 0)
    {
        rect.row = 0;
    }
    if (rect.col < 0)
    {
        rect.col = 0;
    }

    wsui::cFileBrowser browser;
    if (startPath.empty() == false)
    {
        browser.SetDirectory(startPath);
    }

    int gridRows = rect.rows - 4;
    if (saveMode == true)
    {
        gridRows = gridRows - 2;
    }
    if (gridRows < 1)
    {
        gridRows = 1;
    }

    sRect gridRect;
    gridRect.row = rect.row + 2;
    gridRect.col = rect.col + 2;
    gridRect.rows = gridRows;
    gridRect.cols = rect.cols - 4;
    browser.SetBounds(gridRect);

    sRect fieldRect;
    fieldRect.row = rect.row + rect.rows - 2;
    fieldRect.col = rect.col + 8;
    fieldRect.rows = 1;
    fieldRect.cols = rect.cols - 10;
    cTextField nameField(fieldRect, "");

    int focus = 0;
    bool accepted = false;

    sStyle dialogStyle = host->HostTheme().GetStyle(THEME_ROLE_DIALOG);
    sStyle titleStyle = host->HostTheme().GetStyle(THEME_ROLE_DIALOG_TITLE);

    auto draw = [&](cScreen& screen, const cTheme& theme)
    {
        screen.FillRect(rect, " ", dialogStyle);
        screen.DrawBox(rect, dialogStyle);
        screen.PutText(rect.row, rect.col + 2, " " + title + " ", titleStyle);

        std::string dir = browser.GetCurrentDirectory();
        int maxDir = rect.cols - 4;
        if ((maxDir > 0) && (static_cast<int>(dir.size()) > maxDir))
        {
            dir = dir.substr(dir.size() - static_cast<size_t>(maxDir));
        }
        screen.PutText(rect.row + 1, rect.col + 2, dir, dialogStyle);

        bool browserFocused = (focus == 0);
        browser.Draw(screen, theme, browserFocused);

        if (saveMode == true)
        {
            screen.PutText(rect.row + rect.rows - 2, rect.col + 2, "Name:", dialogStyle);
            nameField.SetFocus(focus == 1);
            nameField.Draw(screen, theme);
        }
    };

    auto handle = [&](const sInputEvent& event) -> bool
    {
        if (IsEscape(event) == true)
        {
            accepted = false;
            return false;
        }

        if ((saveMode == true) && (event.type == INPUT_TYPE_SPECIAL) &&
            (event.special == SPECIAL_KEY_TAB))
        {
            if (focus == 0)
            {
                focus = 1;
            }
            else
            {
                focus = 0;
            }
            return true;
        }

        if ((saveMode == true) && (focus == 1))
        {
            if (IsEnter(event) == true)
            {
                std::string name = nameField.GetText();
                if (name.empty() == false)
                {
                    result = (std::filesystem::path(browser.GetCurrentDirectory()) / name).string();
                    accepted = true;
                    return false;
                }
                return true;
            }
            nameField.HandleEvent(event);
            return true;
        }

        wsui::cFileBrowser::eAction action = browser.HandleEvent(event);
        if (action == wsui::cFileBrowser::ACTION_FILE)
        {
            result = browser.GetSelectedFile();
            accepted = true;
            return false;
        }
        if (action == wsui::cFileBrowser::ACTION_EXIT_TOP)
        {
            if (saveMode == true)
            {
                focus = 1;
            }
            return true;
        }
        return true;
    };

    host->HostRunModalRaw(draw, handle);
    return accepted;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::string& title [in] dialog title
/// @param  const std::vector<std::string>& lines [in] text lines
///
/// @return nothing
///
/// @brief
/// Display a scrollable read-only text viewer.
///
/////////////////////////////////////////////////////////////////////////////
void TextViewer(iWSDialogHost* host, const std::string& title,
                const std::vector<std::string>& lines)
{
    sTerminalSize size = host->HostScreenSize();

    int width = size.cols - 4;
    int height = size.rows - 2;

    if (width < 20)
    {
        width = 20;
    }

    if (height < 6)
    {
        height = 6;
    }

    sRect rect;
    rect.rows = height;
    rect.cols = width;
    rect.row = (size.rows - height) / 2;
    rect.col = (size.cols - width) / 2;

    if (rect.row < 0)
    {
        rect.row = 0;
    }

    if (rect.col < 0)
    {
        rect.col = 0;
    }

    sRect viewRect;
    viewRect.row = rect.row + 1;
    viewRect.col = rect.col + 1;
    viewRect.rows = rect.rows - 3;
    viewRect.cols = rect.cols - 2;

    if (viewRect.rows < 1)
    {
        viewRect.rows = 1;
    }

    cScrollTextView view(viewRect);
    view.SetLines(lines);
    view.SetFocus(true);

    sStyle dialogStyle = host->HostTheme().GetStyle(THEME_ROLE_DIALOG);
    sStyle titleStyle = host->HostTheme().GetStyle(THEME_ROLE_DIALOG_TITLE);

    auto draw = [&](cScreen& screen, const cTheme& theme) {
        screen.FillRect(rect, " ", dialogStyle);
        screen.DrawBox(rect, dialogStyle);
        screen.PutText(rect.row, rect.col + 2, " " + title + " ", titleStyle);
        screen.PutText(rect.row + rect.rows - 1, rect.col + 2, " Esc to close ", titleStyle);
        view.Draw(screen, theme);
    };

    auto handle = [&](const sInputEvent& event) -> bool {
        if (IsEscape(event) == true)
        {
            return false;
        }

        if (event.type == INPUT_TYPE_SPECIAL)
        {
            if ((event.special == SPECIAL_KEY_ARROW_UP) ||
                (event.special == SPECIAL_KEY_ARROW_DOWN) ||
                (event.special == SPECIAL_KEY_PAGE_UP) ||
                (event.special == SPECIAL_KEY_PAGE_DOWN) ||
                (event.special == SPECIAL_KEY_HOME) ||
                (event.special == SPECIAL_KEY_END))
            {
                view.HandleEvent(event);
            }
        }

        return true;
    };

    host->HostRunModalRaw(draw, handle);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const sFindOptions& initial [in] initial values
///
/// @return the find options with ok set when Find was pressed
///
/// @brief
/// Display the Find dialog.
///
/////////////////////////////////////////////////////////////////////////////
sFindOptions FindDialog(iWSDialogHost* host, const sFindOptions& initial)
{
    sFindOptions options = initial;
    options.ok = false;

    int width = 54;
    int height = 15;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Find");

    sRect labelRect;
    labelRect.row = rect.row + 2;
    labelRect.col = rect.col + 2;
    labelRect.rows = 1;
    labelRect.cols = 12;
    dialog.AddWidget(std::make_unique<cLabel>(labelRect, "Find:"));

    sRect fieldRect;
    fieldRect.row = rect.row + 2;
    fieldRect.col = rect.col + 10;
    fieldRect.rows = 1;
    fieldRect.cols = rect.cols - 12;

    auto field = std::make_unique<cTextField>(fieldRect, initial.text);
    cTextField* fieldPtr = field.get();
    dialog.AddWidget(std::move(field));

    sRect wholeRect;
    wholeRect.row = rect.row + 4;
    wholeRect.col = rect.col + 2;
    wholeRect.rows = 1;
    wholeRect.cols = 22;
    auto wholeBox = std::make_unique<cCheckBox>(wholeRect, "Whole word", BoolToTri(initial.wholeWord));
    cCheckBox* wholePtr = wholeBox.get();
    dialog.AddWidget(std::move(wholeBox));

    sRect caseRect;
    caseRect.row = rect.row + 4;
    caseRect.col = rect.col + 26;
    caseRect.rows = 1;
    caseRect.cols = 22;
    auto caseBox = std::make_unique<cCheckBox>(caseRect, "Ignore case", BoolToTri(initial.ignoreCase));
    cCheckBox* casePtr = caseBox.get();
    dialog.AddWidget(std::move(caseBox));

    sRect backRect;
    backRect.row = rect.row + 5;
    backRect.col = rect.col + 2;
    backRect.rows = 1;
    backRect.cols = 22;
    auto backBox = std::make_unique<cCheckBox>(backRect, "Backward", BoolToTri(initial.backward));
    cCheckBox* backPtr = backBox.get();
    dialog.AddWidget(std::move(backBox));

    sRect wildRect;
    wildRect.row = rect.row + 5;
    wildRect.col = rect.col + 26;
    wildRect.rows = 1;
    wildRect.cols = 22;
    auto wildBox = std::make_unique<cCheckBox>(wildRect, "Wildcard", BoolToTri(initial.wildcard));
    cCheckBox* wildPtr = wildBox.get();
    dialog.AddWidget(std::move(wildBox));

    sRect scopeRect;
    scopeRect.row = rect.row + 7;
    scopeRect.col = rect.col + 2;
    scopeRect.rows = 2;
    scopeRect.cols = rect.cols - 4;
    auto scope = std::make_unique<cRadioGroup>(scopeRect);
    scope->AddChoice("This point forward", 0);
    scope->AddChoice("Entire document", 1);
    scope->SetSelectedIndex(Clamp(initial.scope, 0, 1));
    cRadioGroup* scopePtr = scope.get();
    dialog.AddWidget(std::move(scope));

    cDialog* dialogPtr = &dialog;

    sRect findRect;
    findRect.rows = 1;
    findRect.cols = 10;
    findRect.row = rect.row + rect.rows - 2;
    findRect.col = rect.col + ((rect.cols - 24) / 2);
    dialog.AddWidget(std::make_unique<cButton>(findRect, "Find", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = findRect.row;
    cancelRect.col = findRect.col + 12;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        options.text = fieldPtr->GetText();
        options.wholeWord = TriToBool(wholePtr);
        options.ignoreCase = TriToBool(casePtr);
        options.backward = TriToBool(backPtr);
        options.wildcard = TriToBool(wildPtr);
        options.scope = scopePtr->GetSelectedValue();
        options.ok = true;
    }

    return options;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const sReplaceOptions& initial [in] initial values
///
/// @return the replace options with ok set when Replace was pressed
///
/// @brief
/// Display the Replace dialog.
///
/////////////////////////////////////////////////////////////////////////////
sReplaceOptions ReplaceDialog(iWSDialogHost* host, const sReplaceOptions& initial)
{
    sReplaceOptions options = initial;
    options.ok = false;

    int width = 54;
    int height = 18;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Replace");

    sRect findLabelRect;
    findLabelRect.row = rect.row + 2;
    findLabelRect.col = rect.col + 2;
    findLabelRect.rows = 1;
    findLabelRect.cols = 12;
    dialog.AddWidget(std::make_unique<cLabel>(findLabelRect, "Find:"));

    sRect findFieldRect;
    findFieldRect.row = rect.row + 2;
    findFieldRect.col = rect.col + 12;
    findFieldRect.rows = 1;
    findFieldRect.cols = rect.cols - 14;
    auto findField = std::make_unique<cTextField>(findFieldRect, initial.find);
    cTextField* findPtr = findField.get();
    dialog.AddWidget(std::move(findField));

    sRect replaceLabelRect;
    replaceLabelRect.row = rect.row + 3;
    replaceLabelRect.col = rect.col + 2;
    replaceLabelRect.rows = 1;
    replaceLabelRect.cols = 12;
    dialog.AddWidget(std::make_unique<cLabel>(replaceLabelRect, "Replace:"));

    sRect replaceFieldRect;
    replaceFieldRect.row = rect.row + 3;
    replaceFieldRect.col = rect.col + 12;
    replaceFieldRect.rows = 1;
    replaceFieldRect.cols = rect.cols - 14;
    auto replaceField = std::make_unique<cTextField>(replaceFieldRect, initial.replace);
    cTextField* replacePtr = replaceField.get();
    dialog.AddWidget(std::move(replaceField));

    sRect wholeRect;
    wholeRect.row = rect.row + 5;
    wholeRect.col = rect.col + 2;
    wholeRect.rows = 1;
    wholeRect.cols = 22;
    auto wholeBox = std::make_unique<cCheckBox>(wholeRect, "Whole word", BoolToTri(initial.wholeWord));
    cCheckBox* wholePtr = wholeBox.get();
    dialog.AddWidget(std::move(wholeBox));

    sRect caseRect;
    caseRect.row = rect.row + 5;
    caseRect.col = rect.col + 26;
    caseRect.rows = 1;
    caseRect.cols = 22;
    auto caseBox = std::make_unique<cCheckBox>(caseRect, "Ignore case", BoolToTri(initial.ignoreCase));
    cCheckBox* casePtr = caseBox.get();
    dialog.AddWidget(std::move(caseBox));

    sRect backRect;
    backRect.row = rect.row + 6;
    backRect.col = rect.col + 2;
    backRect.rows = 1;
    backRect.cols = 22;
    auto backBox = std::make_unique<cCheckBox>(backRect, "Backward", BoolToTri(initial.backward));
    cCheckBox* backPtr = backBox.get();
    dialog.AddWidget(std::move(backBox));

    sRect wildRect;
    wildRect.row = rect.row + 6;
    wildRect.col = rect.col + 26;
    wildRect.rows = 1;
    wildRect.cols = 22;
    auto wildBox = std::make_unique<cCheckBox>(wildRect, "Wildcard", BoolToTri(initial.wildcard));
    cCheckBox* wildPtr = wildBox.get();
    dialog.AddWidget(std::move(wildBox));

    sRect askRect;
    askRect.row = rect.row + 7;
    askRect.col = rect.col + 2;
    askRect.rows = 1;
    askRect.cols = 30;
    auto askBox = std::make_unique<cCheckBox>(askRect, "Replace without asking", BoolToTri(initial.dontAsk));
    cCheckBox* askPtr = askBox.get();
    dialog.AddWidget(std::move(askBox));

    sRect scopeRect;
    scopeRect.row = rect.row + 9;
    scopeRect.col = rect.col + 2;
    scopeRect.rows = 3;
    scopeRect.cols = rect.cols - 4;
    auto scope = std::make_unique<cRadioGroup>(scopeRect);
    scope->AddChoice("Next occurrence", 0);
    scope->AddChoice("Entire document", 1);
    scope->AddChoice("Rest of document", 2);
    scope->SetSelectedIndex(Clamp(initial.scope, 0, 2));
    cRadioGroup* scopePtr = scope.get();
    dialog.AddWidget(std::move(scope));

    cDialog* dialogPtr = &dialog;

    sRect replaceButtonRect;
    replaceButtonRect.rows = 1;
    replaceButtonRect.cols = 11;
    replaceButtonRect.row = rect.row + rect.rows - 2;
    replaceButtonRect.col = rect.col + ((rect.cols - 25) / 2);
    dialog.AddWidget(std::make_unique<cButton>(replaceButtonRect, "Replace", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 11;
    cancelRect.row = replaceButtonRect.row;
    cancelRect.col = replaceButtonRect.col + 13;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        options.find = findPtr->GetText();
        options.replace = replacePtr->GetText();
        options.wholeWord = TriToBool(wholePtr);
        options.ignoreCase = TriToBool(casePtr);
        options.backward = TriToBool(backPtr);
        options.wildcard = TriToBool(wildPtr);
        options.dontAsk = TriToBool(askPtr);
        options.scope = scopePtr->GetSelectedValue();
        options.ok = true;
    }

    return options;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const sPageLayoutValues& initial [in] initial values
/// @param  const std::string& unitSuffix [in] unit suffix for labels
///
/// @return the page layout values with ok set when OK was pressed
///
/// @brief
/// Display the page layout dialog with eight measurement fields.
///
/////////////////////////////////////////////////////////////////////////////
sPageLayoutValues PageLayoutDialog(iWSDialogHost* host, const sPageLayoutValues& initial,
                                   const std::string& unitSuffix)
{
    sPageLayoutValues values = initial;
    values.ok = false;

    int width = 44;
    int height = 15;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Page Layout");

    struct sFieldSpec
    {
        std::string label;
        std::string value;
    };

    std::vector<sFieldSpec> specs;
    specs.push_back({"Top margin", initial.top});
    specs.push_back({"Bottom margin", initial.bottom});
    specs.push_back({"Left margin", initial.left});
    specs.push_back({"Right margin", initial.right});
    specs.push_back({"Odd offset", initial.oddOffset});
    specs.push_back({"Even offset", initial.evenOffset});
    specs.push_back({"Header margin", initial.headerMargin});
    specs.push_back({"Footer margin", initial.footerMargin});

    std::vector<cTextField*> fieldPtrs;

    int labelWidth = 18;
    int fieldCol = rect.col + 2 + labelWidth;
    int fieldWidth = rect.cols - 4 - labelWidth;

    if (fieldWidth < 6)
    {
        fieldWidth = 6;
    }

    for (size_t index = 0; index < specs.size(); ++index)
    {
        int lineRow = rect.row + 2 + static_cast<int>(index);

        sRect fieldLabelRect;
        fieldLabelRect.row = lineRow;
        fieldLabelRect.col = rect.col + 2;
        fieldLabelRect.rows = 1;
        fieldLabelRect.cols = labelWidth;
        dialog.AddWidget(std::make_unique<cLabel>(fieldLabelRect, specs[index].label + " (" + unitSuffix + "):"));

        sRect entryRect;
        entryRect.row = lineRow;
        entryRect.col = fieldCol;
        entryRect.rows = 1;
        entryRect.cols = fieldWidth;
        auto entry = std::make_unique<cTextField>(entryRect, specs[index].value);
        fieldPtrs.push_back(entry.get());
        dialog.AddWidget(std::move(entry));
    }

    cDialog* dialogPtr = &dialog;

    sRect okRect;
    okRect.rows = 1;
    okRect.cols = 10;
    okRect.row = rect.row + rect.rows - 2;
    okRect.col = rect.col + ((rect.cols - 24) / 2);
    dialog.AddWidget(std::make_unique<cButton>(okRect, "OK", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = okRect.row;
    cancelRect.col = okRect.col + 12;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        values.top = fieldPtrs[0]->GetText();
        values.bottom = fieldPtrs[1]->GetText();
        values.left = fieldPtrs[2]->GetText();
        values.right = fieldPtrs[3]->GetText();
        values.oddOffset = fieldPtrs[4]->GetText();
        values.evenOffset = fieldPtrs[5]->GetText();
        values.headerMargin = fieldPtrs[6]->GetText();
        values.footerMargin = fieldPtrs[7]->GetText();
        values.ok = true;
    }

    return values;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::vector<std::string>& families [in] font families
/// @param  const std::string& currentFamily [in] currently selected family
/// @param  const std::string& currentSize [in] current size text
/// @param  std::string& family [out] chosen family
/// @param  std::string& size [out] chosen size
///
/// @return true when OK was pressed
///
/// @brief
/// Display a font selection dialog with a family list and size field.
///
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  printers [in] available CUPS destination names
/// @param  selected [out] chosen printer name when OK is pressed
///
/// @return true when OK was pressed
///
/// @brief
/// Present a modal list of installed printers to choose one to print to,
/// used when no CUPS default destination is configured.
///
/////////////////////////////////////////////////////////////////////////////
bool SelectPrinterDialog(iWSDialogHost* host, const std::vector<std::string>& printers,
                         std::string& selected)
{
    int width = 40;
    int height = static_cast<int>(printers.size()) + 5;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Select Printer");

    sRect listRect;
    listRect.row = rect.row + 1;
    listRect.col = rect.col + 2;
    listRect.rows = rect.rows - 4;
    listRect.cols = rect.cols - 4;
    if (listRect.rows < 1)
    {
        listRect.rows = 1;
    }

    auto list = std::make_unique<cListBox>(listRect);
    list->SetItems(printers);
    list->SetSelectedIndex(0);
    cListBox* listPtr = list.get();
    dialog.AddWidget(std::move(list));

    cDialog* dialogPtr = &dialog;

    sRect okRect;
    okRect.rows = 1;
    okRect.cols = 10;
    okRect.row = rect.row + rect.rows - 2;
    okRect.col = rect.col + ((rect.cols - 24) / 2);
    dialog.AddWidget(std::make_unique<cButton>(okRect, "OK", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = okRect.row;
    cancelRect.col = okRect.col + 12;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        selected = listPtr->GetSelectedText();
        return true;
    }

    return false;
}

bool SelectFontDialog(iWSDialogHost* host, const std::vector<std::string>& families,
                      const std::string& currentFamily, const std::string& currentSize,
                      std::string& family, std::string& size)
{
    int width = 50;
    int height = 18;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Select Font");

    sRect familyLabelRect;
    familyLabelRect.row = rect.row + 1;
    familyLabelRect.col = rect.col + 2;
    familyLabelRect.rows = 1;
    familyLabelRect.cols = rect.cols - 4;
    dialog.AddWidget(std::make_unique<cLabel>(familyLabelRect, "Family:"));

    sRect listRect;
    listRect.row = rect.row + 2;
    listRect.col = rect.col + 2;
    listRect.rows = rect.rows - 7;
    listRect.cols = rect.cols - 4;

    if (listRect.rows < 1)
    {
        listRect.rows = 1;
    }

    auto list = std::make_unique<cListBox>(listRect);
    list->SetItems(families);

    for (size_t index = 0; index < families.size(); ++index)
    {
        if (families[index] == currentFamily)
        {
            list->SetSelectedIndex(static_cast<int>(index));
            break;
        }
    }

    cListBox* listPtr = list.get();
    dialog.AddWidget(std::move(list));

    sRect sizeLabelRect;
    sizeLabelRect.row = rect.row + rect.rows - 4;
    sizeLabelRect.col = rect.col + 2;
    sizeLabelRect.rows = 1;
    sizeLabelRect.cols = 8;
    dialog.AddWidget(std::make_unique<cLabel>(sizeLabelRect, "Size:"));

    sRect sizeFieldRect;
    sizeFieldRect.row = rect.row + rect.rows - 4;
    sizeFieldRect.col = rect.col + 10;
    sizeFieldRect.rows = 1;
    sizeFieldRect.cols = 10;
    auto sizeField = std::make_unique<cTextField>(sizeFieldRect, currentSize);
    cTextField* sizePtr = sizeField.get();
    dialog.AddWidget(std::move(sizeField));

    cDialog* dialogPtr = &dialog;

    sRect okRect;
    okRect.rows = 1;
    okRect.cols = 10;
    okRect.row = rect.row + rect.rows - 2;
    okRect.col = rect.col + ((rect.cols - 24) / 2);
    dialog.AddWidget(std::make_unique<cButton>(okRect, "OK", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = okRect.row;
    cancelRect.col = okRect.col + 12;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        family = listPtr->GetSelectedText();
        size = sizePtr->GetText();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  int& selectedIndex [out] chosen style index when OK is pressed
///
/// @return true when OK was pressed
///
/// @brief
/// Present a modal list of character styles to apply. The returned index maps
/// to the style list below; the caller applies the corresponding formatting.
///
/////////////////////////////////////////////////////////////////////////////
bool SelectStyleDialog(iWSDialogHost* host, int& selectedIndex)
{
    std::vector<std::string> styles = {
        "Bold",
        "Italic",
        "Underline",
        "Strikeout",
        "Superscript",
        "Subscript",
        "Font...",
        "Color..."
    };

    int width = 30;
    int height = static_cast<int>(styles.size()) + 5;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Select Style");

    sRect listRect;
    listRect.row = rect.row + 1;
    listRect.col = rect.col + 2;
    listRect.rows = rect.rows - 4;
    listRect.cols = rect.cols - 4;
    if (listRect.rows < 1)
    {
        listRect.rows = 1;
    }

    auto list = std::make_unique<cListBox>(listRect);
    list->SetItems(styles);
    list->SetSelectedIndex(0);
    cListBox* listPtr = list.get();
    dialog.AddWidget(std::move(list));

    cDialog* dialogPtr = &dialog;

    sRect okRect;
    okRect.rows = 1;
    okRect.cols = 10;
    okRect.row = rect.row + rect.rows - 2;
    okRect.col = rect.col + ((rect.cols - 24) / 2);
    dialog.AddWidget(std::make_unique<cButton>(okRect, "OK", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = okRect.row;
    cancelRect.col = okRect.col + 12;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]() {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        selectedIndex = listPtr->GetSelectedIndex();
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  int red [in] initial red value
/// @param  int green [in] initial green value
/// @param  int blue [in] initial blue value
///
/// @return the color result with ok set when OK was pressed
///
/// @brief
/// Display an RGB color selection dialog.
///
/////////////////////////////////////////////////////////////////////////////
sColorResult SelectColorDialog(iWSDialogHost* host, int red, int green, int blue)
{
    sColorResult result;
    result.red = Clamp(red, 0, 255);
    result.green = Clamp(green, 0, 255);
    result.blue = Clamp(blue, 0, 255);
    result.useDefault = false;
    result.ok = false;

    int width = 54;
    int height = 18;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Select Color");

    // Saturation/value square and hue bar.
    sRect squareRect{rect.row + 2, rect.col + 2, 9, 30};
    auto square = std::make_unique<cColorField2D>(squareRect);
    cColorField2D* squarePtr = square.get();

    sRect hueRect{rect.row + 2, rect.col + 34, 9, 2};
    auto hue = std::make_unique<cHueBar>(hueRect);
    cHueBar* huePtr = hue.get();

    // Preview swatch and hex readout.
    sRect swatchRect{rect.row + 2, rect.col + 38, 5, 13};
    auto swatch = std::make_unique<cColorSwatch>(swatchRect);
    cColorSwatch* swatchPtr = swatch.get();

    sRect hexRect{rect.row + 8, rect.col + 38, 1, 13};
    auto hexLabel = std::make_unique<cLabel>(hexRect, "#000000");
    cLabel* hexPtr = hexLabel.get();

    // Red / green / blue entry fields.
    sRect rLabelRect{rect.row + 11, rect.col + 2, 1, 3};
    dialog.AddWidget(std::make_unique<cLabel>(rLabelRect, "R:"));
    sRect rFieldRect{rect.row + 11, rect.col + 5, 1, 5};
    auto rField = std::make_unique<cTextField>(rFieldRect, std::to_string(result.red));
    cTextField* rPtr = rField.get();

    sRect gLabelRect{rect.row + 11, rect.col + 12, 1, 3};
    dialog.AddWidget(std::make_unique<cLabel>(gLabelRect, "G:"));
    sRect gFieldRect{rect.row + 11, rect.col + 15, 1, 5};
    auto gField = std::make_unique<cTextField>(gFieldRect, std::to_string(result.green));
    cTextField* gPtr = gField.get();

    sRect bLabelRect{rect.row + 11, rect.col + 22, 1, 3};
    dialog.AddWidget(std::make_unique<cLabel>(bLabelRect, "B:"));
    sRect bFieldRect{rect.row + 11, rect.col + 25, 1, 5};
    auto bField = std::make_unique<cTextField>(bFieldRect, std::to_string(result.blue));
    cTextField* bPtr = bField.get();

    // The terminal's detected color depth decides how the result is quantized:
    // 0 = 24-bit (no quantize), 1 = 256 colors, 2 = 16 colors.
    sTerminalCapabilities caps = host->HostCapabilities();
    int colorMode = 2;
    if (caps.trueColor == true)
    {
        colorMode = 0;
    }
    else if (caps.color256 == true)
    {
        colorMode = 1;
    }

    // Use-default checkbox.
    sRect defaultRect{rect.row + 13, rect.col + 2, 1, rect.cols - 4};
    auto defaultBox = std::make_unique<cCheckBox>(defaultRect, "Use default color", TRI_STATE_OFF);
    cCheckBox* defaultPtr = defaultBox.get();

    // Keep the fields, hex readout and preview in sync with the square and hue.
    auto syncColor = [squarePtr, huePtr, rPtr, gPtr, bPtr, hexPtr, swatchPtr]()
    {
        sHsv hsv;
        hsv.h = huePtr->GetHue();
        hsv.s = squarePtr->GetSaturation();
        hsv.v = squarePtr->GetValue();

        sColor color = HsvToRgb(hsv);
        rPtr->SetText(std::to_string(static_cast<int>(color.r)));
        gPtr->SetText(std::to_string(static_cast<int>(color.g)));
        bPtr->SetText(std::to_string(static_cast<int>(color.b)));

        char hexText[8];
        std::snprintf(hexText, sizeof(hexText), "#%02X%02X%02X",
                      static_cast<int>(color.r), static_cast<int>(color.g), static_cast<int>(color.b));
        hexPtr->SetText(hexText);
        swatchPtr->SetColor(color);
    };

    squarePtr->SetOnChange(syncColor);
    huePtr->SetOnChange([squarePtr, huePtr, syncColor]()
    {
        squarePtr->SetHue(huePtr->GetHue());
        syncColor();
    });

    // Seed the widgets from the incoming color.
    sHsv initialHsv = RgbToHsv(MakeRgb(static_cast<uint8_t>(result.red),
                                       static_cast<uint8_t>(result.green),
                                       static_cast<uint8_t>(result.blue)));
    huePtr->SetHue(initialHsv.h);
    squarePtr->SetHue(initialHsv.h);
    squarePtr->SetSaturationValue(initialHsv.s, initialHsv.v);
    syncColor();

    // Add the focusable widgets in tab order.
    dialog.AddWidget(std::move(square));
    dialog.AddWidget(std::move(hue));
    dialog.AddWidget(std::move(swatch));
    dialog.AddWidget(std::move(hexLabel));
    dialog.AddWidget(std::move(rField));
    dialog.AddWidget(std::move(gField));
    dialog.AddWidget(std::move(bField));
    dialog.AddWidget(std::move(defaultBox));

    cDialog* dialogPtr = &dialog;

    sRect okRect;
    okRect.rows = 1;
    okRect.cols = 10;
    okRect.row = rect.row + rect.rows - 2;
    okRect.col = rect.col + ((rect.cols - 24) / 2);
    dialog.AddWidget(std::make_unique<cButton>(okRect, "OK", [dialogPtr]()
    {
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = okRect.row;
    cancelRect.col = okRect.col + 12;
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr]()
    {
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_OK)
    {
        int r = Clamp(ParseInt(rPtr->GetText()), 0, 255);
        int g = Clamp(ParseInt(gPtr->GetText()), 0, 255);
        int b = Clamp(ParseInt(bPtr->GetText()), 0, 255);

        // Quantize the result to the terminal's detected color depth.
        if (colorMode == 2)
        {
            sColor quantized = MakePaletteColor(RgbTo16(MakeRgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b))));
            r = quantized.r;
            g = quantized.g;
            b = quantized.b;
        }
        else if (colorMode == 1)
        {
            sColor quantized = Index256ToRgb(RgbTo256(MakeRgb(static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b))));
            r = quantized.r;
            g = quantized.g;
            b = quantized.b;
        }

        result.red = r;
        result.green = g;
        result.blue = b;
        result.useDefault = TriToBool(defaultPtr);
        result.ok = true;
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  const std::string& word [in] misspelled word
/// @param  const std::vector<std::string>& suggestions [in] suggestions
/// @param  int& selectedIndex [out] chosen suggestion index for replace
///
/// @return 0 ignore, 1 replace, 2 add, -1 cancel
///
/// @brief
/// Display the spell check action dialog.
///
/////////////////////////////////////////////////////////////////////////////
int SpellCheckDialog(iWSDialogHost* host, const std::string& word,
                     const std::vector<std::string>& suggestions, int& selectedIndex)
{
    int width = 50;
    int height = 18;

    sRect rect = CenteredRect(host, height, width);
    cDialog dialog(rect, "Spelling");

    sRect wordLabelRect;
    wordLabelRect.row = rect.row + 2;
    wordLabelRect.col = rect.col + 2;
    wordLabelRect.rows = 1;
    wordLabelRect.cols = rect.cols - 4;
    dialog.AddWidget(std::make_unique<cLabel>(wordLabelRect, "Not found: " + word));

    sRect listRect;
    listRect.row = rect.row + 4;
    listRect.col = rect.col + 2;
    listRect.rows = rect.rows - 8;
    listRect.cols = rect.cols - 4;

    if (listRect.rows < 1)
    {
        listRect.rows = 1;
    }

    auto list = std::make_unique<cListBox>(listRect);
    list->SetItems(suggestions);
    cListBox* listPtr = list.get();
    dialog.AddWidget(std::move(list));

    cDialog* dialogPtr = &dialog;
    int action = -1;
    int* actionPtr = &action;

    int buttonWidth = 10;
    int buttonRow = rect.row + rect.rows - 2;
    int startCol = rect.col + 2;

    sRect ignoreRect;
    ignoreRect.rows = 1;
    ignoreRect.cols = buttonWidth;
    ignoreRect.row = buttonRow;
    ignoreRect.col = startCol;
    dialog.AddWidget(std::make_unique<cButton>(ignoreRect, "Ignore", [dialogPtr, actionPtr]() {
        *actionPtr = 0;
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect replaceRect;
    replaceRect.rows = 1;
    replaceRect.cols = buttonWidth;
    replaceRect.row = buttonRow;
    replaceRect.col = startCol + (buttonWidth + 1);
    dialog.AddWidget(std::make_unique<cButton>(replaceRect, "Replace", [dialogPtr, actionPtr]() {
        *actionPtr = 1;
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect addRect;
    addRect.rows = 1;
    addRect.cols = buttonWidth;
    addRect.row = buttonRow;
    addRect.col = startCol + (2 * (buttonWidth + 1));
    dialog.AddWidget(std::make_unique<cButton>(addRect, "Add", [dialogPtr, actionPtr]() {
        *actionPtr = 2;
        dialogPtr->SetResult(DIALOG_RESULT_OK);
    }));

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = buttonWidth;
    cancelRect.row = buttonRow;
    cancelRect.col = startCol + (3 * (buttonWidth + 1));
    dialog.AddWidget(std::make_unique<cButton>(cancelRect, "Cancel", [dialogPtr, actionPtr]() {
        *actionPtr = -1;
        dialogPtr->SetResult(DIALOG_RESULT_CANCEL);
    }));

    dialog.FocusFirst();
    host->HostRunModal(dialog);

    if (dialog.GetResult() == DIALOG_RESULT_CANCEL)
    {
        return -1;
    }

    if (action == 1)
    {
        selectedIndex = listPtr->GetSelectedIndex();
    }

    return action;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  iWSDialogHost* host [in] dialog host
/// @param  cConfig& config [in,out] configuration edited in place on OK
///
/// @return true when OK was pressed and config was updated
///
/// @brief
/// Display the six-tab System Preferences dialog. Presents General, Editor,
/// Page, Colors, Display and User tabs. On OK every field is written back
/// into the supplied config; on Cancel or Escape the config is left
/// untouched.
///
/// Focus is driven manually: each tab owns a vector of input widgets plus the
/// two shared OK/Cancel buttons. The dialog tracks an active tab index and a
/// focus index into that tab's focus list, calling SetFocus() on the focused
/// widget and forwarding events only to it. Left/Right arrows switch tabs;
/// Up/Down and Tab move focus within a tab.
///
/////////////////////////////////////////////////////////////////////////////
bool SystemPreferences(iWSDialogHost* host, cConfig& config)
{
    int width = 78;
    int height = 27;

    sRect rect = CenteredRect(host, height, width);
    width = rect.cols;
    height = rect.rows;

    // Row layout (relative to rect.row):
    //   +0            top border + title
    //   +1            main tab bar
    //   +2            separator under the main tab bar
    //   +3 (contentTop) first content row
    //   rect.rows - 3 separator above the button row
    //   rect.rows - 2 OK / Cancel button row
    //   rect.rows - 1 bottom border + hint
    int contentTop = rect.row + 3;
    int contentLeft = rect.col + 2;
    int labelWidth = 22;
    int fieldCol = contentLeft + labelWidth;
    int fieldWidth = 24;

    // ---- Per-tab widget ownership ----
    std::vector<std::unique_ptr<cWidget>> generalWidgets;
    std::vector<std::unique_ptr<cWidget>> editorWidgets;
    std::vector<std::unique_ptr<cWidget>> pageWidgets;
    std::vector<std::unique_ptr<cWidget>> colorsWidgets;
    std::vector<std::unique_ptr<cWidget>> displayWidgets;
    std::vector<std::unique_ptr<cWidget>> userWidgets;

    // Focus lists hold non-owning pointers to the focusable widgets of each
    // tab (labels are drawn separately and are not focusable).
    std::vector<cWidget*> generalFocus;
    std::vector<cWidget*> editorFocus;
    std::vector<cWidget*> pageFocus;
    std::vector<cWidget*> colorsFocus;
    std::vector<cWidget*> displayFocus;
    std::vector<cWidget*> userFocus;

    // Static text drawn per tab: right-of-border field labels and bold
    // section headers. These are rendered directly in the draw lambda so the
    // columns line up exactly and headers can be bold, 
    // frontend. Labels are not focusable.
    struct sStaticLabel
    {
        int row;
        int col;
        std::string text;
        bool bold;
    };

    std::vector<sStaticLabel> generalLabels;
    std::vector<sStaticLabel> editorLabels;
    std::vector<sStaticLabel> pageLabels;
    std::vector<sStaticLabel> displayLabels;
    std::vector<sStaticLabel> userLabels;

    // A field label sits at the content-left column and is padded to labelWidth
    // so every input widget on the tab starts at the same column (fieldCol).
    auto MakeFieldRect = [&](int line, int cols) -> sRect {
        sRect r;
        r.row = contentTop + line;
        r.col = fieldCol;
        r.rows = 1;
        r.cols = cols;
        return r;
    };

    // Indented control column used under bold section headers.
    int indentCol = contentLeft + 4;

    auto MakeIndentRect = [&](int line, int cols) -> sRect {
        sRect r;
        r.row = contentTop + line;
        r.col = indentCol;
        r.rows = 1;
        r.cols = cols;
        return r;
    };

    // ============================ General tab ============================
    cDropdown* inputModePtr = nullptr;
    cCheckBox* showControlsPtr = nullptr;
    cTextField* codePagePtr = nullptr;
    cDropdown* measurementPtr = nullptr;
    cTextField* defaultFontPtr = nullptr;
    cTextField* fontSizePtr = nullptr;
    cDropdown* defaultFormatPtr = nullptr;
    cTextField* defaultDirPtr = nullptr;

    {
        // Row 0: Default Font (inline label + field)
        generalLabels.push_back({contentTop + 0, contentLeft, "Default Font:", false});
        auto defaultFont = std::make_unique<cTextField>(MakeFieldRect(0, fieldWidth), config.mDefaultFont);
        defaultFontPtr = defaultFont.get();
        generalFocus.push_back(defaultFontPtr);
        generalWidgets.push_back(std::move(defaultFont));

        // Row 1: Font Size (inline label + field)
        generalLabels.push_back({contentTop + 1, contentLeft, "Font Size:", false});
        char sizeBuffer[32];
        std::snprintf(sizeBuffer, sizeof(sizeBuffer), "%.1f", config.mDefaultFontSize);
        auto fontSize = std::make_unique<cTextField>(MakeFieldRect(1, 10), std::string(sizeBuffer));
        fontSizePtr = fontSize.get();
        generalFocus.push_back(fontSizePtr);
        generalWidgets.push_back(std::move(fontSize));

        // Row 3: Measurement section header + indented dropdown
        generalLabels.push_back({contentTop + 3, contentLeft, "Measurement:", true});
        auto measurement = std::make_unique<cDropdown>(MakeIndentRect(4, fieldWidth));
        measurement->SetItems({"Inches", "Centimeters", "Millimeters"});

        int measureIndex = 0;

        if (config.mMeasurement == "mm")
        {
            measureIndex = 2;
        }
        else if (config.mMeasurement == "cm")
        {
            measureIndex = 1;
        }

        measurement->SetSelectedIndex(measureIndex);
        measurementPtr = measurement.get();
        generalFocus.push_back(measurementPtr);
        generalWidgets.push_back(std::move(measurement));

        // Row 6: Code Page (inline label + field)
        generalLabels.push_back({contentTop + 6, contentLeft, "Code Page:", false});
        auto codePage = std::make_unique<cTextField>(MakeFieldRect(6, 10), std::to_string(config.mCodePage));
        codePagePtr = codePage.get();
        generalFocus.push_back(codePagePtr);
        generalWidgets.push_back(std::move(codePage));

        // Row 8: Show control characters checkbox
        auto showControls = std::make_unique<cCheckBox>(MakeIndentRect(8, fieldWidth),
                                                        "Show control characters", BoolToTri(config.mShowControls));
        showControlsPtr = showControls.get();
        generalFocus.push_back(showControlsPtr);
        generalWidgets.push_back(std::move(showControls));

        // Row 10: Default Format (inline label + dropdown selector)
        generalLabels.push_back({contentTop + 10, contentLeft, "Default Format:", false});
        auto defaultFormat = std::make_unique<cDropdown>(MakeFieldRect(10, fieldWidth));
        defaultFormat->SetItems({"WordStar (.ws)", "RTF (.rtf)"});
        defaultFormat->SetSelectedIndex((config.mDefaultFormat == "rtf") ? 1 : 0);
        defaultFormatPtr = defaultFormat.get();
        generalFocus.push_back(defaultFormatPtr);
        generalWidgets.push_back(std::move(defaultFormat));

        // Row 12: Default Directory (inline label + wide field)
        generalLabels.push_back({contentTop + 12, contentLeft, "Default Directory:", false});
        auto defaultDir = std::make_unique<cTextField>(MakeFieldRect(12, rect.cols - 4 - labelWidth), config.mDefaultDirectory);
        defaultDirPtr = defaultDir.get();
        generalFocus.push_back(defaultDirPtr);
        generalWidgets.push_back(std::move(defaultDir));
    }

    // ============================ Editor tab ============================
    cTextField* autoSavePtr = nullptr;
    cTextField* spellLangPtr = nullptr;
    cCheckBox* spellDotPtr = nullptr;

    {
        // Row 0: Auto-Save Interval (inline label + field)
        editorLabels.push_back({contentTop + 0, contentLeft, "Auto-Save (sec):", false});
        auto autoSave = std::make_unique<cTextField>(MakeFieldRect(0, 10), std::to_string(config.mAutoSaveInterval));
        autoSavePtr = autoSave.get();
        editorFocus.push_back(autoSavePtr);
        editorWidgets.push_back(std::move(autoSave));

        // Row 2: Keyboard Mode section header + indented dropdown. Changing this
        // rebuilds the menu bar so its shortcuts match the mode (see
        // OpenSystemPreferences). The caret blink rate is intentionally omitted
        // on the TUI: the terminal blinks the hardware cursor itself.
        editorLabels.push_back({contentTop + 2, contentLeft, "Keyboard Mode:", true});
        auto inputMode = std::make_unique<cDropdown>(MakeIndentRect(3, fieldWidth));
        inputMode->SetItems({"WordStar", "Modern"});
        inputMode->SetSelectedIndex(Clamp(config.mInputMode, 0, 1));
        inputModePtr = inputMode.get();
        editorFocus.push_back(inputModePtr);
        editorWidgets.push_back(std::move(inputMode));

        // Row 5: Spell Check Language (inline label + field)
        editorLabels.push_back({contentTop + 5, contentLeft, "Spell Language:", false});
        auto spellLang = std::make_unique<cTextField>(MakeFieldRect(5, fieldWidth), config.mSpellCheckLanguage);
        spellLangPtr = spellLang.get();
        editorFocus.push_back(spellLangPtr);
        editorWidgets.push_back(std::move(spellLang));

        // Row 7: Spell-check dot commands checkbox
        auto spellDot = std::make_unique<cCheckBox>(MakeIndentRect(7, rect.cols - 4 - 4),
                                                    "Spell-check dot command text", BoolToTri(config.mSpellCheckDotCommands));
        spellDotPtr = spellDot.get();
        editorFocus.push_back(spellDotPtr);
        editorWidgets.push_back(std::move(spellDot));
    }

    // ============================ Page tab ============================
    struct sPageField
    {
        cTextField* field;
        COORD_T* target;
    };

    std::vector<sPageField> pageFields;
    cCheckBox* landscapePtr = nullptr;

    {
        // Two-column layout paper size and page
        // offsets on the left, margins on the right. Each column has a bold
        // header, aligned label/field rows, and lines up its input widgets in
        // a common column.
        struct sPageSpec
        {
            std::string label;
            COORD_T* target;
            int line;
            int labelCol;
            int fieldCol;
        };

        int leftLabelCol = contentLeft;
        int leftPageLabelWidth = 16;
        int leftFieldCol = leftLabelCol + leftPageLabelWidth;
        int rightLabelCol = contentLeft + 38;
        int rightPageLabelWidth = 16;
        int rightFieldCol = rightLabelCol + rightPageLabelWidth;

        // Bold column headers.
        pageLabels.push_back({contentTop + 0, leftLabelCol, "Paper / Offset:", true});
        pageLabels.push_back({contentTop + 0, rightLabelCol, "Margins:", true});

        std::vector<sPageSpec> specs;
        // Left column.
        specs.push_back({"Paper Width:", &config.mPaperWidth, 1, leftLabelCol, leftFieldCol});
        specs.push_back({"Paper Height:", &config.mPaperHeight, 2, leftLabelCol, leftFieldCol});
        specs.push_back({"Odd Offset:", &config.mPageOffsetOdd, 4, leftLabelCol, leftFieldCol});
        specs.push_back({"Even Offset:", &config.mPageOffsetEven, 5, leftLabelCol, leftFieldCol});
        // Right column.
        specs.push_back({"Left Margin:", &config.mLeftMargin, 1, rightLabelCol, rightFieldCol});
        specs.push_back({"Right Margin:", &config.mRightMargin, 2, rightLabelCol, rightFieldCol});
        specs.push_back({"Top Margin:", &config.mTopMargin, 3, rightLabelCol, rightFieldCol});
        specs.push_back({"Bottom Margin:", &config.mBottomMargin, 4, rightLabelCol, rightFieldCol});
        specs.push_back({"Header Margin:", &config.mHeaderMargin, 5, rightLabelCol, rightFieldCol});
        specs.push_back({"Footer Margin:", &config.mFooterMargin, 6, rightLabelCol, rightFieldCol});

        // Format the page measurements in the unit chosen on the General tab
        // (inches / centimetres / millimetres), showing the unit suffix. The user may type any unit suffix; ParseMeasurement
        // reads it back on OK.
        char pageUnit = 'i';
        if (config.mMeasurement.find("mm") != std::string::npos)
        {
            pageUnit = 'm';
        }
        else if (config.mMeasurement.find('c') != std::string::npos)
        {
            pageUnit = 'c';
        }

        for (size_t index = 0; index < specs.size(); ++index)
        {
            const sPageSpec& spec = specs[index];
            pageLabels.push_back({contentTop + spec.line, spec.labelCol, spec.label, false});

            sRect fieldRect;
            fieldRect.row = contentTop + spec.line;
            fieldRect.col = spec.fieldCol;
            fieldRect.rows = 1;
            fieldRect.cols = 10;

            auto entry = std::make_unique<cTextField>(fieldRect, cConfig::FormatMeasurement(*spec.target, pageUnit));
            pageFields.push_back({entry.get(), spec.target});
            pageFocus.push_back(entry.get());
            pageWidgets.push_back(std::move(entry));
        }

        // Landscape checkbox under the left column.
        sRect landscapeRect;
        landscapeRect.row = contentTop + 7;
        landscapeRect.col = leftLabelCol;
        landscapeRect.rows = 1;
        landscapeRect.cols = fieldWidth;
        auto landscape = std::make_unique<cCheckBox>(landscapeRect,
                                                     "Landscape orientation", BoolToTri(config.mLandscape));
        landscapePtr = landscape.get();
        pageFocus.push_back(landscapePtr);
        pageWidgets.push_back(std::move(landscape));
    }

    // ============================ Colors tab ============================
    // Colors are grouped as PAIRS: one component owns a foreground and a
    // background color. The pairs are shown in two side-by-side columns with
    // live swatches and a "Sample" preview. Each
    // pair keeps raw pointers into config so edits land directly in config on
    // OK. The Colors tab is custom-drawn (not a cListBox).
    struct sColorPair
    {
        std::string label;
        sRGB* fg;
        sRGB* bg;
    };

    std::vector<std::string> colorSubTabNames = {"Editor/Attributes", "Screen", "Preview"};

    // Left column of sub-tab 0: the nine editor colors.
    std::vector<sColorPair> editorColors = {
        {"Normal Text",     &config.mTuiForeground,               &config.mTuiBackground},
        {"Control Codes",   &config.mTuiHighlightForeground,      &config.mTuiHighlightBackground},
        {"Dot Commands",    &config.mTuiDotForeground,            &config.mTuiDotBackground},
        {"Block Selection", &config.mTuiBlockForeground,          &config.mTuiBlockBackground},
        {"Comments",        &config.mTuiCommentForeground,        &config.mTuiCommentBackground},
        {"Errors",          &config.mTuiErrorForeground,          &config.mTuiErrorBackground},
        {"Unknown",         &config.mTuiUnknownForeground,        &config.mTuiUnknownBackground},
        {"Not Implemented", &config.mTuiNotImplementedForeground, &config.mTuiNotImplementedBackground},
        {"Search Results",  &config.mTuiSearchForeground,         &config.mTuiSearchBackground},
    };

    // Right column of sub-tab 0: the six attribute colors.
    std::vector<sColorPair> attrColors = {
        {"Bold",          &config.mTuiBoldForeground,          &config.mTuiBoldBackground},
        {"Italic",        &config.mTuiItalicForeground,        &config.mTuiItalicBackground},
        {"Underline",     &config.mTuiUnderlineForeground,     &config.mTuiUnderlineBackground},
        {"Strikethrough", &config.mTuiStrikethroughForeground, &config.mTuiStrikethroughBackground},
        {"Superscript",   &config.mTuiSuperscriptForeground,   &config.mTuiSuperscriptBackground},
        {"Subscript",     &config.mTuiSubscriptForeground,     &config.mTuiSubscriptBackground},
    };

    // Left column of sub-tab 1: the five "screen 1" chrome colors.
    std::vector<sColorPair> screen1Colors = {
        {"Title Bar",      &config.mTuiTitleBarForeground,             &config.mTuiTitleBarBackground},
        {"Status Bar",     &config.mTuiStatusBarForeground,            &config.mTuiStatusBarBackground},
        {"Help Panel",     &config.mTuiHelpPanelForeground,            &config.mTuiHelpPanelBackground},
        {"Help Keystroke", &config.mTuiHelpPanelKeystrokeForeground,   &config.mTuiHelpPanelKeystrokeBackground},
        {"Menu Bar",       &config.mTuiMenuBarForeground,              &config.mTuiMenuBarBackground},
    };

    // Right column of sub-tab 1: the five "screen 2" chrome colors.
    std::vector<sColorPair> screen2Colors = {
        {"Menu Accelerator", &config.mTuiMenuAcceleratorForeground, &config.mTuiMenuAcceleratorBackground},
        {"Menu Highlight",   &config.mTuiMenuHighlightForeground,   &config.mTuiMenuHighlightBackground},
        {"Ruler",            &config.mTuiRulerForeground,           &config.mTuiRulerBackground},
        {"Flag Column",      &config.mTuiFlagColumnForeground,      &config.mTuiFlagColumnBackground},
        {"Scrollbar",        &config.mTuiScrollbarForeground,       &config.mTuiScrollbarBackground},
    };

    int colorSubTab = 0;
    // 0 = left column, 1 = right column, of the active sub-tab.
    int activeCol = 0;
    // Selected pair index, remembered per column.
    int editorSel = 0;
    int attrSel = 0;
    int screen1Sel = 0;
    int screen2Sel = 0;
    // Which swatch is targeted in each column: false = fg, true = bg.
    bool editingBgEditor = false;
    bool editingBgAttr = false;
    bool editingBgScreen1 = false;
    bool editingBgScreen2 = false;

    // Resolve (subTab, column) to the pair vector for that group.
    auto ColorColumn = [&](int subTab, int col) -> std::vector<sColorPair>& {
        if (subTab == 0)
        {
            if (col == 0)
            {
                return editorColors;
            }

            return attrColors;
        }

        if (col == 0)
        {
            return screen1Colors;
        }

        return screen2Colors;
    };

    // Reference to the selected-index state for a given column.
    auto ColorSel = [&](int subTab, int col) -> int& {
        if (subTab == 0)
        {
            if (col == 0)
            {
                return editorSel;
            }

            return attrSel;
        }

        if (col == 0)
        {
            return screen1Sel;
        }

        return screen2Sel;
    };

    // Reference to the fg/bg editing flag for a given column.
    auto ColorEditingBg = [&](int subTab, int col) -> bool& {
        if (subTab == 0)
        {
            if (col == 0)
            {
                return editingBgEditor;
            }

            return editingBgAttr;
        }

        if (col == 0)
        {
            return editingBgScreen1;
        }

        return editingBgScreen2;
    };

    // ---- Palette data ----
    // Built-in palettes come first, then any user-saved custom palettes. The
    // dropdown shows "(Custom)" at index 0, so dropdown index i maps to
    // allPalettes[i - 1].
    std::vector<sTUIPalette> builtInPalettes = cConfig::GetTUIPalettes();
    std::vector<sTUIPalette> customPalettes = cConfig::LoadCustomTUIPalettes();

    std::vector<sTUIPalette> allPalettes;
    allPalettes.insert(allPalettes.end(), builtInPalettes.begin(), builtInPalettes.end());
    allPalettes.insert(allPalettes.end(), customPalettes.begin(), customPalettes.end());

    std::vector<std::string> paletteLabels;
    paletteLabels.push_back("(Custom)");

    for (const sTUIPalette& pal : allPalettes)
    {
        paletteLabels.push_back(pal.name);
    }

    // Copy a palette's colors into the live color pairs (which point into
    // config). idx is an index into allPalettes.
    auto applyPalette = [&](int idx) {
        if ((idx < 0) || (idx >= static_cast<int>(allPalettes.size())))
        {
            return;
        }

        const sTUIPalette& pal = allPalettes[static_cast<size_t>(idx)];

        // Editor colors.
        *editorColors[0].fg = pal.foreground;
        *editorColors[0].bg = pal.background;
        *editorColors[1].fg = pal.highlightForeground;
        *editorColors[1].bg = pal.highlightBackground;
        *editorColors[2].fg = pal.dotForeground;
        *editorColors[2].bg = pal.dotBackground;
        *editorColors[3].fg = pal.blockForeground;
        *editorColors[3].bg = pal.blockBackground;
        *editorColors[4].fg = pal.commentForeground;
        *editorColors[4].bg = pal.commentBackground;
        *editorColors[5].fg = pal.errorForeground;
        *editorColors[5].bg = pal.errorBackground;
        *editorColors[6].fg = pal.unknownForeground;
        *editorColors[6].bg = pal.unknownBackground;
        *editorColors[7].fg = pal.notImplementedForeground;
        *editorColors[7].bg = pal.notImplementedBackground;
        *editorColors[8].fg = pal.searchForeground;
        *editorColors[8].bg = pal.searchBackground;

        // Attribute colors.
        *attrColors[0].fg = pal.boldForeground;
        *attrColors[0].bg = pal.boldBackground;
        *attrColors[1].fg = pal.italicForeground;
        *attrColors[1].bg = pal.italicBackground;
        *attrColors[2].fg = pal.underlineForeground;
        *attrColors[2].bg = pal.underlineBackground;
        *attrColors[3].fg = pal.strikethroughForeground;
        *attrColors[3].bg = pal.strikethroughBackground;
        *attrColors[4].fg = pal.superscriptForeground;
        *attrColors[4].bg = pal.superscriptBackground;
        *attrColors[5].fg = pal.subscriptForeground;
        *attrColors[5].bg = pal.subscriptBackground;

        // Screen 1 colors.
        *screen1Colors[0].fg = pal.titleBarForeground;
        *screen1Colors[0].bg = pal.titleBarBackground;
        *screen1Colors[1].fg = pal.statusBarForeground;
        *screen1Colors[1].bg = pal.statusBarBackground;
        *screen1Colors[2].fg = pal.helpPanelForeground;
        *screen1Colors[2].bg = pal.helpPanelBackground;
        *screen1Colors[3].fg = pal.helpPanelKeystrokeForeground;
        *screen1Colors[3].bg = pal.helpPanelKeystrokeBackground;
        *screen1Colors[4].fg = pal.menuBarForeground;
        *screen1Colors[4].bg = pal.menuBarBackground;

        // Screen 2 colors.
        *screen2Colors[0].fg = pal.menuAcceleratorForeground;
        *screen2Colors[0].bg = pal.menuAcceleratorBackground;
        *screen2Colors[1].fg = pal.menuHighlightForeground;
        *screen2Colors[1].bg = pal.menuHighlightBackground;
        *screen2Colors[2].fg = pal.rulerForeground;
        *screen2Colors[2].bg = pal.rulerBackground;
        *screen2Colors[3].fg = pal.flagColumnForeground;
        *screen2Colors[3].bg = pal.flagColumnBackground;
        *screen2Colors[4].fg = pal.scrollbarForeground;
        *screen2Colors[4].bg = pal.scrollbarBackground;
    };

    // Read the current color pairs into a new sTUIPalette for saving.
    auto buildPaletteFromCurrent = [&](const std::string& name) -> sTUIPalette {
        sTUIPalette pal;
        pal.name = name;

        // Editor colors.
        pal.foreground = *editorColors[0].fg;
        pal.background = *editorColors[0].bg;
        pal.highlightForeground = *editorColors[1].fg;
        pal.highlightBackground = *editorColors[1].bg;
        pal.dotForeground = *editorColors[2].fg;
        pal.dotBackground = *editorColors[2].bg;
        pal.blockForeground = *editorColors[3].fg;
        pal.blockBackground = *editorColors[3].bg;
        pal.commentForeground = *editorColors[4].fg;
        pal.commentBackground = *editorColors[4].bg;
        pal.errorForeground = *editorColors[5].fg;
        pal.errorBackground = *editorColors[5].bg;
        pal.unknownForeground = *editorColors[6].fg;
        pal.unknownBackground = *editorColors[6].bg;
        pal.notImplementedForeground = *editorColors[7].fg;
        pal.notImplementedBackground = *editorColors[7].bg;
        pal.searchForeground = *editorColors[8].fg;
        pal.searchBackground = *editorColors[8].bg;

        // Attribute colors.
        pal.boldForeground = *attrColors[0].fg;
        pal.boldBackground = *attrColors[0].bg;
        pal.italicForeground = *attrColors[1].fg;
        pal.italicBackground = *attrColors[1].bg;
        pal.underlineForeground = *attrColors[2].fg;
        pal.underlineBackground = *attrColors[2].bg;
        pal.strikethroughForeground = *attrColors[3].fg;
        pal.strikethroughBackground = *attrColors[3].bg;
        pal.superscriptForeground = *attrColors[4].fg;
        pal.superscriptBackground = *attrColors[4].bg;
        pal.subscriptForeground = *attrColors[5].fg;
        pal.subscriptBackground = *attrColors[5].bg;

        // Screen 1 colors.
        pal.titleBarForeground = *screen1Colors[0].fg;
        pal.titleBarBackground = *screen1Colors[0].bg;
        pal.statusBarForeground = *screen1Colors[1].fg;
        pal.statusBarBackground = *screen1Colors[1].bg;
        pal.helpPanelForeground = *screen1Colors[2].fg;
        pal.helpPanelBackground = *screen1Colors[2].bg;
        pal.helpPanelKeystrokeForeground = *screen1Colors[3].fg;
        pal.helpPanelKeystrokeBackground = *screen1Colors[3].bg;
        pal.menuBarForeground = *screen1Colors[4].fg;
        pal.menuBarBackground = *screen1Colors[4].bg;

        // Screen 2 colors.
        pal.menuAcceleratorForeground = *screen2Colors[0].fg;
        pal.menuAcceleratorBackground = *screen2Colors[0].bg;
        pal.menuHighlightForeground = *screen2Colors[1].fg;
        pal.menuHighlightBackground = *screen2Colors[1].bg;
        pal.rulerForeground = *screen2Colors[2].fg;
        pal.rulerBackground = *screen2Colors[2].bg;
        pal.flagColumnForeground = *screen2Colors[3].fg;
        pal.flagColumnBackground = *screen2Colors[3].bg;
        pal.scrollbarForeground = *screen2Colors[4].fg;
        pal.scrollbarBackground = *screen2Colors[4].bg;

        return pal;
    };

    // Detect whether the current colors match a known palette. 0 = "(Custom)",
    // otherwise the matched allPalettes index + 1.
    auto rgbEqual = [](const sRGB& a, const sRGB& b) -> bool {
        return (a.r == b.r) && (a.g == b.g) && (a.b == b.b);
    };

    int currentPaletteIndex = 0;

    for (int i = 0; i < static_cast<int>(allPalettes.size()); ++i)
    {
        const sTUIPalette& pal = allPalettes[static_cast<size_t>(i)];
        bool match = true
            && rgbEqual(*editorColors[0].fg, pal.foreground) && rgbEqual(*editorColors[0].bg, pal.background)
            && rgbEqual(*editorColors[1].fg, pal.highlightForeground) && rgbEqual(*editorColors[1].bg, pal.highlightBackground)
            && rgbEqual(*editorColors[2].fg, pal.dotForeground) && rgbEqual(*editorColors[2].bg, pal.dotBackground)
            && rgbEqual(*editorColors[3].fg, pal.blockForeground) && rgbEqual(*editorColors[3].bg, pal.blockBackground)
            && rgbEqual(*editorColors[4].fg, pal.commentForeground) && rgbEqual(*editorColors[4].bg, pal.commentBackground)
            && rgbEqual(*editorColors[5].fg, pal.errorForeground) && rgbEqual(*editorColors[5].bg, pal.errorBackground)
            && rgbEqual(*editorColors[6].fg, pal.unknownForeground) && rgbEqual(*editorColors[6].bg, pal.unknownBackground)
            && rgbEqual(*editorColors[7].fg, pal.notImplementedForeground) && rgbEqual(*editorColors[7].bg, pal.notImplementedBackground)
            && rgbEqual(*editorColors[8].fg, pal.searchForeground) && rgbEqual(*editorColors[8].bg, pal.searchBackground)
            && rgbEqual(*attrColors[0].fg, pal.boldForeground) && rgbEqual(*attrColors[0].bg, pal.boldBackground)
            && rgbEqual(*attrColors[1].fg, pal.italicForeground) && rgbEqual(*attrColors[1].bg, pal.italicBackground)
            && rgbEqual(*attrColors[2].fg, pal.underlineForeground) && rgbEqual(*attrColors[2].bg, pal.underlineBackground)
            && rgbEqual(*attrColors[3].fg, pal.strikethroughForeground) && rgbEqual(*attrColors[3].bg, pal.strikethroughBackground)
            && rgbEqual(*attrColors[4].fg, pal.superscriptForeground) && rgbEqual(*attrColors[4].bg, pal.superscriptBackground)
            && rgbEqual(*attrColors[5].fg, pal.subscriptForeground) && rgbEqual(*attrColors[5].bg, pal.subscriptBackground)
            && rgbEqual(*screen1Colors[0].fg, pal.titleBarForeground) && rgbEqual(*screen1Colors[0].bg, pal.titleBarBackground)
            && rgbEqual(*screen1Colors[1].fg, pal.statusBarForeground) && rgbEqual(*screen1Colors[1].bg, pal.statusBarBackground)
            && rgbEqual(*screen1Colors[2].fg, pal.helpPanelForeground) && rgbEqual(*screen1Colors[2].bg, pal.helpPanelBackground)
            && rgbEqual(*screen1Colors[3].fg, pal.helpPanelKeystrokeForeground) && rgbEqual(*screen1Colors[3].bg, pal.helpPanelKeystrokeBackground)
            && rgbEqual(*screen1Colors[4].fg, pal.menuBarForeground) && rgbEqual(*screen1Colors[4].bg, pal.menuBarBackground)
            && rgbEqual(*screen2Colors[0].fg, pal.menuAcceleratorForeground) && rgbEqual(*screen2Colors[0].bg, pal.menuAcceleratorBackground)
            && rgbEqual(*screen2Colors[1].fg, pal.menuHighlightForeground) && rgbEqual(*screen2Colors[1].bg, pal.menuHighlightBackground)
            && rgbEqual(*screen2Colors[2].fg, pal.rulerForeground) && rgbEqual(*screen2Colors[2].bg, pal.rulerBackground)
            && rgbEqual(*screen2Colors[3].fg, pal.flagColumnForeground) && rgbEqual(*screen2Colors[3].bg, pal.flagColumnBackground)
            && rgbEqual(*screen2Colors[4].fg, pal.scrollbarForeground) && rgbEqual(*screen2Colors[4].bg, pal.scrollbarBackground);

        if (match == true)
        {
            currentPaletteIndex = i + 1;
            break;
        }
    }

    // Sub-tab bar for the color groups, styled identically to the main tab bar.
    // The sub-tab row sits at contentTop, a separator at contentTop+1, and the
    // two-column color layout begins at contentTop+2.
    cTabBar colorSubTabBar;

    for (const std::string& name : colorSubTabNames)
    {
        colorSubTabBar.AddTab(name);
    }

    sRect colorSubTabRect;
    colorSubTabRect.row = contentTop;
    colorSubTabRect.col = contentLeft;
    colorSubTabRect.rows = 1;
    colorSubTabRect.cols = rect.cols - 4;
    colorSubTabBar.SetBounds(colorSubTabRect);

    // Geometry of the custom-drawn two-column layout. The columns start below
    // the sub-tab bar and its separator.
    int colorListRow = contentTop + 2;
    int colorLeftCol = contentLeft;
    int colorColWidth = 36;
    int colorRightCol = colorLeftCol + colorColWidth + 2;
    int colorSepCol = colorLeftCol + colorColWidth;

    // ---- Palette picker + Save Theme button ----
    // Both sit on the palette-label line, just above the button-row separator.
    int paletteRow = rect.row + rect.rows - 4;

    sRect paletteDropdownRect;
    paletteDropdownRect.row = paletteRow;
    paletteDropdownRect.col = contentLeft + 9;
    paletteDropdownRect.rows = 1;
    paletteDropdownRect.cols = 22;

    auto palettePickerOwned = std::make_unique<cDropdown>(paletteDropdownRect);
    palettePickerOwned->SetItems(paletteLabels);
    palettePickerOwned->SetSelectedIndex(currentPaletteIndex);
    palettePickerOwned->SetOpenUpward(true);
    cDropdown* palettePicker = palettePickerOwned.get();
    colorsFocus.push_back(palettePicker);
    colorsWidgets.push_back(std::move(palettePickerOwned));

    bool saveThemeRequested = false;

    sRect saveThemeRect;
    saveThemeRect.row = paletteRow;
    saveThemeRect.col = rect.col + rect.cols - 16;
    saveThemeRect.rows = 1;
    saveThemeRect.cols = 14;

    auto saveThemeOwned = std::make_unique<cButton>(saveThemeRect, "Save Theme", [&saveThemeRequested]() {
        saveThemeRequested = true;
    });
    cButton* saveThemeButton = saveThemeOwned.get();
    colorsFocus.push_back(saveThemeButton);
    colorsWidgets.push_back(std::move(saveThemeOwned));

    int lastPaletteSel = currentPaletteIndex;

    // ============================ Display tab ============================
    cCheckBox* showTitlePtr = nullptr;
    cCheckBox* showRulerPtr = nullptr;
    cCheckBox* showScrollPtr = nullptr;
    cCheckBox* showStatusPtr = nullptr;
    cCheckBox* showStylePtr = nullptr;
    cCheckBox* showMenuPtr = nullptr;
    cCheckBox* alwaysDotPtr = nullptr;
    cCheckBox* alwaysFlagPtr = nullptr;
    cDropdown* helpPtr = nullptr;

    {
        // Two-column layout Left column holds the
        // "Display On Screen" checkboxes and the help-display dropdown; right
        // column holds the dot-command and flag-column toggles. Focus order is
        // left column top-to-bottom, then right column.
        int leftCol = contentLeft + 4;
        int rightCol = contentLeft + 40;
        int checkWidth = 30;

        auto MakeDisplayRect = [&](int col, int line, int cols) -> sRect {
            sRect r;
            r.row = contentTop + line;
            r.col = col;
            r.rows = 1;
            r.cols = cols;
            return r;
        };

        // ---- Left column ----
        displayLabels.push_back({contentTop + 0, contentLeft, "Display On Screen:", true});

        auto showTitle = std::make_unique<cCheckBox>(MakeDisplayRect(leftCol, 1, checkWidth), "Show title bar", BoolToTri(config.mTuiShowTitleBar));
        showTitlePtr = showTitle.get();
        displayFocus.push_back(showTitlePtr);
        displayWidgets.push_back(std::move(showTitle));

        auto showRuler = std::make_unique<cCheckBox>(MakeDisplayRect(leftCol, 2, checkWidth), "Show ruler", BoolToTri(config.mTuiShowRuler));
        showRulerPtr = showRuler.get();
        displayFocus.push_back(showRulerPtr);
        displayWidgets.push_back(std::move(showRuler));

        auto showScroll = std::make_unique<cCheckBox>(MakeDisplayRect(leftCol, 3, checkWidth), "Show scroll bar", BoolToTri(config.mTuiShowScrollBar));
        showScrollPtr = showScroll.get();
        displayFocus.push_back(showScrollPtr);
        displayWidgets.push_back(std::move(showScroll));

        auto showStatus = std::make_unique<cCheckBox>(MakeDisplayRect(leftCol, 4, checkWidth), "Show status bar", BoolToTri(config.mTuiShowStatusBar));
        showStatusPtr = showStatus.get();
        displayFocus.push_back(showStatusPtr);
        displayWidgets.push_back(std::move(showStatus));

        auto showStyle = std::make_unique<cCheckBox>(MakeDisplayRect(leftCol, 5, checkWidth), "Show style bar", BoolToTri(config.mTuiShowStyleBar));
        showStylePtr = showStyle.get();
        displayFocus.push_back(showStylePtr);
        displayWidgets.push_back(std::move(showStyle));

        auto showMenu = std::make_unique<cCheckBox>(MakeDisplayRect(leftCol, 6, checkWidth), "Show menu", BoolToTri(config.mTuiShowMenu));
        showMenuPtr = showMenu.get();
        displayFocus.push_back(showMenuPtr);
        displayWidgets.push_back(std::move(showMenu));

        displayLabels.push_back({contentTop + 8, contentLeft, "Help Level:", true});
        auto help = std::make_unique<cDropdown>(MakeDisplayRect(leftCol, 9, fieldWidth));
        help->SetItems({"0 - Off", "1 - Menus Off", "2 - Compact", "3 - Full"});
        help->SetSelectedIndex(Clamp(config.mTuiShowHelp, 0, 3));
        helpPtr = help.get();
        displayFocus.push_back(helpPtr);
        displayWidgets.push_back(std::move(help));

        // ---- Right column ----
        displayLabels.push_back({contentTop + 0, rightCol - 4, "Dot Commands:", true});
        auto alwaysDot = std::make_unique<cCheckBox>(MakeDisplayRect(rightCol, 1, checkWidth), "Always show dot commands", BoolToTri(config.mTuiAlwaysDotCommands));
        alwaysDotPtr = alwaysDot.get();
        displayFocus.push_back(alwaysDotPtr);
        displayWidgets.push_back(std::move(alwaysDot));

        displayLabels.push_back({contentTop + 3, rightCol - 4, "Flag Column:", true});
        auto alwaysFlag = std::make_unique<cCheckBox>(MakeDisplayRect(rightCol, 4, checkWidth), "Always show flag column", BoolToTri(config.mTuiAlwaysFlagColumn));
        alwaysFlagPtr = alwaysFlag.get();
        displayFocus.push_back(alwaysFlagPtr);
        displayWidgets.push_back(std::move(alwaysFlag));
    }

    // ============================ User tab ============================
    cTextField* shortNamePtr = nullptr;
    cTextField* longNamePtr = nullptr;

    {
        userLabels.push_back({contentTop + 0, contentLeft, "Short Name:", false});
        auto shortName = std::make_unique<cTextField>(MakeFieldRect(0, fieldWidth), config.mShortName);
        shortNamePtr = shortName.get();
        userFocus.push_back(shortNamePtr);
        userWidgets.push_back(std::move(shortName));

        userLabels.push_back({contentTop + 1, contentLeft, "Long Name:", false});
        auto longName = std::make_unique<cTextField>(MakeFieldRect(1, rect.cols - 4 - labelWidth), config.mLongName);
        longNamePtr = longName.get();
        userFocus.push_back(longNamePtr);
        userWidgets.push_back(std::move(longName));
    }

    // ---- Tab bar ----
    cTabBar tabBar;
    tabBar.AddTab("General");
    tabBar.AddTab("Editor");
    tabBar.AddTab("Page");
    tabBar.AddTab("Colors");
    tabBar.AddTab("Display");
    tabBar.AddTab("User");

    sRect tabRect;
    tabRect.row = rect.row + 1;
    tabRect.col = rect.col + 2;
    tabRect.rows = 1;
    tabRect.cols = rect.cols - 4;
    tabBar.SetBounds(tabRect);

    // ---- Shared OK / Cancel buttons ----
    bool result = false;
    bool* resultPtr = &result;
    bool closeRequested = false;
    bool* closePtr = &closeRequested;

    sRect okRect;
    okRect.rows = 1;
    okRect.cols = 10;
    okRect.row = rect.row + rect.rows - 2;
    okRect.col = rect.col + ((rect.cols - 24) / 2);

    sRect cancelRect;
    cancelRect.rows = 1;
    cancelRect.cols = 10;
    cancelRect.row = okRect.row;
    cancelRect.col = okRect.col + 12;

    cButton okButton(okRect, "OK", [resultPtr, closePtr]() {
        *resultPtr = true;
        *closePtr = true;
    });

    cButton cancelButton(cancelRect, "Cancel", [resultPtr, closePtr]() {
        *resultPtr = false;
        *closePtr = true;
    });

    // Return the focus list for the active tab, with OK/Cancel appended.
    auto BuildFocusList = [&](int activeTab) -> std::vector<cWidget*> {
        std::vector<cWidget*> focus;

        if (activeTab == 0)
        {
            focus = generalFocus;
        }
        else if (activeTab == 1)
        {
            focus = editorFocus;
        }
        else if (activeTab == 2)
        {
            focus = pageFocus;
        }
        else if (activeTab == 3)
        {
            // A leading nullptr sentinel represents the custom-drawn color
            // list; the remaining entries are the RGB edit fields.
            focus.push_back(nullptr);

            for (cWidget* widget : colorsFocus)
            {
                focus.push_back(widget);
            }
        }
        else if (activeTab == 4)
        {
            focus = displayFocus;
        }
        else
        {
            focus = userFocus;
        }

        focus.push_back(&okButton);
        focus.push_back(&cancelButton);
        return focus;
    };

    // Return the drawable widgets (including labels) for the active tab.
    auto ActiveWidgets = [&](int activeTab) -> std::vector<std::unique_ptr<cWidget>>* {
        if (activeTab == 0)
        {
            return &generalWidgets;
        }

        if (activeTab == 1)
        {
            return &editorWidgets;
        }

        if (activeTab == 2)
        {
            return &pageWidgets;
        }

        if (activeTab == 3)
        {
            return &colorsWidgets;
        }

        if (activeTab == 4)
        {
            return &displayWidgets;
        }

        return &userWidgets;
    };

    int focusIndex = 0;
    std::vector<cWidget*> focusList = BuildFocusList(tabBar.GetActiveTab());

    // Apply focus to exactly one widget in the current focus list. A nullptr
    // entry (the Colors tab's color list) has no widget to focus.
    auto ApplyFocus = [&]() {
        for (size_t index = 0; index < focusList.size(); ++index)
        {
            if (focusList[index] == nullptr)
            {
                continue;
            }

            focusList[index]->SetFocus(static_cast<int>(index) == focusIndex);
        }
    };

    ApplyFocus();

    sStyle dialogStyle = host->HostTheme().GetStyle(THEME_ROLE_DIALOG);
    sStyle titleStyle = host->HostTheme().GetStyle(THEME_ROLE_DIALOG_TITLE);

    // A run of horizontal box-drawing characters (U+2500) used for the
    // separator lines under the tab bars.
    auto MakeSeparator = [](int cols) -> std::string {
        std::string line;

        for (int index = 0; index < cols; ++index)
        {
            line = line + "\xe2\x94\x80";
        }

        return line;
    };

    // Read-only preview: a miniature editor screen using the chrome colors for
    // the bars and the editor colors for a few sample text lines.
    auto DrawColorPreview = [&](cScreen& screen) {
        // fg-over-own-bg style for a pair.
        auto PairStyle = [&](const sColorPair& pair) -> sStyle {
            sStyle style = dialogStyle;
            style.fg = MakeRgb(static_cast<uint8_t>(pair.fg->r),
                               static_cast<uint8_t>(pair.fg->g),
                               static_cast<uint8_t>(pair.fg->b));
            style.bg = MakeRgb(static_cast<uint8_t>(pair.bg->r),
                               static_cast<uint8_t>(pair.bg->g),
                               static_cast<uint8_t>(pair.bg->b));
            return style;
        };

        // A run of text drawn in one pair's colors.
        struct sSeg
        {
            std::string text;
            const sColorPair* pair;
        };

        int width = rect.cols - 4;
        int row = colorListRow;

        // A full-width chrome bar filled with one pair, then colored segments.
        auto DrawSegs = [&](int atRow, const sColorPair& fillPair, const std::vector<sSeg>& segs) {
            sRect bar;
            bar.row = atRow;
            bar.col = contentLeft;
            bar.rows = 1;
            bar.cols = width;
            screen.FillRect(bar, " ", PairStyle(fillPair));

            int col = contentLeft;
            for (const sSeg& seg : segs)
            {
                screen.PutText(atRow, col, seg.text, PairStyle(*seg.pair));
                col += static_cast<int>(seg.text.size());
            }
        };

        // An editor content line: normal-text bg, flag column, segments, scrollbar.
        auto DrawEd = [&](int atRow, const std::vector<sSeg>& segs) {
            sStyle normalBg = PairStyle(editorColors[0]);

            sRect body;
            body.row = atRow;
            body.col = contentLeft;
            body.rows = 1;
            body.cols = width;
            screen.FillRect(body, " ", normalBg);
            screen.PutText(atRow, contentLeft, "\xe2\x96\x88", PairStyle(screen2Colors[3]));

            int col = contentLeft + 2;
            for (const sSeg& seg : segs)
            {
                screen.PutText(atRow, col, seg.text, PairStyle(*seg.pair));
                col += static_cast<int>(seg.text.size());
            }

            screen.PutText(atRow, contentLeft + width - 1, "\xe2\x96\x88", PairStyle(screen2Colors[4]));
        };

        // ---- Chrome bars ----
        DrawSegs(row + 0, screen1Colors[0], {{" sample.ws - WordTsar 0.5 Alpha ", &screen1Colors[0]}});
        DrawSegs(row + 1, screen1Colors[4], {
            {" ", &screen1Colors[4]},
            {"F", &screen2Colors[0]}, {"ile  ", &screen1Colors[4]},
            {"E", &screen2Colors[0]}, {"dit  ", &screen1Colors[4]},
            {"V", &screen2Colors[0]}, {"iew  ", &screen1Colors[4]},
            {"S", &screen2Colors[0]}, {"tyle  ", &screen1Colors[4]},
            {" Highlighted ", &screen2Colors[1]},
        });
        DrawSegs(row + 2, screen1Colors[1], {{" Body Text | TNR 12 | B I U |   | L C R J", &screen1Colors[1]}});
        DrawSegs(row + 3, screen1Colors[2], {
            {" ", &screen1Colors[2]},
            {"^J", &screen1Colors[3]}, {" help   ", &screen1Colors[2]},
            {"^KD", &screen1Colors[3]}, {" done   ", &screen1Colors[2]},
            {"^KS", &screen1Colors[3]}, {" save   ", &screen1Colors[2]},
            {"^KQ", &screen1Colors[3]}, {" quit", &screen1Colors[2]},
        });
        DrawSegs(row + 4, screen2Colors[2], {{
            "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\xba"
            "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\xba\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
            "\xe2\x96\xba\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\xa4", &screen2Colors[2]}});

        // ---- Editor content lines ----
        DrawEd(row + 5, {{"The quick brown fox jumps over the lazy dog.", &editorColors[0]}});
        DrawEd(row + 6, {
            {"This is ", &editorColors[0]}, {"bold", &attrColors[0]}, {" and ", &editorColors[0]},
            {"italic", &attrColors[1]}, {" and ", &editorColors[0]}, {"underline", &attrColors[2]},
            {" text.", &editorColors[0]},
        });
        DrawEd(row + 7, {
            {"Also ", &editorColors[0]}, {"strikethrough", &attrColors[3]}, {", ", &editorColors[0]},
            {"superscript", &attrColors[4]}, {", ", &editorColors[0]}, {"subscript", &attrColors[5]},
            {".", &editorColors[0]},
        });
        DrawEd(row + 8, {
            {"Use ", &editorColors[0]}, {"B", &editorColors[1]}, {"bold", &attrColors[0]}, {"B", &editorColors[1]},
            {" and ", &editorColors[0]}, {"Y", &editorColors[1]}, {"italic", &attrColors[1]}, {"Y", &editorColors[1]},
            {" and ", &editorColors[0]}, {"S", &editorColors[1]}, {"underline", &attrColors[2]}, {"S", &editorColors[1]},
            {" text.", &editorColors[0]},
        });
        DrawEd(row + 9, {{".LH 12", &editorColors[2]}});
        DrawEd(row + 10, {{".. This is a comment line", &editorColors[4]}});
        DrawEd(row + 11, {{"This line is selected as a block.", &editorColors[3]}});
        DrawEd(row + 12, {{"error in dot command", &editorColors[5]}});
        DrawEd(row + 13, {{"unknown dot command", &editorColors[6]}});
        DrawEd(row + 14, {{"not implemented dot command", &editorColors[7]}});
        DrawEd(row + 15, {
            {"Found: ", &editorColors[0]}, {"search result", &editorColors[8]}, {" in document.", &editorColors[0]},
        });

        // ---- Bottom status bar ----
        DrawSegs(row + 16, screen1Colors[1], {{" Pg 1  Ln 11  Col 1  Insert  Words: 42", &screen1Colors[1]}});
    };

    // A solid two-cell color block glyph (U+2588 x2) used for swatches.
    const std::string colorSwatchGlyph = "\xe2\x96\x88\xe2\x96\x88";

    // Draw a single pair row for a column at (rowY, startCol).
    //   focused    = this row belongs to the active column and is its selection
    //   activeRow  = this row is the selected row of the non-active column
    //   editingBg  = for a focused row, which swatch is bracketed
    auto DrawColorPairRow = [&](cScreen& screen, int rowY, int startCol,
                                const sColorPair& pair, bool focused,
                                bool activeRow, bool editingBg) {
        int col = startCol;

        // ---- prefix ----
        std::string prefix = "  ";

        if (focused == true)
        {
            prefix = "> ";
        }

        screen.PutText(rowY, col, prefix, dialogStyle);
        col += 2;

        // Bracket glyphs for a swatch: filled when it is the active edit target.
        std::string openBracket = " ";
        std::string closeBracket = " ";

        // ---- label padded to 18 columns ----
        std::string label = pair.label;

        if (static_cast<int>(label.size()) > 18)
        {
            label = label.substr(0, 18);
        }

        while (static_cast<int>(label.size()) < 18)
        {
            label = label + " ";
        }

        sStyle labelStyle = dialogStyle;

        if (focused == true)
        {
            labelStyle.attrs = labelStyle.attrs | CELL_ATTR_INVERSE;
        }
        else if (activeRow == true)
        {
            labelStyle.attrs = labelStyle.attrs | CELL_ATTR_BOLD;
        }

        screen.PutText(rowY, col, label, labelStyle);
        col += 18;
        col += 1;

        // ---- foreground swatch, bracketed when it is the edit target ----
        sStyle fgSwatch = dialogStyle;
        fgSwatch.fg = MakeRgb(static_cast<uint8_t>(pair.fg->r),
                              static_cast<uint8_t>(pair.fg->g),
                              static_cast<uint8_t>(pair.fg->b));

        bool fgActive = (focused == true) && (editingBg == false);
        openBracket = " ";
        closeBracket = " ";

        if (fgActive == true)
        {
            openBracket = "[";
            closeBracket = "]";
        }

        screen.PutText(rowY, col, openBracket, dialogStyle);
        col += 1;
        screen.PutText(rowY, col, colorSwatchGlyph, fgSwatch);
        col += 2;
        screen.PutText(rowY, col, closeBracket, dialogStyle);
        col += 1;

        // ---- background swatch, bracketed when it is the edit target ----
        sStyle bgSwatch = dialogStyle;
        bgSwatch.fg = MakeRgb(static_cast<uint8_t>(pair.bg->r),
                              static_cast<uint8_t>(pair.bg->g),
                              static_cast<uint8_t>(pair.bg->b));

        bool bgActive = (focused == true) && (editingBg == true);
        openBracket = " ";
        closeBracket = " ";

        if (bgActive == true)
        {
            openBracket = "[";
            closeBracket = "]";
        }

        screen.PutText(rowY, col, openBracket, dialogStyle);
        col += 1;
        screen.PutText(rowY, col, colorSwatchGlyph, bgSwatch);
        col += 2;
        screen.PutText(rowY, col, closeBracket, dialogStyle);
        col += 1;

        // ---- "Sample" preview in the pair's fg-on-bg colors ----
        sStyle sample = dialogStyle;
        sample.fg = MakeRgb(static_cast<uint8_t>(pair.fg->r),
                            static_cast<uint8_t>(pair.fg->g),
                            static_cast<uint8_t>(pair.fg->b));
        sample.bg = MakeRgb(static_cast<uint8_t>(pair.bg->r),
                            static_cast<uint8_t>(pair.bg->g),
                            static_cast<uint8_t>(pair.bg->b));
        screen.PutText(rowY, col, " Sample ", sample);
    };

    // Draw one column of pairs (header + group header + rows).
    auto DrawColorColumn = [&](cScreen& screen, int startCol,
                               const std::vector<sColorPair>& pairs,
                               const std::string& groupHeader,
                               int selected, bool columnActive, bool editingBg) {
        // Column header line, aligned to the swatch/preview columns produced by
        // DrawColorPairRow: prefix(2) + label(18) + gap(1) + brackets/swatches.
        sStyle boldStyle = dialogStyle;
        boldStyle.attrs = boldStyle.attrs | CELL_ATTR_BOLD;

        std::string header = "  Component";
        while (static_cast<int>(header.size()) < 22)
        {
            header = header + " ";
        }
        header = header + "Fg";
        while (static_cast<int>(header.size()) < 26)
        {
            header = header + " ";
        }
        header = header + "Bg";
        while (static_cast<int>(header.size()) < 30)
        {
            header = header + " ";
        }
        header = header + "Preview";
        screen.PutText(colorListRow, startCol, header, boldStyle);

        // Group header line (bold + dim).
        sStyle groupStyle = dialogStyle;
        groupStyle.attrs = groupStyle.attrs | CELL_ATTR_BOLD | CELL_ATTR_DIM;
        screen.PutText(colorListRow + 1, startCol, "  " + groupHeader, groupStyle);

        for (size_t index = 0; index < pairs.size(); ++index)
        {
            int rowY = colorListRow + 2 + static_cast<int>(index);
            bool isSelected = (static_cast<int>(index) == selected);
            bool focused = (columnActive == true) && (isSelected == true);
            bool activeRow = (columnActive == false) && (isSelected == true);
            DrawColorPairRow(screen, rowY, startCol, pairs[index], focused, activeRow, editingBg);
        }
    };

    // Custom-draw the Colors tab: a sub-tab bar, a separator, then either the
    // two-column pair layout (sub-tabs 0/1) or a read-only preview (sub-tab 2).
    auto DrawColorsTab = [&](cScreen& screen, const cTheme& theme) {
        // ---- Sub-tab bar (identical look to the main tab bar) ----
        colorSubTabBar.Draw(screen, theme);

        // ---- Separator under the sub-tab bar ----
        screen.PutText(contentTop + 1, contentLeft, MakeSeparator(rect.cols - 4), dialogStyle);

        if (colorSubTab == 2)
        {
            DrawColorPreview(screen);
            return;
        }

        // Two-column pair layout for sub-tabs 0 and 1.
        std::vector<sColorPair>& leftPairs = ColorColumn(colorSubTab, 0);
        std::vector<sColorPair>& rightPairs = ColorColumn(colorSubTab, 1);
        std::string leftHeader = "Editor Colors";
        std::string rightHeader = "Attributes";

        if (colorSubTab == 1)
        {
            leftHeader = "Screen 1";
            rightHeader = "Screen 2";
        }

        DrawColorColumn(screen, colorLeftCol, leftPairs, leftHeader,
                        ColorSel(colorSubTab, 0), activeCol == 0,
                        ColorEditingBg(colorSubTab, 0));
        DrawColorColumn(screen, colorRightCol, rightPairs, rightHeader,
                        ColorSel(colorSubTab, 1), activeCol == 1,
                        ColorEditingBg(colorSubTab, 1));

        // Vertical separator between the two columns.
        int maxRows = static_cast<int>(leftPairs.size());

        if (static_cast<int>(rightPairs.size()) > maxRows)
        {
            maxRows = static_cast<int>(rightPairs.size());
        }

        for (int r = 0; r < maxRows + 2; ++r)
        {
            screen.PutText(colorListRow + r, colorSepCol, "\xe2\x94\x82", dialogStyle);
        }
    };

    // Bold style used for section headers, derived from the dialog style.
    sStyle headerStyle = dialogStyle;
    headerStyle.attrs = headerStyle.attrs | CELL_ATTR_BOLD;

    // Return the static labels (field labels + bold section headers) for the
    // active tab. The Colors tab draws its own headers so it has none here.
    auto ActiveLabels = [&](int activeTab) -> const std::vector<sStaticLabel>* {
        if (activeTab == 0)
        {
            return &generalLabels;
        }

        if (activeTab == 1)
        {
            return &editorLabels;
        }

        if (activeTab == 2)
        {
            return &pageLabels;
        }

        if (activeTab == 4)
        {
            return &displayLabels;
        }

        if (activeTab == 5)
        {
            return &userLabels;
        }

        return nullptr;
    };

    auto draw = [&](cScreen& screen, const cTheme& theme) {
        screen.FillRect(rect, " ", dialogStyle);
        screen.DrawBox(rect, dialogStyle);
        screen.PutText(rect.row, rect.col + 2, " System Preferences ", titleStyle);

        // Main tab bar and the separator directly beneath it.
        tabBar.Draw(screen, theme);
        screen.PutText(rect.row + 2, rect.col + 2, MakeSeparator(rect.cols - 4), dialogStyle);

        int activeTab = tabBar.GetActiveTab();

        // Static field labels and bold section headers for the active tab.
        const std::vector<sStaticLabel>* labels = ActiveLabels(activeTab);

        if (labels != nullptr)
        {
            for (const sStaticLabel& label : *labels)
            {
                sStyle style = dialogStyle;

                if (label.bold == true)
                {
                    style = headerStyle;
                }

                screen.PutText(label.row, label.col, label.text, style);
            }
        }

        // The Colors tab draws its own sub-tab bar, separator, two-column pair
        // layout (or preview), and a palette label line.
        if (activeTab == 3)
        {
            DrawColorsTab(screen, theme);

            // Palette label just above the button-row separator. The dropdown
            // draws its own value.
            screen.PutText(rect.row + rect.rows - 4, contentLeft, "Palette:", dialogStyle);
        }

        std::vector<std::unique_ptr<cWidget>>* widgets = ActiveWidgets(activeTab);

        // Two passes: ordinary widgets first, then any widget showing an
        // overlay (an open dropdown) so its list and border are not painted
        // over by a neighbouring control drawn later.
        for (const std::unique_ptr<cWidget>& widget : *widgets)
        {
            if (widget->HasOpenOverlay() == false)
            {
                widget->Draw(screen, theme);
            }
        }

        for (const std::unique_ptr<cWidget>& widget : *widgets)
        {
            if (widget->HasOpenOverlay() == true)
            {
                widget->Draw(screen, theme);
            }
        }

        // Separator above the centered button row.
        screen.PutText(rect.row + rect.rows - 3, rect.col + 2, MakeSeparator(rect.cols - 4), dialogStyle);

        okButton.Draw(screen, theme);
        cancelButton.Draw(screen, theme);

        std::string hint = " Arrows: tab/field   Enter: OK   Esc: Cancel ";

        if (activeTab == 3)
        {
            hint = " Up/Down: row  Left/Right: Fg/Bg  Enter: edit  Esc: Cancel ";
        }

        screen.PutText(rect.row + rect.rows - 1, rect.col + 2, hint, titleStyle);
    };

    // Poll the palette widgets after an event has been forwarded: auto-apply a
    // newly selected palette, and run the Save Theme flow when requested. Called
    // from every spot that forwards an event to a widget.
    auto ProcessPaletteWidgets = [&]() {
        int sel = palettePicker->GetSelectedIndex();

        if (sel != lastPaletteSel)
        {
            lastPaletteSel = sel;

            if (sel > 0)
            {
                applyPalette(sel - 1);
            }
        }

        if (saveThemeRequested == true)
        {
            saveThemeRequested = false;

            std::string name;

            if ((InputBox(host, "Save Theme", "Theme name:", name) == true) && (name.empty() == false))
            {
                bool isBuiltIn = false;

                for (const sTUIPalette& pal : builtInPalettes)
                {
                    if (pal.name == name)
                    {
                        isBuiltIn = true;
                        break;
                    }
                }

                if (isBuiltIn == true)
                {
                    MessageBox(host, "Save Theme", "That name is reserved for a built-in palette.");
                }
                else
                {
                    sTUIPalette np = buildPaletteFromCurrent(name);
                    cConfig::SaveCustomTUIPalette(np);

                    // Refresh the palette list and labels.
                    customPalettes = cConfig::LoadCustomTUIPalettes();
                    allPalettes.clear();
                    allPalettes.insert(allPalettes.end(), builtInPalettes.begin(), builtInPalettes.end());
                    allPalettes.insert(allPalettes.end(), customPalettes.begin(), customPalettes.end());

                    paletteLabels.clear();
                    paletteLabels.push_back("(Custom)");

                    for (const sTUIPalette& pal : allPalettes)
                    {
                        paletteLabels.push_back(pal.name);
                    }

                    palettePicker->SetItems(paletteLabels);

                    // Select the newly saved theme.
                    int newIndex = 0;

                    for (int i = 0; i < static_cast<int>(allPalettes.size()); ++i)
                    {
                        if (allPalettes[static_cast<size_t>(i)].name == name)
                        {
                            newIndex = i + 1;
                            break;
                        }
                    }

                    palettePicker->SetSelectedIndex(newIndex);
                    lastPaletteSel = newIndex;
                }
            }
        }
    };

    auto handle = [&](const sInputEvent& event) -> bool {
        if (IsEscape(event) == true)
        {
            *resultPtr = false;
            return false;
        }

        // Mouse clicks on the tab row switch tabs.
        if ((event.type == INPUT_TYPE_MOUSE) && (event.mouseRow == tabRect.row))
        {
            if (tabBar.HandleEvent(event) == true)
            {
                focusList = BuildFocusList(tabBar.GetActiveTab());
                focusIndex = 0;
                ApplyFocus();
            }

            return true;
        }

        // On the Colors tab, mouse clicks on the sub-tab row switch the color
        // sub-tab, mirroring the main tab bar's click behavior.
        if ((tabBar.GetActiveTab() == 3) &&
            (event.type == INPUT_TYPE_MOUSE) && (event.mouseRow == colorSubTabRect.row))
        {
            if (colorSubTabBar.HandleEvent(event) == true)
            {
                colorSubTab = colorSubTabBar.GetActiveTab();
            }

            return true;
        }

        cWidget* focused = nullptr;

        if ((focusIndex >= 0) && (focusIndex < static_cast<int>(focusList.size())))
        {
            focused = focusList[static_cast<size_t>(focusIndex)];
        }

        // On the Colors tab, the leading nullptr focus entry is the color list.
        bool onColorList = ((tabBar.GetActiveTab() == 3) && (focused == nullptr));

        if (event.type == INPUT_TYPE_SPECIAL)
        {
            // On the Colors tab with the color list focused, Left/Right move
            // through the four swatch "stops" (leftFg, leftBg, rightFg, rightBg)
            // rather than switching the outer tab bar. The Preview sub-tab has
            // no columns, so Left/Right are simply consumed there.
            if ((onColorList == true) &&
                ((event.special == SPECIAL_KEY_ARROW_LEFT) || (event.special == SPECIAL_KEY_ARROW_RIGHT)))
            {
                // The Preview sub-tab has no columns; Left returns to the
                // previous sub-tab, Right stays.
                if (colorSubTab == 2)
                {
                    if (event.special == SPECIAL_KEY_ARROW_LEFT)
                    {
                        colorSubTab = 1;
                        activeCol = 1;
                        ColorEditingBg(colorSubTab, 1) = true;
                        colorSubTabBar.SetActiveTab(colorSubTab);
                    }

                    return true;
                }

                bool& editBg = ColorEditingBg(colorSubTab, activeCol);

                if (event.special == SPECIAL_KEY_ARROW_RIGHT)
                {
                    if (editBg == false)
                    {
                        // fg -> bg within the current column.
                        editBg = true;
                    }
                    else if (activeCol == 0)
                    {
                        // Past left bg: jump to right column's fg.
                        activeCol = 1;
                        ColorEditingBg(colorSubTab, 1) = false;
                    }
                    else
                    {
                        // Past the right column: advance to the next sub-tab.
                        colorSubTab = colorSubTab + 1;
                        colorSubTabBar.SetActiveTab(colorSubTab);

                        if (colorSubTab != 2)
                        {
                            activeCol = 0;
                            ColorEditingBg(colorSubTab, 0) = false;
                        }
                    }
                }
                else
                {
                    if (editBg == true)
                    {
                        // bg -> fg within the current column.
                        editBg = false;
                    }
                    else if (activeCol == 1)
                    {
                        // Past right fg: jump to left column's bg.
                        activeCol = 0;
                        ColorEditingBg(colorSubTab, 0) = true;
                    }
                    else if (colorSubTab > 0)
                    {
                        // Before the left column: retreat to the previous sub-tab.
                        colorSubTab = colorSubTab - 1;
                        colorSubTabBar.SetActiveTab(colorSubTab);
                        activeCol = 1;
                        ColorEditingBg(colorSubTab, 1) = true;
                    }
                }

                return true;
            }

            // Left/Right switch tabs unless a dropdown is capturing them.
            if ((event.special == SPECIAL_KEY_ARROW_LEFT) || (event.special == SPECIAL_KEY_ARROW_RIGHT))
            {
                int active = tabBar.GetActiveTab();

                if (event.special == SPECIAL_KEY_ARROW_LEFT)
                {
                    active = active - 1;
                }
                else
                {
                    active = active + 1;
                }

                if (active < 0)
                {
                    active = 0;
                }

                if (active >= tabBar.GetTabCount())
                {
                    active = tabBar.GetTabCount() - 1;
                }

                tabBar.SetActiveTab(active);
                focusList = BuildFocusList(tabBar.GetActiveTab());
                focusIndex = 0;
                ApplyFocus();
                return true;
            }

            // On the color list, Up/Down move the selected pair within the
            // active column (clamped).
            if ((onColorList == true) &&
                ((event.special == SPECIAL_KEY_ARROW_UP) || (event.special == SPECIAL_KEY_ARROW_DOWN)))
            {
                if (colorSubTab == 2)
                {
                    return true;
                }

                int count = static_cast<int>(ColorColumn(colorSubTab, activeCol).size());
                int& selected = ColorSel(colorSubTab, activeCol);

                if (event.special == SPECIAL_KEY_ARROW_UP)
                {
                    selected = selected - 1;
                }
                else
                {
                    selected = selected + 1;
                }

                if (selected < 0)
                {
                    selected = 0;
                }

                if (selected >= count)
                {
                    selected = count - 1;
                }

                return true;
            }

            // Tab and Up/Down move focus within the active tab. On the Colors
            // tab, Up/Down are reserved for navigating the list when it holds
            // focus so those cases fall through to the widget.
            bool moveFocus = (event.special == SPECIAL_KEY_TAB);

            if ((onColorList == false) &&
                ((event.special == SPECIAL_KEY_ARROW_UP) || (event.special == SPECIAL_KEY_ARROW_DOWN)))
            {
                moveFocus = true;
            }

            if (moveFocus == true)
            {
                bool backward = false;

                if (event.special == SPECIAL_KEY_TAB)
                {
                    backward = event.shift;
                }
                else if (event.special == SPECIAL_KEY_ARROW_UP)
                {
                    backward = true;
                }

                if (focusList.empty() == false)
                {
                    if (backward == true)
                    {
                        focusIndex = focusIndex - 1;

                        if (focusIndex < 0)
                        {
                            focusIndex = static_cast<int>(focusList.size()) - 1;
                        }
                    }
                    else
                    {
                        focusIndex = focusIndex + 1;

                        if (focusIndex >= static_cast<int>(focusList.size()))
                        {
                            focusIndex = 0;
                        }
                    }
                }

                ApplyFocus();
                return true;
            }

            // Enter on a color-list swatch opens the RGB picker for it.
            if ((event.special == SPECIAL_KEY_ENTER) && (onColorList == true) && (colorSubTab != 2))
            {
                std::vector<sColorPair>& pairs = ColorColumn(colorSubTab, activeCol);
                int selected = ColorSel(colorSubTab, activeCol);

                if ((selected >= 0) && (selected < static_cast<int>(pairs.size())))
                {
                    bool editBg = ColorEditingBg(colorSubTab, activeCol);
                    sRGB* target = pairs[static_cast<size_t>(selected)].fg;

                    if (editBg == true)
                    {
                        target = pairs[static_cast<size_t>(selected)].bg;
                    }

                    sColorResult picked = SelectColorDialog(host, target->r, target->g, target->b);

                    if (picked.ok == true)
                    {
                        target->r = ClampChannel(picked.red);
                        target->g = ClampChannel(picked.green);
                        target->b = ClampChannel(picked.blue);

                        // A manual edit means the colors no longer match a
                        // named palette.
                        palettePicker->SetSelectedIndex(0);
                        lastPaletteSel = 0;
                    }
                }

                return true;
            }

            if ((event.special == SPECIAL_KEY_ENTER) && (focused != &okButton) && (focused != &cancelButton)
                && (focused != palettePicker) && (focused != saveThemeButton))
            {
                *resultPtr = true;
                *closePtr = true;
                return false;
            }
        }

        // Mouse press: focus and route to the widget under the cursor, so every
        // button, field, and dropdown is clickable -- not just the focused one.
        if ((event.type == INPUT_TYPE_MOUSE) && (event.mouseAction == MOUSE_ACTION_PRESS))
        {
            if ((focused != nullptr) && (focused->ContainsEventPoint(event.mouseRow, event.mouseCol) == true))
            {
                focused->HandleEvent(event);
                ProcessPaletteWidgets();

                if (*closePtr == true)
                {
                    return false;
                }

                return true;
            }

            for (int index = 0; index < static_cast<int>(focusList.size()); ++index)
            {
                cWidget* widget = focusList[static_cast<size_t>(index)];

                if ((widget != nullptr) && (widget->ContainsEventPoint(event.mouseRow, event.mouseCol) == true))
                {
                    focusIndex = index;
                    ApplyFocus();
                    widget->HandleEvent(event);
                    ProcessPaletteWidgets();

                    if (*closePtr == true)
                    {
                        return false;
                    }

                    return true;
                }
            }
        }

        // Forward the event to the focused widget.
        if (focused != nullptr)
        {
            focused->HandleEvent(event);
        }

        ProcessPaletteWidgets();

        if (*closePtr == true)
        {
            return false;
        }

        return true;
    };

    host->HostRunModalRaw(draw, handle);

    if (result == false)
    {
        return false;
    }

    // ---- Write every widget value back into the config ----
    // Color pairs point directly into config, so they are already committed.
    config.mInputMode = Clamp(inputModePtr->GetSelectedIndex(), 0, 1);
    config.mShowControls = TriToBool(showControlsPtr);
    config.mCodePage = ParseInt(codePagePtr->GetText());

    int measureIndex = measurementPtr->GetSelectedIndex();

    if (measureIndex == 2)
    {
        config.mMeasurement = "mm";
    }
    else if (measureIndex == 1)
    {
        config.mMeasurement = "cm";
    }
    else
    {
        config.mMeasurement = "in";
    }

    config.mDefaultFont = defaultFontPtr->GetText();
    config.mDefaultFontSize = ParseDouble(fontSizePtr->GetText());
    config.mDefaultFormat = (defaultFormatPtr->GetSelectedIndex() == 1) ? "rtf" : "ws";
    config.mDefaultDirectory = defaultDirPtr->GetText();

    config.mAutoSaveInterval = ParseInt(autoSavePtr->GetText());
    config.mSpellCheckLanguage = spellLangPtr->GetText();
    config.mSpellCheckDotCommands = TriToBool(spellDotPtr);

    for (const sPageField& field : pageFields)
    {
        *field.target = cConfig::ParseMeasurement(field.field->GetText().c_str());
    }

    config.mLandscape = TriToBool(landscapePtr);

    config.mTuiShowTitleBar = TriToBool(showTitlePtr);
    config.mTuiShowRuler = TriToBool(showRulerPtr);
    config.mTuiShowScrollBar = TriToBool(showScrollPtr);
    config.mTuiShowStatusBar = TriToBool(showStatusPtr);
    config.mTuiShowStyleBar = TriToBool(showStylePtr);
    config.mTuiShowMenu = TriToBool(showMenuPtr);
    config.mTuiAlwaysDotCommands = TriToBool(alwaysDotPtr);
    config.mTuiAlwaysFlagColumn = TriToBool(alwaysFlagPtr);
    config.mTuiShowHelp = Clamp(helpPtr->GetSelectedIndex(), 0, 3);

    config.mShortName = shortNamePtr->GetText();
    config.mLongName = longNamePtr->GetText();

    return true;
}

}
