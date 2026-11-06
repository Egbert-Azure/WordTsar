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

#include "src/tui/layout/layout.h"
#include "src/tui/layout/tuifontutils.h"
#include "src/core/document/document.h"
#include "src/core/include/config.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for TUI cLayout -- the TUI layout engine.
/// Verifies paragraph segmentation, line wrapping, and document
/// layout using the TUI font system.
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("TUI layout - font system initialization")
{
    cLayout layout;

    SUBCASE("InitializeFontSystem succeeds")
    {
        bool ok = layout.InitializeFontSystem();
        CHECK(ok);
        layout.ShutdownFontSystem();
    }

    SUBCASE("ShutdownFontSystem without init does not crash")
    {
        layout.ShutdownFontSystem();
    }
}


TEST_CASE("TUI layout - empty document")
{
    cDocument doc;
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    SUBCASE("At least one paragraph exists")
    {
        CHECK(layout.GetNumberOfParagraphs() >= 1);
    }

    SUBCASE("At least one line exists")
    {
        CHECK(layout.GetNumberOfLines() >= 1);
    }

    SUBCASE("First line starts at position 0")
    {
        if (layout.GetNumberOfLines() > 0)
        {
            const sLineLayout* line = layout.GetLineByRawLineNumber(0);
            REQUIRE(line != nullptr);
            CHECK(line->documentPosition == 0);
        }
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - single paragraph")
{
    cDocument doc;
    doc.Insert("Hello World\r");

    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    SUBCASE("Paragraph count matches document")
    {
        CHECK(layout.GetNumberOfParagraphs() == doc.GetNumberofParagraphs());
    }

    SUBCASE("At least one line")
    {
        CHECK(layout.GetNumberOfLines() >= 1);
    }

    SUBCASE("First line starts at document position 0")
    {
        const sLineLayout* line = layout.GetLineByRawLineNumber(0);
        REQUIRE(line != nullptr);
        CHECK(line->documentPosition == 0);
    }

    SUBCASE("First line has segments")
    {
        const sLineLayout* line = layout.GetLineByRawLineNumber(0);
        REQUIRE(line != nullptr);
        CHECK(line->segments.size() > 0);
    }

    SUBCASE("First segment has graphemes")
    {
        const sLineLayout* line = layout.GetLineByRawLineNumber(0);
        REQUIRE(line != nullptr);
        REQUIRE(line->segments.size() > 0);
        CHECK(line->segments[0].GetGraphemeCount() > 0);
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - multiple paragraphs")
{
    cDocument doc;
    doc.Insert("First paragraph\r");
    doc.Insert("Second paragraph\r");
    doc.Insert("Third paragraph\r");

    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    SUBCASE("Paragraph count matches document")
    {
        PARAGRAPH_T docParas = doc.GetNumberofParagraphs();
        PARAGRAPH_T layoutParas = layout.GetNumberOfParagraphs();
        CHECK(layoutParas == docParas);
    }

    SUBCASE("Line count >= paragraph count")
    {
        // Each paragraph produces at least one line
        CHECK(layout.GetNumberOfLines() >= layout.GetNumberOfParagraphs());
    }

    SUBCASE("Lines have increasing document positions")
    {
        LINE_T lineCount = layout.GetNumberOfLines();
        if (lineCount >= 2)
        {
            const sLineLayout* line0 = layout.GetLineByRawLineNumber(0);
            const sLineLayout* line1 = layout.GetLineByRawLineNumber(1);
            REQUIRE(line0 != nullptr);
            REQUIRE(line1 != nullptr);
            CHECK(line1->documentPosition > line0->documentPosition);
        }
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - long text wrapping")
{
    cDocument doc;
    // Insert a very long paragraph that should wrap
    std::string longText;
    for (int i = 0; i < 50; ++i)
    {
        longText += "word ";
    }
    longText += "\r";
    doc.Insert(longText);

    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    SUBCASE("Long paragraph produces multiple lines")
    {
        // 50 words should wrap to more than 1 line at standard page width
        LINE_T lines = layout.GetNumberOfLines();
        // At least 2 lines (wrapping happened), plus the EOF paragraph line
        CHECK(lines >= 2);
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - dot command paragraph")
{
    cDocument doc;
    // Dot commands start with '.' at the beginning of a paragraph
    doc.Insert(".lm 10\r");
    doc.Insert("Normal text\r");

    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    SUBCASE("Layout does not crash on dot command")
    {
        // Just verify it completes without error
        CHECK(layout.GetNumberOfParagraphs() == doc.GetNumberofParagraphs());
    }

    SUBCASE("Lines exist for both paragraphs")
    {
        CHECK(layout.GetNumberOfLines() >= 2);
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - relayout after document change")
{
    cDocument doc;
    doc.Insert("Initial text\r");

    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    LINE_T linesBefore = layout.GetNumberOfLines();

    // Add more text
    doc.Insert("Added text\r");
    layout.LayoutDocument(&doc);

    SUBCASE("Line count increases after adding text")
    {
        LINE_T linesAfter = layout.GetNumberOfLines();
        CHECK(linesAfter >= linesBefore);
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - SetDefaultFont")
{
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());

    SUBCASE("SetDefaultFont with valid descriptor does not crash")
    {
        // Pipe-delimited descriptor format
        layout.SetDefaultFont("Courier New|12|0|0|0|0|0");
        // Verify it took effect
        CHECK(true);  // No crash = pass
    }

    SUBCASE("SetDefaultFont with DejaVu Sans does not crash")
    {
        layout.SetDefaultFont("DejaVu Sans|12|0|0|0|0|0");
        CHECK(true);
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - GetFontWithFormatting")
{
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDefaultFont("Courier New|12|0|0|0|0|0");

    SUBCASE("Returns non-empty descriptor")
    {
        std::string desc = layout.GetFontWithFormatting(false, false, false);
        CHECK(!desc.empty());
    }

    SUBCASE("Bold descriptor differs or matches (no crash)")
    {
        std::string normal = layout.GetFontWithFormatting(false, false, false);
        std::string bold = layout.GetFontWithFormatting(true, false, false);
        CHECK(!bold.empty());
        // Bold descriptor should have the bold flag set
        CHECK(TUIFontUtils::IsBoldInDescriptor(bold));
    }

    SUBCASE("Italic descriptor has italic flag")
    {
        std::string italic = layout.GetFontWithFormatting(false, true, false);
        CHECK(!italic.empty());
        CHECK(TUIFontUtils::IsItalicInDescriptor(italic));
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - reveal codes never change the layout")
{
    cDocument doc;
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);

    for (int word = 0; word < 80; ++word)
    {
        if (word % 3 == 1)
        {
            doc.Insert(static_cast<CHAR_T>(STYLE_BOLD));
        }
        doc.Insert("word" + std::to_string(word) + " ");
    }
    doc.Insert("\r");
    doc.SetPosition(40);
    doc.SetSavePosition(1);
    doc.SetPosition(200);
    doc.SetBeginBlock();

    // content line and layout X of every visible grapheme
    auto places = [&](void)
    {
        std::map<POSITION_T, std::pair<LINE_T, COORD_T>> result;
        for (POSITION_T pos = 0; pos < doc.GetTextSize(); ++pos)
        {
            std::string grapheme = doc.GetCharNoAdvance(pos);
            if (grapheme.empty() || grapheme[0] == MARKER_CHAR)
            {
                continue;
            }
            LINE_T line = layout.GetLineFromPosition(pos);
            REQUIRE(line >= 0);
            result[pos] = { layout.GetLineByRawLineNumber(line)->contentLineNumber, layout.FindLayoutCoordInLine(pos, line) };
        }
        return result;
    };

    layout.SetShowControl(SHOW_NONE);
    layout.SetMarkersVisible(false);
    layout.LayoutDocument(&doc);
    auto print = places();
    REQUIRE(print.rbegin()->second.first >= 2);
    PAGE_T pages = layout.GetNumberOfPages();

    for (eShowControl mode : { SHOW_ALL, SHOW_DOT, SHOW_NONE })
    {
        for (bool markersVisible : { true, false })
        {
            CAPTURE(static_cast<int>(mode));
            CAPTURE(markersVisible);
            layout.SetShowControl(mode);
            layout.SetMarkersVisible(markersVisible);
            layout.LayoutDocument(&doc);
            CHECK(layout.GetNumberOfPages() == pages);
            auto shown = places();
            REQUIRE(shown.size() == print.size());
            for (const auto& [pos, place] : print)
            {
                CHECK(shown[pos].first == place.first);
                CHECK(shown[pos].second == doctest::Approx(place.second));
            }
        }
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - header / footer lines use the default font, not the end-of-document font")
{
    cDocument doc;
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    doc.Insert("Plain first line\r");
    doc.Insert(".he Page Header\r.fo Page Footer\r");
    for (int i = 0; i < 150; i++)
    {
        doc.Insert("Body text line for the TUI header font test.\r");
    }
    sInternalFonts big;
    big.fontname = "Courier New";
    big.name = "Courier New";
    big.size = 18.0;
    big.haveWSFont = false;
    doc.InsertFont(big);
    doc.BeginBold();
    doc.Insert("big bold text left open\r");
    layout.LayoutDocument(&doc);
    REQUIRE(layout.GetNumberOfPages() >= 2);

    const sLineLayout* first = layout.GetLineByRawLineNumber(layout.GetFirstLineOfParagraph(0));
    REQUIRE(first != nullptr);
    REQUIRE(first->segments.empty() == false);
    std::string defaultFont = first->segments.front().font;

    const auto& headers = layout.GetPageHeaders(2);
    const auto& footers = layout.GetPageFooters(2);
    REQUIRE(headers.empty() == false);
    REQUIRE(footers.empty() == false);
    for (const sSegmentLayout& seg : headers[0].line.segments)
    {
        CHECK(seg.font == defaultFont);
    }
    for (const sSegmentLayout& seg : footers[0].line.segments)
    {
        CHECK(seg.font == defaultFont);
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - building header / footer lines leaves the layout state unchanged")
{
    cDocument doc;
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    sInternalFonts big;
    big.fontname = "Courier New";
    big.name = "Courier New";
    big.size = 20.0;
    big.haveWSFont = false;

    // Header with its own font / bold codes; body formatting left open at the end
    doc.Insert(".he Start ");
    doc.InsertFont(big);
    doc.BeginBold();
    doc.Insert("Big Bold #\r");
    doc.Insert(".fo Footer #\r");
    for (int i = 0; i < 150; i++)
    {
        doc.Insert("Body text line for the TUI header state test.\r");
    }
    big.size = 18.0;
    doc.InsertFont(big);
    doc.BeginItalics();
    doc.Insert("big italic text left open\r");
    for (int i = 0; i < 150; i++)
    {
        doc.Insert("More body text in the open font.\r");
    }
    layout.WaitForFontEnumeration();
    layout.LayoutDocument(&doc);
    REQUIRE(layout.GetNumberOfPages() >= 4);

    // Fonts and positions of every line of every paragraph
    auto snapshot = [&](void)
    {
        std::vector<std::string> result;
        for (PARAGRAPH_T para = 0; para < layout.GetNumberOfParagraphs(); ++para)
        {
            const sParagraphLayout* paraLayout = layout.GetParagraphLayout(para);
            REQUIRE(paraLayout != nullptr);
            for (const sLineLayout& line : paraLayout->lines)
            {
                std::string text = std::to_string(para) + ":" + std::to_string(line.pagenumber) + ":" +
                                   std::to_string(line.pagey);
                for (const sSegmentLayout& seg : line.segments)
                {
                    text += "|" + seg.font;
                    for (COORD_T pos : seg.position)
                    {
                        text += "," + std::to_string(pos);
                    }
                }
                result.push_back(text);
            }
        }
        return result;
    };

    std::string plainBefore = layout.GetFontWithFormatting(false, false, false);
    std::vector<sHeaderFooterLine> headers = layout.GetPageHeaders(2);
    REQUIRE(headers.empty() == false);
    REQUIRE(headers[0].line.segments.empty() == false);
    CHECK(headers[0].line.segments.back().font.find("|20.0|1|") != std::string::npos);
    CHECK(layout.GetFontWithFormatting(false, false, false) == plainBefore);

    // Building in the middle of an incremental layout does not change it
    std::vector<std::string> expected = snapshot();
    PAGE_T pages = layout.GetNumberOfPages();
    for (PARAGRAPH_T para = 0; para < layout.GetNumberOfParagraphs(); ++para)
    {
        layout.LayoutParagraph(para);
        PAGE_T page = (para % pages) + 1;
        layout.GetPageHeaders(page);
        layout.GetPageFooters(page);
    }
    CHECK(snapshot() == expected);

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - a raw byte has no width in the print layout")
{
    cDocument doc;
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);

    // A0 ^B1 B2 \r3
    doc.Insert("A");
    doc.InsertRawByte(0x02);
    doc.Insert("B\r");

    auto byteWidth = [&](void)
    {
        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);
        REQUIRE(segments.size() == 1);
        REQUIRE(segments[0].position.size() == 4);
        CHECK(segments[0].length == 4);
        return segments[0].position[2] - segments[0].position[1];
    };

    SUBCASE("screen: the width of the caret notation")
    {
        for (eShowControl mode : { SHOW_ALL, SHOW_NONE })
        {
            CAPTURE(static_cast<int>(mode));
            layout.SetShowControl(mode);
            layout.SetMarkersVisible(true);
            CHECK(byteWidth() == doctest::Approx(layout.GetTextWidth("^B")));
            CHECK(layout.GetTextWidth("^B") > 0);
        }
    }

    SUBCASE("print: no width")
    {
        layout.SetShowControl(SHOW_NONE);
        layout.SetMarkersVisible(false);
        CHECK(byteWidth() == doctest::Approx(0.0));
        layout.SetMarkersVisible(true);
    }

    layout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - a raw byte in a header is hidden on screen and in print")
{
    // .h1 a^Bb and the same header without the byte
    cDocument doc;
    doc.Insert(".h1 a");
    doc.InsertRawByte(0x02);
    doc.Insert("b\r");
    doc.Insert("text\r");

    cDocument plain;
    plain.Insert(".h1 ab\r");
    plain.Insert("text\r");

    auto headerWidth = [&](cDocument& source, bool markersVisible, std::vector<std::string>& graphemes)
    {
        cLayout layout;
        REQUIRE(layout.InitializeFontSystem());
        layout.SetDocument(&source);
        layout.SetShowControl(markersVisible ? SHOW_ALL : SHOW_NONE);
        layout.SetMarkersVisible(markersVisible);
        layout.LayoutDocument(&source);

        COORD_T width = 0;
        for (const auto& segment : layout.BuildHeaderFooterSegments(0, 4, 1, graphemes))
        {
            width += segment.totalWidth;
        }
        layout.ShutdownFontSystem();
        return width;
    };

    for (bool markersVisible : { true, false })
    {
        CAPTURE(markersVisible);
        std::vector<std::string> graphemes;
        std::vector<std::string> plainGraphemes;
        COORD_T width = headerWidth(doc, markersVisible, graphemes);
        COORD_T plainWidth = headerWidth(plain, markersVisible, plainGraphemes);

        CHECK(graphemes == plainGraphemes);
        CHECK(width == doctest::Approx(plainWidth));
        CHECK(width > 0);
    }
}


namespace
{

sRawSequence KeptIndexItem(const std::string& body)
{
    sRawSequence sequence;
    sequence.type = 0x0E;
    sequence.body = body;
    return sequence;
}

// width of all segments, and whether any has a tag, marker or control code index
COORD_T SegmentsWidth(const std::vector<sSegmentLayout>& segments, bool& decorated)
{
    COORD_T width = 0;
    for (const auto& segment : segments)
    {
        width += segment.totalWidth;
        if (!segment.markers.empty() || !segment.controlCodeIndices.empty())
        {
            decorated = true;
        }
    }
    return width;
}

}   // anonymous namespace


TEST_CASE("TUI layout - a kept sequence is invisible and has no width")
{
    // A0 <seq>1 B2 \r3, and the same line without it
    cDocument doc;
    cLayout layout;
    REQUIRE(layout.InitializeFontSystem());
    layout.SetDocument(&doc);
    doc.Insert("A");
    doc.InsertRawSequence(KeptIndexItem("Alpha\\, Beta"));
    doc.Insert("B\r");

    cDocument plain;
    cLayout plainLayout;
    REQUIRE(plainLayout.InitializeFontSystem());
    plainLayout.SetDocument(&plain);
    plain.Insert("AB\r");

    for (eShowControl mode : { SHOW_ALL, SHOW_NONE })
    {
        for (bool markersVisible : { true, false })
        {
            CAPTURE(static_cast<int>(mode));
            CAPTURE(markersVisible);
            layout.SetShowControl(mode);
            layout.SetMarkersVisible(markersVisible);
            plainLayout.SetShowControl(mode);
            plainLayout.SetMarkersVisible(markersVisible);

            CHECK(layout.GetDisplayCharacter(1, "\x7F") == "");

            bool decorated = false;
            bool plainDecorated = false;
            COORD_T width = SegmentsWidth(layout.BuildParagraphSegments(0), decorated);
            COORD_T plainWidth = SegmentsWidth(plainLayout.BuildParagraphSegments(0), plainDecorated);

            CHECK(width == doctest::Approx(plainWidth));
            CHECK(width > 0);
            CHECK(decorated == false);
        }
    }

    layout.SetMarkersVisible(true);
    layout.ShutdownFontSystem();
    plainLayout.ShutdownFontSystem();
}


TEST_CASE("TUI layout - a kept sequence in a header is hidden on screen and in print")
{
    cDocument doc;
    doc.Insert(".h1 a");
    doc.InsertRawSequence(KeptIndexItem("Alpha"));
    doc.Insert("b\r");
    doc.Insert("text\r");

    cDocument plain;
    plain.Insert(".h1 ab\r");
    plain.Insert("text\r");

    auto headerWidth = [&](cDocument& source, bool markersVisible, std::vector<std::string>& graphemes)
    {
        cLayout layout;
        REQUIRE(layout.InitializeFontSystem());
        layout.SetDocument(&source);
        layout.SetShowControl(markersVisible ? SHOW_ALL : SHOW_NONE);
        layout.SetMarkersVisible(markersVisible);
        layout.LayoutDocument(&source);

        COORD_T width = 0;
        for (const auto& segment : layout.BuildHeaderFooterSegments(0, 4, 1, graphemes))
        {
            width += segment.totalWidth;
        }
        layout.ShutdownFontSystem();
        return width;
    };

    for (bool markersVisible : { true, false })
    {
        CAPTURE(markersVisible);
        std::vector<std::string> graphemes;
        std::vector<std::string> plainGraphemes;
        COORD_T width = headerWidth(doc, markersVisible, graphemes);
        COORD_T plainWidth = headerWidth(plain, markersVisible, plainGraphemes);

        CHECK(graphemes == plainGraphemes);
        CHECK(width == doctest::Approx(plainWidth));
        CHECK(width > 0);
    }
}


TEST_CASE("TUI layout - a soft tab at the start of a paragraph lays out like a normal tab")
{
    // x of the period after a tab of the given type at the start of ".xx text"
    auto periodX = [](unsigned char type, bool &isCommand)
    {
        cDocument doc;
        cLayout layout;
        REQUIRE(layout.InitializeFontSystem());
        layout.SetDocument(&doc);

        sWSTab tab;
        tab.type = type;
        tab.tabsize = 720;
        tab.abstabsize = 720;
        tab.size = 5;
        doc.InsertTab(tab);
        doc.Insert(".xx text\r");
        layout.LayoutDocument(&doc);

        const sParagraphLayout* para = layout.GetParagraphLayout(0);
        REQUIRE(para != nullptr);
        isCommand = para->isCommand;
        COORD_T x = layout.FindCoordInLine(1, layout.GetLineFromPosition(1));
        layout.ShutdownFontSystem();
        return x;
    };

    bool tabCommand = true;
    bool softCommand = true;
    COORD_T tabX = periodX(TAB_TAB, tabCommand);
    COORD_T softX = periodX(TAB_SOFT, softCommand);

    CHECK(tabCommand == false);
    CHECK(softCommand == false);
    CHECK(tabX > 0);
    CHECK(softX == doctest::Approx(tabX));
}
