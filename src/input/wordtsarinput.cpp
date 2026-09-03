//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
// Copyright (C) 2026 Egbert H. Schroeer
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

/**
 * @class cWordStarInput
 *
 * @brief WordStar keyboard input handler implementation.
 *
 * Implements the cWordStarInput class, which translates WordStar-style
 * keyboard commands into editor actions. This class is platform-independent
 * (no Qt dependency) and is shared by both the GUI and TUI editors.
 *
 * @section wsinput_single Single Control Keys
 * Direct action keys that trigger immediately on press:
 * - Navigation: Ctrl-S (left), Ctrl-D (right), Ctrl-E (up), Ctrl-X (down),
 *   Ctrl-A (word left), Ctrl-F (word right)
 * - Editing: Ctrl-G (delete char), Ctrl-T (delete word), Ctrl-Y (delete line),
 *   Ctrl-N (insert line break), Ctrl-V (toggle insert/overwrite)
 * - Misc: Ctrl-L (find again), Ctrl-U (undo), Ctrl-R (page up), Ctrl-C (page down)
 *
 * @section wsinput_chords Two-Key Chord Sequences
 * Chord prefixes set a modal state that routes the next keystroke:
 * - Ctrl-K (block/file): ^KB (begin block), ^KK (end block), ^KC (copy block),
 *   ^KV (move block), ^KY (delete block), ^KR (open file), ^KS (save),
 *   ^KD (save and quit), ^KQ (quit without saving)
 * - Ctrl-Q (quick movement): ^QS (line start), ^QD (line end), ^QR (doc start),
 *   ^QC (doc end), ^QF (find), ^QA (find/replace), ^QB (goto block start),
 *   ^QK (goto block end)
 * - Ctrl-O (onscreen formatting): ^OL (left margin), ^OR (right margin),
 *   ^OC (center line), ^OJ (justify toggle), ^OP (page layout)
 * - Ctrl-P (print/style): ^PB (bold), ^PY (italic), ^PS (underline),
 *   ^PT (font), ^PX (strikethrough), ^PV (subscript), ^PP (superscript)
 * - Ctrl-J (help): ^JH (help display toggle), ^JD (help panel navigation)
 *
 * @section wsinput_state Modal State Management
 * Each chord prefix (Ctrl-K, Ctrl-Q, etc.) sets a boolean flag
 * (mControlKMode, mControlQMode, etc.) that causes the next keystroke
 * to be dispatched to the corresponding handler method (OnControlKChar,
 * OnControlQChar, etc.). The flag is reset after the second key is processed.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cWordStarInput WordStar input handler class
 * @see cEditorBase Editor base class receiving commands
 */

#include <filesystem>

#include "wordtsarinput.h"
#include "src/core/editor/editorbase.h"
#include "src/core/include/utils.h"

// #include "src/files/file.h"  // OLD - temporarily removed (depends on old editor)

namespace {

/////////////////////////////////////////////////////////////////////////////
/// Per-command help text for WordStar 7's real F1 key: "press F1, then press
/// the command you want help with" (manual, "Onscreen Help"). Text is
/// paraphrased from the WS4 manual's per-key reference (Appendix G,
/// "Commands from the Edit Menu") -- the content is the same across both
/// versions for these basic cursor/editing keys; only the trigger key
/// (^J vs. F1) and the ^M/Macro Menu content differ between them. F1's
/// lookup is independent of OnControlMChar's own switch, so every single
/// control key can have an entry here regardless of what a chord's own
/// sub-letters mean -- see KEY_MAPPING.md's "Single Control Keys" table for
/// how these map onto what WordTsar actually implements today.
/////////////////////////////////////////////////////////////////////////////
const char *LookupControlKeyHelp(char lower)
{
    switch(lower)
    {
        case 'a' :
            return "^A - Move cursor left one word\n\n"
                   "Moves the cursor left to the first character of the previous word." ;
        case 'b' :
            return "^B - Reformat paragraph (not yet implemented)\n\n"
                   "In WordStar 4.0: aligns the current paragraph between the current "
                   "margins (including any temporary margin set with ^OG), stopping at "
                   "the first hard return. If hyphen help is on, ^B continues aligning "
                   "without hyphenating the current word." ;
        case 'c' :
            return "^C - Move cursor down one page\n\n"
                   "Scrolls down one screen (same as Page Down)." ;
        case 'd' :
            return "^D - Move cursor right\n\n"
                   "Moves the cursor one character to the right." ;
        case 'e' :
            return "^E - Move cursor up one line\n\n"
                   "Moves the cursor up one line, keeping its column until it reaches "
                   "a shorter line." ;
        case 'f' :
            return "^F - Move cursor right one word\n\n"
                   "Moves the cursor right to the first character of the next word." ;
        case 'g' :
            return "^G - Delete character at cursor\n\n"
                   "Deletes the character at the cursor. At the end of a line, deletes "
                   "the line break instead, joining the next line onto this one." ;
        case 'h' :
            return "^H - Delete character before cursor (Backspace)\n\n"
                   "Deletes the character to the left of the cursor. At the start of a "
                   "line, deletes the previous line break, joining this line onto the one above." ;
        case 'i' :
            return "^I - Insert tab\n\n"
                   "Moves the cursor to the next tab stop. With Insert on, inserts space "
                   "and shifts existing text along; with Insert off, moves past existing "
                   "text without shifting it. Does nothing if the line has no more tabs." ;
        case 'k' :
            return "^K - Enter Block and File chord\n\n"
                   "Opens the Block and Save menu -- block marking, copy/move/delete, "
                   "save/save as/quit, and file operations. Press a second key to choose." ;
        case 'l' :
            return "^L - Find next match (Find Again)\n\n"
                   "Repeats the last Find or Find & Replace." ;
        case 'm' :
            return "^M - Enter Macro Menu chord\n\n"
                   "Opens the Macro menu -- date/time/filename/path insertion, and "
                   "keyboard macro record/play/edit (macro playback not yet implemented). "
                   "Press a second key to choose." ;
        case 'n' :
            return "^N - Insert line break\n\n"
                   "Inserts a blank line without moving the cursor, pushing any text to "
                   "the right of the cursor down onto it." ;
        case 'o' :
            return "^O - Enter Onscreen Format chord\n\n"
                   "Opens the Onscreen Format menu -- margins, tabs, alignment, and "
                   "display settings. Press a second key to choose." ;
        case 'p' :
            return "^P - Enter Print Controls chord\n\n"
                   "Opens the Print Controls menu -- bold/underline/italic and other "
                   "print-time styles. Press a second key to choose." ;
        case 'q' :
            return "^Q - Enter Quick Functions chord\n\n"
                   "Opens the Quick menu -- fast cursor jumps, find/replace, spell "
                   "check, and deletion shortcuts. Press a second key to choose." ;
        case 'r' :
            return "^R - Move cursor up one page\n\n"
                   "Scrolls up one screen (same as Page Up)." ;
        case 's' :
            return "^S - Move cursor left\n\n"
                   "Moves the cursor one character to the left." ;
        case 't' :
            return "^T - Delete word to the right\n\n"
                   "Deletes from the cursor to the next space or punctuation mark. At "
                   "the end of a line, deletes the line break instead." ;
        case 'u' :
            return "^U - Undo last action\n\n"
                   "Restores (unerases) the most recently deleted text, except single "
                   "characters deleted with ^H, ^G, or Delete. Also interrupts a command in progress." ;
        case 'v' :
            return "^V - Toggle insert/overwrite mode\n\n"
                   "Switches between Insert and Overwrite. The status line shows which is active." ;
        case 'w' :
            return "^W - Scroll up one line\n\n"
                   "Scrolls the view up by one line, so the text on screen shifts down; "
                   "the cursor moves with the text." ;
        case 'x' :
            return "^X - Move cursor down one line\n\n"
                   "Moves the cursor down one line, keeping its column until it reaches "
                   "a shorter line." ;
        case 'y' :
            return "^Y - Delete line\n\n"
                   "Deletes the current line, including its line break." ;
        case 'z' :
            return "^Z - Scroll down one line\n\n"
                   "Scrolls the view down by one line, so the text on screen shifts up; "
                   "the cursor moves with the text." ;
        default  : return nullptr ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// Per-command help text for the ^K (Block and File) chord's sub-letters,
/// reached via F1, K, <letter>. Descriptions are paraphrased from the WS4
/// manual's Appendix G entries for the same commands, matched against what
/// this letter actually does in WordTsar today (see KEY_MAPPING.md's "^K
/// Chord" table) -- covers every letter OnControlKChar implements, not just
/// a subset.
/////////////////////////////////////////////////////////////////////////////
const char *LookupKChordHelp(char lower)
{
    switch(lower)
    {
        case 'b' :
            return "^KB - Mark beginning of block\n\n"
                   "Marks the current cursor position as the start of a block, shown "
                   "as <B> in the text. Used with ^KK to select a block for copying, "
                   "moving, or deleting." ;
        case 'k' :
            return "^KK - Mark end of block\n\n"
                   "Marks the current cursor position as the end of a block, shown as "
                   "<K> in the text, completing the selection started with ^KB." ;
        case 'c' :
            return "^KC - Copy block\n\n"
                   "Copies the marked block to the cursor position. The original block "
                   "is unchanged and remains marked." ;
        case 'v' :
            return "^KV - Move block\n\n"
                   "Moves the marked block to the cursor position, removing it from "
                   "its original location." ;
        case 'y' :
            return "^KY - Delete block\n\n"
                   "Deletes the marked block of text." ;
        case '\"' :
            return "^K\" - Convert block to uppercase\n\n"
                   "Converts every character in the marked block to uppercase." ;
        case '\'' :
            return "^K' - Convert block to lowercase\n\n"
                   "Converts every character in the marked block to lowercase." ;
        case '.' :
            return "^K. - Convert block to title case\n\n"
                   "Capitalizes the first letter of each word in the marked block, "
                   "converting the rest to lowercase." ;
        case '<' :
            return "^K< - Unmark block\n\n"
                   "Clears the current block's beginning and end markers without "
                   "changing the text itself." ;
        case 'h' :
            return "^KH - Toggle block display\n\n"
                   "Turns the highlighting of a marked block, and the display of "
                   "place markers, on or off. Block operations can't be performed "
                   "while block display is off." ;
        case 'u' :
            return "^KU - Mark previous block\n\n"
                   "Swaps the current marked block with the previously marked one, "
                   "letting you flip back and forth between two selections." ;
        case '[' :
            return "^K[ - Paste from clipboard\n\n"
                   "Inserts the contents of the system clipboard at the cursor position." ;
        case ']' :
            return "^K] - Copy to clipboard\n\n"
                   "Copies the marked block to the system clipboard." ;
        case '?' :
            return "^K? - Word/character count\n\n"
                   "Displays a word and character count for the marked block, or the "
                   "whole document if no block is marked." ;
        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' :
        case '8' :
        case '9' :
            return "^K0-^K9 - Set a place marker\n\n"
                   "Sets a nonprinting marker (0-9) at the current cursor position. "
                   "Use ^Q0-^Q9 to jump back to it. Place markers don't move with "
                   "blocks and aren't saved once you close the file." ;
        case 'r' :
            return "^KR - Open a file\n\n"
                   "Prompts for a file and loads it, replacing the file currently "
                   "open for editing." ;
        case 's' :
            return "^KS - Save file\n\n"
                   "Saves the current file without leaving the editor." ;
        case 't' :
            return "^KT - Save as\n\n"
                   "Prompts for a new filename and saves the current file under it." ;
        case 'd' :
            return "^KD - Save and clear\n\n"
                   "Saves the current file, then clears the editor back to a new, "
                   "unnamed document." ;
        case 'x' :
            return "^KX - Save and exit\n\n"
                   "Saves the current file and quits WordTsar." ;
        case 'q' :
            return "^KQ - Abandon file\n\n"
                   "Closes the current file without saving, discarding any changes "
                   "made this session." ;
        case 'p' :
            return "^KP - Print preview\n\n"
                   "Opens print preview for the current document." ;
        default  : return nullptr ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// Per-command help text for the ^Q (Quick Functions) chord's sub-letters,
/// reached via F1, Q, <letter>. Same sourcing approach as LookupKChordHelp --
/// see KEY_MAPPING.md's "^Q Chord" table for what's actually implemented.
/////////////////////////////////////////////////////////////////////////////
const char *LookupQChordHelp(char lower)
{
    switch(lower)
    {
        case 's' :
            return "^QS - Move to start of line\n\n"
                   "Moves the cursor to column 1 of the current line." ;
        case 'd' :
            return "^QD - Move to end of line\n\n"
                   "Moves the cursor to the right end of the current line." ;
        case 'r' :
            return "^QR - Move to start of document\n\n"
                   "Moves the cursor to the very beginning of the document." ;
        case 'c' :
            return "^QC - Move to end of document\n\n"
                   "Moves the cursor to the very end of the document." ;
        case 'e' :
            return "^QE - Move to top of screen\n\n"
                   "Moves the cursor to the left end of the first line currently on "
                   "screen, without scrolling." ;
        case 'x' :
            return "^QX - Move to bottom of screen\n\n"
                   "Moves the cursor to the right end of the last line currently on "
                   "screen, without scrolling." ;
        case 'b' :
            return "^QB - Go to block start\n\n"
                   "Moves the cursor to the marked block's beginning marker, if one "
                   "is set." ;
        case 'k' :
            return "^QK - Go to block end\n\n"
                   "Moves the cursor to the marked block's ending marker, if one is set." ;
        case 'f' :
            return "^QF - Find\n\n"
                   "Opens the Find dialog to search for a string of text." ;
        case 'a' :
            return "^QA - Find and replace\n\n"
                   "Opens the Find and Replace dialog to search for a string and "
                   "replace it with another." ;
        case 'g' :
            return "^QG - Go to character (forward)\n\n"
                   "Moves the cursor forward to the next occurrence of a character "
                   "you specify." ;
        case 'h' :
            return "^QH - Go to character (backward)\n\n"
                   "Moves the cursor backward to the previous occurrence of a "
                   "character you specify." ;
        case 'i' :
            return "^QI - Go to page\n\n"
                   "Moves the cursor to the start of a page number you specify." ;
        case 'p' :
            return "^QP - Return to previous position\n\n"
                   "Moves the cursor back to where it was before the last command "
                   "that jumped it. Must be used right after that command." ;
        case 'v' :
            return "^QV - Go to last find/replace\n\n"
                   "Moves the cursor to the location of the last Find or Find and "
                   "Replace match." ;
        case '=' :
            return "^Q= - Go to next font tag\n\n"
                   "Moves the cursor to the next in-text font/style tag." ;
        case 'u' :
            return "^QU - Reformat document\n\n"
                   "Forces the entire document to be fully re-paginated and redrawn." ;
        case 'l' :
            return "^QL - Spell check document\n\n"
                   "Checks the spelling of every word in the document." ;
        case 'n' :
            return "^QN - Spell check word\n\n"
                   "Checks the spelling of the word at the cursor." ;
        case 'o' :
            return "^QO - Add word to dictionary\n\n"
                   "Adds a word you specify to the spelling dictionary." ;
        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' :
        case '8' :
        case '9' :
            return "^Q0-^Q9 - Go to a place marker\n\n"
                   "Moves the cursor to the marker (0-9) previously set with ^K0-^K9." ;
        case ' ' :
        case 0x7F :
            return "^Q(Space) - Delete to start of line\n\n"
                   "Deletes from the cursor to the start of the current line." ;
        case 'y' :
            return "^QY - Delete to end of line\n\n"
                   "Deletes from the cursor to the end of the current line, not "
                   "including the line break." ;
        case 't' :
            return "^QT - Delete to character\n\n"
                   "Deletes from the cursor up to and including the next occurrence "
                   "of a character you specify." ;
        default  : return nullptr ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// Per-command help text for the ^O (Onscreen Format) chord's sub-letters,
/// reached via F1, O, <letter>. See KEY_MAPPING.md's "^O Chord" table for
/// what's actually implemented -- most letters not covered here route to
/// the shared Preferences dialog rather than their own real WS4/WS7 screen;
/// that's covered by one shared entry below rather than a guess per letter.
/////////////////////////////////////////////////////////////////////////////
const char *LookupOChordHelp(char lower)
{
    switch(lower)
    {
        case 'd' :
            return "^OD - Toggle control code display\n\n"
                   "Shows or hides in-text formatting codes (bold, underline, and "
                   "similar markers) on screen." ;
        case 'c' :
            return "^OC - Center line\n\n"
                   "Centers the current line between the current margins." ;
        case ']' :
            return "^O] - Right-align line\n\n"
                   "Right-aligns the current line to the right margin." ;
        case '?' :
            return "^O? - Display status\n\n"
                   "Shows the current file, directory, and free disk space." ;
        case 'p' :
            return "^OP - Print preview\n\n"
                   "Opens print preview for the current document." ;
        case 'y' :
            return "^OY - Page layout\n\n"
                   "Opens the page layout dialog (margins, page size, orientation)." ;
        case 't' :
            return "^OT - Toggle page/continuous mode\n\n"
                   "Switches the display between paginated (page-break) and "
                   "continuous scrolling view." ;
        case 'j' :
            return "^OJ - Toggle justification\n\n"
                   "Turns right-margin justification on or off for the current "
                   "document." ;
        case '<' :
            return "^O< - Left-align paragraph\n\n"
                   "Sets the current paragraph's alignment to left-aligned." ;
        case '>' :
            return "^O> - Right-align paragraph\n\n"
                   "Sets the current paragraph's alignment to right-aligned." ;
        case '=' :
            return "^O= - Center paragraph\n\n"
                   "Sets the current paragraph's alignment to centered." ;
        case '+' :
            return "^O+ - Justify paragraph\n\n"
                   "Sets the current paragraph's alignment to fully justified." ;
        case 'l' :
        case 'g' :
        case 'x' :
        case 'i' :
        case 'o' :
        case 'u' :
        case 'f' :
        case 's' :
        case 'v' :
        case 'e' :
        case 'h' :
        case 'a' :
        case 'w' :
        case ' ' :
        case 'b' :
            return "^O<letter> - Opens Preferences\n\n"
                   "This Onscreen Format letter doesn't have its own screen in "
                   "WordTsar; it opens the Preferences dialog instead, where the "
                   "matching setting lives." ;
        default  : return nullptr ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// Per-command help text for the ^P (Print Controls) chord's sub-letters,
/// reached via F1, P, <letter>. See KEY_MAPPING.md's "^P Chord" table.
/////////////////////////////////////////////////////////////////////////////
const char *LookupPChordHelp(char lower)
{
    switch(lower)
    {
        case 'b' :
            return "^PB - Toggle bold\n\n"
                   "Boldfaces text typed from here, until used again." ;
        case 's' :
            return "^PS - Toggle underline\n\n"
                   "Underlines text typed from here, until used again." ;
        case 'y' :
            return "^PY - Toggle italic\n\n"
                   "Italicizes text typed from here, until used again." ;
        case 'v' :
            return "^PV - Toggle subscript\n\n"
                   "Subscripts text typed from here, until used again." ;
        case 't' :
            return "^PT - Toggle superscript\n\n"
                   "Superscripts text typed from here, until used again." ;
        case 'x' :
        case 'k' :
            return "^PX - Toggle strikethrough\n\n"
                   "Strikes through text typed from here, until used again." ;
        case '=' :
        case '+' :
            return "^P= - Select font\n\n"
                   "Opens the font selection dialog." ;
        case '-' :
            return "^P- - Select color\n\n"
                   "Opens the color selection dialog." ;
        default  : return nullptr ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// Per-command help text for the ^M (Macro Menu) chord's sub-letters,
/// reached via F1, M, <letter>. This is WS7-specific content (WS4 has no
/// real ^M chord in this role), paraphrased from the WS7 manual's "Other
/// Value" insert table and its Macros section -- see KEY_MAPPING.md's "^M
/// Chord" table.
/////////////////////////////////////////////////////////////////////////////
const char *LookupMChordHelp(char lower)
{
    switch(lower)
    {
        case '@' :
            return "^M@ - Insert today's date\n\n"
                   "Inserts the current date at the cursor, for example "
                   "\"June 4, 2026\"." ;
        case '!' :
            return "^M! - Insert current time\n\n"
                   "Inserts the current time at the cursor, for example \"9:30 PM\"." ;
        case '*' :
            return "^M* - Insert filename\n\n"
                   "Inserts the name of the current file." ;
        case ':' :
            return "^M: - Insert drive letter\n\n"
                   "Inserts the currently logged drive letter (Windows only)." ;
        case '.' :
            return "^M. - Insert directory\n\n"
                   "Inserts the currently logged directory path." ;
        case '\\' :
            return "^M\\ - Insert path and filename\n\n"
                   "Inserts the full path and filename of the current file." ;
        case '=' :
            return "^M= - Insert last math result (not yet implemented)\n\n"
                   "Would insert the result of the last calculation performed with "
                   "Calculator (^QM) or Block Math (^KM)." ;
        case '#' :
            return "^M# - Insert last math expression (not yet implemented)\n\n"
                   "Would insert the expression last entered in the Calculator (^QM)." ;
        case '$' :
            return "^M$ - Insert last math result as dollar amount (not yet "
                   "implemented)\n\n"
                   "Would insert the last Calculator/Block Math result formatted as "
                   "currency, for example \"$23.10\"." ;
        case 'p' :
            return "^MP - Play macro (not yet implemented)\n\n"
                   "Would run a previously recorded keyboard macro." ;
        case 'r' :
            return "^MR - Record macro (not yet implemented)\n\n"
                   "Would record a series of keystrokes to a named macro file for "
                   "later playback." ;
        case 'd' :
            return "^MD - Edit/Create macro (not yet implemented)\n\n"
                   "Would open a macro's own text for direct editing, using "
                   "WordStar's macro language." ;
        case 's' :
            return "^MS - Single-step macro (not yet implemented)\n\n"
                   "Would play a macro back one command at a time, to help locate "
                   "errors in it." ;
        case 'e' :
            return "^ME - Rename macro (not yet implemented)\n\n"
                   "Would rename a macro, or reassign its hot key." ;
        case 'o' :
            return "^MO - Copy macro (not yet implemented)\n\n"
                   "Would copy an existing macro under a new name." ;
        case 'y' :
            return "^MY - Delete macro (not yet implemented)\n\n"
                   "Would delete a recorded macro." ;
        default  : return nullptr ;
    }
}

} // namespace

/// @ingroup Keyboard
/// @{

cWordStarInput::cWordStarInput(cEditorBase *editor)
{
    mEditor = editor ;
    
    mControlMMode = false ;
    mControlKMode = false ;
    mControlOMode = false ;
    mControlPMode = false ;
    mControlQMode = false ;
    mWaitingForHelpTarget = false ;
    mWaitingForHelpChordTarget = false ;
    mHelpChordPrefix = 0 ;

    mOldHelpStatus = HELP_NONE ;
}


cWordStarInput::~cWordStarInput(void)
{

}


bool cWordStarInput::CheckControlMode(void)
{
    return mControlMMode || mControlKMode || mControlOMode || mControlPMode || mControlQMode ;
}



bool cWordStarInput::HandleKey(char ch, bool shift, bool alt)
{
    UNUSED_ARGUMENT(shift) ;

    bool handled = false ;

    // F1, <chord-prefix> (e.g. "F1, K") is waiting for the sub-letter that
    // picks which chord command to describe (e.g. "F1, K, B" -> ^KB's help).
    // Checked ahead of mWaitingForHelpTarget since it's the deeper of the
    // two states.
    if (mWaitingForHelpChordTarget == true)
    {
        mWaitingForHelpChordTarget = false ;
        OnHelpChordTargetChar(ch) ;
        return true ;
    }

    // F1 (WordStar 7's real help key -- see OnHelpTargetChar) is waiting for
    // the command it should describe. Takes priority over everything else,
    // including chord entry, so "F1, K" describes ^K rather than entering it.
    if (mWaitingForHelpTarget == true)
    {
        mWaitingForHelpTarget = false ;
        OnHelpTargetChar(ch) ;
        return true ;
    }

    // Alt+letter in WordStar mode: enter chord mode as alternative to Ctrl+letter
    // This allows TUI users to use Alt+K/Q/O/P as equivalent to Ctrl+K/Q/O/P
    if (alt)
    {
        char lower = tolower(ch) ;

        if (lower == 'k')
        {
            mControlKMode = true ;
            mOldHelpStatus = mEditor->mHelpDisplay ;
            mEditor->mHelpDisplay = HELP_CTRLK ;
            return true ;
        }
        if (lower == 'q')
        {
            mControlQMode = true ;
            mOldHelpStatus = mEditor->mHelpDisplay ;
            mEditor->mHelpDisplay = HELP_CTRLQ ;
            return true ;
        }
        if (lower == 'o')
        {
            mControlOMode = true ;
            mOldHelpStatus = mEditor->mHelpDisplay ;
            mEditor->mHelpDisplay = HELP_CTRLO ;
            return true ;
        }
        if (lower == 'p')
        {
            mControlPMode = true ;
            mOldHelpStatus = mEditor->mHelpDisplay ;
            mEditor->mHelpDisplay = HELP_CTRLP ;
            return true ;
        }
        if (lower == 'm')
        {
            mControlMMode = true ;
            mOldHelpStatus = mEditor->mHelpDisplay ;
            mEditor->mHelpDisplay = HELP_CTRLM ;
            return true ;
        }

        // Alt+U: Redo (replaces the old Ctrl+Alt+U keyPressEvent hack)
        if (ch == CTRL_U)
        {
            mEditor->Redo() ;
            return true ;
        }

        // Other Alt combinations not handled in WordStar mode
        return false ;
    }

    if(ch == 27)                    // escape key
    {
        // mOldHelpStatus only holds a meaningful value once a real ^K/^Q/^O/
        // ^P/^M chord has actually been entered (that's where it's captured).
        // Restoring it unconditionally would stomp a perfectly good display
        // with its stale HELP_NONE construction default the first time
        // Escape is pressed in a session that never entered one.
        bool hadActiveChord = mControlMMode || mControlKMode || mControlOMode
                            || mControlPMode || mControlQMode ;

        mControlMMode = false ;
        mControlKMode = false ;
        mControlOMode = false ;
        mControlPMode = false ;
        mControlQMode = false ;
        mWaitingForHelpTarget = false ;
        mWaitingForHelpChordTarget = false ;
        if(hadActiveChord == true)
        {
            mEditor->mHelpDisplay = mOldHelpStatus ;
        }
        handled = true ;
    }

    // handle special modes
    if(mControlMMode == true)
    {
        mEditor->mHelpDisplay = mOldHelpStatus ;
        OnControlMChar(ch) ;
        handled = true ;
    }
    else if(mControlKMode == true)
    {
        mEditor->mHelpDisplay = mOldHelpStatus ;
        OnControlKChar(ch) ;
        handled = true ;
    }
    else if(mControlQMode == true)
    {
        mEditor->mHelpDisplay = mOldHelpStatus ;
        OnControlQChar(ch) ;
        handled = true ;
    }
    else if(mControlPMode == true)
    {
        mEditor->mHelpDisplay = mOldHelpStatus ;
        OnControlPChar(ch) ;
        handled = true ;
    }
    else if(mControlOMode == true)
    {
        mEditor->mHelpDisplay = mOldHelpStatus ;
        OnControlOChar(ch) ;
        handled = true ;
    }

    if(handled == false)
    {
        bool upordown = mEditor->mLastKeyUpOrDown ;                     // has an up or down key been pressed
        mEditor->mLastKeyUpOrDown = false ;

        switch(ch)
        {
            // deal with menus
            case CTRL_M :
                mControlMMode = true ;
                mOldHelpStatus = mEditor->mHelpDisplay ;
                mEditor->mHelpDisplay = HELP_CTRLM ;
                handled = true ;
                break ;

            case CTRL_K :
                mControlKMode = true ;
                mOldHelpStatus = mEditor->mHelpDisplay ;
                mEditor->mHelpDisplay = HELP_CTRLK ;
                handled = true ;
                break ;
                
            case CTRL_Q :
                mControlQMode = true ;
                mOldHelpStatus = mEditor->mHelpDisplay ;
                mEditor->mHelpDisplay = HELP_CTRLQ ;
                handled = true ;
                break ;
                
            case CTRL_P :
                mControlPMode = true ;
                mOldHelpStatus = mEditor->mHelpDisplay ;
                mEditor->mHelpDisplay = HELP_CTRLP ;
                handled = true ;
                break ;
                
            case CTRL_O :
                mControlOMode = true ;
                mOldHelpStatus = mEditor->mHelpDisplay ;
                mEditor->mHelpDisplay = HELP_CTRLO ;
                handled = true ;
                break ;
                
            // deal with regular control keys
            case CTRL_E :
                // API CHANGE: Old used mLayout->ResetVerticalCaretMove()
                // New system uses mCaretStickyX = 0 in base class
                if(upordown != true)
                {
                    // Reset sticky X when changing from horizontal to vertical movement
                    // This is handled automatically in MoveCaretLine when upordown is false
                }
                mEditor->mLastKeyUpOrDown = true ;
                mEditor->MoveCaretLine(-1) ;  // API CHANGE: MoveUp() becomes MoveCaretLine(-1)
                handled = true ;
                break ;

            case CTRL_X :
                if(upordown != true)
                {
                    // Reset sticky X - handled automatically in MoveCaretLine
                }
                mEditor->mLastKeyUpOrDown = true ;
                mEditor->MoveCaretLine(1) ;  // API CHANGE: MoveDown() becomes MoveCaretLine(1)
                handled = true ;
                break ;

            case CTRL_S :
                mEditor->MoveCaretLeft() ;  // API CHANGE: MoveLeft() becomes MoveCaretLeft()
                handled = true ;
                break ;

            case CTRL_D :
                mEditor->MoveCaretRight() ;  // API CHANGE: MoveRight() becomes MoveCaretRight()
                handled = true ;
                break ;

            case CTRL_R :
                mEditor->MoveCaretPage(-1) ;  // API CHANGE: PageUp() becomes MoveCaretPage(-1)
                handled = true ;
                break ;

            case CTRL_C :
                mEditor->MoveCaretPage(1) ;  // API CHANGE: PageDown() becomes MoveCaretPage(1)
                handled = true ;
                break ;

            case CTRL_A :
                mEditor->MoveCaretWordLeft() ;  // API CHANGE: WordLeft() becomes MoveCaretWordLeft()
                handled = true ;
                break ;

            case CTRL_F :
                mEditor->MoveCaretWordRight() ;  // API CHANGE: WordRight() becomes MoveCaretWordRight()
                handled = true ;
                break ;

            case CTRL_W :
                mEditor->ScrollUp() ;
                mEditor->MoveCaretLine(-1) ;  // Move caret with viewport (matches old editor behavior)
                handled = true ;
                break ;

            case CTRL_Z :
                mEditor->ScrollDown() ;
                mEditor->MoveCaretLine(1) ;  // Move caret with viewport (matches old editor behavior)
                handled = true ;
                break ;
                
            case CTRL_V :
                mEditor->mInsertMode = !mEditor->mInsertMode ;
                mEditor->CalculateCaretPosition();  // Update caret width for insert/overwrite mode
                handled = true ;
                break ;
            
            case CTRL_G :
                mEditor->DeleteKey() ;  // Delete character AT cursor (DEL key behavior)
                handled = true ;
                break ;

            case CTRL_H :
                // Backspace: delete character BEFORE cursor
                mEditor->DeleteChar() ;  // DeleteChar already deletes at (pos-1)
                handled = true ;
                break ;

            case CTRL_T :
                mEditor->DeleteWordRight() ;  // Already implemented in cEditorBase
                handled = true ;
                break ;

            case CTRL_Y :
                mEditor->DeleteLine() ;
                handled = true ;
                break ;
                
            case CTRL_U :
                // Undo only -- Redo is handled via Alt+U (alt=true) above
                mEditor->Undo() ;
                handled = true ;
                break ;
                
            case CTRL_N :
                mEditor->LineBreak() ;  // Already implemented in cEditorBase
                handled = true ;
                break ;

            case CTRL_L :
                mEditor->FindAgain() ;  // Stub - P1 operation
                handled = true ;
                break ;

            case CTRL_I :
                // Tab insertion - P0 operation
                // API CHANGE: Access document through GetDocument()
                {
                    sWSTab tab ;
                    tab.type = TAB_TAB ;
                    cDocument* doc = mEditor->GetDocument();
                    if (doc)
                    {
                        doc->InsertTab(tab);
                    }
                }
                handled = true ;
                break ;
        }
    }
    
    return handled ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  eSpecialKey key  [in] the special key pressed
/// @param  bool shift       [in] true if Shift modifier is held
/// @param  bool ctrl        [in] true if Ctrl modifier is held
/// @param  bool alt         [in] true if Alt modifier is held
///
/// @return true if the key was handled, false otherwise
///
/// @brief
/// Handle non-character keys (F-keys, navigation, editing) in WordStar mode.
/// This method receives all special keys that cannot be represented as a
/// single ASCII byte. Navigation and editing behavior matches the original
/// WordStar keyboard layout where applicable.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordStarInput::HandleSpecialKey(eSpecialKey key, bool shift, bool ctrl, bool alt)
{
    // While a WordStar chord (^Q/^K/^O/^P/^J) is pending, the Delete key must
    // complete the chord as its char equivalent (0x7F) -- e.g. ^Q Del = Delete
    // Line Left -- matching the GUI, which feeds Delete to HandleKey as 0x7F.
    // Standalone Delete (no pending chord) falls through to DeleteKey below.
    if ((CheckControlMode() == true) && (key == SPECIAL_DELETE))
    {
        return HandleKey(static_cast<char>(0x7F), shift, alt) ;
    }

    bool handled = true ;

    switch (key)
    {
        // --- Navigation keys ---
        case SPECIAL_UP:
        {
            bool upordown = mEditor->mLastKeyUpOrDown ;
            mEditor->mLastKeyUpOrDown = true ;
            UNUSED_ARGUMENT(upordown) ;
            mEditor->MoveCaretLine(-1) ;
            break ;
        }

        case SPECIAL_DOWN:
        {
            bool upordown = mEditor->mLastKeyUpOrDown ;
            mEditor->mLastKeyUpOrDown = true ;
            UNUSED_ARGUMENT(upordown) ;
            mEditor->MoveCaretLine(1) ;
            break ;
        }

        case SPECIAL_LEFT:
        {
            if (ctrl)
            {
                mEditor->MoveCaretWordLeft() ;
            }
            else
            {
                mEditor->MoveCaretLeft() ;
            }
            break ;
        }

        case SPECIAL_RIGHT:
        {
            if (ctrl)
            {
                mEditor->MoveCaretWordRight() ;
            }
            else
            {
                mEditor->MoveCaretRight() ;
            }
            break ;
        }

        case SPECIAL_HOME:
        {
            if (ctrl)
            {
                mEditor->MoveCaretToDocStart() ;
            }
            else
            {
                mEditor->MoveCursorStartLine() ;
            }
            break ;
        }

        case SPECIAL_END:
        {
            if (ctrl)
            {
                mEditor->MoveCaretToDocEnd() ;
            }
            else
            {
                mEditor->MoveCursorEndLine() ;
            }
            break ;
        }

        case SPECIAL_PAGE_UP:
        {
            if (ctrl)
            {
                mEditor->ScrollUp() ;
            }
            else
            {
                mEditor->MoveCaretPage(-1) ;
            }
            break ;
        }

        case SPECIAL_PAGE_DOWN:
        {
            if (ctrl)
            {
                mEditor->ScrollDown() ;
            }
            else
            {
                mEditor->MoveCaretPage(1) ;
            }
            break ;
        }

        // --- Editing keys ---
        case SPECIAL_DELETE:
        {
            if (ctrl)
            {
                mEditor->DeleteWordRight() ;
            }
            else
            {
                mEditor->DeleteKey() ;
            }
            break ;
        }

        case SPECIAL_BACKSPACE:
        {
            if (ctrl)
            {
                mEditor->DeleteWordLeft() ;
            }
            else
            {
                mEditor->Backspace() ;
            }
            break ;
        }

        case SPECIAL_TAB:
        {
            mEditor->Tab() ;
            break ;
        }

        case SPECIAL_ENTER:
        {
            mEditor->LineBreak() ;
            break ;
        }

        case SPECIAL_ESCAPE:
        {
            // Cancel any active chord mode. See the identical guard in
            // HandleKey's ch==27 branch for why the display restore is
            // conditional -- mOldHelpStatus is only meaningful once a real
            // chord has actually been entered.
            bool hadActiveChord = mControlMMode || mControlKMode || mControlOMode
                                || mControlPMode || mControlQMode ;

            mControlMMode = false ;
            mControlKMode = false ;
            mControlOMode = false ;
            mControlPMode = false ;
            mControlQMode = false ;
            mWaitingForHelpTarget = false ;
            mWaitingForHelpChordTarget = false ;
            if(hadActiveChord == true)
            {
                mEditor->mHelpDisplay = mOldHelpStatus ;
            }
            break ;
        }

        // --- Function keys ---
        // WordStar 7's real F1: "press F1, then press the command you want
        // help with" (manual, "Onscreen Help"); F1 F1 changes the help level
        // (manual, "Change Help Level"). System Preferences moved fully to
        // Cmd+, (see cEditorCtrl::keyPressEvent) now that F1 has a real job.
        case SPECIAL_F1:
        {
            if (mWaitingForHelpTarget == true)
            {
                mWaitingForHelpTarget = false ;
                mEditor->ChangeHelpLevel() ;
            }
            else
            {
                // Cancel any pending chord first (same as Escape) -- F1 is
                // meant to be pressed from a neutral state. Without this, a
                // stray F1 mid-chord (e.g. after ^K) would leave that chord
                // flag stuck true, silently misrouting the *next* real
                // keystroke as a chord subcommand once the help lookup
                // consumes this one. The display restore is conditional on
                // a chord having actually been active -- mOldHelpStatus is
                // only meaningful once one has really been entered; doing
                // this unconditionally blanked the help bar the first time
                // F1 was pressed in a fresh session (mOldHelpStatus still
                // held its HELP_NONE construction default).
                bool hadActiveChord = mControlMMode || mControlKMode || mControlOMode
                                    || mControlPMode || mControlQMode ;

                mControlMMode = false ;
                mControlKMode = false ;
                mControlOMode = false ;
                mControlPMode = false ;
                mControlQMode = false ;
                mWaitingForHelpChordTarget = false ;
                if(hadActiveChord == true)
                {
                    mEditor->mHelpDisplay = mOldHelpStatus ;
                }
                mWaitingForHelpTarget = true ;
            }
            break ;
        }

        case SPECIAL_F11:
        {
            mEditor->ToggleFullscreen() ;
            break ;
        }

        // Unhandled function keys
        case SPECIAL_F2:
        case SPECIAL_F3:
        case SPECIAL_F4:
        case SPECIAL_F5:
        case SPECIAL_F6:
        case SPECIAL_F7:
        case SPECIAL_F8:
        case SPECIAL_F9:
        case SPECIAL_F10:
        case SPECIAL_F12:
        {
            handled = false ;
            break ;
        }
    }

    return handled ;
}


eHelpDisplay cWordStarInput::GetHelpStatus(void)
{
        return mEditor->mHelpDisplay ;
}



void cWordStarInput::OnControlMChar(char ch)
{
    mControlMMode = false ;

    // if it's a control key, change it to  character
    if(ch < ' ')
    {
        ch += '@' ;
    }
    ch = tolower(ch) ;

    switch(ch)
    {
        case '@' :
            {
                std::time_t now = std::time(nullptr);
                std::tm* local = std::localtime(&now);
                char buf[80];
                std::strftime(buf, sizeof(buf), "%A, %B %e, %Y", local);
                std::string b = buf ;
                mEditor->InsertWordStarString(b) ;
                break ;
            }

        case '!' :
        {
            std::time_t now = std::time(nullptr);
            std::tm* local = std::localtime(&now);
            char buf[80];
            std::strftime(buf, sizeof(buf), "%I:%M %p", local);
            std::string b = buf ;
            mEditor->InsertWordStarString(b) ;
            break ;
        }

        case '*' :
            {
                mEditor->InsertWordStarString(mEditor->mFileName) ;
                break ;
            }

        case ':' :
            {
#ifdef _WINDOWS
            std::string name = mEditor->mFileDir.substr(0, 2);
                mEditor->InsertWordStarString(name) ;
#endif
                break ;
            }

        case '.' :
            {
#ifdef _WINDOWS
            std::string name = mEditor->mFileDir.substr(2, mEditor->mFileDir.length()); 
#else
                std::string name = mEditor->mFileDir ; // .toStdString() ;
#endif
                mEditor->InsertWordStarString(name) ;
                break ;
            }


        case '\\' :
            {
                std::string name = mEditor->mFileDir + mEditor->mFileName ;
                mEditor->InsertWordStarString(name) ;
                break ;
            }


        case '=' :
        case '#' :
        case '$' :
        case 'p' :
        case 'r' :
        case 'd' :
        case 's' :
        case 'e' :
        case 'o' :
        case 'y' :
            {
                std::string t = string_sprintf("^M-%c", ch) ;
                mEditor->NotImplemented(t) ;
            }
            break ;

        default :
            {
                std::string t = string_sprintf("^M-%c", ch).c_str() ;
                mEditor->InvalidCommand(t) ;
            }
            break ;

    }

}


/////////////////////////////////////////////////////////////////////////////
/// WordStar 7's real F1 behavior: "press F1, then press the command you want
/// help with" (manual, "Onscreen Help"). Handles the key immediately after a
/// single F1 press (F1 F1 is handled separately, in HandleSpecialKey, since
/// it's two special-key presses rather than F1 followed by a character).
/////////////////////////////////////////////////////////////////////////////
void cWordStarInput::OnHelpTargetChar(char ch)
{
    // if it's a control key, change it to a character
    if(ch < ' ')
    {
        ch += '@' ;
    }
    ch = tolower(ch) ;

    // K/Q/O/P/M are chord prefixes, not commands in their own right -- wait
    // for the sub-letter that actually picks a command (e.g. "F1, K, B")
    // instead of describing the bare chord-entry keystroke.
    if(ch == 'k' || ch == 'q' || ch == 'o' || ch == 'p' || ch == 'm')
    {
        mHelpChordPrefix = ch ;
        mWaitingForHelpChordTarget = true ;
        return ;
    }

    const char *help = LookupControlKeyHelp(ch) ;
    if(help != nullptr)
    {
        mEditor->ShowMessage("Help", help) ;
    }
    else
    {
        std::string t = string_sprintf("F1-%c", ch).c_str() ;
        mEditor->InvalidCommand(t) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
/// Second half of "F1, <chord-prefix>, <sub-letter>" -- looks up help for a
/// specific chord command (^KB, ^QF, and so on) once both keystrokes after
/// F1 are known. mHelpChordPrefix was set by OnHelpTargetChar above.
/////////////////////////////////////////////////////////////////////////////
void cWordStarInput::OnHelpChordTargetChar(char ch)
{
    char prefix = mHelpChordPrefix ;
    mHelpChordPrefix = 0 ;

    char lower = ch ;
    if(lower < ' ')
    {
        lower += '@' ;
    }
    lower = tolower(lower) ;

    const char *help = nullptr ;
    switch(prefix)
    {
        case 'k' : help = LookupKChordHelp(lower) ; break ;
        case 'q' : help = LookupQChordHelp(lower) ; break ;
        case 'o' : help = LookupOChordHelp(lower) ; break ;
        case 'p' : help = LookupPChordHelp(lower) ; break ;
        case 'm' : help = LookupMChordHelp(lower) ; break ;
        default  : break ;
    }

    if(help != nullptr)
    {
        std::string title = string_sprintf("Help: ^%c", toupper(prefix)) ;
        mEditor->ShowMessage(title, help) ;
    }
    else
    {
        std::string t = string_sprintf("F1-%c-%c", toupper(prefix), ch).c_str() ;
        mEditor->InvalidCommand(t) ;
    }
}


bool cWordStarInput::OnControlKChar(char ch)
{
    mControlKMode = false ;
    bool retval = false ;

    if(ch < ' ')
    {
        ch += '@' ;
    }
    ch = tolower(ch) ;
    
    switch(ch)
    {
        case 'r' :          // insert/open a file
            {
                // P0: File I/O temporarily disabled (Week 3)
                // TODO: Re-enable when file handlers are updated for new editor
                /*
                QString loadable ;
                bool first = true ;
                for(auto & mFileType : mEditor->mFileTypes)
                {
                    if(mFileType->CanLoad())
                    {
                        std::string ext = mFileType->GetExtensions() ;
                        if(!first)
                        {
                            ext = ";;" + ext ;
                        }
                        first = false ;
                        loadable.append(ext.c_str()) ;
                    }
                }

                QString filename = QFileDialog::getOpenFileName(mEditor, "Insert a file...", QString(), loadable) ;
                */
                std::string filename = mEditor->PromptForLoadFile();
                if(!filename.empty())
                {
                    // API CHANGE: Access document through GetDocument()
                    cDocument* doc = mEditor->GetDocument();
                    POSITION_T position = doc ? doc->GetPosition() : 0 ;
                    mEditor->SetEnabled(false) ;

                    std::filesystem::path filepath(filename) ;
                    mEditor->mFileName = filepath.filename().string() ;
                    mEditor->mFileDir = filepath.parent_path().string() + '/' ;
                    mEditor->mFileSet = true ;

                    std::string loadfile = mEditor->mFileDir + mEditor->mFileName ;
                    mEditor->LoadFile(loadfile) ;

                    mEditor->SetEnabled(true) ;

                    if (doc)
                    {
                        doc->SetPosition(position) ;
                    }

                }
                retval = true ;
            }
            break ;
            
        case 'd' :          // save file and clear buffer
            // API CHANGE: Access document through GetDocument()
            if(mEditor->GetDocument() && mEditor->GetDocument()->mChanged)
            {
                if(mEditor->mFileSet != false)      // if we have a directory, we have a valid file name
                {
                    std::string fname = mEditor->mFileDir + "/" + mEditor->mFileName ;

                    bool ok = mEditor->SaveFile(fname) ;
                    if(ok == false)
                    {
                        mEditor->ShowError("Error", "File Save failed") ;
                    }
                    else
                    {
                        mEditor->GetDocument()->mChanged = false ;

                        retval = true ;
                    }
                }
                else
                {
                    retval = OnControlKChar('T') ;
                }
            }

            if(retval == true && mEditor->GetDocument())
            {
                mEditor->GetDocument()->Clear() ;
                mEditor->LayoutDocument(true) ;  // Resync layout with cleared document
                mEditor->mFileDir = cEditorBase::DefaultFileDir() ;
                mEditor->mFileName = "Unknown.ws" ;
                mEditor->mFileSet = false ;
                mEditor->SetTitle(mEditor->mFileName) ;
                mEditor->GetLayout()->SetFilename(mEditor->mFileName) ;
                mEditor->GetLayout()->SetFileDir(mEditor->mFileDir) ;
            }
            break ;

        case 's' :          // save file
            if(mEditor->GetDocument() && mEditor->GetDocument()->mChanged == true)
            {
                if(mEditor->mFileSet != false)      // if we have a directory, we have a valid file name
                {
                    std::string fname =  mEditor->mFileDir + "/" + mEditor->mFileName ;

                    bool ok = mEditor->SaveFile(fname) ;
                    if(ok == false)
                    {
                        mEditor->ShowError("Error", "File Save failed") ;
                    }
                    else
                    {
                        mEditor->GetDocument()->mChanged = false ;

                        retval = true ;
                    }
                    break ;
                }
                else
                {
                    retval = OnControlKChar('T') ;
                }
            }
            break ;

        case 'x' :          // save and exit
            mEditor->Quit();
            break ;

        case 'q' :          // abandoned
            mEditor->AbandonFile();
            break ;

        case 't' :          // save as
            {
                // P0: File I/O temporarily disabled (Week 3)
                // TODO: Re-enable when file handlers are updated for new editor
                /*
                QString loadable ;
                bool first = true ;
                for(auto & mFileType : mEditor->mFileTypes)
                {
                    if(mFileType->CanSave())
                    {
                        std::string ext = mFileType->GetExtensions() ;
                        if(!first)
                        {
                            ext = ";;" + ext ;
                        }
                        first = false ;
                        loadable.append(ext.c_str()) ;
                    }
                }

                QString filename = QFileDialog::getSaveFileName(mEditor, "Save file...", QString(), loadable) ;
                */
                std::string filename = mEditor->PromptForSaveFile();
                if(!filename.empty())
                {
                    std::filesystem::path filepath(filename) ;
                    mEditor->mFileDir = filepath.parent_path().string() ;
                    mEditor->mFileName = filepath.filename().string() ;
                    mEditor->mFileSet = true ;

                    bool ok = mEditor->SaveFile(filename) ;
                    if(ok == false)
                    {
                        retval = false ;
                    }
                    else
                    {
                        mEditor->GetDocument()->mChanged = false ;
                        retval = true ;

                        std::string msg = string_sprintf("File %s saved", filename.c_str()) ;
                        mEditor->ShowMessage("Save OK", msg) ;

                        std::string temp = mEditor->mFileName ;
                        mEditor->SetTitle(temp) ;
                    }
                }
            }
            break ;

        case 'b' :          // begin block
            mEditor->SetBeginBlock() ;
            break ;
            
        case 'k' :          // end block
            mEditor->SetEndBlock() ;
            break ;
            
        case 'c' :          // copy block
            mEditor->CopyBlock() ;
            break ; 
            
        case 'v' :          // move a block
            mEditor->MoveBlock() ;
            break ;
            
        case 'y' :          // delete a block
            mEditor->DeleteBlock() ;
            break ;
            
        case '\"' :         // upper case block
            mEditor->UpperCaseBlock() ;
            break ;
        
        case '\'' :         // low case block
            mEditor->LowerCaseBlock() ;
            break ;
            
        case '.' :
            mEditor->TitleCaseBlock() ;
            break ;

        case '<' :          // unset block
            mEditor->UnSetBlock() ;
            break ;

        case 'h' :          // toggle hide block
            mEditor->ToggleHideBlock() ;
            break ;
            
        case '[' :          // system clipboard paste
            mEditor->ClipboardPaste() ;
            break ;
            
        case ']' :          // system clipboard paste
            mEditor->ClipboardCopy() ;
            break ;
            
        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' :
        case '8' :
        case '9' :
            {
                int offset = ch - '0' ;
                mEditor->SavePosition(offset) ;
            }
            break ;
            
        case '?' :
            mEditor->WordCountBlock() ;
            break ;
            
        case 'p' :
            mEditor->PrintPreview() ;
            break ;

        case 'u' :
            mEditor->SetPreviousBlock() ;
            break ;

        case 'o' :
        case 'e' :
        case 'j' :
        case '\\' :
        case 'l' :
        case 'f' :
        case 'w' :
        case 'm' :
        case 'z' :
        case 'n' :
        case 'i' :
        case 'a' :
        case 'g' :
            {
                std::string t = string_sprintf("^K-%c", ch) ;
                mEditor->NotImplemented(t) ;
            }
            break ;

        default :
            {
                std::string t = string_sprintf("^K-%c", ch).c_str() ;
                mEditor->InvalidCommand(t) ;
            }
            break ;
    }
    

    return retval ;
}


void cWordStarInput::OnControlOChar(char ch)
{
    mControlOMode = false ;
    
    if(ch < ' ')
    {
        ch += '@' ;
    }

    ch = tolower(ch) ;
    
    switch(ch)
    {
        case 'd' :
            // ^OD: in page mode toggles reveal codes pane, in continuous mode cycles show control
            mEditor->ToggleShowControl() ;
            break ;

        case 'c' :
            mEditor->InsertCenterTab() ;
            break ;
        
        case ']' :
            mEditor->InsertRightTab() ;
            break ;
        
        case '?' :
            mEditor->About() ;
            break ;
            
        case 'p' :
            mEditor->PrintPreview() ;
            break ;
            
        case 'y' :
            mEditor->PageLayout() ;
            break ;

        case 't' :
            // GUI: real pixel-based page/continuous mode.
            // TUI: no pixel canvas to paginate, so instead center the
            // editing pane (and ruler) horizontally in the terminal.
            if (mEditor->IsPageModeSupported())
            {
                mEditor->ToggleDisplayMode() ;
            }
            else
            {
                mEditor->ToggleCenterView() ;
            }
            break ;

        case 'j' :
            mEditor->ToggleJustification() ;
            break ;

        case 'w' :          // ^OW: word wrap on/off (real WordStar 7 command)
            mEditor->ToggleWordWrap() ;
            break ;

        case 'l' :
        case 'g' :
        case 'x' :
        case 'i' :
        case 'o' :
        case 'u' :
        case 'f' :
        case 's' :
        case 'v' :
        case 'e' :
        case 'h' :
        case 'a' :
        case ' ' :
        case 'b' :
            {
                mEditor->Preferences() ;
            }
            break ;

        case 'k' :
        case 'm' :
        case 'z' :
        case '#' :
        case 'n' :
            {
                std::string t = string_sprintf("^O-%c", ch) ;
                mEditor->NotImplemented(t) ;
            }
            break ;
        
        case '<' :
            mEditor->SetParagraphAlignment(JUST_LEFT) ;
            break ;

        case '>' :
            mEditor->SetParagraphAlignment(JUST_RIGHT) ;
            break ;

        case '=' :
            mEditor->SetParagraphAlignment(JUST_CENTER) ;
            break ;

        case '+' :
            mEditor->SetParagraphAlignment(JUST_JUST) ;
            break ;

        default :
            {
                std::string t = string_sprintf("^O-%c", ch).c_str() ;
                mEditor->InvalidCommand(t) ;
            }
            break ;
    }
}


void cWordStarInput::OnControlQChar(char ch)
{
    mControlQMode = false ;
    
    if(ch < ' ')
    {
        ch += '@' ;
    }
    ch = tolower(ch) ;
    
    switch(ch)
    {
        case 'a' :
            mEditor->Replace() ;
            break ;
            
        case 'f' :
            mEditor->Find() ;
            break ;
            
        case 'e' :
            mEditor->MoveCursorTopLeft() ;
            break ;
            
        case 'x' :
            mEditor->MoveCursorBottomRight() ;
            break ;
            
        case 'r' :
            mEditor->MoveCursorTopofFile() ;
            break ;
            
        case 'c' :
            mEditor->MoveCursorEndofFile() ;
            break ;
            
        case 'b' :
            mEditor->MoveCursorStartBlock() ;
            break ;
            
        case 'k' :
            mEditor->MoveCursorEndBlock() ;
            break ;
            
        case 's' :
            mEditor->MoveCursorStartLine() ;
            break ;
            
        case 'd' :
            mEditor->MoveCursorEndLine() ;
            break ;
            
        case '0' :
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '5' :
        case '6' :
        case '7' :
        case '8' :
        case '9' :
            {
                int offset = ch - '0' ;
                mEditor->GotoSavePosition(offset) ;
            }
            break ;

        case 'u' :
            mEditor->LayoutDocument(true) ;
            break ;
            
        case 'l' :
            mEditor->SpellCheckDocument() ;
            break ;

        case 'n' :
            mEditor->SpellCheckWord() ;
            break ;
            
        case 'o' :
            mEditor->SpellCheckEnterWord() ;
            break ;

        case 'g' :
            mEditor->GotoCharacter() ;
            break ;

        case 'h' :
            mEditor->GotoCharacterBackward() ;
            break ;

        case 'i' :
            mEditor->GotoPage() ;
            break ;

        case ' ' :      // Space key
        case 0x7F :     // DEL key (passed through from keyPressEvent)
            mEditor->DeleteLineLeft() ;
            break ;

        case 'y' :
            mEditor->DeleteLineRight() ;
        break ;

        case 't' :
            mEditor->DeleteToChar() ;
            break ;

        case '=' :
            mEditor->GotoFontTag() ;
            break ;

        case 'p' :
            mEditor->GotoPreviousPosition() ;
            break ;

        case 'v' :
            mEditor->GotoLastFindandReplace() ;
            break ;

        case '<' :
        case 'm' :
        case 'j' :
        case 'w' :
        case 'z' :
            {
                std::string t = string_sprintf("^Q-%c", ch) ;
                mEditor->NotImplemented(t) ;
            }
            break ;
        
        default :
            {
                std::string t = string_sprintf("^Q-%c", ch).c_str() ;
                mEditor->InvalidCommand(t) ;
            }
            break ;
    }
}



void cWordStarInput::OnControlPChar(char ch)
{
    mControlPMode = false ;

    // close typing group before formatting changes
    mEditor->CloseTypingGroup() ;

    if(ch < ' ')
    {
        ch += '@' ;
    }
    ch = tolower(ch) ;
    
    switch(ch)
    {
        case 'b' :
            // API CHANGE: Access document through GetDocument()
            if (mEditor->GetDocument())
            {
                mEditor->GetDocument()->BeginBold() ;
            }
            break ;

        case 's' :
            if (mEditor->GetDocument())
            {
                mEditor->GetDocument()->BeginUnderline() ;
            }
            break ;

        case 'v' :
            if (mEditor->GetDocument())
            {
                mEditor->GetDocument()->BeginSubscript() ;
            }
            break ;

        case 't' :
            if (mEditor->GetDocument())
            {
                mEditor->GetDocument()->BeginSuperscript() ;
            }
            break ;

        case 'y' :
            if (mEditor->GetDocument())
            {
                mEditor->GetDocument()->BeginItalics() ;
            }
            break ;

        case 'x' :
            if (mEditor->GetDocument())
            {
                mEditor->GetDocument()->BeginStrikeThrough() ;
            }
            break ;

        case 'k' :
            if (mEditor->GetDocument())
            {
                mEditor->GetDocument()->BeginStrikeThrough() ;
            }
            break ;
            
        case '=' :
        case '+' :
            mEditor->SelectFont() ;
            break ;
            
        case '-' :
            mEditor->SelectColor() ;
            break ;
            
        case 'd' :
        case 'n' :
        case 'a' :
        case 'h' :
        case ' ' :
        case 'f' :
        case 'g' :
        case '*' :
        case '&' :
        case 'o' :
        case 'c' :
        case 'i' :
        case '.' :
        case '0' :
        case 'q' :
        case 'w' :
        case 'e' :
        case 'r' :
        case '!' :
        case '?' :
            {
                std::string t = string_sprintf("^P-%c", ch) ;
                mEditor->NotImplemented(t) ;
            }
            break ;
            
        default :
            {
                std::string t = string_sprintf("^P-%c", ch).c_str() ;
                mEditor->InvalidCommand(t) ;
            }
            break ;
    }
}


/// @}
