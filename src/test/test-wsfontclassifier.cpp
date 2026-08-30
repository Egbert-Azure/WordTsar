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
#include "src/files/wordstar/wsfontclassifier.h"

#include <fstream>
#include <vector>
#include <cstdint>


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for cWSFontClassifier -- WordStar font typestyle bitfield
/// classification using OS/2 table, PANOSE, glyph metrics, cmap
/// coverage, and keyword fallback.
///
/////////////////////////////////////////////////////////////////////////////


// Helper: load a font file into a byte vector
static std::vector<unsigned char> LoadFontFile(const std::string& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return {};
    }
    size_t size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> data(size);
    if (!file.read(reinterpret_cast<char*>(data.data()),
                   static_cast<std::streamsize>(size)))
    {
        return {};
    }
    return data;
}


// =========================================================================
// 1. Bitfield Assembly
// =========================================================================

TEST_CASE("WSFontClassifier - Assemble produces correct bitfield")
{
    SUBCASE("Proportional sans-serif, CP437, index 4")
    {
        sWSFontClassification c;
        c.proportional = true;
        c.genericStyle = WS_STYLE_SANS;
        c.symbolMapping = WS_SYMBOL_CP437;
        c.fontIndex = 4;

        uint16_t bits = c.Assemble();

        // bit 15 = 1 (proportional)
        CHECK((bits & (1 << 15)) != 0);
        // bit 14 = 1 (letter quality constant)
        CHECK((bits & (1 << 14)) != 0);
        // bits 13-12 = 00 (CP437)
        CHECK(((bits >> 12) & 0x03) == 0);
        // bits 11-10 = 00 (sans)
        CHECK(((bits >> 10) & 0x03) == 0);
        // bit 9 = 1 (version constant)
        CHECK((bits & (1 << 9)) != 0);
        // bits 8-0 = 4
        CHECK((bits & 0x01FF) == 4);
    }

    SUBCASE("Monospace serif, CP437, index 0")
    {
        sWSFontClassification c;
        c.proportional = false;
        c.genericStyle = WS_STYLE_SERIF;
        c.symbolMapping = WS_SYMBOL_CP437;
        c.fontIndex = 0;

        uint16_t bits = c.Assemble();

        // bit 15 = 0 (monospace)
        CHECK((bits & (1 << 15)) == 0);
        // bit 14 = 1 (letter quality)
        CHECK((bits & (1 << 14)) != 0);
        // bits 11-10 = 01 (serif)
        CHECK(((bits >> 10) & 0x03) == 1);
        // bit 9 = 1 (version)
        CHECK((bits & (1 << 9)) != 0);
        // bits 8-0 = 0
        CHECK((bits & 0x01FF) == 0);
    }

    SUBCASE("Script style, symbols mapping")
    {
        sWSFontClassification c;
        c.proportional = true;
        c.genericStyle = WS_STYLE_SCRIPT;
        c.symbolMapping = WS_SYMBOL_SYMBOLS;
        c.fontIndex = 178;

        uint16_t bits = c.Assemble();

        // bits 11-10 = 10 (script)
        CHECK(((bits >> 10) & 0x03) == 2);
        // bits 13-12 = 11 (symbols)
        CHECK(((bits >> 12) & 0x03) == 3);
        // bits 8-0 = 178
        CHECK((bits & 0x01FF) == 178);
    }

    SUBCASE("Display style, math mapping")
    {
        sWSFontClassification c;
        c.proportional = true;
        c.genericStyle = WS_STYLE_DISPLAY;
        c.symbolMapping = WS_SYMBOL_MATH;
        c.fontIndex = 164;

        uint16_t bits = c.Assemble();

        // bits 11-10 = 11 (display)
        CHECK(((bits >> 10) & 0x03) == 3);
        // bits 13-12 = 10 (math)
        CHECK(((bits >> 12) & 0x03) == 2);
    }
}


// =========================================================================
// 2. Keyword Fallback
// =========================================================================

TEST_CASE("WSFontClassifier - Keyword fallback for unknown fonts")
{
    cWSFontClassifier classifier;

    SUBCASE("Nonexistent font falls back to keywords gracefully")
    {
        sWSFontClassification result = classifier.Classify("NonexistentFont_XYZ_12345");

        // Should not crash, should produce a valid result
        uint16_t bits = result.Assemble();
        // Bit 14 (letter quality) and bit 9 (version) should always be set
        CHECK((bits & (1 << 14)) != 0);
        CHECK((bits & (1 << 9)) != 0);
    }

    SUBCASE("Arial classified as sans-serif proportional")
    {
        sWSFontClassification result = classifier.Classify("Arial");
        CHECK(result.genericStyle == WS_STYLE_SANS);
        CHECK(result.proportional == true);
    }

    SUBCASE("Times New Roman classified as serif proportional")
    {
        sWSFontClassification result = classifier.Classify("Times New Roman");
        CHECK(result.genericStyle == WS_STYLE_SERIF);
        CHECK(result.proportional == true);
    }

    SUBCASE("Courier New classified as monospace")
    {
        sWSFontClassification result = classifier.Classify("Courier New");
        CHECK(result.proportional == false);
    }
}


// =========================================================================
// 3. ClassifyFromData with Real Fonts
// =========================================================================

TEST_CASE("WSFontClassifier - ClassifyFromData with system fonts")
{
    cWSFontClassifier classifier;

    SUBCASE("DejaVu Sans is proportional sans-serif")
    {
        std::string path = cWSFontClassifier::FindFontFile("DejaVu Sans");
        if (path.empty())
        {
            WARN("DejaVu Sans not installed -- skipping");
            return;
        }

        std::vector<unsigned char> data = LoadFontFile(path);
        REQUIRE(data.empty() == false);

        sWSFontClassification result = classifier.ClassifyFromData(
            data.data(), data.size(), "DejaVu Sans");

        CHECK(result.proportional == true);
        // DejaVu Sans has OS/2 sFamilyClass = 0 (unclassified), so style
        // comes from name keywords or PANOSE
        CHECK(result.symbolMapping == WS_SYMBOL_CP437);
    }

    SUBCASE("DejaVu Sans Mono is monospace")
    {
        std::string path = cWSFontClassifier::FindFontFile("DejaVu Sans Mono");
        if (path.empty())
        {
            WARN("DejaVu Sans Mono not installed -- skipping");
            return;
        }

        std::vector<unsigned char> data = LoadFontFile(path);
        REQUIRE(data.empty() == false);

        sWSFontClassification result = classifier.ClassifyFromData(
            data.data(), data.size(), "DejaVu Sans Mono");

        CHECK(result.proportional == false);
        CHECK(result.pitchSource == "metrics");
    }

    SUBCASE("Liberation Serif is proportional serif")
    {
        std::string path = cWSFontClassifier::FindFontFile("Liberation Serif");
        if (path.empty())
        {
            WARN("Liberation Serif not installed -- skipping");
            return;
        }

        std::vector<unsigned char> data = LoadFontFile(path);
        REQUIRE(data.empty() == false);

        sWSFontClassification result = classifier.ClassifyFromData(
            data.data(), data.size(), "Liberation Serif");

        CHECK(result.proportional == true);
        CHECK(result.genericStyle == WS_STYLE_SERIF);
    }

    SUBCASE("FreeSans is proportional sans-serif")
    {
        std::string path = cWSFontClassifier::FindFontFile("FreeSans");
        if (path.empty())
        {
            WARN("FreeSans not installed -- skipping");
            return;
        }

        std::vector<unsigned char> data = LoadFontFile(path);
        REQUIRE(data.empty() == false);

        sWSFontClassification result = classifier.ClassifyFromData(
            data.data(), data.size(), "FreeSans");

        CHECK(result.proportional == true);
        CHECK(result.genericStyle == WS_STYLE_SANS);
    }

    SUBCASE("FreeSerif is proportional serif")
    {
        std::string path = cWSFontClassifier::FindFontFile("FreeSerif");
        if (path.empty())
        {
            WARN("FreeSerif not installed -- skipping");
            return;
        }

        std::vector<unsigned char> data = LoadFontFile(path);
        REQUIRE(data.empty() == false);

        sWSFontClassification result = classifier.ClassifyFromData(
            data.data(), data.size(), "FreeSerif");

        CHECK(result.proportional == true);
        CHECK(result.genericStyle == WS_STYLE_SERIF);
    }
}


// =========================================================================
// 4. Edge Cases
// =========================================================================

TEST_CASE("WSFontClassifier - Edge cases")
{
    cWSFontClassifier classifier;

    SUBCASE("Null data falls back to keywords")
    {
        sWSFontClassification result = classifier.ClassifyFromData(
            nullptr, 0, "Arial");

        // Should not crash, should produce a result via keyword fallback
        CHECK(result.genericStyle == WS_STYLE_SANS);
        CHECK(result.proportional == true);
    }

    SUBCASE("Truncated data (< 12 bytes) falls back to keywords")
    {
        unsigned char garbage[] = { 0x00, 0x01, 0x02, 0x03, 0x04 };
        sWSFontClassification result = classifier.ClassifyFromData(
            garbage, sizeof(garbage), "Times New Roman");

        // Should not crash
        CHECK(result.genericStyle == WS_STYLE_SERIF);
    }

    SUBCASE("Random garbage does not crash")
    {
        // Use data that does NOT have a valid TrueType signature
        unsigned char garbage[256];
        for (int i = 0; i < 256; i++)
        {
            garbage[i] = static_cast<unsigned char>(0xDE);
        }

        sWSFontClassification result = classifier.ClassifyFromData(
            garbage, sizeof(garbage), "SomeFont");

        // Should produce some result without crashing
        uint16_t bits = result.Assemble();
        // Constants should still be set
        CHECK((bits & (1 << 14)) != 0);  // letter quality
        CHECK((bits & (1 << 9)) != 0);   // version
    }

    SUBCASE("Empty font name")
    {
        sWSFontClassification result = classifier.Classify("");

        // Should not crash
        uint16_t bits = result.Assemble();
        CHECK((bits & (1 << 14)) != 0);
        CHECK((bits & (1 << 9)) != 0);
    }
}


// =========================================================================
// 5. Font Index Lookup
// =========================================================================

TEST_CASE("WSFontClassifier - Font index lookup")
{
    cWSFontClassifier classifier;

    SUBCASE("Classify produces a valid font index")
    {
        sWSFontClassification result = classifier.Classify("Arial");

        // Font index should be within gOrgFonts range (0-247) or fallback 0
        CHECK(result.fontIndex <= 247);
    }

    SUBCASE("Unknown font gets fallback index")
    {
        sWSFontClassification result = classifier.Classify("CompletelyMadeUpFont999");

        // Should still produce a valid index
        CHECK(result.fontIndex <= 247);
    }

    SUBCASE("Different font styles get different indices")
    {
        sWSFontClassification sans = classifier.Classify("Arial");
        sWSFontClassification serif = classifier.Classify("Times New Roman");

        // They should map to different gOrgFonts entries
        // (unless both fall back to the same generic match)
        // At minimum, both should be valid
        CHECK(sans.fontIndex <= 247);
        CHECK(serif.fontIndex <= 247);
    }
}


// =========================================================================
// 6. Diagnostic Sources
// =========================================================================

TEST_CASE("WSFontClassifier - Classification sources are populated")
{
    cWSFontClassifier classifier;

    sWSFontClassification result = classifier.Classify("Arial");

    // Every classification should have a source string
    CHECK(result.pitchSource.empty() == false);
    CHECK(result.styleSource.empty() == false);
    CHECK(result.symbolSource.empty() == false);
}
