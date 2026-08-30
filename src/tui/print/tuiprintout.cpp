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
 * @class cTUIPrintout
 * @brief TUI PDF generation and print preview via libharu (HPDF) with TrueType font embedding.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIPrintout, which generates PDF output from the fully laid-out
 * document for the terminal-based interface. Walks all layout pages, rendering
 * body text lines, headers, and footers with proper font embedding via the
 * TUI font manager. Supports bold, italic, underline, strikethrough,
 * superscript, and subscript formatting through embedded TrueType fonts.
 * Provides print preview by writing a temporary PDF and launching an
 * external viewer (xdg-open, evince, or open on macOS).
 *
 * @section tuiprint_rendering Page Rendering
 * Iterates over all pages from the layout engine, converting twips-based
 * coordinates to PDF points (1/72 inch). Each line's segments are rendered
 * with the appropriate embedded font, applying formatting attributes from
 * the layout segments.
 *
 * @section tuiprint_preview Print Preview
 * Writes the generated PDF to a temporary file and shells out to xdg-open,
 * evince, or open (macOS) to display it. The temporary file is cleaned up
 * after the viewer exits.
 *
 * @see cTUIPrintout
 * @see cTUIEditorCtrl
 * @see cLayoutBase
 * @see cDocument
 * @see cTUIFontManager
 */

#include "tuiprintout.h"

#include "src/core/editor/editorbase.h"
#include "src/tui/layout/layout.h"
#include "src/tui/layout/tuifontutils.h"
#include "src/tui/fonts/fontmanager.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/document/document.h"
#include "src/core/include/config.h"

#include <filesystem>
#include <cstdlib>
#include <cstring>


/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor [in] - TUI editor control containing layout and document
///
/// @return nothing
///
/// @brief
/// Constructor. Stores references to editor, layout, document, and font
/// manager for use during PDF generation.
///
/////////////////////////////////////////////////////////////////////////////
cTUIPrintout::cTUIPrintout(cEditorBase* editor)
    : mEditor(editor)
    , mLayout(nullptr)
    , mDocument(nullptr)
    , mFontManager(nullptr)
    , mPdf(nullptr)
{
    mLayout = mEditor->GetLayout();
    if (mLayout)
    {
        mDocument = mLayout->GetDocument();

        // Access the TUI font manager through the TUI layout
        cLayout* tuiLayout = dynamic_cast<cLayout*>(mLayout);
        if (tuiLayout)
        {
            mFontManager = tuiLayout->GetFontManager();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor. Font cache entries are owned by the HPDF_Doc and are
/// freed when the document is freed, so no manual cleanup needed.
///
/////////////////////////////////////////////////////////////////////////////
cTUIPrintout::~cTUIPrintout(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filepath [in] - Output PDF file path
///
/// @return true if PDF was generated successfully, false on error
///
/// @brief
/// Generates a complete PDF of the laid-out document. Creates one PDF
/// page per layout page, rendering all body text, headers, and footers
/// with embedded TrueType fonts.
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIPrintout::GeneratePDF(const std::string& filepath)
{
    if (!mLayout || !mDocument)
    {
        return false;
    }

    // Create libharu PDF document
    mPdf = HPDF_New(nullptr, nullptr);
    if (!mPdf)
    {
        return false;
    }

    // Use UTF-8 encoding
    HPDF_UseUTFEncodings(mPdf);
    HPDF_SetCurrentEncoder(mPdf, "UTF-8");

    // Clear font cache for this generation pass
    mFontCache.clear();

    // Get total number of pages from layout
    PAGE_T numPages = mLayout->GetNumberOfPages();
    if (numPages == 0)
    {
        HPDF_Free(mPdf);
        mPdf = nullptr;
        return false;
    }

    // Generate each page
    for (PAGE_T pageNum = 1; pageNum <= numPages; pageNum++)
    {
        // Get page dimensions from layout
        sPageInfo pageInfo = mLayout->GetPageInfo(pageNum);
        double pageWidthPt = TwipsToPoints(pageInfo.paperwidth);
        double pageHeightPt = TwipsToPoints(pageInfo.paperheight);

        // Create PDF page with document dimensions
        HPDF_Page page = HPDF_AddPage(mPdf);
        HPDF_Page_SetWidth(page, static_cast<HPDF_REAL>(pageWidthPt));
        HPDF_Page_SetHeight(page, static_cast<HPDF_REAL>(pageHeightPt));

        // Render body text and headers/footers
        RenderPage(page, pageNum);
        RenderHeadersFooters(page, pageNum, pageHeightPt);
    }

    // Save PDF to file
    HPDF_STATUS status = HPDF_SaveToFile(mPdf, filepath.c_str());

    // Clean up
    HPDF_Free(mPdf);
    mPdf = nullptr;
    mFontCache.clear();

    return (status == HPDF_OK);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Generates a PDF to a temp file and launches the system PDF viewer.
/// Checks for a graphical desktop environment first. The temp file is
/// named WordTsar-<docname>-preview.pdf in the system temp directory.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::PrintPreview(void)
{
    // Check for desktop environment before generating
    if (!HasDisplayEnvironment())
    {
        mEditor->ShowMessage("Print Preview",
            "Print preview requires a graphical desktop environment.\n"
            "No DISPLAY or WAYLAND_DISPLAY detected (SSH session?).\n\n"
            "Save your document and transfer it to a system with a PDF viewer.");
        return;
    }

    if (!mLayout || !mDocument)
    {
        mEditor->ShowError("Print Preview", "No document loaded.");
        return;
    }

    // Save current show control state (same pattern as GUI printDocument())
    eShowControl savedShowControl = mLayout->GetShowControl();

    // Switch to SHOW_NONE so control codes and dot commands are hidden
    mLayout->SetShowControl(SHOW_NONE);
    mLayout->SetActiveParagraph(-1);

    // Re-layout the document with SHOW_NONE to get clean print output
    mLayout->LayoutDocument(mDocument);

    // Build temp file path
    std::string tempPath = BuildPreviewPath();

    // Generate the PDF
    bool success = GeneratePDF(tempPath);

    // Restore original show control state
    mLayout->SetShowControl(savedShowControl);

    // Restore active paragraph from current cursor position
    PARAGRAPH_T curPara = mDocument->GetParagraphFromPosition(mDocument->GetPosition());
    mLayout->SetActiveParagraph(curPara);

    // Re-layout to restore the editor display
    mLayout->LayoutDocument(mDocument);

    if (!success)
    {
        mEditor->ShowError("Print Preview", "Failed to generate PDF file.");
        return;
    }

    // Launch system PDF viewer
    LaunchPDFViewer(tempPath);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if a graphical desktop is available
///
/// @brief
/// Checks whether a desktop environment is available for launching a
/// PDF viewer. On Linux, checks DISPLAY and WAYLAND_DISPLAY environment
/// variables. On macOS, checks DISPLAY (for X11 forwarding over SSH) or
/// assumes local desktop if running from a local terminal. On Windows,
/// checks for a console window as a proxy for an interactive session.
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIPrintout::HasDisplayEnvironment(void)
{
#ifdef _WIN32
    // Windows: check if we have an interactive session
    // GetConsoleWindow() returns NULL for non-interactive services
    // For SSH sessions, the start command may still work
    return true;
#elif defined(__APPLE__)
    // macOS: if SSH_CONNECTION is set and no DISPLAY is available,
    // the 'open' command may not work (headless SSH session).
    // If SSH_CONNECTION is not set, we're on a local desktop.
    const char* sshConn = getenv("SSH_CONNECTION");
    if (sshConn != nullptr && sshConn[0] != '\0')
    {
        // Remote SSH session -- check for X11 forwarding
        const char* display = getenv("DISPLAY");
        return (display != nullptr && display[0] != '\0');
    }
    // Local session -- macOS always has a desktop
    return true;
#else
    // Linux/BSD: check for X11 or Wayland display
    const char* display = getenv("DISPLAY");
    const char* wayland = getenv("WAYLAND_DISPLAY");
    return (display != nullptr && display[0] != '\0')
        || (wayland != nullptr && wayland[0] != '\0');
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  twips [in] - Coordinate value in twips
///
/// @return Value in PDF points (1 point = 20 twips)
///
/// @brief
/// Converts a twips measurement to PDF points. Both the layout engine
/// and PDF use points as a base unit, with 1 point = 20 twips.
///
/////////////////////////////////////////////////////////////////////////////
double cTUIPrintout::TwipsToPoints(COORD_T twips) const
{
    return static_cast<double>(twips) / 20.0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] - libharu page handle
/// @param  pageNum [in] - Page number (1-based)
///
/// @return nothing
///
/// @brief
/// Renders all body text for one page. Iterates through all paragraphs,
/// finds lines belonging to this page, and renders each line's segments.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::RenderPage(HPDF_Page page, PAGE_T pageNum)
{
    // Get page height for Y-axis flip (PDF Y is bottom-up)
    sPageInfo pageInfo = mLayout->GetPageInfo(pageNum);
    double pageHeightPt = TwipsToPoints(pageInfo.paperheight);

    // Iterate through all paragraphs and render lines on this page
    // (same approach as GUI printout.cpp)
    for (PARAGRAPH_T paraNum = 0; paraNum < mLayout->GetNumberOfParagraphs(); paraNum++)
    {
        const sParagraphLayout* para = mLayout->GetParagraphLayout(paraNum);
        if (!para)
        {
            continue;
        }

        for (const auto& line : para->lines)
        {
            if (line.pagenumber == pageNum)
            {
                RenderLine(page, line, pageHeightPt);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] - libharu page handle
/// @param  pageNum [in] - Page number (1-based)
/// @param  pageHeightPt [in] - Page height in PDF points (for Y flip)
///
/// @return nothing
///
/// @brief
/// Renders headers and footers for a page. Uses pre-rendered graphemes
/// stored in sHeaderFooterLine (header/footer text is not in the
/// document model).
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::RenderHeadersFooters(HPDF_Page page, PAGE_T pageNum, double pageHeightPt)
{
    if (!mLayout)
    {
        return;
    }

    // Get headers and footers
    const auto& allHeaders = mLayout->GetPageHeaders();
    const auto& allFooters = mLayout->GetPageFooters();

    // Draw headers for this page
    auto headerIt = allHeaders.find(pageNum);
    if (headerIt != allHeaders.end())
    {
        for (const auto& hfLine : headerIt->second)
        {
            RenderHeaderFooterLine(page, hfLine, pageHeightPt);
        }
    }

    // Draw footers for this page
    auto footerIt = allFooters.find(pageNum);
    if (footerIt != allFooters.end())
    {
        for (const auto& hfLine : footerIt->second)
        {
            RenderHeaderFooterLine(page, hfLine, pageHeightPt);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] - libharu page handle
/// @param  hfLine [in] - Header/footer line with pre-rendered graphemes
/// @param  pageHeightPt [in] - Page height in PDF points (for Y flip)
///
/// @return nothing
///
/// @brief
/// Renders a single header or footer line. Header/footer graphemes are
/// stored in the sHeaderFooterLine struct, not in the document. Iterates
/// segments and places each grapheme at its computed position.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::RenderHeaderFooterLine(HPDF_Page page, const sHeaderFooterLine& hfLine,
                                           double pageHeightPt)
{
    const sLineLayout& line = hfLine.line;

    if (line.segments.empty() || hfLine.graphemes.empty())
    {
        return;
    }

    // Compute the maximum segment height for baseline alignment
    // (sub/super segments have reduced height, but baseline must use normal height)
    COORD_T lineHeight = 0;
    for (const auto& seg : line.segments)
    {
        if (seg.segmentheight > lineHeight)
        {
            lineHeight = seg.segmentheight;
        }
    }

    // Track current grapheme index across segments
    size_t graphemeIndex = 0;
    COORD_T segmentBaseX = 0;

    for (const auto& segment : line.segments)
    {
        // Set font
        HPDF_Font pdfFont = GetOrLoadFont(segment.font);
        double pointSize = TUIFontUtils::GetSizeFromDescriptor(segment.font);
        if (pointSize <= 0)
        {
            pointSize = 12.0;
        }

        if (pdfFont)
        {
            HPDF_Page_SetFontAndSize(page, pdfFont, static_cast<HPDF_REAL>(pointSize));
        }

        // Set text color (default sentinel prints as black)
        if (segment.textcolor.IsDefault())
        {
            HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            HPDF_Page_SetRGBFill(page,
                segment.textcolor.red / 255.0f,
                segment.textcolor.green / 255.0f,
                segment.textcolor.blue / 255.0f);
        }

        // Draw each grapheme in this segment
        for (size_t i = 0; i < segment.position.size() && graphemeIndex < hfLine.graphemes.size(); ++i)
        {
            const std::string& grapheme = hfLine.graphemes[graphemeIndex];

            if (!grapheme.empty())
            {
                // Skip non-printable characters (CR, LF, tab, etc.)
                unsigned char firstByte = static_cast<unsigned char>(grapheme[0]);
                if (firstByte < 0x20 || firstByte == 0x7F)
                {
                    graphemeIndex++;
                    continue;
                }

                // Compute absolute position
                COORD_T glyphX = line.pagex + segmentBaseX + static_cast<COORD_T>(segment.position[i]);

                // Convert to PDF coordinates (Y flip)
                // Sub/super use lineHeight (normal height) as baseline reference + .SR roll
                double pdfX = TwipsToPoints(glyphX);
                double pdfY;
                if (segment.isSubscript)
                {
                    pdfY = pageHeightPt - TwipsToPoints(line.pagey + lineHeight + mLayout->GetSubSuperRoll()
                                                         - segment.segmentheight / 2);
                }
                else if (segment.isSuperscript)
                {
                    pdfY = pageHeightPt - TwipsToPoints(line.pagey + lineHeight - mLayout->GetSubSuperRoll());
                }
                else
                {
                    pdfY = pageHeightPt - TwipsToPoints(line.pagey + segment.segmentheight);
                }

                // Draw the grapheme
                HPDF_Page_BeginText(page);
                HPDF_Page_MoveTextPos(page,
                    static_cast<HPDF_REAL>(pdfX),
                    static_cast<HPDF_REAL>(pdfY));
                HPDF_Page_ShowText(page, grapheme.c_str());
                HPDF_Page_EndText(page);
            }

            graphemeIndex++;
        }

        // Draw underline for this segment if the font descriptor has underline flag set
        if (TUIFontUtils::IsUnderlineInDescriptor(segment.font) && !segment.position.empty())
        {
            double underlineThickness = pointSize / 14.0;
            double underlineOffset = pointSize / 7.0;

            // X range for this segment
            double startX = TwipsToPoints(line.pagex + segmentBaseX
                                          + static_cast<COORD_T>(segment.position.front()));
            double endX = TwipsToPoints(line.pagex + segmentBaseX
                                        + static_cast<COORD_T>(segment.position.back())
                                        + segment.totalWidth / static_cast<COORD_T>(segment.position.size()));

            // Baseline Y using same calculation as text rendering
            double baselineY;
            if (segment.isSubscript)
            {
                baselineY = pageHeightPt - TwipsToPoints(line.pagey + lineHeight + mLayout->GetSubSuperRoll()
                                                          - segment.segmentheight / 2);
            }
            else if (segment.isSuperscript)
            {
                baselineY = pageHeightPt - TwipsToPoints(line.pagey + lineHeight - mLayout->GetSubSuperRoll());
            }
            else
            {
                baselineY = pageHeightPt - TwipsToPoints(line.pagey + segment.segmentheight);
            }
            double lineDrawY = baselineY - underlineOffset;

            if (segment.textcolor.IsDefault())
            {
                HPDF_Page_SetRGBStroke(page, 0.0f, 0.0f, 0.0f);
            }
            else
            {
                HPDF_Page_SetRGBStroke(page,
                    segment.textcolor.red / 255.0f,
                    segment.textcolor.green / 255.0f,
                    segment.textcolor.blue / 255.0f);
            }
            HPDF_Page_SetLineWidth(page, static_cast<HPDF_REAL>(underlineThickness));
            HPDF_Page_MoveTo(page,
                static_cast<HPDF_REAL>(startX),
                static_cast<HPDF_REAL>(lineDrawY));
            HPDF_Page_LineTo(page,
                static_cast<HPDF_REAL>(endX),
                static_cast<HPDF_REAL>(lineDrawY));
            HPDF_Page_Stroke(page);
        }

        // Update base X for next segment
        segmentBaseX += segment.totalWidth;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] - libharu page handle
/// @param  line [in] - Layout line to render
/// @param  pageHeightPt [in] - Page height in PDF points (for Y flip)
///
/// @return nothing
///
/// @brief
/// Renders a single line by iterating through its segments and rendering
/// each one. Skips non-printable lines (dot commands).
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::RenderLine(HPDF_Page page, const sLineLayout& line, double pageHeightPt)
{
    // Skip dot command lines
    if (!line.isPrintable)
    {
        return;
    }

    // Compute the maximum segment height across all segments in this line.
    // Sub/superscript segments have reduced heights (~58% of normal), but their
    // baselines must be computed relative to the normal line height.
    COORD_T lineHeight = 0;
    for (const auto& seg : line.segments)
    {
        if (seg.segmentheight > lineHeight)
        {
            lineHeight = seg.segmentheight;
        }
    }

    // Render each segment in the line
    for (const auto& segment : line.segments)
    {
        RenderSegment(page, segment, line.pagex, line.pagey, lineHeight, pageHeightPt);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] - libharu page handle
/// @param  segment [in] - Segment containing positions and formatting
/// @param  lineX [in] - Line's X position on page (twips)
/// @param  lineY [in] - Line's Y position on page (twips)
/// @param  lineHeight [in] - Max segment height in line (twips, for baseline)
/// @param  pageHeightPt [in] - Page height in PDF points (for Y flip)
///
/// @return nothing
///
/// @brief
/// Renders a single segment with its glyphs at pre-calculated positions.
/// Sets font and color from segment, then draws each glyph individually
/// at its absolute position (converted from twips to PDF points).
///
/// Sub/superscript segments have reduced segmentheight (~58% of normal).
/// Their baselines are computed from lineHeight (the normal height) with
/// the .SR roll offset applied, matching the GUI screen renderer approach.
///
/// Handles MARKER_CHAR control codes: expands variables and skips other
/// control characters (same logic as GUI printout).
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::RenderSegment(HPDF_Page page, const sSegmentLayout& segment,
                                  COORD_T lineX, COORD_T lineY, COORD_T lineHeight,
                                  double pageHeightPt)
{
    // Skip empty segments
    if (!mDocument || segment.position.empty() || segment.GetGraphemeCount() == 0)
    {
        return;
    }

    // Fetch graphemes from document
    std::vector<std::string> graphemes;
    segment.GetGraphemes(mDocument, graphemes);

    if (graphemes.empty())
    {
        return;
    }

    // Set font from segment descriptor
    HPDF_Font pdfFont = GetOrLoadFont(segment.font);
    double pointSize = TUIFontUtils::GetSizeFromDescriptor(segment.font);
    if (pointSize <= 0)
    {
        pointSize = 12.0;
    }

    if (pdfFont)
    {
        HPDF_Page_SetFontAndSize(page, pdfFont, static_cast<HPDF_REAL>(pointSize));
    }

    // Set text color from segment (default sentinel prints as black)
    if (segment.textcolor.IsDefault())
    {
        HPDF_Page_SetRGBFill(page, 0.0f, 0.0f, 0.0f);
    }
    else
    {
        HPDF_Page_SetRGBFill(page,
            segment.textcolor.red / 255.0f,
            segment.textcolor.green / 255.0f,
            segment.textcolor.blue / 255.0f);
    }

    // Draw each grapheme at its position
    for (size_t i = 0; i < graphemes.size(); ++i)
    {
        std::string displayGrapheme = graphemes[i];

        // Handle MARKER_CHAR (control codes stored in document)
        if (!graphemes[i].empty() && graphemes[i][0] == MARKER_CHAR)
        {
            // Calculate document position for variable expansion
            POSITION_T paragraphStart = 0;
            POSITION_T paragraphEnd = 0;
            mDocument->GetParagraphStartandEnd(segment.paragraph, paragraphStart, paragraphEnd);
            POSITION_T docPos = paragraphStart + segment.startPosition + static_cast<POSITION_T>(i);

            eModifiers controlType = mDocument->GetControlChar(docPos);
            if (controlType == STYLE_VARIABLE)
            {
                // Expand variable (page number, date, etc.)
                eVariableType varType = mDocument->GetVariable(docPos);
                displayGrapheme = mLayout->GetVariableExpansion(varType);
            }
            else
            {
                // Skip other control codes in print output
                continue;
            }
        }

        // Skip empty graphemes and non-printable characters (CR, LF, tab, etc.)
        // Qt's drawText() silently ignores these, but libharu renders them as boxes
        if (displayGrapheme.empty())
        {
            continue;
        }
        unsigned char firstByte = static_cast<unsigned char>(displayGrapheme[0]);
        if (firstByte < 0x20 || firstByte == 0x7F)
        {
            continue;
        }

        // Get X position: add line's pagex to relative segment position
        COORD_T glyphX = lineX + static_cast<COORD_T>(segment.position[i]);

        // Convert to PDF coordinates
        // PDF Y-axis is bottom-up, layout Y is top-down
        // Normal text baseline: pageHeight - (lineY + segmentheight)
        // Sub/super: use lineHeight (normal height) as baseline reference,
        // then apply .SR roll offset (matches GUI screen renderer at editorctrl.cpp:2171)
        double pdfX = TwipsToPoints(glyphX);
        double pdfY;
        if (segment.isSubscript)
        {
            // Subtract half the subscript char height to compensate for descent gap
            pdfY = pageHeightPt - TwipsToPoints(lineY + lineHeight + mLayout->GetSubSuperRoll()
                                                 - segment.segmentheight / 2);
        }
        else if (segment.isSuperscript)
        {
            pdfY = pageHeightPt - TwipsToPoints(lineY + lineHeight - mLayout->GetSubSuperRoll());
        }
        else
        {
            pdfY = pageHeightPt - TwipsToPoints(lineY + segment.segmentheight);
        }

        // Draw the grapheme using libharu text operations
        HPDF_Page_BeginText(page);
        HPDF_Page_MoveTextPos(page,
            static_cast<HPDF_REAL>(pdfX),
            static_cast<HPDF_REAL>(pdfY));
        HPDF_Page_ShowText(page, displayGrapheme.c_str());
        HPDF_Page_EndText(page);
    }

    // Draw underline if the font descriptor has the underline flag set
    // libharu does not support font-level underline (unlike Qt), so draw manually
    if (TUIFontUtils::IsUnderlineInDescriptor(segment.font) && !segment.position.empty())
    {
        // Calculate underline geometry from font size
        double underlineThickness = pointSize / 14.0;
        double underlineOffset = pointSize / 7.0;

        // X range: from first glyph to end of last glyph
        double startX = TwipsToPoints(lineX + static_cast<COORD_T>(segment.position.front()));
        double endX = TwipsToPoints(lineX + static_cast<COORD_T>(segment.position.back())
                                    + segment.totalWidth / static_cast<COORD_T>(segment.position.size()));

        // Y position: baseline minus offset (PDF Y goes up, so subtract to go down)
        // Use same baseline calculation as text rendering above
        double baselineY;
        if (segment.isSubscript)
        {
            baselineY = pageHeightPt - TwipsToPoints(lineY + lineHeight + mLayout->GetSubSuperRoll()
                                                      - segment.segmentheight / 2);
        }
        else if (segment.isSuperscript)
        {
            baselineY = pageHeightPt - TwipsToPoints(lineY + lineHeight - mLayout->GetSubSuperRoll());
        }
        else
        {
            baselineY = pageHeightPt - TwipsToPoints(lineY + segment.segmentheight);
        }
        double lineDrawY = baselineY - underlineOffset;

        // Set line color to match text color (default sentinel prints as black)
        if (segment.textcolor.IsDefault())
        {
            HPDF_Page_SetRGBStroke(page, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            HPDF_Page_SetRGBStroke(page,
                segment.textcolor.red / 255.0f,
                segment.textcolor.green / 255.0f,
                segment.textcolor.blue / 255.0f);
        }
        HPDF_Page_SetLineWidth(page, static_cast<HPDF_REAL>(underlineThickness));
        HPDF_Page_MoveTo(page,
            static_cast<HPDF_REAL>(startX),
            static_cast<HPDF_REAL>(lineDrawY));
        HPDF_Page_LineTo(page,
            static_cast<HPDF_REAL>(endX),
            static_cast<HPDF_REAL>(lineDrawY));
        HPDF_Page_Stroke(page);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string (pipe-delimited)
///
/// @return HPDF_Font handle, or nullptr if font could not be loaded
///
/// @brief
/// Loads a TrueType font into the PDF document and caches it by
/// descriptor string. Uses the TUI font manager to resolve the font
/// family name to a file path, then embeds the font via libharu.
///
/// Falls back to Helvetica (built-in PDF font) if the TrueType file
/// cannot be loaded.
///
/////////////////////////////////////////////////////////////////////////////
HPDF_Font cTUIPrintout::GetOrLoadFont(const std::string& descriptor)
{
    if (!mPdf || descriptor.empty())
    {
        return nullptr;
    }

    // Check cache first
    auto it = mFontCache.find(descriptor);
    if (it != mFontCache.end())
    {
        return it->second;
    }

    // Parse descriptor to get font properties
    std::string family = TUIFontUtils::GetFamilyFromDescriptor(descriptor);
    bool bold = TUIFontUtils::IsBoldInDescriptor(descriptor);
    bool italic = TUIFontUtils::IsItalicInDescriptor(descriptor);

    // Try to resolve font to a file path via font manager
    HPDF_Font pdfFont = nullptr;

    if (mFontManager)
    {
        // Search available fonts by family for best bold/italic match
        std::vector<sTUIAvailableFont> familyFonts = mFontManager->GetFontsByFamily(family);

        // If no exact family match, try all available fonts
        if (familyFonts.empty())
        {
            familyFonts = mFontManager->GetAvailableFonts();
        }

        // Find best match by bold/italic flags
        const sTUIAvailableFont* bestMatch = nullptr;
        int bestScore = -1;

        for (const auto& font : familyFonts)
        {
            int score = 0;

            // Exact bold/italic match is preferred
            if (font.bold == bold)
            {
                score += 10;
            }
            if (font.italic == italic)
            {
                score += 10;
            }

            // Prefer fonts with file paths (system fonts)
            if (!font.fullName.empty() && font.fullName.find('/') != std::string::npos)
            {
                score += 5;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestMatch = &font;
            }
        }

        if (bestMatch && !bestMatch->fullName.empty())
        {
            // fullName contains the file path to the TrueType font
            std::string fontPath = bestMatch->fullName;

            // Check if the file exists and is a TTF/OTF file
            if (std::filesystem::exists(fontPath))
            {
                // Load TrueType font into PDF with embedding enabled
                const char* fontName = HPDF_LoadTTFontFromFile(mPdf, fontPath.c_str(), HPDF_TRUE);
                if (fontName)
                {
                    pdfFont = HPDF_GetFont(mPdf, fontName, "UTF-8");
                }
            }
        }
    }

    // Fall back to built-in PDF font if TrueType loading failed
    if (!pdfFont)
    {
        // Map common font families to built-in PDF fonts
        const char* fallbackName = "Helvetica";

        if (family.find("Courier") != std::string::npos || family.find("Mono") != std::string::npos)
        {
            if (bold && italic)
            {
                fallbackName = "Courier-BoldOblique";
            }
            else if (bold)
            {
                fallbackName = "Courier-Bold";
            }
            else if (italic)
            {
                fallbackName = "Courier-Oblique";
            }
            else
            {
                fallbackName = "Courier";
            }
        }
        else if (family.find("Times") != std::string::npos || family.find("Serif") != std::string::npos)
        {
            if (bold && italic)
            {
                fallbackName = "Times-BoldItalic";
            }
            else if (bold)
            {
                fallbackName = "Times-Bold";
            }
            else if (italic)
            {
                fallbackName = "Times-Italic";
            }
            else
            {
                fallbackName = "Times-Roman";
            }
        }
        else
        {
            // Sans-serif (Arial, Helvetica, etc.)
            if (bold && italic)
            {
                fallbackName = "Helvetica-BoldOblique";
            }
            else if (bold)
            {
                fallbackName = "Helvetica-Bold";
            }
            else if (italic)
            {
                fallbackName = "Helvetica-Oblique";
            }
            else
            {
                fallbackName = "Helvetica";
            }
        }

        pdfFont = HPDF_GetFont(mPdf, fallbackName, nullptr);
    }

    // Cache the result (even nullptr to avoid repeated failed loads)
    mFontCache[descriptor] = pdfFont;

    return pdfFont;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filepath [in] - Path to the PDF file to open
///
/// @return nothing
///
/// @brief
/// Launches the system PDF viewer using the platform-appropriate command.
/// Runs asynchronously (& on Unix, start on Windows) so the TUI app
/// continues running.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::LaunchPDFViewer(const std::string& filepath)
{
#ifdef _WIN32
    std::string cmd = "start \"\" \"" + filepath + "\"";
#elif defined(__APPLE__)
    std::string cmd = "open \"" + filepath + "\" &";
#else
    std::string cmd = "xdg-open \"" + filepath + "\" &";
#endif
    [[maybe_unused]] int ret = std::system(cmd.c_str());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return Temp file path for PDF preview
///
/// @brief
/// Builds the temp file path for print preview. Extracts the document
/// base name and creates a path like /tmp/WordTsar-myfile-preview.pdf.
///
/////////////////////////////////////////////////////////////////////////////
std::string cTUIPrintout::BuildPreviewPath(void) const
{
    // Extract document base name (e.g., "myfile.ws" -> "myfile")
    std::filesystem::path docPath(mEditor->mFileName);
    std::string baseName = docPath.stem().string();
    if (baseName.empty())
    {
        baseName = "Untitled";
    }

    // Build temp path: /tmp/WordTsar-myfile-preview.pdf
    std::filesystem::path tempPath = std::filesystem::temp_directory_path()
        / ("WordTsar-" + baseName + "-preview.pdf");

    return tempPath.string();
}
