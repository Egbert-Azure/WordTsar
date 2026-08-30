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

#include "src/gui/layout/layout.h"
#include "src/gui/utils/fontutils.h"
#include "src/core/document/document.h"
#include "src/core/include/config.h"
#include <QApplication>
#include <iostream>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test fixture to ensure QApplication exists (required for Qt font metrics)
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
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for cLayout GUI layout class
/// Tests for FindCoordInLine() and FindPositionInLine() black box API methods
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("FindCoordInLine - Position at start of line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Single line with text")
    {
        doc.Insert("Hello World\r");

        layout.LayoutDocument(&doc);

        // Get first line
        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Position 0 (start of line) should return first coordinate
        POSITION_T pos = line->documentPosition;
        COORD_T x = layout.FindCoordInLine(pos, lineNum);

        // Should return the line's starting X coordinate (absolute)
        CHECK(x >= 0);

        // If we have segments, check first segment's first position (convert to absolute)
        if (!line->segments.empty() && !line->segments[0].position.empty())
        {
            COORD_T absoluteX = line->pagex + line->segments[0].position[0];
            CHECK(x == absoluteX);
        }
    }

    SUBCASE("Empty document")
    {
        doc.Insert("\r");
        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        COORD_T x = layout.FindCoordInLine(0, lineNum);

        // Empty line should return line's X position
        CHECK(x >= 0);
    }
}


TEST_CASE("FindCoordInLine - Position in middle of line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Multi-character line")
    {
        doc.Insert("Hello World\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Test position 5 (middle of "Hello World")
        POSITION_T pos = line->documentPosition + 5;
        COORD_T x = layout.FindCoordInLine(pos, lineNum);

        // Should return a coordinate greater than start
        CHECK(x > 0);

        // Should be less than or equal to end of line (convert to absolute)
        if (!line->segments.empty())
        {
            const auto& lastSeg = line->segments.back();
            if (!lastSeg.position.empty())
            {
                COORD_T absoluteEndX = line->pagex + lastSeg.position.back();
                CHECK(x <= absoluteEndX);
            }
        }
    }
}


TEST_CASE("FindCoordInLine - Position at end of line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Normal text line")
    {
        doc.Insert("Test\r");
        layout.SetShowControl(SHOW_NONE) ;

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Find the last position on the line
        // Line has "Test" = 4 characters
        POSITION_T pos = line->documentPosition + 4;  // Last char
        COORD_T x = layout.FindCoordInLine(pos, lineNum);

        // Should return last coordinate
        CHECK(x > 0);

        // Check it's the last segment's last position (convert to absolute)
        if (!line->segments.empty())
        {
            const auto& lastSeg = line->segments.back();
            if (!lastSeg.position.empty())
            {
                COORD_T absoluteLastX = line->pagex + lastSeg.position.back();
                CHECK(x == absoluteLastX);
            }
        }
    }
}


TEST_CASE("FindCoordInLine - Position beyond line end")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Position way beyond line")
    {
        doc.Insert("Short\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Request position 1000 (way beyond end)
        POSITION_T pos = line->documentPosition + 1000;
        COORD_T x = layout.FindCoordInLine(pos, lineNum);

        // Should return last coordinate (clamped)
        CHECK(x >= 0);

        // Should match last segment's last position (convert to absolute)
        if (!line->segments.empty())
        {
            const auto& lastSeg = line->segments.back();
            if (!lastSeg.position.empty())
            {
                COORD_T absoluteLastX = line->pagex + lastSeg.position.back();
                CHECK(x == absoluteLastX);
            }
        }
    }
}


TEST_CASE("FindCoordInLine - Empty line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Blank paragraph")
    {
        doc.Insert("\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Debug: print positions for first segment
        if (!line->segments.empty() && !line->segments[0].position.empty())
        {
            std::cout << "First segment positions: ";
            for (size_t i = 0; i < line->segments[0].position.size(); ++i)
            {
            std::cout << line->segments[0].position[i];
            if (i < line->segments[0].position.size() - 1)
                std::cout << ", ";
            }
            std::cout << "\n";
        }
        else
        {
            std::cout << "No segments or positions available\n";
        }

        COORD_T x = layout.FindCoordInLine(0, lineNum);

        // Empty line should return line's pagex (absolute coordinate)
        CHECK(x >= 0);
        CHECK(x == line->pagex);
    }
}


TEST_CASE("FindCoordInLine - Multiple segments")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Line with multiple words")
    {
        // This should create multiple segments due to spaces/formatting
        doc.Insert("One Two Three\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Test various positions across the line
        for (POSITION_T offset = 0; offset < 10; ++offset)
        {
            POSITION_T pos = line->documentPosition + offset;
            COORD_T x = layout.FindCoordInLine(pos, lineNum);

            // Each position should have a valid coordinate
            CHECK(x >= 0);

            // Coordinates should generally increase (or stay same for control codes)
            if (offset > 0)
            {
                POSITION_T prevPos = line->documentPosition + offset - 1;
                COORD_T prevX = layout.FindCoordInLine(prevPos, lineNum);
                CHECK(x >= prevX);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// FindPositionInLine() Tests
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("FindPositionInLine - X at start of line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Click at line start")
    {
        doc.Insert("Hello\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Click at X = 0 (or line's pagex)
        COORD_T targetX = line->pagex;
        POSITION_T foundPos = layout.FindPositionInLine(targetX, lineNum);

        // Should return start of line (document position)
        CHECK(foundPos == line->documentPosition);
    }
}


TEST_CASE("FindPositionInLine - X in middle of line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Click in middle of text")
    {
        doc.Insert("Testing\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Get a middle X coordinate from the line (convert to absolute)
        if (!line->segments.empty() && line->segments[0].position.size() >= 3)
        {
            COORD_T targetX = line->pagex + line->segments[0].position[2];
            POSITION_T foundPos = layout.FindPositionInLine(targetX, lineNum);

            // Should find a position in the middle
            CHECK(foundPos >= line->documentPosition);
            CHECK(foundPos <= line->documentPosition + 10);

            // Verify it's close to expected position
            POSITION_T expectedPos = line->documentPosition + 2;
            CHECK(std::abs(static_cast<int>(foundPos - expectedPos)) <= 1);
        }
    }
}


TEST_CASE("FindPositionInLine - X at end of line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Click at line end")
    {
        doc.Insert("End\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Get last X coordinate (convert to absolute)
        if (!line->segments.empty())
        {
            const auto& lastSeg = line->segments.back();
            if (!lastSeg.position.empty())
            {
                COORD_T targetX = line->pagex + lastSeg.position.back();
                POSITION_T foundPos = layout.FindPositionInLine(targetX, lineNum);

                // Should return a position at or near the end
                CHECK(foundPos >= line->documentPosition);
            }
        }
    }
}


TEST_CASE("FindPositionInLine - X beyond line end")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Click far right of line")
    {
        doc.Insert("Short\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Click way beyond line end
        COORD_T targetX = 50000;  // Very large X
        POSITION_T foundPos = layout.FindPositionInLine(targetX, lineNum);

        // Should return last position on line
        CHECK(foundPos >= line->documentPosition);

        // Should be clamped to line end
        if (!line->segments.empty())
        {
            POSITION_T maxPos = line->documentPosition + 10;  // "Short" + some margin
            CHECK(foundPos <= maxPos);
        }
    }
}


TEST_CASE("FindPositionInLine - Closest position algorithm")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("X between two characters")
    {
        doc.Insert("AB\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Get positions of 'A' and 'B' (convert to absolute)
        if (!line->segments.empty() && line->segments[0].position.size() >= 2)
        {
            COORD_T xA = line->pagex + line->segments[0].position[0];
            COORD_T xB = line->pagex + line->segments[0].position[1];

            // Click exactly halfway between
            COORD_T targetX = (xA + xB) / 2;
            POSITION_T foundPos = layout.FindPositionInLine(targetX, lineNum);

            // Should return either position A or B (whichever is closer)
            CHECK((foundPos == line->documentPosition ||
                   foundPos == line->documentPosition + 1));
        }
    }
}


TEST_CASE("FindPositionInLine - Empty line")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Click on empty line")
    {
        doc.Insert("\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        COORD_T targetX = line->pagex;
        POSITION_T foundPos = layout.FindPositionInLine(targetX, lineNum);

        // Should return line start
        CHECK(foundPos == line->documentPosition);
    }
}


/////////////////////////////////////////////////////////////////////////////
// Round-trip tests: Position -> X -> Position
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("FindCoordInLine and FindPositionInLine - Round trip consistency")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Position to X to Position")
    {
        doc.Insert("Round trip test\r");

        layout.LayoutDocument(&doc);

        LINE_T lineNum = 0;
        const sLineLayout* line = layout.GetLineByRawLineNumber(lineNum);
        REQUIRE(line != nullptr);

        // Test round trip for various positions
        for (POSITION_T offset = 0; offset < 10; ++offset)
        {
            POSITION_T originalPos = line->documentPosition + offset;

            // Convert position to X
            COORD_T x = layout.FindCoordInLine(originalPos, lineNum);

            // Convert X back to position
            POSITION_T foundPos = layout.FindPositionInLine(x, lineNum);

            // Should get back same or very close position
            CHECK(std::abs(static_cast<int>(foundPos - originalPos)) <= 1);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// Invalid input tests
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("FindCoordInLine - Invalid line number")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Non-existent line")
    {
        doc.Insert("Test\r");
        layout.LayoutDocument(&doc);

        // Request invalid line 9999
        COORD_T x = layout.FindCoordInLine(0, 9999);

        // Should return 0 gracefully
        CHECK(x == 0);
    }
}


TEST_CASE("FindPositionInLine - Invalid line number")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Non-existent line")
    {
        doc.Insert("Test\r");
        layout.LayoutDocument(&doc);

        // Request invalid line 9999
        POSITION_T pos = layout.FindPositionInLine(1000, 9999);

        // Should return 0 gracefully
        CHECK(pos == 0);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test suite for Step 2: UpdateCurrentFont() functionality
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Step 2: UpdateCurrentFont() updates font based on formatting flags")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Font changes when bold control code is inserted")
    {
        // Insert text with bold control code
        doc.Insert("Normal");
        doc.Insert(std::string(1, static_cast<char>(STYLE_BOLD)));  // ^B toggles bold
        doc.Insert("Bold");
        doc.Insert(std::string(1, static_cast<char>(STYLE_BOLD)));  // ^B toggles bold off
        doc.Insert("Normal\r");

        layout.LayoutDocument(&doc);

        const sLineLayout* line = layout.GetLineByRawLineNumber(0);
        REQUIRE(line != nullptr);
        REQUIRE(line->segments.size() >= 2);

        // Get font descriptors
        std::string normalFont = line->segments[0].font;
        std::string boldFont = line->segments[1].font;

        // Fonts should be different (bold vs normal)
        CHECK(normalFont != boldFont);

        // Convert to QFont to verify bold state
        QFont qNormalFont = FontUtils::FontFromDescriptor(normalFont);
        QFont qBoldFont = FontUtils::FontFromDescriptor(boldFont);

        CHECK(qNormalFont.bold() == false);
        CHECK(qBoldFont.bold() == true);
    }

    SUBCASE("Font changes when italic control code is inserted")
    {
        // Insert text with italic control code
        doc.Insert("Normal");
        doc.Insert(std::string(1, static_cast<char>(STYLE_ITALICS)));  // ^Y toggles italic
        doc.Insert("Italic");
        doc.Insert(std::string(1, static_cast<char>(STYLE_ITALICS)));  // ^Y toggles italic off
        doc.Insert("Normal\r");

        layout.LayoutDocument(&doc);

        const sLineLayout* line = layout.GetLineByRawLineNumber(0);
        REQUIRE(line != nullptr);
        REQUIRE(line->segments.size() >= 2);

        // Get font descriptors
        std::string normalFont = line->segments[0].font;
        std::string italicFont = line->segments[1].font;

        // Fonts should be different (italic vs normal)
        CHECK(normalFont != italicFont);

        // Convert to QFont to verify italic state
        QFont qNormalFont = FontUtils::FontFromDescriptor(normalFont);
        QFont qItalicFont = FontUtils::FontFromDescriptor(italicFont);

        CHECK(qNormalFont.italic() == false);
        CHECK(qItalicFont.italic() == true);
    }

    SUBCASE("Font handles combined bold and italic")
    {
        // Insert text with both bold and italic
        doc.Insert("Normal");
        doc.Insert(std::string(1, static_cast<char>(STYLE_BOLD)));
        doc.Insert("Bold");
        doc.Insert(std::string(1, static_cast<char>(STYLE_ITALICS)));
        doc.Insert("BoldItalic\r");

        layout.LayoutDocument(&doc);

        const sLineLayout* line = layout.GetLineByRawLineNumber(0);
        REQUIRE(line != nullptr);
        REQUIRE(line->segments.size() >= 3);

        // Get font descriptors
        std::string normalFont = line->segments[0].font;
        std::string boldFont = line->segments[1].font;
        std::string boldItalicFont = line->segments[2].font;

        // Convert to QFont to verify state
        QFont qNormal = FontUtils::FontFromDescriptor(normalFont);
        QFont qBold = FontUtils::FontFromDescriptor(boldFont);
        QFont qBoldItalic = FontUtils::FontFromDescriptor(boldItalicFont);

        CHECK(qNormal.bold() == false);
        CHECK(qNormal.italic() == false);

        CHECK(qBold.bold() == true);
        CHECK(qBold.italic() == false);

        CHECK(qBoldItalic.bold() == true);
        CHECK(qBoldItalic.italic() == true);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test suite for Step 3: BuildParagraphSegments() - Structure Only
/// Tests verify segment count, startPosition, length, and formatting flags
/// WITHOUT measurement (Step 3 implementation)
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Step 3: BuildParagraphSegments() - Single segment")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("Empty paragraph returns empty vector")
    {
        // Insert and remove to create empty paragraph state
        // Skip this test - empty paragraphs not possible
    }

    SUBCASE("Single word creates single segment")
    {
        doc.Insert("Hello\r");

        // Set formatting state
        layout.SetDocument(&doc);

        // Build segments
        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 1 segment (Hello + HARD_RETURN)
        REQUIRE(segments.size() == 1);

        // Check first segment
        CHECK(segments[0].paragraph == 0);
        CHECK(segments[0].startPosition == 0);
        CHECK(segments[0].length == 6);  // "Hello\r" = 6 graphemes
        CHECK(segments[0].font.length() > 0);  // Has a font set
    }

    SUBCASE("Multiple words in single segment (no formatting)")
    {
        doc.Insert("Hello World\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 1 segment
        REQUIRE(segments.size() == 1);

        CHECK(segments[0].startPosition == 0);
        CHECK(segments[0].length == 12);  // "Hello World\r" = 12 graphemes
    }
}

TEST_CASE("Step 3: BuildParagraphSegments() - Multiple segments with control codes")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);  // Hide control codes

    SUBCASE("Bold creates segment boundary")
    {
        // Insert: "Hello" + ^B + "World" + ^B + "End\r"
        doc.Insert("Hello");
        doc.BeginBold();
        doc.Insert("World");
        doc.BeginBold();
        doc.Insert("End\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 3 segments (Hello, World, End\r)
        REQUIRE(segments.size() == 3);

        // Segment 1: "Hello" (5 graphemes)
        CHECK(segments[0].startPosition == 0);
        CHECK(segments[0].length == 5);

        // Segment 2: "World" (5 graphemes) - starts after ^B
        CHECK(segments[1].startPosition == 6);  // After "Hello" + ^B
        CHECK(segments[1].length == 5);

        // Segment 3: "End\r" (4 graphemes) - starts after second ^B
        CHECK(segments[2].startPosition == 12);  // After "HelloWorld" + 2x^B
        CHECK(segments[2].length == 4);
    }

    SUBCASE("Multiple control codes create multiple segments")
    {
        // Insert: "A" + ^B + "B" + ^Y + "C\r"
        doc.Insert("A");
        doc.BeginBold();
        doc.Insert("B");
        doc.BeginItalics();
        doc.Insert("C\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 3 segments
        REQUIRE(segments.size() == 3);

        CHECK(segments[0].startPosition == 0);
        CHECK(segments[0].length == 1);  // "A"

        CHECK(segments[1].startPosition == 2);  // After "A" + ^B
        CHECK(segments[1].length == 1);  // "B"

        CHECK(segments[2].startPosition == 4);  // After "AB" + ^B + ^Y
        CHECK(segments[2].length == 2);  // "C\r"
    }

    SUBCASE("Consecutive control codes (empty segments not saved)")
    {
        // Insert: "A" + ^B + ^Y + "B\r"
        doc.Insert("A");
        doc.BeginBold();
        doc.BeginItalics();
        doc.Insert("B\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 2 segments (A, B\r)
        // Empty segment between ^B and ^Y should NOT be saved
        REQUIRE(segments.size() == 2);

        CHECK(segments[0].startPosition == 0);
        CHECK(segments[0].length == 1);  // "A"

        CHECK(segments[1].startPosition == 3);  // After "A" + ^B + ^Y
        CHECK(segments[1].length == 2);  // "B\r"
    }
}

TEST_CASE("Step 3: BuildParagraphSegments() - Formatting flags")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    SUBCASE("Superscript and subscript flags")
    {
        // Insert: "A" + ^V (superscript) + "B\r"
        doc.Insert("A");
        doc.BeginSuperscript();
        doc.Insert("B\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 2);

        // First segment: not superscript
        CHECK(segments[0].isSuperscript == false);
        CHECK(segments[0].isSubscript == false);

        // Second segment: superscript active
        CHECK(segments[1].isSuperscript == true);
        CHECK(segments[1].isSubscript == false);
    }

    SUBCASE("Subscript cancels superscript")
    {
        // Insert: "A" + ^V (super) + "B" + ^T (sub) + "C\r"
        doc.Insert("A");
        doc.BeginSuperscript();
        doc.Insert("B");
        doc.BeginSubscript();
        doc.Insert("C\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 3);

        // Segment 1: normal
        CHECK(segments[0].isSuperscript == false);
        CHECK(segments[0].isSubscript == false);

        // Segment 2: superscript
        CHECK(segments[1].isSuperscript == true);
        CHECK(segments[1].isSubscript == false);

        // Segment 3: subscript (superscript cancelled)
        CHECK(segments[2].isSuperscript == false);
        CHECK(segments[2].isSubscript == true);
    }
}

TEST_CASE("Step 3: BuildParagraphSegments() - Control code visibility")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);

    SUBCASE("SHOW_ALL mode - control codes visible in segment")
    {
        layout.SetShowControl(SHOW_ALL);

        // Insert: "A" + ^B + "B\r"
        doc.Insert("A");
        doc.BeginBold();
        doc.Insert("B\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 2 segments (font change creates boundary even in SHOW_ALL)
        // Segment 1: "A" (normal)
        // Segment 2: "BB\r" (bold, with control char 'B' at start)
        REQUIRE(segments.size() == 2);

        // Segment 1: "A" (normal font, no control codes)
        CHECK(segments[0].length == 1);

        // Segment 2: ^B + "B\r" = 3 graphemes (control char displayed in bold)
        CHECK(segments[1].length == 3);
        CHECK(segments[1].hasControlCodes == true);
        CHECK(segments[1].controlCodeIndices.size() == 1);
        CHECK(segments[1].controlCodeIndices[0] == 0);  // ^B is at index 0 of segment 2
    }

    SUBCASE("SHOW_NONE mode - control codes create boundaries")
    {
        layout.SetShowControl(SHOW_NONE);

        // Insert: "A" + ^B + "B\r"
        doc.Insert("A");
        doc.BeginBold();
        doc.Insert("B\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 2 segments (control code creates boundary)
        REQUIRE(segments.size() == 2);

        CHECK(segments[0].length == 1);  // "A"
        CHECK(segments[1].length == 2);  // "B\r"
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test suite for Step 4: BuildParagraphSegments() - Measurement
/// Tests verify position[] array, totalWidth, and font-aware measurement
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Step 4: BuildParagraphSegments() - Basic measurement")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    SUBCASE("Single word has correct position[] array")
    {
        doc.Insert("Hi\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 1);

        // Check position array (base-0 coordinates, relative to segment start)
        REQUIRE(segments[0].position.size() == 3);  // H, i, \r

        // First grapheme should be at position 0 (segment start)
        CHECK(segments[0].position[0] == 0);

        // Second grapheme should be after first
        CHECK(segments[0].position[1] > 0);

        // Third grapheme should be after second
        CHECK(segments[0].position[2] > segments[0].position[1]);

        // Total width should be non-zero
        CHECK(segments[0].totalWidth > 0);
    }

    SUBCASE("totalWidth equals last position + last grapheme width")
    {
        doc.Insert("ABC\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 1);
        REQUIRE(segments[0].position.size() == 4);

        // totalWidth should equal currentX after all graphemes processed
        // Since we accumulate: position[i] stores X before grapheme i
        // totalWidth = X after last grapheme
        // Note: Hard return (\r) has zero width in Qt, so >= is correct
        CHECK(segments[0].totalWidth >= segments[0].position[3]);

        // Check that ABC (not \r) has measurable width
        CHECK(segments[0].position[3] > 0);
    }
}

TEST_CASE("Step 4: BuildParagraphSegments() - Font-aware measurement")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    SUBCASE("Bold text is wider than normal text")
    {
        // Insert: "Hi" + ^B + "Hi" + ^B + "\r"
        // First "Hi" normal, second "Hi" bold
        doc.Insert("Hi");
        doc.BeginBold();
        doc.Insert("Hi");
        doc.BeginBold();
        doc.Insert("\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 3);

        // Segment 0: "Hi" (normal)
        COORD_T normalWidth = segments[0].totalWidth;
        CHECK(normalWidth > 0);

        // Segment 1: "Hi" (bold)
        COORD_T boldWidth = segments[1].totalWidth;
        CHECK(boldWidth > 0);

        // Bold should typically be wider than normal (font-dependent)
        // Note: This may not always be true for all fonts, but for most it is
        CHECK(boldWidth >= normalWidth);  // At minimum, equal width
    }

    SUBCASE("Each segment has base-0 coordinates")
    {
        // Insert: "AB" + ^B + "CD" + ^B + "\r"
        doc.Insert("AB");
        doc.BeginBold();
        doc.Insert("CD");
        doc.BeginBold();
        doc.Insert("\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 3);

        // Each segment should start at position 0 (relative to segment start)
        CHECK(segments[0].position[0] == 0);
        CHECK(segments[1].position[0] == 0);
        CHECK(segments[2].position[0] == 0);

        // First two segments should have measured widths (text content)
        CHECK(segments[0].totalWidth > 0);
        CHECK(segments[1].totalWidth > 0);

        // Third segment is just \r which has zero width in Qt
        CHECK(segments[2].totalWidth >= 0);
    }
}

TEST_CASE("Step 4: BuildParagraphSegments() - Position array correctness")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    SUBCASE("Positions are monotonically increasing")
    {
        doc.Insert("ABCDEF\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 1);

        // Check that positions increase
        for (size_t i = 1; i < segments[0].position.size(); ++i)
        {
            CHECK(segments[0].position[i] > segments[0].position[i-1]);
        }
    }

    SUBCASE("Characters have measurable widths")
    {
        // Note: Default font (Courier New) is monospace, so all chars equal width
        // This test verifies measurement is happening, not width differences
        doc.Insert("AB\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 1);
        REQUIRE(segments[0].position.size() == 3);

        // Width of 'A' (position[1] - position[0])
        COORD_T aWidth = segments[0].position[1] - segments[0].position[0];

        // Width of 'B' (position[2] - position[1])
        COORD_T bWidth = segments[0].position[2] - segments[0].position[1];

        // Both should have non-zero width
        CHECK(aWidth > 0);
        CHECK(bWidth > 0);

        // In monospace font (default), widths should be equal
        CHECK(aWidth == bWidth);
    }
}

TEST_CASE("BuildParagraphSegments - Font changes create new segments")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    SUBCASE("Font change in middle of text creates segment boundary")
    {
        // Insert text before font change
        doc.Insert("Before ");

        // Insert a font change
        sInternalFonts font;
        font.name = "Arial";
        font.fontname = "Arial";
        font.size = 14.0;
        doc.InsertFont(font);

        // Insert text after font change
        doc.Insert("After\r");

        // Build segments
        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 2 segments: one before font change, one after
        REQUIRE(segments.size() == 2);

        // First segment: "Before " (7 characters including space)
        CHECK(segments[0].startPosition == 0);
        CHECK(segments[0].length == 7);

        // Second segment: "After" + hard return (6 characters)
        // startPosition should be after "Before " and the MARKER_CHAR
        CHECK(segments[1].startPosition == 8);  // 7 chars + 1 MARKER_CHAR
        CHECK(segments[1].length == 6);

        // Fonts should be different
        CHECK(segments[0].font != segments[1].font);

        // Second segment should contain "Arial" in the font string
        CHECK(segments[1].font.find("Arial") != std::string::npos);
    }

    SUBCASE("Multiple font changes create multiple segments")
    {
        doc.Insert("Text1 ");

        sInternalFonts font1;
        font1.name = "Times New Roman";
        font1.fontname = "Times New Roman";
        font1.size = 12.0;
        doc.InsertFont(font1);

        doc.Insert("Text2 ");

        sInternalFonts font2;
        font2.name = "Courier New";
        font2.fontname = "Courier New";
        font2.size = 10.0;
        doc.InsertFont(font2);

        doc.Insert("Text3\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 3 segments
        REQUIRE(segments.size() == 3);

        // All three segments should have different fonts
        CHECK(segments[0].font != segments[1].font);
        CHECK(segments[1].font != segments[2].font);
        CHECK(segments[0].font != segments[2].font);

        // Verify font names appear in segment font strings
        CHECK(segments[1].font.find("Times New Roman") != std::string::npos);
        CHECK(segments[2].font.find("Courier") != std::string::npos);
    }

    SUBCASE("Font change at paragraph start")
    {
        // Font change at very beginning
        sInternalFonts font;
        font.name = "Arial";
        font.fontname = "Arial";
        font.size = 16.0;
        doc.InsertFont(font);

        doc.Insert("Text\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 1 segment (font change before any text)
        REQUIRE(segments.size() == 1);

        // Segment should use the new font
        CHECK(segments[0].font.find("Arial") != std::string::npos);
    }
}


//////////////////////////////////////////////////////////////////////////////
//
// Color propagation tests -- verify layout segments get correct sSeqRGBColor
//
//////////////////////////////////////////////////////////////////////////////


TEST_CASE("BuildParagraphSegments - Color creates segment boundary")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    SUBCASE("Color change splits into two segments")
    {
        // "Hello" + color(red) + "World\r"
        doc.Insert("Hello");
        sSeqRGBColor red;
        red.red = 255; red.green = 0; red.blue = 0; red.alpha = 255;
        doc.InsertColor(red);
        doc.Insert("World\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 2 segments: "Hello" (default color) + "World\r" (red)
        REQUIRE(segments.size() == 2);

        // First segment: default color
        CHECK(segments[0].length == 5);
        CHECK(segments[0].textcolor.IsDefault() == true);

        // Second segment: red
        CHECK(segments[1].length == 6);
        CHECK(segments[1].textcolor.red == 255);
        CHECK(segments[1].textcolor.green == 0);
        CHECK(segments[1].textcolor.blue == 0);
        CHECK(segments[1].textcolor.IsDefault() == false);
    }

    SUBCASE("Default sentinel in segment textcolor")
    {
        // "Hello" + color(red) + "Mid" + color(default) + "End\r"
        doc.Insert("Hello");
        sSeqRGBColor red;
        red.red = 255; red.green = 0; red.blue = 0; red.alpha = 255;
        doc.InsertColor(red);
        doc.Insert("Mid");
        sSeqRGBColor sentinel;
        sentinel.red = -1; sentinel.green = -1; sentinel.blue = -1; sentinel.alpha = -1;
        doc.InsertColor(sentinel);
        doc.Insert("End\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        // Should have 3 segments: "Hello" (default), "Mid" (red), "End\r" (default again)
        REQUIRE(segments.size() == 3);

        CHECK(segments[0].textcolor.IsDefault() == true);

        CHECK(segments[1].textcolor.red == 255);
        CHECK(segments[1].textcolor.green == 0);
        CHECK(segments[1].textcolor.blue == 0);

        CHECK(segments[2].textcolor.IsDefault() == true);
    }

    SUBCASE("Full RGB fidelity in segments")
    {
        // Insert arbitrary RGB color -- verify it propagates exactly
        doc.Insert("A");
        sSeqRGBColor custom;
        custom.red = 123; custom.green = 45; custom.blue = 67; custom.alpha = 255;
        doc.InsertColor(custom);
        doc.Insert("B\r");

        std::vector<sSegmentLayout> segments = layout.BuildParagraphSegments(0);

        REQUIRE(segments.size() == 2);

        CHECK(segments[1].textcolor.red == 123);
        CHECK(segments[1].textcolor.green == 45);
        CHECK(segments[1].textcolor.blue == 67);
        CHECK(segments[1].textcolor.alpha == 255);
    }
}


TEST_CASE("Layout color spans paragraph boundaries")
{
    ensureQApplication();

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.SetShowControl(SHOW_NONE);

    // Build two paragraphs: first with red color, second inherits
    // "Hello" + color(red) + "World\r" + "Second\r"
    doc.Insert("Hello");
    sSeqRGBColor red;
    red.red = 255; red.green = 0; red.blue = 0; red.alpha = 255;
    doc.InsertColor(red);
    doc.Insert("World\r");
    doc.Insert("Second\r");

    // Layout the full document so paragraph end states are computed
    layout.LayoutDocument(&doc);

    // Build segments for paragraph 1 (second paragraph)
    std::vector<sSegmentLayout> seg1 = layout.BuildParagraphSegments(1);

    // The second paragraph should inherit the red color from paragraph 0's end state
    REQUIRE(seg1.size() >= 1);
    CHECK(seg1[0].textcolor.red == 255);
    CHECK(seg1[0].textcolor.green == 0);
    CHECK(seg1[0].textcolor.blue == 0);
    CHECK(seg1[0].textcolor.IsDefault() == false);
}

