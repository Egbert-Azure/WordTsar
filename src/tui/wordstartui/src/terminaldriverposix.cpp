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

#include "terminaldriverposix.h"

#ifndef _WIN32

#include "utf8helper.h"
#include "colorutils.h"

#include <cerrno>
#include <clocale>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

namespace
{
volatile sig_atomic_t gResizePending = 0;

void PosixResizeHandler(int signalNumber)
{
    if (signalNumber == SIGWINCH)
    {
        gResizePending = 1;
    }
}
}

namespace wordstartui
{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTerminalDriverPosix
///
/// @brief
/// Implements a non-serial POSIX terminal backend using termios and ANSI/VT
/// output.
///
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct the POSIX terminal driver.
///
/////////////////////////////////////////////////////////////////////////////
cTerminalDriverPosix::cTerminalDriverPosix(void)
{
    mOpen = false;
    mHaveOriginalTermios = false;
    mCursorVisible = true;
    mCursorShape = CURSOR_SHAPE_DEFAULT;
    mSize.rows = 25;
    mSize.cols = 80;

    mCapabilities.color16 = true;
    mCapabilities.color256 = true;
    mCapabilities.trueColor = true;
    mCapabilities.bold = true;
    mCapabilities.dim = true;
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
/// Destroy the POSIX terminal driver and restore the terminal if required.
///
/////////////////////////////////////////////////////////////////////////////
cTerminalDriverPosix::~cTerminalDriverPosix(void)
{
    Close();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the terminal was opened and raw mode was entered
///
/// @brief
/// Enter raw terminal mode and initialize the screen.
///
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Probe the terminal for italic and strikethrough support via its terminfo
/// (tput sitm / smxx). Bold, underline, dim and inverse are treated as
/// universal. Unsupported attributes fall back to color rendering.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::DetectCapabilities(void)
{
    const char* term = std::getenv("TERM");

    if ((term == nullptr) || (std::string(term) == "dumb"))
    {
        mCapabilities.italic = false;
        mCapabilities.strikethrough = false;
        mCapabilities.trueColor = false;
        mCapabilities.color256 = false;
        mCapabilities.color16 = true;
        return;
    }

    auto hasCap = [](const char* cap) -> bool
    {
        std::string command = "tput ";
        command += cap;
        command += " >/dev/null 2>&1";
        return std::system(command.c_str()) == 0;
    };

    mCapabilities.italic = hasCap("sitm");
    mCapabilities.strikethrough = hasCap("smxx");

    // Ask terminfo how many colors the terminal advertises.
    int tputColors = -1;
    FILE* pipe = popen("tput colors 2>/dev/null", "r");
    if (pipe != nullptr)
    {
        char buffer[32];
        if (std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
        {
            tputColors = std::atoi(buffer);
        }
        pclose(pipe);
    }

    const char* colorterm = std::getenv("COLORTERM");
    eColorLevel level = DetectColorLevel(colorterm, term, tputColors);

    mCapabilities.trueColor = (level == COLOR_LEVEL_TRUECOLOR);
    mCapabilities.color256 = ((level == COLOR_LEVEL_TRUECOLOR) || (level == COLOR_LEVEL_256));
    mCapabilities.color16 = true;
}

bool cTerminalDriverPosix::Open(void)
{
    if (mOpen == true)
    {
        return true;
    }

    std::setlocale(LC_ALL, "");

    DetectCapabilities();

    if (tcgetattr(STDIN_FILENO, &mOriginalTermios) != 0)
    {
        return false;
    }

    mHaveOriginalTermios = true;

    termios raw = mOriginalTermios;
    raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON | IXOFF));
    raw.c_oflag &= static_cast<tcflag_t>(~(OPOST));
    raw.c_cflag |= CS8;
    raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        return false;
    }

    std::signal(SIGWINCH, PosixResizeHandler);

    mSize = GetSize();
    mOpen = true;

    WriteString("\x1b[?25l");
    WriteString("\x1b[?1000h");
    WriteString("\x1b[?1002h");
    WriteString("\x1b[?1006h");
    WriteString("\x1b[0m");
    WriteString("\x1b[2J");
    WriteString("\x1b[H");
    Flush();

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Restore the terminal to the mode in effect before Open().
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::Close(void)
{
    if (mOpen == false)
    {
        return;
    }

    WriteString("\x1b[?1006l");
    WriteString("\x1b[?1002l");
    WriteString("\x1b[?1000l");
    WriteString("\x1b[0m");
    WriteString("\x1b[0 q");        // restore default cursor shape
    WriteString("\x1b[?25h");
    WriteString("\x1b[2J");
    WriteString("\x1b[H");
    Flush();

    if (mHaveOriginalTermios == true)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &mOriginalTermios);
    }

    mOpen = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return terminal size in rows and columns
///
/// @brief
/// Query the current terminal window size.
///
/////////////////////////////////////////////////////////////////////////////
sTerminalSize cTerminalDriverPosix::GetSize(void) const
{
    winsize size;
    sTerminalSize result;

    result.rows = 25;
    result.cols = 80;

    std::memset(&size, 0, sizeof(size));

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0)
    {
        if ((size.ws_row > 0) && (size.ws_col > 0))
        {
            result.rows = static_cast<int>(size.ws_row);
            result.cols = static_cast<int>(size.ws_col);
        }
    }

    mSize = result;

    return result;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return terminal capability flags
///
/// @brief
/// Return the capabilities used by the ANSI/VT backend.
///
/////////////////////////////////////////////////////////////////////////////
sTerminalCapabilities cTerminalDriverPosix::GetCapabilities(void) const
{
    return mCapabilities;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] event read from the terminal
/// @param  int timeoutMs [in] maximum time to wait in milliseconds
///
/// @return true when an event was read
///
/// @brief
/// Read and decode one input event.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverPosix::ReadEvent(sInputEvent& event, int timeoutMs)
{
    InitializeEvent(event);

    if (gResizePending != 0)
    {
        gResizePending = 0;
        sTerminalSize size = GetSize();
        event.type = INPUT_TYPE_RESIZE;
        event.resizeRows = size.rows;
        event.resizeCols = size.cols;
        return true;
    }

    std::vector<uint8_t> bytes;

    if (mPendingBytes.empty() == false)
    {
        bytes.push_back(mPendingBytes.front());
        mPendingBytes.erase(mPendingBytes.begin());
    }
    else
    {
        uint8_t byte = 0;

        if (ReadByte(byte, timeoutMs) == false)
        {
            return false;
        }

        bytes.push_back(byte);
    }

    if (bytes[0] == 0x1BU)
    {
        uint8_t byte = 0;

        while (ReadByte(byte, 15) == true)
        {
            bytes.push_back(byte);

            if (((byte >= 0x40U) && (byte <= 0x7EU)) && (bytes.size() > 1U))
            {
                bool csiIntro = (bytes[1] == '[');
                bool ss3Intro = (bytes[1] == 'O');

                if ((csiIntro == false) && (ss3Intro == false))
                {
                    // ESC + single printable byte (Alt+key): stop here.
                    break;
                }

                // CSI (ESC [ ... final) and SS3 (ESC O final, e.g. F1-F4): the
                // second byte is the intro, so keep reading until a final byte
                // beyond it.
                if (bytes.size() > 2U)
                {
                    break;
                }
            }

            if (bytes.size() > 64U)
            {
                break;
            }
        }
    }
    else if ((bytes[0] & 0x80U) != 0U)
    {
        int expected = 1;

        if ((bytes[0] & 0xE0U) == 0xC0U)
        {
            expected = 2;
        }
        else if ((bytes[0] & 0xF0U) == 0xE0U)
        {
            expected = 3;
        }
        else if ((bytes[0] & 0xF8U) == 0xF0U)
        {
            expected = 4;
        }

        while (static_cast<int>(bytes.size()) < expected)
        {
            uint8_t byte = 0;

            if (ReadByte(byte, 15) == false)
            {
                break;
            }

            bytes.push_back(byte);
        }
    }

    return DecodeBytes(event, bytes);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<sCellRun>& runs [in] changed screen runs
///
/// @return nothing
///
/// @brief
/// Present changed cell runs to the terminal using ANSI escape sequences.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::Present(const std::vector<sCellRun>& runs)
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
/// Move and show or hide the terminal cursor.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::SetCursor(int row, int col, bool visible)
{
    WriteString(MoveSequence(row, col));

    if (visible == true)
    {
        WriteString("\x1b[?25h");
        mCursorVisible = true;
    }
    else
    {
        WriteString("\x1b[?25l");
        mCursorVisible = false;
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
/// insert-style editing, blinking block for overwrite. Redundant writes are
/// skipped so the shape only changes when the mode does.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::SetCursorShape(eCursorShape shape)
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
/// Flush pending terminal output.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::Flush(void)
{
    fsync(STDOUT_FILENO);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] event to initialize
///
/// @return nothing
///
/// @brief
/// Reset an input event to its default empty state.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::InitializeEvent(sInputEvent& event) const
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
/// @param  uint8_t& byte [out] byte read from stdin
/// @param  int timeoutMs [in] maximum time to wait
///
/// @return true when a byte was read
///
/// @brief
/// Read one byte using poll so resize and idle redraws can be processed.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverPosix::ReadByte(uint8_t& byte, int timeoutMs)
{
    pollfd descriptor;
    descriptor.fd = STDIN_FILENO;
    descriptor.events = POLLIN;
    descriptor.revents = 0;

    const int pollResult = poll(&descriptor, 1, timeoutMs);

    if (pollResult <= 0)
    {
        return false;
    }

    if ((descriptor.revents & POLLIN) == 0)
    {
        return false;
    }

    ssize_t readResult = read(STDIN_FILENO, &byte, 1);

    if (readResult == 1)
    {
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] decoded input event
/// @param  const std::vector<uint8_t>& bytes [in] raw bytes read
///
/// @return true when the bytes were decoded into an event
///
/// @brief
/// Decode terminal bytes into a normalized event while retaining raw bytes.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverPosix::DecodeBytes(sInputEvent& event, const std::vector<uint8_t>& bytes)
{
    event.rawBytes = bytes;

    if (bytes.empty() == true)
    {
        return false;
    }

    const uint8_t first = bytes[0];

    if (first == 0x1BU)
    {
        return DecodeEscape(event, bytes);
    }

    if (first == 0x7FU)
    {
        event.type = INPUT_TYPE_SPECIAL;
        event.special = SPECIAL_KEY_BACKSPACE;
        return true;
    }

    if (first < 0x20U)
    {
        event.type = INPUT_TYPE_CONTROL;
        event.controlCode = first;
        event.ctrl = true;

        if (first == 0x09U)
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_TAB;
        }
        else if (first == 0x0DU)
        {
            event.type = INPUT_TYPE_SPECIAL;
            event.special = SPECIAL_KEY_ENTER;
            event.controlCode = 13;
            event.ctrl = true;
        }
        else if (first == 0x0AU)
        {
            event.type = INPUT_TYPE_CONTROL;
            event.controlCode = 10;
            event.ctrl = true;
        }

        return true;
    }

    return DecodeUtf8Text(event, bytes);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] decoded event
/// @param  const std::vector<uint8_t>& bytes [in] escape sequence bytes
///
/// @return true when the escape sequence was decoded
///
/// @brief
/// Decode escape and CSI sequences used by common terminals.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverPosix::DecodeEscape(sInputEvent& event, const std::vector<uint8_t>& bytes)
{
    if (bytes.size() == 1U)
    {
        event.type = INPUT_TYPE_SPECIAL;
        event.special = SPECIAL_KEY_ESCAPE;
        return true;
    }

    std::string sequence;

    for (uint8_t byte : bytes)
    {
        sequence.push_back(static_cast<char>(byte));
    }

    if (sequence.rfind("\x1b[", 0) == 0)
    {
        return DecodeCsi(event, sequence);
    }

    if (sequence == "\x1bOP")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 1;
        return true;
    }
    else if (sequence == "\x1bOQ")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 2;
        return true;
    }
    else if (sequence == "\x1bOR")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 3;
        return true;
    }
    else if (sequence == "\x1bOS")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 4;
        return true;
    }

    // ESC followed by a single printable byte is Alt+key (meta). Deliver it as
    // a text event with the alt modifier set so menu hotkeys and WordStar
    // Alt-chords work; the raw bytes remain available on the event.
    if ((bytes.size() == 2U) && (bytes[1] >= 0x20U) && (bytes[1] < 0x7fU))
    {
        event.type = INPUT_TYPE_TEXT;
        event.alt = true;
        event.textUtf8 = std::string(1, static_cast<char>(bytes[1]));
        event.codepoint = static_cast<char32_t>(bytes[1]);
        return true;
    }

    event.type = INPUT_TYPE_SPECIAL;
    event.special = SPECIAL_KEY_ESCAPE;

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] decoded event
/// @param  const std::string& sequence [in] CSI sequence
///
/// @return true when the CSI sequence was decoded
///
/// @brief
/// Decode common xterm/Linux-console CSI key sequences.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverPosix::DecodeCsi(sInputEvent& event, const std::string& sequence)
{
    if (sequence.rfind("\x1b[<", 0) == 0)
    {
        return DecodeSgrMouse(event, sequence);
    }

    event.type = INPUT_TYPE_SPECIAL;

    if (sequence == "\x1b[A")
    {
        event.special = SPECIAL_KEY_ARROW_UP;
    }
    else if (sequence == "\x1b[B")
    {
        event.special = SPECIAL_KEY_ARROW_DOWN;
    }
    else if (sequence == "\x1b[C")
    {
        event.special = SPECIAL_KEY_ARROW_RIGHT;
    }
    else if (sequence == "\x1b[D")
    {
        event.special = SPECIAL_KEY_ARROW_LEFT;
    }
    else if ((sequence == "\x1b[H") || (sequence == "\x1b[1~") || (sequence == "\x1b[7~"))
    {
        event.special = SPECIAL_KEY_HOME;
    }
    else if ((sequence == "\x1b[F") || (sequence == "\x1b[4~") || (sequence == "\x1b[8~"))
    {
        event.special = SPECIAL_KEY_END;
    }
    else if (sequence == "\x1b[2~")
    {
        event.special = SPECIAL_KEY_INSERT;
    }
    else if (sequence == "\x1b[3~")
    {
        event.special = SPECIAL_KEY_DELETE;
    }
    else if (sequence == "\x1b[5~")
    {
        event.special = SPECIAL_KEY_PAGE_UP;
    }
    else if (sequence == "\x1b[6~")
    {
        event.special = SPECIAL_KEY_PAGE_DOWN;
    }
    else if (sequence == "\x1b[11~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 1;
    }
    else if (sequence == "\x1b[12~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 2;
    }
    else if (sequence == "\x1b[13~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 3;
    }
    else if (sequence == "\x1b[14~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 4;
    }
    else if (sequence == "\x1b[15~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 5;
    }
    else if (sequence == "\x1b[17~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 6;
    }
    else if (sequence == "\x1b[18~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 7;
    }
    else if (sequence == "\x1b[19~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 8;
    }
    else if (sequence == "\x1b[20~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 9;
    }
    else if (sequence == "\x1b[21~")
    {
        event.type = INPUT_TYPE_FUNCTION;
        event.functionKey = 10;
    }
    else
    {
        event.special = SPECIAL_KEY_ESCAPE;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] decoded mouse event
/// @param  const std::string& sequence [in] SGR mouse sequence
///
/// @return true when the sequence was decoded
///
/// @brief
/// Decode xterm SGR mouse sequences into portable mouse events.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverPosix::DecodeSgrMouse(sInputEvent& event, const std::string& sequence) const
{
    if (sequence.size() < 7U)
    {
        return false;
    }

    char final = sequence.back();

    if ((final != 'M') && (final != 'm'))
    {
        return false;
    }

    std::string body = sequence.substr(3, sequence.size() - 4);
    std::vector<int> values;
    std::string current;

    for (char ch : body)
    {
        if (ch == ';')
        {
            values.push_back(std::atoi(current.c_str()));
            current.clear();
        }
        else
        {
            current.push_back(ch);
        }
    }

    if (current.empty() == false)
    {
        values.push_back(std::atoi(current.c_str()));
    }

    if (values.size() != 3U)
    {
        return false;
    }

    int buttonCode = values[0];
    int col = values[1] - 1;
    int row = values[2] - 1;

    event.type = INPUT_TYPE_MOUSE;
    event.mouseRow = row;
    event.mouseCol = col;
    event.mouseWheel = 0;
    event.mouseButton = MOUSE_BUTTON_NONE;
    event.mouseAction = MOUSE_ACTION_NONE;

    if ((buttonCode & 4) != 0)
    {
        event.shift = true;
    }

    if ((buttonCode & 8) != 0)
    {
        event.alt = true;
    }

    if ((buttonCode & 16) != 0)
    {
        event.ctrl = true;
    }

    if ((buttonCode & 64) != 0)
    {
        event.mouseAction = MOUSE_ACTION_WHEEL;

        if ((buttonCode & 1) == 0)
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

    int button = buttonCode & 3;

    if (button == 0)
    {
        event.mouseButton = MOUSE_BUTTON_LEFT;
    }
    else if (button == 1)
    {
        event.mouseButton = MOUSE_BUTTON_MIDDLE;
    }
    else if (button == 2)
    {
        event.mouseButton = MOUSE_BUTTON_RIGHT;
    }
    else
    {
        event.mouseButton = MOUSE_BUTTON_NONE;
    }

    if (final == 'm')
    {
        event.mouseAction = MOUSE_ACTION_RELEASE;
        return true;
    }

    if ((buttonCode & 32) != 0)
    {
        event.mouseAction = MOUSE_ACTION_DRAG;
    }
    else
    {
        event.mouseAction = MOUSE_ACTION_PRESS;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  sInputEvent& event [out] decoded event
/// @param  const std::vector<uint8_t>& bytes [in] UTF-8 bytes
///
/// @return true when a text event was created
///
/// @brief
/// Decode UTF-8 text input into a printable grapheme event.
///
/////////////////////////////////////////////////////////////////////////////
bool cTerminalDriverPosix::DecodeUtf8Text(sInputEvent& event, const std::vector<uint8_t>& bytes)
{
    std::string text;

    for (uint8_t byte : bytes)
    {
        text.push_back(static_cast<char>(byte));
    }

    size_t index = 0;
    char32_t codepoint = 0;

    if (cUtf8Helper::DecodeNext(text, index, codepoint) == false)
    {
        return false;
    }

    event.type = INPUT_TYPE_TEXT;
    event.textUtf8 = text;
    event.codepoint = codepoint;

    return true;
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
/// Build the ANSI style sequence needed to switch to a new style.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTerminalDriverPosix::BuildAnsiStyle(const sStyle& style, const sStyle& previous, bool force) const
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
    if ((style.attrs & CELL_ATTR_BLINK) != 0U)
    {
        text += ";5";
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
std::string cTerminalDriverPosix::MoveSequence(int row, int col) const
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
/// Map a 16-color TUI color to ANSI SGR numbers.
///
/////////////////////////////////////////////////////////////////////////////
int cTerminalDriverPosix::AnsiColor(eColor color, bool foreground) const
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
/// @param  const std::string& text [in] bytes to write
///
/// @return nothing
///
/// @brief
/// Write a string to stdout.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminalDriverPosix::WriteString(const std::string& text)
{
    const char* data = text.data();
    size_t remaining = text.size();

    while (remaining > 0U)
    {
        const ssize_t written = write(STDOUT_FILENO, data, remaining);

        if (written <= 0)
        {
            break;
        }

        data += written;
        remaining -= static_cast<size_t>(written);
    }
}

}

#endif
