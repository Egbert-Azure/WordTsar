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

#include "src/tui/layout/tuitextmeasurement.h"
#include "src/tui/layout/tuifontutils.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for cTUITextMeasurement -- the TUI text measurement class.
/// Verifies text width, font height, line spacing, and font
/// descriptor handling.
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("TUI text measurement - initialization")
{
    cTUITextMeasurement measure;

    SUBCASE("Initialize succeeds")
    {
        bool ok = measure.Initialize();
        CHECK(ok);
        measure.Shutdown();
    }

    SUBCASE("Shutdown without initialize does not crash")
    {
        measure.Shutdown();
    }

    SUBCASE("GetFontManager returns non-null after init")
    {
        REQUIRE(measure.Initialize());
        cTUIFontManager* mgr = measure.GetFontManager();
        CHECK(mgr != nullptr);
        measure.Shutdown();
    }
}


TEST_CASE("TUI text measurement - set font")
{
    cTUITextMeasurement measure;
    REQUIRE(measure.Initialize());

    SUBCASE("Set font by family and size")
    {
        // Try common fonts, fall back to whatever is available
        bool ok = measure.SetFont("Courier New", 12.0, false, false);
        if (!ok)
        {
            ok = measure.SetFont("DejaVu Sans Mono", 12.0, false, false);
        }
        if (!ok)
        {
            // Use font manager to find any available font
            cTUIFontManager* mgr = measure.GetFontManager();
            REQUIRE(mgr != nullptr);
            auto families = mgr->GetFontFamilies();
            REQUIRE(families.size() > 0);
            ok = measure.SetFont(families[0], 12.0, false, false);
        }
        CHECK(ok);
    }

    SUBCASE("Set font from descriptor string")
    {
        // Pipe-delimited descriptor format
        bool ok = measure.SetFontFromDescriptor("Courier New|12|0|0|0|0|0");
        // May fail if Courier New not available -- that's OK
        if (!ok)
        {
            // Build descriptor from first available font
            cTUIFontManager* mgr = measure.GetFontManager();
            REQUIRE(mgr != nullptr);
            auto families = mgr->GetFontFamilies();
            REQUIRE(families.size() > 0);
            std::string desc = TUIFontUtils::FontToDescriptor(
                families[0], 12.0, false, false, false);
            ok = measure.SetFontFromDescriptor(desc);
        }
        CHECK(ok);
    }

    measure.Shutdown();
}


TEST_CASE("TUI text measurement - text width")
{
    cTUITextMeasurement measure;
    REQUIRE(measure.Initialize());

    // Set a font
    bool fontSet = measure.SetFont("Courier New", 12.0);
    if (!fontSet)
    {
        cTUIFontManager* mgr = measure.GetFontManager();
        auto families = mgr->GetFontFamilies();
        REQUIRE(families.size() > 0);
        fontSet = measure.SetFont(families[0], 12.0);
    }
    REQUIRE(fontSet);

    SUBCASE("Width of non-empty text is positive")
    {
        COORD_T width = measure.GetTextWidth("Hello");
        CHECK(width > 0);
    }

    SUBCASE("Width of empty text is zero")
    {
        COORD_T width = measure.GetTextWidth("");
        CHECK(width == 0);
    }

    SUBCASE("Width of single char is positive")
    {
        COORD_T width = measure.GetTextWidth("A");
        CHECK(width > 0);
    }

    SUBCASE("Longer text has greater or equal width")
    {
        COORD_T w1 = measure.GetTextWidth("A");
        COORD_T w3 = measure.GetTextWidth("AAA");
        CHECK(w3 >= w1);
    }

    SUBCASE("Width scales roughly linearly for monospace")
    {
        // If we have Courier New (monospace), 5 chars ~= 5 * 1 char
        COORD_T w1 = measure.GetTextWidth("A");
        COORD_T w5 = measure.GetTextWidth("AAAAA");
        if (w1 > 0)
        {
            // Allow 10% tolerance for rounding
            double ratio = static_cast<double>(w5) / static_cast<double>(w1);
            CHECK(ratio >= 4.0);
            CHECK(ratio <= 6.0);
        }
    }

    measure.Shutdown();
}


TEST_CASE("TUI text measurement - font height and line spacing")
{
    cTUITextMeasurement measure;
    REQUIRE(measure.Initialize());

    // Set a font
    bool fontSet = measure.SetFont("Courier New", 12.0);
    if (!fontSet)
    {
        cTUIFontManager* mgr = measure.GetFontManager();
        auto families = mgr->GetFontFamilies();
        REQUIRE(families.size() > 0);
        fontSet = measure.SetFont(families[0], 12.0);
    }
    REQUIRE(fontSet);

    SUBCASE("Font height is positive")
    {
        COORD_T height = measure.GetFontHeight();
        CHECK(height > 0);
    }

    SUBCASE("Line spacing is positive")
    {
        COORD_T spacing = measure.GetFontLineSpacing();
        CHECK(spacing > 0);
    }

    SUBCASE("Line spacing >= font height")
    {
        COORD_T height = measure.GetFontHeight();
        COORD_T spacing = measure.GetFontLineSpacing();
        CHECK(spacing >= height);
    }

    SUBCASE("Larger font has larger height")
    {
        COORD_T h12 = measure.GetFontHeight();

        // Set a larger font
        cTUIFontManager* mgr = measure.GetFontManager();
        auto families = mgr->GetFontFamilies();
        REQUIRE(families.size() > 0);
        bool ok = measure.SetFont(families[0], 24.0);
        if (ok)
        {
            COORD_T h24 = measure.GetFontHeight();
            CHECK(h24 > h12);
        }
    }

    measure.Shutdown();
}


TEST_CASE("TUI text measurement - bold does not crash")
{
    cTUITextMeasurement measure;
    REQUIRE(measure.Initialize());

    cTUIFontManager* mgr = measure.GetFontManager();
    auto families = mgr->GetFontFamilies();
    REQUIRE(families.size() > 0);

    // Set regular
    bool ok = measure.SetFont(families[0], 12.0, false, false);
    REQUIRE(ok);
    COORD_T wRegular = measure.GetTextWidth("Hello");

    // Set bold -- should not crash regardless of whether bold variant exists
    ok = measure.SetFont(families[0], 12.0, true, false);
    if (ok)
    {
        COORD_T wBold = measure.GetTextWidth("Hello");
        CHECK(wBold > 0);
    }

    // Set italic -- should not crash
    ok = measure.SetFont(families[0], 12.0, false, true);
    if (ok)
    {
        COORD_T wItalic = measure.GetTextWidth("Hello");
        CHECK(wItalic > 0);
    }

    (void)wRegular;  // Used only for comparison context
    measure.Shutdown();
}
