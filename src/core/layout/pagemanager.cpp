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
 * @class cPageManager
 *
 * @brief Manages page boxes, page breaks, and margin recalculation for the layout engine.
 *
 * Implements the cPageManager class which owns the global box list (sBoxes)
 * and handles creation of new page boxes when page breaks or margin changes
 * occur. Extracted from cLayoutBase to isolate page/box management from the
 * main layout algorithm.
 *
 * @section pagemgr_boxes Box Management
 * - CreatePageBox(): allocates a new sBoxes structure with geometry derived
 *   from current margins and page offsets in cLayoutState
 * - Tracks current box index and associates lines with boxes during layout
 * - Handles box geometry updates when margins change mid-page
 *
 * @section pagemgr_pagebreaks Page Break Logic
 * - Detects page overflow: triggers a new page when content exceeds
 *   the bottom margin of the current box
 * - Explicit page breaks: handles .PA (unconditional) and .CP (conditional)
 *   dot commands that force new pages
 * - Margin change detection: creates new boxes when left/right margins
 *   change due to dot commands
 *
 * @section pagemgr_numbering Page Numbering
 * - Tracks both physical page numbers (sequential from 1) and logical
 *   page numbers (affected by .PN dot command overrides)
 * - Maps boxes to their physical and logical page numbers
 * - Supports odd/even page offset alternation for binding margins
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see sBoxes Page box geometry structure
 * @see cLayoutState Layout state providing margins and page geometry
 * @see cLayoutBase Layout engine coordinating page management
 * @see cDocument Document providing paragraph content
 */

#include "pagemanager.h"
#include "layoutstate.h"
#include "src/core/document/document.h"




/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in] pointer to layout state (NOT owned)
///
/// @brief
/// Constructor - initializes page manager with state reference
///
/////////////////////////////////////////////////////////////////////////////
cPageManager::cPageManager(cLayoutState* state)
    : mCurrentBoxIndex(NOT_SET),
      mLastPageNumberForBox(0),
      mCurrentPage(1),
      mLogicalPageNumber(1),
      mLastBoxLeftMargin(0),
      mLastBoxRightMargin(0),
      mLastBoxPageOffset(0),
      mLastBoxTopMargin(0),
      mLastBoxBottomMargin(0),
      mLastBoxPageLength(0),
      mBoxLeft(0),
      mBoxRight(0),
      mBoxTop(0),
      mBoxBottom(0),
      mLayoutState(state)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Destructor - nothing to delete (mLayoutState not owned)
///
/////////////////////////////////////////////////////////////////////////////
cPageManager::~cPageManager(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number for the new box (1-based)
///
/// @return true if box created successfully, false otherwise
///
/// @brief
/// Creates a new text box for a page break.
///
/// Box coordinates:
///   left   = pageOffset + GetLeftMargin()
///   right  = pageOffset + GetRightMargin()
///   top    = GetTopMargin()
///   bottom = PaperHeight - GetBottomMargin()
///
/// Note: Header/footer margins do NOT affect the text box. They specify
/// where headers/footers are rendered within the margin space:
///   header Y = GetTopMargin() - GetHeaderMargin() (above top margin line)
///   footer Y = (PaperHeight - GetBottomMargin()) + GetFooterMargin() (below bottom margin line)
///
/// For help displays, uses 0 margins and arbitrarily large bounds.
/// If page length (.PL) is set, bottom is adjusted to not exceed it.
///
/////////////////////////////////////////////////////////////////////////////
bool cPageManager::CreatePageBox(PAGE_T page)
{
    // Phase 4 (idempotence fix): Check if a box ACTUALLY exists for this page
    // Don't rely on tracking variables which may be stale after state restoration
    std::vector<int> existingBoxes = GetBoxesOnPage(page);
    if (!existingBoxes.empty())
    {
        // Reuse existing box - set current box index to first box on page
        mCurrentBoxIndex = existingBoxes[0];
        mLastPageNumberForBox = page;

        // Get mutable reference to reset fill position
        sBoxes& box = mGlobalBoxList[mCurrentBoxIndex];

        // CRITICAL: Reset currentY to 0 when reusing a box for page break
        box.currentY = 0;

        // Update box coordinates from current margin state
        // (margins may have changed since box was first created, e.g. .mt 5i added)
        COORD_T pageOffset = (page % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();
        if (mLayoutState->IsHelp())
        {
            box.left = 0;
            box.right = 655350;
            box.top = 0;
            box.bottom = 655350;
        }
        else
        {
            box.left = pageOffset + mLayoutState->GetLeftMargin();
            box.right = pageOffset + mLayoutState->GetRightMargin();
            box.top = mLayoutState->GetTopMargin();
            box.bottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin();
        }

        // Apply page length if set (.PL command)
        if (mLayoutState->GetPageLength() > 0)
        {
            COORD_T plBottom = box.top + mLayoutState->GetPageLength();
            if (plBottom < box.bottom)
            {
                box.bottom = plBottom;
            }
        }

        // Update pageInfo
        box.pageInfo.topmargin = mLayoutState->GetTopMargin();
        box.pageInfo.bottommargin = mLayoutState->GetBottomMargin();
        box.pageInfo.leftmargin = mLayoutState->GetLeftMargin();
        box.pageInfo.rightmargin = mLayoutState->GetRightMargin();
        box.pageInfo.headermargin = mLayoutState->GetHeaderMargin();
        box.pageInfo.footermargin = mLayoutState->GetFooterMargin();

        // Sync local box coordinates
        mBoxLeft = box.left;
        mBoxRight = box.right;
        mBoxTop = box.top;
        mBoxBottom = box.bottom;

        // Track margins used for this box
        mLastBoxLeftMargin = mLayoutState->GetLeftMargin();
        mLastBoxRightMargin = mLayoutState->GetRightMargin();
        mLastBoxPageOffset = pageOffset;
        mLastBoxTopMargin = mLayoutState->GetTopMargin();
        mLastBoxBottomMargin = mLayoutState->GetBottomMargin();
        mLastBoxPageLength = mLayoutState->GetPageLength();

        return true;
    }

    // No existing box found - create new box
    sBoxes box;
    box.type = BOX_TEXT;
    box.pageNumber = page;
    box.boxNumber = static_cast<int>(mGlobalBoxList.size());

    // Calculate page offset (odd/even pages may differ)
    // For help displays, use 0 margins and large bounds
    COORD_T pageOffset = (page % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();

    if (mLayoutState->IsHelp())
    {
        // Help mode: no margins, no page offset, arbitrarily large bounds
        box.left = 0;
        box.right = 655350;  // Arbitrarily large (from old layout)
        box.top = 0;
        box.bottom = 655350;  // Arbitrarily large (from old layout)
    }
    else
    {
        // Normal mode: calculate box coordinates (ALL absolute)
        // Header/footer margins don't affect text box - they only position headers/footers
        box.left = pageOffset + mLayoutState->GetLeftMargin();
        box.right = pageOffset + mLayoutState->GetRightMargin();
        box.top = mLayoutState->GetTopMargin();
        box.bottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin();
    }

    // Apply page length if set (.PL command)
    if (mLayoutState->GetPageLength() > 0)
    {
        COORD_T plBottom = box.top + mLayoutState->GetPageLength();
        if (plBottom < box.bottom)
        {
            box.bottom = plBottom;
        }
    }

    box.currentY = 0;
    box.columnCount = 1;
    box.columnGap = 0;

    // Capture page info (Phase 1B - for GetPageInfo())
    box.pageInfo.paperwidth = mLayoutState->GetPaperWidth();
    box.pageInfo.paperheight = mLayoutState->GetPaperHeight();
    box.pageInfo.papertype = PaperLetter;
    box.pageInfo.set = false;
    box.pageInfo.topmargin = mLayoutState->GetTopMargin();
    box.pageInfo.bottommargin = mLayoutState->GetBottomMargin();
    box.pageInfo.leftmargin = mLayoutState->GetLeftMargin();
    box.pageInfo.rightmargin = mLayoutState->GetRightMargin();
    box.pageInfo.headermargin = mLayoutState->GetHeaderMargin();
    box.pageInfo.footermargin = mLayoutState->GetFooterMargin();

    // Add to global list
    mGlobalBoxList.push_back(box);
    mCurrentBoxIndex = static_cast<int>(mGlobalBoxList.size()) - 1;

    // Remember this page number
    mLastPageNumberForBox = page;

    // Update current box coordinates for wrapping
    mBoxLeft = box.left;
    mBoxRight = box.right;
    mBoxTop = box.top;
    mBoxBottom = box.bottom;

    // Track margins used for this box
    mLastBoxLeftMargin = mLayoutState->GetLeftMargin();
    mLastBoxRightMargin = mLayoutState->GetRightMargin();
    mLastBoxPageOffset = pageOffset;
    mLastBoxTopMargin = mLayoutState->GetTopMargin();
    mLastBoxBottomMargin = mLayoutState->GetBottomMargin();
    mLastBoxPageLength = mLayoutState->GetPageLength();

    // Headers/footers will be inserted after full document layout
    // (moved to end of LayoutDocument to ensure all commands are parsed first)

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if margins have changed since last box creation
///
/// @brief
/// Checks if the current margin settings differ from those used to
/// create the last box. Used to determine if a new margin box is needed.
///
/// Compares left margin, right margin, and page offset.
///
/////////////////////////////////////////////////////////////////////////////
bool cPageManager::CheckMarginChange(void)
{
    // If no boxes created yet, no change to detect
    if (mCurrentBoxIndex < 0)
    {
        return false;
    }

    // Calculate current page offset based on last page number
    COORD_T pageOffset = (mLastPageNumberForBox % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();

    // Check if any margin has changed
    if (!CoordsEqual(mLayoutState->GetLeftMargin(), mLastBoxLeftMargin) ||
        !CoordsEqual(mLayoutState->GetRightMargin(), mLastBoxRightMargin) ||
        !CoordsEqual(pageOffset, mLastBoxPageOffset))
    {
        return true;
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if page-level margins (top/bottom) have changed
///
/// @brief
/// Checks if the top or bottom margin has changed since the last box was
/// created. These are page-level changes that affect the entire page's
/// usable area, unlike left/right margin changes which create stacked
/// content boxes.
///
/// @see CheckMarginChange() for content-flow (left/right) margin detection
///
/////////////////////////////////////////////////////////////////////////////
bool cPageManager::CheckPageChange(void)
{
    // If no boxes created yet, no change to detect
    if (mCurrentBoxIndex < 0)
    {
        return false;
    }

    // Check if page-level margins or page length have changed
    if (!CoordsEqual(mLayoutState->GetTopMargin(), mLastBoxTopMargin) ||
        !CoordsEqual(mLayoutState->GetBottomMargin(), mLastBoxBottomMargin) ||
        !CoordsEqual(mLayoutState->GetPageLength(), mLastBoxPageLength))
    {
        return true;
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if page box was updated successfully
///
/// @brief
/// Modifies the current page's box coordinates in-place when page-level
/// margins (top/bottom) change. Updates all boxes on the current page
/// to reflect the new usable area.
///
/// Unlike CreateMarginBox() which stacks a new box for left/right changes,
/// this modifies existing boxes because top/bottom changes affect the
/// entire page's usable area.
///
/////////////////////////////////////////////////////////////////////////////
bool cPageManager::UpdatePageBox(void)
{
    if (mCurrentBoxIndex < 0)
    {
        return false;
    }

    // Find all boxes on the current page
    std::vector<int> boxesOnPage = GetBoxesOnPage(mLastPageNumberForBox);
    if (boxesOnPage.empty())
    {
        return false;
    }

    // Calculate new page-level bounds
    COORD_T newTop = mLayoutState->GetTopMargin();
    COORD_T newBottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin();

    // Apply page length if set (.PL command)
    if (mLayoutState->GetPageLength() > 0)
    {
        COORD_T plBottom = newTop + mLayoutState->GetPageLength();
        if (plBottom < newBottom)
        {
            newBottom = plBottom;
        }
    }

    // Update the first box's top (page-level coordinate)
    int firstBoxIdx = boxesOnPage[0];
    sBoxes& firstBox = mGlobalBoxList[firstBoxIdx];
    firstBox.top = newTop;
    firstBox.bottom = newBottom;

    // Update pageInfo on first box
    firstBox.pageInfo.topmargin = mLayoutState->GetTopMargin();
    firstBox.pageInfo.bottommargin = mLayoutState->GetBottomMargin();

    // Update stacked boxes on this page (their bottom also changes)
    for (size_t i = 1; i < boxesOnPage.size(); i++)
    {
        sBoxes& stackedBox = mGlobalBoxList[boxesOnPage[i]];
        stackedBox.bottom = newBottom;
    }

    // Update cached coordinates from current box
    sBoxes& currentBox = mGlobalBoxList[mCurrentBoxIndex];
    mBoxLeft = currentBox.left;
    mBoxRight = currentBox.right;
    mBoxTop = currentBox.top;
    mBoxBottom = currentBox.bottom;

    // Track new margins
    mLastBoxTopMargin = mLayoutState->GetTopMargin();
    mLastBoxBottomMargin = mLayoutState->GetBottomMargin();
    mLastBoxPageLength = mLayoutState->GetPageLength();

    return true;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraphNum [in] paragraph number to check
/// @param  doc [in] document pointer (for future use)
///
/// @return true if previous paragraph has pageBreak flag set
///
/// @brief
/// Checks if the previous paragraph had a .PA command, which would
/// require starting the current paragraph on a new page.
///
/// NOTE: This method is a placeholder - actual implementation depends
/// on paragraph layout which is in cLayoutBase
///
/////////////////////////////////////////////////////////////////////////////
bool cPageManager::CheckPageBreak(PARAGRAPH_T paragraphNum, cDocument* doc)
{
    (void)paragraphNum;  // Unused - needs access to mParagraphLayout
    (void)doc;           // Unused - for future use

    // This method needs to be called via cLayoutBase which has access
    // to mParagraphLayout. Left here as placeholder for future refactoring.
    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  lineHeight [in] height of line to be added (in twips)
///
/// @return true if adding this line would overflow the current box
///
/// @brief
/// Checks if there's enough space in the current box for a line of
/// the specified height. Used for automatic page breaks when box fills.
///
/////////////////////////////////////////////////////////////////////////////
bool cPageManager::NeedNewPage(COORD_T lineHeight)
{
    // No current box - need to create one
    if (mCurrentBoxIndex < 0)
    {
        return true;
    }

    const sBoxes& currentBox = mGlobalBoxList[mCurrentBoxIndex];

    // Calculate available space from page dimensions
    // Header/footer margins don't affect text box - they only position headers/footers
    COORD_T boxBottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin();

    // Apply page length limit if set (.PL command)
    // This must match the logic in CreatePageBox() for consistency
    if (mLayoutState->GetPageLength() > 0)
    {
        COORD_T plBottom = currentBox.top + mLayoutState->GetPageLength();
        if (plBottom < boxBottom)
        {
            boxBottom = plBottom;
        }
    }

    // Calculate remaining space in box
    COORD_T remainingSpace = boxBottom - (currentBox.top + currentBox.currentY);

    // Not enough space for this line
    if (remainingSpace < lineHeight)
    {
        return true;
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number for the new box (1-based)
///
/// @return true if box created successfully, false otherwise
///
/// @brief
/// Creates a new text box due to margin change mid-page.
///
/// Unlike CreatePageBox, this stacks vertically from the previous box.
/// The top of the new box starts at the previous box's currentY position.
///
/// Box coordinates:
///   left   = pageOffset + GetLeftMargin()
///   right  = pageOffset + GetRightMargin()
///   top    = previousBox.top + previousBox.currentY
///   bottom = PaperHeight - GetBottomMargin()
///
/// If page length (.PL) is set, bottom is adjusted to not exceed it.
///
/////////////////////////////////////////////////////////////////////////////
bool cPageManager::CreateMarginBox(PAGE_T page)
{
    // Must have a previous box to stack from
    if (mCurrentBoxIndex < 0)
    {
        return false;
    }

    // Get the previous box to calculate stacking position
    sBoxes& prevBox = mGlobalBoxList[mCurrentBoxIndex];

    // Truncate previous box bottom to where content actually ended
    // This prevents boxes from overlapping when a new margin box is created
    prevBox.bottom = prevBox.top + prevBox.currentY;

    // Calculate page offset (odd/even pages may differ)
    COORD_T pageOffset = (page % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();

    // Search forward for an existing box on the same page.
    // During partial layout, margin boxes from a previous layout pass
    // may not be adjacent (e.g., in a 363-page doc, page 1's margin
    // box is at index 363, not index 1).
    int reuseIdx = -1;
    for (int i = mCurrentBoxIndex + 1; i < static_cast<int>(mGlobalBoxList.size()); ++i)
    {
        if (mGlobalBoxList[i].pageNumber == page)
        {
            reuseIdx = i;
            break;
        }
    }

    if (reuseIdx >= 0)
    {
        // Reuse existing box, update its coordinates
        sBoxes& box = mGlobalBoxList[reuseIdx];

        box.left = pageOffset + mLayoutState->GetLeftMargin();
        box.right = pageOffset + mLayoutState->GetRightMargin();
        box.top = prevBox.bottom;
        box.bottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin();

        // Apply page length if set (.PL command)
        if (mLayoutState->GetPageLength() > 0)
        {
            COORD_T plBottom = mLayoutState->GetTopMargin() + mLayoutState->GetPageLength();
            if (plBottom < box.bottom)
            {
                box.bottom = plBottom;
            }
        }

        box.currentY = 0;

        // Update page info
        box.pageInfo.paperwidth = mLayoutState->GetPaperWidth();
        box.pageInfo.paperheight = mLayoutState->GetPaperHeight();
        box.pageInfo.topmargin = mLayoutState->GetTopMargin();
        box.pageInfo.bottommargin = mLayoutState->GetBottomMargin();
        box.pageInfo.leftmargin = mLayoutState->GetLeftMargin();
        box.pageInfo.rightmargin = mLayoutState->GetRightMargin();
        box.pageInfo.headermargin = mLayoutState->GetHeaderMargin();
        box.pageInfo.footermargin = mLayoutState->GetFooterMargin();

        mCurrentBoxIndex = reuseIdx;

        // Update current box coordinates for wrapping
        mBoxLeft = box.left;
        mBoxRight = box.right;
        mBoxTop = box.top;
        mBoxBottom = box.bottom;

        // Track margins used for this box
        mLastBoxLeftMargin = mLayoutState->GetLeftMargin();
        mLastBoxRightMargin = mLayoutState->GetRightMargin();
        mLastBoxPageOffset = pageOffset;
        mLastBoxTopMargin = mLayoutState->GetTopMargin();
        mLastBoxBottomMargin = mLayoutState->GetBottomMargin();
        mLastBoxPageLength = mLayoutState->GetPageLength();

        return true;
    }

    // No existing box to reuse, create a new one
    sBoxes box;
    box.type = BOX_TEXT;
    box.pageNumber = page;
    box.boxNumber = static_cast<int>(mGlobalBoxList.size());

    // Calculate box coordinates (ALL absolute)
    box.left = pageOffset + mLayoutState->GetLeftMargin();
    box.right = pageOffset + mLayoutState->GetRightMargin();
    box.top = prevBox.bottom;
    box.bottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin();

    // Apply page length if set (.PL command)
    if (mLayoutState->GetPageLength() > 0)
    {
        COORD_T plBottom = mLayoutState->GetTopMargin() + mLayoutState->GetPageLength();
        if (plBottom < box.bottom)
        {
            box.bottom = plBottom;
        }
    }

    box.currentY = 0;
    box.columnCount = 1;
    box.columnGap = 0;

    // Capture page info
    box.pageInfo.paperwidth = mLayoutState->GetPaperWidth();
    box.pageInfo.paperheight = mLayoutState->GetPaperHeight();
    box.pageInfo.papertype = PaperLetter;
    box.pageInfo.set = false;
    box.pageInfo.topmargin = mLayoutState->GetTopMargin();
    box.pageInfo.bottommargin = mLayoutState->GetBottomMargin();
    box.pageInfo.leftmargin = mLayoutState->GetLeftMargin();
    box.pageInfo.rightmargin = mLayoutState->GetRightMargin();
    box.pageInfo.headermargin = mLayoutState->GetHeaderMargin();
    box.pageInfo.footermargin = mLayoutState->GetFooterMargin();

    // Add to global list
    mGlobalBoxList.push_back(box);
    mCurrentBoxIndex = static_cast<int>(mGlobalBoxList.size()) - 1;

    // Update current box coordinates for wrapping
    mBoxLeft = box.left;
    mBoxRight = box.right;
    mBoxTop = box.top;
    mBoxBottom = box.bottom;

    // Track margins used for this box
    mLastBoxLeftMargin = mLayoutState->GetLeftMargin();
    mLastBoxRightMargin = mLayoutState->GetRightMargin();
    mLastBoxPageOffset = pageOffset;
    mLastBoxTopMargin = mLayoutState->GetTopMargin();
    mLastBoxBottomMargin = mLayoutState->GetBottomMargin();
    mLastBoxPageLength = mLayoutState->GetPageLength();

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return reference to global box list
///
/// @brief
/// Returns the global box list (all boxes in the document)
///
/////////////////////////////////////////////////////////////////////////////
const std::vector<sBoxes>& cPageManager::GetGlobalBoxList(void) const
{
    return mGlobalBoxList;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current box index, or NOT_SET if no current box
///
/// @brief
/// Returns the index into the global box list for the current box
///
/////////////////////////////////////////////////////////////////////////////
int cPageManager::GetCurrentBoxIndex(void) const
{
    return mCurrentBoxIndex;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return pointer to current box, or nullptr if no current box
///
/// @brief
/// Returns pointer to the current box in the global list
///
/////////////////////////////////////////////////////////////////////////////
const sBoxes* cPageManager::GetCurrentBox(void) const
{
    if (mCurrentBoxIndex < 0 || mCurrentBoxIndex >= static_cast<int>(mGlobalBoxList.size()))
    {
        return nullptr;
    }

    return &mGlobalBoxList[mCurrentBoxIndex];
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  boxIndex [in] index into global box list
///
/// @return pointer to box, or nullptr if index invalid
///
/// @brief
/// Returns pointer to a specific box by index
///
/////////////////////////////////////////////////////////////////////////////
const sBoxes* cPageManager::GetBoxByIndex(int boxIndex) const
{
    if (boxIndex < 0 || boxIndex >= static_cast<int>(mGlobalBoxList.size()))
    {
        return nullptr;
    }

    return &mGlobalBoxList[boxIndex];
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  contentLineNumber [in] global content line number (excludes dot-command lines)
///
/// @return pointer to box containing the line, nullptr if line not found
///
/// @brief
/// Finds which box contains a specific line.
///
/// NOTE: This method is a placeholder - actual implementation depends
/// on paragraph layout which is in cLayoutBase. Must be called via
/// cLayoutBase::GetBoxForLine() which has access to mParagraphLayout.
///
/////////////////////////////////////////////////////////////////////////////
const sBoxes* cPageManager::GetBoxForLine(LINE_T contentLineNumber) const
{
    (void)contentLineNumber;  // Unused - needs access to mParagraphLayout

    // This method needs to be called via cLayoutBase which has access
    // to mParagraphLayout. Left here as placeholder for future refactoring.
    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number (1-based)
///
/// @return vector of box indices for the specified page
///
/// @brief
/// Returns all box indices that belong to a specific page.
/// The vector will be empty if no boxes exist on the page.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<int> cPageManager::GetBoxesOnPage(PAGE_T page) const
{
    std::vector<int> boxes;

    for (size_t i = 0; i < mGlobalBoxList.size(); i++)
    {
        if (mGlobalBoxList[i].pageNumber == page)
        {
            boxes.push_back(static_cast<int>(i));
        }
    }

    return boxes;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return total number of boxes in the layout
///
/// @brief
/// Returns the count of boxes in the global box list.
///
/////////////////////////////////////////////////////////////////////////////
int cPageManager::GetBoxCount(void) const
{
    return static_cast<int>(mGlobalBoxList.size());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return pointer to current box (mutable), or nullptr if no current box
///
/// @brief
/// Returns mutable pointer to the current box in the global list.
/// Used when box needs to be modified (e.g., updating currentY).
///
/////////////////////////////////////////////////////////////////////////////
sBoxes* cPageManager::GetCurrentBoxMutable(void)
{
    if (mCurrentBoxIndex < 0 || mCurrentBoxIndex >= static_cast<int>(mGlobalBoxList.size()))
    {
        return nullptr;
    }

    return &mGlobalBoxList[mCurrentBoxIndex];
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  boxIndex [in] index into global box list
///
/// @return pointer to box (mutable), or nullptr if index invalid
///
/// @brief
/// Returns mutable pointer to a specific box by index
///
/////////////////////////////////////////////////////////////////////////////
sBoxes* cPageManager::GetBoxByIndexMutable(int boxIndex)
{
    if (boxIndex < 0 || boxIndex >= static_cast<int>(mGlobalBoxList.size()))
    {
        return nullptr;
    }

    return &mGlobalBoxList[boxIndex];
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  deltaY [in] amount to add to current box's Y coordinate
///
/// @brief
/// Updates the current box's Y coordinate by adding deltaY.
/// Used during layout to track vertical position within box.
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::UpdateCurrentBoxY(COORD_T deltaY)
{
    if (mCurrentBoxIndex >= 0 && mCurrentBoxIndex < static_cast<int>(mGlobalBoxList.size()))
    {
        mGlobalBoxList[mCurrentBoxIndex].currentY += deltaY;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return left coordinate of current box in twips
///
/// @brief
/// Returns the left edge of the current box.
/// Used during wrapping to determine where text can start.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cPageManager::GetBoxLeft(void) const
{
    return mBoxLeft;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return right coordinate of current box in twips
///
/// @brief
/// Returns the right edge of the current box.
/// Used during wrapping to determine where text must wrap.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cPageManager::GetBoxRight(void) const
{
    return mBoxRight;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return top coordinate of current box in twips
///
/// @brief
/// Returns the top edge of the current box.
/// Used during wrapping to calculate absolute Y positions.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cPageManager::GetBoxTop(void) const
{
    return mBoxTop;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bottom coordinate of current box in twips
///
/// @brief
/// Returns the bottom edge of the current box.
/// Used to detect when page is full and new box is needed.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cPageManager::GetBoxBottom(void) const
{
    return mBoxBottom;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rawLineNumber [in] global raw line number (every laid-out row, includes dot commands)
///
/// @return page number for the line, or NOT_SET if line not found
///
/// @brief
/// Finds which page contains a specific line.
///
/// NOTE: This method is a placeholder - actual implementation depends
/// on paragraph layout which is in cLayoutBase. Must be called via
/// cLayoutBase::GetPageFromLine() which has access to mParagraphLayout.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cPageManager::GetPageFromLine(LINE_T rawLineNumber) const
{
    (void)rawLineNumber;  // Unused - needs access to mParagraphLayout

    // This method needs to be called via cLayoutBase which has access
    // to mParagraphLayout. Left here as placeholder for future refactoring.
    return NOT_SET;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return highest page number in any box, or 0 if no boxes exist
///
/// @brief
/// Calculates the total number of pages by finding the highest page
/// number among all boxes in the global box list.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cPageManager::GetNumberOfPages(void) const
{
    PAGE_T maxPage = 0;

    for (const auto& box : mGlobalBoxList)
    {
        if (box.pageNumber > maxPage)
        {
            maxPage = box.pageNumber;
        }
    }

    return maxPage;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current logical page number
///
/// @brief
/// Returns the logical page number (for display, includes .PN offset)
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cPageManager::GetLogicalPageNumber(void) const
{
    return mLogicalPageNumber;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current page number
///
/// @brief
/// Returns the current physical page number during layout
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cPageManager::GetCurrentPage(void) const
{
    return mCurrentPage;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number to set
///
/// @brief
/// Sets the current physical page number
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::SetCurrentPage(PAGE_T page)
{
    mCurrentPage = page;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Increments the logical page number
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::IncrementLogicalPageNumber(void)
{
    mLogicalPageNumber++;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] logical page number to set
///
/// @brief
/// Sets the logical page number (for display, includes .PN offset)
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::SetLogicalPageNumber(PAGE_T page)
{
    mLogicalPageNumber = page;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  index [in] box index to set as current
///
/// @brief
/// Sets the current box index
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::SetCurrentBoxIndex(int index)
{
    mCurrentBoxIndex = index;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  boxIndex [in] index into global box list
///
/// @return nothing
///
/// @brief
/// Syncs margin tracking variables from an existing box.
/// Called during partial layout when restoring box state from a previous
/// paragraph's endState, so CheckMarginChange() compares against the
/// correct margins and does not create duplicate boxes.
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::SyncLastBoxMargins(int boxIndex)
{
    if (boxIndex < 0 || boxIndex >= static_cast<int>(mGlobalBoxList.size()))
    {
        return;
    }

    const sBoxes& box = mGlobalBoxList[boxIndex];

    // Restore margin tracking from the box's stored page info
    mLastBoxLeftMargin = box.pageInfo.leftmargin;
    mLastBoxRightMargin = box.pageInfo.rightmargin;
    mLastBoxTopMargin = box.pageInfo.topmargin;
    mLastBoxBottomMargin = box.pageInfo.bottommargin;

    // Derive page offset from box coordinates
    mLastBoxPageOffset = box.left - box.pageInfo.leftmargin;

    // Sync page number
    mLastPageNumberForBox = box.pageNumber;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return last page number we created a box for
///
/// @brief
/// Returns the last page number that had a box created for it
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cPageManager::GetLastPageNumberForBox(void) const
{
    return mLastPageNumberForBox;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number to remember
///
/// @brief
/// Sets the last page number we created a box for
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::SetLastPageNumberForBox(PAGE_T page)
{
    mLastPageNumberForBox = page;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  lineNumber [in] line number to add to current box
///
/// @brief
/// Adds a line to the current box's line list.
///
/// Updates the box's currentY coordinate to track vertical position.
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::AddLineToCurrentBox(LINE_T lineNumber)
{
    if (mCurrentBoxIndex >= 0 && mCurrentBoxIndex < static_cast<int>(mGlobalBoxList.size()))
    {
        mGlobalBoxList[mCurrentBoxIndex].containedLines.push_back(lineNumber);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Resets page manager for new layout.
///
/// Clears all boxes and resets page tracking.
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::Reset(void)
{
    mGlobalBoxList.clear();
    mCurrentBoxIndex = NOT_SET;
    mLastPageNumberForBox = 0;
    mCurrentPage = 1;
    mLogicalPageNumber = 1;
    mLastBoxLeftMargin = 0;
    mLastBoxRightMargin = 0;
    mLastBoxPageOffset = 0;
    mLastBoxTopMargin = 0;
    mLastBoxBottomMargin = 0;
    mLastBoxPageLength = 0;
    mBoxLeft = 0;
    mBoxRight = 0;
    mBoxTop = 0;
    mBoxBottom = 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Compact all internal containers to release excess allocated memory.
/// Shrinks the global box list and each box's contained-lines vector.
///
/////////////////////////////////////////////////////////////////////////////
void cPageManager::ShrinkToFit(void)
{
    mGlobalBoxList.shrink_to_fit();

    for (auto& box : mGlobalBoxList)
    {
        box.containedLines.shrink_to_fit();
    }
}
