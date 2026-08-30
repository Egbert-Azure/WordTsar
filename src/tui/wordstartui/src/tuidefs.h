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

#ifndef WORDTSAR_TUI_TUIDEFS_H
#define WORDTSAR_TUI_TUIDEFS_H

#include <cstdint>
#include <string>
#include <vector>

namespace wordstartui
{

enum eColor
{
    COLOR_DEFAULT,
    COLOR_BLACK,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_RED,
    COLOR_MAGENTA,
    COLOR_BROWN,
    COLOR_LIGHT_GRAY,
    COLOR_DARK_GRAY,
    COLOR_LIGHT_BLUE,
    COLOR_LIGHT_GREEN,
    COLOR_LIGHT_CYAN,
    COLOR_LIGHT_RED,
    COLOR_LIGHT_MAGENTA,
    COLOR_YELLOW,
    COLOR_WHITE
};

// A 24-bit cell color. When isDefault is true the terminal's own default
// foreground/background is used (SGR 39/49) and r/g/b are ignored. The eColor
// enum above is kept as a convenience for the built-in theme and color picker;
// it is converted to sColor via MakePaletteColor().
struct sColor
{
    bool isDefault;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool operator==(const sColor& other) const = default;
};

enum eCellAttr
{
    CELL_ATTR_NONE = 0x0000,
    CELL_ATTR_BOLD = 0x0001,
    CELL_ATTR_DIM = 0x0002,
    CELL_ATTR_ITALIC = 0x0004,
    CELL_ATTR_UNDERLINE = 0x0008,
    CELL_ATTR_STRIKETHROUGH = 0x0010,
    CELL_ATTR_INVERSE = 0x0020,
    CELL_ATTR_BLINK = 0x0040,
    CELL_ATTR_SUPERSCRIPT_PREVIEW = 0x0080,
    CELL_ATTR_SUBSCRIPT_PREVIEW = 0x0100
};

enum eCursorShape
{
    CURSOR_SHAPE_DEFAULT,
    CURSOR_SHAPE_BLOCK,          // blinking block
    CURSOR_SHAPE_BLOCK_STEADY,   // steady block
    CURSOR_SHAPE_UNDERLINE,
    CURSOR_SHAPE_BAR
};

enum eInputType
{
    INPUT_TYPE_NONE,
    INPUT_TYPE_TEXT,
    INPUT_TYPE_CONTROL,
    INPUT_TYPE_SPECIAL,
    INPUT_TYPE_FUNCTION,
    INPUT_TYPE_MOUSE,
    INPUT_TYPE_RESIZE,
    INPUT_TYPE_PASTE_BEGIN,
    INPUT_TYPE_PASTE_END
};

enum eSpecialKey
{
    SPECIAL_KEY_NONE,
    SPECIAL_KEY_ESCAPE,
    SPECIAL_KEY_ENTER,
    SPECIAL_KEY_BACKSPACE,
    SPECIAL_KEY_TAB,
    SPECIAL_KEY_DELETE,
    SPECIAL_KEY_INSERT,
    SPECIAL_KEY_HOME,
    SPECIAL_KEY_END,
    SPECIAL_KEY_PAGE_UP,
    SPECIAL_KEY_PAGE_DOWN,
    SPECIAL_KEY_ARROW_UP,
    SPECIAL_KEY_ARROW_DOWN,
    SPECIAL_KEY_ARROW_LEFT,
    SPECIAL_KEY_ARROW_RIGHT
};

enum eMouseButton
{
    MOUSE_BUTTON_NONE,
    MOUSE_BUTTON_LEFT,
    MOUSE_BUTTON_MIDDLE,
    MOUSE_BUTTON_RIGHT,
    MOUSE_BUTTON_WHEEL_UP,
    MOUSE_BUTTON_WHEEL_DOWN
};

enum eMouseAction
{
    MOUSE_ACTION_NONE,
    MOUSE_ACTION_PRESS,
    MOUSE_ACTION_RELEASE,
    MOUSE_ACTION_DRAG,
    MOUSE_ACTION_WHEEL
};

enum eDialogResult
{
    DIALOG_RESULT_NONE,
    DIALOG_RESULT_OK,
    DIALOG_RESULT_CANCEL,
    DIALOG_RESULT_HELP
};

enum eTriState
{
    TRI_STATE_OFF,
    TRI_STATE_ON,
    TRI_STATE_INHERIT
};

enum eThemeRole
{
    THEME_ROLE_EDITOR,
    THEME_ROLE_EDITOR_STATUS,
    THEME_ROLE_EDITOR_BLOCK,
    THEME_ROLE_MENU,
    THEME_ROLE_MENU_ACTIVE,
    THEME_ROLE_MENU_ACCEL,
    THEME_ROLE_DIALOG,
    THEME_ROLE_DIALOG_TITLE,
    THEME_ROLE_DIALOG_FOCUS,
    THEME_ROLE_BUTTON,
    THEME_ROLE_BUTTON_FOCUS,
    THEME_ROLE_FIELD,
    THEME_ROLE_FIELD_FOCUS,
    THEME_ROLE_LIST,
    THEME_ROLE_LIST_SELECTED,
    THEME_ROLE_HELP,
    THEME_ROLE_WARNING,
    THEME_ROLE_COUNT
};

struct sStyle
{
    sColor fg;
    sColor bg;
    uint32_t attrs;
};

// Color constructors (defined in tuidefs.cpp).
sColor MakeRgb(uint8_t r, uint8_t g, uint8_t b);
sColor MakeDefaultColor(void);
sColor MakePaletteColor(eColor color);

struct sCell
{
    std::string textUtf8;
    sStyle style;
    int width;
    bool wideTail;
};

struct sCellRun
{
    int row;
    int col;
    std::vector<sCell> cells;
};

struct sRect
{
    int row;
    int col;
    int rows;
    int cols;
};

struct sTerminalSize
{
    int rows;
    int cols;
};

struct sInputEvent
{
    eInputType type;
    eSpecialKey special;
    std::vector<uint8_t> rawBytes;
    std::string textUtf8;
    char32_t codepoint;
    int controlCode;
    int functionKey;
    bool ctrl;
    bool alt;
    bool shift;
    int resizeRows;
    int resizeCols;
    int mouseRow;
    int mouseCol;
    eMouseButton mouseButton;
    eMouseAction mouseAction;
    int mouseWheel;
};

struct sTerminalCapabilities
{
    bool color16;
    bool color256;
    bool trueColor;
    bool bold;
    bool dim;
    bool italic;
    bool underline;
    bool strikethrough;
    bool inverse;
    bool blink;
    bool unicode;
    bool mouse;
    bool resizeEvents;
};

}

#endif
