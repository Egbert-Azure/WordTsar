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

#include "pdfprintout.h"

#include "src/gui/editor/editorctrl.h"
#include "src/gui/utils/fontutils.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/document/document.h"
#include "src/core/include/config.h"

#include <QFont>

#include <filesystem>


/////////////////////////////////////////////////////////////////////////////
cGUIPDFPrintout::cGUIPDFPrintout(cEditorCtrl* editor)
    : mEditor(editor)
    , mLayout(nullptr)
    , mDocument(nullptr)
    , mPdfContext(nullptr)
{
    mLayout = editor->GetLayout();
    if (mLayout)
    {
        mDocument = mLayout->GetDocument();
    }
}


/////////////////////////////////////////////////////////////////////////////
cGUIPDFPrintout::~cGUIPDFPrintout(void)
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
/// Generates a complete PDF of the laid-out document. Printing never shows
/// control codes or dot commands, so this relayouts under SHOW_NONE first
/// (same dance the old QPainter-based printDocument() did), generates every
/// page, then restores the editor's own display layout.
///
/////////////////////////////////////////////////////////////////////////////
bool cGUIPDFPrintout::GeneratePDF(const std::string& filepath)
{
    if (!mLayout || !mDocument)
    {
        return false;
    }

    eShowControl savedShowControl = mLayout->GetShowControl();
    mLayout->SetShowControl(SHOW_NONE);
    mLayout->SetActiveParagraph(-1);
    mLayout->LayoutDocument(mDocument);

    bool result = false;

    PAGE_T numPages = mLayout->GetNumberOfPages();
    if (numPages > 0)
    {
        sPageInfo firstPageInfo = mLayout->GetPageInfo(1);
        CGRect initialBox = CGRectMake(0, 0,
            static_cast<CGFloat>(TwipsToPoints(firstPageInfo.paperwidth)),
            static_cast<CGFloat>(TwipsToPoints(firstPageInfo.paperheight)));

        CFURLRef url = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(filepath.c_str()),
            static_cast<CFIndex>(filepath.length()),
            false);

        if (url)
        {
            mPdfContext = CGPDFContextCreateWithURL(url, &initialBox, nullptr);
            CFRelease(url);

            if (mPdfContext)
            {
                ClearFontCache();

                for (PAGE_T pageNum = 1; pageNum <= numPages; pageNum++)
                {
                    sPageInfo pageInfo = mLayout->GetPageInfo(pageNum);
                    double pageHeightPt = TwipsToPoints(pageInfo.paperheight);
                    CGRect pageBox = CGRectMake(0, 0,
                        static_cast<CGFloat>(TwipsToPoints(pageInfo.paperwidth)),
                        static_cast<CGFloat>(pageHeightPt));

                    CGContextBeginPage(mPdfContext, &pageBox);
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
                result = !ec && size > 0;
            }
        }
    }

    mLayout->SetShowControl(savedShowControl);
    PARAGRAPH_T curPara = mDocument->GetParagraphFromPosition(mDocument->GetPosition());
    mLayout->SetActiveParagraph(curPara);
    mLayout->LayoutDocument(mDocument);

    if (mEditor)
    {
        mEditor->update();
    }

    return result;
}


/////////////////////////////////////////////////////////////////////////////
double cGUIPDFPrintout::TwipsToPoints(COORD_T twips) const
{
    return static_cast<double>(twips) / 20.0;
}


/////////////////////////////////////////////////////////////////////////////
void cGUIPDFPrintout::RenderPage(CGContextRef ctx, PAGE_T pageNum)
{
    sPageInfo pageInfo = mLayout->GetPageInfo(pageNum);
    double pageHeightPt = TwipsToPoints(pageInfo.paperheight);

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
void cGUIPDFPrintout::RenderHeadersFooters(CGContextRef ctx, PAGE_T pageNum, double pageHeightPt)
{
    if (!mLayout)
    {
        return;
    }

    const auto& allHeaders = mLayout->GetPageHeaders();
    const auto& allFooters = mLayout->GetPageFooters();

    auto headerIt = allHeaders.find(pageNum);
    if (headerIt != allHeaders.end())
    {
        for (const auto& hfLine : headerIt->second)
        {
            RenderHeaderFooterLine(ctx, hfLine, pageHeightPt);
        }
    }

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
void cGUIPDFPrintout::RenderHeaderFooterLine(CGContextRef ctx, const sHeaderFooterLine& hfLine,
                                              double pageHeightPt)
{
    const sLineLayout& line = hfLine.line;

    if (line.segments.empty() || hfLine.graphemes.empty())
    {
        return;
    }

    COORD_T lineHeight = 0;
    for (const auto& seg : line.segments)
    {
        if (seg.segmentheight > lineHeight)
        {
            lineHeight = seg.segmentheight;
        }
    }

    size_t graphemeIndex = 0;
    COORD_T segmentBaseX = 0;

    for (const auto& segment : line.segments)
    {
        CTFontRef ctFont = GetOrLoadFont(segment.font);

        for (size_t i = 0; i < segment.position.size() && graphemeIndex < hfLine.graphemes.size(); ++i)
        {
            const std::string& grapheme = hfLine.graphemes[graphemeIndex];

            if (!grapheme.empty())
            {
                unsigned char firstByte = static_cast<unsigned char>(grapheme[0]);
                if (firstByte >= 0x20 && firstByte != 0x7F)
                {
                    COORD_T glyphX = line.pagex + segmentBaseX + static_cast<COORD_T>(segment.position[i]);

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
            }

            graphemeIndex++;
        }

        // Draw underline for this segment if its font descriptor has the
        // underline flag set. Core Text has no per-font underline attribute
        // akin to Qt's, so draw manually -- same as the TUI renderer.
        {
            QFont qfont = FontUtils::FontFromDescriptor(segment.font);
            if (qfont.underline() && !segment.position.empty())
            {
                double pointSize = qfont.pointSizeF();
                if (pointSize <= 0)
                {
                    pointSize = 12.0;
                }
                double underlineThickness = pointSize / 14.0;
                double underlineOffset = pointSize / 7.0;

                double startX = TwipsToPoints(line.pagex + segmentBaseX
                                              + static_cast<COORD_T>(segment.position.front()));
                double endX = TwipsToPoints(line.pagex + segmentBaseX
                                            + static_cast<COORD_T>(segment.position.back())
                                            + segment.totalWidth / static_cast<COORD_T>(segment.position.size()));

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
        }

        segmentBaseX += segment.totalWidth;
    }
}


/////////////////////////////////////////////////////////////////////////////
void cGUIPDFPrintout::RenderLine(CGContextRef ctx, const sLineLayout& line, double pageHeightPt)
{
    if (!line.isPrintable)
    {
        return;
    }

    COORD_T lineHeight = 0;
    for (const auto& seg : line.segments)
    {
        if (seg.segmentheight > lineHeight)
        {
            lineHeight = seg.segmentheight;
        }
    }

    for (const auto& segment : line.segments)
    {
        RenderSegment(ctx, segment, line.pagex, line.pagey, lineHeight, pageHeightPt);
    }
}


/////////////////////////////////////////////////////////////////////////////
void cGUIPDFPrintout::RenderSegment(CGContextRef ctx, const sSegmentLayout& segment,
                                     COORD_T lineX, COORD_T lineY, COORD_T lineHeight,
                                     double pageHeightPt)
{
    if (!mDocument || segment.position.empty() || segment.GetGraphemeCount() == 0)
    {
        return;
    }

    std::vector<std::string> graphemes;
    segment.GetGraphemes(mDocument, graphemes);

    if (graphemes.empty())
    {
        return;
    }

    CTFontRef ctFont = GetOrLoadFont(segment.font);

    for (size_t i = 0; i < graphemes.size(); ++i)
    {
        std::string displayGrapheme = graphemes[i];

        // Soft hyphen (U+00AD, real ^OE character): invisible unless word
        // wrap actually broke the line here (decided once in
        // WordWrapSegmentsIntoLines(), not re-derived here), matching the
        // on-screen renderer.
        if (graphemes[i] == "\xC2\xAD")
        {
            if (!segment.explicitHyphenAtBreak || i + 1 != graphemes.size())
            {
                continue;
            }
            displayGrapheme = "-";
        }
        else if (!graphemes[i].empty() && graphemes[i][0] == MARKER_CHAR)
        {
            POSITION_T paragraphStart = 0;
            POSITION_T paragraphEnd = 0;
            mDocument->GetParagraphStartandEnd(segment.paragraph, paragraphStart, paragraphEnd);
            POSITION_T docPos = paragraphStart + segment.startPosition + static_cast<POSITION_T>(i);

            eModifiers controlType = mDocument->GetControlChar(docPos);
            if (controlType == STYLE_VARIABLE)
            {
                eVariableType varType = mDocument->GetVariable(docPos);
                displayGrapheme = mLayout->GetVariableExpansion(varType);
            }
            else
            {
                continue;
            }
        }

        if (displayGrapheme.empty())
        {
            continue;
        }
        unsigned char firstByte = static_cast<unsigned char>(displayGrapheme[0]);
        if (firstByte < 0x20 || firstByte == 0x7F)
        {
            continue;
        }

        COORD_T glyphX = lineX + static_cast<COORD_T>(segment.position[i]);

        double pdfX = TwipsToPoints(glyphX);
        double pdfY;
        if (segment.isSubscript)
        {
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

    // Automatic hyphenation (.hy): word wrap chose a dictionary break point
    // ending this segment, but no real document character backs the glyph
    // (see WordWrapSegmentsIntoLines()) -- draw it once, right after the
    // segment's real content. totalWidth already reserves its width.
    if (segment.autoHyphen && !segment.position.empty())
    {
        COORD_T hyphenGlyphWidth = mLayout->GetTextWidth("-", segment.font);
        COORD_T glyphX = lineX + segment.position[0] + segment.totalWidth - hyphenGlyphWidth;
        double pdfX = TwipsToPoints(glyphX);
        double pdfY;
        if (segment.isSubscript)
        {
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
        DrawGrapheme(ctx, ctFont, segment.textcolor, "-", pdfX, pdfY);
    }

    // Draw underline if the font descriptor has the underline flag set.
    // Core Text has no per-font underline attribute akin to Qt's, so draw
    // manually -- same as the TUI renderer.
    {
        QFont qfont = FontUtils::FontFromDescriptor(segment.font);
        if (qfont.underline() && !segment.position.empty())
        {
            double pointSize = qfont.pointSizeF();
            if (pointSize <= 0)
            {
                pointSize = 12.0;
            }
            double underlineThickness = pointSize / 14.0;
            double underlineOffset = pointSize / 7.0;

            double startX = TwipsToPoints(lineX + static_cast<COORD_T>(segment.position.front()));
            double endX = TwipsToPoints(lineX + static_cast<COORD_T>(segment.position.back())
                                        + segment.totalWidth / static_cast<COORD_T>(segment.position.size()));

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
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string (pipe-delimited)
///
/// @return CTFontRef, sized and ready to draw, or nullptr if it could not be loaded
///
/// @brief
/// Resolves a font descriptor the same way the GUI already does for on-screen
/// rendering -- via FontUtils::FontFromDescriptor(), which is what has been
/// correctly picking the right installed font family all along (the print
/// bug was never about wrong fonts, only about the render-time size). The
/// resolved family/size/bold/italic are then handed to Core Text directly.
///
/////////////////////////////////////////////////////////////////////////////
CTFontRef cGUIPDFPrintout::GetOrLoadFont(const std::string& descriptor)
{
    if (!mPdfContext || descriptor.empty())
    {
        return nullptr;
    }

    auto it = mFontCache.find(descriptor);
    if (it != mFontCache.end())
    {
        return it->second;
    }

    QFont qfont = FontUtils::FontFromDescriptor(descriptor);
    std::string family = qfont.family().toStdString();
    double pointSize = qfont.pointSizeF();
    if (pointSize <= 0)
    {
        pointSize = 12.0;
    }
    bool bold = qfont.bold();
    bool italic = qfont.italic();

    CTFontRef ctFont = nullptr;

    CFStringRef familyStr = CFStringCreateWithCString(kCFAllocatorDefault, family.c_str(), kCFStringEncodingUTF8);
    if (familyStr)
    {
        CTFontRef baseFont = CTFontCreateWithName(familyStr, static_cast<CGFloat>(pointSize), nullptr);
        CFRelease(familyStr);

        if (baseFont)
        {
            if (bold || italic)
            {
                CTFontSymbolicTraits traits = 0;
                if (bold)
                {
                    traits |= kCTFontBoldTrait;
                }
                if (italic)
                {
                    traits |= kCTFontItalicTrait;
                }

                // Ask Core Text for the matching family member (e.g. "Courier
                // New Bold") with the requested traits. Falls back to the
                // plain face if the family has no such member.
                CTFontRef styledFont = CTFontCreateCopyWithSymbolicTraits(
                    baseFont, static_cast<CGFloat>(pointSize), nullptr, traits, traits);
                if (styledFont)
                {
                    CFRelease(baseFont);
                    ctFont = styledFont;
                }
                else
                {
                    ctFont = baseFont;
                }
            }
            else
            {
                ctFont = baseFont;
            }
        }
    }

    // Fall back to a standard PDF font name if family resolution somehow failed
    if (!ctFont)
    {
        CFStringRef fallback = CFSTR("Helvetica");
        ctFont = CTFontCreateWithName(fallback, static_cast<CGFloat>(pointSize), nullptr);
    }

    mFontCache[descriptor] = ctFont;
    return ctFont;
}


/////////////////////////////////////////////////////////////////////////////
void cGUIPDFPrintout::ClearFontCache(void)
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
CGColorRef cGUIPDFPrintout::MakeCGColor(const sSeqRGBColor& color)
{
    if (color.IsDefault())
    {
        return CGColorCreateGenericRGB(0.0, 0.0, 0.0, 1.0);
    }
    return CGColorCreateGenericRGB(color.red / 255.0, color.green / 255.0, color.blue / 255.0, 1.0);
}


/////////////////////////////////////////////////////////////////////////////
void cGUIPDFPrintout::DrawGrapheme(CGContextRef ctx, CTFontRef font, const sSeqRGBColor& color,
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
void cGUIPDFPrintout::DrawUnderline(CGContextRef ctx, const sSeqRGBColor& color,
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
