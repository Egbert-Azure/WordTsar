#ifndef WORDSTARMENU_H
#define WORDSTARMENU_H

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

#include "src/gui/menu/menuprovider.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @class cWordStarMenuProvider
///
/// @brief
/// Provides menu labels with WordStar keyboard shortcuts.
/// Implements IMenuProvider for WordStar input mode.
///
/////////////////////////////////////////////////////////////////////////////
class cWordStarMenuProvider : public IMenuProvider
{
public:
    cWordStarMenuProvider(void) = default;
    ~cWordStarMenuProvider(void) override = default;

    // File Menu
    QString GetFileOpenLabel(void) const override;
    QString GetFileSaveLabel(void) const override;
    QString GetFileSaveAsLabel(void) const override;
    QString GetFileSaveAndCloseLabel(void) const override;
    QString GetFileCloseLabel(void) const override;
    QString GetFilePrintLabel(void) const override;
    QString GetFilePrintPreviewLabel(void) const override;
    QString GetFileExitLabel(void) const override;

    // Edit Menu - Basic
    QString GetEditUndoLabel(void) const override;
    QString GetEditRedoLabel(void) const override;
    QString GetEditMarkBlockStartLabel(void) const override;
    QString GetEditMarkBlockEndLabel(void) const override;

    // Edit Menu - Move
    QString GetEditMoveBlockLabel(void) const override;

    // Edit Menu - Copy
    QString GetEditCopyBlockLabel(void) const override;
    QString GetEditCopyFromClipboardLabel(void) const override;
    QString GetEditCopyToClipboardLabel(void) const override;

    // Edit Menu - Delete
    QString GetEditDeleteBlockLabel(void) const override;
    QString GetEditDeleteWordLabel(void) const override;
    QString GetEditDeleteLineLabel(void) const override;
    QString GetEditDeleteLineLeftLabel(void) const override;
    QString GetEditDeleteLineRightLabel(void) const override;
    QString GetEditDeleteToCharLabel(void) const override;

    // Edit Menu - Find/Replace
    QString GetEditMarkPreviousBlockLabel(void) const override;
    QString GetEditFindLabel(void) const override;
    QString GetEditFindAndReplaceLabel(void) const override;
    QString GetEditFindNextLabel(void) const override;

    // Edit Menu - Goto
    QString GetEditGotoCharLabel(void) const override;
    QString GetEditGotoPageLabel(void) const override;
    QString GetEditGotoMarker1Label(void) const override;
    QString GetEditGotoMarker2Label(void) const override;
    QString GetEditGotoMarker3Label(void) const override;
    QString GetEditGotoMarker4Label(void) const override;
    QString GetEditGotoMarker5Label(void) const override;
    QString GetEditGotoMarker6Label(void) const override;
    QString GetEditGotoMarker7Label(void) const override;
    QString GetEditGotoMarker8Label(void) const override;
    QString GetEditGotoMarker9Label(void) const override;
    QString GetEditGotoMarker0Label(void) const override;
    QString GetEditGotoFontTagLabel(void) const override;
    QString GetEditGotoStyleTagLabel(void) const override;
    QString GetEditGotoNoteLabel(void) const override;
    QString GetEditGotoPreviousPositionLabel(void) const override;
    QString GetEditGotoLastFindReplaceLabel(void) const override;
    QString GetEditGotoBeginningOfBlockLabel(void) const override;
    QString GetEditGotoEndOfBlockLabel(void) const override;
    QString GetEditGotoDocumentStartLabel(void) const override;
    QString GetEditGotoDocumentEndLabel(void) const override;
    QString GetEditGotoScrollUpLabel(void) const override;
    QString GetEditGotoScrollDownLabel(void) const override;

    // Edit Menu - Set Marker
    QString GetEditSetMarker1Label(void) const override;
    QString GetEditSetMarker2Label(void) const override;
    QString GetEditSetMarker3Label(void) const override;
    QString GetEditSetMarker4Label(void) const override;
    QString GetEditSetMarker5Label(void) const override;
    QString GetEditSetMarker6Label(void) const override;
    QString GetEditSetMarker7Label(void) const override;
    QString GetEditSetMarker8Label(void) const override;
    QString GetEditSetMarker9Label(void) const override;
    QString GetEditSetMarker0Label(void) const override;

    // Edit Menu - Notes
    QString GetEditEditNoteLabel(void) const override;
    QString GetEditNoteConvertLabel(void) const override;
    QString GetEditNoteEndnoteLocationLabel(void) const override;

    // Edit Menu - Settings
    QString GetEditColumnBlockModeLabel(void) const override;
    QString GetEditColumnReplaceModeLabel(void) const override;
    QString GetEditAutoAlignLabel(void) const override;
    QString GetEditCloseDialogLabel(void) const override;

    // View Menu
    QString GetViewPreviewLabel(void) const override;
    QString GetViewCommandTagsLabel(void) const override;
    QString GetViewBlockHighlightingLabel(void) const override;
    QString GetViewScreenSettingsLabel(void) const override;
    QString GetViewSwitchModesLabel(void) const override;

    // Insert Menu - Basic
    QString GetInsertPageBreakLabel(void) const override;
    QString GetInsertDateLabel(void) const override;

    // Insert Menu - Other Values
    QString GetInsertTimeLabel(void) const override;
    QString GetInsertMathResultLabel(void) const override;
    QString GetInsertMathExpressionLabel(void) const override;
    QString GetInsertMathDollarLabel(void) const override;
    QString GetInsertFilenameLabel(void) const override;
    QString GetInsertDirectoryLabel(void) const override;
    QString GetInsertPathLabel(void) const override;

    // Insert Menu - Variables
    QString GetInsertVarDateLabel(void) const override;
    QString GetInsertVarTimeLabel(void) const override;
    QString GetInsertVarPageLabel(void) const override;
    QString GetInsertVarFilenameLabel(void) const override;
    QString GetInsertVarDriveLabel(void) const override;
    QString GetInsertVarDirectoryLabel(void) const override;
    QString GetInsertVarPathLabel(void) const override;
    QString GetInsertVarWordCountLabel(void) const override;

    // Insert Menu - Extended/Files
    QString GetInsertExtendedCharLabel(void) const override;
    QString GetInsertFileLabel(void) const override;
    QString GetInsertFileAtPrintLabel(void) const override;
    QString GetInsertGraphicLabel(void) const override;

    // Insert Menu - Notes
    QString GetInsertNoteCommentLabel(void) const override;
    QString GetInsertNoteFootnoteLabel(void) const override;
    QString GetInsertNoteEndnoteLabel(void) const override;
    QString GetInsertNoteAnnotationLabel(void) const override;

    // Insert Menu - Index/TOC
    QString GetInsertTOCEntryLabel(void) const override;
    QString GetInsertIndexEntryLabel(void) const override;
    QString GetInsertDotLeaderLabel(void) const override;
    QString GetInsertParOutlineNumberLabel(void) const override;

    // Style Menu - Basic Styles
    QString GetStyleBoldLabel(void) const override;
    QString GetStyleItalicLabel(void) const override;
    QString GetStyleUnderlineLabel(void) const override;
    QString GetStyleFontLabel(void) const override;
    QString GetStyleStrikeoutLabel(void) const override;
    QString GetStyleSubscriptLabel(void) const override;
    QString GetStyleSuperscriptLabel(void) const override;
    QString GetStyleDoubleStrikeLabel(void) const override;
    QString GetStyleColorLabel(void) const override;

    // Style Menu - Paragraph Styles
    QString GetStyleSelectParStyleLabel(void) const override;
    QString GetStyleReturnToPrevStyleLabel(void) const override;
    QString GetStyleDefineParStyleLabel(void) const override;
    QString GetStyleCopyStyleToLibraryLabel(void) const override;
    QString GetStyleDeleteLibraryStyleLabel(void) const override;
    QString GetStyleRenameLibraryStyleLabel(void) const override;
    QString GetStyleRenameDocStyleLabel(void) const override;

    // Style Menu - Case
    QString GetStyleUppercaseLabel(void) const override;
    QString GetStyleLowercaseLabel(void) const override;
    QString GetStyleSentenceCaseLabel(void) const override;

    // Layout Menu - Line Alignment
    QString GetLayoutCenterLineLabel(void) const override;
    QString GetLayoutRightAlignLabel(void) const override;

    // Layout Menu - Paragraph Alignment
    QString GetLayoutLeftAlignParaLabel(void) const override;
    QString GetLayoutCenterParaLabel(void) const override;
    QString GetLayoutRightAlignParaLabel(void) const override;
    QString GetLayoutJustifyParaLabel(void) const override;

    QString GetLayoutRulerLineLabel(void) const override;

    // Layout Menu - Page Layout
    QString GetLayoutColumnsLabel(void) const override;
    QString GetLayoutPageLabel(void) const override;
    QString GetLayoutHeaderLabel(void) const override;
    QString GetLayoutFooterLabel(void) const override;
    QString GetLayoutPageNumberingLabel(void) const override;
    QString GetLayoutLineNumberingLabel(void) const override;
    QString GetLayoutAlignmentLabel(void) const override;

    // Layout Menu - Special
    QString GetLayoutOverprintCharLabel(void) const override;
    QString GetLayoutOverprintLineLabel(void) const override;
    QString GetLayoutOptionalHyphenLabel(void) const override;
    QString GetLayoutVerticalCenterLabel(void) const override;
    QString GetLayoutKeepWordsTogetherLabel(void) const override;
    QString GetLayoutKeepLinesTogetherPageLabel(void) const override;
    QString GetLayoutKeepLinesTogetherColumnLabel(void) const override;

    // Utilities Menu - Spell Check
    QString GetUtilSpellCheckGlobalLabel(void) const override;
    QString GetUtilSpellCheckRestLabel(void) const override;
    QString GetUtilSpellCheckWordLabel(void) const override;
    QString GetUtilSpellCheckTypeLabel(void) const override;
    QString GetUtilThesaurusLabel(void) const override;

    // Utilities Menu - Tools
    QString GetUtilInsetLabel(void) const override;
    QString GetUtilCalculatorLabel(void) const override;
    QString GetUtilBlockMathLabel(void) const override;
    QString GetUtilSortBlockAscLabel(void) const override;
    QString GetUtilSortBlockDesLabel(void) const override;
    QString GetUtilWordCountLabel(void) const override;

    // Utilities Menu - Macros
    QString GetUtilPlayMacroLabel(void) const override;
    QString GetUtilRecordMacroLabel(void) const override;
    QString GetUtilEditMacroLabel(void) const override;
    QString GetUtilSingleStepLabel(void) const override;
    QString GetUtilCopyMacroLabel(void) const override;
    QString GetUtilDeleteMacroLabel(void) const override;
    QString GetUtilRenameMacroLabel(void) const override;

    // Utilities Menu - Merge/Variables
    QString GetUtilDataFileLabel(void) const override;
    QString GetUtilNameVarsLabel(void) const override;
    QString GetUtilSetVarLabel(void) const override;
    QString GetUtilSetVarMathLabel(void) const override;
    QString GetUtilAskVarLabel(void) const override;
    QString GetUtilIfLabel(void) const override;
    QString GetUtilElseLabel(void) const override;
    QString GetUtilEndIfLabel(void) const override;
    QString GetUtilTopLabel(void) const override;
    QString GetUtilBottomLabel(void) const override;
    QString GetUtilClearLabel(void) const override;
    QString GetUtilDisplayLabel(void) const override;
    QString GetUtilPrintNTimesLabel(void) const override;

    // Utilities Menu - Reformat
    QString GetUtilReformatRestLabel(void) const override;
    QString GetUtilReformatNotesLabel(void) const override;

    // Help Menu
    QString GetHelpAboutLabel(void) const override;
};

#endif // WORDSTARMENU_H
