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

#include "src/tui/fonts/fontmanager.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for cTUIFontManager -- the TUI font system manager.
/// Verifies initialization, backend detection, font discovery,
/// and text measurement.
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("TUI font manager - initialization")
{
    cTUIFontManager mgr;

    SUBCASE("Initialize in document mode succeeds")
    {
        bool ok = mgr.Initialize(true);
        CHECK(ok);
        mgr.Shutdown();
    }

    SUBCASE("Initialize in terminal mode succeeds")
    {
        bool ok = mgr.Initialize(false);
        CHECK(ok);
        mgr.Shutdown();
    }

    SUBCASE("Double initialization does not crash")
    {
        bool ok1 = mgr.Initialize(true);
        bool ok2 = mgr.Initialize(true);
        CHECK(ok1);
        CHECK(ok2);
        mgr.Shutdown();
    }

    SUBCASE("Shutdown without initialize does not crash")
    {
        mgr.Shutdown();
    }
}


TEST_CASE("TUI font manager - backend detection")
{
    cTUIFontManager mgr;
    REQUIRE(mgr.Initialize(true));

    SUBCASE("Backend is not zero/uninitialized")
    {
        eTUIFontMeasurementBackend backend = mgr.GetCurrentBackend();
        // Should be one of the valid backends
        CHECK(backend >= FONT_BACKEND_DIRECTWRITE);
        CHECK(backend <= FONT_BACKEND_BUILT_IN_METRICS);
    }

    SUBCASE("Backend name is non-empty")
    {
        std::string name = mgr.GetBackendName();
        CHECK(!name.empty());
    }

    SUBCASE("Available backends list is non-empty")
    {
        auto backends = mgr.GetAvailableBackends();
        CHECK(backends.size() > 0);
    }

    mgr.Shutdown();
}


TEST_CASE("TUI font manager - font discovery")
{
    cTUIFontManager mgr;
    REQUIRE(mgr.Initialize(true));

    SUBCASE("Available fonts list is non-empty")
    {
        auto fonts = mgr.GetAvailableFonts();
        CHECK(fonts.size() > 0);
    }

    SUBCASE("Font families list is non-empty")
    {
        auto families = mgr.GetFontFamilies();
        CHECK(families.size() > 0);
    }

    SUBCASE("At least one monospace font exists")
    {
        auto monoFonts = mgr.GetMonospaceFonts();
        CHECK(monoFonts.size() > 0);
    }

    SUBCASE("Built-in or system fonts available")
    {
        // On systems with HarfBuzz/FreeType, built-in fonts may be zero
        // since real font backends are preferred. Check either category.
        auto builtIn = mgr.GetBuiltInFonts();
        auto system = mgr.GetSystemFonts();
        CHECK((builtIn.size() > 0 || system.size() > 0));
    }

    mgr.Shutdown();
}


#ifdef __APPLE__
TEST_CASE("TUI font manager - macOS system font discovery finds real files")
{
    // Regression test for a real gap: DiscoverSystemFonts() had no macOS
    // branch and fell through to a Linux-only directory scan (/usr/share/fonts,
    // etc.), which never finds anything on a Mac. Print then only "worked" via
    // tuiprintout.cpp's own standard-14 PostScript-name fallback -- any other
    // real installed font never resolved to a real file for embedding. This
    // confirms the CoreText-based discovery actually populates real paths.
    cTUIFontManager mgr;
    REQUIRE(mgr.Initialize(true));
    mgr.WaitForFontEnumeration();

    auto allFonts = mgr.GetAvailableFonts();

    bool foundRealFilePath = false;
    for (const auto& font : allFonts)
    {
        if (!font.fullName.empty() && font.fullName.find('/') != std::string::npos)
        {
            foundRealFilePath = true;
            break;
        }
    }
    CHECK(foundRealFilePath);

    mgr.Shutdown();
}
#endif


TEST_CASE("TUI font manager - set font")
{
    cTUIFontManager mgr;
    REQUIRE(mgr.Initialize(true));

    SUBCASE("Set font by family and size")
    {
        // Try Courier New first, fall back to any available font
        bool ok = mgr.SetFont("Courier New", 12, false, false);
        if (!ok)
        {
            // Fall back to first available font
            auto families = mgr.GetFontFamilies();
            REQUIRE(families.size() > 0);
            ok = mgr.SetFont(families[0], 12, false, false);
        }
        CHECK(ok);
    }

    SUBCASE("Set font with criteria struct")
    {
        sTUIFontSelectionCriteria criteria;
        criteria.size = 12;
        criteria.bold = false;
        criteria.italic = false;

        // Try Courier New first
        criteria.family = "Courier New";
        bool ok = mgr.SetFont(criteria);
        if (!ok)
        {
            auto families = mgr.GetFontFamilies();
            REQUIRE(families.size() > 0);
            criteria.family = families[0];
            ok = mgr.SetFont(criteria);
        }
        CHECK(ok);
    }

    SUBCASE("Current font info is valid after SetFont")
    {
        auto families = mgr.GetFontFamilies();
        REQUIRE(families.size() > 0);
        mgr.SetFont(families[0], 12);

        sTUIFontInfo info = mgr.GetCurrentFont();
        CHECK(!info.name.empty());
        CHECK(info.size > 0);
    }

    mgr.Shutdown();
}


TEST_CASE("TUI font manager - text measurement")
{
    cTUIFontManager mgr;
    REQUIRE(mgr.Initialize(true));

    // Set a font (use first available if Courier New not found)
    bool fontSet = mgr.SetFont("Courier New", 12);
    if (!fontSet)
    {
        auto families = mgr.GetFontFamilies();
        REQUIRE(families.size() > 0);
        fontSet = mgr.SetFont(families[0], 12);
    }
    REQUIRE(fontSet);

    SUBCASE("MeasureText returns positive width")
    {
        sTUITextMetrics metrics = mgr.MeasureText("Hello");
        CHECK(metrics.widthTWIPS > 0);
    }

    SUBCASE("MeasureText of empty string returns zero width")
    {
        sTUITextMetrics metrics = mgr.MeasureText("");
        CHECK(metrics.widthTWIPS == 0);
    }

    SUBCASE("Longer text has greater width")
    {
        sTUITextMetrics m1 = mgr.MeasureText("A");
        sTUITextMetrics m5 = mgr.MeasureText("AAAAA");
        CHECK(m5.widthTWIPS > m1.widthTWIPS);
    }

    SUBCASE("Line height is positive")
    {
        int height = mgr.GetLineHeight();
        CHECK(height > 0);
    }

    SUBCASE("Character width is positive for printable char")
    {
        int width = mgr.MeasureCharacterWidth('A');
        CHECK(width > 0);
    }

    mgr.Shutdown();
}


TEST_CASE("TUI font manager - default font helpers")
{
    SUBCASE("Default font family is non-empty")
    {
        std::string def = cTUIFontManager::GetDefaultFontFamily();
        CHECK(!def.empty());
    }

    SUBCASE("Default monospace font family is non-empty")
    {
        std::string def = cTUIFontManager::GetDefaultFontFamily(true);
        CHECK(!def.empty());
    }

    SUBCASE("Default font size is reasonable")
    {
        int size = cTUIFontManager::GetDefaultFontSize();
        CHECK(size >= 6);
        CHECK(size <= 72);
    }

    SUBCASE("Common font sizes list is non-empty")
    {
        auto sizes = cTUIFontManager::GetCommonFontSizes();
        CHECK(sizes.size() > 0);
    }
}
