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
 * @class cLayout
 * @brief TUI layout engine extending cLayoutBase with terminal-specific segment building.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements the TUI cLayout class, which extends cLayoutBase with TUI-specific
 * implementations of the three pure virtual methods: BuildParagraphSegments,
 * BuildHeaderFooterSegments, and GetFontWithFormatting. Owns a
 * cTUITextMeasurement instance for glyph measurement and delegates font system
 * initialization, font selection, and font-family enumeration through it.
 *
 * @section tuilayout_segments Segment Building
 * BuildParagraphSegments measures each grapheme in a paragraph using the TUI
 * font manager and populates sSegmentLayout entries with twips-based positions.
 * BuildHeaderFooterSegments handles header/footer text layout with the same
 * measurement pipeline.
 *
 * @section tuilayout_fonts Font Resolution
 * GetFontWithFormatting applies bold, italic, underline, and size attributes
 * to a base font descriptor and returns the resolved sTUIFontInfo. Font family
 * enumeration for the GUI font dialog is delegated through the text
 * measurement layer.
 *
 * @see cLayout
 * @see cLayoutBase
 * @see cTUITextMeasurement
 * @see cTUIFontManager
 * @see sSegmentLayout
 */

#include "layout.h"
#include "tuitextmeasurement.h"
#include "tuifontutils.h"
#include "src/core/document/document.h"
#include "src/core/include/timer.h"



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor - initializes the TUI layout with default Courier New 12pt
/// font and creates the TUI text measurement implementation.
///
/////////////////////////////////////////////////////////////////////////////
cLayout::cLayout(void)
{
    // Create TUI text measurement
    mTUITextMeasurement = new cTUITextMeasurement();

    // Set text measurement interface on base class
    SetTextMeasurement(mTUITextMeasurement);

    // Initialize default font info (Courier New 12pt)
    mCurrentFontInfo.name = "Courier New";
    mCurrentFontInfo.size = 12;
    mCurrentFontInfo.bold = false;
    mCurrentFontInfo.italic = false;
    mCurrentFontInfo.underline = false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor - cleans up TUI text measurement
///
/////////////////////////////////////////////////////////////////////////////
cLayout::~cLayout(void)
{
    delete mTUITextMeasurement;
    mTUITextMeasurement = nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if font system initialized successfully
///
/// @brief
/// Initializes the font system. Must be called before LayoutDocument().
/// Delegates to the text measurement class which manages the font manager.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayout::InitializeFontSystem(void)
{
    return mTUITextMeasurement->Initialize();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shuts down the font system and releases resources.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::ShutdownFontSystem(void)
{
    mTUITextMeasurement->Shutdown();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  family [in] Font family name
/// @param  pointSize [in] Font size in points
/// @param  bold [in] Bold flag
/// @param  italic [in] Italic flag
///
/// @return nothing
///
/// @brief
/// Sets the current font for layout. Updates both the internal font state
/// and the text measurement system.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::SetFont(const std::string& family, double pointSize,
                       bool bold, bool italic)
{
    mCurrentFontInfo.name = family;
    mCurrentFontInfo.size = static_cast<int>(pointSize);
    mCurrentFontInfo.bold = bold;
    mCurrentFontInfo.italic = italic;

    // Update text measurement
    if (mTUITextMeasurement)
    {
        mTUITextMeasurement->SetFont(family, pointSize, bold, italic);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> - sorted list of available font family names
///
/// @brief
/// Returns font family names from the font manager. Delegates to
/// cTUITextMeasurement::GetFontManager()->GetFontFamilies().
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cLayout::GetFontFamilies(void)
{
    if (mTUITextMeasurement)
    {
        cTUIFontManager* fm = mTUITextMeasurement->GetFontManager();
        if (fm)
        {
            return fm->GetFontFamilies();
        }
    }
    return {};
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return Pointer to the TUI font manager, or nullptr if not initialized
///
/// @brief
/// Returns the font manager for direct font operations (e.g., resolving
/// font family names to file paths for PDF font embedding).
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontManager* cLayout::GetFontManager(void)
{
    if (mTUITextMeasurement)
    {
        return mTUITextMeasurement->GetFontManager();
    }
    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if background font enumeration has completed
///
/// @brief
/// Delegates to cTUITextMeasurement::IsFontEnumerationDone().
///
/////////////////////////////////////////////////////////////////////////////
bool cLayout::IsFontEnumerationDone(void) const
{
    if (mTUITextMeasurement)
    {
        return mTUITextMeasurement->IsFontEnumerationDone();
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Blocks until background font enumeration finishes.
/// Delegates to cTUITextMeasurement::WaitForFontEnumeration().
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::WaitForFontEnumeration(void)
{
    if (mTUITextMeasurement)
    {
        mTUITextMeasurement->WaitForFontEnumeration();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] Font descriptor string (pipe-delimited format)
///
/// @return nothing
///
/// @brief
/// Override of base class SetDefaultFont to update TUI font objects.
/// Converts font string to sTUIFontInfo and updates the text measurement.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::SetDefaultFont(const std::string& font)
{
    // Call base class to set mLayoutState->SetDefaultFont()
    cLayoutBase::SetDefaultFont(font);

    // Update TUI font state from descriptor
    sTUIFontInfo info = TUIFontUtils::FontInfoFromDescriptor(font);
    SetFont(info.name, info.size, info.bold, info.italic);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Updates mCurrentFontInfo to reflect current formatting state.
/// Reads bold, italic, underline, subscript, superscript flags from
/// mLayoutState and configures the font info and text measurement accordingly.
///
/// This is the TUI equivalent of the GUI's UpdateCurrentFont() which
/// updates QFont and QFontMetrics.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::UpdateCurrentFont(void)
{
    // Validate current font - if empty, use default
    if (mLayoutState->GetCurrentFont().empty())
    {
        mLayoutState->SetCurrentFont(mLayoutState->GetDefaultFont());
    }

    // Build fresh clean font from current font descriptor (no styles)
    sTUIFontInfo cleanFont = TUIFontUtils::FontInfoFromDescriptor(mLayoutState->GetCurrentFont());

    // Apply current formatting state
    mCurrentFontInfo = cleanFont;
    mCurrentFontInfo.bold = mLayoutState->IsBoldActive();
    mCurrentFontInfo.italic = mLayoutState->IsItalicActive();
    mCurrentFontInfo.underline = mLayoutState->IsUnderlineActive();

    // Handle subscript/superscript font size
    if (mLayoutState->IsSubscriptActive() || mLayoutState->IsSuperscriptActive())
    {
        mCurrentFontInfo.size = static_cast<int>(cleanFont.size * 0.58);
    }

    // Update the font manager with new font settings
    mTUITextMeasurement->SetFont(
        mCurrentFontInfo.name,
        mCurrentFontInfo.size,
        mCurrentFontInfo.bold,
        mCurrentFontInfo.italic
    );
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  bold [in] Apply bold formatting
/// @param  italic [in] Apply italic formatting
/// @param  underline [in] Apply underline formatting
///
/// @return Font descriptor string with formatting applied
///
/// @brief
/// Creates a font descriptor string with the specified formatting applied.
/// Uses the current base font from layout state and applies formatting flags.
///
/////////////////////////////////////////////////////////////////////////////
std::string cLayout::GetFontWithFormatting(bool bold, bool italic, bool underline)
{
    // Start with the current base font from layout state
    sTUIFontInfo font = TUIFontUtils::FontInfoFromDescriptor(mLayoutState->GetCurrentFont());

    // Apply formatting flags
    font.bold = bold;
    font.italic = italic;
    font.underline = underline;

    // Return as descriptor string
    return TUIFontUtils::FontToDescriptor(font);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  seg [out] Segment to initialize
/// @param  paragraph [in] Paragraph number for the segment
/// @param  startPosition [in] Start position within paragraph
/// @param  lineHeight [in] Line height for the segment
///
/// @return nothing
///
/// @brief
/// Initializes a segment with default values and current formatting state.
/// Uses TUIFontUtils::FontToDescriptor instead of FontUtils::FontToDescriptor.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::InitializeSegment(sSegmentLayout& seg, PARAGRAPH_T paragraph,
                                  POSITION_T startPosition, COORD_T lineHeight)
{
    seg = sSegmentLayout();
    seg.paragraph = paragraph;
    seg.startPosition = startPosition;
    seg.length = 0;
    seg.font = TUIFontUtils::FontToDescriptor(mCurrentFontInfo);
    seg.segmentheight = lineHeight;
    seg.isSubscript = mLayoutState->IsSubscriptActive();
    seg.isSuperscript = mLayoutState->IsSuperscriptActive();
    seg.textcolor = mLayoutState->GetCurrentColor();
    seg.backcolor.alpha = 0;
    seg.isBlock = false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  controlType [in] Type of formatting control code
/// @param  documentPos [in] Document position for font lookup
///
/// @return nothing
///
/// @brief
/// Applies a formatting control code by toggling the appropriate state.
/// Handles bold, italic, underline, strikethrough, subscript, superscript,
/// font changes, and color changes.
/// Does NOT call UpdateCurrentFont() - caller must do that.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::ApplyFormattingControlCode(eModifiers controlType, POSITION_T documentPos)
{
    if (controlType == STYLE_BOLD)
    {
        mLayoutState->SetBoldActive(!mLayoutState->IsBoldActive());
    }
    else if (controlType == STYLE_ITALICS)
    {
        mLayoutState->SetItalicActive(!mLayoutState->IsItalicActive());
    }
    else if (controlType == STYLE_UNDERLINE)
    {
        mLayoutState->SetUnderlineActive(!mLayoutState->IsUnderlineActive());
    }
    else if (controlType == STYLE_STRIKETHROUGH)
    {
        // Strikethrough doesn't affect font metrics, just visual rendering
    }
    else if (controlType == STYLE_SUBSCRIPT)
    {
        mLayoutState->SetSubscriptActive(!mLayoutState->IsSubscriptActive());
        if (mLayoutState->IsSubscriptActive())
        {
            mLayoutState->SetSuperscriptActive(false);  // Subscript cancels superscript
        }
    }
    else if (controlType == STYLE_SUPERSCRIPT)
    {
        mLayoutState->SetSuperscriptActive(!mLayoutState->IsSuperscriptActive());
        if (mLayoutState->IsSuperscriptActive())
        {
            mLayoutState->SetSubscriptActive(false);  // Superscript cancels subscript
        }
    }
    else if (controlType == STYLE_FONT1)
    {
        // Get font information from document
        sInternalFonts internalfont;
        if (GetDocument()->GetFont(documentPos, internalfont))
        {
            // Update current font string to the new font (TUI version -- no Qt)
            sTUIFontInfo fontInfo;
            fontInfo.name = internalfont.name;
            fontInfo.size = static_cast<int>(internalfont.size);
            mLayoutState->SetCurrentFont(TUIFontUtils::FontToDescriptor(fontInfo));
        }
    }
    else if (controlType == STYLE_INTERNAL_COLOR)
    {
        // Get color from document and apply to layout state
        // Document now stores full RGB directly (no palette lookup needed)
        sSeqRGBColor rgbColor;
        if (GetDocument()->GetColor(documentPos, rgbColor))
        {
            mLayoutState->SetCurrentColor(rgbColor);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraph [in] Paragraph number for the segment
/// @param  startPosition [in] Start position within paragraph
/// @param  documentPos [in] Document position for tab
/// @param  tabInfo [in] Tab information from document
/// @param  lineHeight [in] Line height for the segment
///
/// @return Initialized tab segment
///
/// @brief
/// Creates a tab segment with all properties set. Tab width is left at 0
/// as it's calculated by WordWrapSegmentsIntoLines.
///
/////////////////////////////////////////////////////////////////////////////
sSegmentLayout cLayout::CreateTabSegment(PARAGRAPH_T paragraph, POSITION_T startPosition,
                                            POSITION_T documentPos, const sWSTab& tabInfo,
                                            COORD_T lineHeight)
{
    sSegmentLayout tabSegment;
    tabSegment.paragraph = paragraph;
    tabSegment.startPosition = startPosition;
    tabSegment.length = 0;
    tabSegment.font = TUIFontUtils::FontToDescriptor(mCurrentFontInfo);
    tabSegment.segmentheight = lineHeight;
    tabSegment.isSubscript = mLayoutState->IsSubscriptActive();
    tabSegment.isSuperscript = mLayoutState->IsSuperscriptActive();
    tabSegment.textcolor = mLayoutState->GetCurrentColor();
    tabSegment.backcolor.alpha = 0;
    tabSegment.isBlock = false;
    tabSegment.isTab = true;
    tabSegment.tabDocPosition = documentPos;
    tabSegment.tabType = static_cast<eTabTypes>(tabInfo.type);

    return tabSegment;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  seg [in/out] Segment to add grapheme to
/// @param  currentX [in/out] Current X position, updated with grapheme width
/// @param  grapheme [in] Grapheme to add
///
/// @return nothing
///
/// @brief
/// Measures a grapheme and adds it to the segment. Updates currentX with
/// the grapheme width. Handles HARD_RETURN by measuring as space.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::AddGraphemeToSegment(sSegmentLayout& seg, COORD_T& currentX,
                                     const std::string& grapheme)
{
    // Map HARD_RETURN to space for measurement (CR has undefined width in fonts)
    std::string measureText = (grapheme.size() == 1 && grapheme[0] == HARD_RETURN) ? " " : grapheme;
    COORD_T glyphWidth = GetTextWidth(measureText);

    // Store position (relative to segment start, base-0)
    seg.position.push_back(currentX);
    seg.length++;

    // Accumulate running X position for next grapheme
    currentX += glyphWidth;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  segments [in/out] Vector to add segment to
/// @param  segment [in/out] Segment to flush (cleared after flush)
/// @param  currentX [in] Current X position (becomes totalWidth)
///
/// @return nothing
///
/// @brief
/// If segment has content (length > 0), sets its totalWidth and adds it
/// to the segments vector. Does nothing if segment is empty.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::FlushSegmentIfNonEmpty(std::vector<sSegmentLayout>& segments,
                                       sSegmentLayout& segment, COORD_T currentX)
{
    if (segment.length > 0)
    {
        segment.totalWidth = currentX;
        segments.push_back(segment);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraphNum [in] Paragraph number to build segments for
///
/// @return vector of segments with measurements
///
/// @brief
/// Build all segments for a paragraph in ONE pass through graphemes.
/// This is the TUI equivalent of the GUI's BuildParagraphSegments().
///
/// The algorithm is identical to the GUI version. The only differences
/// are in how fonts are represented (sTUIFontInfo vs QFont) and how text
/// is measured (cTUIFontManager vs QFontMetricsF). Font descriptor
/// strings use the same pipe-delimited format for compatibility.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sSegmentLayout> cLayout::BuildParagraphSegments(PARAGRAPH_T paragraphNum)
{
    std::vector<sSegmentLayout> segments;

    // Validate document
    if (GetDocument() == nullptr)
    {
        return segments;
    }

    // Get graphemes for paragraph using black box API
    std::vector<std::string> graphemes;
    std::vector<POSITION_T> offsets;
    GetDocument()->GetParagraphGraphemes(paragraphNum, graphemes, offsets);

    // Check for empty paragraph
    if (graphemes.empty())
    {
        return segments;
    }

    // Get paragraph start position for control code lookup
    POSITION_T paragraphStart = 0;
    POSITION_T paragraphEnd = 0;
    GetDocument()->GetParagraphStartandEnd(paragraphNum, paragraphStart, paragraphEnd);

    // Get block selection range for segment splitting
    POSITION_T blockStart = 0;
    POSITION_T blockEnd = 0;
    bool hasBlockSelection = GetDocument()->GetBlock(blockStart, blockEnd);

    // Apply initial font state
    UpdateCurrentFont();

    // Calculate line height from TUI font metrics (already in twips)
    COORD_T lineHeight = mTUITextMeasurement->GetFontHeight();

    // Initialize current segment using helper
    sSegmentLayout currentSegment;
    InitializeSegment(currentSegment, paragraphNum, 0, lineHeight);

    // Track current X position (relative to segment start)
    COORD_T currentX = 0;

    // Loop through each grapheme
    for (POSITION_T i = 0; i < static_cast<POSITION_T>(graphemes.size()); ++i)
    {
        const std::string& grapheme = graphemes[i];

        // Check for REPLACE_CHAR (block-begin marker) or SAVE_CHAR (^K0..^K9 bookmark)
        // Both are ALWAYS displayed WITH background (unlike MARKER_CHAR)
        if (!grapheme.empty() && (grapheme[0] == REPLACE_CHAR || grapheme[0] == SAVE_CHAR))
        {
            POSITION_T documentPos = paragraphStart + i;

            // Flush current segment
            FlushSegmentIfNonEmpty(segments, currentSegment, currentX);

            // Start fresh segment for the marker
            InitializeSegment(currentSegment, paragraphNum, i, lineHeight);
            currentX = 0;

            // Get display character and add to segment
            std::string displayGrapheme = GetDisplayCharacter(documentPos, grapheme);
            COORD_T glyphWidth = GetTextWidth(displayGrapheme);

            currentSegment.position.push_back(currentX);
            currentSegment.length++;
            currentX += glyphWidth;

            // Mark as control code for background drawing
            currentSegment.hasControlCodes = true;
            currentSegment.controlCodeIndices.push_back(currentSegment.length - 1);

            continue;
        }

        // Check if this is a control code
        if (!grapheme.empty() && grapheme[0] == MARKER_CHAR)
        {
            POSITION_T documentPos = paragraphStart + i;
            eModifiers controlType = GetDocument()->GetControlChar(documentPos);

            // Tab handling -- single self-contained tab segment
            if (controlType == STYLE_TAB)
            {
                sWSTab tabInfo = GetDocument()->GetTab(documentPos);

                // Flush current segment
                FlushSegmentIfNonEmpty(segments, currentSegment, currentX);

                // Create tab segment using helper
                sSegmentLayout tabSegment = CreateTabSegment(paragraphNum, i, documentPos, tabInfo, lineHeight);

                // Tab segments always occupy 1 grapheme for caret navigation
                tabSegment.position.push_back(0);
                tabSegment.length = 1;

                // Show tab display character and highlight background only in SHOW_ALL
                if (GetShowControl() == SHOW_ALL)
                {
                    tabSegment.hasControlCodes = true;
                    tabSegment.controlCodeIndices.push_back(0);

                    // Only set totalWidth for alignment markers
                    eTabTypes tt = tabSegment.tabType;
                    if (tt == TAB_CENTER || tt == TAB_RIGHT || tt == TAB_RIGHT1)
                    {
                        std::string displayGrapheme = GetDisplayCharacter(documentPos, grapheme);
                        COORD_T charWidth = GetTextWidth(displayGrapheme);
                        tabSegment.totalWidth = charWidth;
                    }
                }

                segments.push_back(tabSegment);

                // Start fresh segment after tab
                InitializeSegment(currentSegment, paragraphNum, i + 1, lineHeight);
                currentX = 0;

                continue;
            }

            // Variable handling - always visible, always expanded
            if (controlType == STYLE_VARIABLE)
            {
                eVariableType varType = GetDocument()->GetVariable(documentPos);
                std::string displayGrapheme = GetVariableExpansion(varType);

                currentSegment.position.push_back(currentX);
                currentSegment.length++;

                // Mark as control code for background drawing in SHOW_ALL mode
                if (GetShowControl() == SHOW_ALL)
                {
                    currentSegment.hasControlCodes = true;
                    currentSegment.controlCodeIndices.push_back(currentSegment.length - 1);
                }

                COORD_T glyphWidth = GetTextWidth(displayGrapheme);
                currentX += glyphWidth;

                continue;
            }

            // Apply formatting control code using helper
            ApplyFormattingControlCode(controlType, documentPos);

            // Update TUI font and recalculate line height
            UpdateCurrentFont();
            lineHeight = mTUITextMeasurement->GetFontHeight();

            // Flush current segment
            FlushSegmentIfNonEmpty(segments, currentSegment, currentX);

            // Start new segment with updated font
            // If SHOW_ALL, control code is part of new segment, so start at i
            // If SHOW_NONE, control code is hidden, so start at i+1
            POSITION_T startPos = (GetShowControl() == SHOW_ALL) ? i : i + 1;
            InitializeSegment(currentSegment, paragraphNum, startPos, lineHeight);
            currentX = 0;

            // If control codes are visible, add to NEW segment with NEW font
            if (GetShowControl() == SHOW_ALL)
            {
                std::string displayGrapheme = GetDisplayCharacter(documentPos, grapheme);
                COORD_T glyphWidth = GetTextWidth(displayGrapheme);

                currentSegment.position.push_back(currentX);
                currentSegment.length++;
                currentX += glyphWidth;

                currentSegment.hasControlCodes = true;
                currentSegment.controlCodeIndices.push_back(currentSegment.length - 1);
            }
        }
        else
        {
            // Regular grapheme - use helper
            AddGraphemeToSegment(currentSegment, currentX, grapheme);
        }

        // Check if next position is a block boundary (requires segment split)
        if (hasBlockSelection && currentSegment.length > 0)
        {
            POSITION_T nextDocPos = paragraphStart + i + 1;
            if (nextDocPos == blockStart || nextDocPos == blockEnd)
            {
                // Save current segment with block marking
                currentSegment.totalWidth = currentX;
                MarkSegmentIfInRange(currentSegment, paragraphStart);
                segments.push_back(currentSegment);

                // Start new segment at boundary
                InitializeSegment(currentSegment, paragraphNum, i + 1, lineHeight);
                currentX = 0;
            }
        }
    }

    // Save final segment if it has content
    if (currentSegment.length > 0)
    {
        currentSegment.totalWidth = currentX;
        MarkSegmentIfInRange(currentSegment, paragraphStart);
        segments.push_back(currentSegment);
    }

    return segments;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  sourceParagraph [in] Paragraph number where header/footer was defined
/// @param  sourceStartPos [in] Position within paragraph where header text starts
/// @param  page [in] Page number for page number substitution
/// @param  outGraphemes [out] Pre-rendered graphemes with # expanded to page number
///
/// @return vector of sSegmentLayout - Segments with fonts and positions
///
/// @brief
/// Segments header/footer text using the same logic as BuildParagraphSegments.
/// This is the TUI equivalent of the GUI's BuildHeaderFooterSegments().
///
/// Key differences from BuildParagraphSegments:
/// - Starts processing at sourceStartPos (after .HE/.FO prefix)
/// - Substitutes '#' with formatted page number
/// - Substitutes '\#' with literal '#' (escape sequence)
/// - Returns pre-rendered graphemes via outGraphemes
/// - No block selection handling (headers/footers can't be selected)
/// - Control codes are never visible (always SHOW_NONE for printing)
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sSegmentLayout> cLayout::BuildHeaderFooterSegments(
    PARAGRAPH_T sourceParagraph,
    POSITION_T sourceStartPos,
    PAGE_T page,
    std::vector<std::string>& outGraphemes)
{
    std::vector<sSegmentLayout> segments;
    outGraphemes.clear();

    // Validate document
    if (GetDocument() == nullptr)
    {
        return segments;
    }

    // Get graphemes for paragraph using black box API
    std::vector<std::string> graphemes;
    std::vector<POSITION_T> offsets;
    GetDocument()->GetParagraphGraphemes(sourceParagraph, graphemes, offsets);

    // Check for valid start position
    if (static_cast<size_t>(sourceStartPos) >= graphemes.size())
    {
        return segments;
    }

    // Get paragraph start position for control code lookup
    POSITION_T paragraphStart = 0;
    POSITION_T paragraphEnd = 0;
    GetDocument()->GetParagraphStartandEnd(sourceParagraph, paragraphStart, paragraphEnd);

    // Pre-format page number for substitution (per-page format, not the running
    // scan format, so earlier pages keep their own format)
    std::string pageNumStr = FormatPageNumber(page, mLayoutState->GetPageNumFormatForPage(page));

    // Apply initial font state
    UpdateCurrentFont();

    // Calculate line height from TUI font metrics (already in twips)
    COORD_T lineHeight = mTUITextMeasurement->GetFontHeight();

    // Initialize current segment using helper
    sSegmentLayout currentSegment;
    InitializeSegment(currentSegment, sourceParagraph, sourceStartPos, lineHeight);

    // Track current X position (relative to segment start)
    COORD_T currentX = 0;

    // Track escape state for \# handling
    bool escapeNext = false;

    // Loop through graphemes starting at sourceStartPos
    for (POSITION_T i = sourceStartPos; i < static_cast<POSITION_T>(graphemes.size()); ++i)
    {
        const std::string& grapheme = graphemes[i];
        POSITION_T documentPos = paragraphStart + i;

        // Handle escape character '\'
        if (grapheme == "\\")
        {
            // Check if next character is '#'
            POSITION_T nextI = i + 1;
            if (nextI < static_cast<POSITION_T>(graphemes.size()) && graphemes[nextI] == "#")
            {
                // Set escape flag and skip the backslash
                escapeNext = true;
                continue;
            }
            // Otherwise treat backslash as regular character (fall through)
        }

        // Handle '#' - page number placeholder or escaped literal
        if (grapheme == "#")
        {
            if (escapeNext)
            {
                // Escaped - add literal '#'
                escapeNext = false;
                COORD_T glyphWidth = GetTextWidth("#");
                currentSegment.position.push_back(currentX);
                currentSegment.length++;
                currentX += glyphWidth;
                outGraphemes.push_back("#");
            }
            else
            {
                // Not escaped - expand to page number
                for (char pnChar : pageNumStr)
                {
                    std::string pnGlyph(1, pnChar);
                    COORD_T glyphWidth = GetTextWidth(pnGlyph);
                    currentSegment.position.push_back(currentX);
                    currentSegment.length++;
                    currentX += glyphWidth;
                    outGraphemes.push_back(pnGlyph);
                }
            }
            continue;
        }

        // Reset escape flag for any other character
        escapeNext = false;

        // Skip block-begin and save-bookmark markers in headers/footers
        if (!grapheme.empty() && (grapheme[0] == REPLACE_CHAR || grapheme[0] == SAVE_CHAR))
        {
            continue;
        }

        // Check if this is a control code (MARKER_CHAR)
        if (!grapheme.empty() && grapheme[0] == MARKER_CHAR)
        {
            eModifiers controlType = GetDocument()->GetControlChar(documentPos);

            // Tab handling -- create self-contained tab segment using helper
            if (controlType == STYLE_TAB)
            {
                sWSTab tabInfo = GetDocument()->GetTab(documentPos);

                // Flush current segment
                FlushSegmentIfNonEmpty(segments, currentSegment, currentX);

                // Create tab segment using helper
                sSegmentLayout tabSegment = CreateTabSegment(sourceParagraph, i, documentPos, tabInfo, lineHeight);
                segments.push_back(tabSegment);

                // Start fresh segment after tab
                InitializeSegment(currentSegment, sourceParagraph, i + 1, lineHeight);
                currentX = 0;

                continue;
            }

            // Variable handling - expand and add to segment
            if (controlType == STYLE_VARIABLE)
            {
                eVariableType varType = GetDocument()->GetVariable(documentPos);

                // For page number, use page-correct pageNumStr (same as # expansion)
                // GetVariableExpansion uses GetCurrentPage() which is wrong during
                // header rendering (mCurrentPage is the last page at that point)
                std::string expansion;
                if (varType == VAR_PAGE_NUMBER)
                {
                    expansion = pageNumStr;
                }
                else
                {
                    expansion = GetVariableExpansion(varType);
                }

                // Add each character of the expansion to both segment and outGraphemes
                for (char expChar : expansion)
                {
                    std::string expGlyph(1, expChar);
                    COORD_T glyphWidth = GetTextWidth(expGlyph);
                    currentSegment.position.push_back(currentX);
                    currentSegment.length++;
                    currentX += glyphWidth;
                    outGraphemes.push_back(expGlyph);
                }
                continue;
            }

            // Apply formatting control code using helper
            ApplyFormattingControlCode(controlType, documentPos);

            // Update TUI font and recalculate line height
            UpdateCurrentFont();
            lineHeight = mTUITextMeasurement->GetFontHeight();

            // Flush current segment
            FlushSegmentIfNonEmpty(segments, currentSegment, currentX);

            // Start new segment with updated font (control codes never visible in headers)
            InitializeSegment(currentSegment, sourceParagraph, i + 1, lineHeight);
            currentX = 0;

            continue;
        }
        else
        {
            // Regular grapheme - add to segment and outGraphemes
            AddGraphemeToSegment(currentSegment, currentX, grapheme);
            outGraphemes.push_back(grapheme);
        }
    }

    // Save final segment if it has content
    FlushSegmentIfNonEmpty(segments, currentSegment, currentX);

    return segments;
}
