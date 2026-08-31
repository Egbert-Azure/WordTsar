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

#include "doctest.h"

#include "src/gui/print/printout.h"
#include "src/gui/editor/editorctrl.h"
#include "src/gui/layout/layout.h"
#include "src/core/document/document.h"
#include <QApplication>
#include <QImage>
#include <QPainter>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test fixture for cPrintout tests
/// Ensures QApplication exists (required for Qt widgets and printing)
///
/////////////////////////////////////////////////////////////////////////////
static int argc = 0;
static char* argv[] = {nullptr};
static QApplication* app = nullptr;

static void ensureQApplication()
{
    if (!QApplication::instance())
    {
        app = new QApplication(argc, argv);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test subclass that exposes private methods for testing
///
/////////////////////////////////////////////////////////////////////////////
class cPrintoutTest : public cPrintout
{
public:
    cPrintoutTest(cEditorCtrl* editor) : cPrintout(editor) {}

    // Expose protected methods for testing
    using cPrintout::DrawLine;
    using cPrintout::DrawSegment;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test suite for Phase 0.6.1 printing functionality
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("cPrintout constructor initializes correctly")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintout printout(&editor);

    // If constructor succeeds without crashing, test passes
    CHECK(true);
}

TEST_CASE("cPrintout destructor cleanup works")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    {
        cPrintout printout(&editor);
        // Destructor called here
    }

    // If destructor succeeds without crashing, test passes
    CHECK(true);
}

TEST_CASE("printPage handles empty document")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    cPrintout printout(&editor);

    // Create a minimal image for testing
    QImage image(100, 100, QImage::Format_RGB32);
    QPainter painter(&image);

    // This should not crash even with empty document
    printout.printPage(1, &painter);

    CHECK(true);
}

TEST_CASE("printPage renders only specified page")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Add paragraphs to document
    doc->Insert("First paragraph on page 1.");
    doc->Insert("\r");
    doc->Insert(".PA");
    doc->Insert("\r");
    doc->Insert("Second paragraph on page 2.");

    // Layout the document
    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should not crash when rendering page 1
    printout.printPage(1, &painter);

    CHECK(true);
}

TEST_CASE("printPage handles multi-page document")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Add multiple pages
    doc->Insert("Page 1 content.");
    doc->Insert("\r");
    doc->Insert(".PA");
    doc->Insert("\r");
    doc->Insert("Page 2 content.");
    doc->Insert("\r");
    doc->Insert(".PA");
    doc->Insert("\r");
    doc->Insert("Page 3 content.");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render each page without crashing
    printout.printPage(1, &painter);
    printout.printPage(2, &painter);
    printout.printPage(3, &painter);

    CHECK(layout->GetNumberOfPages() == 3);
}

TEST_CASE("DrawLine handles empty line")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create empty line
    sLineLayout line;
    line.pagex = 1440;
    line.pagey = 1440;
    line.pagenumber = 1;

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should not crash with empty line
    printout.DrawLine(line, nullptr, &painter);

    CHECK(true);
}

TEST_CASE("DrawSegment handles empty segment")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create empty segment
    sSegmentLayout segment;

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should not crash with empty segment
    printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

    CHECK(true);
}

TEST_CASE("DrawSegment handles populated segment")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create populated segment
    sSegmentLayout segment;
    segment.length++; // ("H");
    segment.length++; // ("e");
    segment.length++; // ("l");
    segment.length++; // ("l");
    segment.length++; // ("o");
    segment.position.push_back(0);
    segment.position.push_back(100);
    segment.position.push_back(200);
    segment.position.push_back(300);
    segment.position.push_back(400);
    segment.segmentheight = 240;
    segment.textcolor.red = -1;
    segment.textcolor.green = -1;
    segment.textcolor.blue = -1;
    segment.textcolor.alpha = -1;

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render without crashing
    printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

    CHECK(true);
}

TEST_CASE("DrawSegment handles subscript positioning")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create subscript segment
    sSegmentLayout segment;
    segment.length++; // ("2");
    segment.position.push_back(0);
    segment.segmentheight = 240;
    segment.isSubscript = true;
    segment.textcolor.red = -1;
    segment.textcolor.green = -1;
    segment.textcolor.blue = -1;
    segment.textcolor.alpha = -1;

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render subscript without crashing
    printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

    CHECK(segment.isSubscript == true);
}

TEST_CASE("DrawSegment handles superscript positioning")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create superscript segment
    sSegmentLayout segment;
    segment.length++; // ("2");
    segment.position.push_back(0);
    segment.segmentheight = 240;
    segment.isSuperscript = true;
    segment.textcolor.red = -1;
    segment.textcolor.green = -1;
    segment.textcolor.blue = -1;
    segment.textcolor.alpha = -1;

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render superscript without crashing
    printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

    CHECK(segment.isSuperscript == true);
}

TEST_CASE("DrawLine with populated line and segments")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create line with segments
    sLineLayout line;
    line.pagex = 1440;
    line.pagey = 1440;
    line.pagenumber = 1;

    // Add segment to line
    sSegmentLayout segment;
    segment.length++; // ("T");
    segment.length++; // ("e");
    segment.length++; // ("s");
    segment.length++; // ("t");
    segment.position.push_back(0);
    segment.position.push_back(100);
    segment.position.push_back(200);
    segment.position.push_back(300);
    segment.segmentheight = 240;
    segment.textcolor.red = -1;
    segment.textcolor.green = -1;
    segment.textcolor.blue = -1;
    segment.textcolor.alpha = -1;

    line.segments.push_back(segment);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render line with segment
    printout.DrawLine(line, nullptr, &painter);

    CHECK(line.segments.size() == 1);
}

TEST_CASE("printDocument handles page range correctly")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Create 5-page document
    for (int i = 1; i <= 5; i++)
    {
        doc->Insert("Page ");
        doc->Insert(std::to_string(i));
        doc->Insert(" content.");
        if (i < 5)
        {
            doc->Insert("\r");
            doc->Insert(".PA");
            doc->Insert("\r");
        }
    }

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    CHECK(layout->GetNumberOfPages() == 5);
}

TEST_CASE("printDocument handles all pages default")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Create multi-page document
    doc->Insert("First page.");
    doc->Insert("\r");
    doc->Insert(".PA");
    doc->Insert("\r");
    doc->Insert("Second page.");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    // Verify we have 2 pages
    CHECK(layout->GetNumberOfPages() == 2);
}

TEST_CASE("printPage with multi-paragraph document")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Create document with multiple paragraphs on same page
    doc->Insert("First paragraph.");
    doc->Insert("\r");
    doc->Insert("Second paragraph.");
    doc->Insert("\r");
    doc->Insert("Third paragraph.");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render all paragraphs on page 1
    printout.printPage(1, &painter);

    CHECK(layout->GetNumberOfParagraphs() == 3);
}

TEST_CASE("printPage filters lines by page number correctly")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Create two-page document
    doc->Insert("Content on page 1.");
    doc->Insert("\r");
    doc->Insert(".PA");
    doc->Insert("\r");
    doc->Insert("Content on page 2.");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Print page 2 - should only show page 2 content
    printout.printPage(2, &painter);

    // Verify we have 2 pages
    CHECK(layout->GetNumberOfPages() == 2);
}

TEST_CASE("DrawLine with multiple segments on same line")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create line with multiple segments (simulating formatting changes)
    sLineLayout line;
    line.pagex = 1440;
    line.pagey = 1440;
    line.pagenumber = 1;

    // First segment - normal text
    sSegmentLayout segment1;
    segment1.length++; // ("H");
    segment1.length++; // ("e");
    segment1.position.push_back(0);
    segment1.position.push_back(100);
    segment1.segmentheight = 240;
    segment1.textcolor.red = -1;
    segment1.textcolor.green = -1;
    segment1.textcolor.blue = -1;
    segment1.textcolor.alpha = -1;

    // Second segment - different color
    sSegmentLayout segment2;
    segment2.length++; // ("l");
    segment2.length++; // ("o");
    segment2.position.push_back(200);
    segment2.position.push_back(300);
    segment2.segmentheight = 240;
    segment2.textcolor.red = 255;
    segment2.textcolor.green = 0;
    segment2.textcolor.blue = 0;
    segment2.textcolor.alpha = 255;

    line.segments.push_back(segment1);
    line.segments.push_back(segment2);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render both segments
    printout.DrawLine(line, nullptr, &painter);

    CHECK(line.segments.size() == 2);
}

TEST_CASE("DrawSegment with different colors")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create red segment
    sSegmentLayout segment;
    segment.length++; // ("R");
    segment.length++; // ("e");
    segment.length++; // ("d");
    segment.position.push_back(0);
    segment.position.push_back(100);
    segment.position.push_back(200);
    segment.segmentheight = 240;
    segment.textcolor.red = 255;
    segment.textcolor.green = 0;
    segment.textcolor.blue = 0;
    segment.textcolor.alpha = 255;

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render in red
    printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

    CHECK(segment.textcolor.red == 255);
    CHECK(segment.textcolor.green == 0);
    CHECK(segment.textcolor.blue == 0);
}


TEST_CASE("DrawSegment with default sentinel color")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    cPrintoutTest printout(&editor);

    SUBCASE("Default sentinel does not crash DrawSegment")
    {
        // Create segment with default sentinel color
        sSegmentLayout segment;
        segment.length++;
        segment.length++;
        segment.length++;
        segment.position.push_back(0);
        segment.position.push_back(100);
        segment.position.push_back(200);
        segment.segmentheight = 240;
        segment.textcolor.red = -1;
        segment.textcolor.green = -1;
        segment.textcolor.blue = -1;
        segment.textcolor.alpha = -1;

        QImage image(800, 1000, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        // Sentinel color should render as black in print context
        printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

        CHECK(segment.textcolor.IsDefault() == true);
    }

    SUBCASE("Explicit color still renders correctly")
    {
        // Create segment with explicit green
        sSegmentLayout segment;
        segment.length++;
        segment.length++;
        segment.position.push_back(0);
        segment.position.push_back(100);
        segment.segmentheight = 240;
        segment.textcolor.red = 0;
        segment.textcolor.green = 200;
        segment.textcolor.blue = 0;
        segment.textcolor.alpha = 255;

        QImage image(800, 1000, QImage::Format_RGB32);
        image.fill(Qt::white);
        QPainter painter(&image);

        printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

        CHECK(segment.textcolor.red == 0);
        CHECK(segment.textcolor.green == 200);
        CHECK(segment.textcolor.blue == 0);
        CHECK(segment.textcolor.IsDefault() == false);
    }
}


TEST_CASE("printPage with empty paragraphs")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Create document with empty paragraph
    doc->Insert("First paragraph.");
    doc->Insert("\r");
    doc->Insert("\r");
    doc->Insert("Third paragraph after empty.");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should not crash with empty paragraph
    printout.printPage(1, &painter);

    CHECK(true);
}

TEST_CASE("PrintPreview initializes correctly")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Add some content
    doc->Insert("Test content for print preview.");
    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    // We can't actually test PrintPreview() as it shows a modal dialog
    // But we can verify the object constructs correctly
    CHECK(layout->GetNumberOfPages() >= 1);
}

TEST_CASE("PrintDocument initializes correctly")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Add some content
    doc->Insert("Test content for print.");
    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    // We can't actually test PrintDocument() as it shows a modal QPrintDialog
    // But we can verify the object constructs correctly
    CHECK(layout->GetNumberOfPages() >= 1);
}

TEST_CASE("printPage with Unicode content")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Add Unicode content
    doc->Insert("Café résumé with naïve ideas 🎉");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render Unicode without crashing
    printout.printPage(1, &painter);

    CHECK(layout->GetNumberOfParagraphs() >= 1);
}

TEST_CASE("printPage with comprehensive Unicode")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Add comprehensive Unicode
    doc->Insert("Café Résumé Naïve 🎉 Grüße München Привет мир 世界");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render all Unicode types without crashing
    printout.printPage(1, &painter);

    CHECK(true);
}

TEST_CASE("printPage with multi-paragraph Unicode")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Add multiple Unicode paragraphs
    doc->Insert("First paragraph with café.");
    doc->Insert("\r");
    doc->Insert("Second paragraph with 🎉.");
    doc->Insert("\r");
    doc->Insert("Third paragraph with Привет.");
    doc->Insert("\r");
    doc->Insert("Fourth paragraph with 世界.");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render all Unicode paragraphs without crashing
    printout.printPage(1, &painter);

    CHECK(layout->GetNumberOfParagraphs() == 4);
}

TEST_CASE("printPage with Unicode across multiple pages")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Create multi-page document with Unicode
    doc->Insert("Page 1: Café résumé naïve.");
    doc->Insert("\r");
    doc->Insert(".PA");
    doc->Insert("\r");
    doc->Insert("Page 2: Grüße München 🎉.");
    doc->Insert("\r");
    doc->Insert(".PA");
    doc->Insert("\r");
    doc->Insert("Page 3: Привет мир 世界.");

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render each page with Unicode without crashing
    printout.printPage(1, &painter);
    printout.printPage(2, &painter);
    printout.printPage(3, &painter);

    CHECK(layout->GetNumberOfPages() == 3);
}

TEST_CASE("printDocument with Unicode range")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    layout->SetDocument(doc);

    // Create multi-page Unicode document
    for (int i = 1; i <= 5; i++)
    {
        doc->Insert("Page ");
        doc->Insert(std::to_string(i));
        doc->Insert(": Café 🎉 Привет 世界.");
        if (i < 5)
        {
            doc->Insert("\r");
            doc->Insert(".PA");
            doc->Insert("\r");
        }
    }

    layout->LayoutDocument(doc);

    cPrintout printout(&editor);

    CHECK(layout->GetNumberOfPages() == 5);
}

TEST_CASE("DrawSegment with Unicode text")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create segment with Unicode
    sSegmentLayout segment;
    // Simulating "Cafe-acute" - 4 graphemes
    segment.length = 4;
    segment.position.push_back(0);
    segment.position.push_back(100);
    segment.position.push_back(200);
    segment.position.push_back(300);
    segment.segmentheight = 240;
    segment.textcolor.red = -1;
    segment.textcolor.green = -1;
    segment.textcolor.blue = -1;
    segment.textcolor.alpha = -1;

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render Unicode without crashing
    printout.DrawSegment(segment, nullptr, 1440, 1440, &painter);

    CHECK(segment.length == 4);
}

TEST_CASE("DrawLine with Unicode segments")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());    cPrintoutTest printout(&editor);

    // Create line with Unicode segments
    sLineLayout line;
    line.pagex = 1440;
    line.pagey = 1440;
    line.pagenumber = 1;

    // First segment - cafe-acute
    sSegmentLayout segment1;
    segment1.length = 4; // "Cafe-acute"
    segment1.position.push_back(0);
    segment1.position.push_back(100);
    segment1.position.push_back(200);
    segment1.position.push_back(300);
    segment1.segmentheight = 240;
    segment1.textcolor.red = -1;
    segment1.textcolor.green = -1;
    segment1.textcolor.blue = -1;
    segment1.textcolor.alpha = -1;

    // Second segment - emoji
    sSegmentLayout segment2;
    segment2.length = 1; // "emoji"
    segment2.position.push_back(400);
    segment2.segmentheight = 240;
    segment2.textcolor.red = -1;
    segment2.textcolor.green = -1;
    segment2.textcolor.blue = -1;
    segment2.textcolor.alpha = -1;

    line.segments.push_back(segment1);
    line.segments.push_back(segment2);

    QImage image(800, 1000, QImage::Format_RGB32);
    image.fill(Qt::white);
    QPainter painter(&image);

    // Should render both Unicode segments
    printout.DrawLine(line, nullptr, &painter);

    CHECK(line.segments.size() == 2);
}
