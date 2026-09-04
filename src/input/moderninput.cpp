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

/**
 * @class cModernInput
 *
 * @brief Modern/CUA keyboard input handler implementation.
 *
 * Implements the cModernInput class, which translates CUA-style keyboard
 * commands into editor actions. This class is platform-independent (no Qt
 * dependencies) and is shared by both the GUI and TUI editors.
 *
 * @section moderninput_direct Direct CUA Bindings (Ctrl+letter)
 * Single-key commands:
 * - File: Ctrl+N (new), Ctrl+O (open), Ctrl+S (save), Ctrl+W (close),
 *   Ctrl+P (print)
 * - Edit: Ctrl+Z (undo), Ctrl+Y (redo), Ctrl+X (cut), Ctrl+C (copy),
 *   Ctrl+V (paste), Ctrl+A (select all)
 * - Find: Ctrl+F (find), Ctrl+H (replace), Ctrl+G (goto page)
 * - Format: Ctrl+B (bold), Ctrl+I (italic), Ctrl+U (underline),
 *   Ctrl+D (font)
 * - Align: Ctrl+E (center), Ctrl+L (left), Ctrl+R (right),
 *   Ctrl+J (justify)
 *
 * @section moderninput_prefix Alt Prefix Chords
 * Alt+K/Q/O/P enter prefix modes for advanced operations:
 * - Alt+K (block/marker): B=begin, K=end, C=copy, V=move, Y=delete, 0-9=markers
 * - Alt+Q (navigation): E=top, X=bottom, B/K=block start/end, 0-9=goto marker
 * - Alt+O (onscreen): C=center tab, ]=right tab, D=show control, T=page mode
 * - Alt+P (style): V=subscript, T=superscript, X=strikethrough
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cModernInput Modern/CUA input handler class
 * @see cEditorBase Editor base class receiving commands
 */

#include <filesystem>

#include "moderninput.h"
#include "wordtsarinput.h"
#include "src/core/editor/editorbase.h"
#include "src/core/include/utils.h"

/// @ingroup Input
/// @{


/////////////////////////////////////////////////////////////////////////////
///
/// @param  cEditorBase *editor  [in] pointer to the editor receiving commands
///
/// @return nothing
///
/// @brief
/// Construct a Modern/CUA input handler. Stores the editor pointer and
/// initializes all prefix mode flags to false.
///
/////////////////////////////////////////////////////////////////////////////
cModernInput::cModernInput(cEditorBase *editor)
{
    mEditor = editor ;

    mAltKMode = false ;
    mAltQMode = false ;
    mAltOMode = false ;
    mAltPMode = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor.
///
/////////////////////////////////////////////////////////////////////////////
cModernInput::~cModernInput(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if in prefix chord mode, false otherwise
///
/// @brief
/// Check if the handler is waiting for a follow-up key after an Alt
/// prefix chord (Alt+K, Alt+Q, Alt+O, or Alt+P).
///
/////////////////////////////////////////////////////////////////////////////
bool cModernInput::CheckControlMode(void)
{
    if (mAltKMode || mAltQMode || mAltOMode || mAltPMode)
    {
        return true ;
    }

    return false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  char ch    [in] character value (1-26 for Ctrl+A..Z, or printable)
/// @param  bool shift [in] true if Shift modifier is held
/// @param  bool alt   [in] true if Alt modifier is held
///
/// @return true if handled as command, false to insert as text
///
/// @brief
/// Handle a keyboard character input in Modern/CUA mode.
///
/// When alt=true, checks for Alt+K/Q/O/P prefix entry.
/// When a prefix mode is active, dispatches to the corresponding
/// OnAlt*Char handler. Otherwise, interprets Ctrl+letter values (1-26)
/// as CUA commands.
///
/////////////////////////////////////////////////////////////////////////////
bool cModernInput::HandleKey(char ch, bool shift, bool alt)
{
    bool handled = false ;

    // Handle Alt+letter: enter prefix modes or return false
    if (alt)
    {
        char lower = tolower(ch) ;

        if (lower == 'k')
        {
            mAltKMode = true ;
            return true ;
        }
        if (lower == 'q')
        {
            mAltQMode = true ;
            return true ;
        }
        if (lower == 'o')
        {
            mAltOMode = true ;
            return true ;
        }
        if (lower == 'p')
        {
            mAltPMode = true ;
            return true ;
        }

        // Other Alt combinations not handled in Modern mode
        return false ;
    }

    // Handle prefix mode follow-up keys
    if (mAltKMode)
    {
        OnAltKChar(ch) ;
        return true ;
    }
    if (mAltQMode)
    {
        OnAltQChar(ch) ;
        return true ;
    }
    if (mAltOMode)
    {
        OnAltOChar(ch) ;
        return true ;
    }
    if (mAltPMode)
    {
        OnAltPChar(ch) ;
        return true ;
    }

    // Handle CUA Ctrl+letter bindings (values 1-26)
    switch (ch)
    {
        // --- Selection ---
        case CTRL_A :
        {
            // Select All: set block from start to end of document
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                // Move to start and begin block
                doc->SetPosition(0) ;
                mEditor->SetBeginBlock() ;

                // Move to end (before EOF marker) and end block
                POSITION_T endPos = doc->GetTextSize() - 1 ;
                if (endPos < 0)
                {
                    endPos = 0 ;
                }
                doc->SetPosition(endPos) ;
                mEditor->SetEndBlock() ;
            }
            handled = true ;
            break ;
        }

        // --- Formatting ---
        case CTRL_B :
        {
            // Bold toggle
            mEditor->CloseTypingGroup() ;
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                doc->BeginBold() ;
            }
            handled = true ;
            break ;
        }

        // --- Clipboard ---
        case CTRL_C :
        {
            // Copy to clipboard
            mEditor->ClipboardCopy() ;
            handled = true ;
            break ;
        }

        // --- Font ---
        case CTRL_D :
        {
            // Font dialog
            mEditor->SelectFont() ;
            handled = true ;
            break ;
        }

        // --- Alignment ---
        case CTRL_E :
        {
            // Center alignment (brackets paragraph with .oj dot commands)
            mEditor->SetParagraphAlignment(JUST_CENTER) ;
            handled = true ;
            break ;
        }

        // --- Find ---
        case CTRL_F :
        {
            mEditor->Find() ;
            handled = true ;
            break ;
        }

        // --- Goto ---
        case CTRL_G :
        {
            mEditor->GotoPage() ;
            handled = true ;
            break ;
        }

        // --- Replace ---
        case CTRL_H :
        {
            mEditor->Replace() ;
            handled = true ;
            break ;
        }

        // --- Italic ---
        case CTRL_I :
        {
            // Italic toggle
            mEditor->CloseTypingGroup() ;
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                doc->BeginItalics() ;
            }
            handled = true ;
            break ;
        }

        // --- Justify ---
        case CTRL_J :
        {
            mEditor->SetParagraphAlignment(JUST_JUST) ;
            handled = true ;
            break ;
        }

        // --- Block Begin ---
        case CTRL_K :
        {
            mEditor->SetBeginBlock() ;
            handled = true ;
            break ;
        }

        // --- Left Align ---
        case CTRL_L :
        {
            mEditor->SetParagraphAlignment(JUST_LEFT) ;
            handled = true ;
            break ;
        }

        // --- New document ---
        case CTRL_N :
        {
            // New document: clear and reset
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                // Check if document has unsaved changes
                if (doc->mChanged)
                {
                    bool save = mEditor->AskYesNo("New Document",
                        "Current document has unsaved changes. Save first?") ;
                    if (save)
                    {
                        if (mEditor->mFileSet)
                        {
                            std::string fname = mEditor->mFileDir + "/" + mEditor->mFileName ;
                            mEditor->SaveFile(fname) ;
                        }
                        else
                        {
                            std::string filename = mEditor->PromptForSaveFile() ;
                            if (!filename.empty())
                            {
                                mEditor->SaveFile(filename) ;
                            }
                        }
                    }
                }

                doc->Clear() ;
                mEditor->LayoutDocument(true) ;
                mEditor->mFileDir = cEditorBase::DefaultFileDir() ;
                mEditor->mFileName = "Unknown.ws" ;
                mEditor->mFileSet = false ;
                mEditor->SetTitle(mEditor->mFileName) ;
                mEditor->GetLayout()->SetFilename(mEditor->mFileName) ;
                mEditor->GetLayout()->SetFileDir(mEditor->mFileDir) ;
            }
            handled = true ;
            break ;
        }

        // --- Open ---
        case CTRL_O :
        {
            std::string filename = mEditor->PromptForLoadFile() ;
            if (!filename.empty())
            {
                cDocument* doc = mEditor->GetDocument() ;
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
                    doc->SetPosition(0) ;
                }
            }
            handled = true ;
            break ;
        }

        // --- Print ---
        case CTRL_P :
        {
            mEditor->PrintPreview() ;
            handled = true ;
            break ;
        }

        // --- Ctrl+Q: unassigned in Modern mode ---
        case CTRL_Q :
        {
            handled = false ;
            break ;
        }

        // --- Right Align ---
        case CTRL_R :
        {
            mEditor->SetParagraphAlignment(JUST_RIGHT) ;
            handled = true ;
            break ;
        }

        // --- Save ---
        case CTRL_S :
        {
            if (shift)
            {
                // Ctrl+Shift+S: Save As
                std::string filename = mEditor->PromptForSaveFile() ;
                if (!filename.empty())
                {
                    std::filesystem::path filepath(filename) ;
                    mEditor->mFileDir = filepath.parent_path().string() ;
                    mEditor->mFileName = filepath.filename().string() ;
                    mEditor->mFileSet = true ;

                    bool ok = mEditor->SaveFile(filename) ;
                    if (ok)
                    {
                        mEditor->GetDocument()->mChanged = false ;
                        mEditor->SetTitle(mEditor->mFileName) ;
                    }
                    else
                    {
                        mEditor->ShowError("Error", "File Save failed") ;
                    }
                }
            }
            else
            {
                // Ctrl+S: Save
                cDocument* doc = mEditor->GetDocument() ;
                if (doc && doc->mChanged)
                {
                    if (mEditor->mFileSet)
                    {
                        std::string fname = mEditor->mFileDir + "/" + mEditor->mFileName ;
                        bool ok = mEditor->SaveFile(fname) ;
                        if (ok)
                        {
                            doc->mChanged = false ;
                        }
                        else
                        {
                            mEditor->ShowError("Error", "File Save failed") ;
                        }
                    }
                    else
                    {
                        // No filename set, do Save As
                        std::string filename = mEditor->PromptForSaveFile() ;
                        if (!filename.empty())
                        {
                            std::filesystem::path filepath(filename) ;
                            mEditor->mFileDir = filepath.parent_path().string() ;
                            mEditor->mFileName = filepath.filename().string() ;
                            mEditor->mFileSet = true ;

                            bool ok = mEditor->SaveFile(filename) ;
                            if (ok)
                            {
                                doc->mChanged = false ;
                                mEditor->SetTitle(mEditor->mFileName) ;
                            }
                            else
                            {
                                mEditor->ShowError("Error", "File Save failed") ;
                            }
                        }
                    }
                }
            }
            handled = true ;
            break ;
        }

        // --- Tab ---
        case CTRL_T :
        {
            // Insert tab
            sWSTab tab ;
            tab.type = TAB_TAB ;
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                doc->InsertTab(tab) ;
            }
            handled = true ;
            break ;
        }

        // --- Underline ---
        case CTRL_U :
        {
            mEditor->CloseTypingGroup() ;
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                doc->BeginUnderline() ;
            }
            handled = true ;
            break ;
        }

        // --- Paste ---
        case CTRL_V :
        {
            mEditor->ClipboardPaste() ;
            handled = true ;
            break ;
        }

        // --- Close ---
        case CTRL_W :
        {
            mEditor->CloseEvent() ;
            handled = true ;
            break ;
        }

        // --- Cut ---
        case CTRL_X :
        {
            // Cut = Copy + Delete
            mEditor->ClipboardCopy() ;
            mEditor->DeleteBlock() ;
            handled = true ;
            break ;
        }

        // --- Redo ---
        case CTRL_Y :
        {
            mEditor->Redo() ;
            handled = true ;
            break ;
        }

        // --- Undo ---
        case CTRL_Z :
        {
            if (shift)
            {
                // Ctrl+Shift+Z: alternative Redo
                mEditor->Redo() ;
            }
            else
            {
                mEditor->Undo() ;
            }
            handled = true ;
            break ;
        }

        // --- Escape ---
        case ESCAPE :
        {
            // Cancel any active prefix mode
            mAltKMode = false ;
            mAltQMode = false ;
            mAltOMode = false ;
            mAltPMode = false ;
            handled = true ;
            break ;
        }

        default :
        {
            // Not handled as command -- caller should insert as text
            handled = false ;
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
/// Handle non-character keys (F-keys, navigation, editing) in Modern/CUA
/// mode. Navigation behavior is identical to WordStar mode (arrows, Home,
/// End, PgUp, PgDn). F-key behavior differs: F3=FindAgain (not debug).
///
/////////////////////////////////////////////////////////////////////////////
bool cModernInput::HandleSpecialKey(eSpecialKey key, bool shift, bool ctrl, bool alt)
{
    UNUSED_ARGUMENT(shift) ;
    UNUSED_ARGUMENT(alt) ;

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
            // Cancel any active prefix mode
            mAltKMode = false ;
            mAltQMode = false ;
            mAltOMode = false ;
            mAltPMode = false ;
            break ;
        }

        // --- Function keys ---
        case SPECIAL_F1:
        {
            mEditor->SystemPreferences() ;
            break ;
        }

        case SPECIAL_F3:
        {
            // Modern mode: F3 = Find Next
            mEditor->FindAgain() ;
            break ;
        }

        case SPECIAL_F11:
        {
            mEditor->ToggleFullscreen() ;
            break ;
        }

        // Unhandled function keys
        case SPECIAL_F2:
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


/////////////////////////////////////////////////////////////////////////////
///
/// @return the current help display status
///
/// @brief
/// Modern mode always returns HELP_NONE. Help panel content is driven
/// by mInputMode rather than GetHelpStatus().
///
/////////////////////////////////////////////////////////////////////////////
eHelpDisplay cModernInput::GetHelpStatus(void)
{
    return HELP_NONE ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  char key  [in] the follow-up key after Alt+K
///
/// @return nothing
///
/// @brief
/// Dispatch follow-up key for the Alt+K (block/marker) prefix chord.
/// Handles: 0-9 (set marker), B (block begin), K (block end),
/// C (copy block), V (move block), Y (delete block), H (hide block),
/// < (unset block), U (previous block), " (uppercase), ' (lowercase),
/// . (title case), ? (word count).
///
/////////////////////////////////////////////////////////////////////////////
void cModernInput::OnAltKChar(char key)
{
    mAltKMode = false ;

    // Convert control chars to printable for matching
    if (key < ' ')
    {
        key += '@' ;
    }
    key = tolower(key) ;

    switch (key)
    {
        case 'b' :
        {
            mEditor->SetBeginBlock() ;
            break ;
        }

        case 'k' :
        {
            mEditor->SetEndBlock() ;
            break ;
        }

        case 'c' :
        {
            mEditor->CopyBlock() ;
            break ;
        }

        case 'v' :
        {
            mEditor->MoveBlock() ;
            break ;
        }

        case 'y' :
        {
            mEditor->DeleteBlock() ;
            break ;
        }

        case 'h' :
        {
            mEditor->ToggleHideBlock() ;
            break ;
        }

        case '<' :
        {
            mEditor->UnSetBlock() ;
            break ;
        }

        case 'u' :
        {
            mEditor->SetPreviousBlock() ;
            break ;
        }

        case '\"' :
        {
            mEditor->UpperCaseBlock() ;
            break ;
        }

        case '\'' :
        {
            mEditor->LowerCaseBlock() ;
            break ;
        }

        case '.' :
        {
            mEditor->SentenceCaseBlock() ;
            break ;
        }

        case '?' :
        {
            mEditor->WordCountBlock() ;
            break ;
        }

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
            int offset = key - '0' ;
            mEditor->SavePosition(offset) ;
            break ;
        }

        default :
        {
            // Unknown follow-up key -- ignore
            break ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  char key  [in] the follow-up key after Alt+Q
///
/// @return nothing
///
/// @brief
/// Dispatch follow-up key for the Alt+Q (navigation/deletion) prefix chord.
/// Handles: 0-9 (goto marker), E/X (top/bottom screen), B/K (block nav),
/// P (previous position), V (last find), = (font tag), G/H (goto char),
/// U (re-layout), DEL (delete line left), Y (delete line right),
/// T (delete to char).
///
/////////////////////////////////////////////////////////////////////////////
void cModernInput::OnAltQChar(char key)
{
    mAltQMode = false ;

    // Convert control chars to printable for matching
    if (key < ' ')
    {
        key += '@' ;
    }
    key = tolower(key) ;

    switch (key)
    {
        case 'e' :
        {
            mEditor->MoveCursorTopLeft() ;
            break ;
        }

        case 'x' :
        {
            mEditor->MoveCursorBottomRight() ;
            break ;
        }

        case 'b' :
        {
            mEditor->MoveCursorStartBlock() ;
            break ;
        }

        case 'k' :
        {
            mEditor->MoveCursorEndBlock() ;
            break ;
        }

        case 'p' :
        {
            mEditor->GotoPreviousPosition() ;
            break ;
        }

        case 'v' :
        {
            mEditor->GotoLastFindandReplace() ;
            break ;
        }

        case '=' :
        {
            mEditor->GotoFontTag() ;
            break ;
        }

        case 'g' :
        {
            mEditor->GotoCharacter() ;
            break ;
        }

        case 'h' :
        {
            mEditor->GotoCharacterBackward() ;
            break ;
        }

        case 'u' :
        {
            mEditor->LayoutDocument(true) ;
            break ;
        }

        case 'y' :
        {
            mEditor->DeleteLineRight() ;
            break ;
        }

        case 't' :
        {
            mEditor->DeleteToChar() ;
            break ;
        }

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
            int offset = key - '0' ;
            mEditor->GotoSavePosition(offset) ;
            break ;
        }

        case ' ' :
        case 0x7F :
        {
            // Space or DEL: delete line left
            mEditor->DeleteLineLeft() ;
            break ;
        }

        default :
        {
            // Unknown follow-up key -- ignore
            break ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  char key  [in] the follow-up key after Alt+O
///
/// @return nothing
///
/// @brief
/// Dispatch follow-up key for the Alt+O (onscreen formatting) prefix chord.
/// Handles: C (center tab), ] (right tab), D (show control toggle),
/// T (display mode toggle), J (justification toggle), Y (page layout).
///
/////////////////////////////////////////////////////////////////////////////
void cModernInput::OnAltOChar(char key)
{
    mAltOMode = false ;

    // Convert control chars to printable for matching
    if (key < ' ')
    {
        key += '@' ;
    }
    key = tolower(key) ;

    switch (key)
    {
        case 'c' :
        {
            mEditor->InsertCenterTab() ;
            break ;
        }

        case ']' :
        {
            mEditor->InsertRightTab() ;
            break ;
        }

        case 'd' :
        {
            mEditor->ToggleShowControl() ;
            break ;
        }

        case 't' :
        {
            // GUI: toggle page/continuous mode. TUI: center the editing
            // pane horizontally instead (no pixel canvas to paginate).
            if (mEditor->IsPageModeSupported())
            {
                mEditor->ToggleDisplayMode() ;
            }
            else
            {
                mEditor->ToggleCenterView() ;
            }
            break ;
        }

        case 'j' :
        {
            mEditor->ToggleJustification() ;
            break ;
        }

        case 'y' :
        {
            mEditor->PageLayout() ;
            break ;
        }

        default :
        {
            // Unknown follow-up key -- ignore
            break ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  char key  [in] the follow-up key after Alt+P
///
/// @return nothing
///
/// @brief
/// Dispatch follow-up key for the Alt+P (style/print) prefix chord.
/// Handles: V (subscript), T (superscript), X/K (strikethrough),
/// - (color dialog).
///
/////////////////////////////////////////////////////////////////////////////
void cModernInput::OnAltPChar(char key)
{
    mAltPMode = false ;

    // Close typing group before formatting changes
    mEditor->CloseTypingGroup() ;

    // Convert control chars to printable for matching
    if (key < ' ')
    {
        key += '@' ;
    }
    key = tolower(key) ;

    switch (key)
    {
        case 'v' :
        {
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                doc->BeginSubscript() ;
            }
            break ;
        }

        case 't' :
        {
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                doc->BeginSuperscript() ;
            }
            break ;
        }

        case 'x' :
        case 'k' :
        {
            cDocument* doc = mEditor->GetDocument() ;
            if (doc)
            {
                doc->BeginStrikeThrough() ;
            }
            break ;
        }

        case '-' :
        {
            mEditor->SelectColor() ;
            break ;
        }

        default :
        {
            // Unknown follow-up key -- ignore
            break ;
        }
    }
}


/// @}
