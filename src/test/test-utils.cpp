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

static bool SameRGB(const sRGB& a, sRGB b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

static void CheckEditorAndStyleColors(const sTUIPalette& p)
{
    CHECK(SameRGB(p.foreground,               {170, 170, 170}));
    CHECK(SameRGB(p.background,               {0, 0, 170}));
    CHECK(SameRGB(p.highlightForeground,      {0, 0, 0}));
    CHECK(SameRGB(p.highlightBackground,      {100, 200, 255}));
    CHECK(SameRGB(p.dotForeground,            {170, 170, 170}));
    CHECK(SameRGB(p.dotBackground,            {100, 220, 180}));
    CHECK(SameRGB(p.blockForeground,          {255, 255, 255}));
    CHECK(SameRGB(p.blockBackground,          {80, 130, 255}));
    CHECK(SameRGB(p.commentForeground,        {0, 0, 0}));
    CHECK(SameRGB(p.commentBackground,        {255, 191, 0}));
    CHECK(SameRGB(p.errorForeground,          {0, 0, 0}));
    CHECK(SameRGB(p.errorBackground,          {255, 100, 100}));
    CHECK(SameRGB(p.unknownForeground,        {0, 0, 0}));
    CHECK(SameRGB(p.unknownBackground,        {255, 255, 0}));
    CHECK(SameRGB(p.notImplementedForeground, {0, 0, 0}));
    CHECK(SameRGB(p.notImplementedBackground, {220, 200, 80}));
    CHECK(SameRGB(p.searchForeground,         {255, 255, 255}));
    CHECK(SameRGB(p.searchBackground,         {80, 130, 255}));
    CHECK(SameRGB(p.boldForeground,           {255, 255, 255}));
    CHECK(SameRGB(p.boldBackground,           {0, 0, 170}));
    CHECK(SameRGB(p.italicForeground,         {0, 128, 128}));
    CHECK(SameRGB(p.italicBackground,         {0, 0, 170}));
    CHECK(SameRGB(p.underlineForeground,      {255, 105, 180}));
    CHECK(SameRGB(p.underlineBackground,      {0, 0, 170}));
    CHECK(SameRGB(p.strikethroughForeground,  {180, 0, 0}));
    CHECK(SameRGB(p.strikethroughBackground,  {0, 0, 170}));
    CHECK(SameRGB(p.superscriptForeground,    {128, 0, 128}));
    CHECK(SameRGB(p.superscriptBackground,    {0, 0, 170}));
    CHECK(SameRGB(p.subscriptForeground,      {0, 128, 0}));
    CHECK(SameRGB(p.subscriptBackground,      {0, 0, 170}));
    CHECK(SameRGB(p.rulerForeground,          {170, 170, 170}));
    CHECK(SameRGB(p.rulerBackground,          {0, 0, 0}));
    CHECK(SameRGB(p.flagColumnForeground,     {170, 170, 170}));
    CHECK(SameRGB(p.flagColumnBackground,     {0, 0, 0}));
    CHECK(SameRGB(p.scrollbarForeground,      {0, 0, 170}));
    CHECK(SameRGB(p.scrollbarBackground,      {85, 85, 128}));
}

TEST_CASE("UT-2g: cConfig GetTUIPalettes provides a WordStar 7 preset last")
{
    std::vector<sTUIPalette> palettes = cConfig::GetTUIPalettes();
    REQUIRE(palettes.size() == 6);

    // Existing presets keep their positions
    CHECK(palettes[0].name == "WordTsar");
    CHECK(palettes[1].name == "Standard CGA");
    CHECK(palettes[2].name == "Monochrome");
    CHECK(palettes[3].name == "Alternate CGA");
    CHECK(palettes[4].name == "Sawyer");

    const sTUIPalette& ws7 = palettes.back();
    CHECK(ws7.name == "WordStar 7");
    CheckEditorAndStyleColors(ws7);
    CHECK(SameRGB(ws7.titleBarForeground,           {170, 170, 170}));
    CHECK(SameRGB(ws7.titleBarBackground,           {0, 0, 0}));
    CHECK(SameRGB(ws7.statusBarForeground,          {170, 170, 170}));
    CHECK(SameRGB(ws7.statusBarBackground,          {0, 0, 0}));
    CHECK(SameRGB(ws7.menuBarForeground,            {0, 0, 0}));
    CHECK(SameRGB(ws7.menuBarBackground,            {170, 170, 170}));
    CHECK(SameRGB(ws7.menuAcceleratorForeground,    {85, 85, 255}));
    CHECK(SameRGB(ws7.menuAcceleratorBackground,    {170, 170, 170}));
    CHECK(SameRGB(ws7.menuHighlightForeground,      {170, 170, 170}));
    CHECK(SameRGB(ws7.menuHighlightBackground,      {0, 0, 0}));
    CHECK(SameRGB(ws7.helpPanelForeground,          {0, 0, 0}));
    CHECK(SameRGB(ws7.helpPanelBackground,          {170, 170, 170}));
    CHECK(SameRGB(ws7.helpPanelKeystrokeForeground, {85, 85, 255}));
    CHECK(SameRGB(ws7.helpPanelKeystrokeBackground, {170, 170, 170}));

    // "WordTsar" preset is unchanged
    const sTUIPalette& wt = palettes[0];
    CheckEditorAndStyleColors(wt);
    CHECK(SameRGB(wt.titleBarForeground,           {0, 0, 170}));
    CHECK(SameRGB(wt.titleBarBackground,           {170, 170, 170}));
    CHECK(SameRGB(wt.statusBarForeground,          {0, 0, 170}));
    CHECK(SameRGB(wt.statusBarBackground,          {170, 170, 170}));
    CHECK(SameRGB(wt.helpPanelForeground,          {0, 0, 34}));
    CHECK(SameRGB(wt.helpPanelBackground,          {170, 170, 170}));
    CHECK(SameRGB(wt.helpPanelKeystrokeForeground, {0, 0, 180}));
    CHECK(SameRGB(wt.helpPanelKeystrokeBackground, {170, 170, 170}));
    CHECK(SameRGB(wt.menuBarForeground,            {0, 0, 34}));
    CHECK(SameRGB(wt.menuBarBackground,            {170, 170, 170}));
    CHECK(SameRGB(wt.menuAcceleratorForeground,    {170, 0, 0}));
    CHECK(SameRGB(wt.menuAcceleratorBackground,    {170, 170, 170}));
    CHECK(SameRGB(wt.menuHighlightForeground,      {170, 170, 170}));
    CHECK(SameRGB(wt.menuHighlightBackground,      {0, 0, 170}));

    // Default-constructed config TUI colors are unchanged
    cConfig config;
    CHECK(SameRGB(config.mTuiBackground,                   {0, 0, 170}));
    CHECK(SameRGB(config.mTuiForeground,                   {170, 170, 170}));
    CHECK(SameRGB(config.mTuiHighlightBackground,          {100, 200, 255}));
    CHECK(SameRGB(config.mTuiHighlightForeground,          {0, 0, 0}));
    CHECK(SameRGB(config.mTuiDotBackground,                {100, 220, 180}));
    CHECK(SameRGB(config.mTuiDotForeground,                {170, 170, 170}));
    CHECK(SameRGB(config.mTuiBlockBackground,              {80, 130, 255}));
    CHECK(SameRGB(config.mTuiBlockForeground,              {255, 255, 255}));
    CHECK(SameRGB(config.mTuiCommentBackground,            {255, 191, 0}));
    CHECK(SameRGB(config.mTuiCommentForeground,            {0, 0, 0}));
    CHECK(SameRGB(config.mTuiErrorBackground,              {255, 100, 100}));
    CHECK(SameRGB(config.mTuiErrorForeground,              {0, 0, 0}));
    CHECK(SameRGB(config.mTuiUnknownBackground,            {255, 255, 0}));
    CHECK(SameRGB(config.mTuiUnknownForeground,            {0, 0, 0}));
    CHECK(SameRGB(config.mTuiNotImplementedBackground,     {220, 200, 80}));
    CHECK(SameRGB(config.mTuiNotImplementedForeground,     {0, 0, 0}));
    CHECK(SameRGB(config.mTuiSearchBackground,             {80, 130, 255}));
    CHECK(SameRGB(config.mTuiSearchForeground,             {255, 255, 255}));
    CHECK(SameRGB(config.mTuiBoldForeground,               {0, 0, 180}));
    CHECK(SameRGB(config.mTuiBoldBackground,               {0, 0, 170}));
    CHECK(SameRGB(config.mTuiItalicForeground,             {0, 128, 128}));
    CHECK(SameRGB(config.mTuiItalicBackground,             {0, 0, 170}));
    CHECK(SameRGB(config.mTuiUnderlineForeground,          {0, 0, 180}));
    CHECK(SameRGB(config.mTuiUnderlineBackground,          {0, 0, 170}));
    CHECK(SameRGB(config.mTuiStrikethroughForeground,      {180, 0, 0}));
    CHECK(SameRGB(config.mTuiStrikethroughBackground,      {0, 0, 170}));
    CHECK(SameRGB(config.mTuiSuperscriptForeground,        {128, 0, 128}));
    CHECK(SameRGB(config.mTuiSuperscriptBackground,        {0, 0, 170}));
    CHECK(SameRGB(config.mTuiSubscriptForeground,          {0, 128, 0}));
    CHECK(SameRGB(config.mTuiSubscriptBackground,          {0, 0, 170}));
    CHECK(SameRGB(config.mTuiTitleBarForeground,           {0, 0, 170}));
    CHECK(SameRGB(config.mTuiTitleBarBackground,           {170, 170, 170}));
    CHECK(SameRGB(config.mTuiStatusBarForeground,          {0, 0, 170}));
    CHECK(SameRGB(config.mTuiStatusBarBackground,          {170, 170, 170}));
    CHECK(SameRGB(config.mTuiHelpPanelForeground,          {0, 0, 34}));
    CHECK(SameRGB(config.mTuiHelpPanelBackground,          {170, 170, 170}));
    CHECK(SameRGB(config.mTuiHelpPanelKeystrokeForeground, {0, 0, 180}));
    CHECK(SameRGB(config.mTuiHelpPanelKeystrokeBackground, {170, 170, 170}));
    CHECK(SameRGB(config.mTuiMenuBarForeground,            {0, 0, 34}));
    CHECK(SameRGB(config.mTuiMenuBarBackground,            {170, 170, 170}));
    CHECK(SameRGB(config.mTuiMenuAcceleratorForeground,    {170, 0, 0}));
    CHECK(SameRGB(config.mTuiMenuAcceleratorBackground,    {170, 170, 170}));
    CHECK(SameRGB(config.mTuiMenuHighlightForeground,      {170, 170, 170}));
    CHECK(SameRGB(config.mTuiMenuHighlightBackground,      {0, 0, 170}));
    CHECK(SameRGB(config.mTuiRulerForeground,              {170, 170, 170}));
    CHECK(SameRGB(config.mTuiRulerBackground,              {0, 0, 0}));
    CHECK(SameRGB(config.mTuiFlagColumnForeground,         {170, 170, 170}));
    CHECK(SameRGB(config.mTuiFlagColumnBackground,         {0, 0, 0}));
    CHECK(SameRGB(config.mTuiScrollbarForeground,          {0, 0, 170}));
    CHECK(SameRGB(config.mTuiScrollbarBackground,          {85, 85, 128}));
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

TEST_CASE("UT-3a: AbbreviatePathForDisplay leaves paths that already fit untouched")
{
    CHECK(AbbreviatePathForDisplay("/tmp/test.ws", 60) == "/tmp/test.ws");
    CHECK(AbbreviatePathForDisplay("test.ws", 7) == "test.ws");
}

TEST_CASE("UT-3b: AbbreviatePathForDisplay maxWidth 0 means no limit")
{
    std::string longPath = "/Users/egbert/Documents/Vati - Buchkonzept und Kapitel/New Chapter One.ws";
    CHECK(AbbreviatePathForDisplay(longPath, 0) == longPath);
}

TEST_CASE("UT-3c: AbbreviatePathForDisplay keeps the filename, elides the directory")
{
    std::string longPath = "/Users/egbert/Documents/Vati - Buchkonzept und Kapitel/New Chapter One.ws";
    std::string result = AbbreviatePathForDisplay(longPath, 40);
    CHECK(result.size() <= 40);
    CHECK(result.find("New Chapter One.ws") != std::string::npos);
    CHECK(result.substr(0, 4) == ".../");
}

TEST_CASE("UT-3d: AbbreviatePathForDisplay elides the filename itself when nothing else fits")
{
    std::string longPath = "/Users/egbert/Documents/Vati - Buchkonzept und Kapitel/New Chapter One.ws";
    std::string result = AbbreviatePathForDisplay(longPath, 20);
    CHECK(result.size() <= 20);
    // Even the bare ".../filename" form doesn't fit at width 20, so the
    // filename's own middle gets elided -- the extension must survive.
    CHECK(result.find(".ws") != std::string::npos);
    CHECK(result.find("...") != std::string::npos);
}

TEST_CASE("UT-3e: AbbreviatePathForDisplay handles a bare filename with no directory")
{
    CHECK(AbbreviatePathForDisplay("New Chapter One.ws", 10).find(".ws") != std::string::npos);
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
