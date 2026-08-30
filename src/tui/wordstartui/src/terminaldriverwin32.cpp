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

#include "terminaldriverwin32.h"

#ifdef _WIN32

#include "utf8helper.h"
#include "colorutils.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTerminalDriverWin32
///
/// @brief
/// Implements the Windows Console backend using native input events and VT
/// output.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct the Windows terminal driver.
///
/////////////////////////////////////////////////////////////////////////////
cTerminalDriverWin32::cTerminalDriverWin32(void)
{
    mInputHandle = INVALID_HANDLE_VALUE;
    mOutputHandle = INVALID_HANDLE_VALUE;
    mOriginalInputMode = 0;
    mOriginalOutputMode = 0;
    mOriginalInputCodePage = 0;
    mOriginalOutputCodePage = 0;
    mOpen = false;
    mCursorShape = CURSOR_SHAPE_DEFAULT;

    mCapabilities.color16 = true;
    mCapabilities.color256 = true;
    mCapabilities.trueColor = true;
    mCapabilities.bold = true;
    mCapabilities.dim = false;
    mCapabilities.italic = true;
    mCapabilities.underline = true;
    mCapabilities.strikethrough = true;
    mCapabilities.inverse = true;
    mCapabilities.blink = false;
    mCapabilities.unicode = true;
    mCapabilities.mouse = true;
    mCapabilities.resizeEvents = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destroy the Windows terminal driver and restore console state.
///
/////////////////////////////////////////////////////////////////////////////
cTerminalDriverWin32::~cTerminalDriverWin32(void)
{
    Close();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the console was opened and configured
///
/// @brief
/// Configure Windows console input and output for TUI use.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverWin32::Open(void)
{
    if (mOpen == true)
    {
        return true;
    }

    mInputHandle = GetStdHandle(STD_INPUT_HANDLE);
    mOutputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    if ((mInputHandle == INVALID_HANDLE_VALUE) || (mOutputHandle == INVALID_HANDLE_VALUE))
    {
        return false;
    }

    if (GetConsoleMode(mInputHandle, &mOriginalInputMode) == 0)
    {
        return false;
    }

    if (GetConsoleMode(mOutputHandle, &mOriginalOutputMode) == 0)
    {
        return false;
    }

    mOriginalInputCodePage = GetConsoleCP();
    mOriginalOutputCodePage = GetConsoleOutputCP();

    DWORD inputMode = mOriginalInputMode;
    inputMode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
    inputMode &= ~ENABLE_QUICK_EDIT_MODE;
    inputMode |= ENABLE_WINDOW_INPUT;
    inputMode |= ENABLE_MOUSE_INPUT;
    inputMode |= ENABLE_EXTENDED_FLAGS;

    DWORD outputMode = mOriginalOutputMode;
    outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;

    if (SetConsoleMode(mInputHandle, inputMode) == 0)
    {
        return false;
    }

    if (SetConsoleMode(mOutputHandle, outputMode) == 0)
    {
        return false;
    }

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    // VT processing is enabled above, so italic and underline render on modern
    // Windows consoles; strikethrough is reliable on Windows Terminal.
    mCapabilities.italic = true;
    mCapabilities.strikethrough = (std::getenv("WT_SESSION") != nullptr);

    // Modern VT-enabled consoles default to truecolor (set in the constructor).
    // Windows Terminal advertises truecolor; otherwise honor COLORTERM if set.
    bool windowsTerminal = (std::getenv("WT_SESSION") != nullptr);
    const char* colorterm = std::getenv("COLORTERM");
    if (windowsTerminal == true)
    {
        mCapabilities.trueColor = true;
        mCapabilities.color256 = true;
    }
    else if (colorterm != nullptr)
    {
        eColorLevel level = DetectColorLevel(colorterm, nullptr, -1);
        mCapabilities.trueColor = (level == COLOR_LEVEL_TRUECOLOR);
        mCapabilities.color256 = ((level == COLOR_LEVEL_TRUECOLOR) || (level == COLOR_LEVEL_256));
    }
    mCapabilities.color16 = true;

    mOpen = true;

    WriteString("\x1b[?25l");
    WriteString("\x1b[0m");
    WriteString("\x1b[2J");
    WriteString("\x1b[H");

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Restore console state.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverWin32::Close(void)
{
    if (mOpen == false)
    {
        return;
    }

    WriteString("\x1b[0m");
    WriteString("\x1b[0 q");        // restore default cursor shape
    WriteString("\x1b[?25h");
    WriteString("\x1b[2J");
    WriteString("\x1b[H");

    SetConsoleMode(mInputHandle, mOriginalInputMode);
    SetConsoleMode(mOutputHandle, mOriginalOutputMode);
    SetConsoleCP(mOriginalInputCodePage);
    SetConsoleOutputCP(mOriginalOutputCodePage);

    mOpen = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return console size in rows and columns
///
/// @brief
/// Query the active Windows console viewport size.
///
/////////////////////////////////////////////////////////////////////////////
sTerminalSize cTerminalDriverWin32::GetSize(void) const
{
    sTerminalSize size;
    CONSOLE_SCREEN_BUFFER_INFO info;

    size.rows = 25;
    size.cols = 80;

    if (GetConsoleScreenBufferInfo(mOutputHandle, &info) != 0)
    {
        size.rows = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1);
        size.cols = static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1);
    }

    return size;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return terminal capability flags
///
/// @brief
/// Return the capabilities used by the Windows console backend.
///
/////////////////////////////////////////////////////////////////////////////
sTerminalCapabilities cTerminalDriverWin32::GetCapabilities(void) const
{
    return mCapabilities;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] event read from the console
/// @param  int timeoutMs [in] maximum time to wait in milliseconds
///
/// @return true when an event was read
///
/// @brief
/// Read one Windows console input event.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverWin32::ReadEvent(sInputEvent& event, int timeoutMs)
{
    InitializeEvent(event);

    DWORD waitResult = WaitForSingleObject(mInputHandle, static_cast<DWORD>(timeoutMs));

    if (waitResult != WAIT_OBJECT_0)
    {
        return false;
    }

    INPUT_RECORD record;
    DWORD readCount = 0;

    while (ReadConsoleInputW(mInputHandle, &record, 1, &readCount) != 0)
    {
        if (readCount == 0)
        {
            return false;
        }

        if (record.EventType == KEY_EVENT)
        {
            if (record.Event.KeyEvent.bKeyDown != 0)
            {
                if (DecodeKeyEvent(event, record.Event.KeyEvent) == true)
                {
                    return true;
                }
            }
        }
        else if (record.EventType == MOUSE_EVENT)
        {
            if (DecodeMouseEvent(event, record.Event.MouseEvent) == true)
            {
                return true;
            }
        }
        else if (record.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            sTerminalSize size = GetSize();
            event.type = INPUT_TYPE_RESIZE;
            event.resizeRows = size.rows;
            event.resizeCols = size.cols;
            return true;
        }

        DWORD available = 0;

        if (GetNumberOfConsoleInputEvents(mInputHandle, &available) == 0)
        {
            return false;
        }

        if (available == 0)
        {
            return false;
        }
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<sCellRun>& runs [in] changed screen runs
///
/// @return nothing
///
/// @brief
/// Present changed cell runs to the console using VT output.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverWin32::Present(const std::vector<sCellRun>& runs)
{
    sStyle previous;
    previous.fg = MakeDefaultColor();
    previous.bg = MakeDefaultColor();
    previous.attrs = 0xFFFFFFFFU;

    for (const sCellRun& run : runs)
    {
        WriteString(MoveSequence(run.row, run.col));

        bool previousWidePrinted = false;

        for (const sCell& cell : run.cells)
        {
            WriteString(BuildAnsiStyle(cell.style, previous, false));
            previous = cell.style;

            if (cell.wideTail == true)
            {
                if (previousWidePrinted == false)
                {
                    WriteString(" ");
                }
                previousWidePrinted = false;
            }
            else
            {
                if (cell.textUtf8.empty() == true)
                {
                    WriteString(" ");
                }
                else
                {
                    WriteString(cell.textUtf8);
                }

                if (cell.width > 1)
                {
                    previousWidePrinted = true;
                }
                else
                {
                    previousWidePrinted = false;
                }
            }
        }
    }

    WriteString("\x1b[0m");
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] zero-based row
/// @param  int col [in] zero-based column
/// @param  bool visible [in] true to show the cursor
///
/// @return nothing
///
/// @brief
/// Move and show or hide the console cursor.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverWin32::SetCursor(int row, int col, bool visible)
{
    WriteString(MoveSequence(row, col));

    if (visible == true)
    {
        WriteString("\x1b[?25h");
    }
    else
    {
        WriteString("\x1b[?25l");
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eCursorShape shape [in] requested cursor shape
///
/// @return nothing
///
/// @brief
/// Set the hardware cursor shape via DECSCUSR (CSI n SP q). Blinking bar for
/// insert-style editing, blinking block for overwrite.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverWin32::SetCursorShape(eCursorShape shape)
{
    if (shape == mCursorShape)
    {
        return;
    }

    mCursorShape = shape;

    switch (shape)
    {
        case CURSOR_SHAPE_BLOCK:
        {
            WriteString("\x1b[1 q");    // blinking block
            break;
        }
        case CURSOR_SHAPE_BLOCK_STEADY:
        {
            WriteString("\x1b[2 q");    // steady block
            break;
        }
        case CURSOR_SHAPE_UNDERLINE:
        {
            WriteString("\x1b[3 q");    // blinking underline
            break;
        }
        case CURSOR_SHAPE_BAR:
        {
            WriteString("\x1b[5 q");    // blinking bar
            break;
        }
        case CURSOR_SHAPE_DEFAULT:
        default:
        {
            WriteString("\x1b[0 q");    // terminal default
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Flush pending console output.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverWin32::Flush(void)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] event to initialize
///
/// @return nothing
///
/// @brief
/// Reset an input event to its empty state.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverWin32::InitializeEvent(sInputEvent& event) const
{
    event.type = INPUT_TYPE_NONE;
    event.special = SPECIAL_KEY_NONE;
    event.rawBytes.clear();
    event.textUtf8.clear();
    event.codepoint = 0;
    event.controlCode = 0;
    event.functionKey = 0;
    event.ctrl = false;
    event.alt = false;
    event.shift = false;
    event.resizeRows = 0;
    event.resizeCols = 0;
    event.mouseRow = 0;
    event.mouseCol = 0;
    event.mouseButton = MOUSE_BUTTON_NONE;
    event.mouseAction = MOUSE_ACTION_NONE;
    event.mouseWheel = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] decoded input event
/// @param  const KEY_EVENT_RECORD& keyEvent [in] Windows key record
///
/// @return true when the key record was decoded
///
/// @brief
/// Translate native Windows key information into the portable input event.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverWin32::DecodeKeyEvent(sInputEvent& event, const KEY_EVENT_RECORD& keyEvent)
{
    const DWORD state = keyEvent.dwControlKeyState;

    if ((state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0U)
    {
        event.ctrl = true;
    }
    if ((state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0U)
    {
        event.alt = true;
    }
    if ((state & SHIFT_PRESSED) != 0U)
    {
        event.shift = true;
    }

    switch (keyEvent.wVirtualKeyCode)
    {
        case VK_LEFT:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_ARROW_LEFT;
            return true;
        }
        case VK_RIGHT:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_ARROW_RIGHT;
            return true;
        }
        case VK_UP:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_ARROW_UP;
            return true;
        }
        case VK_DOWN:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_ARROW_DOWN;
            return true;
        }
        case VK_HOME:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_HOME;
            return true;
        }
        case VK_END:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_END;
            return true;
        }
        case VK_PRIOR:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_PAGE_UP;
            return true;
        }
        case VK_NEXT:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_PAGE_DOWN;
            return true;
        }
        case VK_INSERT:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_INSERT;
            return true;
        }
        case VK_DELETE:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_DELETE;
            return true;
        }
        case VK_ESCAPE:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_ESCAPE;
            return true;
        }
        case VK_RETURN:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_ENTER;
            event.controlCode = 13;
            event.ctrl = true;
            event.rawBytes.push_back(13);
            return true;
        }
        case VK_TAB:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_TAB;
            event.rawBytes.push_back(9);
            return true;
        }
        case VK_BACK:
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_BACKSPACE;
            event.rawBytes.push_back(8);
            return true;
        }
        default:
        {
            break;
        }
    }

    if ((keyEvent.wVirtualKeyCode >= VK_F1) && (keyEvent.wVirtualKeyCode <= VK_F12))
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = static_cast<int>(keyEvent.wVirtualKeyCode - VK_F1 + 1);
        return true;
    }

    wchar_t wideChar = keyEvent.uChar.UnicodeChar;

    if (wideChar != 0)
    {
        if (wideChar < 0x20)
        {
            event.type = INPUT_TYPE_CONTROL;
            event.controlCode = static_cast<int>(wideChar);
            event.ctrl = true;
            event.rawBytes.push_back(static_cast<uint8_t>(wideChar));
            return true;
        }

        event.type = INPUT_TYPE_TEXT;
        event.textUtf8 = WideToUtf8(wideChar);
        event.codepoint = static_cast<char32_t>(wideChar);
        event.rawBytes.assign(event.textUtf8.begin(), event.textUtf8.end());
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] decoded input event
/// @param  const MOUSE_EVENT_RECORD& mouseEvent [in] Windows mouse record
///
/// @return true when the mouse record was decoded
///
/// @brief
/// Translate native Windows mouse events into the portable input event.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverWin32::DecodeMouseEvent(sInputEvent& event, const MOUSE_EVENT_RECORD& mouseEvent)
{
    const DWORD state = mouseEvent.dwControlKeyState;

    if ((state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0U)
    {
        event.ctrl = true;
    }
    if ((state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0U)
    {
        event.alt = true;
    }
    if ((state & SHIFT_PRESSED) != 0U)
    {
        event.shift = true;
    }

    event.type = INPUT_TYPE_MOUSE;
    event.mouseRow = static_cast<int>(mouseEvent.dwMousePosition.Y);
    event.mouseCol = static_cast<int>(mouseEvent.dwMousePosition.X);
    event.mouseButton = MOUSE_BUTTON_NONE;
    event.mouseAction = MOUSE_ACTION_NONE;
    event.mouseWheel = 0;

    if (mouseEvent.dwEventFlags == MOUSE_WHEELED)
    {
        SHORT delta = static_cast<SHORT>((mouseEvent.dwButtonState >> 16) & 0xFFFFU);
        event.mouseAction = MOUSE_ACTION_WHEEL;

        if (delta > 0)
        {
            event.mouseButton = MOUSE_BUTTON_WHEEL_UP;
            event.mouseWheel = -1;
        }
        else
        {
            event.mouseButton = MOUSE_BUTTON_WHEEL_DOWN;
            event.mouseWheel = 1;
        }

        return true;
    }

    if ((mouseEvent.dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0U)
    {
        event.mouseButton = MOUSE_BUTTON_LEFT;
    }
    else if ((mouseEvent.dwButtonState & FROM_LEFT_2ND_BUTTON_PRESSED) != 0U)
    {
        event.mouseButton = MOUSE_BUTTON_MIDDLE;
    }
    else if ((mouseEvent.dwButtonState & RIGHTMOST_BUTTON_PRESSED) != 0U)
    {
        event.mouseButton = MOUSE_BUTTON_RIGHT;
    }

    if (mouseEvent.dwEventFlags == MOUSE_MOVED)
    {
        if (event.mouseButton == MOUSE_BUTTON_NONE)
        {
            return false;
        }

        event.mouseAction = MOUSE_ACTION_DRAG;
        return true;
    }

    if ((mouseEvent.dwEventFlags == 0) || (mouseEvent.dwEventFlags == DOUBLE_CLICK))
    {
        if (event.mouseButton == MOUSE_BUTTON_NONE)
        {
            event.mouseAction = MOUSE_ACTION_RELEASE;
        }
        else
        {
            event.mouseAction = MOUSE_ACTION_PRESS;
        }

        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  wchar_t value [in] UTF-16 code unit from the console
///
/// @return UTF-8 encoded text
///
/// @brief
/// Convert a Windows Unicode character to UTF-8.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTerminalDriverWin32::WideToUtf8(wchar_t value) const
{
    wchar_t input[2];
    input[0] = value;
    input[1] = 0;

    int needed = WideCharToMultiByte(CP_UTF8, 0, input, 1, nullptr, 0, nullptr, nullptr);

    if (needed <= 0)
    {
        return std::string();
    }

    std::string output;
    output.resize(static_cast<size_t>(needed));

    WideCharToMultiByte(CP_UTF8, 0, input, 1, output.data(), needed, nullptr, nullptr);

    return output;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sStyle& style [in] requested style
/// @param  const sStyle& previous [in] previously emitted style
/// @param  bool force [in] true to force all attributes
///
/// @return ANSI SGR sequence
///
/// @brief
/// Build an ANSI style sequence for Windows VT output.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTerminalDriverWin32::BuildAnsiStyle(const sStyle& style, const sStyle& previous, bool force) const
{
    if ((force == false) && (style.fg == previous.fg) && (style.bg == previous.bg) && (style.attrs == previous.attrs))
    {
        return std::string();
    }

    std::string text = "\x1b[0";

    if ((style.attrs & CELL_ATTR_BOLD) != 0U)
    {
        text += ";1";
    }
    if ((style.attrs & CELL_ATTR_DIM) != 0U)
    {
        text += ";2";
    }
    if ((style.attrs & CELL_ATTR_ITALIC) != 0U)
    {
        text += ";3";
    }
    if ((style.attrs & CELL_ATTR_UNDERLINE) != 0U)
    {
        text += ";4";
    }
    if ((style.attrs & CELL_ATTR_INVERSE) != 0U)
    {
        text += ";7";
    }
    if ((style.attrs & CELL_ATTR_STRIKETHROUGH) != 0U)
    {
        text += ";9";
    }

    // Emit each color at the depth the terminal supports.
    eColorLevel level = COLOR_LEVEL_16;
    if (mCapabilities.trueColor == true)
    {
        level = COLOR_LEVEL_TRUECOLOR;
    }
    else if (mCapabilities.color256 == true)
    {
        level = COLOR_LEVEL_256;
    }

    text += ";";
    text += AnsiColorParams(style.fg, true, level);
    text += ";";
    text += AnsiColorParams(style.bg, false, level);

    text += "m";

    return text;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int row [in] zero-based row
/// @param  int col [in] zero-based column
///
/// @return ANSI cursor movement sequence
///
/// @brief
/// Build a one-based ANSI cursor movement sequence.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTerminalDriverWin32::MoveSequence(int row, int col) const
{
    std::string sequence;

    sequence = "\x1b[";
    sequence += std::to_string(row + 1);
    sequence += ";";
    sequence += std::to_string(col + 1);
    sequence += "H";

    return sequence;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eColor color [in] TUI color
/// @param  bool foreground [in] true for foreground mapping
///
/// @return ANSI color number or -1 for default
///
/// @brief
/// Map TUI colors to ANSI SGR colors.
///
/////////////////////////////////////////////////////////////////////////////
int cTerminalDriverWin32::AnsiColor(eColor color, bool foreground) const
{
    int base = 30;
    int result = -1;

    if (foreground == false)
    {
        base = 40;
    }

    switch (color)
    {
        case COLOR_BLACK:
        {
            result = base + 0;
            break;
        }
        case COLOR_RED:
        {
            result = base + 1;
            break;
        }
        case COLOR_GREEN:
        {
            result = base + 2;
            break;
        }
        case COLOR_BROWN:
        {
            result = base + 3;
            break;
        }
        case COLOR_BLUE:
        {
            result = base + 4;
            break;
        }
        case COLOR_MAGENTA:
        {
            result = base + 5;
            break;
        }
        case COLOR_CYAN:
        {
            result = base + 6;
            break;
        }
        case COLOR_LIGHT_GRAY:
        case COLOR_WHITE:
        {
            result = base + 7;
            break;
        }
        case COLOR_DARK_GRAY:
        {
            result = base + 60 + 0;
            break;
        }
        case COLOR_LIGHT_RED:
        {
            result = base + 60 + 1;
            break;
        }
        case COLOR_LIGHT_GREEN:
        {
            result = base + 60 + 2;
            break;
        }
        case COLOR_YELLOW:
        {
            result = base + 60 + 3;
            break;
        }
        case COLOR_LIGHT_BLUE:
        {
            result = base + 60 + 4;
            break;
        }
        case COLOR_LIGHT_MAGENTA:
        {
            result = base + 60 + 5;
            break;
        }
        case COLOR_LIGHT_CYAN:
        {
            result = base + 60 + 6;
            break;
        }
        case COLOR_DEFAULT:
        default:
        {
            result = -1;
            break;
        }
    }

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] UTF-8 text or VT sequence to write
///
/// @return nothing
///
/// @brief
/// Write bytes to the Windows console output stream.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverWin32::WriteString(const std::string& text)
{
    DWORD written = 0;
    WriteFile(mOutputHandle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
}

}

#endif
