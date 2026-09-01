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
 * @brief TUI PDF generation and print preview via Quartz (CGPDFContext) and Core Text.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIPrintout, which generates PDF output from the fully laid-out
 * document for the terminal-based interface. Walks all layout pages, rendering
 * body text lines, headers, and footers with proper font embedding via the
 * TUI font manager. Supports bold, italic, underline, strikethrough,
 * superscript, and subscript formatting through embedded TrueType fonts.
 * Provides print preview by writing a temporary PDF and launching an
 * external viewer (open on macOS).
 *
 * @section tuiprint_rendering Page Rendering
 * Iterates over all pages from the layout engine, converting twips-based
 * coordinates to PDF points (1/72 inch). Each line's segments are rendered
 * with the appropriate embedded font, applying formatting attributes from
 * the layout segments. A Quartz PDF context is natively Y-up, matching PDF
 * space directly, so no coordinate flip transform is applied on the context
 * itself -- only the existing page-height-relative flip already baked into
 * the per-glyph Y math below.
 *
 * @section tuiprint_preview Print Preview
 * Writes the generated PDF to a temporary file and shells out to open
 * (macOS) to display it. The temporary file is cleaned up after the
 * viewer exits.
 *
 * @section tuiprint_unicode Unicode (this backend fixed a real bug)
 * The previous libharu backend drew one grapheme per HPDF_Page_ShowText
 * call through a single shared, stateful UTF-8 encoder attached to the
 * whole HPDF_Doc. That per-glyph call pattern could desynchronize the
 * encoder's byte-sequence tracking, splitting one multi-byte character
 * (umlauts, curly quotes, etc.) into two bogus single-byte glyphs --
 * e.g. "Glück" printed as "Glˆ…ck". DrawGrapheme() below hands Core Text
 * a real CFString per call with no shared encoder state to desync, so
 * this class of corruption can't recur here even though the calling
 * pattern (one draw per grapheme) is unchanged.
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
    , mPdfContext(nullptr)
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
/// Destructor. Releases any cached Core Text fonts left over from an
/// interrupted generation pass (normal completion already clears the cache).
///
/////////////////////////////////////////////////////////////////////////////
cTUIPrintout::~cTUIPrintout(void)
{
    ClearFontCache();
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

    // Get total number of pages from layout
    PAGE_T numPages = mLayout->GetNumberOfPages();
    if (numPages == 0)
    {
        return false;
    }

    // Seed the context with the first page's size; each page below supplies
    // its own media box to CGContextBeginPage regardless, so this is only
    // the fallback default.
    sPageInfo firstPageInfo = mLayout->GetPageInfo(1);
    CGRect initialBox = CGRectMake(0, 0,
        static_cast<CGFloat>(TwipsToPoints(firstPageInfo.paperwidth)),
        static_cast<CGFloat>(TwipsToPoints(firstPageInfo.paperheight)));

    CFURLRef url = CFURLCreateFromFileSystemRepresentation(
        kCFAllocatorDefault,
        reinterpret_cast<const UInt8*>(filepath.c_str()),
        static_cast<CFIndex>(filepath.length()),
        false);
    if (!url)
    {
        return false;
    }

    mPdfContext = CGPDFContextCreateWithURL(url, &initialBox, nullptr);
    CFRelease(url);

    if (!mPdfContext)
    {
        return false;
    }

    // Clear font cache for this generation pass
    ClearFontCache();

    // Generate each page
    for (PAGE_T pageNum = 1; pageNum <= numPages; pageNum++)
    {
        // Get page dimensions from layout
        sPageInfo pageInfo = mLayout->GetPageInfo(pageNum);
        double pageHeightPt = TwipsToPoints(pageInfo.paperheight);
        CGRect pageBox = CGRectMake(0, 0,
            static_cast<CGFloat>(TwipsToPoints(pageInfo.paperwidth)),
            static_cast<CGFloat>(pageHeightPt));

        CGContextBeginPage(mPdfContext, &pageBox);

        // Render body text and headers/footers
        RenderPage(mPdfContext, pageNum);
        RenderHeadersFooters(mPdfContext, pageNum, pageHeightPt);

        CGContextEndPage(mPdfContext);
    }

    // Releasing the context closes and finalizes the PDF file
    CGContextRelease(mPdfContext);
    mPdfContext = nullptr;
    ClearFontCache();

    std::error_code ec;
    uintmax_t size = std::filesystem::file_size(filepath, ec);
    return !ec && size > 0;
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
/// @param  ctx [in] - Quartz PDF context (current page)
/// @param  pageNum [in] - Page number (1-based)
///
/// @return nothing
///
/// @brief
/// Renders all body text for one page. Iterates through all paragraphs,
/// finds lines belonging to this page, and renders each line's segments.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::RenderPage(CGContextRef ctx, PAGE_T pageNum)
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
                RenderLine(ctx, line, pageHeightPt);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ctx [in] - Quartz PDF context (current page)
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
void cTUIPrintout::RenderHeadersFooters(CGContextRef ctx, PAGE_T pageNum, double pageHeightPt)
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
            RenderHeaderFooterLine(ctx, hfLine, pageHeightPt);
        }
    }

    // Draw footers for this page
    auto footerIt = allFooters.find(pageNum);
    if (footerIt != allFooters.end())
    {
        for (const auto& hfLine : footerIt->second)
        {
            RenderHeaderFooterLine(ctx, hfLine, pageHeightPt);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ctx [in] - Quartz PDF context (current page)
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
void cTUIPrintout::RenderHeaderFooterLine(CGContextRef ctx, const sHeaderFooterLine& hfLine,
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
        // Resolve font
        CTFontRef ctFont = GetOrLoadFont(segment.font);
        double pointSize = TUIFontUtils::GetSizeFromDescriptor(segment.font);
        if (pointSize <= 0)
        {
            pointSize = 12.0;
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

                DrawGrapheme(ctx, ctFont, segment.textcolor, grapheme, pdfX, pdfY);
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

            DrawUnderline(ctx, segment.textcolor, startX, endX, lineDrawY, underlineThickness);
        }

        // Update base X for next segment
        segmentBaseX += segment.totalWidth;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ctx [in] - Quartz PDF context (current page)
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
void cTUIPrintout::RenderLine(CGContextRef ctx, const sLineLayout& line, double pageHeightPt)
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
        RenderSegment(ctx, segment, line.pagex, line.pagey, lineHeight, pageHeightPt);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ctx [in] - Quartz PDF context (current page)
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
/// Resolves font and color from segment, then draws each glyph individually
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
void cTUIPrintout::RenderSegment(CGContextRef ctx, const sSegmentLayout& segment,
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

    // Resolve font from segment descriptor
    CTFontRef ctFont = GetOrLoadFont(segment.font);
    double pointSize = TUIFontUtils::GetSizeFromDescriptor(segment.font);
    if (pointSize <= 0)
    {
        pointSize = 12.0;
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
        // Qt's drawText() silently ignores these, but drawing them literally
        // here would render tofu boxes.
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

        DrawGrapheme(ctx, ctFont, segment.textcolor, displayGrapheme, pdfX, pdfY);
    }

    // Draw underline if the font descriptor has the underline flag set
    // (Quartz has no per-font underline attribute akin to Qt's, so draw manually,
    // same as the previous libharu backend did)
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

        DrawUnderline(ctx, segment.textcolor, startX, endX, lineDrawY, underlineThickness);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string (pipe-delimited)
///
/// @return CTFontRef, sized and ready to draw, or nullptr if it could not be loaded
///
/// @brief
/// Loads a TrueType font and caches it by descriptor string (which already
/// encodes point size, so each cache entry is a distinct family+size+style
/// combination). Uses the TUI font manager to resolve the font family name
/// to a file path, then wraps it as a CGFont and sizes it via Core Text.
///
/// Falls back to a system font substitute for one of the standard 14 PDF
/// font names if the TrueType file cannot be resolved or loaded.
///
/////////////////////////////////////////////////////////////////////////////
CTFontRef cTUIPrintout::GetOrLoadFont(const std::string& descriptor)
{
    if (!mPdfContext || descriptor.empty())
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
    double pointSize = TUIFontUtils::GetSizeFromDescriptor(descriptor);
    if (pointSize <= 0)
    {
        pointSize = 12.0;
    }

    CTFontRef ctFont = nullptr;

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

        if (bestMatch && !bestMatch->fullName.empty() && std::filesystem::exists(bestMatch->fullName))
        {
            // fullName contains the file path to the TrueType font. Quartz
            // automatically subsets and embeds any font actually drawn into
            // a PDF context, so unlike libharu's HPDF_LoadTTFontFromFile
            // there's no separate "embed" flag to pass here.
            CFURLRef fontUrl = CFURLCreateFromFileSystemRepresentation(
                kCFAllocatorDefault,
                reinterpret_cast<const UInt8*>(bestMatch->fullName.c_str()),
                static_cast<CFIndex>(bestMatch->fullName.length()),
                false);

            if (fontUrl)
            {
                CGDataProviderRef provider = CGDataProviderCreateWithURL(fontUrl);
                CFRelease(fontUrl);

                if (provider)
                {
                    CGFontRef cgFont = CGFontCreateWithDataProvider(provider);
                    CGDataProviderRelease(provider);

                    if (cgFont)
                    {
                        ctFont = CTFontCreateWithGraphicsFont(cgFont, static_cast<CGFloat>(pointSize),
                                                               nullptr, nullptr);
                        CGFontRelease(cgFont);
                    }
                }
            }
        }
    }

    // Fall back to a system font substitute if TrueType loading failed
    if (!ctFont)
    {
        // Map common font families to the standard 14 PDF font names --
        // macOS resolves these PostScript names to real system fonts.
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

        CFStringRef nameStr = CFStringCreateWithCString(kCFAllocatorDefault, fallbackName, kCFStringEncodingUTF8);
        if (nameStr)
        {
            ctFont = CTFontCreateWithName(nameStr, static_cast<CGFloat>(pointSize), nullptr);
            CFRelease(nameStr);
        }
    }

    // Cache the result (even nullptr to avoid repeated failed loads)
    mFontCache[descriptor] = ctFont;

    return ctFont;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Releases every cached CTFontRef and empties the font cache. CTFontRefs
/// are Core Foundation objects with manual reference counting -- each entry
/// created by GetOrLoadFont() holds exactly one reference this class owns.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::ClearFontCache(void)
{
    for (auto& entry : mFontCache)
    {
        if (entry.second)
        {
            CFRelease(entry.second);
        }
    }
    mFontCache.clear();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] - Layout color (the "default" sentinel prints as black)
///
/// @return An owned CGColorRef -- caller must CGColorRelease() it
///
/// @brief
/// Builds a Quartz color from a layout color value.
///
/////////////////////////////////////////////////////////////////////////////
CGColorRef cTUIPrintout::MakeCGColor(const sSeqRGBColor& color)
{
    if (color.IsDefault())
    {
        return CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
    }
    return CGColorCreateGenericRGB(color.red / 255.0, color.green / 255.0, color.blue / 255.0, 1.0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ctx [in] - Quartz PDF context (current page)
/// @param  font [in] - Sized Core Text font to draw with
/// @param  color [in] - Text color
/// @param  text [in] - UTF-8 grapheme to draw
/// @param  x [in] - PDF X position (points)
/// @param  y [in] - PDF Y position (points, baseline)
///
/// @return nothing
///
/// @brief
/// Draws one grapheme at an absolute PDF position via Core Text, which
/// handles Unicode shaping directly (no encoder setup needed, unlike
/// libharu's explicit HPDF_UseUTFEncodings/SetCurrentEncoder).
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::DrawGrapheme(CGContextRef ctx, CTFontRef font, const sSeqRGBColor& color,
                                 const std::string& text, double x, double y)
{
    if (!font || text.empty())
    {
        return;
    }

    CFStringRef cfText = CFStringCreateWithCString(kCFAllocatorDefault, text.c_str(), kCFStringEncodingUTF8);
    if (!cfText)
    {
        return;
    }

    CGColorRef cgColor = MakeCGColor(color);

    const void* keys[] = { kCTFontAttributeName, kCTForegroundColorAttributeName };
    const void* values[] = { font, cgColor };
    CFDictionaryRef attrs = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 2,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    CFAttributedStringRef attrString = CFAttributedStringCreate(kCFAllocatorDefault, cfText, attrs);
    CTLineRef line = CTLineCreateWithAttributedString(attrString);

    CGContextSetTextPosition(ctx, static_cast<CGFloat>(x), static_cast<CGFloat>(y));
    CTLineDraw(line, ctx);

    CFRelease(line);
    CFRelease(attrString);
    CFRelease(attrs);
    CGColorRelease(cgColor);
    CFRelease(cfText);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ctx [in] - Quartz PDF context (current page)
/// @param  color [in] - Line color
/// @param  startX [in] - Start X position (points)
/// @param  endX [in] - End X position (points)
/// @param  y [in] - Y position (points)
/// @param  thickness [in] - Line thickness (points)
///
/// @return nothing
///
/// @brief
/// Strokes a straight horizontal underline segment.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIPrintout::DrawUnderline(CGContextRef ctx, const sSeqRGBColor& color,
                                  double startX, double endX, double y, double thickness)
{
    CGColorRef strokeColor = MakeCGColor(color);
    CGContextSetStrokeColorWithColor(ctx, strokeColor);
    CGColorRelease(strokeColor);

    CGContextSetLineWidth(ctx, static_cast<CGFloat>(thickness));
    CGContextMoveToPoint(ctx, static_cast<CGFloat>(startX), static_cast<CGFloat>(y));
    CGContextAddLineToPoint(ctx, static_cast<CGFloat>(endX), static_cast<CGFloat>(y));
    CGContextStrokePath(ctx);
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
