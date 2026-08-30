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

#ifndef PAGEMANAGER_H
#define PAGEMANAGER_H

#include <vector>
#include "layoutstructs.h"
#include "src/core/include/config.h"

// Forward declarations
class cLayoutState;
class cDocument;


class cPageManager
{
public:
    cPageManager(cLayoutState* state);
    ~cPageManager(void);

    // Box management
    bool CreatePageBox(PAGE_T page);
    bool CreateMarginBox(PAGE_T page);
    bool CheckMarginChange(void);
    bool CheckPageChange(void);
    bool UpdatePageBox(void);

    // Page break detection
    bool CheckPageBreak(PARAGRAPH_T paragraphNum, cDocument* doc);
    bool NeedNewPage(COORD_T lineHeight);

    // Box queries (const)
    const std::vector<sBoxes>& GetGlobalBoxList(void) const;
    int GetCurrentBoxIndex(void) const;
    const sBoxes* GetCurrentBox(void) const;
    const sBoxes* GetBoxByIndex(int boxIndex) const;
    const sBoxes* GetBoxForLine(LINE_T contentLineNumber) const;
    std::vector<int> GetBoxesOnPage(PAGE_T page) const;
    int GetBoxCount(void) const;

    // Box queries (mutable)
    sBoxes* GetCurrentBoxMutable(void);
    sBoxes* GetBoxByIndexMutable(int boxIndex);

    // Box modification
    void UpdateCurrentBoxY(COORD_T deltaY);

    // Box coordinate getters
    COORD_T GetBoxLeft(void) const;
    COORD_T GetBoxRight(void) const;
    COORD_T GetBoxTop(void) const;
    COORD_T GetBoxBottom(void) const;

    // Page queries
    PAGE_T GetPageFromLine(LINE_T rawLineNumber) const;
    PAGE_T GetNumberOfPages(void) const;
    PAGE_T GetLogicalPageNumber(void) const;

    // Page tracking (called by cLayoutBase)
    PAGE_T GetCurrentPage(void) const;
    void SetCurrentPage(PAGE_T page);
    void IncrementLogicalPageNumber(void);
    void SetLogicalPageNumber(PAGE_T page);

    // Box tracking
    void SetCurrentBoxIndex(int index);
    void SyncLastBoxMargins(int boxIndex);
    PAGE_T GetLastPageNumberForBox(void) const;
    void SetLastPageNumberForBox(PAGE_T page);
    void AddLineToCurrentBox(LINE_T lineNumber);

    // Reset for new layout
    void Reset(void);

    // Memory compaction
    void ShrinkToFit(void);

private:
    std::vector<sBoxes> mGlobalBoxList;
    int mCurrentBoxIndex;
    PAGE_T mLastPageNumberForBox;
    PAGE_T mCurrentPage;
    PAGE_T mLogicalPageNumber;

    // Content-flow margin tracking (left/right - triggers CreateMarginBox)
    COORD_T mLastBoxLeftMargin;
    COORD_T mLastBoxRightMargin;
    COORD_T mLastBoxPageOffset;

    // Page-level margin tracking (top/bottom/pageLength - triggers UpdatePageBox)
    COORD_T mLastBoxTopMargin;
    COORD_T mLastBoxBottomMargin;
    COORD_T mLastBoxPageLength;

    // Current box coordinates
    COORD_T mBoxLeft;
    COORD_T mBoxRight;
    COORD_T mBoxTop;
    COORD_T mBoxBottom;

    cLayoutState* mLayoutState;  // NOT owned
};

#endif // PAGEMANAGER_H
