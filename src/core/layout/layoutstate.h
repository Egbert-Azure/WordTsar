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

#ifndef SRC_CORE_LAYOUT_LAYOUTSTATE_H
#define SRC_CORE_LAYOUT_LAYOUTSTATE_H

#include <string>
#include <vector>
#include <array>

#include "src/core/include/config.h"
#include "src/core/layout/layoutstructs.h"

//////////////////////////////////////////////////////////////////////////////
///
/// @brief Layout state management class
///
/// Stores all settings and state for layout operations. This is extracted
/// from cLayoutBase to separate state management from layout logic.
/// This is a pure C++ class with no Qt dependencies.
///
//////////////////////////////////////////////////////////////////////////////
class cLayoutState
{
public:
    cLayoutState(void);
    ~cLayoutState(void);

    // Page offset getters/setters
    COORD_T GetPageOffsetOdd(void) const;
    void SetPageOffsetOdd(COORD_T offset);
    COORD_T GetPageOffsetEven(void) const;
    void SetPageOffsetEven(COORD_T offset);

    // Margin getters/setters
    COORD_T GetLeftMargin(void) const;
    void SetLeftMargin(COORD_T margin);
    COORD_T GetRightMargin(void) const;
    void SetRightMargin(COORD_T margin);
    COORD_T GetTopMargin(void) const;
    void SetTopMargin(COORD_T margin);
    COORD_T GetBottomMargin(void) const;
    void SetBottomMargin(COORD_T margin);
    COORD_T GetHeaderMargin(void) const;
    void SetHeaderMargin(COORD_T margin);
    COORD_T GetFooterMargin(void) const;
    void SetFooterMargin(COORD_T margin);

    // Paper size getters/setters
    COORD_T GetPaperWidth(void) const;
    void SetPaperWidth(COORD_T width);
    COORD_T GetPaperHeight(void) const;
    void SetPaperHeight(COORD_T height);

    // Page length and line height
    COORD_T GetPageLength(void) const;
    void SetPageLength(COORD_T length);
    COORD_T GetLineHeight(void) const;
    void SetLineHeight(COORD_T height);
    bool IsAutoLeading(void) const;
    void SetAutoLeading(bool auto_leading);
    COORD_T GetSubSuperRoll(void) const;
    void SetSubSuperRoll(COORD_T roll);

    // Paragraph margin
    COORD_T GetParagraphMargin(void) const;
    void SetParagraphMargin(COORD_T margin);
    bool IsValidParagraphMargin(void) const;
    void SetValidParagraphMargin(bool valid);

    // Paragraph spacing
    COORD_T GetParagraphSpacingBefore(void) const;
    void SetParagraphSpacingBefore(COORD_T spacing);
    COORD_T GetParagraphSpacingAfter(void) const;
    void SetParagraphSpacingAfter(COORD_T spacing);

    // Tab stops
    const std::vector<sTabStop>& GetTabs(void) const;
    void SetTabs(const std::vector<sTabStop>& tabs);

    // Font tracking
    std::string GetCurrentFont(void) const;
    void SetCurrentFont(const std::string& font);
    std::string GetDefaultFont(void) const;
    void SetDefaultFont(const std::string& font);

    // Control code visibility
    eShowControl GetShowControl(void) const;
    void SetShowControl(eShowControl show);

    // Help mode flag
    bool IsHelp(void) const;
    void SetIsHelp(bool is_help);

    // Text modifiers
    const sModifiers& GetModifiers(void) const;
    void SetModifiers(const sModifiers& modifiers);

    // Page number settings
    PAGE_T GetPageNumberOffset(void) const;
    void SetPageNumberOffset(PAGE_T offset);
    ePageNumberFormat GetPageNumFormat(void) const;
    void SetPageNumFormat(ePageNumberFormat format);

    // Per-page page number overrides (from .pn commands)
    void AddPageNumOverride(PAGE_T fromPage, PAGE_T offset, ePageNumberFormat format);
    bool HasPageNumOverrideForPage(PAGE_T physicalPage) const;
    PAGE_T GetPageNumOffsetForPage(PAGE_T physicalPage) const;
    ePageNumberFormat GetPageNumFormatForPage(PAGE_T physicalPage) const;
    void ClearPageNumOverrides(void);

    bool ShouldDoNewPage(void) const;
    void SetDoNewPage(bool do_new_page);
    bool IsWordWrapEnabled(void) const;
    void SetWordWrapEnabled(bool enabled);
    bool IsLandscapeMode(void) const;
    void SetLandscapeMode(bool landscape);
    bool ShouldPrintPageNumbers(void) const;
    void SetPrintPageNumbers(bool print);
    COORD_T GetPageNumberColumn(void) const;   // .pc: distance from left margin, 0 = centered
    void SetPageNumberColumn(COORD_T column);

    // Text formatting state
    bool IsBoldActive(void) const;
    void SetBoldActive(bool active);
    bool IsItalicActive(void) const;
    void SetItalicActive(bool active);
    bool IsUnderlineActive(void) const;
    void SetUnderlineActive(bool active);
    bool IsStrikethroughActive(void) const;
    void SetStrikethroughActive(bool active);
    bool IsSuperscriptActive(void) const;
    void SetSuperscriptActive(bool active);
    bool IsSubscriptActive(void) const;
    void SetSubscriptActive(bool active);
    sSeqRGBColor GetCurrentColor(void) const;
    void SetCurrentColor(const sSeqRGBColor& color);
    sSeqRGBColor GetDefaultTextColor(void) const;
    void SetDefaultTextColor(const sSeqRGBColor& color);

    // Paragraph state
    bool IsFirstLineOfParagraph(void) const;
    void SetIsFirstLineOfParagraph(bool is_first);

    // Formatting state reset
    void ResetFormattingState(void);

    // Memory compaction
    void ShrinkToFit(void);

private:
    // Page settings
    COORD_T mPageOffsetOdd;
    COORD_T mPageOffsetEven;
    COORD_T mLeftMargin;
    COORD_T mRightMargin;
    COORD_T mTopMargin;
    COORD_T mBottomMargin;
    COORD_T mHeaderMargin;
    COORD_T mFooterMargin;
    COORD_T mPaperWidth;
    COORD_T mPaperHeight;
    COORD_T mPageLength;
    COORD_T mLineHeight;
    bool mAutoLeading;
    COORD_T mSubSuperRoll;

    // Paragraph margin
    COORD_T mParagraphMargin;
    bool mValidParagraphMargin;

    // Paragraph spacing
    COORD_T mParagraphSpacingBefore;
    COORD_T mParagraphSpacingAfter;

    // Tab stops
    std::vector<sTabStop> mTabs;

    // Font tracking
    std::string mCurrentFont;
    std::string mDefaultFont;

    // Control code visibility
    eShowControl mShowControl;

    // Help mode flag
    bool mIsHelp;

    // Text modifiers
    sModifiers mModifiers;

    // Page number settings
    PAGE_T mPageNumberOffset;
    ePageNumberFormat mPageNumFormat;

    // Per-page page number overrides (populated by .pn commands during layout)
    // Each entry records: from which physical page, what offset, what format
    struct sPageNumOverride
    {
        PAGE_T fromPage ;           ///< physical page this applies from
        PAGE_T offset ;             ///< offset to add to physical page number
        ePageNumberFormat format ;  ///< number format (Arabic, Roman, etc.)
    };
    std::vector<sPageNumOverride> mPageNumOverrides;
    bool mDoNewPage;
    bool mWordWrapEnabled;
    bool mLandscapeMode;
    bool mPrintPageNumbers;
    COORD_T mPageNumberColumn;

    // Text formatting state
    bool mBoldActive;
    bool mItalicActive;
    bool mUnderlineActive;
    bool mStrikethroughActive;
    bool mSuperscriptActive;
    bool mSubscriptActive;
    sSeqRGBColor mCurrentColor;
    sSeqRGBColor mDefaultTextColor;

    // Paragraph state
    bool mIsFirstLineOfParagraph;
};

#endif // SRC_CORE_LAYOUT_LAYOUTSTATE_H
