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
 * @class cLayoutState
 *
 * @brief Stores and manages all formatting state used by the layout engine.
 *
 * Implements the cLayoutState class which encapsulates all formatting state
 * consumed by the layout engine. Extracted from cLayoutBase to separate
 * persistent state from layout runtime logic.
 *
 * @section layoutstate_page Page Geometry
 * - Paper dimensions: width, height in twips (default US Letter: 12240 x 15840)
 * - Margins: top, bottom, left, right, header, footer in twips
 * - Page offsets: odd and even page offsets for binding margins
 *
 * @section layoutstate_paragraph Paragraph Formatting
 * - Left and right margins (relative to page margins)
 * - Paragraph indent (first line), line spacing multiplier
 * - Line height override (from .LH dot command)
 * - Justification mode: left, center, right, full
 *
 * @section layoutstate_text Text Style State
 * - Character formatting: bold, italic, underline, strikethrough,
 *   superscript, subscript toggle tracking
 * - Font tracking: current font descriptor, default font descriptor
 * - Color state: current foreground and background colors
 *
 * @section layoutstate_pagenumbering Page Numbering
 * - Starting page number, page number format (Arabic, Roman, Alpha)
 * - Page number column position, page number omission flag
 * - Per-page number overrides from .PN dot commands
 *
 * @section layoutstate_reset State Reset
 * ResetFormattingState() restores all formatting to defaults for a clean
 * layout pass. Tab stops are reset to default 0.5-inch intervals. Page
 * geometry is preserved across resets.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cLayoutState Layout state class
 * @see cLayoutBase Layout engine consuming this state
 * @see ePageNumberFormat Page number format enumeration
 * @see sTabStop Tab stop position and type structure
 */

#include "layoutstate.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor - initializes all state variables to default values
///
/////////////////////////////////////////////////////////////////////////////
cLayoutState::cLayoutState(void)
{
    // Page offsets (default to 1 inch for binding margin)
    mPageOffsetOdd = 1440;           // 1 inch
    mPageOffsetEven = 1440;          // 1 inch

    // Margins (default column positions from left edge after page offset)
    mLeftMargin = 0;                 // 0 inches (starts at page offset)
    mRightMargin = 9360;             // 6.5 inches (6.5" text width on 8.5" paper)
    mTopMargin = 1440;               // 1 inch
    mBottomMargin = 1440;            // 1 inch   
    mHeaderMargin = 475;             // 0.33 inch
    mFooterMargin = 475;             // 0.33 inch

    // Paper size (US Letter: 8.5" x 11")
    mPaperWidth = 12240;             // 8.5 inches
    mPaperHeight = 15840;            // 11 inches
    mPageLength = 15840;             // 11 inches
    mLineHeight = NOT_SET;           // Use font-based lineSpacing() by default
    mAutoLeading = false;            // Explicit height by default
    mSubSuperRoll = 90;              // Default 3/48 inch = 90 twips (WordStar default)

    // Paragraph margin
    mParagraphMargin = 0;
    mValidParagraphMargin = false;

    // Paragraph spacing (WordTsar extension)
    mParagraphSpacingBefore = 0;     // No space before paragraphs by default
    mParagraphSpacingAfter = 0;      // No space after paragraphs by default

    // Font tracking (Phase 0.5.7 minimal implementation)
    mCurrentFont = "Monospace|12.0|0|0|0|0|0";  // Default monospace font
    mDefaultFont = "Monospace|12.0|0|0|0|0|0";  // Default font for dot commands/comments

    // Control code visibility
    mShowControl = SHOW_ALL;         // Default: show all (dot commands and control codes)

    // Help mode flag
    mIsHelp = false;                 // Default: not a help display

    // Text modifiers (default: left align, single spacing)
    mModifiers.justify = false;
    mModifiers.left = true;
    mModifiers.right = false;
    mModifiers.center = false;
    mModifiers.linespace = 1.0;

    // Page number settings
    mPageNumberOffset = 0;           // No offset (page 1 displays as 1)
    mPageNumFormat = PAGE_NUM_ARABIC; // Default: 1, 2, 3, ...
    mDoNewPage = false;
    mWordWrapEnabled = true;         // Word wrap enabled by default
    mLandscapeMode = false;          // Portrait orientation by default
    mPrintPageNumbers = true;        // Auto page numbering enabled by default

    // Text formatting state (default: no formatting active)
    mBoldActive = false;
    mItalicActive = false;
    mUnderlineActive = false;
    mStrikethroughActive = false;
    mSuperscriptActive = false;
    mSubscriptActive = false;
    mCurrentColor.red = -1;
    mCurrentColor.green = -1;
    mCurrentColor.blue = -1;
    mCurrentColor.alpha = -1;
    mDefaultTextColor.red = -1;
    mDefaultTextColor.green = -1;
    mDefaultTextColor.blue = -1;
    mDefaultTextColor.alpha = -1;

    // Paragraph margin control (first-line indent)
    mIsFirstLineOfParagraph = false;  // No paragraph being processed initially
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor
///
/////////////////////////////////////////////////////////////////////////////
cLayoutState::~cLayoutState(void)
{
    // Nothing to clean up (all members are value types or STL containers)
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Reset all formatting state (bold, italic, underline, etc.) to defaults
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutState::ResetFormattingState(void)
{
    mBoldActive = false;
    mItalicActive = false;
    mUnderlineActive = false;
    mStrikethroughActive = false;
    mSuperscriptActive = false;
    mSubscriptActive = false;
    mCurrentColor = mDefaultTextColor;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Compact all internal containers to release excess allocated memory.
/// Calls shrink_to_fit() on the tab stops and page number override vectors.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutState::ShrinkToFit(void)
{
    mTabs.shrink_to_fit();
    mPageNumOverrides.shrink_to_fit();
}


// Page offset getters/setters

COORD_T cLayoutState::GetPageOffsetOdd(void) const
{
    return mPageOffsetOdd;
}

void cLayoutState::SetPageOffsetOdd(COORD_T offset)
{
    mPageOffsetOdd = offset;
}

COORD_T cLayoutState::GetPageOffsetEven(void) const
{
    return mPageOffsetEven;
}

void cLayoutState::SetPageOffsetEven(COORD_T offset)
{
    mPageOffsetEven = offset;
}


// Margin getters/setters

COORD_T cLayoutState::GetLeftMargin(void) const
{
    return mLeftMargin;
}

void cLayoutState::SetLeftMargin(COORD_T margin)
{
    mLeftMargin = margin;
}

COORD_T cLayoutState::GetRightMargin(void) const
{
    return mRightMargin;
}

void cLayoutState::SetRightMargin(COORD_T margin)
{
    mRightMargin = margin;
}

COORD_T cLayoutState::GetTopMargin(void) const
{
    return mTopMargin;
}

void cLayoutState::SetTopMargin(COORD_T margin)
{
    mTopMargin = margin;
}

COORD_T cLayoutState::GetBottomMargin(void) const
{
    return mBottomMargin;
}

void cLayoutState::SetBottomMargin(COORD_T margin)
{
    mBottomMargin = margin;
}

COORD_T cLayoutState::GetHeaderMargin(void) const
{
    return mHeaderMargin;
}

void cLayoutState::SetHeaderMargin(COORD_T margin)
{
    mHeaderMargin = margin;
}

COORD_T cLayoutState::GetFooterMargin(void) const
{
    return mFooterMargin;
}

void cLayoutState::SetFooterMargin(COORD_T margin)
{
    mFooterMargin = margin;
}


// Paper size getters/setters

COORD_T cLayoutState::GetPaperWidth(void) const
{
    return mPaperWidth;
}

void cLayoutState::SetPaperWidth(COORD_T width)
{
    mPaperWidth = width;
}

COORD_T cLayoutState::GetPaperHeight(void) const
{
    return mPaperHeight;
}

void cLayoutState::SetPaperHeight(COORD_T height)
{
    mPaperHeight = height;
}


// Page length and line height

COORD_T cLayoutState::GetPageLength(void) const
{
    return mPageLength;
}

void cLayoutState::SetPageLength(COORD_T length)
{
    mPageLength = length;
}

COORD_T cLayoutState::GetLineHeight(void) const
{
    return mLineHeight;
}

void cLayoutState::SetLineHeight(COORD_T height)
{
    mLineHeight = height;
}

bool cLayoutState::IsAutoLeading(void) const
{
    return mAutoLeading;
}

void cLayoutState::SetAutoLeading(bool auto_leading)
{
    mAutoLeading = auto_leading;
}

COORD_T cLayoutState::GetSubSuperRoll(void) const
{
    return mSubSuperRoll;
}

void cLayoutState::SetSubSuperRoll(COORD_T roll)
{
    mSubSuperRoll = roll;
}


// Paragraph margin

COORD_T cLayoutState::GetParagraphMargin(void) const
{
    return mParagraphMargin;
}

void cLayoutState::SetParagraphMargin(COORD_T margin)
{
    mParagraphMargin = margin;
}

bool cLayoutState::IsValidParagraphMargin(void) const
{
    return mValidParagraphMargin;
}

void cLayoutState::SetValidParagraphMargin(bool valid)
{
    mValidParagraphMargin = valid;
}


// Paragraph spacing

COORD_T cLayoutState::GetParagraphSpacingBefore(void) const
{
    return mParagraphSpacingBefore;
}

void cLayoutState::SetParagraphSpacingBefore(COORD_T spacing)
{
    mParagraphSpacingBefore = spacing;
}

COORD_T cLayoutState::GetParagraphSpacingAfter(void) const
{
    return mParagraphSpacingAfter;
}

void cLayoutState::SetParagraphSpacingAfter(COORD_T spacing)
{
    mParagraphSpacingAfter = spacing;
}


// Tab stops

const std::vector<sTabStop>& cLayoutState::GetTabs(void) const
{
    return mTabs;
}

void cLayoutState::SetTabs(const std::vector<sTabStop>& tabs)
{
    mTabs = tabs;
}


// Font tracking

std::string cLayoutState::GetCurrentFont(void) const
{
    return mCurrentFont;
}

void cLayoutState::SetCurrentFont(const std::string& font)
{
    mCurrentFont = font;
}

std::string cLayoutState::GetDefaultFont(void) const
{
    return mDefaultFont;
}

void cLayoutState::SetDefaultFont(const std::string& font)
{
    mDefaultFont = font;
}


// Control code visibility

eShowControl cLayoutState::GetShowControl(void) const
{
    return mShowControl;
}

void cLayoutState::SetShowControl(eShowControl show)
{
    mShowControl = show;
}


// Help mode flag

bool cLayoutState::IsHelp(void) const
{
    return mIsHelp;
}

void cLayoutState::SetIsHelp(bool is_help)
{
    mIsHelp = is_help;
}


// Text modifiers

const sModifiers& cLayoutState::GetModifiers(void) const
{
    return mModifiers;
}

void cLayoutState::SetModifiers(const sModifiers& modifiers)
{
    mModifiers = modifiers;
}


// Page number settings

PAGE_T cLayoutState::GetPageNumberOffset(void) const
{
    return mPageNumberOffset;
}

void cLayoutState::SetPageNumberOffset(PAGE_T offset)
{
    mPageNumberOffset = offset;
}

ePageNumberFormat cLayoutState::GetPageNumFormat(void) const
{
    return mPageNumFormat;
}

void cLayoutState::SetPageNumFormat(ePageNumberFormat format)
{
    mPageNumFormat = format;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  fromPage [in] physical page this override applies from
/// @param  offset [in] offset to add to physical page number
/// @param  format [in] number format (Arabic, Roman, etc.)
///
/// @return nothing
///
/// @brief
/// Records a page number override from a .pn command. Overrides are
/// ordered by fromPage and looked up during FormatPageNumber to get
/// the correct offset and format for each physical page.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutState::AddPageNumOverride(PAGE_T fromPage, PAGE_T offset, ePageNumberFormat format)
{
    sPageNumOverride ovr;
    ovr.fromPage = fromPage;
    ovr.offset = offset;
    ovr.format = format;
    mPageNumOverrides.push_back(ovr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  physicalPage [in] physical page number to check
///
/// @return true if any override applies to this page
///
/// @brief
/// Checks whether a per-page override exists for the given physical page.
/// Returns true if any override has fromPage <= physicalPage.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutState::HasPageNumOverrideForPage(PAGE_T physicalPage) const
{
    for (const auto& ovr : mPageNumOverrides)
    {
        if (ovr.fromPage <= physicalPage)
        {
            return true;
        }
    }
    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  physicalPage [in] physical page number to look up
///
/// @return offset to add to physical page number
///
/// @brief
/// Finds the effective page number offset for a given physical page.
/// Searches overrides in order, using the last one where fromPage <= physicalPage.
/// Returns 0 if no override applies (default: no offset).
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cLayoutState::GetPageNumOffsetForPage(PAGE_T physicalPage) const
{
    PAGE_T offset = 0;
    for (const auto& ovr : mPageNumOverrides)
    {
        if (ovr.fromPage <= physicalPage)
        {
            offset = ovr.offset;
        }
    }
    return offset;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  physicalPage [in] physical page number to look up
///
/// @return effective page number format for that page
///
/// @brief
/// Finds the effective page number format for a given physical page.
/// Searches overrides in order, using the last one where fromPage <= physicalPage.
/// Returns PAGE_NUM_ARABIC if no override applies (default).
///
/////////////////////////////////////////////////////////////////////////////
ePageNumberFormat cLayoutState::GetPageNumFormatForPage(PAGE_T physicalPage) const
{
    ePageNumberFormat format = PAGE_NUM_ARABIC;
    for (const auto& ovr : mPageNumOverrides)
    {
        if (ovr.fromPage <= physicalPage)
        {
            format = ovr.format;
        }
    }
    return format;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears all page number overrides. Called at start of full layout.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutState::ClearPageNumOverrides(void)
{
    mPageNumOverrides.clear();
}


bool cLayoutState::ShouldDoNewPage(void) const
{
    return mDoNewPage;
}

void cLayoutState::SetDoNewPage(bool do_new_page)
{
    mDoNewPage = do_new_page;
}

bool cLayoutState::IsWordWrapEnabled(void) const
{
    return mWordWrapEnabled;
}

void cLayoutState::SetWordWrapEnabled(bool enabled)
{
    mWordWrapEnabled = enabled;
}

bool cLayoutState::IsLandscapeMode(void) const
{
    return mLandscapeMode;
}

void cLayoutState::SetLandscapeMode(bool landscape)
{
    mLandscapeMode = landscape;
}

bool cLayoutState::ShouldPrintPageNumbers(void) const
{
    return mPrintPageNumbers;
}

void cLayoutState::SetPrintPageNumbers(bool print)
{
    mPrintPageNumbers = print;
}


// Text formatting state

bool cLayoutState::IsBoldActive(void) const
{
    return mBoldActive;
}

void cLayoutState::SetBoldActive(bool active)
{
    mBoldActive = active;
}

bool cLayoutState::IsItalicActive(void) const
{
    return mItalicActive;
}

void cLayoutState::SetItalicActive(bool active)
{
    mItalicActive = active;
}

bool cLayoutState::IsUnderlineActive(void) const
{
    return mUnderlineActive;
}

void cLayoutState::SetUnderlineActive(bool active)
{
    mUnderlineActive = active;
}

bool cLayoutState::IsStrikethroughActive(void) const
{
    return mStrikethroughActive;
}

void cLayoutState::SetStrikethroughActive(bool active)
{
    mStrikethroughActive = active;
}

bool cLayoutState::IsSuperscriptActive(void) const
{
    return mSuperscriptActive;
}

void cLayoutState::SetSuperscriptActive(bool active)
{
    mSuperscriptActive = active;
}

bool cLayoutState::IsSubscriptActive(void) const
{
    return mSubscriptActive;
}

void cLayoutState::SetSubscriptActive(bool active)
{
    mSubscriptActive = active;
}

sSeqRGBColor cLayoutState::GetCurrentColor(void) const
{
    return mCurrentColor;
}

void cLayoutState::SetCurrentColor(const sSeqRGBColor& color)
{
    mCurrentColor = color;
}

sSeqRGBColor cLayoutState::GetDefaultTextColor(void) const
{
    return mDefaultTextColor;
}

void cLayoutState::SetDefaultTextColor(const sSeqRGBColor& color)
{
    mDefaultTextColor = color;
}


// Paragraph state

bool cLayoutState::IsFirstLineOfParagraph(void) const
{
    return mIsFirstLineOfParagraph;
}

void cLayoutState::SetIsFirstLineOfParagraph(bool is_first)
{
    mIsFirstLineOfParagraph = is_first;
}
