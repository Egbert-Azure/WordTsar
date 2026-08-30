#include "doctest.h"

#include "src/core/include/utils.h"
#include "src/core/utils/config.h"

TEST_CASE("string_sprintf")
{
    std::string s = string_sprintf("Hello %s", "World") ;
    CHECK(s == "Hello World") ;

    s = string_sprintf("Hello %d", 42) ;
    CHECK(s == "Hello 42") ;

    s = string_sprintf("Hello %c", 'A') ;
    CHECK(s == "Hello A") ;

    s = string_sprintf("Hello %.2f", 3.14) ;
    CHECK(s == "Hello 3.14") ;
}

TEST_CASE("UT-1a: string_sprintf with long string exceeding stack buffer")
{
    // The stack buffer is 256 bytes. Create a format that produces > 256 chars
    // to exercise the C++17 dynamic allocation path
    std::string longStr(300, 'X');
    std::string result = string_sprintf("%s", longStr.c_str());
    CHECK(result.length() == 300);
    CHECK(result == longStr);
}

TEST_CASE("UT-2a: cConfig BlendChannel")
{
    // Blend at t=0.0 should return first channel
    CHECK(cConfig::BlendChannel(0, 255, 0.0f) == 0);

    // Blend at t=1.0 should return second channel
    CHECK(cConfig::BlendChannel(0, 255, 1.0f) == 255);

    // Blend at t=0.5 should return midpoint
    CHECK(cConfig::BlendChannel(0, 200, 0.5f) == 100);

    // Blend should clamp to 0-255
    CHECK(cConfig::BlendChannel(0, 255, 2.0f) == 255);
    CHECK(cConfig::BlendChannel(255, 0, 2.0f) == 0);
}

TEST_CASE("UT-2b: cConfig AddRecentFile")
{
    cConfig config;

    SUBCASE("Add file to empty list")
    {
        config.AddRecentFile("test.ws");
        CHECK(config.mRecentFiles[0] == "test.ws");
    }

    SUBCASE("Add empty filename does nothing")
    {
        config.AddRecentFile("first.ws");
        config.AddRecentFile("");
        CHECK(config.mRecentFiles[0] == "first.ws");
    }

    SUBCASE("Adding same file at front does nothing")
    {
        config.AddRecentFile("first.ws");
        config.AddRecentFile("first.ws");
        CHECK(config.mRecentFiles[0] == "first.ws");
        CHECK(config.mRecentFiles[1].empty());
    }

    SUBCASE("Adding new file shifts existing down")
    {
        config.AddRecentFile("first.ws");
        config.AddRecentFile("second.ws");
        CHECK(config.mRecentFiles[0] == "second.ws");
        CHECK(config.mRecentFiles[1] == "first.ws");
    }

    SUBCASE("Adding existing file moves it to front")
    {
        config.AddRecentFile("first.ws");
        config.AddRecentFile("second.ws");
        config.AddRecentFile("third.ws");
        // List is now: third, second, first
        config.AddRecentFile("first.ws");
        // Should move first.ws to front: first, third, second
        CHECK(config.mRecentFiles[0] == "first.ws");
        CHECK(config.mRecentFiles[1] == "third.ws");
        CHECK(config.mRecentFiles[2] == "second.ws");
    }

    SUBCASE("List maxes out at 10 entries")
    {
        for (int i = 0; i < 12; i++)
        {
            config.AddRecentFile("file" + std::to_string(i) + ".ws");
        }
        // Most recent should be at front
        CHECK(config.mRecentFiles[0] == "file11.ws");
        // Oldest still in list should be file2 (file0 and file1 were pushed out)
        CHECK(config.mRecentFiles[9] == "file2.ws");
    }
}

TEST_CASE("UT-2c: cConfig GetGUIPalettes returns built-in palettes")
{
    std::vector<sGUIPalette> palettes = cConfig::GetGUIPalettes();

    // Should have at least 2 built-in palettes (Default Light and Dark)
    CHECK(palettes.size() >= 2);

    // First palette should be "Default Light"
    CHECK(palettes[0].name == "Default Light");

    // Verify the palette has reasonable color values (0-255 range)
    CHECK(palettes[0].background.r >= 0);
    CHECK(palettes[0].background.r <= 255);
    CHECK(palettes[0].foreground.r >= 0);
    CHECK(palettes[0].foreground.r <= 255);
}

TEST_CASE("UT-2d: cConfig GetTUIPalettes returns built-in palettes")
{
    std::vector<sTUIPalette> palettes = cConfig::GetTUIPalettes();

    // Should have multiple built-in palettes
    CHECK(palettes.size() >= 2);

    // First palette should be "WordTsar"
    CHECK(palettes[0].name == "WordTsar");

    // Verify palette has reasonable color values
    CHECK(palettes[0].background.r >= 0);
    CHECK(palettes[0].background.r <= 255);
}

TEST_CASE("UT-2e: cConfig ApplyGUIPalette")
{
    cConfig config;

    // Get the built-in palettes and apply the second one (Dark)
    std::vector<sGUIPalette> palettes = cConfig::GetGUIPalettes();
    REQUIRE(palettes.size() >= 2);

    config.ApplyGUIPalette(palettes[1]);

    // Verify the config's color members match the applied palette
    CHECK(config.mGuiBackground.r == palettes[1].background.r);
    CHECK(config.mGuiBackground.g == palettes[1].background.g);
    CHECK(config.mGuiBackground.b == palettes[1].background.b);
    CHECK(config.mGuiForeground.r == palettes[1].foreground.r);
    CHECK(config.mGuiStatusBarForeground.r == palettes[1].statusBarForeground.r);
    CHECK(config.mGuiRulerBackground.r == palettes[1].rulerBackground.r);
}

TEST_CASE("UT-2f: cConfig ApplyTUIPalette")
{
    cConfig config;

    // Get the built-in palettes and apply a non-default one
    std::vector<sTUIPalette> palettes = cConfig::GetTUIPalettes();
    REQUIRE(palettes.size() >= 2);

    config.ApplyTUIPalette(palettes[1]);

    // Verify the config's TUI color members match the applied palette
    CHECK(config.mTuiBackground.r == palettes[1].background.r);
    CHECK(config.mTuiBackground.g == palettes[1].background.g);
    CHECK(config.mTuiBackground.b == palettes[1].background.b);
    CHECK(config.mTuiForeground.r == palettes[1].foreground.r);

    // Check style colors
    CHECK(config.mTuiBoldForeground.r == palettes[1].boldForeground.r);

    // Check screen colors
    CHECK(config.mTuiTitleBarForeground.r == palettes[1].titleBarForeground.r);
    CHECK(config.mTuiScrollbarBackground.r == palettes[1].scrollbarBackground.r);
}
