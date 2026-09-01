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
 * @class cWordStarMenuProvider
 *
 * @brief WordStar-style menu label provider for the GUI menu bar.
 *
 * Implements the cWordStarMenuProvider class, which supplies the display
 * labels and keyboard shortcut annotations for every menu item in the
 * application. Implements the IMenuProvider interface.
 *
 * @section wsmenu_notation Shortcut Notation
 * Labels use WordStar control-key notation where caret (^) represents the
 * Ctrl key. Examples:
 * - ^KR: Open file (Ctrl+K, R)
 * - ^KS: Save file (Ctrl+K, S)
 * - ^QF: Find text (Ctrl+Q, F)
 * - ^PB: Bold toggle (Ctrl+P, B)
 *
 * @section wsmenu_menus Menu Coverage
 * Provides labels for all seven menus:
 * - File: New, Open, Save, Save As, Close, Quit
 * - Edit: Undo, Redo, Cut, Copy, Paste, Find, Replace, Goto
 * - View: Display modes, control codes, ruler, reveal codes
 * - Style: Bold, Italic, Underline, Strikethrough, Super/Subscript, Font, Color
 * - Layout: Page layout, margins, columns, headers/footers
 * - Utilities: Spell check, word count, preferences, about
 * - Help: WordStar help panels
 *
 * @section wsmenu_extensibility Extensibility
 * The IMenuProvider interface allows alternative menu providers with different
 * shortcut schemes (e.g., CUA-style Ctrl+O/Ctrl+S) to be substituted.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cWordStarMenuProvider WordStar menu provider class
 * @see IMenuProvider Menu provider interface
 * @see cWordTsar Main application window using the menu provider
 */

#include "src/gui/menu/wordstarmenu.h"

/////////////////////////////////////////////////////////////////////////////
// File Menu
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetFileOpenLabel(void) const
{
    return "&Open/Read\t^KR";
}

QString cWordStarMenuProvider::GetFileSaveLabel(void) const
{
    return "&Save\t^KS";
}

QString cWordStarMenuProvider::GetFileSaveAsLabel(void) const
{
    return "Save &As...\t^KT";
}

QString cWordStarMenuProvider::GetFileSaveAndCloseLabel(void) const
{
    return "Save and &Close\t^KD";
}

QString cWordStarMenuProvider::GetFileCloseLabel(void) const
{
    return "";
}

QString cWordStarMenuProvider::GetFilePrintLabel(void) const
{
    return "Print...\t^KP";
}

QString cWordStarMenuProvider::GetFilePrintPreviewLabel(void) const
{
    return "Print Preview...\t^OP";
}

QString cWordStarMenuProvider::GetFileExitLabel(void) const
{
    return "E&xit WordTsar\t^KX";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Basic
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditUndoLabel(void) const
{
    return "&Undo\t^U";
}

QString cWordStarMenuProvider::GetEditRedoLabel(void) const
{
    return "&Redo\tCtrl+Alt+U";
}

QString cWordStarMenuProvider::GetEditMarkBlockStartLabel(void) const
{
    return "Mark Block &Beginning\t^KB";
}

QString cWordStarMenuProvider::GetEditMarkBlockEndLabel(void) const
{
    return "Mark Block &End\t^KK";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Move
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditMoveBlockLabel(void) const
{
    return "&Block\t^KV";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Copy
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditCopyBlockLabel(void) const
{
    return "&Block\t^KC";
}

QString cWordStarMenuProvider::GetEditCopyFromClipboardLabel(void) const
{
    return "&From Clipboard\t^K[";
}

QString cWordStarMenuProvider::GetEditCopyToClipboardLabel(void) const
{
    return "&To Clipboard\t^K]";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Delete
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditDeleteBlockLabel(void) const
{
    return "&Block\t^KY";
}

QString cWordStarMenuProvider::GetEditDeleteWordLabel(void) const
{
    return "&Word\t^T";
}

QString cWordStarMenuProvider::GetEditDeleteLineLabel(void) const
{
    return "&Line\t^Y";
}

QString cWordStarMenuProvider::GetEditDeleteLineLeftLabel(void) const
{
    return "L&ine Left of Cursor\t^QDel";
}

QString cWordStarMenuProvider::GetEditDeleteLineRightLabel(void) const
{
    return "Line &Right of Cursor\t^QY";
}

QString cWordStarMenuProvider::GetEditDeleteToCharLabel(void) const
{
    return "&To Character...\t^QT";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Find/Replace
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditMarkPreviousBlockLabel(void) const
{
    return "Mark &Previous Block\t^KU";
}

QString cWordStarMenuProvider::GetEditFindLabel(void) const
{
    return "&Find\t^QF";
}

QString cWordStarMenuProvider::GetEditFindAndReplaceLabel(void) const
{
    return "Find and &Replace...\t^QA";
}

QString cWordStarMenuProvider::GetEditFindNextLabel(void) const
{
    return "Find Ne&xt\t^L";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Goto
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditGotoCharLabel(void) const
{
    return "&Go to Character...\t^QG";
}

QString cWordStarMenuProvider::GetEditGotoPageLabel(void) const
{
    return "Goto &Page...\t^QI";
}

QString cWordStarMenuProvider::GetEditGotoMarker1Label(void) const
{
    return "&1\t^Q1";
}

QString cWordStarMenuProvider::GetEditGotoMarker2Label(void) const
{
    return "&2\t^Q2";
}

QString cWordStarMenuProvider::GetEditGotoMarker3Label(void) const
{
    return "&3\t^Q3";
}

QString cWordStarMenuProvider::GetEditGotoMarker4Label(void) const
{
    return "&4\t^Q4";
}

QString cWordStarMenuProvider::GetEditGotoMarker5Label(void) const
{
    return "&5\t^Q5";
}

QString cWordStarMenuProvider::GetEditGotoMarker6Label(void) const
{
    return "&6\t^Q6";
}

QString cWordStarMenuProvider::GetEditGotoMarker7Label(void) const
{
    return "&7\t^Q7";
}

QString cWordStarMenuProvider::GetEditGotoMarker8Label(void) const
{
    return "&8\t^Q8";
}

QString cWordStarMenuProvider::GetEditGotoMarker9Label(void) const
{
    return "&9\t^Q9";
}

QString cWordStarMenuProvider::GetEditGotoMarker0Label(void) const
{
    return "&0\t^Q0";
}

QString cWordStarMenuProvider::GetEditGotoFontTagLabel(void) const
{
    return "&Font Tag\t^Q=";
}

QString cWordStarMenuProvider::GetEditGotoStyleTagLabel(void) const
{
    return "S&tyle Tag\t^Q<";
}

QString cWordStarMenuProvider::GetEditGotoNoteLabel(void) const
{
    return "Note...\t^ONG";
}

QString cWordStarMenuProvider::GetEditGotoPreviousPositionLabel(void) const
{
    return "&Previous Position\t^QP";
}

QString cWordStarMenuProvider::GetEditGotoLastFindReplaceLabel(void) const
{
    return "&Last Find/Replace\t^QV";
}

QString cWordStarMenuProvider::GetEditGotoBeginningOfBlockLabel(void) const
{
    return "Beginning of Block\t^QB";
}

QString cWordStarMenuProvider::GetEditGotoEndOfBlockLabel(void) const
{
    return "&End of Block\t^QK";
}

QString cWordStarMenuProvider::GetEditGotoDocumentStartLabel(void) const
{
    return "&Document Beginning\t^QR";
}

QString cWordStarMenuProvider::GetEditGotoDocumentEndLabel(void) const
{
    return "D&ocument End\t^QC";
}

QString cWordStarMenuProvider::GetEditGotoScrollUpLabel(void) const
{
    return "Scroll Continuously &Up\t^QW";
}

QString cWordStarMenuProvider::GetEditGotoScrollDownLabel(void) const
{
    return "&Scroll COntinuously Down\t^QZ";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Set Marker
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditSetMarker1Label(void) const
{
    return "&1\t^K1";
}

QString cWordStarMenuProvider::GetEditSetMarker2Label(void) const
{
    return "&2\t^K2";
}

QString cWordStarMenuProvider::GetEditSetMarker3Label(void) const
{
    return "&3\t^K3";
}

QString cWordStarMenuProvider::GetEditSetMarker4Label(void) const
{
    return "&4\t^K4";
}

QString cWordStarMenuProvider::GetEditSetMarker5Label(void) const
{
    return "&5\t^K5";
}

QString cWordStarMenuProvider::GetEditSetMarker6Label(void) const
{
    return "&6\t^K6";
}

QString cWordStarMenuProvider::GetEditSetMarker7Label(void) const
{
    return "&7\t^K7";
}

QString cWordStarMenuProvider::GetEditSetMarker8Label(void) const
{
    return "&8\t^K8";
}

QString cWordStarMenuProvider::GetEditSetMarker9Label(void) const
{
    return "&9\t^K9";
}

QString cWordStarMenuProvider::GetEditSetMarker0Label(void) const
{
    return "&0\t^K0";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Notes
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditEditNoteLabel(void) const
{
    return "Edit &Note\t^OND";
}

QString cWordStarMenuProvider::GetEditNoteStartNumberLabel(void) const
{
    return "&Starting Number for Note...";
}

QString cWordStarMenuProvider::GetEditNoteConvertLabel(void) const
{
    return "&Convert Note...\t^ONV";
}

QString cWordStarMenuProvider::GetEditNoteConvertPrintLabel(void) const
{
    return "Convert at &Print...\t.cv";
}

QString cWordStarMenuProvider::GetEditNoteEndnoteLocationLabel(void) const
{
    return "&Endnote Location\t.pe";
}

/////////////////////////////////////////////////////////////////////////////
// Edit Menu - Settings
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetEditColumnBlockModeLabel(void) const
{
    return "&Column Block Mode\t^KN";
}

QString cWordStarMenuProvider::GetEditColumnReplaceModeLabel(void) const
{
    return "Column &Replace Mode\t^KI";
}

QString cWordStarMenuProvider::GetEditAutoAlignLabel(void) const
{
    return "&Auto Align\t^OA";
}

QString cWordStarMenuProvider::GetEditCloseDialogLabel(void) const
{
    return "↵ Closes Dialog\t^O↵";
}

/////////////////////////////////////////////////////////////////////////////
// View Menu
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetViewPreviewLabel(void) const
{
    return "&Print Preview\t^OP";
}

QString cWordStarMenuProvider::GetViewCommandTagsLabel(void) const
{
    return "&Command Tags\t^OD";
}

QString cWordStarMenuProvider::GetViewBlockHighlightingLabel(void) const
{
    return "&Block Highlighting\t^KH";
}

QString cWordStarMenuProvider::GetViewScreenSettingsLabel(void) const
{
    return "&Screen Settings...\t^OB";
}

QString cWordStarMenuProvider::GetViewSwitchModesLabel(void) const
{
    return "S&witch Modes\t^OT";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Basic
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetInsertPageBreakLabel(void) const
{
    return "&Page Break\t.pa";
}

QString cWordStarMenuProvider::GetInsertColumnBreakLabel(void) const
{
    return "&Column Break\t.cb";
}

QString cWordStarMenuProvider::GetInsertDateLabel(void) const
{
    return "&Today's Date Value\t^M@";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Other Values
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetInsertTimeLabel(void) const
{
    return "Current &Time\t^M!";
}

QString cWordStarMenuProvider::GetInsertMathResultLabel(void) const
{
    return "Last &Math Result\t^M=";
}

QString cWordStarMenuProvider::GetInsertMathExpressionLabel(void) const
{
    return "Last Math &Expression\t^M#";
}

QString cWordStarMenuProvider::GetInsertMathDollarLabel(void) const
{
    return "&Last Math as Dollar\t^M$";
}

QString cWordStarMenuProvider::GetInsertFilenameLabel(void) const
{
    return "Current &Filename\t^M*";
}

QString cWordStarMenuProvider::GetInsertDriveLabel(void) const
{
    return "Current &Drive\t^M:";
}

QString cWordStarMenuProvider::GetInsertDirectoryLabel(void) const
{
    return "Current D&irectory\t^M.";
}

QString cWordStarMenuProvider::GetInsertPathLabel(void) const
{
    return "Current P&ath\t^M\\";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Variables
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetInsertVarDateLabel(void) const
{
    return "&Date\t&&@&&";
}

QString cWordStarMenuProvider::GetInsertVarTimeLabel(void) const
{
    return "&Time\t&&!&&";
}

QString cWordStarMenuProvider::GetInsertVarPageLabel(void) const
{
    return "&Page\t&&#&&";
}

QString cWordStarMenuProvider::GetInsertVarLineLabel(void) const
{
    return "&Line\t&&_&&";
}

QString cWordStarMenuProvider::GetInsertVarFilenameLabel(void) const
{
    return "&Filename\t&&*&&";
}

QString cWordStarMenuProvider::GetInsertVarDriveLabel(void) const
{
    return "Dri&ve\t&&:&&";
}

QString cWordStarMenuProvider::GetInsertVarDirectoryLabel(void) const
{
    return "D&irectory\t&&.&&";
}

QString cWordStarMenuProvider::GetInsertVarPathLabel(void) const
{
    return "P&ath\t&&\\&&";
}

QString cWordStarMenuProvider::GetInsertVarWordCountLabel(void) const
{
    return "&Word Count\t&&?&&";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Extended/Files
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetInsertExtendedCharLabel(void) const
{
    return "&Extended Character...\t^PO";
}

QString cWordStarMenuProvider::GetInsertFileLabel(void) const
{
    return "&File...\t^KR";
}

QString cWordStarMenuProvider::GetInsertFileAtPrintLabel(void) const
{
    return "Fi&le at Print Time...\t.fi";
}

QString cWordStarMenuProvider::GetInsertGraphicLabel(void) const
{
    return "&Graphic...\t^P*";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Notes
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetInsertNoteCommentLabel(void) const
{
    return "&Comment...\t^ONC";
}

QString cWordStarMenuProvider::GetInsertNoteFootnoteLabel(void) const
{
    return "&Footnote...\t^ONF";
}

QString cWordStarMenuProvider::GetInsertNoteEndnoteLabel(void) const
{
    return "&Endnote...\t^ONE";
}

QString cWordStarMenuProvider::GetInsertNoteAnnotationLabel(void) const
{
    return "&Annotation\t^ONA";
}

/////////////////////////////////////////////////////////////////////////////
// Insert Menu - Index/TOC
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetInsertTOCEntryLabel(void) const
{
    return "&TOC Entry...\t.tc";
}

QString cWordStarMenuProvider::GetInsertIndexEntryLabel(void) const
{
    return "&Index Entry...\t^ONI";
}

QString cWordStarMenuProvider::GetInsertMarkTextForIndexLabel(void) const
{
    return "&Mark Text for Index\t^PK";
}

QString cWordStarMenuProvider::GetInsertDotLeaderLabel(void) const
{
    return "&Dot Leader to Tab\t^P.";
}

QString cWordStarMenuProvider::GetInsertParOutlineNumberLabel(void) const
{
    return "Par. &Outline Number...\t^OZ";
}

/////////////////////////////////////////////////////////////////////////////
// Style Menu - Basic Styles
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetStyleBoldLabel(void) const
{
    return "&Bold\t^PB";
}

QString cWordStarMenuProvider::GetStyleItalicLabel(void) const
{
    return "&Italic\t^PY";
}

QString cWordStarMenuProvider::GetStyleUnderlineLabel(void) const
{
    return "&Underline\t^PS";
}

QString cWordStarMenuProvider::GetStyleFontLabel(void) const
{
    return "&Font\t^P=";
}

QString cWordStarMenuProvider::GetStyleStrikeoutLabel(void) const
{
    return "&Strikeout\t^PX";
}

QString cWordStarMenuProvider::GetStyleSubscriptLabel(void) const
{
    return "Su&bscript\t^PV";
}

QString cWordStarMenuProvider::GetStyleSuperscriptLabel(void) const
{
    return "Su&perscript\t^PT";
}

QString cWordStarMenuProvider::GetStyleDoubleStrikeLabel(void) const
{
    return "&Doublestrike\t^PD";
}

QString cWordStarMenuProvider::GetStyleColorLabel(void) const
{
    return "&Color...\t^P-";
}

/////////////////////////////////////////////////////////////////////////////
// Style Menu - Paragraph Styles
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetStyleSelectParStyleLabel(void) const
{
    return "&Select Paragraph Style...\t^OFS";
}

QString cWordStarMenuProvider::GetStyleReturnToPrevStyleLabel(void) const
{
    return "&Return to Previous Style\t^OFP";
}

QString cWordStarMenuProvider::GetStyleDefineParStyleLabel(void) const
{
    return "&Defne Paragraph Style\t^OFD";
}

QString cWordStarMenuProvider::GetStyleCopyStyleToLibraryLabel(void) const
{
    return "&Copy Style to Library\t^OFO";
}

QString cWordStarMenuProvider::GetStyleDeleteLibraryStyleLabel(void) const
{
    return "&Delete Library Style\t^OFY";
}

QString cWordStarMenuProvider::GetStyleRenameLibraryStyleLabel(void) const
{
    return "&Rename Library Style\t^OFR";
}

QString cWordStarMenuProvider::GetStyleRenameDocStyleLabel(void) const
{
    return "R&ename Document Style\t^OFE";
}

/////////////////////////////////////////////////////////////////////////////
// Style Menu - Case
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetStyleUppercaseLabel(void) const
{
    return "&Uppercase\t^K\"";
}

QString cWordStarMenuProvider::GetStyleLowercaseLabel(void) const
{
    return "&Lowercase\t^K\'";
}

QString cWordStarMenuProvider::GetStyleSentenceCaseLabel(void) const
{
    return "&Sentence Case\t^K.";
}

QString cWordStarMenuProvider::GetStyleSettingsLabel(void) const
{
    return "S&ettings";
}

/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Line Alignment
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetLayoutCenterLineLabel(void) const
{
    return "&Center Line\t^OC";
}

QString cWordStarMenuProvider::GetLayoutRightAlignLabel(void) const
{
    return "R&ight Align Line\t^OJ";
}

/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Paragraph Alignment
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetLayoutLeftAlignParaLabel(void) const
{
    return "&Left Align Paragraph\t^O<";
}

QString cWordStarMenuProvider::GetLayoutCenterParaLabel(void) const
{
    return "Ce&nter Paragraph\t^O=";
}

QString cWordStarMenuProvider::GetLayoutRightAlignParaLabel(void) const
{
    return "Ri&ght Align Paragraph\t^O>";
}

QString cWordStarMenuProvider::GetLayoutJustifyParaLabel(void) const
{
    return "&Justify Paragraph\t^O+";
}

QString cWordStarMenuProvider::GetLayoutRulerLineLabel(void) const
{
    return "&Ruler Line...\t^OL";
}

/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Page Layout
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetLayoutColumnsLabel(void) const
{
    return "C&olumns...\t^OU";
}

QString cWordStarMenuProvider::GetLayoutPageLabel(void) const
{
    return "&Page...\t^OY";
}

QString cWordStarMenuProvider::GetLayoutHeaderLabel(void) const
{
    return "&Header...\t.he";
}

QString cWordStarMenuProvider::GetLayoutFooterLabel(void) const
{
    return "&Footer...\t.fo";
}

QString cWordStarMenuProvider::GetLayoutPageNumberingLabel(void) const
{
    return "Page &Numbering...\t^O#";
}

QString cWordStarMenuProvider::GetLayoutLineNumberingLabel(void) const
{
    return "Line Numbering...\t.l#";
}

QString cWordStarMenuProvider::GetLayoutAlignmentLabel(void) const
{
    return "&Alignment and Spacing\t^OS";
}

/////////////////////////////////////////////////////////////////////////////
// Layout Menu - Special
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetLayoutOverprintCharLabel(void) const
{
    return "Overprint &Character\t^PH";
}

QString cWordStarMenuProvider::GetLayoutOverprintLineLabel(void) const
{
    return "Overprint &Line\t^P↵";
}

QString cWordStarMenuProvider::GetLayoutOptionalHyphenLabel(void) const
{
    return "Option &Hyphen\t^OE";
}

QString cWordStarMenuProvider::GetLayoutVerticalCenterLabel(void) const
{
    return "&Vertically Center Text on Page\t^OV";
}

QString cWordStarMenuProvider::GetLayoutKeepWordsTogetherLabel(void) const
{
    return "&Keep Word Together on Line\t^PO";
}

QString cWordStarMenuProvider::GetLayoutKeepLinesTogetherPageLabel(void) const
{
    return "Keep Lines Together on &Page...\t.cp";
}

QString cWordStarMenuProvider::GetLayoutKeepLinesTogetherColumnLabel(void) const
{
    return "Keep Lines Together on C&olumn\t.cc";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Spell Check
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetUtilSpellCheckGlobalLabel(void) const
{
    return "&Spell Check Global\t^QR^QL";
}

QString cWordStarMenuProvider::GetUtilSpellCheckRestLabel(void) const
{
    return "&Rest of Document\t^QL";
}

QString cWordStarMenuProvider::GetUtilSpellCheckWordLabel(void) const
{
    return "&Word\t^QN";
}

QString cWordStarMenuProvider::GetUtilSpellCheckTypeLabel(void) const
{
    return "&Type Word...\t^QO";
}

QString cWordStarMenuProvider::GetUtilSpellCheckNotesLabel(void) const
{
    return "Rest of &Notes\t^ONL";
}

QString cWordStarMenuProvider::GetUtilThesaurusLabel(void) const
{
    return "&Thesaurus\t^QJ";
}

QString cWordStarMenuProvider::GetUtilLanguageChangeLabel(void) const
{
    return "Language Change...\t.la";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Tools
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetUtilInsetLabel(void) const
{
    return "&Inset\t^P&&";
}

QString cWordStarMenuProvider::GetUtilCalculatorLabel(void) const
{
    return "&Calculator\t^QM";
}

QString cWordStarMenuProvider::GetUtilBlockMathLabel(void) const
{
    return "&Block Math\t^KM";
}

QString cWordStarMenuProvider::GetUtilSortBlockAscLabel(void) const
{
    return "&Ascending\t^KZA";
}

QString cWordStarMenuProvider::GetUtilSortBlockDesLabel(void) const
{
    return "&Descending\t^KZD";
}

QString cWordStarMenuProvider::GetUtilWordCountLabel(void) const
{
    return "&Word Count\t^K?";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Macros
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetUtilPlayMacroLabel(void) const
{
    return "&Play...\t^MP";
}

QString cWordStarMenuProvider::GetUtilRecordMacroLabel(void) const
{
    return "&Record...\t^MR";
}

QString cWordStarMenuProvider::GetUtilEditMacroLabel(void) const
{
    return "&Edit/Create...\t^MD";
}

QString cWordStarMenuProvider::GetUtilSingleStepLabel(void) const
{
    return "&Single Step...\t^MS";
}

QString cWordStarMenuProvider::GetUtilCopyMacroLabel(void) const
{
    return "&Copy...\t^MO";
}

QString cWordStarMenuProvider::GetUtilDeleteMacroLabel(void) const
{
    return "&Delete...\t^MY";
}

QString cWordStarMenuProvider::GetUtilRenameMacroLabel(void) const
{
    return "Re&name...\t^ME";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Merge/Variables
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetUtilDataFileLabel(void) const
{
    return "&Data File...\t.df";
}

QString cWordStarMenuProvider::GetUtilNameVarsLabel(void) const
{
    return "&Name Variables...\t.rv";
}

QString cWordStarMenuProvider::GetUtilSetVarLabel(void) const
{
    return "&Set Variable...\t.sv";
}

QString cWordStarMenuProvider::GetUtilSetVarMathLabel(void) const
{
    return "Set &Variable to Math Result...\t.ma";
}

QString cWordStarMenuProvider::GetUtilAskVarLabel(void) const
{
    return "&Ask for Variable...\t.av";
}

QString cWordStarMenuProvider::GetUtilIfLabel(void) const
{
    return "&If...\t.if";
}

QString cWordStarMenuProvider::GetUtilElseLabel(void) const
{
    return "E&lse\t.el";
}

QString cWordStarMenuProvider::GetUtilEndIfLabel(void) const
{
    return "&End If\t.ei";
}

QString cWordStarMenuProvider::GetUtilTopLabel(void) const
{
    return "Go to &Top of Document\t.go t";
}

QString cWordStarMenuProvider::GetUtilBottomLabel(void) const
{
    return "Go to &Bottom of Document\t.go b";
}

QString cWordStarMenuProvider::GetUtilClearLabel(void) const
{
    return "&Clear Screen While Printing...\t.cs";
}

QString cWordStarMenuProvider::GetUtilDisplayLabel(void) const
{
    return "Display &Message...\t.dm";
}

QString cWordStarMenuProvider::GetUtilPrintNTimesLabel(void) const
{
    return "P&rint File n Times...\t.rp";
}

/////////////////////////////////////////////////////////////////////////////
// Utilities Menu - Reformat
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetUtilReformatRestLabel(void) const
{
    return "&Rest of Document\t^QU";
}

QString cWordStarMenuProvider::GetUtilReformatParaLabel(void) const
{
    return "&Paragraph\t^B";
}

QString cWordStarMenuProvider::GetUtilReformatNotesLabel(void) const
{
    return "Resst of &Notes\t^ONU";
}

QString cWordStarMenuProvider::GetUtilRepeatKeyLabel(void) const
{
    return "R&epeat Next Keystroke\t^QQ";
}

/////////////////////////////////////////////////////////////////////////////
// Help Menu
/////////////////////////////////////////////////////////////////////////////

QString cWordStarMenuProvider::GetHelpAboutLabel(void) const
{
    return "&About WordTsar";
}
