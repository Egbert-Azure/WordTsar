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
#include "src/core/utils/config.h"

#include <cstdlib>
#include <filesystem>
#include <string>


/////////////////////////////////////////////////////////////////////////////
///
/// @brief Helper RAII class to temporarily override XDG_CONFIG_HOME
/// so that palette tests write to a temp directory instead of ~/.config.
///
/////////////////////////////////////////////////////////////////////////////
class TempConfigDir
{
public:
    TempConfigDir()
    {
        // Create a unique temp directory for this test run
        mTempDir = std::filesystem::temp_directory_path() / "wordtsar-test-palettes";
        std::filesystem::create_directories(mTempDir);

        // Save existing XDG_CONFIG_HOME (if any) and override
        const char* existing = std::getenv("XDG_CONFIG_HOME");
        if (existing != nullptr)
        {
            mOldValue = existing;
            mHadOldValue = true;
        }
        setenv("XDG_CONFIG_HOME", mTempDir.c_str(), 1);
    }

    ~TempConfigDir()
    {
        // Restore original XDG_CONFIG_HOME
        if (mHadOldValue)
        {
            setenv("XDG_CONFIG_HOME", mOldValue.c_str(), 1);
        }
        else
        {
            unsetenv("XDG_CONFIG_HOME");
        }

        // Clean up temp directory
        std::filesystem::remove_all(mTempDir);
    }

private:
    std::filesystem::path mTempDir;
    std::string mOldValue;
    bool mHadOldValue = false;
};


/////////////////////////////////////////////////////////////////////////////
// Helper: compare two sRGB values
/////////////////////////////////////////////////////////////////////////////
static void CheckRGB(const sRGB& actual, const sRGB& expected, const char* label)
{
    INFO(label);
    CHECK(actual.r == expected.r);
    CHECK(actual.g == expected.g);
    CHECK(actual.b == expected.b);
}


TEST_SUITE("cConfig - Custom Palettes")
{

TEST_CASE("LoadCustomGUIPalettes returns empty when file missing")
{
    TempConfigDir tmpDir;

    auto palettes = cConfig::LoadCustomGUIPalettes();
    CHECK(palettes.empty());
}


TEST_CASE("LoadCustomTUIPalettes returns empty when file missing")
{
    TempConfigDir tmpDir;

    auto palettes = cConfig::LoadCustomTUIPalettes();
    CHECK(palettes.empty());
}


TEST_CASE("GUI palette round-trip: save and load")
{
    TempConfigDir tmpDir;

    // Build a test palette with distinctive values
    sGUIPalette original;
    original.name = "Test Theme";
    original.background                   = {10, 20, 30};
    original.foreground                   = {200, 210, 220};
    original.highlightBackground          = {40, 50, 60};
    original.highlightForeground          = {230, 240, 250};
    original.dotBackground                = {70, 80, 90};
    original.dotForeground                = {100, 110, 120};
    original.blockBackground              = {11, 22, 33};
    original.blockForeground              = {44, 55, 66};
    original.commentBackground            = {77, 88, 99};
    original.commentForeground            = {111, 122, 133};
    original.errorBackground              = {144, 155, 166};
    original.errorForeground              = {177, 188, 199};
    original.unknownBackground            = {201, 202, 203};
    original.unknownForeground            = {204, 205, 206};
    original.notImplementedBackground     = {207, 208, 209};
    original.notImplementedForeground     = {210, 211, 212};
    original.searchBackground             = {213, 214, 215};
    original.searchForeground             = {216, 217, 218};
    original.statusBarForeground          = {1, 2, 3};
    original.statusBarBackground          = {4, 5, 6};
    original.helpPanelForeground          = {7, 8, 9};
    original.helpPanelBackground          = {10, 11, 12};
    original.helpPanelKeystrokeForeground = {13, 14, 15};
    original.helpPanelKeystrokeBackground = {16, 17, 18};
    original.rulerForeground              = {19, 20, 21};
    original.rulerBackground              = {22, 23, 24};

    // Save it
    bool saved = cConfig::SaveCustomGUIPalette(original);
    CHECK(saved);

    // Load it back
    auto palettes = cConfig::LoadCustomGUIPalettes();
    REQUIRE(palettes.size() == 1);
    CHECK(palettes[0].name == "Test Theme");

    // Verify all 26 color values
    const auto& loaded = palettes[0];
    CheckRGB(loaded.background,                   original.background,                   "background");
    CheckRGB(loaded.foreground,                   original.foreground,                   "foreground");
    CheckRGB(loaded.highlightBackground,          original.highlightBackground,          "highlightBackground");
    CheckRGB(loaded.highlightForeground,          original.highlightForeground,          "highlightForeground");
    CheckRGB(loaded.dotBackground,                original.dotBackground,                "dotBackground");
    CheckRGB(loaded.dotForeground,                original.dotForeground,                "dotForeground");
    CheckRGB(loaded.blockBackground,              original.blockBackground,              "blockBackground");
    CheckRGB(loaded.blockForeground,              original.blockForeground,              "blockForeground");
    CheckRGB(loaded.commentBackground,            original.commentBackground,            "commentBackground");
    CheckRGB(loaded.commentForeground,            original.commentForeground,            "commentForeground");
    CheckRGB(loaded.errorBackground,              original.errorBackground,              "errorBackground");
    CheckRGB(loaded.errorForeground,              original.errorForeground,              "errorForeground");
    CheckRGB(loaded.unknownBackground,            original.unknownBackground,            "unknownBackground");
    CheckRGB(loaded.unknownForeground,            original.unknownForeground,            "unknownForeground");
    CheckRGB(loaded.notImplementedBackground,     original.notImplementedBackground,     "notImplementedBackground");
    CheckRGB(loaded.notImplementedForeground,     original.notImplementedForeground,     "notImplementedForeground");
    CheckRGB(loaded.searchBackground,             original.searchBackground,             "searchBackground");
    CheckRGB(loaded.searchForeground,             original.searchForeground,             "searchForeground");
    CheckRGB(loaded.statusBarForeground,          original.statusBarForeground,          "statusBarForeground");
    CheckRGB(loaded.statusBarBackground,          original.statusBarBackground,          "statusBarBackground");
    CheckRGB(loaded.helpPanelForeground,          original.helpPanelForeground,          "helpPanelForeground");
    CheckRGB(loaded.helpPanelBackground,          original.helpPanelBackground,          "helpPanelBackground");
    CheckRGB(loaded.helpPanelKeystrokeForeground, original.helpPanelKeystrokeForeground, "helpPanelKeystrokeForeground");
    CheckRGB(loaded.helpPanelKeystrokeBackground, original.helpPanelKeystrokeBackground, "helpPanelKeystrokeBackground");
    CheckRGB(loaded.rulerForeground,              original.rulerForeground,              "rulerForeground");
    CheckRGB(loaded.rulerBackground,              original.rulerBackground,              "rulerBackground");
}


TEST_CASE("TUI palette round-trip: save and load")
{
    TempConfigDir tmpDir;

    // Build a test palette with distinctive values
    sTUIPalette original;
    original.name = "TUI Test";
    // Editor colors
    original.background                   = {10, 20, 30};
    original.foreground                   = {200, 210, 220};
    original.highlightBackground          = {40, 50, 60};
    original.highlightForeground          = {230, 240, 250};
    original.dotBackground                = {70, 80, 90};
    original.dotForeground                = {100, 110, 120};
    original.blockBackground              = {11, 22, 33};
    original.blockForeground              = {44, 55, 66};
    original.commentBackground            = {77, 88, 99};
    original.commentForeground            = {111, 122, 133};
    original.errorBackground              = {144, 155, 166};
    original.errorForeground              = {177, 188, 199};
    original.unknownBackground            = {201, 202, 203};
    original.unknownForeground            = {204, 205, 206};
    original.notImplementedBackground     = {207, 208, 209};
    original.notImplementedForeground     = {210, 211, 212};
    original.searchBackground             = {213, 214, 215};
    original.searchForeground             = {216, 217, 218};
    // Style colors
    original.boldForeground               = {1, 2, 3};
    original.boldBackground               = {4, 5, 6};
    original.italicForeground             = {7, 8, 9};
    original.italicBackground             = {10, 11, 12};
    original.underlineForeground          = {13, 14, 15};
    original.underlineBackground          = {16, 17, 18};
    original.strikethroughForeground      = {19, 20, 21};
    original.strikethroughBackground      = {22, 23, 24};
    original.superscriptForeground        = {25, 26, 27};
    original.superscriptBackground        = {28, 29, 30};
    original.subscriptForeground          = {31, 32, 33};
    original.subscriptBackground          = {34, 35, 36};
    // Screen colors
    original.titleBarForeground           = {37, 38, 39};
    original.titleBarBackground           = {40, 41, 42};
    original.statusBarForeground          = {43, 44, 45};
    original.statusBarBackground          = {46, 47, 48};
    original.helpPanelForeground          = {49, 50, 51};
    original.helpPanelBackground          = {52, 53, 54};
    original.helpPanelKeystrokeForeground = {55, 56, 57};
    original.helpPanelKeystrokeBackground = {58, 59, 60};
    original.menuBarForeground            = {61, 62, 63};
    original.menuBarBackground            = {64, 65, 66};
    original.menuAcceleratorForeground    = {67, 68, 69};
    original.menuAcceleratorBackground    = {70, 71, 72};
    original.menuHighlightForeground      = {73, 74, 75};
    original.menuHighlightBackground      = {76, 77, 78};
    original.rulerForeground              = {79, 80, 81};
    original.rulerBackground              = {82, 83, 84};
    original.flagColumnForeground         = {85, 86, 87};
    original.flagColumnBackground         = {88, 89, 90};
    original.scrollbarForeground          = {91, 92, 93};
    original.scrollbarBackground          = {94, 95, 96};

    // Save it
    bool saved = cConfig::SaveCustomTUIPalette(original);
    CHECK(saved);

    // Load and verify
    auto palettes = cConfig::LoadCustomTUIPalettes();
    REQUIRE(palettes.size() == 1);
    CHECK(palettes[0].name == "TUI Test");

    const auto& loaded = palettes[0];

    // Spot-check a selection of fields from each group
    CheckRGB(loaded.background,                   original.background,                   "background");
    CheckRGB(loaded.foreground,                   original.foreground,                   "foreground");
    CheckRGB(loaded.searchForeground,             original.searchForeground,             "searchForeground");
    CheckRGB(loaded.boldForeground,               original.boldForeground,               "boldForeground");
    CheckRGB(loaded.subscriptBackground,          original.subscriptBackground,          "subscriptBackground");
    CheckRGB(loaded.titleBarForeground,           original.titleBarForeground,           "titleBarForeground");
    CheckRGB(loaded.scrollbarBackground,          original.scrollbarBackground,          "scrollbarBackground");
    CheckRGB(loaded.menuHighlightForeground,      original.menuHighlightForeground,      "menuHighlightForeground");
    CheckRGB(loaded.flagColumnBackground,         original.flagColumnBackground,         "flagColumnBackground");
}


TEST_CASE("Multiple palettes in one file")
{
    TempConfigDir tmpDir;

    // Save three palettes
    sGUIPalette pal1;
    pal1.name = "Alpha";
    pal1.background = {10, 10, 10};
    pal1.foreground = {200, 200, 200};
    // Leave rest as defaults (will load as black/white)

    sGUIPalette pal2;
    pal2.name = "Beta";
    pal2.background = {20, 20, 20};
    pal2.foreground = {190, 190, 190};

    sGUIPalette pal3;
    pal3.name = "Gamma";
    pal3.background = {30, 30, 30};
    pal3.foreground = {180, 180, 180};

    CHECK(cConfig::SaveCustomGUIPalette(pal1));
    CHECK(cConfig::SaveCustomGUIPalette(pal2));
    CHECK(cConfig::SaveCustomGUIPalette(pal3));

    // Load and verify all three are present
    auto palettes = cConfig::LoadCustomGUIPalettes();
    REQUIRE(palettes.size() == 3);

    // Verify names (order preserved by LoadOrder sort)
    CHECK(palettes[0].name == "Alpha");
    CHECK(palettes[1].name == "Beta");
    CHECK(palettes[2].name == "Gamma");

    // Verify distinctive values survived
    CheckRGB(palettes[0].background, {10, 10, 10}, "Alpha bg");
    CheckRGB(palettes[1].background, {20, 20, 20}, "Beta bg");
    CheckRGB(palettes[2].background, {30, 30, 30}, "Gamma bg");
}


TEST_CASE("Overwrite existing palette by name")
{
    TempConfigDir tmpDir;

    // Save initial version
    sGUIPalette pal;
    pal.name = "Overwrite Me";
    pal.background = {100, 100, 100};
    pal.foreground = {200, 200, 200};
    CHECK(cConfig::SaveCustomGUIPalette(pal));

    // Overwrite with new colors
    pal.background = {50, 50, 50};
    pal.foreground = {250, 250, 250};
    CHECK(cConfig::SaveCustomGUIPalette(pal));

    // Should still be exactly one palette with the new values
    auto palettes = cConfig::LoadCustomGUIPalettes();
    REQUIRE(palettes.size() == 1);
    CHECK(palettes[0].name == "Overwrite Me");
    CheckRGB(palettes[0].background, {50, 50, 50}, "overwritten bg");
    CheckRGB(palettes[0].foreground, {250, 250, 250}, "overwritten fg");
}


TEST_CASE("GetConfigDir creates directory")
{
    TempConfigDir tmpDir;

    std::string dir = cConfig::GetConfigDir();
    CHECK(std::filesystem::exists(dir));
    CHECK(std::filesystem::is_directory(dir));
}


TEST_CASE("Palette file paths are in config dir")
{
    TempConfigDir tmpDir;

    std::string dir = cConfig::GetConfigDir();
    std::string guiPath = cConfig::GetGUIPaletteFilePath();
    std::string tuiPath = cConfig::GetTUIPaletteFilePath();

    // Both paths should start with the config directory
    CHECK(guiPath.find(dir) == 0);
    CHECK(tuiPath.find(dir) == 0);

    // And have the right filenames
    CHECK(guiPath.find("gui-palettes.ini") != std::string::npos);
    CHECK(tuiPath.find("tui-palettes.ini") != std::string::npos);
}

}  // TEST_SUITE


TEST_SUITE("cConfig - Spell Check Dot Commands")
{

TEST_CASE("spellCheckDotCommands default is false")
{
    cConfig config;
    CHECK(config.mSpellCheckDotCommands == false);
}


TEST_CASE("spellCheckDotCommands round-trip save and load")
{
    TempConfigDir tmpDir;

    // Save with flag enabled
    cConfig config1;
    config1.mSpellCheckDotCommands = true;
    config1.Save();

    // Load and verify
    cConfig config2;
    config2.Load();
    CHECK(config2.mSpellCheckDotCommands == true);

    // Save with flag disabled
    config2.mSpellCheckDotCommands = false;
    config2.Save();

    // Load and verify
    cConfig config3;
    config3.Load();
    CHECK(config3.mSpellCheckDotCommands == false);
}

}  // TEST_SUITE
