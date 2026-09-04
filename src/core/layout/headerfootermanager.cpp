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
 * @class cHeaderFooterManager
 *
 * @brief Manages header and footer definitions, storage, and insertion for page layout.
 *
 * Implements the cHeaderFooterManager class which handles parsing of WordStar
 * header/footer dot commands (.HE, .FO, .H1-.H4, .F1-.F4, and even/odd
 * variants), stores their text and associated page numbers, and inserts the
 * appropriate header/footer content during page layout.
 *
 * @section headerfooter_commands Supported Dot Commands
 * - .HE / .FO: default header and footer text for all pages
 * - .H1-.H4 / .F1-.F4: up to four header/footer levels with priority
 * - Even/odd variants: separate content for even and odd pages
 * - Page number token (#) in header/footer text is replaced with the
 *   current page number during rendering
 *
 * @section headerfooter_storage Storage Model
 * Each header/footer level is stored as an sHeaderFooterLine containing the
 * text, associated starting page number, and even/odd page flag. Up to
 * MAX_HEADER_FOOTER levels are supported, with the highest applicable level
 * taking priority during rendering.
 *
 * @section headerfooter_rendering Rendering Pipeline
 * During page layout, InsertHeader/InsertFooter methods are called per page.
 * The manager selects the appropriate header/footer text based on page number
 * and even/odd status, measures it using the layout engine's text measurement
 * facilities, and positions it within the header/footer margin area.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see sHeaderFooterLine Per-level header/footer storage structure
 * @see cLayoutState Layout state providing margin and page geometry
 * @see cLayoutBase Layout engine coordinating header/footer insertion
 */

#include "headerfootermanager.h"
#include "layoutstate.h"
#include "layoutbase.h"
#include "src/core/document/document.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in] pointer to layout state (NOT owned)
/// @param  layout [in] pointer to layout base for text measurement (NOT owned)
///
/// @return nothing
///
/// @brief
/// Constructor. Initializes header/footer manager with external dependencies.
///
/////////////////////////////////////////////////////////////////////////////
cHeaderFooterManager::cHeaderFooterManager(cLayoutState* state, cLayoutBase* layout)
    : mHeaderValue(0),
      mFooterValue(0),
      mLayoutState(state),
      mLayoutBase(layout)
{
    // Initialize all header/footer arrays to empty
    for (int i = 0; i < MAX_HEADER_FOOTER; i++)
    {
        mStoreHeader[i].pagenumber = 0;
        mStoreHeaderEven[i].pagenumber = 0;
        mStoreHeaderOdd[i].pagenumber = 0;
        mStoreFooter[i].pagenumber = 0;
        mStoreFooterEven[i].pagenumber = 0;
        mStoreFooterOdd[i].pagenumber = 0;

        mStoreHeaderText[i] = "";
        mStoreHeaderEvenText[i] = "";
        mStoreHeaderOddText[i] = "";
        mStoreFooterText[i] = "";
        mStoreFooterEvenText[i] = "";
        mStoreFooterOddText[i] = "";

        // Initialize document position arrays
        mStoreHeaderParagraph[i] = 0;
        mStoreHeaderEvenParagraph[i] = 0;
        mStoreHeaderOddParagraph[i] = 0;
        mStoreFooterParagraph[i] = 0;
        mStoreFooterEvenParagraph[i] = 0;
        mStoreFooterOddParagraph[i] = 0;

        mStoreHeaderStartPos[i] = 0;
        mStoreHeaderEvenStartPos[i] = 0;
        mStoreHeaderOddStartPos[i] = 0;
        mStoreFooterStartPos[i] = 0;
        mStoreFooterEvenStartPos[i] = 0;
        mStoreFooterOddStartPos[i] = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor. Nothing to clean up (references not owned).
///
/////////////////////////////////////////////////////////////////////////////
cHeaderFooterManager::~cHeaderFooterManager(void)
{
    // Nothing to delete (mLayoutState and mLayoutBase not owned)
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] raw dot command text (e.g., ".HE Header Text")
/// @param  command [in] uppercase command (e.g., "HE", "H1", "F2")
///
/// @return nothing
///
/// @brief
/// Processes header/footer dot command text and stores it for later rendering.
///
/// Parses commands like .HE, .HEE, .HEO, .FE, .FEE, .FEO
/// Stores text templates with even/odd/both page variants.
/// Text can contain '#' which will be replaced with page number during layout.
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::HandleHeaderFooterText(const std::string& text, const std::string& command)
{
    // Determine if this is a header or footer
    bool isHeader = (command[0] == 'H');
    int value = isHeader ? mHeaderValue : mFooterValue;

    if (value < 1 || value > MAX_HEADER_FOOTER)
    {
        return;  // Invalid value
    }

    // Create a line layout structure to store the text
    sLineLayout line;
    line.pagenumber = mLayoutBase->GetCurrentPage();  // Store page where header/footer was DEFINED
    line.left = true;                // Default left alignment

    // Extract the text content
    // Format: .HE text  or  .HEE text  or  .HEO text
    // Text starts after command and optional space
    size_t textStart = 3;  // After ".HE"

    // Check for even/odd specifier at position 3
    std::string pageType = "B";  // B = both, E = even, O = odd (internal flag)
    if (text.length() > 3)
    {
        char ch = text[3];
        if (ch == 'E' || ch == 'e')
        {
            pageType = "E";
            textStart = 4;  // Skip the E
        }
        else if (ch == 'O' || ch == 'o')
        {
            pageType = "O";
            textStart = 4;  // Skip the O
        }
    }

    // Skip leading whitespace after command
    while (textStart < text.length() && (text[textStart] == ' ' || text[textStart] == '\t'))
    {
        textStart++;
    }

    // Extract the text content
    std::string content;
    if (textStart < text.length())
    {
        content = text.substr(textStart);
    }

    // Store in line (we'll layout later)
    // Store the raw text in a segment
    sSegmentLayout segment;
    segment.font = mLayoutState->GetCurrentFont();
    segment.paragraph = mLayoutBase->GetCurrentParagraph();

    // Position-based access
    // Headers/footers not part of document, so startPosition is 0
    segment.startPosition = 0;
    segment.length = 0;

    // This template segment is a placeholder: rendering rebuilds the real
    // segments from the document via BuildHeaderFooterSegments(), so its length
    // is never consumed (only segments.empty() is checked). Do not count it here
    // (counting bytes would also violate the cDocument grapheme black-box rule).
    line.segments.push_back(segment);

    // Store in appropriate array
    int arrayIndex = value - 1;  // Convert to 0-based index

    // Store document position context for later control code lookup
    PARAGRAPH_T currentParagraph = mLayoutBase->GetCurrentParagraph();
    POSITION_T startPos = static_cast<POSITION_T>(textStart);  // Position within paragraph where content starts

    line.appliesSamePage = (mLayoutBase->GetCurrentPageLineNumber() == 0);

    if (isHeader)
    {
        if (pageType == "E")
        {
            mStoreHeaderEven[arrayIndex] = line;
            mStoreHeaderEvenText[arrayIndex] = content;
            mStoreHeaderEvenParagraph[arrayIndex] = currentParagraph;
            mStoreHeaderEvenStartPos[arrayIndex] = startPos;
        }
        else if (pageType == "O")
        {
            mStoreHeaderOdd[arrayIndex] = line;
            mStoreHeaderOddText[arrayIndex] = content;
            mStoreHeaderOddParagraph[arrayIndex] = currentParagraph;
            mStoreHeaderOddStartPos[arrayIndex] = startPos;
        }
        else
        {
            mStoreHeader[arrayIndex] = line;
            mStoreHeaderText[arrayIndex] = content;
            mStoreHeaderParagraph[arrayIndex] = currentParagraph;
            mStoreHeaderStartPos[arrayIndex] = startPos;
        }
    }
    else
    {
        if (pageType == "E")
        {
            mStoreFooterEven[arrayIndex] = line;
            mStoreFooterEvenText[arrayIndex] = content;
            mStoreFooterEvenParagraph[arrayIndex] = currentParagraph;
            mStoreFooterEvenStartPos[arrayIndex] = startPos;
        }
        else if (pageType == "O")
        {
            mStoreFooterOdd[arrayIndex] = line;
            mStoreFooterOddText[arrayIndex] = content;
            mStoreFooterOddParagraph[arrayIndex] = currentParagraph;
            mStoreFooterOddStartPos[arrayIndex] = startPos;
        }
        else
        {
            mStoreFooter[arrayIndex] = line;
            mStoreFooterText[arrayIndex] = content;
            mStoreFooterParagraph[arrayIndex] = currentParagraph;
            mStoreFooterStartPos[arrayIndex] = startPos;
        }
    }

    // Reset header/footer value for next paragraph
    if (isHeader)
    {
        mHeaderValue = 0;
    }
    else
    {
        mFooterValue = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] - Text content to layout (may contain # for page number)
/// @param  ypos [in] - Vertical position for this line (absolute page Y)
/// @param  page [in] - Page number
/// @param  sourceParagraph [in] - Paragraph where header/footer was defined
/// @param  sourceStartPos [in] - Position within paragraph where text starts
///
/// @return sHeaderFooterLine - Laid out line with absolute coordinates and pre-rendered graphemes
///
/// @brief
/// Layouts header or footer text at specified position.
/// Processes control codes (MARKER_CHAR) from document for formatting.
/// Replaces # with formatted page number, &@& with date, etc.
///
/// Proper segmentation:
///   - Reads graphemes from document for control code lookup
///   - Creates new segment when formatting changes
///   - Each segment has correct font attributes
///
/// Now delegates to BuildHeaderFooterSegments() in cLayout which uses
/// the same segmentation logic as body text (BuildParagraphSegments).
/// This ensures headers/footers support all control codes, fonts, colors, etc.
///
/////////////////////////////////////////////////////////////////////////////
sHeaderFooterLine cHeaderFooterManager::LayoutHeaderFooterText(const std::string& text, COORD_T ypos, PAGE_T page,
                                                                PARAGRAPH_T sourceParagraph, POSITION_T sourceStartPos)
{
    UNUSED_ARGUMENT(text);  // Text is read from document at sourceParagraph/sourceStartPos

    sHeaderFooterLine result;
    sLineLayout& line = result.line;

    // Calculate correct X position for this specific page
    COORD_T pageOffset = (page % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();
    COORD_T headerX = pageOffset + mLayoutState->GetLeftMargin();

    // Set position
    line.pagex = headerX;
    line.pagey = ypos;
    line.screeny = 0;  // Will be calculated during display
    line.pagenumber = page;
    line.lineheight = mLayoutBase->GetLineHeight();
    line.left = true;  // Default left-aligned

    // Get document for grapheme extraction
    cDocument* document = mLayoutBase->GetDocument();
    if (document == nullptr)
    {
        // No document means nothing to render
        return result;
    }

    // Get segments and pre-rendered graphemes using same logic as body text
    // This handles all control codes: fonts, bold, italic, tabs, variables, etc.
    // The graphemes are returned with # already expanded to page number
    std::vector<std::string> graphemes;
    std::vector<sSegmentLayout> segments = mLayoutBase->BuildHeaderFooterSegments(
        sourceParagraph, sourceStartPos, page, graphemes);

    // Use the pre-rendered graphemes directly (# already expanded)
    result.graphemes = std::move(graphemes);

    // Transfer segments to line
    for (auto& seg : segments)
    {
        // Check for center/right line flags from tab segments
        if (seg.isTab)
        {
            if (seg.tabType == TAB_CENTER)
            {
                line.centerLine = true;
            }
            else if (seg.tabType == TAB_RIGHT || seg.tabType == TAB_RIGHT1)
            {
                line.rightLine = true;
            }
            // Don't add tab segments to line - they're just markers
            continue;
        }

        line.segments.push_back(seg);
    }

    // Apply alignment offset for centerLine/rightLine (^Oc / ^O])
    if (line.centerLine || line.rightLine)
    {
        // Calculate line content width
        COORD_T contentWidth = 0;
        for (const auto& seg : line.segments)
        {
            contentWidth += seg.totalWidth;
        }

        // Calculate available width (right margin - left margin)
        COORD_T lineWidth = mLayoutState->GetRightMargin() - mLayoutState->GetLeftMargin();
        COORD_T remainingSpace = lineWidth - contentWidth;
        if (remainingSpace < 0)
        {
            remainingSpace = 0;
        }

        // Determine alignment offset
        COORD_T alignmentOffset = 0;
        if (line.centerLine && line.rightLine)
        {
            alignmentOffset = remainingSpace;  // Right takes precedence
        }
        else if (line.centerLine)
        {
            alignmentOffset = remainingSpace / 2;
        }
        else if (line.rightLine)
        {
            alignmentOffset = remainingSpace;
        }

        // Adjust segment positions by alignment offset
        for (auto& seg : line.segments)
        {
            for (auto& pos : seg.position)
            {
                pos += alignmentOffset;
            }
        }
    }

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number to insert headers/footers for
///
/// @return nothing
///
/// @brief
/// Inserts appropriate headers and footers for the specified page.
///
/// Selects even/odd/both variants based on page number.
/// Headers/footers apply to pages after they are defined, and to their own
/// defining page too if nothing has been laid out on it yet.
/// Stores rendered lines in mPageHeaders and mPageFooters maps.
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::InsertHeadersFooters(PAGE_T page)
{
    // Determine even/odd page
    bool isEvenPage = (page % 2 == 0);

    // Calculate Y positions
    // Headers go at top margin (start of header margin area)
    COORD_T headerY = mLayoutState->GetTopMargin() - mLayoutState->GetHeaderMargin();

    // Footers go at bottom, one line up from bottom margin
    // (at the end of footer margin area)
    COORD_T footerY = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin() + mLayoutState->GetFooterMargin() - mLayoutState->GetLineHeight();

    // Process headers (H1 through H5)
    for (int i = 0; i < MAX_HEADER_FOOTER; i++)
    {
        sLineLayout *headerTemplate = nullptr;
        std::string headerText = "";
        PARAGRAPH_T headerParagraph = 0;
        POSITION_T headerStartPos = 0;

        // Select appropriate template, text, and document position (priority: even/odd > both)
        if (isEvenPage && mStoreHeaderEven[i].pagenumber != 0)
        {
            headerTemplate = &mStoreHeaderEven[i];
            headerText = mStoreHeaderEvenText[i];
            headerParagraph = mStoreHeaderEvenParagraph[i];
            headerStartPos = mStoreHeaderEvenStartPos[i];
        }
        else if (!isEvenPage && mStoreHeaderOdd[i].pagenumber != 0)
        {
            headerTemplate = &mStoreHeaderOdd[i];
            headerText = mStoreHeaderOddText[i];
            headerParagraph = mStoreHeaderOddParagraph[i];
            headerStartPos = mStoreHeaderOddStartPos[i];
        }
        else if (mStoreHeader[i].pagenumber != 0)
        {
            headerTemplate = &mStoreHeader[i];
            headerText = mStoreHeaderText[i];
            headerParagraph = mStoreHeaderParagraph[i];
            headerStartPos = mStoreHeaderStartPos[i];
        }

        // If we have a template, layout and store it.
        if (headerTemplate != nullptr && !headerTemplate->segments.empty())
        {
            bool headerApplies = (headerTemplate->pagenumber < page)
                || (headerTemplate->appliesSamePage && headerTemplate->pagenumber == page);
            if (headerApplies)
            {
                // Layout at header position using stored text and document position for control code lookup
                sHeaderFooterLine headerLine = LayoutHeaderFooterText(headerText, headerY, page, headerParagraph, headerStartPos);
                mPageHeaders[page].push_back(headerLine);

                // Advance Y for next header
                headerY += mLayoutBase->GetLineHeight();
            }
        }
    }

    // Process footers (F1 through F5)
    for (int i = 0; i < MAX_HEADER_FOOTER; i++)
    {
        sLineLayout *footerTemplate = nullptr;
        std::string footerText = "";
        PARAGRAPH_T footerParagraph = 0;
        POSITION_T footerStartPos = 0;

        // Select appropriate template, text, and document position (priority: even/odd > both)
        if (isEvenPage && mStoreFooterEven[i].pagenumber != 0)
        {
            footerTemplate = &mStoreFooterEven[i];
            footerText = mStoreFooterEvenText[i];
            footerParagraph = mStoreFooterEvenParagraph[i];
            footerStartPos = mStoreFooterEvenStartPos[i];
        }
        else if (!isEvenPage && mStoreFooterOdd[i].pagenumber != 0)
        {
            footerTemplate = &mStoreFooterOdd[i];
            footerText = mStoreFooterOddText[i];
            footerParagraph = mStoreFooterOddParagraph[i];
            footerStartPos = mStoreFooterOddStartPos[i];
        }
        else if (mStoreFooter[i].pagenumber != 0)
        {
            footerTemplate = &mStoreFooter[i];
            footerText = mStoreFooterText[i];
            footerParagraph = mStoreFooterParagraph[i];
            footerStartPos = mStoreFooterStartPos[i];
        }

        // If we have a template, layout and store it.
        if (footerTemplate != nullptr && !footerTemplate->segments.empty())
        {
            bool footerApplies = (footerTemplate->pagenumber < page)
                || (footerTemplate->appliesSamePage && footerTemplate->pagenumber == page);
            if (footerApplies)
            {
                // Layout at footer position using stored text and document position for control code lookup
                sHeaderFooterLine footerLine = LayoutHeaderFooterText(footerText, footerY, page, footerParagraph, footerStartPos);
                mPageFooters[page].push_back(footerLine);

                // Move Y up for next footer (footers stack from bottom)
                footerY -= mLayoutBase->GetLineHeight();
            }
        }
    }

    InsertAutomaticPageNumber(page, footerY);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number to insert the automatic page number for
/// @param  footerY [in] Y position for the footer area (unchanged from
///                      InsertHeadersFooters if no real footer applied)
///
/// @return nothing
///
/// @brief
/// Real WordStar 7's automatic page numbering (.pg, position set by .pc):
/// when enabled and no real .fo footer occupies this page, prints the page
/// number by itself, in the current font, at the column .pc set (0 = the
/// default, centered between the margins). No inline formatting is
/// supported here, matching the manual's own "Page numbers are printed in
/// the default font. To use another font, insert a footer."
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::InsertAutomaticPageNumber(PAGE_T page, COORD_T footerY)
{
    if (mLayoutState->ShouldPrintPageNumbers() == false)
    {
        return;
    }

    if (mPageFooters[page].empty() == false)
    {
        return;
    }

    std::string pageNumStr = mLayoutBase->FormatPageNumber(page, mLayoutState->GetPageNumFormatForPage(page));
    if (pageNumStr.empty())
    {
        return;
    }

    bool isEvenPage = (page % 2 == 0);
    COORD_T pageOffset = isEvenPage ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();
    COORD_T leftMargin = pageOffset + mLayoutState->GetLeftMargin();
    COORD_T rightMargin = pageOffset + mLayoutState->GetRightMargin();

    COORD_T totalWidth = mLayoutBase->GetTextWidth(pageNumStr);
    COORD_T column = mLayoutState->GetPageNumberColumn();
    COORD_T textX;

    if (column == 0)
    {
        COORD_T available = rightMargin - leftMargin;
        COORD_T remaining = available - totalWidth;
        if (remaining < 0)
        {
            remaining = 0;
        }
        textX = leftMargin + (remaining / 2);
    }
    else
    {
        textX = leftMargin + column;
    }

    sHeaderFooterLine result;
    sLineLayout& line = result.line;

    line.pagex = textX;
    line.pagey = footerY;
    line.screeny = 0;
    line.pagenumber = page;
    line.lineheight = mLayoutBase->GetLineHeight();
    line.left = true;

    sSegmentLayout segment;
    segment.font = mLayoutState->GetCurrentFont();
    segment.paragraph = 0;
    segment.startPosition = 0;
    segment.length = static_cast<POSITION_T>(pageNumStr.size());
    segment.totalWidth = totalWidth;

    COORD_T runningX = 0;
    for (char ch : pageNumStr)
    {
        std::string glyph(1, ch);
        result.graphemes.push_back(glyph);
        segment.position.push_back(static_cast<float>(runningX));
        runningX += mLayoutBase->GetTextWidth(glyph);
    }

    line.segments.push_back(segment);

    mPageFooters[page].push_back(result);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return reference to page headers map
///
/// @brief
/// Returns map of page numbers to their header lines.
///
/////////////////////////////////////////////////////////////////////////////
const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& cHeaderFooterManager::GetPageHeaders(void) const
{
    return mPageHeaders;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return reference to page footers map
///
/// @brief
/// Returns map of page numbers to their footer lines.
///
/////////////////////////////////////////////////////////////////////////////
const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& cHeaderFooterManager::GetPageFooters(void) const
{
    return mPageFooters;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current header value (0-5)
///
/// @brief
/// Returns current header number being processed (0 = none, 1-5 = H1-H5).
///
/////////////////////////////////////////////////////////////////////////////
int cHeaderFooterManager::GetHeaderValue(void) const
{
    return mHeaderValue;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current footer value (0-5)
///
/// @brief
/// Returns current footer number being processed (0 = none, 1-5 = F1-F5).
///
/////////////////////////////////////////////////////////////////////////////
int cHeaderFooterManager::GetFooterValue(void) const
{
    return mFooterValue;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  value [in] header number (0-5)
///
/// @return nothing
///
/// @brief
/// Sets current header number for next header text command.
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::SetHeaderValue(int value)
{
    mHeaderValue = value;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  value [in] footer number (0-5)
///
/// @return nothing
///
/// @brief
/// Sets current footer number for next footer text command.
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::SetFooterValue(int value)
{
    mFooterValue = value;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Resets header/footer state for new layout pass.
/// Clears all stored templates and rendered pages.
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::Reset(void)
{
    // Reset header/footer values
    mHeaderValue = 0;
    mFooterValue = 0;

    // Clear all stored templates, text, and document positions
    for (int i = 0; i < MAX_HEADER_FOOTER; i++)
    {
        mStoreHeader[i] = sLineLayout();
        mStoreHeaderEven[i] = sLineLayout();
        mStoreHeaderOdd[i] = sLineLayout();
        mStoreFooter[i] = sLineLayout();
        mStoreFooterEven[i] = sLineLayout();
        mStoreFooterOdd[i] = sLineLayout();

        mStoreHeaderText[i] = "";
        mStoreHeaderEvenText[i] = "";
        mStoreHeaderOddText[i] = "";
        mStoreFooterText[i] = "";
        mStoreFooterEvenText[i] = "";
        mStoreFooterOddText[i] = "";

        mStoreHeaderParagraph[i] = 0;
        mStoreHeaderEvenParagraph[i] = 0;
        mStoreHeaderOddParagraph[i] = 0;
        mStoreFooterParagraph[i] = 0;
        mStoreFooterEvenParagraph[i] = 0;
        mStoreFooterOddParagraph[i] = 0;

        mStoreHeaderStartPos[i] = 0;
        mStoreHeaderEvenStartPos[i] = 0;
        mStoreHeaderOddStartPos[i] = 0;
        mStoreFooterStartPos[i] = 0;
        mStoreFooterEvenStartPos[i] = 0;
        mStoreFooterOddStartPos[i] = 0;
    }

    // Clear rendered pages
    mPageHeaders.clear();
    mPageFooters.clear();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Compact all internal containers to release excess allocated memory.
/// Shrinks the per-page header/footer vectors and their nested grapheme
/// vectors.
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::ShrinkToFit(void)
{
    // Shrink per-page header line vectors and nested graphemes
    for (auto& [page, lines] : mPageHeaders)
    {
        lines.shrink_to_fit();
        for (auto& hfLine : lines)
        {
            hfLine.graphemes.shrink_to_fit();
        }
    }

    // Shrink per-page footer line vectors and nested graphemes
    for (auto& [page, lines] : mPageFooters)
    {
        lines.shrink_to_fit();
        for (auto& hfLine : lines)
        {
            hfLine.graphemes.shrink_to_fit();
        }
    }
}
