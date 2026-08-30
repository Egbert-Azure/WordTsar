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
 *
 * @brief Qt-specific layout engine extending cLayoutBase for GUI rendering.
 *
 * Implements the cLayout class, which provides the Qt-dependent overrides for
 * the base layout engine. This is the GUI counterpart to the TUI cLayout class.
 *
 * @section guilayout_overrides Virtual Method Implementations
 * - BuildParagraphSegments(): creates sSegmentLayout entries for each grapheme
 *   with text width measured by cQtTextMeasurement and font formatting applied
 *   via QFont (bold, italic, underline, superscript, subscript, color)
 * - BuildHeaderFooterSegments(): constructs segment layouts for header/footer
 *   text with proper font and alignment
 * - GetFontWithFormatting(): creates a font descriptor string from the current
 *   layout state formatting (font name, size, bold, italic, etc.)
 *
 * @section guilayout_measurement Text Measurement
 * Uses cQtTextMeasurement (backed by QFontMetricsF) for all text width
 * and line height calculations. Font descriptors are converted between
 * the pipe-delimited layout format and QFont objects via the FontUtils
 * namespace.
 *
 * @section guilayout_parity GUI/TUI Parity
 * The GUI and TUI both define a class named cLayout that extends cLayoutBase.
 * They cannot coexist in the same binary due to this name collision. Both
 * use identical font descriptor formats for cross-platform consistency.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cLayout GUI layout class
 * @see cLayoutBase Base layout engine
 * @see cQtTextMeasurement Qt text measurement implementation
 * @see FontUtils Font descriptor conversion namespace
 * @see sSegmentLayout Per-grapheme layout data structure
 */

#include "layout.h"
#include "qttextmeasurement.h"
#include "src/gui/utils/fontutils.h"
#include "src/core/document/document.h"
#include "src/core/include/timer.h"
#include <QString>
#include <QDebug>
#include <sstream>
#include <iomanip>


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor - initializes font to Courier New 12pt and creates
/// font metrics for text measurement.
///
///
/////////////////////////////////////////////////////////////////////////////
cLayout::cLayout(void)
{
    // Create Qt text measurement implementation
    mQtTextMeasurement = new cQtTextMeasurement();

    // Set text measurement interface on base class
    SetTextMeasurement(mQtTextMeasurement);

    // Initialize with a fixed monospace font
    mFont.setFamily("Courier New");
    mFont.setPointSizeF(12.0);
    mFont.setStyleHint(QFont::TypeWriter);  // Fallback to monospace if Courier New not available

    // Update text measurement with initial font
    mQtTextMeasurement->SetFont(mFont);

    // Create font metrics (still needed for direct access in this class)
    mFontMetrics = new QFontMetricsF(mFont);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor - cleans up font metrics
///
/////////////////////////////////////////////////////////////////////////////
cLayout::~cLayout(void)
{
    delete mFontMetrics;
    mFontMetrics = nullptr;

    // Delete Qt text measurement
    delete mQtTextMeasurement;
    mQtTextMeasurement = nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] Qt font to use for layout
///
/// @return nothing
///
/// @brief
/// Sets the font used for text measurement and layout.
/// Updates the font metrics to match the new font.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::SetFont(const QFont& font)
{
    mFont = font;

    // Update text measurement with new font
    if (mQtTextMeasurement)
    {
        mQtTextMeasurement->SetFont(mFont);
    }

    // Recreate font metrics with new font
    delete mFontMetrics;
    mFontMetrics = new QFontMetricsF(mFont);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current QFont
///
/// @brief
/// Returns the current font used for layout
///
/////////////////////////////////////////////////////////////////////////////
QFont cLayout::GetFont(void) const
{
    return mFont;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] font specification string
///
/// @return nothing
///
/// @brief
/// Override of base class SetDefaultFont to update Qt font objects.
/// Converts font string to QFont and updates metrics.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::SetDefaultFont(const std::string& font)
{
    // Call base class to set mLayoutState->GetCurrentFont()
    cLayoutBase::SetDefaultFont(font);

    // Convert string to QFont and update Qt font metrics
    QFont qfont = FontUtils::FontFromDescriptor(font);
    SetFont(qfont);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Updates mFont to reflect current formatting state (bold, italic, etc.).
/// Extracts duplicated font update logic into a reusable method.
///
/// This method applies the current formatting flags (mLayoutState->IsBoldActive(), mLayoutState->IsItalicActive(),
/// mLayoutState->IsUnderlineActive(), mLayoutState->IsSubscriptActive(), mLayoutState->IsSuperscriptActive()) to mFont and
/// updates mFontMetrics accordingly.
///
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::UpdateCurrentFont(void)
{
    // Validate mLayoutState->GetCurrentFont() - if empty or invalid, use default
    // This handles cases where endState wasn't initialized or font string is corrupt
    if (mLayoutState->GetCurrentFont().empty())
    {
        mLayoutState->GetCurrentFont() = mLayoutState->GetDefaultFont();
    }

    // Build fresh clean font from mLayoutState->GetCurrentFont() string (no styles)
    QFont cleanFont = FontUtils::FontFromDescriptor(mLayoutState->GetCurrentFont());

    // Apply current formatting state to clean font
    mFont = cleanFont;
    mFont.setBold(mLayoutState->IsBoldActive());
    mFont.setItalic(mLayoutState->IsItalicActive());
    mFont.setUnderline(mLayoutState->IsUnderlineActive());

    // Handle subscript/superscript font size
    if (mLayoutState->IsSubscriptActive() || mLayoutState->IsSuperscriptActive())
    {
        qreal baseSize = cleanFont.pointSizeF();
        mFont.setPointSizeF(baseSize * 0.58);
    }

    // Update font metrics for width calculations
    delete mFontMetrics;
    mFontMetrics = new QFontMetricsF(mFont);

    // Update text measurement with new font
    if (mQtTextMeasurement)
    {
        mQtTextMeasurement->SetFont(mFont);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  bold [in] apply bold formatting
/// @param  italic [in] apply italic formatting
/// @param  underline [in] apply underline formatting
///
/// @return font descriptor string with formatting applied
///
/// @brief
/// Creates a font descriptor string with the specified formatting applied.
/// Uses the current base font from layout state and applies formatting flags.
///
/////////////////////////////////////////////////////////////////////////////
std::string cLayout::GetFontWithFormatting(bool bold, bool italic, bool underline)
{
    // Start with the current base font
    QFont font = FontUtils::FontFromDescriptor(mLayoutState->GetCurrentFont());

    // Apply formatting flags
    font.setBold(bold);
    font.setItalic(italic);
    font.setUnderline(underline);

    // Return as descriptor string
    return FontUtils::FontToDescriptor(font);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  seg [out] - Segment to initialize
/// @param  paragraph [in] - Paragraph number for the segment
/// @param  startPosition [in] - Start position within paragraph
/// @param  lineHeight [in] - Line height for the segment
///
/// @return nothing
///
/// @brief
/// Initializes a segment with default values and current formatting state.
/// Extracts common segment initialization code from Build*Segments methods.
///
/////////////////////////////////////////////////////////////////////////////
void cLayout::InitializeSegment(sSegmentLayout& seg, PARAGRAPH_T paragraph,
                                  POSITION_T startPosition, COORD_T lineHeight)
{
    seg = sSegmentLayout();
    seg.paragraph = paragraph;
    seg.startPosition = startPosition;
    seg.length = 0;
    seg.font = FontUtils::FontToDescriptor(mFont);
    seg.segmentheight = lineHeight;
    seg.isSubscript = mLayoutState->IsSubscriptActive();
    seg.isSuperscript = mLayoutState->IsSuperscriptActive();
    seg.textcolor = mLayoutState->GetCurrentColor();
    seg.backcolor.alpha = 0;
    seg.isBlock = false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  controlType [in] - Type of formatting control code
/// @param  documentPos [in] - Document position for font lookup
///
/// @return nothing
///
/// @brief
/// Applies a formatting control code by toggling the appropriate state.
/// Handles bold, italic, underline, strikethrough, subscript, superscript,
/// and font changes. Does NOT call UpdateCurrentFont() - caller must do that.
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
            // Update current font string to the new font
            QFont tempFont(QString::fromStdString(internalfont.name));
            tempFont.setPointSizeF(internalfont.size);
            mLayoutState->SetCurrentFont(FontUtils::FontToDescriptor(tempFont));
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
/// @param  paragraph [in] - Paragraph number for the segment
/// @param  startPosition [in] - Start position within paragraph
/// @param  documentPos [in] - Document position for tab
/// @param  tabInfo [in] - Tab information from document
/// @param  lineHeight [in] - Line height for the segment
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
    tabSegment.font = FontUtils::FontToDescriptor(mFont);
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
/// @param  seg [in/out] - Segment to add grapheme to
/// @param  currentX [in/out] - Current X position, updated with grapheme width
/// @param  grapheme [in] - Grapheme to add
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
/// @param  segments [in/out] - Vector to add segment to
/// @param  segment [in/out] - Segment to flush (cleared after flush)
/// @param  currentX [in] - Current X position (becomes totalWidth)
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
/// @param  paragraphNum [in] paragraph number to build segments for
///
/// @return vector of segments with measurements
///
/// @brief
/// Build all segments for a paragraph in ONE pass through graphemes.
///
/// This GUI layer override adds:
/// - UpdateCurrentFont() calls to update Qt fonts when formatting changes
/// - GetTextWidth() measurement for each grapheme
/// - Storage of widths in segment.position[] array (base-0 relative)
/// - Calculation of segment.totalWidth
///
/// Member variables (mLayoutState->IsBoldActive(), mLayoutState->IsItalicActive(), etc.) track formatting
/// state and are used by UpdateCurrentFont() to update mFont.
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

    // Calculate line height from actual font metrics
    COORD_T lineHeight = static_cast<COORD_T>(mFontMetrics->height() * FONTSCALE);

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

            // Update Qt font and recalculate line height
            UpdateCurrentFont();
            lineHeight = static_cast<COORD_T>(mFontMetrics->height() * FONTSCALE);

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
/// @param  sourceParagraph [in] - Paragraph number where header/footer was defined
/// @param  sourceStartPos [in] - Position within paragraph where header text starts
/// @param  page [in] - Page number for page number substitution
/// @param  outGraphemes [out] - Pre-rendered graphemes with # expanded to page number
///
/// @return vector of sSegmentLayout - Segments with fonts and positions
///
/// @brief
/// Segments header/footer text using the same logic as BuildParagraphSegments.
/// Headers/footers are treated as full WordStar paragraphs supporting all
/// formatting: fonts, bold, italic, underline, subscript, superscript, tabs,
/// variables, etc.
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

    // Calculate line height from actual font metrics
    COORD_T lineHeight = static_cast<COORD_T>(mFontMetrics->height() * FONTSCALE);

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

            // Update Qt font and recalculate line height
            UpdateCurrentFont();
            lineHeight = static_cast<COORD_T>(mFontMetrics->height() * FONTSCALE);

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


