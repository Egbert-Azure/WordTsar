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

#ifndef TUI_PRINTOUT_H
#define TUI_PRINTOUT_H

#include <string>
#include <map>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include "src/core/layout/layoutstructs.h"
#include "src/core/layout/headerfootermanager.h"

// Forward declarations
class cEditorBase;
class cLayoutBase;
class cDocument;
class cTUIFontManager;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTUIPrintout
///
/// @brief
/// TUI PDF generation engine. Takes the fully laid-out document from
/// the editor's layout and writes a PDF file via Quartz (CGPDFContext),
/// with glyphs shaped and drawn through Core Text. Each page, line, and
/// segment is converted from twips to PDF points and rendered with
/// embedded TrueType fonts -- Quartz embeds/subsets any font actually
/// drawn into a PDF context automatically, so no explicit "embed" step
/// is needed the way libharu required one.
///
/// Used for both Print Preview (temp file + viewer) and Print (save to
/// user-specified path).
///
/////////////////////////////////////////////////////////////////////////////
class cTUIPrintout
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    explicit cTUIPrintout(cEditorBase* editor);
    ~cTUIPrintout(void);

    // Generate PDF to the specified file path
    bool GeneratePDF(const std::string& filepath);

    // Print Preview: generate to temp file and launch system PDF viewer
    void PrintPreview(void);

    // Check if a graphical desktop is available for launching a PDF viewer
    static bool HasDisplayEnvironment(void);

private:
    // Coordinate conversion: twips to PDF points (1 point = 20 twips)
    double TwipsToPoints(COORD_T twips) const;

    // Render a single page
    void RenderPage(CGContextRef ctx, PAGE_T pageNum);

    // Render headers and footers for a page
    void RenderHeadersFooters(CGContextRef ctx, PAGE_T pageNum, double pageHeightPt);

    // Render a header/footer line
    void RenderHeaderFooterLine(CGContextRef ctx, const sHeaderFooterLine& hfLine,
                                 double pageHeightPt);

    // Render a single line
    void RenderLine(CGContextRef ctx, const sLineLayout& line, double pageHeightPt);

    // Render a single segment (lineHeight = max segment height in the line, for baseline alignment)
    void RenderSegment(CGContextRef ctx, const sSegmentLayout& segment,
                        COORD_T lineX, COORD_T lineY, COORD_T lineHeight,
                        double pageHeightPt);

    // Load and cache a sized Core Text font from its descriptor string
    CTFontRef GetOrLoadFont(const std::string& descriptor);

    // Release every cached font and empty the cache
    void ClearFontCache(void);

    // Build an owned CGColorRef from a layout color (black for the "default" sentinel)
    static CGColorRef MakeCGColor(const sSeqRGBColor& color);

    // Draw one grapheme via Core Text at an absolute PDF position
    static void DrawGrapheme(CGContextRef ctx, CTFontRef font, const sSeqRGBColor& color,
                              const std::string& text, double x, double y);

    // Stroke an underline segment
    static void DrawUnderline(CGContextRef ctx, const sSeqRGBColor& color,
                               double startX, double endX, double y, double thickness);

    // Launch the system PDF viewer
    static void LaunchPDFViewer(const std::string& filepath);

    // Build the temp file path for print preview
    std::string BuildPreviewPath(void) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    cEditorBase* mEditor;                  // Editor (not owned)
    cLayoutBase* mLayout;                  // Layout from editor (not owned)
    cDocument* mDocument;                  // Document from layout (not owned)
    cTUIFontManager* mFontManager;         // Font manager for file path resolution (not owned)

    // Quartz PDF context, one per GeneratePDF() call
    CGContextRef mPdfContext;

    // Font cache: maps font descriptor string (already encodes size) to a
    // retained, sized CTFontRef. Released via ClearFontCache().
    std::map<std::string, CTFontRef> mFontCache;
};

#endif // TUI_PRINTOUT_H
