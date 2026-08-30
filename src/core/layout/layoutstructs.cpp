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
 * @file layoutstructs.cpp
 *
 * @brief Implements methods for the layout data structures used by the layout engine.
 *
 * Provides the non-trivial member functions for the layout engine's output
 * data structures. These structures form the complete representation of a
 * laid-out document, from individual grapheme segments up to the full
 * display list visible in the viewport.
 *
 * @section layoutstructs_hierarchy Structure Hierarchy
 * - sSegmentLayout: individual grapheme with position, width, and formatting
 * - sLineLayout: horizontal line containing one or more segments, with Y position
 * - sParagraphLayout: one document paragraph containing one or more lines
 * - sBoxes: page box (usually one per page) containing paragraph ranges
 * - sPage: page metadata (physical/logical page numbers, box list)
 * - sViewport: visible rectangle in screen coordinates for culling
 * - sDisplayList: final renderable list built from viewport intersection
 *
 * @section layoutstructs_segments Segment Operations
 * - On-demand grapheme fetching: GetGraphemes() lazily loads grapheme data
 *   from cDocument to avoid storing duplicate text in layout structures
 * - Grapheme counting: GetGraphemeCount() returns the number of graphemes
 *   in the segment's document position range
 * - Equality comparison for incremental re-layout change detection
 *
 * @section layoutstructs_boxes Page Box Management
 * - CalculateScreenYRange(): computes screenYTop/screenYBottom from contained
 *   lines for viewport intersection testing
 * - Dirty tracking: marks boxes needing re-render after layout changes
 *
 * @section layoutstructs_display Display List
 * - sDisplayList::Build(): intersects sViewport with layout data to produce
 *   the minimal set of boxes, paragraphs, lines, and segments visible
 *   in the current scroll position
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see sBoxes Page box geometry and dirty tracking
 * @see sSegmentLayout Per-grapheme layout data
 * @see sLineLayout Per-line layout data
 * @see sParagraphLayout Per-paragraph layout data
 * @see sViewport Visible viewport rectangle
 * @see sDisplayList Renderable display list
 * @see sDisplayBox Display list entry for a visible box
 * @see cLayoutBase Layout engine producing these structures
 */

#include "layoutstructs.h"
#include "layoutbase.h"
#include "src/core/document/document.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @param  layout [in] - Layout engine to get line data
///
/// @return nothing
///
/// @brief
/// Calculates screenYTop and screenYBottom from contained lines.
/// This must be called after layout is complete and before viewport
/// calculation. Uses the screeny values from the first and last lines
/// in the box to determine the box's Y range in continuous screen space.
///
/// Screen coordinates are continuous across the entire document and
/// include display mode awareness (page gaps in PAGE mode).
///
/// @note Handles edge cases: empty boxes, missing lines, single-line boxes
///
/////////////////////////////////////////////////////////////////////////////
void sBoxes::CalculateScreenYRange(const cLayoutBase* layout)
{
    if (containedLines.empty())
    {
        screenYTop = 0;
        screenYBottom = 0;
        return;
    }

    // Get first line to determine top
    LINE_T firstLine = containedLines.front();
    const sLineLayout* first = layout->GetLineByRawLineNumber(firstLine);
    if (!first)
    {
        screenYTop = 0;
        screenYBottom = 0;
        return;
    }
    screenYTop = first->screeny;

    // Get last line to determine bottom
    LINE_T lastLine = containedLines.back();
    const sLineLayout* last = layout->GetLineByRawLineNumber(lastLine);
    if (!last)
    {
        screenYBottom = screenYTop;
        return;
    }
    screenYBottom = last->screeny + last->lineheight;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Marks this box as needing redraw.
/// Called when box content or position changes, or after initial layout.
/// The rendering system will redraw this box on the next paint cycle.
///
/////////////////////////////////////////////////////////////////////////////
void sBoxes::MarkDirty(void)
{
    needsRedraw = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears the dirty flag after the box has been redrawn.
/// Called by the rendering system after the box is painted to screen.
///
/////////////////////////////////////////////////////////////////////////////
void sBoxes::ClearDirty(void)
{
    needsRedraw = false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  document [in] document to fetch graphemes from
/// @param  graphemes [out] vector to receive graphemes
///
/// @return number of graphemes fetched
///
/// @brief
/// Fetches this segment's graphemes from the document on-demand.
/// Uses efficient GetParagraphGraphemes() API with slice extraction.
///
/// PERFORMANCE: O(1) if paragraph is cached in cDocument, O(n) for first
/// access per paragraph. Subsequent segments in same paragraph benefit from
/// cDocument's paragraph-level caching (offsets field).
///
/// MEMORY: Returns by filling provided vector. Uses local stack allocation.
/// Small String Optimization (SSO) means most graphemes (1-2 bytes) won't
/// allocate heap memory.
///
/// Phase 3: displayglyph removal - replaces direct access to displayglyph field
///
/////////////////////////////////////////////////////////////////////////////
size_t sSegmentLayout::GetGraphemes(cDocument* document,
                                     std::vector<std::string>& graphemes) const
{
    // Guard: need document
    if (!document)
    {
        graphemes.clear();
        return 0;
    }

    // Get all graphemes for this paragraph (cached by cDocument)
    // This is O(1) after first access - cDocument caches grapheme boundaries
    std::vector<std::string> allGraphemes;
    std::vector<POSITION_T> offsets;  // Required by API but not used here
    document->GetParagraphGraphemes(paragraph, allGraphemes, offsets);

    // Extract just this segment's slice
    // Reserve exact size to avoid reallocations
    graphemes.clear();
    graphemes.reserve(length);

    // Copy our slice from the paragraph's graphemes
    POSITION_T end = startPosition + length;
    for (POSITION_T i = startPosition; i < end && i < static_cast<POSITION_T>(allGraphemes.size()); ++i)
    {
        graphemes.push_back(allGraphemes[i]);
    }

    // DEBUG: Log first and last grapheme if this segment is near position 85-90
    if (startPosition >= 80 && startPosition <= 95 && !graphemes.empty())
    {
        char firstChar = graphemes[0].empty() ? '?' : graphemes[0][0];
        char lastChar = graphemes.back().empty() ? '?' : graphemes.back()[0];
        POSITION_T endPos = startPosition + length - 1;
        UNUSED_ARGUMENT(firstChar);  // Debug variables for future use
        UNUSED_ARGUMENT(lastChar);
        UNUSED_ARGUMENT(endPos);
    }

    return graphemes.size();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  other [in] segment to compare with this segment
///
/// @return true if segments have identical layout, false otherwise
///
/// @brief
/// Compares two segments for layout equality. Checks all formatting,
/// positioning, and content data.
///
/////////////////////////////////////////////////////////////////////////////
bool sSegmentLayout::IsEqualTo(const sSegmentLayout& other) const
{
    // Quick checks first
    if (paragraph != other.paragraph)
    {
        return false;
    }

    // Phase 5: Use position-based fields instead of displayglyph
    if (startPosition != other.startPosition)
    {
        return false;
    }

    if (length != other.length)
    {
        return false;
    }

    if (position.size() != other.position.size())
    {
        return false;
    }

    if (!CoordsEqual(segmentheight, other.segmentheight))
    {
        return false;
    }

    // Check formatting
    if (font != other.font)
    {
        return false;
    }

    if (isSubscript != other.isSubscript || isSuperscript != other.isSuperscript)
    {
        return false;
    }

    if (isBlock != other.isBlock)
    {
        return false;
    }

    // Check colors
    if (textcolor.red != other.textcolor.red ||
        textcolor.green != other.textcolor.green ||
        textcolor.blue != other.textcolor.blue ||
        textcolor.alpha != other.textcolor.alpha)
    {
        return false;
    }

    if (backcolor.red != other.backcolor.red ||
        backcolor.green != other.backcolor.green ||
        backcolor.blue != other.backcolor.blue ||
        backcolor.alpha != other.backcolor.alpha)
    {
        return false;
    }

    // Check positions
    if (position != other.position)
    {
        return false;
    }

    // Check control code tracking (Phase 3.5 - visual display)
    if (hasControlCodes != other.hasControlCodes)
    {
        return false;
    }

    if (controlCodeIndices != other.controlCodeIndices)
    {
        return false;
    }

    // Check measurement fields (Step 1 - WORDWRAP.md)
    if (!CoordsEqual(totalWidth, other.totalWidth))
    {
        return false;
    }

    // Check tab fields
    if (isTab != other.isTab)
    {
        return false;
    }

    if (isTab)  // Only check these if tab is present
    {
        if (tabDocPosition != other.tabDocPosition ||
            !CoordsEqual(tabWidth, other.tabWidth) ||
            tabType != other.tabType)
        {
            return false;
        }
    }

    // Phase 5: No longer need to check displayglyph - using position-based access
    // Content equality is verified by paragraph, startPosition, and length

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraphNum [in] paragraph number for debug output
/// @param  lineNum [in] line number for debug output
/// @param  other [in] line to compare with this line
///
/// @return true if lines have identical layout, false otherwise
///
/// @brief
/// Compares two lines for layout equality. Checks alignment, position,
/// metadata (including Pattern 1 fields), boxIndex, and all segments.
/// Does not compare screeny (derived from previous lines).
///
/////////////////////////////////////////////////////////////////////////////
bool sLineLayout::IsEqualTo(PARAGRAPH_T paragraphNum, LINE_T lineNum, const sLineLayout& other) const
{
    UNUSED_ARGUMENT(paragraphNum);
    UNUSED_ARGUMENT(lineNum);

    // Check alignment
    if (left != other.left || center != other.center ||
        right != other.right || justify != other.justify)
    {
        return false;
    }

    // Check line-level tab flags (Phase 4 Pattern 1)
    if (centerLine != other.centerLine || rightLine != other.rightLine)
    {
        return false;
    }

    // Check position (but not screeny - it's derived from previous lines)
    if (!CoordsEqual(pagex, other.pagex) || !CoordsEqual(pagey, other.pagey))
    {
        return false;
    }

    // Check metadata (Phase 4 Pattern 1: added contentLineNumber, rawLineNumber, cumalativeheight)
    if (pagenumber != other.pagenumber ||
        pageLineNumber != other.pageLineNumber ||
        contentLineNumber != other.contentLineNumber ||
        rawLineNumber != other.rawLineNumber ||
        !CoordsEqual(lineheight, other.lineheight) ||
        !CoordsEqual(cumalativeheight, other.cumalativeheight))
    {
        return false;
    }

    // Check linestart
    if (linestart != other.linestart)
    {
        return false;
    }

    // NOTE: documentPosition is NOT compared here because it shifts when text is
    // inserted/deleted earlier in the document. This doesn't affect visual layout -
    // the paragraph's actual content is unchanged. Position updates are handled
    // separately without requiring full relayout.

    // Check boxIndex (Phase 4 Pattern 1 - CRITICAL for layout change detection)
    if (boxIndex != other.boxIndex)
    {
        return false;
    }

    // Check segments
    if (segments.size() != other.segments.size())
    {
        return false;
    }

    for (size_t i = 0; i < segments.size(); i++)
    {
        if (!segments[i].IsEqualTo(other.segments[i]))
        {
            return false;
        }
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  other [in] paragraph layout to compare with this one
///
/// @return true if paragraphs have identical layout, false otherwise
///
/// @brief
/// Compares two paragraph layouts for equality. Used for incremental
/// layout to determine if a paragraph needs to be re-laid out or if
/// the existing layout can be reused.
///
/// @note Checks number, lines, and end state. All must match
///       for layouts to be considered equal.
///
/////////////////////////////////////////////////////////////////////////////
bool sParagraphLayout::IsEqualTo(const sParagraphLayout& other) const
{
    // Check paragraph number
    if (number != other.number)
    {
        return false;
    }

    // Check number of lines
    if (lines.size() != other.lines.size())
    {
        return false;
    }

    // Compare each line
    for (size_t i = 0; i < lines.size(); i++)
    {
        if (!lines[i].IsEqualTo(number, i, other.lines[i]))
        {
            return false;
        }
    }

    // Check end state - compare all modifier fields
    // Note: sModifierParagraph is defined in paragraphbase.h and contains
    // font, color, and attribute information
    if (endState.font != other.endState.font)
    {
        return false;
    }

    if (endState.textcolor.red != other.endState.textcolor.red ||
        endState.textcolor.green != other.endState.textcolor.green ||
        endState.textcolor.blue != other.endState.textcolor.blue ||
        endState.textcolor.alpha != other.endState.textcolor.alpha)
    {
        return false;
    }

    // Check boolean flags in endState
    if (endState.bold != other.endState.bold ||
        endState.italics != other.endState.italics ||
        endState.underline != other.endState.underline ||
        endState.strikethrough != other.endState.strikethrough ||
        endState.subscript != other.endState.subscript ||
        endState.superscript != other.endState.superscript)
    {
        return false;
    }

    // Check alignment flags in endState
    if (endState.right != other.endState.right ||
        endState.left != other.endState.left ||
        endState.justify != other.endState.justify ||
        endState.center != other.endState.center)
    {
        return false;
    }

    // Check line spacing
    if (endState.linespace != other.endState.linespace)
    {
        return false;
    }

    // Check margin state in endState
    if (!CoordsEqual(endState.leftMargin, other.endState.leftMargin) ||
        !CoordsEqual(endState.rightMargin, other.endState.rightMargin) ||
        !CoordsEqual(endState.paragraphMargin, other.endState.paragraphMargin) ||
        endState.validParagraphMargin != other.endState.validParagraphMargin)
    {
        return false;
    }

    // Check page break flag
    if (pageBreak != other.pageBreak)
    {
        return false;
    }

    // Phase 4 (Pattern 1 fix): Check dot command and comment tracking fields
    if (isCommand != other.isCommand || isComment != other.isComment)
    {
        return false;
    }

    if (dotStatus != other.dotStatus)
    {
        return false;
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return size_t - number of graphemes in this segment
///
/// @brief
/// Returns the grapheme count for this segment.
/// This is a simple accessor that returns the length field without
/// accessing the deprecated displayglyph field.
///
/// @note Use GetGraphemes() if you need the actual grapheme strings
///
/////////////////////////////////////////////////////////////////////////////
size_t sSegmentLayout::GetGraphemeCount(void) const
{
    return length;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  box [in] - Box to check for intersection
///
/// @return bool - true if box intersects viewport Y range
///
/// @brief
/// Checks if a box intersects the viewport using Y coordinates.
/// Box intersects if: box.screenYBottom >= topY AND box.screenYTop <= bottomY
/// This handles all cases: fully inside, partially overlapping, or
/// completely containing the viewport.
///
/// This is the core algorithm for box-based viewport management.
///
/////////////////////////////////////////////////////////////////////////////
bool sViewport::Intersects(const sBoxes& box) const
{
    // Box Y range must overlap viewport Y range
    // Use >= and <= to include boundary cases (touching edges are visible)
    return (box.screenYBottom >= topY && box.screenYTop <= bottomY);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears the viewport by removing all visible boxes.
/// Called before recalculating visible boxes during scrolling or resize.
///
/////////////////////////////////////////////////////////////////////////////
void sViewport::Clear(void)
{
    visibleBoxes.clear();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears the display list by removing all boxes.
/// Called before rebuilding or when display list is no longer needed.
///
/////////////////////////////////////////////////////////////////////////////
void sDisplayList::Clear(void)
{
    boxes.clear();
}
