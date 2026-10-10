//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
// Copyright (C) 2026 Egbert H. Schroeer
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

#include "headerfootermanager.h"
#include "layoutbase.h"
#include "src/core/document/document.h"

#include <algorithm>
#include <numeric>


/////////////////////////////////////////////////////////////////////////////
///
/// @param  layout [in] pointer to layout base (NOT owned)
///
/// @return nothing
///
/// @brief
/// Constructor.
///
/////////////////////////////////////////////////////////////////////////////
cHeaderFooterManager::cHeaderFooterManager(cLayoutBase* layout)
    : mLayoutBase(layout)
{
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
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] raw dot command text (e.g., ".HE Header Text")
/// @param  command [in] uppercase command (e.g., "HE", "H1", "F2")
/// @param  number [in] header / footer number (1-5 for H1-H5 / F1-F5)
///
/// @return nothing
///
/// @brief
/// Parses a header / footer dot command and stores the definition on the
/// paragraph being laid out: side (both / even / odd), slot, text position,
/// definition page and the explicit .LH in effect.
///
/////////////////////////////////////////////////////////////////////////////
void cHeaderFooterManager::HandleHeaderFooterText(const std::string& text, const std::string& command, int number)
{
    if (command.empty() || number < 1 || number > MAX_HEADER_FOOTER)
    {
        return;
    }

    // Text starts after the command (".HE"), an optional E / O and whitespace
    size_t textStart = 3;
    eHeaderFooterSide side = HF_SIDE_BOTH;
    if (text.length() > 3)
    {
        char ch = text[3];
        if (ch == 'E' || ch == 'e')
        {
            side = HF_SIDE_EVEN;
            textStart = 4;
        }
        else if (ch == 'O' || ch == 'o')
        {
            side = HF_SIDE_ODD;
            textStart = 4;
        }
    }

    while (textStart < text.length() && (text[textStart] == ' ' || text[textStart] == '\t'))
    {
        textStart++;
    }

    sHeaderFooterDefinition definition;
    definition.page = mLayoutBase->GetCurrentPage();
    definition.startPos = static_cast<POSITION_T>(textStart);
    definition.lineHeight = mLayoutBase->ExplicitLineHeight();
    definition.side = side;
    definition.isHeader = (command[0] == 'H');
    definition.number = number;
    definition.appliesSamePage = (mLayoutBase->GetCurrentPageLineNumber() == 0);

    mLayoutBase->StoreHeaderFooterDefinition(definition);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page     [in] page number (1-based)
/// @param  isHeader [in] true for headers, false for footers
///
/// @return the page's header (or footer) lines, slot 1 first
///
/// @brief
/// Builds the header / footer lines of one page from the definitions in
/// effect for it. Headers stack down from the top of the header margin area,
/// footers up from the end of the footer margin area, each by its own line
/// height.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sHeaderFooterLine> cHeaderFooterManager::GetPageLines(PAGE_T page, bool isHeader)
{
    std::vector<sHeaderFooterLine> lines;
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> paragraphs = SelectDefinitions(page, isHeader);

    sPageInfo pageInfo = mLayoutBase->GetPageInfo(page);
    COORD_T headerY = pageInfo.topmargin - pageInfo.headermargin;
    COORD_T footerBottom = pageInfo.paperheight - pageInfo.bottommargin + pageInfo.footermargin;

    for (PARAGRAPH_T paragraph : paragraphs)
    {
        if (paragraph == NOT_SET)
        {
            continue;
        }
        const sParagraphLayout* paragraphLayout = mLayoutBase->GetParagraphLayout(paragraph);
        if (paragraphLayout == nullptr)
        {
            continue;
        }
        const sHeaderFooterDefinition& definition = paragraphLayout->headerFooter;
        if (isHeader)
        {
            sHeaderFooterLine line = LayoutHeaderFooterText(definition, paragraph, headerY, page);
            headerY += line.line.lineheight;
            lines.push_back(std::move(line));
        }
        else
        {
            sHeaderFooterLine line = LayoutHeaderFooterText(definition, paragraph, footerBottom, page);
            line.line.pagey = footerBottom - line.line.lineheight;
            footerBottom -= line.line.lineheight;
            lines.push_back(std::move(line));
        }
    }

    // Real WordStar 7's automatic page numbering (.PG, column set by .PC):
    // a page without a footer prints its page number by itself
    if (!isHeader && lines.empty() && pageInfo.printPageNumbers)
    {
        sHeaderFooterLine number = AutomaticPageNumber(page, footerBottom);
        if (!number.graphemes.empty())
        {
            lines.push_back(std::move(number));
        }
    }

    return lines;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page         [in] page number
/// @param  footerBottom [in] bottom of the footer area (absolute page Y)
///
/// @return the page number line (no graphemes when the number is empty)
///
/// @brief
/// The automatic page number: the page number in the default font, at the
/// column .PC set (0 = centered between the page's margins). No inline
/// formatting is supported here, matching the manual's own "Page numbers are
/// printed in the default font. To use another font, insert a footer."
///
/////////////////////////////////////////////////////////////////////////////
sHeaderFooterLine cHeaderFooterManager::AutomaticPageNumber(PAGE_T page, COORD_T footerBottom)
{
    sHeaderFooterLine result;
    sLineLayout& line = result.line;

    sPageInfo pageInfo = mLayoutBase->GetPageInfo(page);
    std::string pageNumStr = mLayoutBase->FormatPageNumber(page, mLayoutBase->mLayoutState->GetPageNumFormatForPage(page));
    if (pageNumStr.empty())
    {
        return result;
    }

    std::string font = mLayoutBase->mLayoutState->GetDefaultFont();
    COORD_T leftMargin = pageInfo.pageoffset + pageInfo.leftmargin;
    COORD_T rightMargin = pageInfo.pageoffset + pageInfo.rightmargin;
    COORD_T lineHeight = std::max(mLayoutBase->GetFontLineSpacing(font), static_cast<COORD_T>(0));
    if (lineHeight <= 0)
    {
        lineHeight = mLayoutBase->GetLineHeight();
    }

    COORD_T totalWidth = mLayoutBase->GetTextWidth(pageNumStr, font);
    COORD_T textX;
    if (pageInfo.pageNumberColumn == 0)
    {
        COORD_T remaining = (rightMargin - leftMargin) - totalWidth;
        if (remaining < 0)
        {
            remaining = 0;
        }
        textX = leftMargin + (remaining / 2);
    }
    else
    {
        textX = leftMargin + pageInfo.pageNumberColumn;
    }

    line.pagex = textX;
    line.pagey = footerBottom - lineHeight;
    line.screeny = 0;
    line.pagenumber = page;
    line.lineheight = lineHeight;
    line.left = true;

    sSegmentLayout segment;
    segment.font = font;
    segment.paragraph = 0;
    segment.startPosition = 0;
    segment.length = static_cast<POSITION_T>(pageNumStr.size());
    segment.totalWidth = totalWidth;
    segment.segmentheight = lineHeight;

    COORD_T runningX = 0;
    for (char ch : pageNumStr)
    {
        std::string glyph(1, ch);
        result.graphemes.push_back(glyph);
        segment.position.push_back(runningX);
        runningX += mLayoutBase->GetTextWidth(glyph, font);
    }

    line.segments.push_back(segment);
    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  definition [in] - Stored header / footer definition (text
///                            position, .LH at the definition)
/// @param  paragraph [in] - Paragraph holding the definition
/// @param  ypos [in] - Vertical position for this line (absolute page Y)
/// @param  page [in] - Page number
///
/// @return sHeaderFooterLine - Laid out line with absolute coordinates and pre-rendered graphemes
///
/// @brief
/// Lays out one header / footer line for a page: segments from
/// BuildHeaderFooterSegments() (same control-code handling as body text,
/// '#' expanded to the page number), X from the page's offset and left
/// margin, centre / right alignment within the page's margins, and the line
/// height.
///
/////////////////////////////////////////////////////////////////////////////
sHeaderFooterLine cHeaderFooterManager::LayoutHeaderFooterText(const sHeaderFooterDefinition& definition,
                                                              PARAGRAPH_T paragraph,
                                                              COORD_T ypos,
                                                              PAGE_T page)
{

    sHeaderFooterLine result;
    sLineLayout& line = result.line;

    // Page geometry in effect for this page (not the state at the end of the layout)
    sPageInfo pageInfo = mLayoutBase->GetPageInfo(page);
    COORD_T headerX = pageInfo.pageoffset + pageInfo.leftmargin;

    // Set position
    line.pagex = headerX;
    line.pagey = ypos;
    line.screeny = 0;  // Will be calculated during display
    line.pagenumber = page;
    line.left = true;  // Default left-aligned

    // Get document for grapheme extraction
    cDocument* document = mLayoutBase->GetDocument();
    if (document == nullptr)
    {
        // No document means nothing to render
        return result;
    }

    // Segments and pre-rendered graphemes (# expanded); the builder starts in the
    // default font and leaves the layout state unchanged
    std::vector<std::string> graphemes;
    std::vector<sSegmentLayout> segments = mLayoutBase->BuildHeaderFooterSegments(
        paragraph, definition.startPos, page, graphemes);

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

    // Line height of this header / footer line (the .LH at its definition, or its own fonts)
    line.lineheight = HeaderFooterLineHeight(definition, line.segments);

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
        COORD_T lineWidth = pageInfo.rightmargin - pageInfo.leftmargin;
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
/// @param  page     [in] page being drawn
/// @param  isHeader [in] true for headers, false for footers
///
/// @return per slot (1-5), the paragraph holding the definition in effect,
///         or NOT_SET
///
/// @brief
/// A header / footer stays in effect until it is redefined: for each slot the
/// result is the latest definition in the document made on an earlier page
/// whose side (both / even / odd) matches the page. A definition never
/// applies to the page that defines it.
///
/////////////////////////////////////////////////////////////////////////////
std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> cHeaderFooterManager::SelectDefinitions(PAGE_T page, bool isHeader) const
{
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> selected;
    selected.fill(NOT_SET);
    bool isEvenPage = (page % 2 == 0);

    PARAGRAPH_T paragraphs = mLayoutBase->GetNumberOfParagraphs();
    for (PARAGRAPH_T para = 0; para < paragraphs; ++para)
    {
        const sParagraphLayout* layout = mLayoutBase->GetParagraphLayout(para);
        if (layout == nullptr || !layout->hasHeaderFooter)
        {
            continue;
        }
        const sHeaderFooterDefinition& definition = layout->headerFooter;
        bool sideMatches = (definition.side == HF_SIDE_BOTH) ||
                           (definition.side == HF_SIDE_EVEN && isEvenPage) ||
                           (definition.side == HF_SIDE_ODD && !isEvenPage);
        bool applies = definition.isHeader == isHeader &&
                       sideMatches &&
                       (definition.page < page || (definition.appliesSamePage && definition.page == page)) &&
                       definition.number >= 1 &&
                       definition.number <= MAX_HEADER_FOOTER;
        if (applies)
        {
            selected[static_cast<size_t>(definition.number - 1)] = para;
        }
    }

    return selected;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  definition [in] the header / footer definition
/// @param  segments   [in] segments of the header / footer line
///
/// @return line height of a header / footer line
///
/// @brief
/// An explicit .LH in effect where the header / footer was defined applies;
/// a later .LH does not. Otherwise the line is as tall as the largest font
/// line spacing among its own segments.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cHeaderFooterManager::HeaderFooterLineHeight(const sHeaderFooterDefinition& definition,
                                                     const std::deque<sSegmentLayout>& segments) const
{
    if (definition.lineHeight > 0)
    {
        return definition.lineHeight;
    }

    COORD_T height = std::accumulate(segments.begin(), segments.end(), static_cast<COORD_T>(0),
        [this](COORD_T tallest, const sSegmentLayout& segment)
        {
            return std::max(tallest, mLayoutBase->GetFontLineSpacing(segment.font));
        });

    if (height <= 0)
    {
        height = mLayoutBase->GetLineHeight();
    }
    return height;
}
