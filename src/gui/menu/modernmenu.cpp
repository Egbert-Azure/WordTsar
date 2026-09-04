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

#include "src/gui/menu/modernmenu.h"

/////////////////////////////////////////////////////////////////////////////
// File Menu
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the File Open menu label with Ctrl+O shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFileOpenLabel(void) const
{
    return "&Open...\tCtrl+O";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the File Save menu label with Ctrl+S shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFileSaveLabel(void) const
{
    return "&Save\tCtrl+S";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the File Save As menu label with Ctrl+Shift+S shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFileSaveAsLabel(void) const
{
    return "Save &As...\tCtrl+Shift+S";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the File Save and Close menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFileSaveAndCloseLabel(void) const
{
    return "Save and C&lose";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the File Close menu label with Ctrl+W shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFileCloseLabel(void) const
{
    return "&Close Current Document\tCtrl+W";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the File Print menu label with Ctrl+P shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFilePrintLabel(void) const
{
    return "&Print...\tCtrl+P";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the File Print Preview menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFilePrintPreviewLabel(void) const
{
    return "Print Pre&view...";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the File Exit menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetFileExitLabel(void) const
{
    return "E&xit WordTsar\tCmd+Q";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Basic
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Undo menu label with Ctrl+Z shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditUndoLabel(void) const
{
    return "&Undo\tCtrl+Z";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Redo menu label with Ctrl+Y shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditRedoLabel(void) const
{
    return "&Redo\tCtrl+Y";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Mark Block Start menu label with Alt+K, B shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditMarkBlockStartLabel(void) const
{
    return "Mark Block &Beginning\tAlt+K, B";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Mark Block End menu label with Alt+K, K shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditMarkBlockEndLabel(void) const
{
    return "Mark Block &End\tAlt+K, K";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Move
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Move Block menu label with Alt+K, V shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditMoveBlockLabel(void) const
{
    return "&Block\tAlt+K, V";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Copy
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Copy Block menu label with Alt+K, C shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditCopyBlockLabel(void) const
{
    return "&Block\tAlt+K, C";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Paste menu label with Ctrl+V shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditCopyFromClipboardLabel(void) const
{
    return "&Paste\tCtrl+V";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Copy menu label with Ctrl+C shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditCopyToClipboardLabel(void) const
{
    return "&Copy\tCtrl+C";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Delete
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Delete Block menu label with Alt+K, Y shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditDeleteBlockLabel(void) const
{
    return "&Block\tAlt+K, Y";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Delete Word menu label with Ctrl+Del shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditDeleteWordLabel(void) const
{
    return "&Word\tCtrl+Del";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the Edit Delete Line menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditDeleteLineLabel(void) const
{
    return "&Line";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Delete Line Left menu label with Alt+Q, DEL shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditDeleteLineLeftLabel(void) const
{
    return "L&ine Left of Cursor\tAlt+Q, DEL";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Delete Line Right menu label with Alt+Q, Y shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditDeleteLineRightLabel(void) const
{
    return "Line &Right of Cursor\tAlt+Q, Y";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Delete To Character menu label with Alt+Q, T shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditDeleteToCharLabel(void) const
{
    return "&To Character...\tAlt+Q, T";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Find/Replace
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Mark Previous Block menu label with Alt+K, U shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditMarkPreviousBlockLabel(void) const
{
    return "Mark &Previous Block\tAlt+K, U";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Find menu label with Ctrl+F shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditFindLabel(void) const
{
    return "&Find...\tCtrl+F";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Find and Replace menu label with Ctrl+H shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditFindAndReplaceLabel(void) const
{
    return "Find and &Replace...\tCtrl+H";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with F3 shortcut
///
/// @brief
/// Returns the Edit Find Next menu label with F3 shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditFindNextLabel(void) const
{
    return "Find Ne&xt\tF3";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Goto
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Goto Character menu label with Alt+Q, G shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoCharLabel(void) const
{
    return "&Go to Character...\tAlt+Q, G";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Goto Page menu label with Ctrl+G shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoPageLabel(void) const
{
    return "Goto &Page...\tCtrl+G";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the goto marker labels with Alt+Q prefix chord shortcuts.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoMarker1Label(void) const
{
    return "&1\tAlt+Q, 1";
}

QString cModernMenuProvider::GetEditGotoMarker2Label(void) const
{
    return "&2\tAlt+Q, 2";
}

QString cModernMenuProvider::GetEditGotoMarker3Label(void) const
{
    return "&3\tAlt+Q, 3";
}

QString cModernMenuProvider::GetEditGotoMarker4Label(void) const
{
    return "&4\tAlt+Q, 4";
}

QString cModernMenuProvider::GetEditGotoMarker5Label(void) const
{
    return "&5\tAlt+Q, 5";
}

QString cModernMenuProvider::GetEditGotoMarker6Label(void) const
{
    return "&6\tAlt+Q, 6";
}

QString cModernMenuProvider::GetEditGotoMarker7Label(void) const
{
    return "&7\tAlt+Q, 7";
}

QString cModernMenuProvider::GetEditGotoMarker8Label(void) const
{
    return "&8\tAlt+Q, 8";
}

QString cModernMenuProvider::GetEditGotoMarker9Label(void) const
{
    return "&9\tAlt+Q, 9";
}

QString cModernMenuProvider::GetEditGotoMarker0Label(void) const
{
    return "&0\tAlt+Q, 0";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Goto Font Tag menu label with Alt+Q, = shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoFontTagLabel(void) const
{
    return "&Font Tag\tAlt+Q, =";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the Edit Goto Style Tag menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoStyleTagLabel(void) const
{
    return "S&tyle Tag";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the Edit Goto Note menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoNoteLabel(void) const
{
    return "Note...";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Goto Previous Position menu label with Alt+Q, P shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoPreviousPositionLabel(void) const
{
    return "&Previous Position\tAlt+Q, P";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Goto Last Find/Replace menu label with Alt+Q, V shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoLastFindReplaceLabel(void) const
{
    return "&Last Find/Replace\tAlt+Q, V";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Goto Beginning of Block menu label with Alt+Q, B shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoBeginningOfBlockLabel(void) const
{
    return "Beginning of Block\tAlt+Q, B";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the Edit Goto End of Block menu label with Alt+Q, K shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoEndOfBlockLabel(void) const
{
    return "&End of Block\tAlt+Q, K";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Goto Document Start menu label with Ctrl+Home shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoDocumentStartLabel(void) const
{
    return "&Document Beginning\tCtrl+Home";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the Edit Goto Document End menu label with Ctrl+End shortcut.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoDocumentEndLabel(void) const
{
    return "D&ocument End\tCtrl+End";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the Edit Goto Scroll Up menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoScrollUpLabel(void) const
{
    return "Scroll Continuously &Up";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the Edit Goto Scroll Down menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditGotoScrollDownLabel(void) const
{
    return "&Scroll Continuously Down";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Set Marker
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the set marker labels with Alt+K prefix chord shortcuts.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditSetMarker1Label(void) const
{
    return "&1\tAlt+K, 1";
}

QString cModernMenuProvider::GetEditSetMarker2Label(void) const
{
    return "&2\tAlt+K, 2";
}

QString cModernMenuProvider::GetEditSetMarker3Label(void) const
{
    return "&3\tAlt+K, 3";
}

QString cModernMenuProvider::GetEditSetMarker4Label(void) const
{
    return "&4\tAlt+K, 4";
}

QString cModernMenuProvider::GetEditSetMarker5Label(void) const
{
    return "&5\tAlt+K, 5";
}

QString cModernMenuProvider::GetEditSetMarker6Label(void) const
{
    return "&6\tAlt+K, 6";
}

QString cModernMenuProvider::GetEditSetMarker7Label(void) const
{
    return "&7\tAlt+K, 7";
}

QString cModernMenuProvider::GetEditSetMarker8Label(void) const
{
    return "&8\tAlt+K, 8";
}

QString cModernMenuProvider::GetEditSetMarker9Label(void) const
{
    return "&9\tAlt+K, 9";
}

QString cModernMenuProvider::GetEditSetMarker0Label(void) const
{
    return "&0\tAlt+K, 0";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Notes
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the note-related menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditEditNoteLabel(void) const
{
    return "Edit &Note";
}

QString cModernMenuProvider::GetEditNoteConvertLabel(void) const
{
    return "&Convert Note...";
}

QString cModernMenuProvider::GetEditNoteEndnoteLocationLabel(void) const
{
    return "&Endnote Location";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Settings
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the settings-related menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetEditColumnBlockModeLabel(void) const
{
    return "&Column Block Mode";
}

QString cModernMenuProvider::GetEditColumnReplaceModeLabel(void) const
{
    return "Column &Replace Mode";
}

QString cModernMenuProvider::GetEditAutoAlignLabel(void) const
{
    return "&Auto Align";
}

QString cModernMenuProvider::GetEditCloseDialogLabel(void) const
{
    return "Close Dialog";
}

/////////////////////////////////////////////////////////////////////////////
// View Menu
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the view menu labels with CUA/Alt prefix shortcuts.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetViewPreviewLabel(void) const
{
    return "&Print Preview";
}

QString cModernMenuProvider::GetViewCommandTagsLabel(void) const
{
    return "Show &Formatting\tAlt+O, D";
}

QString cModernMenuProvider::GetViewBlockHighlightingLabel(void) const
{
    return "&Block Highlighting\tAlt+K, H";
}

QString cModernMenuProvider::GetViewScreenSettingsLabel(void) const
{
    return "&Screen Settings...";
}

QString cModernMenuProvider::GetViewSwitchModesLabel(void) const
{
    return "S&witch Modes\tAlt+O, T";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Basic
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the insert menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetInsertPageBreakLabel(void) const
{
    return "&Page Break";
}

QString cModernMenuProvider::GetInsertDateLabel(void) const
{
    return "&Today's Date Value";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Other Values
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the insert other values menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetInsertTimeLabel(void) const
{
    return "Current &Time";
}

QString cModernMenuProvider::GetInsertMathResultLabel(void) const
{
    return "Last &Math Result";
}

QString cModernMenuProvider::GetInsertMathExpressionLabel(void) const
{
    return "Last Math &Expression";
}

QString cModernMenuProvider::GetInsertMathDollarLabel(void) const
{
    return "&Last Math as Dollar";
}

QString cModernMenuProvider::GetInsertFilenameLabel(void) const
{
    return "Current &Filename";
}


QString cModernMenuProvider::GetInsertDirectoryLabel(void) const
{
    return "Current D&irectory";
}

QString cModernMenuProvider::GetInsertPathLabel(void) const
{
    return "Current P&ath";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Variables
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the insert variable menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetInsertVarDateLabel(void) const
{
    return "&Date";
}

QString cModernMenuProvider::GetInsertVarTimeLabel(void) const
{
    return "&Time";
}

QString cModernMenuProvider::GetInsertVarPageLabel(void) const
{
    return "&Page";
}


QString cModernMenuProvider::GetInsertVarFilenameLabel(void) const
{
    return "&Filename";
}

QString cModernMenuProvider::GetInsertVarDriveLabel(void) const
{
    return "Dri&ve";
}

QString cModernMenuProvider::GetInsertVarDirectoryLabel(void) const
{
    return "D&irectory";
}

QString cModernMenuProvider::GetInsertVarPathLabel(void) const
{
    return "P&ath";
}

QString cModernMenuProvider::GetInsertVarWordCountLabel(void) const
{
    return "&Word Count";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Extended/Files
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the insert extended/files menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetInsertExtendedCharLabel(void) const
{
    return "&Extended Character...";
}

QString cModernMenuProvider::GetInsertFileLabel(void) const
{
    return "&File...";
}

QString cModernMenuProvider::GetInsertFileAtPrintLabel(void) const
{
    return "Fi&le at Print Time...";
}

QString cModernMenuProvider::GetInsertGraphicLabel(void) const
{
    return "&Graphic...";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Notes
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the insert note menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetInsertNoteCommentLabel(void) const
{
    return "&Comment...";
}

QString cModernMenuProvider::GetInsertNoteFootnoteLabel(void) const
{
    return "&Footnote...";
}

QString cModernMenuProvider::GetInsertNoteEndnoteLabel(void) const
{
    return "&Endnote...";
}

QString cModernMenuProvider::GetInsertNoteAnnotationLabel(void) const
{
    return "&Annotation";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Index/TOC
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the insert index/TOC menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetInsertTOCEntryLabel(void) const
{
    return "&TOC Entry...";
}

QString cModernMenuProvider::GetInsertIndexEntryLabel(void) const
{
    return "&Index Entry...";
}

QString cModernMenuProvider::GetInsertDotLeaderLabel(void) const
{
    return "&Dot Leader to Tab";
}

QString cModernMenuProvider::GetInsertParOutlineNumberLabel(void) const
{
    return "Par. &Outline Number...";
}

/////////////////////////////////////////////////////////////////////////////
// Style Menu - Basic Styles
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the style menu labels with CUA or Alt prefix chord shortcuts.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetStyleBoldLabel(void) const
{
    return "&Bold\tCtrl+B";
}

QString cModernMenuProvider::GetStyleItalicLabel(void) const
{
    return "&Italic\tCtrl+I";
}

QString cModernMenuProvider::GetStyleUnderlineLabel(void) const
{
    return "&Underline\tCtrl+U";
}

QString cModernMenuProvider::GetStyleFontLabel(void) const
{
    return "&Font...\tCtrl+D";
}

QString cModernMenuProvider::GetStyleStrikeoutLabel(void) const
{
    return "&Strikeout\tAlt+P, X";
}

QString cModernMenuProvider::GetStyleSubscriptLabel(void) const
{
    return "Su&bscript\tAlt+P, V";
}

QString cModernMenuProvider::GetStyleSuperscriptLabel(void) const
{
    return "Su&perscript\tAlt+P, T";
}

QString cModernMenuProvider::GetStyleDoubleStrikeLabel(void) const
{
    return "&Doublestrike";
}

QString cModernMenuProvider::GetStyleColorLabel(void) const
{
    return "&Color...\tAlt+P, -";
}

/////////////////////////////////////////////////////////////////////////////
// Style Menu - Paragraph Styles
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the paragraph style menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetStyleSelectParStyleLabel(void) const
{
    return "&Select Paragraph Style...";
}

QString cModernMenuProvider::GetStyleReturnToPrevStyleLabel(void) const
{
    return "&Return to Previous Style";
}

QString cModernMenuProvider::GetStyleDefineParStyleLabel(void) const
{
    return "&Define Paragraph Style";
}

QString cModernMenuProvider::GetStyleCopyStyleToLibraryLabel(void) const
{
    return "&Copy Style to Library";
}

QString cModernMenuProvider::GetStyleDeleteLibraryStyleLabel(void) const
{
    return "&Delete Library Style";
}

QString cModernMenuProvider::GetStyleRenameLibraryStyleLabel(void) const
{
    return "&Rename Library Style";
}

QString cModernMenuProvider::GetStyleRenameDocStyleLabel(void) const
{
    return "R&ename Document Style";
}

/////////////////////////////////////////////////////////////////////////////
// Style Menu - Case
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the case conversion menu labels with Alt+K prefix chord shortcuts.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetStyleUppercaseLabel(void) const
{
    return "&Uppercase\tAlt+K, \"";
}

QString cModernMenuProvider::GetStyleLowercaseLabel(void) const
{
    return "&Lowercase\tAlt+K, '";
}

QString cModernMenuProvider::GetStyleSentenceCaseLabel(void) const
{
    return "&Sentence Case\tAlt+K, .";
}


/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Line Alignment
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with CUA shortcut
///
/// @brief
/// Returns the layout alignment menu labels with CUA shortcuts.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetLayoutCenterLineLabel(void) const
{
    return "&Center Paragraph\tCtrl+E";
}

QString cModernMenuProvider::GetLayoutRightAlignLabel(void) const
{
    return "R&ight Align Paragraph\tCtrl+R";
}

/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Paragraph Alignment
/////////////////////////////////////////////////////////////////////////////

QString cModernMenuProvider::GetLayoutLeftAlignParaLabel(void) const
{
    return "&Left Align Paragraph\tCtrl+L";
}

QString cModernMenuProvider::GetLayoutCenterParaLabel(void) const
{
    return "Ce&nter Paragraph\tCtrl+E";
}

QString cModernMenuProvider::GetLayoutRightAlignParaLabel(void) const
{
    return "Ri&ght Align Paragraph\tCtrl+R";
}

QString cModernMenuProvider::GetLayoutJustifyParaLabel(void) const
{
    return "&Justify Paragraph\tCtrl+J";
}

QString cModernMenuProvider::GetLayoutRulerLineLabel(void) const
{
    return "&Ruler Line...";
}

/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Page Layout
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the page layout menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetLayoutColumnsLabel(void) const
{
    return "C&olumns...";
}

QString cModernMenuProvider::GetLayoutPageLabel(void) const
{
    return "&Page...\tAlt+O, Y";
}

QString cModernMenuProvider::GetLayoutHeaderLabel(void) const
{
    return "&Header...";
}

QString cModernMenuProvider::GetLayoutFooterLabel(void) const
{
    return "&Footer...";
}

QString cModernMenuProvider::GetLayoutPageNumberingLabel(void) const
{
    return "Page &Numbering...";
}

QString cModernMenuProvider::GetLayoutLineNumberingLabel(void) const
{
    return "Line Numbering...";
}

QString cModernMenuProvider::GetLayoutAlignmentLabel(void) const
{
    return "&Alignment and Spacing";
}

/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Special
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the layout special menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetLayoutOverprintCharLabel(void) const
{
    return "Overprint &Character";
}

QString cModernMenuProvider::GetLayoutOverprintLineLabel(void) const
{
    return "Overprint &Line";
}

QString cModernMenuProvider::GetLayoutOptionalHyphenLabel(void) const
{
    return "Option &Hyphen";
}

QString cModernMenuProvider::GetLayoutVerticalCenterLabel(void) const
{
    return "&Vertically Center Text on Page";
}

QString cModernMenuProvider::GetLayoutKeepWordsTogetherLabel(void) const
{
    return "&Keep Word Together on Line";
}

QString cModernMenuProvider::GetLayoutKeepLinesTogetherPageLabel(void) const
{
    return "Keep Lines Together on &Page...";
}

QString cModernMenuProvider::GetLayoutKeepLinesTogetherColumnLabel(void) const
{
    return "Keep Lines Together on C&olumn";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Spell Check
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the spell check menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetUtilSpellCheckGlobalLabel(void) const
{
    return "&Spell Check Global";
}

QString cModernMenuProvider::GetUtilSpellCheckRestLabel(void) const
{
    return "&Rest of Document";
}

QString cModernMenuProvider::GetUtilSpellCheckWordLabel(void) const
{
    return "&Word";
}

QString cModernMenuProvider::GetUtilSpellCheckTypeLabel(void) const
{
    return "&Type Word...";
}


QString cModernMenuProvider::GetUtilThesaurusLabel(void) const
{
    return "&Thesaurus";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Tools
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the tools menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetUtilInsetLabel(void) const
{
    return "&Inset";
}

QString cModernMenuProvider::GetUtilCalculatorLabel(void) const
{
    return "&Calculator";
}

QString cModernMenuProvider::GetUtilBlockMathLabel(void) const
{
    return "&Block Math";
}

QString cModernMenuProvider::GetUtilSortBlockAscLabel(void) const
{
    return "&Ascending";
}

QString cModernMenuProvider::GetUtilSortBlockDesLabel(void) const
{
    return "&Descending";
}

QString cModernMenuProvider::GetUtilWordCountLabel(void) const
{
    return "&Word Count\tAlt+K, ?";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Macros
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the macro menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetUtilPlayMacroLabel(void) const
{
    return "&Play...";
}

QString cModernMenuProvider::GetUtilRecordMacroLabel(void) const
{
    return "&Record...";
}

QString cModernMenuProvider::GetUtilEditMacroLabel(void) const
{
    return "&Edit/Create...";
}

QString cModernMenuProvider::GetUtilSingleStepLabel(void) const
{
    return "&Single Step...";
}

QString cModernMenuProvider::GetUtilCopyMacroLabel(void) const
{
    return "&Copy...";
}

QString cModernMenuProvider::GetUtilDeleteMacroLabel(void) const
{
    return "&Delete...";
}

QString cModernMenuProvider::GetUtilRenameMacroLabel(void) const
{
    return "Re&name...";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Merge/Variables
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the merge/variable menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetUtilDataFileLabel(void) const
{
    return "&Data File...";
}

QString cModernMenuProvider::GetUtilNameVarsLabel(void) const
{
    return "&Name Variables...";
}

QString cModernMenuProvider::GetUtilSetVarLabel(void) const
{
    return "&Set Variable...";
}

QString cModernMenuProvider::GetUtilSetVarMathLabel(void) const
{
    return "Set &Variable to Math Result...";
}

QString cModernMenuProvider::GetUtilAskVarLabel(void) const
{
    return "&Ask for Variable...";
}

QString cModernMenuProvider::GetUtilIfLabel(void) const
{
    return "&If...";
}

QString cModernMenuProvider::GetUtilElseLabel(void) const
{
    return "E&lse";
}

QString cModernMenuProvider::GetUtilEndIfLabel(void) const
{
    return "&End If";
}

QString cModernMenuProvider::GetUtilTopLabel(void) const
{
    return "Go to &Top of Document";
}

QString cModernMenuProvider::GetUtilBottomLabel(void) const
{
    return "Go to &Bottom of Document";
}

QString cModernMenuProvider::GetUtilClearLabel(void) const
{
    return "&Clear Screen While Printing...";
}

QString cModernMenuProvider::GetUtilDisplayLabel(void) const
{
    return "Display &Message...";
}

QString cModernMenuProvider::GetUtilPrintNTimesLabel(void) const
{
    return "P&rint File n Times...";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Reformat
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label with Alt prefix chord shortcut
///
/// @brief
/// Returns the reformat menu labels.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetUtilReformatRestLabel(void) const
{
    return "&Rest of Document\tAlt+Q, U";
}


QString cModernMenuProvider::GetUtilReformatNotesLabel(void) const
{
    return "Rest of &Notes";
}

/////////////////////////////////////////////////////////////////////////////
// Help Menu
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - menu label
///
/// @brief
/// Returns the Help About menu label.
///
/////////////////////////////////////////////////////////////////////////////
QString cModernMenuProvider::GetHelpAboutLabel(void) const
{
    return "&About WordTsar";
}
