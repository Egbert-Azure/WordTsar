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
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <unistd.h>

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

// One entry per document position before the end-of-file marker: the
// grapheme, "\\r" for a hard return, or the marker kind with its data.
std::vector<std::string> Cells(cDocument* doc)
{
    std::vector<std::string> cells;
    for (POSITION_T pos = 0; pos < doc->GetTextSize() - 1; pos++)
    {
        std::string g = doc->GetCharNoAdvance(pos);
        if (g.size() == 1 && static_cast<unsigned char>(g[0]) == MARKER_CHAR)
        {
            eModifiers control = doc->GetControlChar(pos);
            if (control == STYLE_TAB)
            {
                cells.push_back("<tab:" + std::to_string(doc->GetTab(pos).type) + ">");
            }
            else if (control == STYLE_RAW_BYTE)
            {
                cells.push_back("<raw:" + std::to_string(doc->GetRawByte(pos)) + ">");
            }
            else if (control == STYLE_BOLD)
            {
                cells.push_back("<bold>");
            }
            else
            {
                cells.push_back("<ctl:" + std::to_string(control) + ">");
            }
        }
        else if (g == "\r")
        {
            cells.push_back("\\r");
        }
        else
        {
            cells.push_back(g);
        }
    }
    return cells;
}

std::string Join(const std::vector<std::string>& cells)
{
    std::string out;
    for (const auto& c : cells)
    {
        out += "[" + c + "]";
    }
    return out;
}

void InsertTabMarker(cDocument* doc)
{
    doc->Insert(static_cast<CHAR_T>(STYLE_TAB));
}

void InsertBoldMarker(cDocument* doc)
{
    doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
}

const std::string TAB = "<tab:" + std::to_string(TAB_TAB) + ">";
const std::string BOLD = "<bold>";
const std::string RAW2 = "<raw:2>";
}

namespace {
// head\r0-4 | b TAB 2 \r5-8 | a B x B \r9-13 | c RAW 3 \r14-17 | tail18
void InsertSortLines(cDocument* doc)
{
    doc->Insert("head\rb");
    InsertTabMarker(doc);
    doc->Insert("2\ra");
    InsertBoldMarker(doc);
    doc->Insert("x");
    InsertBoldMarker(doc);
    doc->Insert("\rc");
    doc->InsertRawByte(0x02);
    doc->Insert("3\rtail");
}

const std::vector<std::string> LINE_A = { "a", BOLD, "x", BOLD };
const std::vector<std::string> LINE_B = { "b", TAB, "2" };
const std::vector<std::string> LINE_C = { "c", RAW2, "3" };

std::vector<std::string> SortLinesCells(const std::vector<std::vector<std::string>>& lines)
{
    std::vector<std::string> cells = { "h", "e", "a", "d", "\\r" };
    for (const auto& line : lines)
    {
        cells.insert(cells.end(), line.begin(), line.end());
        cells.push_back("\\r");
    }
    for (const char* c : { "t", "a", "i", "l" })
    {
        cells.push_back(c);
    }
    return cells;
}
}

TEST_CASE("SortBlock keeps tabs, bold and raw bytes with their lines, one undo restores")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    InsertSortLines(doc);
    layout->LayoutDocument(doc);

    // block ends at the end of the last sorted line, before its \r
    doc->SetPosition(5);
    doc->SetBeginBlock();
    doc->SetPosition(17);
    doc->SetEndBlock();

    std::vector<std::string> before = Cells(doc);
    REQUIRE(Join(before) == Join(SortLinesCells({ LINE_B, LINE_A, LINE_C })));

    bool ascending = true;
    std::vector<std::string> expected;
    SUBCASE("ascending")
    {
        ascending = true;
        expected = SortLinesCells({ LINE_A, LINE_B, LINE_C });
    }
    SUBCASE("descending")
    {
        ascending = false;
        expected = SortLinesCells({ LINE_C, LINE_B, LINE_A });
    }

    editor.SortBlock(ascending);

    CHECK(Join(Cells(doc)) == Join(expected));
    CHECK(doc->GetNumberofParagraphs() == 5);
    CHECK(doc->mBlockSet == true);
    CHECK(doc->mStartBlock == 5);
    CHECK(doc->mEndBlock == 17);

    REQUIRE(editor.Undo());
    CHECK(Join(Cells(doc)) == Join(before));

    REQUIRE(editor.Redo());
    CHECK(Join(Cells(doc)) == Join(expected));
}

TEST_CASE("SortBlock over whole lines leaves the line after the block separate")
{
    ensureQApplication();

    // 18: block ends at the start of the following line
    // 17: block ends at the end of its last line
    for (POSITION_T blockEnd : { 18, 17 })
    {
        for (bool ascending : { true, false })
        {
            CAPTURE(blockEnd);
            CAPTURE(ascending);

            cEditorCtrl editor;
            cDocument* doc = editor.GetDocument();
            cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
            layout->SetDocument(doc);

            InsertSortLines(doc);
            layout->LayoutDocument(doc);

            std::vector<std::string> before = Cells(doc);
            std::vector<std::string> expected = ascending
                ? SortLinesCells({ LINE_A, LINE_B, LINE_C })
                : SortLinesCells({ LINE_C, LINE_B, LINE_A });

            doc->mStartBlock = 5;
            doc->mEndBlock = blockEnd;
            doc->mBlockSet = true;

            editor.SortBlock(ascending);

            CHECK(Join(Cells(doc)) == Join(expected));
            CHECK(doc->GetNumberofParagraphs() == 5);
            CHECK(doc->GetPlainText(18, 22, "\n") == "tail");
            CHECK(doc->mStartBlock == 5);
            CHECK(doc->mEndBlock == blockEnd);

            REQUIRE(editor.Undo());
            CHECK(Join(Cells(doc)) == Join(before));
        }
    }
}

TEST_CASE("SortBlock of plain whole lines ending at the start of the next line")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("banana\rapple\rcherry\rzzz");
    layout->LayoutDocument(doc);

    // block is "banana\rapple\rcherry\r", up to the start of "zzz"
    doc->SetPosition(0);
    doc->SetBeginBlock();
    doc->SetPosition(20);
    doc->SetEndBlock();

    std::string expected;
    SUBCASE("ascending")
    {
        editor.SortBlock(true);
        expected = "apple\rbanana\rcherry\rzzz";
    }
    SUBCASE("descending")
    {
        editor.SortBlock(false);
        expected = "cherry\rbanana\rapple\rzzz";
    }

    std::string text = doc->GetText();
    INFO("actual: [", Escape(text), "]");
    CHECK(text.substr(0, text.find('\x7F')) == expected);
    CHECK(doc->GetNumberofParagraphs() == 4);
}

TEST_CASE("UpperCaseBlock keeps tabs, bold and raw bytes in place, one undo restores")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    // x0 ' '1 a2 b3 TAB4 c5 B6 d7 ß8 Q9 R10 B11 RAW12 e13 ' '14 y15 \r16 next17
    doc->Insert("x ab");
    InsertTabMarker(doc);
    doc->Insert("c");
    InsertBoldMarker(doc);
    doc->Insert("dßQR");
    InsertBoldMarker(doc);
    doc->InsertRawByte(0x02);
    doc->Insert("e y\rnext");
    layout->LayoutDocument(doc);

    doc->SetPosition(10);
    doc->SetSavePosition(0);
    doc->SetPosition(15);
    doc->SetSavePosition(1);
    doc->SetPosition(2);
    doc->SetBeginBlock();
    doc->SetPosition(14);
    doc->SetEndBlock();

    std::vector<std::string> before = Cells(doc);
    REQUIRE(Join(before) == Join({ "x", " ", "a", "b", TAB, "c", BOLD, "d", "ß", "Q", "R", BOLD, RAW2,
                                   "e", " ", "y", "\\r", "n", "e", "x", "t" }));

    editor.UpperCaseBlock();

    std::vector<std::string> expected = { "x", " ", "A", "B", TAB, "C", BOLD, "D", "S", "S", "Q", "R", BOLD, RAW2,
                                          "E", " ", "y", "\\r", "n", "e", "x", "t" };
    CHECK(Join(Cells(doc)) == Join(expected));
    CHECK(doc->GetPlainText(0, doc->GetTextSize() - 1, "\n") == "x AB\tCDSSQR\x02" "E y\nnext");
    CHECK(doc->mBlockSet == true);
    CHECK(doc->mStartBlock == 2);
    CHECK(doc->mEndBlock == 15);
    // R is at 11 after ß became SS; the letters around it were not touched
    CHECK(doc->mSavePosition[0] == 11);
    CHECK(doc->mSavePosition[1] == 16);

    REQUIRE(editor.Undo());
    CHECK(Join(Cells(doc)) == Join(before));

    REQUIRE(editor.Redo());
    CHECK(Join(Cells(doc)) == Join(expected));
}

TEST_CASE("LowerCaseBlock keeps tabs, bold and raw bytes in place, one undo restores")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    // X0 ' '1 A2 B3 TAB4 C5 B6 D7 q8 r9 B10 RAW11 E12 ' '13 Y14 \r15 NEXT16
    doc->Insert("X AB");
    InsertTabMarker(doc);
    doc->Insert("C");
    InsertBoldMarker(doc);
    doc->Insert("Dqr");
    InsertBoldMarker(doc);
    doc->InsertRawByte(0x02);
    doc->Insert("E Y\rNEXT");
    layout->LayoutDocument(doc);

    doc->SetPosition(9);
    doc->SetSavePosition(0);
    doc->SetPosition(2);
    doc->SetBeginBlock();
    doc->SetPosition(13);
    doc->SetEndBlock();

    std::vector<std::string> before = Cells(doc);

    editor.LowerCaseBlock();

    std::vector<std::string> expected = { "X", " ", "a", "b", TAB, "c", BOLD, "d", "q", "r", BOLD, RAW2,
                                          "e", " ", "Y", "\\r", "N", "E", "X", "T" };
    CHECK(Join(Cells(doc)) == Join(expected));
    CHECK(doc->GetPlainText(0, doc->GetTextSize() - 1, "\n") == "X ab\tcdqr\x02" "e Y\nNEXT");
    CHECK(doc->mBlockSet == true);
    CHECK(doc->mStartBlock == 2);
    CHECK(doc->mEndBlock == 13);
    CHECK(doc->mSavePosition[0] == 9);

    REQUIRE(editor.Undo());
    CHECK(Join(Cells(doc)) == Join(before));

    REQUIRE(editor.Redo());
    CHECK(Join(Cells(doc)) == Join(expected));
}

TEST_CASE("SentenceCaseBlock keeps tabs, bold and raw bytes in place, one undo restores")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    // end. aB TAB i TAB B Cd B RAW x! yes \r next
    doc->Insert("end. aB");
    InsertTabMarker(doc);
    doc->Insert("i");
    InsertTabMarker(doc);
    InsertBoldMarker(doc);
    doc->Insert("Cd");
    InsertBoldMarker(doc);
    doc->InsertRawByte(0x02);
    doc->Insert("x! yes\rnext");
    layout->LayoutDocument(doc);

    // between e(19) and s(20)
    doc->SetPosition(20);
    doc->SetSavePosition(0);
    doc->SetPosition(0);
    doc->SetBeginBlock();
    doc->SetPosition(21);
    doc->SetEndBlock();

    std::vector<std::string> before = Cells(doc);

    editor.SentenceCaseBlock();

    std::vector<std::string> expected = { "e", "n", "d", ".", " ", "A", "b", TAB, "I", TAB, BOLD, "c", "d", BOLD, RAW2,
                                          "x", "!", " ", "Y", "e", "s", "\\r", "n", "e", "x", "t" };
    CHECK(Join(Cells(doc)) == Join(expected));
    CHECK(doc->GetPlainText(0, doc->GetTextSize() - 1, "\n") == "end. Ab\tI\tcd\x02x! Yes\nnext");
    CHECK(doc->mBlockSet == true);
    CHECK(doc->mStartBlock == 0);
    CHECK(doc->mEndBlock == 21);
    CHECK(doc->mSavePosition[0] == 20);

    REQUIRE(editor.Undo());
    CHECK(Join(Cells(doc)) == Join(before));

    REQUIRE(editor.Redo());
    CHECK(Join(Cells(doc)) == Join(expected));
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

TEST_CASE("SortBlock sorts marked paragraphs descending")
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

TEST_CASE("SortBlock descending with text directly before and after the block, same line")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("prebanana\rapple\rcherrypost");
    layout->LayoutDocument(doc);

    // Block is "banana\rapple\rcherry": positions 3 to 22 (exclusive).
    doc->SetPosition(3);
    doc->SetBeginBlock();
    doc->SetPosition(22);
    doc->SetEndBlock();

    editor.SortBlock(false);

    std::string text = doc->GetText();
    INFO("actual: [", Escape(text), "]");
    CHECK(text.substr(0, text.find('\x7F')) == "precherry\rbanana\rapplepost");
}

TEST_CASE("SortBlock descending with paragraphs directly before and after the block")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    doc->Insert("head\rbanana\rapple\rcherry\rtail");
    layout->LayoutDocument(doc);

    // Block is "banana\rapple\rcherry": positions 5 to 24 (exclusive).
    doc->SetPosition(5);
    doc->SetBeginBlock();
    doc->SetPosition(24);
    doc->SetEndBlock();

    editor.SortBlock(false);

    std::string text = doc->GetText();
    INFO("actual: [", Escape(text), "]");
    CHECK(text.substr(0, text.find('\x7F')) == "head\rcherry\rbanana\rapple\rtail");
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

namespace {
// Code points of a UTF-8 string as "U+XXXX ..." for failure output.
std::string CodePoints(const std::string& text)
{
    std::string out;
    size_t i = 0;
    while (i < text.size())
    {
        unsigned char c = static_cast<unsigned char>(text[i]);
        size_t len = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        char32_t cp = (len == 1) ? c : (len == 2) ? (c & 0x1F) : (len == 3) ? (c & 0x0F) : (c & 0x07);
        for (size_t k = 1; k < len && i + k < text.size(); k++)
        {
            cp = (cp << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        }
        char buf[16];
        std::snprintf(buf, sizeof(buf), "U+%04X ", static_cast<unsigned>(cp));
        out += buf;
        i += len;
    }
    return out;
}

// Every grapheme of the document as stored, end-of-file marker excluded.
std::string StoredText(cDocument* doc)
{
    std::string out;
    for (POSITION_T pos = 0; pos < doc->GetTextSize() - 1; pos++)
    {
        out += doc->GetCharNoAdvance(pos);
    }
    return out;
}

std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

const std::string FAMILY = "\U0001F468\u200D\U0001F469\u200D\U0001F467";
const std::string FLAG = "\U0001F1E9\U0001F1EA";

// Document "text\rtail" with text marked as the block. Runs op, checks the
// text against expected through character access, GetPlainText and a text
// save, then one undo against the original and redo against expected.
void CheckBlockOp(const std::string& text, const std::function<void(cEditorCtrl&)>& op,
                  const std::string& expectedText)
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-graphemes-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;
    struct sRemoveDir
    {
        std::filesystem::path path;
        ~sRemoveDir()
        {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } removeDir{ dir };

    {
        cEditorCtrl editor;
        cDocument* doc = editor.GetDocument();
        cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
        layout->SetDocument(doc);

        doc->Insert(text + "\rtail");
        layout->LayoutDocument(doc);

        std::string original = doc->Normalize(text) + "\rtail";
        std::string expected = doc->Normalize(expectedText) + "\rtail";
        std::string originalPlain = original;
        std::string expectedPlain = expected;
        std::replace(originalPlain.begin(), originalPlain.end(), '\r', '\n');
        std::replace(expectedPlain.begin(), expectedPlain.end(), '\r', '\n');
        REQUIRE(StoredText(doc) == original);

        doc->mStartBlock = 0;
        doc->mEndBlock = doc->GetTextSize() - 1 - 5;
        doc->mBlockSet = true;
        doc->ClearUndoHistory();

        REQUIRE(editor.SaveFile((dir / "before.txt").string()));
        std::string savedBefore = ReadBytes(dir / "before.txt");
        REQUIRE(savedBefore.find(originalPlain) != std::string::npos);

        op(editor);

        INFO("expected: ", CodePoints(expected));
        INFO("actual:   ", CodePoints(StoredText(doc)));
        CHECK(StoredText(doc) == expected);
        CHECK(doc->GetPlainText(0, doc->GetTextSize() - 1, "\n") == expectedPlain);
        REQUIRE(editor.SaveFile((dir / "after.txt").string()));
        std::string savedAfter = ReadBytes(dir / "after.txt");
        CHECK(savedAfter.find(expectedPlain) != std::string::npos);

        // an operation that changes nothing leaves no undo step
        if (expected == original)
        {
            CHECK(doc->CanUndo() == false);
            return;
        }

        REQUIRE(editor.Undo());
        INFO("after undo: ", CodePoints(StoredText(doc)));
        CHECK(StoredText(doc) == original);
        CHECK(doc->GetPlainText(0, doc->GetTextSize() - 1, "\n") == originalPlain);
        REQUIRE(editor.SaveFile((dir / "undo.txt").string()));
        CHECK(ReadBytes(dir / "undo.txt") == savedBefore);

        REQUIRE(editor.Redo());
        INFO("after redo: ", CodePoints(StoredText(doc)));
        CHECK(StoredText(doc) == expected);
        REQUIRE(editor.SaveFile((dir / "redo.txt").string()));
        CHECK(ReadBytes(dir / "redo.txt") == savedAfter);
    }
}

std::string JoinLines(const std::vector<std::string>& lines)
{
    std::string out;
    for (size_t i = 0; i < lines.size(); i++)
    {
        out += (i > 0 ? "\r" : "") + lines[i];
    }
    return out;
}
}

TEST_CASE("SortBlock keeps every code point of multi-code-point characters")
{
    ensureQApplication();

    const std::string l1 = "1 " + FLAG;
    const std::string l2 = "2 \u00DF\u0301 x";
    const std::string l3 = "3 q\u0301";
    const std::string l4 = "4 e\u0301";
    const std::string l5 = "5 a\u0323\u0301";
    const std::string l6 = "6 " + FAMILY;
    std::string text = JoinLines({ l3, l6, l1, l5, l2, l4 });

    SUBCASE("ascending")
    {
        CheckBlockOp(text, [](cEditorCtrl& e) { e.SortBlock(true); },
                     JoinLines({ l1, l2, l3, l4, l5, l6 }));
    }
    SUBCASE("descending")
    {
        CheckBlockOp(text, [](cEditorCtrl& e) { e.SortBlock(false); },
                     JoinLines({ l6, l5, l4, l3, l2, l1 }));
    }
}

TEST_CASE("Block copy and delete undo keep every code point of multi-code-point characters")
{
    ensureQApplication();

    const std::string text = "q\u0301 " + FAMILY + " " + FLAG + " a\u0323\u0301 \u00DF\u0301 e\u0301";

    SUBCASE("^KC copies the block after itself")
    {
        CheckBlockOp(text, [](cEditorCtrl& e)
        {
            cDocument* doc = e.GetDocument();
            doc->SetPosition(doc->mEndBlock);
            e.CopyBlock();
        }, text + text);
    }
    SUBCASE("delete of the block, undone")
    {
        CheckBlockOp(text, [](cEditorCtrl& e)
        {
            cDocument* doc = e.GetDocument();
            POSITION_T length = doc->mEndBlock - doc->mStartBlock;
            doc->Delete(0, length);
        }, "");
    }
}

TEST_CASE("Case conversion keeps every other code point of multi-code-point characters")
{
    ensureQApplication();

    auto upper = [](cEditorCtrl& e) { e.UpperCaseBlock(); };
    auto lower = [](cEditorCtrl& e) { e.LowerCaseBlock(); };
    auto sentence = [](cEditorCtrl& e) { e.SentenceCaseBlock(); };

    SUBCASE("^K\" upper case")
    {
        CheckBlockOp("q\u0301", upper, "Q\u0301");
        CheckBlockOp(FAMILY, upper, FAMILY);
        CheckBlockOp(FLAG, upper, FLAG);
        CheckBlockOp("a\u0323\u0301", upper, "\u1EA0\u0301");
        CheckBlockOp("\u00DF\u0301 x", upper, "SS\u0301 X");
        CheckBlockOp("e\u0301", upper, "\u00C9");
        CheckBlockOp("q\u0301 " + FAMILY + " " + FLAG + " a\u0323\u0301 \u00DF\u0301 e\u0301", upper,
                     "Q\u0301 " + FAMILY + " " + FLAG + " \u1EA0\u0301 SS\u0301 \u00C9");
    }
    SUBCASE("^K' lower case")
    {
        CheckBlockOp("Q\u0301", lower, "q\u0301");
        CheckBlockOp(FAMILY, lower, FAMILY);
        CheckBlockOp(FLAG, lower, FLAG);
        CheckBlockOp("A\u0323\u0301", lower, "\u1EA1\u0301");
        CheckBlockOp("\u00DF\u0301 X", lower, "\u00DF\u0301 x");
        CheckBlockOp("E\u0301", lower, "\u00E9");
        CheckBlockOp("Q\u0301 " + FAMILY + " " + FLAG + " A\u0323\u0301 \u00DF\u0301 E\u0301", lower,
                     "q\u0301 " + FAMILY + " " + FLAG + " \u1EA1\u0301 \u00DF\u0301 \u00E9");
    }
    SUBCASE("^K. sentence case")
    {
        CheckBlockOp("x. q\u0301", sentence, "x. Q\u0301");
        CheckBlockOp("X " + FAMILY, sentence, "x " + FAMILY);
        CheckBlockOp("X " + FLAG, sentence, "x " + FLAG);
        CheckBlockOp("x. a\u0323\u0301", sentence, "x. \u1EA0\u0301");
        CheckBlockOp("\u00DF\u0301 X", sentence, "\u00DF\u0301 x");
        CheckBlockOp("x! e\u0301", sentence, "x! \u00C9");
        CheckBlockOp("Q\u0301Q. " + FAMILY + " " + FLAG + " a\u0323\u0301 \u00DF\u0301X! e\u0301 i", sentence,
                     "q\u0301q. " + FAMILY + " " + FLAG + " \u1EA0\u0301 \u00DF\u0301x! \u00C9 I");
    }
}

TEST_CASE("SentenceCaseBlock keeps sentence context when a marker splits a character")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->SetDocument(doc);

    // the bold marker splits the flag into two characters in the document;
    // without the marker the two regional indicators are one character
    doc->Insert("end. wOW \U0001F1E9");
    InsertBoldMarker(doc);
    doc->Insert("\U0001F1EA i AM\rtail");
    layout->LayoutDocument(doc);

    doc->mStartBlock = 0;
    doc->mEndBlock = doc->GetTextSize() - 1 - 5;
    doc->mBlockSet = true;
    std::vector<std::string> before = Cells(doc);

    editor.SentenceCaseBlock();

    CHECK(Join(Cells(doc)) == Join({ "e", "n", "d", ".", " ", "W", "o", "w", " ", "\U0001F1E9", BOLD, "\U0001F1EA",
                                     " ", "I", " ", "a", "m", "\\r", "t", "a", "i", "l" }));

    REQUIRE(editor.Undo());
    CHECK(Join(Cells(doc)) == Join(before));
}
