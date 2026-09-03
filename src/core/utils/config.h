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

#ifndef WORDTSAR_CONFIG_H
#define WORDTSAR_CONFIG_H

#include "src/core/include/config.h"

#include <string>
#include <vector>
#include <cstdint>


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sRGB
///
/// @brief
/// Simple RGB color triplet with short components (0-255).
/// Used by cConfig for all color storage.
///
/////////////////////////////////////////////////////////////////////////////
struct sRGB
{
    short r;
    short g;
    short b;
};


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sGUIPalette
///
/// @brief
/// Named preset color palette for the GUI. Contains all 26 sRGB values
/// (13 fg+bg pairs) that populate the [gui.colors] and [gui.screen]
/// config sections. Used as a starting point; users can customize
/// individual colors after applying a palette.
///
/////////////////////////////////////////////////////////////////////////////
struct sGUIPalette
{
    std::string name;

    // [gui.colors] -- 9 editor color pairs (18 sRGB)
    sRGB background, foreground;
    sRGB highlightBackground, highlightForeground;
    sRGB dotBackground, dotForeground;
    sRGB blockBackground, blockForeground;
    sRGB commentBackground, commentForeground;
    sRGB errorBackground, errorForeground;
    sRGB unknownBackground, unknownForeground;
    sRGB notImplementedBackground, notImplementedForeground;
    sRGB searchBackground, searchForeground;

    // [gui.screen] -- 4 screen color pairs (8 sRGB)
    sRGB statusBarForeground, statusBarBackground;
    sRGB helpPanelForeground, helpPanelBackground;
    sRGB helpPanelKeystrokeForeground, helpPanelKeystrokeBackground;
    sRGB rulerForeground, rulerBackground;
};


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sTUIPalette
///
/// @brief
/// Named preset color palette for the TUI. Contains all 50 sRGB values
/// (25 fg+bg pairs) that populate the [tui.colors], [tui.styleColors],
/// and [tui.screen] config sections. Used as a starting point; users can
/// customize individual colors after applying a palette.
///
/////////////////////////////////////////////////////////////////////////////
struct sTUIPalette
{
    std::string name;

    // [tui.colors] -- 9 editor color pairs (18 sRGB)
    sRGB background, foreground;
    sRGB highlightBackground, highlightForeground;
    sRGB dotBackground, dotForeground;
    sRGB blockBackground, blockForeground;
    sRGB commentBackground, commentForeground;
    sRGB errorBackground, errorForeground;
    sRGB unknownBackground, unknownForeground;
    sRGB notImplementedBackground, notImplementedForeground;
    sRGB searchBackground, searchForeground;

    // [tui.styleColors] -- 6 style fallback color pairs (12 sRGB)
    sRGB boldForeground, boldBackground;
    sRGB italicForeground, italicBackground;
    sRGB underlineForeground, underlineBackground;
    sRGB strikethroughForeground, strikethroughBackground;
    sRGB superscriptForeground, superscriptBackground;
    sRGB subscriptForeground, subscriptBackground;

    // [tui.screen] -- 10 screen color pairs (20 sRGB)
    sRGB titleBarForeground, titleBarBackground;
    sRGB statusBarForeground, statusBarBackground;
    sRGB helpPanelForeground, helpPanelBackground;
    sRGB helpPanelKeystrokeForeground, helpPanelKeystrokeBackground;
    sRGB menuBarForeground, menuBarBackground;
    sRGB menuAcceleratorForeground, menuAcceleratorBackground;
    sRGB menuHighlightForeground, menuHighlightBackground;
    sRGB rulerForeground, rulerBackground;
    sRGB flagColumnForeground, flagColumnBackground;
    sRGB scrollbarForeground, scrollbarBackground;
};


class cConfig
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cConfig(void);

    // --- Load / Save ---
    bool Load(void);
    bool Save(void);

    // --- Recent files ---
    void AddRecentFile(const std::string& filename);

    // --- Palette application ---
    void ApplyGUIPalette(const sGUIPalette& palette);
    void ApplyTUIPalette(const sTUIPalette& palette);

    // --- Static utility functions ---
    static std::string GetConfigFilePath(void);
    static std::string GetConfigDir(void);
    static bool ParseRGB(const char* value, short& r, short& g, short& b);
    static std::string FormatRGB(short r, short g, short b);
    static COORD_T ParseMeasurement(const char* value);
    static std::string FormatMeasurement(COORD_T twips, char unit);
    static short BlendChannel(short a, short b, float t);

    // --- Static palette getters ---
    static std::vector<sGUIPalette> GetGUIPalettes(void);
    static std::vector<sTUIPalette> GetTUIPalettes(void);

    // --- Static custom palette file helpers ---
    static std::string GetGUIPaletteFilePath(void);
    static std::string GetTUIPaletteFilePath(void);
    static std::vector<sGUIPalette> LoadCustomGUIPalettes(void);
    static std::vector<sTUIPalette> LoadCustomTUIPalettes(void);
    static bool SaveCustomGUIPalette(const sGUIPalette& palette);
    static bool SaveCustomTUIPalette(const sTUIPalette& palette);

private:
    sRGB ReadRGB(void* ini, const char* section, const char* key, sRGB defaultVal);
    void WriteRGB(void* ini, const char* section, const char* key, const sRGB& color);
    COORD_T ReadMeasurement(void* ini, const char* section, const char* key, const char* defaultVal);
    void WriteMeasurement(void* ini, const char* section, const char* key, COORD_T twips, char unit);
    void WriteCommentedTemplate(const std::string& path);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

public:
    // --- [editor] settings ---
    int mInputMode;                  // 0 = WordStar, 1 = Modern/CUA
    bool mShowControls;
    int mCodePage;
    std::string mMeasurement;
    std::string mDefaultFont;
    double mDefaultFontSize;
    std::string mDefaultFormat;
    int mAutoSaveInterval;           // seconds (0 = disabled)
    int mCaretBlinkRate;             // milliseconds
    std::string mSpellCheckLanguage;
    bool mSpellCheckDotCommands;     // spell check text content of dot command lines
    std::string mDefaultDirectory;

    // --- [editor.pageSetup] settings (stored in twips internally) ---
    COORD_T mPaperWidth;
    COORD_T mPaperHeight;
    COORD_T mPageOffsetOdd;
    COORD_T mPageOffsetEven;
    COORD_T mLeftMargin;
    COORD_T mRightMargin;
    COORD_T mTopMargin;
    COORD_T mBottomMargin;
    COORD_T mHeaderMargin;
    COORD_T mFooterMargin;
    bool mLandscape;

    // --- [user] settings ---
    std::string mShortName;
    std::string mLongName;

    // --- [recentFiles] ---
    std::string mRecentFiles[10];

    // --- [gui] settings ---
    int mWindowWidth;
    int mWindowHeight;

    // --- [gui.display] settings ---
    int mGuiShowHelp;
    bool mGuiShowRuler;
    bool mGuiShowScrollBar;
    bool mGuiShowStatusBar;
    bool mGuiShowStyleBar;
    bool mGuiShowMenu;
    bool mGuiAlwaysDotCommands;
    bool mGuiAlwaysFlagColumn;

    // --- [gui.colors] settings (9 editor color pairs = 18 sRGB values) ---
    sRGB mGuiBackground;
    sRGB mGuiForeground;
    sRGB mGuiHighlightBackground;
    sRGB mGuiHighlightForeground;
    sRGB mGuiDotBackground;
    sRGB mGuiDotForeground;
    sRGB mGuiBlockBackground;
    sRGB mGuiBlockForeground;
    sRGB mGuiCommentBackground;
    sRGB mGuiCommentForeground;
    sRGB mGuiErrorBackground;
    sRGB mGuiErrorForeground;
    sRGB mGuiUnknownBackground;
    sRGB mGuiUnknownForeground;
    sRGB mGuiNotImplementedBackground;
    sRGB mGuiNotImplementedForeground;
    sRGB mGuiSearchBackground;
    sRGB mGuiSearchForeground;

    // --- [gui.screen] settings (4 screen color pairs = 8 sRGB values) ---
    sRGB mGuiStatusBarForeground;
    sRGB mGuiStatusBarBackground;
    sRGB mGuiHelpPanelForeground;
    sRGB mGuiHelpPanelBackground;
    sRGB mGuiHelpPanelKeystrokeForeground;
    sRGB mGuiHelpPanelKeystrokeBackground;
    sRGB mGuiRulerForeground;
    sRGB mGuiRulerBackground;

    // --- [tui.display] settings ---
    bool mTuiShowTitleBar;
    int mTuiShowHelp;
    bool mTuiShowRuler;
    bool mTuiShowScrollBar;
    bool mTuiShowStatusBar;
    bool mTuiShowStyleBar;
    bool mTuiShowMenu;
    bool mTuiAlwaysDotCommands;
    bool mTuiAlwaysFlagColumn;
    bool mTuiCenterView;

    // --- [tui.colors] settings (9 editor color pairs = 18 sRGB values) ---
    sRGB mTuiBackground;
    sRGB mTuiForeground;
    sRGB mTuiHighlightBackground;
    sRGB mTuiHighlightForeground;
    sRGB mTuiDotBackground;
    sRGB mTuiDotForeground;
    sRGB mTuiBlockBackground;
    sRGB mTuiBlockForeground;
    sRGB mTuiCommentBackground;
    sRGB mTuiCommentForeground;
    sRGB mTuiErrorBackground;
    sRGB mTuiErrorForeground;
    sRGB mTuiUnknownBackground;
    sRGB mTuiUnknownForeground;
    sRGB mTuiNotImplementedBackground;
    sRGB mTuiNotImplementedForeground;
    sRGB mTuiSearchBackground;
    sRGB mTuiSearchForeground;

    // --- [tui.styleColors] settings (6 style fallback color pairs = 12 sRGB values) ---
    sRGB mTuiBoldForeground;
    sRGB mTuiBoldBackground;
    sRGB mTuiItalicForeground;
    sRGB mTuiItalicBackground;
    sRGB mTuiUnderlineForeground;
    sRGB mTuiUnderlineBackground;
    sRGB mTuiStrikethroughForeground;
    sRGB mTuiStrikethroughBackground;
    sRGB mTuiSuperscriptForeground;
    sRGB mTuiSuperscriptBackground;
    sRGB mTuiSubscriptForeground;
    sRGB mTuiSubscriptBackground;

    // --- [tui.screen] settings (10 screen color pairs = 20 sRGB values) ---
    sRGB mTuiTitleBarForeground;
    sRGB mTuiTitleBarBackground;
    sRGB mTuiStatusBarForeground;
    sRGB mTuiStatusBarBackground;
    sRGB mTuiHelpPanelForeground;
    sRGB mTuiHelpPanelBackground;
    sRGB mTuiHelpPanelKeystrokeForeground;
    sRGB mTuiHelpPanelKeystrokeBackground;
    sRGB mTuiMenuBarForeground;
    sRGB mTuiMenuBarBackground;
    sRGB mTuiMenuAcceleratorForeground;
    sRGB mTuiMenuAcceleratorBackground;
    sRGB mTuiMenuHighlightForeground;
    sRGB mTuiMenuHighlightBackground;
    sRGB mTuiRulerForeground;
    sRGB mTuiRulerBackground;
    sRGB mTuiFlagColumnForeground;
    sRGB mTuiFlagColumnBackground;
    sRGB mTuiScrollbarForeground;
    sRGB mTuiScrollbarBackground;

private:
    std::string mConfigPath;
};

#endif // WORDTSAR_CONFIG_H
