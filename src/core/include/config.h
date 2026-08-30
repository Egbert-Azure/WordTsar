#ifndef CONFIG_H
#define CONFIG_H

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

#include <sys/types.h>

// yeah, my own assertion. I don't want to stop running, I just want to report
#ifndef NDEBUG
    #include <cassert>
    #include <iostream>
        #define MY_ASSERT(condition)   \
        {  \
            if(!(condition))  \
            {  \
                std::cerr << "ASSERTION! " << __FILE__ << "  " << __FUNCTION__  << "  line: " << __LINE__  << " COndition: " << #condition << std::endl ;  \
                abort() ; \
            } \
        }
#else
#define MY_ASSERT(condition) { }
#endif

#ifdef _WIN64
typedef long long ssize_t;
#else
typedef long ssize_t;
#endif


///< @TODO SEE TICKET #62 
#define USE_UNICODE_WORDBREAK 1                     ///< uncomment to use the UNICODE word break algotith instead of using spaces and tabs

// #define LAYOUT_TIMER   1                        ///< uncomment to time layout engine
// #define DETAIL_LAYOUT_TIMER 1

#define NOT_SET -1                              ///< used to indicate a value is not set

// typedefs to help me stay consistant
using POSITION_T = ssize_t ;                    ///< text buffer indexing
using LINE_T = ssize_t ;                        ///< line indexing
using PARAGRAPH_T = ssize_t ;                   ///< paragraph indexing
using PAGE_T = ssize_t ;                        ///< page indexing
using COORD_T = double ;                      ///< coordinate type
using CHAR_T = char32_t ;                     ///< really bad way to store UTF8, which can be anywhere from 1 to 4 bytes in size (used for temp storage)


// accessors for typedefs
#define CAST_POSITION_SIZE  static_cast<POSITION_T>

constexpr COORD_T COORD_EPSILON = 0.5 ;            ///< half a twip, below visible resolution

constexpr bool CoordsEqual(COORD_T left, COORD_T right)
{
    COORD_T diff = left - right ;
    if(diff < 0)
    {
        diff = -diff ;
    }
    return diff < COORD_EPSILON ;
}



constexpr double ONEINCH_INMMX10 = 254.0 ;
constexpr double TWIPSPERINCH = 1440.0 ;   // 1439.990929191 ;
constexpr double POINTSPERINCH = 1.0 / 72.0 ;
constexpr double TWIPSPERMM = TWIPSPERINCH / 25.4 ;
constexpr double TWIPSPERCM = TWIPSPERMM * 10;
constexpr double POINTSTOTWIPS = TWIPSPERINCH / 72.0 ; ///< Font height in twips is 20 * pointsize = twips
constexpr double TWIPSTOPOINTS = 20 ;

#if defined(Q_OS_MACOS) || (defined(WORDTSAR_TUI_BUILD) && defined(__APPLE__))
constexpr double FONTSCALE = (TWIPSPERINCH / 72.0)  ;    ///< fontscale based on 72 DPI
#else
constexpr double FONTSCALE = (TWIPSPERINCH / 96.0)  ;    ///< fontscale based on 96 DPI
#endif

constexpr CHAR_T REPLACE_CHAR = 0 ;                 ///< dummy character in the buffer for block-begin marker
constexpr CHAR_T SAVE_CHAR = 1 ;                    ///< dummy character in the buffer for ^K0..^K9 save bookmarks
constexpr CHAR_T HARD_RETURN = 13 ;
constexpr CHAR_T SPACE = 32 ;
constexpr CHAR_T MARKER_CHAR = 127 ;
// constexpr CHAR_T NODELETE_MARKER_CHAR = 15 ;     ///< this overrides enum eModifiers STYLE_CTRL_O

constexpr COORD_T DEFAULT_CARET_WIDTH  = 30 ;    ///< default insert-mode caret width in twips
constexpr COORD_T DEFAULT_CARET_HEIGHT = 300 ;   ///< default caret height in twips (fallback)

constexpr int MAX_HEADER_FOOTER = 5 ;           ///< the maximum number of lines in a header or footer

#define CHAR_RETURN  "↵"                    ///< the charcater to display for hard returns CR-symbol pilcrow'
#define CHAR_EOF     "‡"                    ///< the charcater to display for end of file

#define OVERWRITE_MODE  false
#define INSERT_MODE     true

#define MAXWSCOLORS 16                      ///< the number of colors the original Wordstar knows about.

constexpr int MAX_UNDO_STEPS = 100;         ///< maximum undo levels (matches Word/LibreOffice default)

#define UNUSED_ARGUMENT(x) (void)x          // also used for unused locals

// Timer intervals (in milliseconds)
constexpr int STATUS_UPDATE_INTERVAL_MS = 200;      ///< Status bar update frequency (5 times per second)
constexpr int WORD_COUNT_INTERVAL_MS = 5000;        ///< Word count update frequency (every 5 seconds)
constexpr int AUTO_SAVE_INTERVAL_MS = 60000;        ///< Auto-save backup frequency (every 1 minute)


///////////////////////////////////////////////////////////////////////
/// @enum eType
///
/// @note Used to match control chars with proper type
///
///////////////////////////////////////////////////////////////////////
enum eType
{
    TYPE_FORMAT,
    TYPE_TAB,
    TYPE_FONT,
    TYPE_COLOR,
    TYPE_INDEX,
    TYPE_FOOTNOTE,
    TYPE_ENDNOTE,
    TYPE_SAVED_POSITION,
    TYPE_VARIABLE,
};


///////////////////////////////////////////////////////////////////////
/// @enum eVariableType
///
/// @brief
/// Types of document variables that expand to dynamic values at
/// layout/print time. Stored in the document as single MARKER_CHAR
/// bytes with metadata identifying the variable type.
///
/// In body text, variables are entered as &X& (e.g., &@& for date).
/// In headers/footers, # is shorthand for &#& (page number).
///
///////////////////////////////////////////////////////////////////////
enum eVariableType
{
    VAR_DATE,           // &@& - current date
    VAR_TIME,           // &!& - current time
    VAR_PAGE_NUMBER,    // &#& - current page number
    VAR_LINE_NUMBER,    // &_& - current line number
    VAR_FILENAME,       // &*& - name of current file
    VAR_DRIVE,          // &:& - current drive letter (/ on Unix)
    VAR_DIRECTORY,      // &.& - current directory
    VAR_FULLPATH,       // &\& - full path and filename
    VAR_WORD_COUNT,     // &?& - document word count (non-standard)
};


///////////////////////////////////////////////////////////////////////
/// @enum eModifiers
///
/// @note Basic original wordstar styles (order is important)
///
///////////////////////////////////////////////////////////////////////
enum eModifiers
{
    STYLE_NOT_USED1,
    STYLE_FONT1,                        // done font change
    STYLE_BOLD,                         // done
    STYLE_VARIABLE,                     // variable expansion (&@&, &#&, etc.) -- reuses unused WS byte 3
    STYLE_NOT_USED3,                    // not used - double strike toggle
    STYLE_NOT_USED4,                    // not used - custom print control
    STYLE_PHANTOM_SPACE,                // not used
    STYLE_PHANTOM_BACKSPACE,            // not used
    STYLE_BACKSPACE,                    // not used
    STYLE_TAB,                          // done
    STYLE_LINEFEED,                     // done
    STYLE_INDEX,                        // PARTIAL
    STYLE_FORMFEED,                     // done - inserts .pa
    STYLE_RETURN,                       // done
    STYLE_NOBREAK_SPACE,                // done
    STYLE_CTRL_O,                       // used as non deletable marker char
    STYLE_INTERNAL_COLOR,               // reserved by wordstar, used by WordTsar for internal colors  STYLE_RESERVED,
    STYLE_NOT_USED7,                    // not used - custom print control
    STYLE_NOT_USED8,                    // not used - custom print control
    STYLE_UNDERLINE,                    // done
    STYLE_SUPERSCRIPT,                  // done
    STYLE_RESERVED1,
    STYLE_SUBSCRIPT,                    // done
    STYLE_NOT_USED9,                    // not used - custome print control
    STYLE_STRIKETHROUGH,                // done
    STYLE_ITALICS,                      // done
    STYLE_EOF,                          // done
    STYLE_EXTSTART,                     // done - next character is extended character if 2nd byte following is STYLE_EXTEND
    STYLE_EXTEND,                       // done - preceding character is extended character if 2nd byte preceding is STYLE_EXTSTART
    STYLE_SEQUENCE,                     // PARTIAL - read ok, not written
    STYLE_FOOTNOTE,                     // done (WS uses this for - soft-hypen for where wordwrap can occur)
    STYLE_ENDNOTE,                      // done (WS uses this for - soft-hypen for where wordwrap has occurred)
    STYLE_END_OF_STYLES
} ;



///////////////////////////////////////////////////////////////////////
/// @enum eTabTyoes
///
/// @note What type of tab are we
///
///////////////////////////////////////////////////////////////////////
enum eTabTypes
{
        TAB_TAB = ' ',
        TAB_DECIMAL = '#',
        TAB_CENTER = '!',
        TAB_RIGHT = '[',
        TAB_RIGHT1 = ']',                     // not in the docs, but a MicroPro document used it (WS7.0d print.tst)
        TAB_SOFT = 0xA0,                     // soft space tab

        TAB_BAD = 0xFF                     // used to indicate a bad tab
};



///////////////////////////////////////////////////////////////////////
/// @enum eJustification
///
/// @note what justification are we
///
///////////////////////////////////////////////////////////////////////
enum eJustification
{
    JUST_LEFT,
    JUST_CENTER,
    JUST_RIGHT,
    JUST_JUST
};



///////////////////////////////////////////////////////////////////////
/// @enum eMeasurement
///
/// @note what measurement are we displaying
///
///////////////////////////////////////////////////////////////////////
enum eMeasurement
{
    MSR_TWIPS,
    MSR_POINTS,
    MSR_MILLIMETERS,
    MSR_CENTIMETERS,
    MSR_INCHES
};


///////////////////////////////////////////////////////////////////////
/// @enum eLayouts
///
/// @note what render layout engine are we using
///
///////////////////////////////////////////////////////////////////////
enum eLayouts
{
    LAYOUT_STANDARD,
    LAYOUT_PRINT,
};




///////////////////////////////////////////////////////////////////////
/// @enum eHelpDisplay
///
/// @note What are we displaying for help
///
///////////////////////////////////////////////////////////////////////
enum eHelpDisplay
{
    HELP_UNKNOWN,
    HELP_NONE,
    HELP_MAIN,
    HELP_CTRLJ,
    HELP_CTRLK,
    HELP_CTRLP,
    HELP_CTRLQ,
    HELP_CTRLO
} ;


///////////////////////////////////////////////////////////////////////
/// @enum eInputMode
///
/// @note Keyboard input mode (WordStar, WordPerfect, etc.)
///       Determines which command set the input handler uses.
///       Currently only WordStar is supported.
///
///////////////////////////////////////////////////////////////////////
enum eInputMode
{
    INPUT_WORDSTAR,         ///< WordStar command mode (default)
    INPUT_MODERN            ///< Modern/CUA command mode (Ctrl+C/V/X/Z)
    // Future modes (not yet implemented):
    // INPUT_WORDPERFECT,   ///< WordPerfect command mode
    // INPUT_MSWORD,        ///< Microsoft Word command mode
    // INPUT_EMACS,         ///< Emacs key bindings
    // INPUT_VIM,           ///< Vim key bindings
};


///////////////////////////////////////////////////////////////////////
/// @enum eShowControl
///
/// @note Levels of show control
///
///////////////////////////////////////////////////////////////////////
enum eShowControl
{
    SHOW_ALL,
    SHOW_DOT,
    SHOW_NONE
};



///////////////////////////////////////////////////////////////////////
/// @enum eHeaderFooter
///
/// @note Header/footer types
///
///////////////////////////////////////////////////////////////////////
enum eHeaderFooter
{
    HEADER_BOTH,
    HEADER_EVEN,
    HEADER_ODD,
    FOOTER_BOTH,
    FOOTER_EVEN,
    FOOTER_ODD
} ;



///////////////////////////////////////////////////////////////////////
/// @enum eHeaderFooter
///
/// @note
///
///////////////////////////////////////////////////////////////////////
enum eNoteSymbol
{
    NOTE_SYMBOLS,
    NOTE_UPPER,
    NOTE_LOWER,
    NOTE_NUMBER
};


///////////////////////////////////////////////////////////////////////
/// @enum eCodePage
///
/// @note code page conversion selector. Set by user or via WS 7 file format
///
///////////////////////////////////////////////////////////////////////
enum eCodePage
{
    CP437,                  // English, German, Swedish
    CP737,                  // Greek, English
    CP850,                  // DOS Latin-1 (Western European)
    CP1252                  // Windows Latin-1 (Western European)
};


#endif
