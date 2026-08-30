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

#ifndef TUI_PRINTOUT_H
#define TUI_PRINTOUT_H

#include <string>
#include <map>

#include "hpdf.h"

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
/// the editor's layout and writes a PDF file using libharu. Each page,
/// line, and segment is converted from twips to PDF points and rendered
/// with embedded TrueType fonts.
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
    void RenderPage(HPDF_Page page, PAGE_T pageNum);

    // Render headers and footers for a page
    void RenderHeadersFooters(HPDF_Page page, PAGE_T pageNum, double pageHeightPt);

    // Render a header/footer line
    void RenderHeaderFooterLine(HPDF_Page page, const sHeaderFooterLine& hfLine,
                                 double pageHeightPt);

    // Render a single line
    void RenderLine(HPDF_Page page, const sLineLayout& line, double pageHeightPt);

    // Render a single segment (lineHeight = max segment height in the line, for baseline alignment)
    void RenderSegment(HPDF_Page page, const sSegmentLayout& segment,
                        COORD_T lineX, COORD_T lineY, COORD_T lineHeight,
                        double pageHeightPt);

    // Load and cache a font from its descriptor string
    HPDF_Font GetOrLoadFont(const std::string& descriptor);

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

    // libharu PDF document handle
    HPDF_Doc mPdf;

    // Font cache: maps font descriptor string to loaded HPDF_Font
    std::map<std::string, HPDF_Font> mFontCache;
};

#endif // TUI_PRINTOUT_H
