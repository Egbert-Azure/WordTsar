//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
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

#ifndef GUI_PDFPRINTOUT_H
#define GUI_PDFPRINTOUT_H

#include <string>
#include <map>

#include <CoreGraphics/CoreGraphics.h>
#include <CoreText/CoreText.h>

#include "src/core/layout/layoutstructs.h"
#include "src/core/layout/headerfootermanager.h"

// Forward declarations
class cEditorCtrl;
class cLayoutBase;
class cDocument;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cGUIPDFPrintout
///
/// @brief
/// GUI PDF generation engine. Takes the fully laid-out document from the
/// editor's layout and writes a PDF file via Quartz (CGPDFContext), with
/// glyphs shaped and drawn through Core Text -- exactly the same technique
/// as the TUI's cTUIPrintout (src/tui/print/tuiprintout.cpp), which this
/// class closely mirrors line-for-line on the rendering side.
///
/// Replaces the old QPrintPreviewDialog/QPrinter-based renderer: that path
/// recomputed a font-size scale factor from QPrinter::resolution() on every
/// paint pass, and Qt invokes that pass twice per preview-then-print session
/// (once for the on-screen preview, once more for the committed print) with
/// no guarantee the two agree -- confirmed to corrupt real printed pages,
/// not just the interactive preview window. Generating one real PDF up
/// front and showing/printing that removes the resolution-dependent
/// rescaling step entirely: PDF coordinates are native points, not tied to
/// any device's DPI.
///
/// Font resolution differs from the TUI version: GUI already has working,
/// correct font resolution via Qt/FontUtils (used for on-screen rendering
/// and, previously, for print) -- this class reuses that instead of the
/// TUI's file-path-based TrueType loader, asking Core Text for the same
/// family name Qt already resolved.
///
/// @see cTUIPrintout (src/tui/print/tuiprintout.cpp) -- the reference
///      implementation this mirrors; kept as a separate class rather than
///      shared, since the GUI and TUI define distinct cLayout subclasses
///      and are never linked into the same binary.
///
/////////////////////////////////////////////////////////////////////////////
class cGUIPDFPrintout
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    explicit cGUIPDFPrintout(cEditorCtrl* editor);
    ~cGUIPDFPrintout(void);

    // Generate PDF to the specified file path. Handles the SHOW_NONE
    // relayout/restore dance around generation, same as the old
    // printDocument() did.
    bool GeneratePDF(const std::string& filepath);

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
                        double pageHeightPt, bool isLastSegmentOfLine = false);

    // Resolve a segment's font descriptor to a sized Core Text font, via
    // Qt/FontUtils' existing descriptor parsing (same family match Qt uses
    // on screen) rather than a separate font-matching implementation.
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

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    cEditorCtrl* mEditor;                  // Editor (not owned)
    cLayoutBase* mLayout;                  // Layout from editor (not owned)
    cDocument* mDocument;                  // Document from layout (not owned)

    // Quartz PDF context, one per GeneratePDF() call
    CGContextRef mPdfContext;

    // Font cache: maps font descriptor string (already encodes point size)
    // to a retained, sized CTFontRef. Released via ClearFontCache().
    std::map<std::string, CTFontRef> mFontCache;
};

#endif // GUI_PDFPRINTOUT_H
