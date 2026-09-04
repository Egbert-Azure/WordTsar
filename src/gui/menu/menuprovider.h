#ifndef MENUPROVIDER_H
#define MENUPROVIDER_H

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

#include <QString>

/////////////////////////////////////////////////////////////////////////////
///
/// @class IMenuProvider
///
/// @brief
/// Interface for providing menu labels with keyboard shortcuts.
/// Implements Strategy Pattern to support multiple input modes
/// (WordStar, WordPerfect, MS Word, etc.)
///
/// Each input mode has different keyboard shortcuts, so menus must
/// display the correct shortcuts for the active mode.
///
/////////////////////////////////////////////////////////////////////////////
class IMenuProvider
{
public:
    virtual ~IMenuProvider(void) = default;

    // File Menu
    virtual QString GetFileOpenLabel(void) const = 0;
    virtual QString GetFileSaveLabel(void) const = 0;
    virtual QString GetFileSaveAsLabel(void) const = 0;
    virtual QString GetFileSaveAndCloseLabel(void) const = 0;
    virtual QString GetFileCloseLabel(void) const = 0;
    virtual QString GetFilePrintLabel(void) const = 0;
    virtual QString GetFilePrintPreviewLabel(void) const = 0;
    virtual QString GetFileExitLabel(void) const = 0;

    // Edit Menu - Basic
    virtual QString GetEditUndoLabel(void) const = 0;
    virtual QString GetEditRedoLabel(void) const = 0;
    virtual QString GetEditMarkBlockStartLabel(void) const = 0;
    virtual QString GetEditMarkBlockEndLabel(void) const = 0;

    // Edit Menu - Move
    virtual QString GetEditMoveBlockLabel(void) const = 0;

    // Edit Menu - Copy
    virtual QString GetEditCopyBlockLabel(void) const = 0;
    virtual QString GetEditCopyFromClipboardLabel(void) const = 0;
    virtual QString GetEditCopyToClipboardLabel(void) const = 0;

    // Edit Menu - Delete
    virtual QString GetEditDeleteBlockLabel(void) const = 0;
    virtual QString GetEditDeleteWordLabel(void) const = 0;
    virtual QString GetEditDeleteLineLabel(void) const = 0;
    virtual QString GetEditDeleteLineLeftLabel(void) const = 0;
    virtual QString GetEditDeleteLineRightLabel(void) const = 0;
    virtual QString GetEditDeleteToCharLabel(void) const = 0;

    // Edit Menu - Find/Replace
    virtual QString GetEditMarkPreviousBlockLabel(void) const = 0;
    virtual QString GetEditFindLabel(void) const = 0;
    virtual QString GetEditFindAndReplaceLabel(void) const = 0;
    virtual QString GetEditFindNextLabel(void) const = 0;

    // Edit Menu - Goto
    virtual QString GetEditGotoCharLabel(void) const = 0;
    virtual QString GetEditGotoPageLabel(void) const = 0;
    virtual QString GetEditGotoMarker1Label(void) const = 0;
    virtual QString GetEditGotoMarker2Label(void) const = 0;
    virtual QString GetEditGotoMarker3Label(void) const = 0;
    virtual QString GetEditGotoMarker4Label(void) const = 0;
    virtual QString GetEditGotoMarker5Label(void) const = 0;
    virtual QString GetEditGotoMarker6Label(void) const = 0;
    virtual QString GetEditGotoMarker7Label(void) const = 0;
    virtual QString GetEditGotoMarker8Label(void) const = 0;
    virtual QString GetEditGotoMarker9Label(void) const = 0;
    virtual QString GetEditGotoMarker0Label(void) const = 0;
    virtual QString GetEditGotoFontTagLabel(void) const = 0;
    virtual QString GetEditGotoStyleTagLabel(void) const = 0;
    virtual QString GetEditGotoNoteLabel(void) const = 0;
    virtual QString GetEditGotoPreviousPositionLabel(void) const = 0;
    virtual QString GetEditGotoLastFindReplaceLabel(void) const = 0;
    virtual QString GetEditGotoBeginningOfBlockLabel(void) const = 0;
    virtual QString GetEditGotoEndOfBlockLabel(void) const = 0;
    virtual QString GetEditGotoDocumentStartLabel(void) const = 0;
    virtual QString GetEditGotoDocumentEndLabel(void) const = 0;
    virtual QString GetEditGotoScrollUpLabel(void) const = 0;
    virtual QString GetEditGotoScrollDownLabel(void) const = 0;

    // Edit Menu - Set Marker
    virtual QString GetEditSetMarker1Label(void) const = 0;
    virtual QString GetEditSetMarker2Label(void) const = 0;
    virtual QString GetEditSetMarker3Label(void) const = 0;
    virtual QString GetEditSetMarker4Label(void) const = 0;
    virtual QString GetEditSetMarker5Label(void) const = 0;
    virtual QString GetEditSetMarker6Label(void) const = 0;
    virtual QString GetEditSetMarker7Label(void) const = 0;
    virtual QString GetEditSetMarker8Label(void) const = 0;
    virtual QString GetEditSetMarker9Label(void) const = 0;
    virtual QString GetEditSetMarker0Label(void) const = 0;

    // Edit Menu - Notes
    virtual QString GetEditEditNoteLabel(void) const = 0;
    virtual QString GetEditNoteConvertLabel(void) const = 0;
    virtual QString GetEditNoteEndnoteLocationLabel(void) const = 0;

    // Edit Menu - Settings
    virtual QString GetEditColumnBlockModeLabel(void) const = 0;
    virtual QString GetEditColumnReplaceModeLabel(void) const = 0;
    virtual QString GetEditAutoAlignLabel(void) const = 0;
    virtual QString GetEditCloseDialogLabel(void) const = 0;

    // View Menu
    virtual QString GetViewPreviewLabel(void) const = 0;
    virtual QString GetViewCommandTagsLabel(void) const = 0;
    virtual QString GetViewBlockHighlightingLabel(void) const = 0;
    virtual QString GetViewScreenSettingsLabel(void) const = 0;
    virtual QString GetViewSwitchModesLabel(void) const = 0;

    // Insert Menu - Basic
    virtual QString GetInsertPageBreakLabel(void) const = 0;
    virtual QString GetInsertDateLabel(void) const = 0;

    // Insert Menu - Other Values
    virtual QString GetInsertTimeLabel(void) const = 0;
    virtual QString GetInsertMathResultLabel(void) const = 0;
    virtual QString GetInsertMathExpressionLabel(void) const = 0;
    virtual QString GetInsertMathDollarLabel(void) const = 0;
    virtual QString GetInsertFilenameLabel(void) const = 0;
    virtual QString GetInsertDirectoryLabel(void) const = 0;
    virtual QString GetInsertPathLabel(void) const = 0;

    // Insert Menu - Variables
    virtual QString GetInsertVarDateLabel(void) const = 0;
    virtual QString GetInsertVarTimeLabel(void) const = 0;
    virtual QString GetInsertVarPageLabel(void) const = 0;
    virtual QString GetInsertVarFilenameLabel(void) const = 0;
    virtual QString GetInsertVarDriveLabel(void) const = 0;
    virtual QString GetInsertVarDirectoryLabel(void) const = 0;
    virtual QString GetInsertVarPathLabel(void) const = 0;
    virtual QString GetInsertVarWordCountLabel(void) const = 0;

    // Insert Menu - Extended/Files
    virtual QString GetInsertExtendedCharLabel(void) const = 0;
    virtual QString GetInsertFileLabel(void) const = 0;
    virtual QString GetInsertFileAtPrintLabel(void) const = 0;
    virtual QString GetInsertGraphicLabel(void) const = 0;

    // Insert Menu - Notes
    virtual QString GetInsertNoteCommentLabel(void) const = 0;
    virtual QString GetInsertNoteFootnoteLabel(void) const = 0;
    virtual QString GetInsertNoteEndnoteLabel(void) const = 0;
    virtual QString GetInsertNoteAnnotationLabel(void) const = 0;

    // Insert Menu - Index/TOC
    virtual QString GetInsertTOCEntryLabel(void) const = 0;
    virtual QString GetInsertIndexEntryLabel(void) const = 0;
    virtual QString GetInsertDotLeaderLabel(void) const = 0;
    virtual QString GetInsertParOutlineNumberLabel(void) const = 0;

    // Style Menu - Basic Styles
    virtual QString GetStyleBoldLabel(void) const = 0;
    virtual QString GetStyleItalicLabel(void) const = 0;
    virtual QString GetStyleUnderlineLabel(void) const = 0;
    virtual QString GetStyleFontLabel(void) const = 0;
    virtual QString GetStyleStrikeoutLabel(void) const = 0;
    virtual QString GetStyleSubscriptLabel(void) const = 0;
    virtual QString GetStyleSuperscriptLabel(void) const = 0;
    virtual QString GetStyleDoubleStrikeLabel(void) const = 0;
    virtual QString GetStyleColorLabel(void) const = 0;

    // Style Menu - Paragraph Styles
    virtual QString GetStyleSelectParStyleLabel(void) const = 0;
    virtual QString GetStyleReturnToPrevStyleLabel(void) const = 0;
    virtual QString GetStyleDefineParStyleLabel(void) const = 0;
    virtual QString GetStyleCopyStyleToLibraryLabel(void) const = 0;
    virtual QString GetStyleDeleteLibraryStyleLabel(void) const = 0;
    virtual QString GetStyleRenameLibraryStyleLabel(void) const = 0;
    virtual QString GetStyleRenameDocStyleLabel(void) const = 0;

    // Style Menu - Case
    virtual QString GetStyleUppercaseLabel(void) const = 0;
    virtual QString GetStyleLowercaseLabel(void) const = 0;
    virtual QString GetStyleSentenceCaseLabel(void) const = 0;

    // Layout Menu - Line Alignment
    virtual QString GetLayoutCenterLineLabel(void) const = 0;
    virtual QString GetLayoutRightAlignLabel(void) const = 0;

    // Layout Menu - Paragraph Alignment
    virtual QString GetLayoutLeftAlignParaLabel(void) const = 0;
    virtual QString GetLayoutCenterParaLabel(void) const = 0;
    virtual QString GetLayoutRightAlignParaLabel(void) const = 0;
    virtual QString GetLayoutJustifyParaLabel(void) const = 0;

    virtual QString GetLayoutRulerLineLabel(void) const = 0;

    // Layout Menu - Page Layout
    virtual QString GetLayoutColumnsLabel(void) const = 0;
    virtual QString GetLayoutPageLabel(void) const = 0;
    virtual QString GetLayoutHeaderLabel(void) const = 0;
    virtual QString GetLayoutFooterLabel(void) const = 0;
    virtual QString GetLayoutPageNumberingLabel(void) const = 0;
    virtual QString GetLayoutLineNumberingLabel(void) const = 0;
    virtual QString GetLayoutAlignmentLabel(void) const = 0;

    // Layout Menu - Special
    virtual QString GetLayoutOverprintCharLabel(void) const = 0;
    virtual QString GetLayoutOverprintLineLabel(void) const = 0;
    virtual QString GetLayoutOptionalHyphenLabel(void) const = 0;
    virtual QString GetLayoutVerticalCenterLabel(void) const = 0;
    virtual QString GetLayoutKeepWordsTogetherLabel(void) const = 0;
    virtual QString GetLayoutKeepLinesTogetherPageLabel(void) const = 0;
    virtual QString GetLayoutKeepLinesTogetherColumnLabel(void) const = 0;

    // Utilities Menu - Spell Check
    virtual QString GetUtilSpellCheckGlobalLabel(void) const = 0;
    virtual QString GetUtilSpellCheckRestLabel(void) const = 0;
    virtual QString GetUtilSpellCheckWordLabel(void) const = 0;
    virtual QString GetUtilSpellCheckTypeLabel(void) const = 0;
    virtual QString GetUtilThesaurusLabel(void) const = 0;

    // Utilities Menu - Tools
    virtual QString GetUtilInsetLabel(void) const = 0;
    virtual QString GetUtilCalculatorLabel(void) const = 0;
    virtual QString GetUtilBlockMathLabel(void) const = 0;
    virtual QString GetUtilSortBlockAscLabel(void) const = 0;
    virtual QString GetUtilSortBlockDesLabel(void) const = 0;
    virtual QString GetUtilWordCountLabel(void) const = 0;

    // Utilities Menu - Macros
    virtual QString GetUtilPlayMacroLabel(void) const = 0;
    virtual QString GetUtilRecordMacroLabel(void) const = 0;
    virtual QString GetUtilEditMacroLabel(void) const = 0;
    virtual QString GetUtilSingleStepLabel(void) const = 0;
    virtual QString GetUtilCopyMacroLabel(void) const = 0;
    virtual QString GetUtilDeleteMacroLabel(void) const = 0;
    virtual QString GetUtilRenameMacroLabel(void) const = 0;

    // Utilities Menu - Merge/Variables
    virtual QString GetUtilDataFileLabel(void) const = 0;
    virtual QString GetUtilNameVarsLabel(void) const = 0;
    virtual QString GetUtilSetVarLabel(void) const = 0;
    virtual QString GetUtilSetVarMathLabel(void) const = 0;
    virtual QString GetUtilAskVarLabel(void) const = 0;
    virtual QString GetUtilIfLabel(void) const = 0;
    virtual QString GetUtilElseLabel(void) const = 0;
    virtual QString GetUtilEndIfLabel(void) const = 0;
    virtual QString GetUtilTopLabel(void) const = 0;
    virtual QString GetUtilBottomLabel(void) const = 0;
    virtual QString GetUtilClearLabel(void) const = 0;
    virtual QString GetUtilDisplayLabel(void) const = 0;
    virtual QString GetUtilPrintNTimesLabel(void) const = 0;

    // Utilities Menu - Reformat
    virtual QString GetUtilReformatRestLabel(void) const = 0;
    virtual QString GetUtilReformatNotesLabel(void) const = 0;

    // Help Menu
    virtual QString GetHelpAboutLabel(void) const = 0;
};

#endif // MENUPROVIDER_H
