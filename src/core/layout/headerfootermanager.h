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

#ifndef HEADERFOOTERMANAGER_H
#define HEADERFOOTERMANAGER_H

#include <array>
#include <map>
#include <vector>
#include <string>
#include "layoutstructs.h"
#include "src/core/include/config.h"

// Forward declarations
class cLayoutState;
class cLayoutBase;

#define MAX_HEADER_FOOTER 5

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sHeaderFooterLine
///
/// @brief
/// Pairs a layout line with its pre-rendered graphemes.
/// Header/footer text is not part of the document, so graphemes must be
/// stored separately rather than read from cDocument during rendering.
///
/////////////////////////////////////////////////////////////////////////////
struct sHeaderFooterLine
{
    sLineLayout line;
    std::vector<std::string> graphemes;     // Pre-rendered graphemes for display
};


class cHeaderFooterManager
{
public:
    cHeaderFooterManager(cLayoutState* state, cLayoutBase* layout);
    ~cHeaderFooterManager(void);

    // Header/Footer text storage
    void HandleHeaderFooterText(const std::string& text, const std::string& command);

    // Layout generation
    sHeaderFooterLine LayoutHeaderFooterText(const std::string& text, COORD_T ypos, PAGE_T page,
                                              PARAGRAPH_T sourceParagraph, POSITION_T sourceStartPos);
    void InsertHeadersFooters(PAGE_T page);

    // Access for rendering
    const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& GetPageHeaders(void) const;
    const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& GetPageFooters(void) const;

    // Current tracking
    int GetHeaderValue(void) const;
    int GetFooterValue(void) const;
    void SetHeaderValue(int value);
    void SetFooterValue(int value);

    // Reset for new layout
    void Reset(void);

    // Memory compaction
    void ShrinkToFit(void);

private:
    // Storage arrays for header/footer templates
    std::array<sLineLayout, MAX_HEADER_FOOTER> mStoreHeader;      // Regular headers (both pages)
    std::array<sLineLayout, MAX_HEADER_FOOTER> mStoreHeaderEven;  // Even page headers
    std::array<sLineLayout, MAX_HEADER_FOOTER> mStoreHeaderOdd;   // Odd page headers
    std::array<sLineLayout, MAX_HEADER_FOOTER> mStoreFooter;      // Regular footers (both pages)
    std::array<sLineLayout, MAX_HEADER_FOOTER> mStoreFooterEven;  // Even page footers
    std::array<sLineLayout, MAX_HEADER_FOOTER> mStoreFooterOdd;   // Odd page footers

    // Storage arrays for header/footer text (for re-layout with page numbers)
    std::array<std::string, MAX_HEADER_FOOTER> mStoreHeaderText;      // Regular headers text
    std::array<std::string, MAX_HEADER_FOOTER> mStoreHeaderEvenText;  // Even page headers text
    std::array<std::string, MAX_HEADER_FOOTER> mStoreHeaderOddText;   // Odd page headers text
    std::array<std::string, MAX_HEADER_FOOTER> mStoreFooterText;      // Regular footers text
    std::array<std::string, MAX_HEADER_FOOTER> mStoreFooterEvenText;  // Even page footers text
    std::array<std::string, MAX_HEADER_FOOTER> mStoreFooterOddText;   // Odd page footers text

    // Storage arrays for document position context (for control code lookup)
    // Paragraph number where header/footer was defined
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> mStoreHeaderParagraph;
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> mStoreHeaderEvenParagraph;
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> mStoreHeaderOddParagraph;
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> mStoreFooterParagraph;
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> mStoreFooterEvenParagraph;
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> mStoreFooterOddParagraph;
    // Starting position within paragraph (after ".HE " prefix)
    std::array<POSITION_T, MAX_HEADER_FOOTER> mStoreHeaderStartPos;
    std::array<POSITION_T, MAX_HEADER_FOOTER> mStoreHeaderEvenStartPos;
    std::array<POSITION_T, MAX_HEADER_FOOTER> mStoreHeaderOddStartPos;
    std::array<POSITION_T, MAX_HEADER_FOOTER> mStoreFooterStartPos;
    std::array<POSITION_T, MAX_HEADER_FOOTER> mStoreFooterEvenStartPos;
    std::array<POSITION_T, MAX_HEADER_FOOTER> mStoreFooterOddStartPos;

    // Current header/footer tracking
    int mHeaderValue;                  // Current header number (0 = none, 1-5 = H1-H5)
    int mFooterValue;                  // Current footer number (0 = none, 1-5 = F1-F5)

    // Per-page rendered headers/footers (with pre-rendered graphemes)
    std::map<PAGE_T, std::vector<sHeaderFooterLine>> mPageHeaders;
    std::map<PAGE_T, std::vector<sHeaderFooterLine>> mPageFooters;

    // References (NOT owned)
    cLayoutState* mLayoutState;
    cLayoutBase* mLayoutBase;
};

#endif // HEADERFOOTERMANAGER_H
