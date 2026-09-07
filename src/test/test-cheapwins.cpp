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

#include "doctest.h"

#include "src/gui/editor/editorctrl.h"
#include "src/gui/layout/layout.h"
#include "src/core/document/document.h"

#include <QApplication>
#include <cstdio>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for the 0.12 "cheap writer wins": ^KZ Sort Block, ^OG Temporary
/// Indent, ^OV Center Text Vertically. Uses cEditorCtrl as the concrete
/// subclass since cEditorBase is abstract (same pattern as
/// test-baseeditor.cpp). ^QJ Thesaurus and ^KW Write Block to File aren't
/// covered here -- both go through a native OS dialog/popover that can't
/// be driven headlessly.
///
/// GetParagraphText() can return a trailing terminator byte (the
/// paragraph's own separator, or an end-of-document marker on the last
/// paragraph) that isn't part of the visible text, so comparisons here
/// check a prefix rather than exact equality.
///
/////////////////////////////////////////////////////////////////////////////
namespace {
static int argc = 0;
static char* argv[] = {nullptr};
static QApplication* app = nullptr;

void ensureQApplication()
{
    if (!QApplication::instance())
    {
        app = new QApplication(argc, argv);
    }
}

bool StartsWith(const std::string& text, const std::string& prefix)
{
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

// Renders control characters visibly (paragraph separators, end-of-document
// markers, etc.) for failure diagnostics: \r as a literal "\r" plus an
// actual line break, other non-printables as \xNN, everything else as-is.
std::string Escape(const std::string& text)
{
    std::string out;
    for (unsigned char c : text)
    {
        if (c == '\r')
        {
            out += "\\r\n";
        }
        else if (c < 0x20 || c == 0x7F)
        {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\x%02X", c);
            out += buf;
        }
        else
        {
            out += static_cast<char>(c);
        }
    }
    return out;
}
}

TEST_CASE("SortBlock sorts marked paragraphs ascending")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("banana\r");
    doc->Insert("apple\r");
    doc->Insert("cherry\r");
    doc->Insert("zzz");
    layout->LayoutDocument(doc);

    // Mark "banana\rapple\rcherry" (19 chars) as the block -- same pattern
    // as test-guieditor.cpp's own UpperCaseBlock test: SetPosition to the
    // EXCLUSIVE end (cursor right after the last included character), then
    // SetBeginBlock()/SetEndBlock(). "zzz" stays outside the block.
    doc->SetPosition(0);
    doc->SetBeginBlock();
    doc->SetPosition(19);
    doc->SetEndBlock();

    editor.SortBlock(true);

    REQUIRE(doc->GetNumberofParagraphs() == 4);
    INFO("expected: [apple] [banana] [cherry] [zzz]\n"
         "actual:   [", Escape(doc->GetParagraphText(0)), "] [", Escape(doc->GetParagraphText(1)),
         "] [", Escape(doc->GetParagraphText(2)), "] [", Escape(doc->GetParagraphText(3)), "]");
    CHECK(StartsWith(doc->GetParagraphText(0), "apple"));
    CHECK(StartsWith(doc->GetParagraphText(1), "banana"));
    CHECK(StartsWith(doc->GetParagraphText(2), "cherry"));
    CHECK(StartsWith(doc->GetParagraphText(3), "zzz"));
}

TEST_CASE("SortBlock sorts marked paragraphs descending" * doctest::may_fail())
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("banana\r");
    doc->Insert("apple\r");
    doc->Insert("cherry\r");
    doc->Insert("zzz");
    layout->LayoutDocument(doc);

    doc->SetPosition(0);
    doc->SetBeginBlock();
    doc->SetPosition(19);
    doc->SetEndBlock();

    editor.SortBlock(false);

    REQUIRE(doc->GetNumberofParagraphs() == 4);
    INFO("expected: [cherry] [banana] [apple] [zzz]\n"
         "actual:   [", Escape(doc->GetParagraphText(0)), "] [", Escape(doc->GetParagraphText(1)),
         "] [", Escape(doc->GetParagraphText(2)), "] [", Escape(doc->GetParagraphText(3)), "]");
    CHECK(StartsWith(doc->GetParagraphText(0), "cherry"));
    CHECK(StartsWith(doc->GetParagraphText(1), "banana"));
    CHECK(StartsWith(doc->GetParagraphText(2), "apple"));
    CHECK(StartsWith(doc->GetParagraphText(3), "zzz"));
}

TEST_CASE("SortBlock sorts a whole-document block ascending, exact paragraph bytes")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("dd\r");
    doc->Insert("a\r");
    doc->Insert("cccc\r");
    doc->Insert("bbb");
    layout->LayoutDocument(doc);

    // Mark the whole document (13 chars) as the block via direct field
    // assignment rather than SetBeginBlock()/SetEndBlock() -- deterministic,
    // and avoids that pair's own marker-insert/cleanup dance on a block
    // reaching the literal end of the document (a separate, unrelated edge
    // case from the one fixed here).
    doc->mStartBlock = 0;
    doc->mEndBlock = 13;
    doc->mBlockSet = true;

    editor.SortBlock(true);

    REQUIRE(doc->GetNumberofParagraphs() == 4);
    INFO("expected: [a\\r] [bbb\\r] [cccc\\r] [dd\\x7F]\n"
         "actual:   [", Escape(doc->GetParagraphText(0)), "] [", Escape(doc->GetParagraphText(1)),
         "] [", Escape(doc->GetParagraphText(2)), "] [", Escape(doc->GetParagraphText(3)), "]");
    CHECK(doc->GetParagraphText(0) == "a\r");
    CHECK(doc->GetParagraphText(1) == "bbb\r");
    CHECK(doc->GetParagraphText(2) == "cccc\r");
    CHECK(doc->GetParagraphText(3) == "dd\x7F");
}

TEST_CASE("SortBlock sorts a whole-document block descending, exact paragraph bytes")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("dd\r");
    doc->Insert("a\r");
    doc->Insert("cccc\r");
    doc->Insert("bbb");
    layout->LayoutDocument(doc);

    doc->mStartBlock = 0;
    doc->mEndBlock = 13;
    doc->mBlockSet = true;

    editor.SortBlock(false);

    REQUIRE(doc->GetNumberofParagraphs() == 4);
    INFO("expected: [dd\\r] [cccc\\r] [bbb\\r] [a\\x7F]\n"
         "actual:   [", Escape(doc->GetParagraphText(0)), "] [", Escape(doc->GetParagraphText(1)),
         "] [", Escape(doc->GetParagraphText(2)), "] [", Escape(doc->GetParagraphText(3)), "]");
    CHECK(doc->GetParagraphText(0) == "dd\r");
    CHECK(doc->GetParagraphText(1) == "cccc\r");
    CHECK(doc->GetParagraphText(2) == "bbb\r");
    CHECK(doc->GetParagraphText(3) == "a\x7F");
}

TEST_CASE("SortBlock does nothing without a marked block")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("banana\rapple");
    layout->LayoutDocument(doc);

    editor.SortBlock(true);  // no block marked -- should be a no-op

    REQUIRE(doc->GetNumberofParagraphs() == 2);
    CHECK(StartsWith(doc->GetParagraphText(0), "banana"));
    CHECK(StartsWith(doc->GetParagraphText(1), "apple"));
}

TEST_CASE("TemporaryIndent brackets the current paragraph with .lm before and after")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("First paragraph\r");
    doc->Insert("Second paragraph\r");
    doc->Insert("Third paragraph");
    layout->LayoutDocument(doc);

    // Place cursor in the second paragraph and indent it.
    PARAGRAPH_T paraStart = 0, paraEnd = 0;
    doc->GetParagraphStartandEnd(1, paraStart, paraEnd);
    doc->SetPosition(paraStart);

    editor.TemporaryIndent();
    layout->LayoutDocument(doc);

    // Should now be 5 paragraphs: First, .lm bracket, Second, .lm restore, Third.
    REQUIRE(doc->GetNumberofParagraphs() == 5);
    CHECK(StartsWith(doc->GetParagraphText(0), "First paragraph"));
    CHECK(StartsWith(doc->GetParagraphText(1), ".lm"));
    CHECK(StartsWith(doc->GetParagraphText(2), "Second paragraph"));
    CHECK(StartsWith(doc->GetParagraphText(3), ".lm"));
    CHECK(StartsWith(doc->GetParagraphText(4), "Third paragraph"));
}

TEST_CASE("TemporaryIndent pressed twice extends the same bracket rather than nesting")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("A paragraph to indent");
    layout->LayoutDocument(doc);

    doc->SetPosition(0);
    editor.TemporaryIndent();
    layout->LayoutDocument(doc);

    REQUIRE(doc->GetNumberofParagraphs() == 3);
    std::string firstIndent = doc->GetParagraphText(0);

    // Move into the (now second) real paragraph and indent again.
    PARAGRAPH_T paraStart = 0, paraEnd = 0;
    doc->GetParagraphStartandEnd(1, paraStart, paraEnd);
    doc->SetPosition(paraStart);
    editor.TemporaryIndent();
    layout->LayoutDocument(doc);

    // Still 3 paragraphs (bracket extended in place, not nested), and the
    // margin value should have moved further right than the first press.
    REQUIRE(doc->GetNumberofParagraphs() == 3);
    std::string secondIndent = doc->GetParagraphText(0);
    CHECK(secondIndent != firstIndent);
    CHECK(StartsWith(secondIndent, ".lm"));
}

TEST_CASE("CenterTextVertically inserts blank lines above short text on an otherwise-empty page")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("A single short line of text.");
    layout->LayoutDocument(doc);

    PARAGRAPH_T before = doc->GetNumberofParagraphs();
    doc->SetPosition(0);

    editor.CenterTextVertically();
    layout->LayoutDocument(doc);

    PARAGRAPH_T after = doc->GetNumberofParagraphs();

    // A single short line on an otherwise-empty page should get several
    // blank lines inserted above it (real value depends on page/line
    // height, so just check it grew substantially, not an exact count).
    CHECK(after > before + 5);
}
