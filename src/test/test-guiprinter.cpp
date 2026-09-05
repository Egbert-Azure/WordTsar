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

#include "doctest.h"

#include "src/gui/print/pdfprintout.h"
#include "src/gui/editor/editorctrl.h"
#include "src/gui/layout/layout.h"
#include "src/core/document/document.h"

#include <QApplication>

#include <CoreGraphics/CoreGraphics.h>

#include <filesystem>
#include <cstdio>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test fixture: ensures QApplication exists (required for Qt widgets and
/// font resolution) and gives each test a private temp PDF path.
///
/// These tests exercise cGUIPDFPrintout::GeneratePDF() -- the Quartz/Core
/// Text PDF generator that replaced the old QPrinter/QPainter-based
/// cPrintout::printPage()/DrawLine()/DrawSegment() this file used to test
/// directly. Rather than re-testing private drawing internals, these check
/// the real, externally-verifiable contract: does GeneratePDF() produce a
/// valid PDF with the expected page count for a given document.
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

namespace
{
    // Returns the page count of a PDF file, or -1 if it can't be opened.
    int GetPDFPageCount(const std::string& path)
    {
        CFURLRef url = CFURLCreateFromFileSystemRepresentation(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(path.c_str()),
            static_cast<CFIndex>(path.length()),
            false);
        if (!url)
        {
            return -1;
        }

        CGPDFDocumentRef pdf = CGPDFDocumentCreateWithURL(url);
        CFRelease(url);
        if (!pdf)
        {
            return -1;
        }

        int pageCount = static_cast<int>(CGPDFDocumentGetNumberOfPages(pdf));
        CGPDFDocumentRelease(pdf);
        return pageCount;
    }

    // Unique temp path for a test's generated PDF, cleaned up by the caller.
    std::string TempPDFPath(const char* label)
    {
        auto path = std::filesystem::temp_directory_path()
            / (std::string("WordTsar-test-") + label + "-"
               + std::to_string(reinterpret_cast<uintptr_t>(label)) + ".pdf");
        return path.string();
    }
}

TEST_CASE("cGUIPDFPrintout constructor initializes correctly")
{
    ensureQApplication();

    cEditorCtrl editor;
    cGUIPDFPrintout printout(&editor);

    // If construction succeeds without crashing, this passes.
    CHECK(true);
}

TEST_CASE("GeneratePDF handles empty document")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    cGUIPDFPrintout printout(&editor);
    std::string path = TempPDFPath("empty");

    bool ok = printout.GeneratePDF(path);

    CHECK(ok);
    CHECK(GetPDFPageCount(path) >= 1);

    std::remove(path.c_str());
}

TEST_CASE("GeneratePDF produces one PDF page per layout page")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    // Three-page document via explicit .PA breaks.
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
    int expectedPages = layout->GetNumberOfPages();
    REQUIRE(expectedPages == 3);

    cGUIPDFPrintout printout(&editor);
    std::string path = TempPDFPath("multipage");

    bool ok = printout.GeneratePDF(path);

    CHECK(ok);
    CHECK(GetPDFPageCount(path) == expectedPages);

    std::remove(path.c_str());
}

TEST_CASE("GeneratePDF handles multi-paragraph document on one page")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("First paragraph.");
    doc->Insert("\r");
    doc->Insert("Second paragraph.");
    doc->Insert("\r");
    doc->Insert("Third paragraph.");

    layout->LayoutDocument(doc);
    REQUIRE(layout->GetNumberOfParagraphs() == 3);

    cGUIPDFPrintout printout(&editor);
    std::string path = TempPDFPath("multipara");

    bool ok = printout.GeneratePDF(path);

    CHECK(ok);
    CHECK(GetPDFPageCount(path) == 1);

    std::remove(path.c_str());
}

TEST_CASE("GeneratePDF handles empty paragraphs")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("First paragraph.");
    doc->Insert("\r");
    doc->Insert("\r");
    doc->Insert("Third paragraph after empty.");

    layout->LayoutDocument(doc);

    cGUIPDFPrintout printout(&editor);
    std::string path = TempPDFPath("emptypara");

    bool ok = printout.GeneratePDF(path);

    CHECK(ok);
    CHECK(GetPDFPageCount(path) >= 1);

    std::remove(path.c_str());
}

TEST_CASE("GeneratePDF handles Unicode content")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("Café Résumé Naïve 🎉 Grüße München Привет мир 世界");

    layout->LayoutDocument(doc);

    cGUIPDFPrintout printout(&editor);
    std::string path = TempPDFPath("unicode");

    bool ok = printout.GeneratePDF(path);

    CHECK(ok);
    CHECK(GetPDFPageCount(path) >= 1);

    std::remove(path.c_str());
}

TEST_CASE("GeneratePDF handles Unicode content across multiple pages")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

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
    int expectedPages = layout->GetNumberOfPages();
    REQUIRE(expectedPages == 3);

    cGUIPDFPrintout printout(&editor);
    std::string path = TempPDFPath("unicodepages");

    bool ok = printout.GeneratePDF(path);

    CHECK(ok);
    CHECK(GetPDFPageCount(path) == expectedPages);

    std::remove(path.c_str());
}

TEST_CASE("Word-wrap-disabled paragraphs still trigger page breaks (real QUICKREF.WS)")
{
    ensureQApplication();

    // Regression test for a real bug: cLayoutBase::WordWrapParagraph()'s
    // "word wrap disabled" branch (.aw off) built each paragraph's single
    // line without ever calling NeedNewPage()/IncrementPageAndCreateBox(),
    // so a run of many one-line paragraphs under .aw off just kept
    // stacking below the visible page with no automatic page break,
    // silently losing content until the document's own next .PA caught
    // up. QUICKREF.WS sets .aw off near the top and never turns it back
    // on, so it reproduces this directly.
    cEditorCtrl editor;
    bool loaded = editor.LoadFile("/Users/egbert/Documents/GitHub/WordTsar/docs/QUICKREF.WS");
    REQUIRE(loaded);

    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    REQUIRE(layout != nullptr);
    cDocument* doc = layout->GetDocument();
    REQUIRE(doc != nullptr);

    // Match GeneratePDF()'s own relayout (SHOW_NONE) before inspecting.
    eShowControl saved = layout->GetShowControl();
    layout->SetShowControl(SHOW_NONE);
    layout->SetActiveParagraph(-1);
    layout->LayoutDocument(doc);

    // Every line on every page must sit within that page's physical height
    // (Letter portrait = 15840 twips) -- anything past that is off the
    // physical page and will never actually print.
    const COORD_T pageHeightTwips = 15840;
    int violations = 0;
    for (int p = 0; p < layout->GetNumberOfParagraphs(); p++)
    {
        const sParagraphLayout* pl = layout->GetParagraphLayout(p);
        if (!pl)
        {
            continue;
        }
        for (auto& line : pl->lines)
        {
            if (line.pagey > pageHeightTwips)
            {
                violations++;
                MESSAGE("Para ", p, " pagey=", (int)line.pagey, " exceeds page height on page ", line.pagenumber);
            }
        }
    }
    CHECK(violations == 0);

    layout->SetShowControl(saved);
    PARAGRAPH_T curPara = doc->GetParagraphFromPosition(doc->GetPosition());
    layout->SetActiveParagraph(curPara);
    layout->LayoutDocument(doc);
}


TEST_CASE("GeneratePDF handles a populated multi-segment document")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    // Real content across several paragraphs, exercising the same segment
    // walk (RenderPage -> RenderLine -> RenderSegment) that draws bold/
    // italic/underline/color/sub/superscript runs, without depending on
    // this file's own knowledge of the document's internal control-code
    // byte encoding (covered by the editor/document-level formatting tests
    // elsewhere).
    doc->Insert("Plain text, then a second paragraph.");
    doc->Insert("\r");
    doc->Insert("A third paragraph with more words to wrap across a line.");

    layout->LayoutDocument(doc);

    cGUIPDFPrintout printout(&editor);
    std::string path = TempPDFPath("formatting");

    bool ok = printout.GeneratePDF(path);

    CHECK(ok);
    CHECK(GetPDFPageCount(path) >= 1);

    std::remove(path.c_str());
}
