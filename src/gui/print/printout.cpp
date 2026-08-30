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
 * @class cPrintout
 *
 * @brief Print and print-preview engine for the WordTsar document.
 *
 * Implements the cPrintout class, which renders the laid-out document to a
 * QPrinter for both printing and print-preview.
 *
 * @section printout_rendering Page Rendering
 * - Iterates over layout pages, drawing each page's lines and segments
 * - Applies font formatting per segment: bold, italic, underline, strikethrough,
 *   superscript, subscript, and color via QPainter and QFont
 * - Handles page breaks between logical pages
 * - Scales coordinates from twips to printer resolution
 *
 * @section printout_preview Print Preview
 * Uses QPrintPreviewDialog for interactive preview before sending output
 * to the printer. The preview renders pages using the same drawing code
 * as actual printing for accurate WYSIWYG representation.
 *
 * @section printout_setup Page Setup
 * - Orientation: portrait and landscape support
 * - Paper size: derives from document's page layout settings
 * - Margins: applied from the document's margin configuration
 * - Progress dialog: shows page-by-page progress during multi-page prints
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cPrintout Print engine class
 * @see cEditorCtrl Editor providing document and layout access
 * @see cLayout GUI layout engine providing page/line data
 * @see FontUtils Font descriptor to QFont conversion for rendering
 */

#include <QProgressDialog>

#include "printout.h"
#include "src/gui/editor/editorctrl.h"
#include "src/gui/layout/layout.h"
#include "src/gui/utils/fontutils.h"

#include "src/core/include/config.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor [in] - Editor control containing layout and document
/// @param  title [in] - Window title for print preview
///
/// @return nothing
///
/// @brief
/// Constructor for print engine. Stores reference to editor and layout.
///
/////////////////////////////////////////////////////////////////////////////
cPrintout::cPrintout(cEditorCtrl* editor, const QString &title)
        : QWidget(editor)
{
    UNUSED_ARGUMENT(title);
    mEditor = editor;
    mLayout = editor->GetLayout();
    mScale = 1.0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for print engine.
///
/////////////////////////////////////////////////////////////////////////////
cPrintout::~cPrintout(void)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Opens print preview dialog and displays document for preview before
/// printing. User can adjust settings and preview all pages.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::PrintPreview(void)
{
    QPrinter printer(QPrinter::HighResolution);
    QPageSize pagesize(QPageSize::Letter);
    printer.setPageSize(pagesize);
    printer.setFullPage(true);
    printer.setFromTo(1, mLayout->GetNumberOfPages());

    // Default to the printer's duplex setting so two-sided printing is
    // honoured and reachable under the print dialog's Properties page.
    printer.setDuplex(QPrinter::DuplexAuto);

    // Set orientation based on document's .PR command
    if (mLayout->GetLandscapeMode())
    {
        printer.setPageOrientation(QPageLayout::Landscape);
    }
    else
    {
        printer.setPageOrientation(QPageLayout::Portrait);
    }

    QPrintPreviewDialog preview(&printer, mEditor);
    QSize size = mEditor->size();
    preview.resize(size);
    connect(&preview, &QPrintPreviewDialog::paintRequested,
            this, &cPrintout::printDocument);
    preview.exec();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  printer [in] - QPrinter to print to
///
/// @return nothing
///
/// @brief
/// Prints entire document to printer. Iterates through all pages in
/// layout and prints each one.
///
/// Printing never shows control codes or dot commands. We temporarily
/// set ShowControl to SHOW_NONE, relayout, print, then restore original
/// state and relayout again.
///
/// Uses QPainter coordinate transformation (painter.scale) to work
/// directly in twips, avoiding manual pixel conversions.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::printDocument(QPrinter *printer)
{
    // Remember current show control state
    eShowControl savedShowControl = mLayout->GetShowControl();

    // Set to SHOW_NONE for printing (never show dot commands or control codes)
    mLayout->SetShowControl(SHOW_NONE);
    mLayout->SetActiveParagraph(-1);  // Hide all dot commands including caret's

    // Relayout document without dot commands
    cDocument* doc = mLayout->GetDocument();
    if (doc != nullptr)
    {
        mLayout->LayoutDocument(doc);
    }

    int from = printer->fromPage();
    int to = printer->toPage();

    if (from == 0 && to == 0)
    {
        to = mLayout->GetNumberOfPages();
        from = 1;
        printer->setFromTo(from, to);
    }

    QProgressDialog progress(tr("Preparing Pages..."), tr("&Cancel"), from, to, mEditor);
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setWindowTitle(tr("Print Preview"));
    progress.setMinimum(printer->fromPage() - 1);
    progress.setMaximum(printer->toPage());

    QPainter painter;
    painter.begin(printer);
    painter.setPen(Qt::blue);

    // Get paper dimensions from printer (in millimeters)
    QRectF prect = printer->pageLayout().fullRect(QPageLayout::Millimeter);
    QSizeF psize;
    psize.setHeight(prect.height());
    psize.setWidth(prect.width());

    // Convert paper size to twips for coordinate system
    int printerwidth = psize.width() * TWIPSPERMM;
    int printerheight = psize.height() * TWIPSPERMM;

    // Set up coordinate transformation to work in twips
    // Use setWindow to map twips coordinates directly to printer pixels
    // This is the same approach as the old printout.cpp (line 81)
    painter.setWindow(0, 0, printerwidth, printerheight);

    // Calculate scale factor for font size conversion (pixels per twip)
    // When using setWindow(), font sizes need to be converted from device units
    // to logical coordinate system units using this scale factor
    int screenwidth = painter.device()->width();
    mScale = (double)screenwidth / (double)printerwidth;

    bool firstPage = true;

    for (int page = from; page <= to; ++page)
    {
        if (!firstPage)
        {
            printer->newPage();
        }

        QApplication::processEvents();
        if (progress.wasCanceled())
        {
            break;
        }

        printPage(page, &painter);
        progress.setValue(page);
        firstPage = false;
    }

    painter.end();

    // Restore original show control state
    mLayout->SetShowControl(savedShowControl);

    // Restore active paragraph from current caret position and relayout
    if (doc != nullptr)
    {
        PARAGRAPH_T para = doc->GetParagraphFromPosition(doc->GetPosition());
        mLayout->SetActiveParagraph(para);
        mLayout->LayoutDocument(doc);
    }

    // Trigger editor refresh to display restored layout
    if (mEditor != nullptr)
    {
        mEditor->update();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  pageNum [in] - Page number to print (1-based)
/// @param  painter [in] - QPainter to draw with
///
/// @return nothing
///
/// @brief
/// Prints a single page. Draws headers, body text, and footers using
/// absolute page coordinates (pagex, pagey).
///
/// The layout has already been reformatted with SHOW_NONE, so dot commands
/// are not present in mParagraphLayout. Headers and footers are always shown.
///
/// No coordinate conversions needed - painter is already transformed
/// to twips coordinate system.
///
/// Phase 4: Gets document from layout and passes to drawing functions for
/// on-demand grapheme fetching.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::printPage(int pageNum, QPainter *painter)
{
    // this is bizarre, but setting any pen color other than black here makes print preview distplay properly
    // if I set to black, no text is displayed.
    /// @TODO Figure out why this is happening
    // painter->setPen(Qt::blue);

    // Get document from layout for on-demand grapheme fetching
    cDocument* document = mLayout->GetDocument();

    // Draw headers and footers for this page
    DrawHeadersFooters(pageNum, document, painter);

    // Iterate through all paragraphs and draw body text for this page
    // (dot commands already filtered out by SHOW_NONE layout)
    for (PARAGRAPH_T paraNum = 0; paraNum < mLayout->GetNumberOfParagraphs(); paraNum++)
    {
        const sParagraphLayout* para = mLayout->GetParagraphLayout(paraNum);
        if (!para)
        {
            continue;
        }

        for (const auto &line : para->lines)
        {
            if (line.pagenumber == pageNum)
            {
                DrawLine(line, document, painter);
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in] - Line to draw
/// @param  document [in] - Document to fetch graphemes from
/// @param  painter [in] - QPainter to draw with
///
/// @return nothing
///
/// @brief
/// Draws a single line at its absolute page position. Uses pagex/pagey
/// directly since painter is already in twips coordinate system.
///
/// Iterates through all segments in the line and draws each.
///
/// Phase 4: Passes document pointer to DrawSegment for on-demand fetching.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::DrawLine(const sLineLayout &line, cDocument* document, QPainter *painter)
{
    // Draw each segment in the line
    for (const auto &segment : line.segments)
    {
        DrawSegment(segment, document, line.pagex, line.pagey, painter);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  hfLine [in] - Header/footer line with pre-rendered graphemes
/// @param  painter [in] - QPainter to draw with
///
/// @return nothing
///
/// @brief
/// Draws a header or footer line using its stored graphemes.
/// Header/footer text is not in the document model, so graphemes
/// are stored separately in the sHeaderFooterLine struct.
///
/// Segments have pre-applied fonts from LayoutHeaderFooterText, so
/// we just need to iterate through segments and draw their graphemes.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::DrawHeaderFooterLine(const sHeaderFooterLine &hfLine, QPainter *painter)
{
    const sLineLayout& line = hfLine.line;

    if (line.segments.empty() || hfLine.graphemes.empty())
    {
        return;
    }

    // Track current grapheme index across segments
    size_t graphemeIndex = 0;

    // Calculate segment base X (segments store relative positions)
    COORD_T segmentBaseX = 0;

    // Draw each segment with its font
    for (const auto& segment : line.segments)
    {
        // Get font from segment
        QFont font;
        if (!segment.font.empty())
        {
            font = FontUtils::FontFromDescriptor(segment.font);
            qreal scaledSize = font.pointSizeF() / mScale;
            font.setPointSizeF(scaledSize);
        }
        painter->setFont(font);

        // Set text color from segment (default sentinel prints as black)
        QColor textColor;
        if (segment.textcolor.IsDefault())
        {
            textColor = QColor(0, 0, 0);
        }
        else
        {
            textColor = QColor(
                segment.textcolor.red,
                segment.textcolor.green,
                segment.textcolor.blue,
                segment.textcolor.alpha
            );
        }
        painter->setPen(textColor);

        // Draw each grapheme in this segment
        for (size_t i = 0; i < segment.position.size() && graphemeIndex < hfLine.graphemes.size(); ++i)
        {
            const std::string& grapheme = hfLine.graphemes[graphemeIndex];

            // Skip empty graphemes (but still count them)
            if (!grapheme.empty())
            {
                COORD_T glyphX = line.pagex + segmentBaseX + segment.position[i];
                COORD_T glyphY = line.pagey;

                // Adjust Y for subscript/superscript
                if (segment.isSubscript)
                {
                    glyphY += segment.segmentheight * 0.3;
                }
                else if (segment.isSuperscript)
                {
                    glyphY -= segment.segmentheight * 0.3;
                }

                QString glyph = QString::fromStdString(grapheme);
                painter->drawText(QPointF(glyphX, glyphY), glyph);
            }

            graphemeIndex++;
        }

        // Update base X for next segment (segment positions are relative)
        segmentBaseX += segment.totalWidth;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  segment [in] - Segment containing positions and formatting
/// @param  document [in] - Document to fetch graphemes from
/// @param  x [in] - X position of line in twips (line.pagex)
/// @param  y [in] - Y position of line in twips (line.pagey)
/// @param  painter [in] - QPainter to draw with
///
/// @return nothing
///
/// @brief
/// Draws a segment with its glyphs at pre-calculated positions.
/// Sets font and color from segment, then draws each glyph.
///
/// Phase 4: Fetches graphemes on-demand from document using GetGraphemes().
///
/// All coordinates are in twips - no conversions needed.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::DrawSegment(const sSegmentLayout &segment, cDocument* document, COORD_T x, COORD_T y,
                             QPainter *painter)
{
    // Phase 4: Fetch graphemes on-demand from document
    if (!document || segment.position.empty() || segment.GetGraphemeCount() == 0)
    {
        return;
    }

    // Fetch graphemes from document (MARKER_CHAR bytes converted at render time)
    std::vector<std::string> graphemes;
    segment.GetGraphemes(document, graphemes);

    if (graphemes.empty())
    {
        return;
    }

    // Set font from segment
    if (!segment.font.empty())
    {
        QFont font = FontUtils::FontFromDescriptor(segment.font);

        // When using setWindow(), font sizes need to be converted from device units
        // to logical coordinate system units. The scale factor (pixels per twip)
        // determines the conversion: logicalSize = deviceSize / scale
        qreal pointSize = font.pointSizeF();
        qreal scaledSize = pointSize / mScale;
        font.setPointSizeF(scaledSize);

        painter->setFont(font);
    }

    // Set text color from segment (default sentinel prints as black)
    QColor textColor;
    if (segment.textcolor.IsDefault())
    {
        textColor = QColor(0, 0, 0);
    }
    else
    {
        textColor = QColor(
            segment.textcolor.red,
            segment.textcolor.green,
            segment.textcolor.blue,
            segment.textcolor.alpha
        );
    }
    painter->setPen(textColor);

    // Draw each glyph at its position (all in twips)
    for (size_t i = 0; i < graphemes.size(); ++i)
    {
        std::string displayGrapheme = graphemes[i];

        // Handle MARKER_CHAR (control codes stored in document)
        if (!graphemes[i].empty() && graphemes[i][0] == MARKER_CHAR)
        {
            // Calculate document position
            POSITION_T paragraphStart = 0;
            POSITION_T paragraphEnd = 0;
            document->GetParagraphStartandEnd(segment.paragraph, paragraphStart, paragraphEnd);
            POSITION_T docPos = paragraphStart + segment.startPosition + i;

            eModifiers controlType = document->GetControlChar(docPos);
            if (controlType == STYLE_VARIABLE)
            {
                // Expand variable using layout method that has page context
                eVariableType varType = document->GetVariable(docPos);
                displayGrapheme = mLayout->GetVariableExpansion(varType);
            }
            else
            {
                // Skip other control codes in print output
                continue;
            }
        }

        // Get X position: add line's left margin to relative segment position
        COORD_T glyphX = x + segment.position[i];

        // Adjust Y for subscript/superscript (in twips)
        COORD_T glyphY = y;
        if (segment.isSubscript)
        {
            // Subscript: move down by 0.3 of segment height
            glyphY += segment.segmentheight * 0.3;
        }
        else if (segment.isSuperscript)
        {
            // Superscript: move up by 0.3 of segment height
            glyphY -= segment.segmentheight * 0.3;
        }

        // Draw the glyph at baseline (Y coordinate is baseline in Qt, not top)
        QString glyph = QString::fromStdString(displayGrapheme);
        painter->drawText(QPointF(glyphX, glyphY + segment.segmentheight), glyph);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  pageNum [in] - Page number to draw headers/footers for (1-based)
/// @param  document [in] - Document to fetch graphemes from
/// @param  painter [in] - QPainter to draw with
///
/// @return nothing
///
/// @brief
/// Draws headers and footers for a specific page during printing.
///
/// Retrieves headers and footers for the specified page from the layout
/// engine and renders them at their calculated positions. Headers appear
/// at the top of the page in the header margin area, footers at the bottom
/// in the footer margin area.
///
/// Uses absolute page coordinates (pagex, pagey) just like body text.
/// Printing never shows control codes or dot commands, but always shows
/// headers and footers.
///
/// Phase 4: Passes document pointer to DrawLine for on-demand fetching.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::DrawHeadersFooters(int pageNum, [[maybe_unused]] cDocument* document, QPainter *painter)
{
    if (!mLayout)
    {
        return;
    }

    // Get headers and footers for this page
    const auto& allHeaders = mLayout->GetPageHeaders();
    const auto& allFooters = mLayout->GetPageFooters();

    // Draw headers if present for this page
    auto headerIt = allHeaders.find(pageNum);
    if (headerIt != allHeaders.end())
    {
        for (const auto& hfLine : headerIt->second)
        {
            // Draw using stored graphemes (header text not in document)
            DrawHeaderFooterLine(hfLine, painter);
        }
    }

    // Draw footers if present for this page
    auto footerIt = allFooters.find(pageNum);
    if (footerIt != allFooters.end())
    {
        for (const auto& hfLine : footerIt->second)
        {
            // Draw using stored graphemes (footer text not in document)
            DrawHeaderFooterLine(hfLine, painter);
        }
    }
}
