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

#ifndef LAYOUTSTRUCTS_H
#define LAYOUTSTRUCTS_H

#include <vector>
#include <deque>
#include <string>

#include "src/core/include/config.h"
#include "src/core/include/papersize.h"
#include "src/core/document/doctstructs.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eDotCommandStatus
///
/// @brief
/// Status of dot command parsing in paragraph layout.
/// Used for visual display (color coding of dot commands).
///
/// Moved from paragraphbase.h to remove dependency on old layout system.
///
/////////////////////////////////////////////////////////////////////////////
enum eDotCommandStatus
{
    DOT_ERROR = 0,          // Dot command has an error (display in red)
    DOT_GOOD,               // Valid dot command (display in green)
    DOT_UNKNOWN,            // Unrecognized dot command (display in yellow)
    DOT_NOTIMPLEMENTED,     // Known but not yet implemented (display in orange)
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sModifierParagraph
///
/// @brief
/// Font, color, and text attributes at the end of a paragraph.
/// Carries state forward to the next paragraph.
///
/// Moved from paragraphbase.h to remove dependency on old layout system.
///
/////////////////////////////////////////////////////////////////////////////
struct sModifierParagraph
{
    std::string font;                              // Font at the end of the paragraph
    sSeqRGBColor textcolor;                        // Color of the text
    bool bold, italics, underline;                 // Attributes at the end of the paragraph
    bool superscript, subscript, strikethrough;    // Attributes at the end of the paragraph
    bool right, left, justify, center;             // Attributes at the end of the paragraph
    bool wordWrap;                                 // Word wrap (.aw) state at the end of the paragraph
    double linespace;

    // Margin state at end of paragraph (for identical full/partial layout)
    COORD_T leftMargin;                            // Left margin in twips
    COORD_T rightMargin;                           // Right margin in twips
    COORD_T paragraphMargin;                       // First-line indent in twips
    bool validParagraphMargin;                     // True if paragraph margin is set

    // Constructor: initialize all fields to defaults
    sModifierParagraph() : font(""), bold(false), italics(false), underline(false),
                           superscript(false), subscript(false), strikethrough(false),
                           right(false), left(true), justify(false), center(false),
                           wordWrap(true),
                           linespace(1.0),
                           leftMargin(0), rightMargin(9360), paragraphMargin(0),
                           validParagraphMargin(false)
    {
        textcolor.red = -1;
        textcolor.green = -1;
        textcolor.blue = -1;
        textcolor.alpha = -1;
    }
};


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sPageInfo
///
/// @brief
/// Page geometry: paper size, margins, and header/footer margins.
/// Stored per-page to support mid-document paper/margin changes.
///
/////////////////////////////////////////////////////////////////////////////
struct sPageInfo
{
    ePaperSize papertype;           // derived from Qt sizes
    COORD_T paperwidth;             // this can be looked up via wxPaperSize, but it's slow
    COORD_T paperheight;            // this can be looked up via wxPaperSize, but it's slow
    bool set;                       // the user can do multiple .pt commands per page, but only the first is actually used.

    COORD_T topmargin;              // page's top margin
    COORD_T bottommargin;           // page's bottom margin (real margin size, not height of print space i.e. 1440 for 1 inch margin)
    COORD_T leftmargin;             // page's left margin
    COORD_T rightmargin;            // pages's right margin (real margin size, not width of print space i.e. 1440 for 1 inch margin)
    COORD_T headermargin;           // page's header margin
    COORD_T footermargin;           // page's footer margin
};

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eBoxType
///
/// @brief
/// Type of box in the new layout engine.
/// Named eBoxType to avoid conflict with old system's eBoxType.
///
/////////////////////////////////////////////////////////////////////////////
enum eBoxType
{
    BOX_TEXT,      // Text flow box
    BOX_TABLE,     // Table box (Phase 5)
    BOX_IMAGE,     // Image box (Phase 6)
    BOX_SIDEBAR    // Sidebar box (Phase 6)
};

// Forward declarations
class cLayoutBase;
class cDocument;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sBoxes
///
/// @brief
/// Rectangular region containing content in the new layout engine.
/// Boxes are the fundamental building blocks of the layout system.
/// All coordinates are absolute page coordinates in twips.
///
/////////////////////////////////////////////////////////////////////////////
struct sBoxes
{
    // Type
    eBoxType type;                         // BOX_TEXT, BOX_TABLE, BOX_IMAGE, BOX_SIDEBAR

    // Absolute page coordinates (in twips)
    COORD_T left;                           // Left edge
    COORD_T top;                            // Top edge
    COORD_T right;                          // Right edge
    COORD_T bottom;                         // Bottom edge

    // Box identity
    int boxNumber;                          // Unique box ID across all pages
    PAGE_T pageNumber;                      // Which page this box is on

    // Content tracking
    std::vector<LINE_T> containedLines;     // Global line numbers in this box
    COORD_T currentY;                       // Current fill height (relative to top)

    // Multi-column support (Phase 1+)
    int columnCount;                        // Number of columns (1 in Phase 0)
    COORD_T columnGap;                      // Gap between columns in twips

    // Viewport management (Phase 3.2)
    COORD_T screenYTop;                     // Continuous Y of first line in box
    COORD_T screenYBottom;                  // Continuous Y of last line in box
    bool needsRedraw;                       // Dirty flag for rendering optimization

    // Continuous Y coordinate where this page starts (y=0 in page coordinates)
    // Calculated as: mScreenY - box.top when box is created
    // Used to convert page-relative box.top/bottom to continuous coordinates
    COORD_T pageStartY;                     // Continuous Y of page origin (can be negative for page 1!)

    // Page info (Phase 1B - for GetPageInfo())
    sPageInfo pageInfo;                     // Margins and settings when box was created

    // Constructor
    sBoxes() : type(BOX_TEXT), left(0), top(0), right(0), bottom(0),
                boxNumber(0), pageNumber(0), currentY(0),
                columnCount(1), columnGap(0),
                screenYTop(0), screenYBottom(0), needsRedraw(false), pageStartY(0) {}

    // Viewport management methods
    void CalculateScreenYRange(const cLayoutBase* layout);
    void MarkDirty(void);
    void ClearDirty(void);
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sTabStop
///
/// @brief
/// Tab stop position and type for .RR (ruler) and .TB (tab stops) commands.
/// Stores absolute position in twips and tab type (normal, decimal, center, right).
///
/////////////////////////////////////////////////////////////////////////////
struct sTabStop
{
    COORD_T position;    // Absolute position in twips
    eTabTypes type;      // Tab type (TAB_TAB, TAB_DECIMAL, TAB_CENTER, TAB_RIGHT)

    // Constructor
    sTabStop() : position(0), type(TAB_TAB) {}
    sTabStop(COORD_T pos, eTabTypes t) : position(pos), type(t) {}

    // Equality comparison (needed by checkpoint operator==)
    bool operator==(const sTabStop& other) const
    {
        return CoordsEqual(position, other.position) && type == other.type;
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sSegmentLayout
///
/// @brief
/// Run of text with consistent formatting within a line.
/// Segments are the smallest layout unit and contain actual display glyphs.
///
/////////////////////////////////////////////////////////////////////////////
struct sSegmentLayout
{
    PARAGRAPH_T paragraph;                  // Paragraph number
    // Phase 5: displayglyph REMOVED - now using position-based access via GetGraphemes()
    // std::deque<std::string> displayglyph;   // DEPRECATED: Use GetGraphemes() instead
    std::vector<float> position;            // X positions (line-relative, twips). float saves 50% vs double; precision ~0.001 twips at max page width, more than sufficient.

    // Position-based access (replaces displayglyph)
    POSITION_T startPosition;               // Start position in paragraph (grapheme index)
    POSITION_T length;                      // Number of graphemes in segment

    // Formatting
    std::string font;                       // Font descriptor (Qt format)
    sSeqRGBColor textcolor;                 // Text color (reuse existing)
    sSeqRGBColor backcolor;                 // Background color

    // Attributes
    bool isBlock;                           // In marked block
    bool isSubscript;                       // Subscripted
    bool isSuperscript;                     // Superscripted
    COORD_T segmentheight;                  // Segment height in twips

    // Control code tracking (for visual display - Phase 3.5)
    bool hasControlCodes;                   // True if this segment contains control codes
    std::vector<size_t> controlCodeIndices; // Which glyphs are control codes (indices into position array)

    // Measurement fields (Step 1 - WORDWRAP.md)
    COORD_T totalWidth;                     // Total width of segment (for quick access)

    // Tab segment -- this segment IS a tab (self-contained, replaces old 3-segment pattern)
    bool isTab;                             // True if this segment represents a tab character
    POSITION_T tabDocPosition;              // Document position of tab (for GetTab call)
    COORD_T tabWidth;                       // Tab width (calculated during word wrap, 0 during segmentation)
    eTabTypes tabType;                      // Type of tab character in document (TAB_TAB, TAB_CENTER, TAB_RIGHT) - line-level alignment
    eTabTypes tabStopType;                  // Type of tab stop this tab expanded to (TAB_TAB, TAB_CENTER, TAB_RIGHT, TAB_DECIMAL) - column positioning

    // Constructor
    sSegmentLayout() : paragraph(0), startPosition(0), length(0),
                        isBlock(false),
                        isSubscript(false), isSuperscript(false), segmentheight(0),
                        hasControlCodes(false),
                        totalWidth(0),
                        isTab(false), tabDocPosition(0), tabWidth(0), tabType(TAB_TAB), tabStopType(TAB_TAB)
    {
        textcolor.red = -1;
        textcolor.green = -1;
        textcolor.blue = -1;
        textcolor.alpha = -1;
        backcolor.red = 0;
        backcolor.green = 0;
        backcolor.blue = 0;
        backcolor.alpha = 0;
    }

    // Helper method to get grapheme count without accessing displayglyph
    size_t GetGraphemeCount(void) const;

    // Helper method to fetch graphemes on-demand from document (Phase 3: displayglyph removal)
    size_t GetGraphemes(cDocument* document, std::vector<std::string>& graphemes) const;

    // Equality comparison for layout change detection (Phase 4: incremental layout)
    bool IsEqualTo(const sSegmentLayout& other) const;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sLineLayout
///
/// @brief
/// Single line of text in the layout system.
/// Contains segments, position information, and metadata.
///
/////////////////////////////////////////////////////////////////////////////
struct sLineLayout
{
    // Alignment
    bool left, center, right, justify;

    // Line-level tab flags (Step 1 - WORDWRAP.md)
    // These override paragraph alignment for the specific line containing the control
    bool centerLine;                        // Center this line only (from TAB_CENTER control in this line)
    bool rightLine;                         // Right-align this line only (from TAB_RIGHT control in this line)

    // Position (absolute page coordinates in twips)
    COORD_T pagex;                          // X position on page
    COORD_T pagey;                          // Y position on page
    COORD_T screeny;                        // Continuous Y for scrolling (ONLY exception to absolute rule)

    // Line metadata
    PAGE_T pagenumber;                      // Page number
    LINE_T pageLineNumber;                  // Content lines on this page (excludes dot commands; resets per page)
    LINE_T contentLineNumber;               // Content lines in document (excludes dot commands)
    LINE_T rawLineNumber;                   // Every laid-out row (includes dot commands)
    COORD_T lineheight;                     // Line height in twips
    COORD_T cumalativeheight;               // Cumulative height from document start (excludes dot commands)
    bool isPrintable;                       // false for dot command and comment lines (no print position)

    // Box tracking (CRITICAL for layout change detection)
    int boxIndex;                           // Index into global box list

    // Content
    std::deque<sSegmentLayout> segments;   // Text segments
    POSITION_T linestart;                   // Start position in paragraph (grapheme index, paragraph-relative)
    POSITION_T documentPosition;            // Absolute document position where line starts (used for caret navigation)

    // Header/footer template use only; not compared in IsEqualTo().
    bool appliesSamePage;

    // Constructor
    sLineLayout() : left(true), center(false), right(false), justify(false),
                     centerLine(false), rightLine(false),
                     pagex(0), pagey(0), screeny(0),
                     pagenumber(0), pageLineNumber(0), contentLineNumber(0), rawLineNumber(0),
                     lineheight(0), cumalativeheight(0), isPrintable(true),
                     boxIndex(-1), linestart(0), documentPosition(0), appliesSamePage(false) {}

    // Equality comparison for layout change detection (Phase 4: incremental layout)
    bool IsEqualTo(PARAGRAPH_T paragraphNum, LINE_T lineNum, const sLineLayout& other) const;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sParagraphLayout
///
/// @brief
/// Collection of lines from one paragraph.
/// Tracks paragraph-level state and formatting.
///
/////////////////////////////////////////////////////////////////////////////
struct sParagraphLayout
{
    PARAGRAPH_T number;                     // Paragraph number
    std::vector<sLineLayout> lines;        // Lines in this paragraph

    // State at end of paragraph (for next paragraph)
    sModifierParagraph endState;            // Font, color, attributes (reuse existing)

    // Page break control
    bool pageBreak;                         // .PA command - force new page before next paragraph

    // Dot command and comment tracking (for visual display)
    bool isCommand;                         // True if this paragraph is a dot command (.MT, .MB, etc.)
    bool isComment;                         // True if this paragraph is a comment (..)
    eDotCommandStatus dotStatus;            // DOT_GOOD, DOT_ERROR, DOT_UNKNOWN, DOT_NOTIMPLEMENTED

    // Dirty flag for page margin propagation
    // Set when a page-affecting dot command (.MT, .MB, .PL) is detected.
    // All paragraphs from the affected page onward are marked dirty.
    // Dirty paragraphs force continued idle layout (prevent early stopping).
    bool dirty;                             // True if paragraph needs re-layout due to page box change

    // Box right edge at layout time (for rendering dot command/comment backgrounds)
    // Stored when paragraph is laid out so rendering uses the right margin that was
    // in effect at that point, not the global current value which may have changed.
    COORD_T boxRight;

    // Position state at END of this paragraph (for next paragraph to use)
    // Stored after laying out this paragraph during full layout.
    // Paragraph N reads paragraph N-1's position end state to get its start state.
    // This ensures partial layout produces identical results to full layout
    // without reading any live mutable state (like box->currentY).
    // Always valid after formatting, even for empty paragraphs or dot commands.
    LINE_T endContentLineNum;               // Content line number after this paragraph (excludes dot commands)
    LINE_T endRawLineNum;                   // Raw line number after this paragraph (every row, includes dot commands)
    LINE_T endPageLineNum;                  // Page line number after this paragraph (excludes dot commands)
    PAGE_T endPage;                         // Page number after this paragraph
    int endBox;                             // Box index after this paragraph
    COORD_T endCumulativeHeight;            // Cumulative height after this paragraph
    COORD_T endScreenY;                     // Continuous screen Y after this paragraph
    COORD_T endBoxY;                        // Box fill position (relative to box.top) after this paragraph

    // Constructor
    sParagraphLayout() : number(0), pageBreak(false),
                          isCommand(false), isComment(false), dotStatus(DOT_GOOD), dirty(false),
                          boxRight(0),
                          endContentLineNum(0), endRawLineNum(0), endPageLineNum(0),
                          endPage(1), endBox(NOT_SET), endCumulativeHeight(0),
                          endScreenY(0), endBoxY(0) {}

    // Equality comparison for layout change detection (Phase 4: incremental layout)
    bool IsEqualTo(const sParagraphLayout& other) const;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sPage
///
/// @brief
/// Contains all boxes for a single page.
/// Top-level structure in the layout hierarchy.
///
/////////////////////////////////////////////////////////////////////////////
struct sPage
{
    PAGE_T pageNumber;                      // 1-based page number
    std::vector<sBoxes> boxes;             // All boxes on this page
    sPageInfo pageInfo;                     // Paper size, margins (reuse existing)
    std::vector<sLineLayout> headers;      // Page headers (Phase 2)
    std::vector<sLineLayout> footers;      // Page footers (Phase 2)
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sViewport
///
/// @brief
/// Viewport represents the visible area of the document on screen.
/// Uses Y-coordinate intersection to support multi-column and complex layouts.
///
/// CRITICAL: This MUST be Y-based, NOT line-based, to support columns!
/// In multi-column layouts, multiple boxes exist at the same Y coordinate
/// (e.g., left and right columns). Line-based viewport cannot represent this.
/// Y-coordinate intersection handles all layout types naturally.
///
/// NOTE: Requires sBoxes.screenYTop and sBoxes.screenYBottom fields
/// for intersection testing.
///
/// Works for both DISPLAY_CONTINUOUS and DISPLAY_PAGE modes because the
/// layout engine calculates screeny values that include display mode
/// awareness (page gaps are included in screeny in page mode).
///
/////////////////////////////////////////////////////////////////////////////
struct sViewport
{
    // Viewport bounds in continuous screen coordinates (twips)
    COORD_T topY;                           // Top of viewport in document screeny coordinates
    COORD_T bottomY;                        // Bottom of viewport in document screeny coordinates
    COORD_T scrollOffset;                   // Current scroll position (twips from document start)
    COORD_T viewportHeight;                 // Height of visible area in twips

    // Visible boxes (pointers to boxes in layout, not owned by viewport)
    std::vector<sBoxes*> visibleBoxes;     // Boxes intersecting viewport Y range

    // Constructor
    sViewport() : topY(0), bottomY(0), scrollOffset(0), viewportHeight(0) {}

    // Intersection testing (implementation in .cpp)
    bool Intersects(const sBoxes& box) const;

    // Clear visible boxes (implementation in .cpp)
    void Clear(void);
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDisplayBox
///
/// @brief
/// Display list entry for one box with its visible lines.
/// Part of the display list rendering pipeline.
///
/// This structure represents a single box that needs to be rendered,
/// along with the specific lines within that box that are visible in
/// the current viewport. This is the second level of filtering after
/// viewport box intersection.
///
/// The structure contains pointers to layout data (not owned) and is
/// rebuilt every frame during paintEvent. This keeps the design simple
/// and avoids complex dirty tracking.
///
/// Used by sDisplayList to build the complete render list.
///
/// Named sDisplayBox to avoid conflict with old system's sDisplayBox.
///
/////////////////////////////////////////////////////////////////////////////
struct sDisplayBox
{
    sBoxes* box;                           // Pointer to box in layout (not owned)
    std::vector<sLineLayout*> visibleLines; // Lines in this box that are visible
    COORD_T screenYOffset;                  // Y offset for scrolling (cached from viewport)
    bool needsRedraw;                       // Dirty flag (cached from box)

    // Constructor
    sDisplayBox() : box(nullptr), screenYOffset(0), needsRedraw(false) {}

    // Calculate which lines in this box are visible in the viewport (impl in layoutbase.cpp)
    void CalculateVisibleLines(const sViewport& viewport, const cLayoutBase* layout);
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDisplayList
///
/// @brief
/// Complete display list structure containing all boxes to render.
/// This is the final rendering pipeline stage before actual drawing.
///
/// The display list is built from the viewport's visible boxes and
/// contains only the specific lines that need to be rendered. This
/// two-level filtering (boxes, then lines) ensures efficient rendering
/// of large documents.
///
/// Usage pattern:
/// 1. CalculateViewport() - determines visible boxes
/// 2. displayList.Build(viewport, layout) - builds render list
/// 3. Iterate displayList.boxes to render
///
/// The display list is transient and rebuilt every frame. This keeps
/// the design simple and avoids complex invalidation logic.
///
/// Named sDisplayList to avoid conflict with old system's sDisplayList.
///
/////////////////////////////////////////////////////////////////////////////
struct sDisplayList
{
    std::vector<sDisplayBox> boxes;        // Boxes to render (with filtered lines)
    COORD_T scrollOffset;                   // Current scroll position (cached)

    // Constructor
    sDisplayList() : scrollOffset(0) {}

    // Build display list from visible boxes in viewport (impl in layoutbase.cpp)
    void Build(const sViewport& viewport, const cLayoutBase* layout);

    // Clear display list (implementation in .cpp)
    void Clear(void);
};

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eDisplayMode
///
/// @brief
/// Display mode for editor viewport.
/// Phase 0.5: Adds page view mode to existing continuous scrolling.
///
/////////////////////////////////////////////////////////////////////////////
enum eDisplayMode
{
    DISPLAY_CONTINUOUS,    // Continuous vertical scrolling (existing behavior)
    DISPLAY_PAGE          // Page-by-page with visual gaps (new)
};

/////////////////////////////////////////////////////////////////////////////
///
/// @enum ePageNumberFormat
///
/// @brief
/// Page number display format for headers and footers.
/// Phase 2: Controls how # is replaced in header/footer text.
///
/////////////////////////////////////////////////////////////////////////////
enum ePageNumberFormat
{
    PAGE_NUM_ARABIC,        // 1, 2, 3, 4, ...
    PAGE_NUM_ROMAN_LOWER,   // i, ii, iii, iv, ...
    PAGE_NUM_ROMAN_UPPER    // I, II, III, IV, ...
};

// Gap at page breaks in continuous mode (twips). 0.25 inches.
// Used by layout engine (mScreenY gap) and renderer (separator line centering).
static const COORD_T CONTINUOUS_PAGE_GAP = 360;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDisplaySettings
///
/// @brief
/// Display configuration for rendering.
/// Contains all settings needed to render layout data to screen.
///
/////////////////////////////////////////////////////////////////////////////
struct sDisplaySettings
{
    eDisplayMode mode;           // Current display mode
    eShowControl showControl;    // Control code visibility (SHOW_ALL, SHOW_DOT, SHOW_NONE)

    // Page mode settings
    COORD_T pageGap;            // Gap between pages in page mode (twips)
    COORD_T pageBorder;         // Border around pages in page mode (twips)
    bool showPageShadows;       // Draw shadows around pages (visual effect)
    sSeqRGBColor backgroundColor; // Background color for gaps between pages

    // Continuous mode settings
    bool showPageNumbers;       // Overlay page numbers in continuous mode

    // Constructor with sensible defaults
    sDisplaySettings() :
        mode(DISPLAY_CONTINUOUS),
        showControl(SHOW_ALL),
        pageGap(360),          // 0.25 inches between pages
        pageBorder(360),       // 0.25 inches border around pages
        showPageShadows(true),
        showPageNumbers(false)
    {
        backgroundColor.red = 128;
        backgroundColor.green = 128;
        backgroundColor.blue = 128;
        backgroundColor.alpha = 255;
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sModifiers
///
/// @brief
/// Text modifier state for layout engine.
/// Tracks justification, alignment, and line spacing settings.
///
/////////////////////////////////////////////////////////////////////////////
struct sModifiers
{
    bool justify;      // Full justification (.oj on)
    bool left;         // Left align - default (.oj off)
    bool right;        // Right align (.oj r)
    bool center;       // Center text (.oj c)
    double linespace;  // Line spacing multiplier (.ls) - for Task 5

    // Constructor: default to left align, single spacing
    sModifiers() : justify(false), left(true), right(false), center(false), linespace(1.0) {}

    // Equality comparison for checkpoint matching
    bool operator==(const sModifiers& other) const
    {
        return justify == other.justify && left == other.left &&
               right == other.right && center == other.center &&
               linespace == other.linespace;
    }
};

#endif // LAYOUTSTRUCTS_H
