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

/**
 * @class cConfig
 *
 * @brief Application configuration persistence using INI file format.
 *
 * Implements the cConfig class which manages reading, writing, and
 * defaulting all WordTsar settings stored in a platform-specific INI file
 * via SimpleIni. Provides centralized configuration for both GUI and TUI
 * interfaces with no Qt dependency.
 *
 * @section config_sections INI File Sections
 * - [editor]: general editor preferences (undo steps, caret blink rate,
 *   auto-save interval, default file format, spell check language)
 * - [editor.pageSetup]: page margins and paper size in configurable units
 * - [user]: user name and initials for document metadata
 * - [gui]: GUI-specific display preferences (display mode, control codes)
 * - [gui.display]: GUI window geometry and state
 * - [gui.colors]: GUI color palette with named color entries
 * - [tui.display]: TUI display preferences (display mode, control codes)
 * - [tui.colors]: TUI terminal color palette (RGB triplets)
 * - [tui.styleColors]: TUI text style colors (selection, control codes)
 * - [mRecentFiles]: recently opened file paths (ordered list)
 *
 * @section config_paths Configuration File Location
 * Uses XDG-standard paths on Linux (~/.config/wordtsar/config.ini),
 * with platform-appropriate defaults on Windows and macOS. Creates the
 * configuration directory and a commented template file on first run.
 *
 * @section config_units Measurement Unit Conversion
 * Supports inches, centimeters, points, and picas as input units for
 * margins and paper size, converting all values to twips (1/1440 inch)
 * for internal use. Unit preference is stored and preserved across sessions.
 *
 * @section config_colors Color Handling
 * Parses RGB color values as comma-separated triplets (e.g., "255,128,0")
 * into sRGB structures. Provides separate color palettes for GUI (sGUIPalette)
 * and TUI (sTUIPalette) with distinct default values appropriate to each
 * interface.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cConfig Configuration class
 * @see sRGB RGB color value structure
 * @see sGUIPalette GUI color palette structure
 * @see sTUIPalette TUI color palette structure
 */

#include "config.h"
#include "SimpleIni.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor -- sets all members to default values matching ini.md spec.
///
/////////////////////////////////////////////////////////////////////////////
cConfig::cConfig(void)
{
    // [editor] defaults
    mInputMode = INPUT_WORDSTAR;
    mShowControls = true;
    mCodePage = 0;
    mMeasurement = "0i";
    mDefaultFont = "Courier New";
    mDefaultFontSize = 12.0;
    mDefaultFormat = "ws";
    mAutoSaveInterval = 60;
    mCaretBlinkRate = 500;
    mSpellCheckLanguage = "en_US";
    mSpellCheckDotCommands = false;
    mDefaultDirectory = "";

    // [editor.pageSetup] defaults (match layoutstate.cpp constructor)
    mPaperWidth = 12240;         // 8.5 inches
    mPaperHeight = 15840;        // 11.0 inches
    mPageOffsetOdd = 1440;       // 1.0 inch
    mPageOffsetEven = 1440;      // 1.0 inch
    mLeftMargin = 0;             // 0.0 inches
    mRightMargin = 9360;         // 6.5 inches
    mTopMargin = 1440;           // 1.0 inch
    mBottomMargin = 1440;        // 1.0 inch
    mHeaderMargin = 475;         // 0.33 inches
    mFooterMargin = 475;         // 0.33 inches
    mLandscape = false;

    // [user] defaults
    mShortName = "none";
    mLongName = "none";

    // [mRecentFiles] defaults (all empty)
    for (int i = 0; i < 10; i++)
    {
        mRecentFiles[i] = "";
    }

    // [gui] defaults
    mWindowWidth = 1000;
    mWindowHeight = 900;

    // [gui.display] defaults
    // WordStar 7's own real default is level 4 (pull-down bar). WordTsar's
    // pull-down bar is always shown regardless of level though, so level 4
    // would only subtract the classic Edit Menu panel without adding
    // anything level 3 doesn't already have -- level 3 (classic menu +
    // submenus, matching what's actually useful for relearning WordStar)
    // is the more sensible default for this shell.
    mGuiShowHelp = 3;
    mGuiShowRuler = true;
    mGuiShowScrollBar = true;
    mGuiShowStatusBar = true;
    mGuiShowStyleBar = true;
    mGuiShowMenu = true;
    mGuiAlwaysDotCommands = true;
    mGuiAlwaysFlagColumn = true;

    // [gui.colors] defaults (matching original WordTsar color scheme)
    mGuiBackground = {245, 245, 245};
    mGuiForeground = {0, 0, 0};
    mGuiHighlightBackground = {0, 150, 200};
    mGuiHighlightForeground = {0, 0, 0};
    mGuiDotBackground = {100, 200, 200};
    mGuiDotForeground = {0, 0, 0};
    mGuiBlockBackground = {50, 100, 200};
    mGuiBlockForeground = {0, 0, 0};
    mGuiCommentBackground = {255, 178, 102};
    mGuiCommentForeground = {0, 0, 0};
    mGuiErrorBackground = {194, 70, 65};
    mGuiErrorForeground = {255, 255, 255};
    mGuiUnknownBackground = {194, 100, 0};
    mGuiUnknownForeground = {255, 255, 255};
    mGuiNotImplementedBackground = {205, 192, 3};
    mGuiNotImplementedForeground = {0, 0, 0};
    mGuiSearchBackground = {50, 100, 200};
    mGuiSearchForeground = {0, 0, 0};

    // [gui.screen] defaults
    mGuiStatusBarForeground = {0, 0, 0};
    mGuiStatusBarBackground = {240, 240, 240};
    mGuiHelpPanelForeground = {0, 0, 0};
    mGuiHelpPanelBackground = {245, 245, 245};
    mGuiHelpPanelKeystrokeForeground = {0, 0, 180};
    mGuiHelpPanelKeystrokeBackground = {245, 245, 245};
    mGuiRulerForeground = {0, 0, 0};
    mGuiRulerBackground = {255, 255, 255};

    // [tui.display] defaults
    mTuiShowTitleBar = true;
    mTuiShowHelp = 3;                   // see the matching mGuiShowHelp comment above
    mTuiShowRuler = true;
    mTuiShowScrollBar = true;
    mTuiShowStatusBar = true;
    mTuiShowStyleBar = true;
    mTuiShowMenu = true;
    mTuiAlwaysDotCommands = true;
    mTuiAlwaysFlagColumn = true;
    mTuiCenterView = false;

    // [tui.colors] defaults (CGA blue bg, CGA gray text)
    mTuiBackground = {0, 0, 170};
    mTuiForeground = {170, 170, 170};
    mTuiHighlightBackground = {100, 200, 255};
    mTuiHighlightForeground = {0, 0, 0};
    mTuiDotBackground = {100, 220, 180};
    mTuiDotForeground = {170, 170, 170};
    mTuiBlockBackground = {80, 130, 255};
    mTuiBlockForeground = {255, 255, 255};
    mTuiCommentBackground = {255, 191, 0};
    mTuiCommentForeground = {0, 0, 0};
    mTuiErrorBackground = {255, 100, 100};
    mTuiErrorForeground = {0, 0, 0};
    mTuiUnknownBackground = {255, 255, 0};
    mTuiUnknownForeground = {0, 0, 0};
    mTuiNotImplementedBackground = {220, 200, 80};
    mTuiNotImplementedForeground = {0, 0, 0};
    mTuiSearchBackground = {80, 130, 255};
    mTuiSearchForeground = {255, 255, 255};

    // [tui.styleColors] defaults (foreground only used when terminal can't display attribute)
    mTuiBoldForeground = {0, 0, 180};
    mTuiBoldBackground = {0, 0, 170};
    mTuiItalicForeground = {0, 128, 128};
    mTuiItalicBackground = {0, 0, 170};
    mTuiUnderlineForeground = {0, 0, 180};
    mTuiUnderlineBackground = {0, 0, 170};
    mTuiStrikethroughForeground = {180, 0, 0};
    mTuiStrikethroughBackground = {0, 0, 170};
    mTuiSuperscriptForeground = {128, 0, 128};
    mTuiSuperscriptBackground = {0, 0, 170};
    mTuiSubscriptForeground = {0, 128, 0};
    mTuiSubscriptBackground = {0, 0, 170};

    // [tui.screen] defaults (match current derived CGA blue/gray scheme)
    mTuiTitleBarForeground = {0, 0, 170};
    mTuiTitleBarBackground = {170, 170, 170};
    mTuiStatusBarForeground = {0, 0, 170};
    mTuiStatusBarBackground = {170, 170, 170};
    mTuiHelpPanelForeground = {0, 0, 34};
    mTuiHelpPanelBackground = {170, 170, 170};
    mTuiHelpPanelKeystrokeForeground = {0, 0, 180};
    mTuiHelpPanelKeystrokeBackground = {170, 170, 170};
    mTuiMenuBarForeground = {0, 0, 34};
    mTuiMenuBarBackground = {170, 170, 170};
    mTuiMenuAcceleratorForeground = {170, 0, 0};
    mTuiMenuAcceleratorBackground = {170, 170, 170};
    mTuiMenuHighlightForeground = {170, 170, 170};
    mTuiMenuHighlightBackground = {0, 0, 170};
    mTuiRulerForeground = {170, 170, 170};
    mTuiRulerBackground = {0, 0, 0};
    mTuiFlagColumnForeground = {170, 170, 170};
    mTuiFlagColumnBackground = {0, 0, 0};
    mTuiScrollbarForeground = {0, 0, 170};
    mTuiScrollbarBackground = {85, 85, 128};
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return full path to config file (creates directory if needed)
///
/// @brief
/// Returns the platform-standard config file path.
/// Linux:   $XDG_CONFIG_HOME/wordtsar/config.ini (default ~/.config/wordtsar/config.ini)
/// macOS:   ~/Library/Application Support/WordTsar/config.ini
/// Windows: %APPDATA%\WordTsar\config.ini
///
/////////////////////////////////////////////////////////////////////////////
std::string cConfig::GetConfigDir(void)
{
    std::string configDir;

#ifdef _WIN32
    // Windows: use %APPDATA%
    const char* appdata = std::getenv("APPDATA");
    if (appdata != nullptr)
    {
        configDir = std::string(appdata) + "\\WordTsar";
    }
    else
    {
        // Fallback to current directory
        configDir = ".";
    }
#elif defined(__APPLE__)
    // macOS: ~/Library/Application Support/WordTsar
    const char* home = std::getenv("HOME");
    if (home != nullptr)
    {
        configDir = std::string(home) + "/Library/Application Support/WordTsar";
    }
    else
    {
        configDir = ".";
    }
#else
    // Linux: $XDG_CONFIG_HOME/wordtsar (default: ~/.config/wordtsar)
    const char* xdgConfig = std::getenv("XDG_CONFIG_HOME");
    if (xdgConfig != nullptr && xdgConfig[0] != '\0')
    {
        configDir = std::string(xdgConfig) + "/wordtsar";
    }
    else
    {
        const char* home = std::getenv("HOME");
        if (home != nullptr)
        {
            configDir = std::string(home) + "/.config/wordtsar";
        }
        else
        {
            configDir = ".";
        }
    }
#endif

    // Create directory if it doesn't exist
    std::filesystem::create_directories(configDir);

    return configDir;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - full path to config.ini
///
/// @brief
/// Returns the platform-standard config file path, creating the
/// directory if it doesn't exist.
///
/////////////////////////////////////////////////////////////////////////////
std::string cConfig::GetConfigFilePath(void)
{
#ifdef _WIN32
    return GetConfigDir() + "\\config.ini";
#else
    return GetConfigDir() + "/config.ini";
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - full path to gui-palettes.ini
///
/// @brief
/// Returns the path to the custom GUI palette file.
///
/////////////////////////////////////////////////////////////////////////////
std::string cConfig::GetGUIPaletteFilePath(void)
{
#ifdef _WIN32
    return GetConfigDir() + "\\gui-palettes.ini";
#else
    return GetConfigDir() + "/gui-palettes.ini";
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - full path to tui-palettes.ini
///
/// @brief
/// Returns the path to the custom TUI palette file.
///
/////////////////////////////////////////////////////////////////////////////
std::string cConfig::GetTUIPaletteFilePath(void)
{
#ifdef _WIN32
    return GetConfigDir() + "\\tui-palettes.ini";
#else
    return GetConfigDir() + "/tui-palettes.ini";
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  value [in] string in "R, G, B" format
/// @param  r [out] red component (0-255)
/// @param  g [out] green component (0-255)
/// @param  b [out] blue component (0-255)
///
/// @return true if parse succeeded, false on error
///
/// @brief
/// Parse an "R, G, B" color string into its components.
///
/////////////////////////////////////////////////////////////////////////////
bool cConfig::ParseRGB(const char* value, short& r, short& g, short& b)
{
    if (value == nullptr || value[0] == '\0')
    {
        return false;
    }

    int ri = 0, gi = 0, bi = 0;
    int count = std::sscanf(value, "%d , %d , %d", &ri, &gi, &bi);
    if (count != 3)
    {
        return false;
    }

    // Clamp to 0-255
    r = static_cast<short>(std::max(0, std::min(255, ri)));
    g = static_cast<short>(std::max(0, std::min(255, gi)));
    b = static_cast<short>(std::max(0, std::min(255, bi)));

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  r [in] red component (0-255)
/// @param  g [in] green component (0-255)
/// @param  b [in] blue component (0-255)
///
/// @return formatted "R, G, B" string
///
/// @brief
/// Format RGB components into an "R, G, B" string for INI storage.
///
/////////////////////////////////////////////////////////////////////////////
std::string cConfig::FormatRGB(short r, short g, short b)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d, %d, %d", r, g, b);
    return std::string(buf);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  value [in] mMeasurement string with optional unit suffix
///
/// @return value in twips
///
/// @brief
/// Parse a mMeasurement string with unit suffix into twips.
/// Suffixes: i or " = inches, c = cm, m = mm, p = points.
/// No suffix defaults to inches.
/// Uses same constants as dot command parsing (TWIPSPERINCH, etc.).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cConfig::ParseMeasurement(const char* value)
{
    if (value == nullptr || value[0] == '\0')
    {
        return 0;
    }

    // Parse the numeric part
    char* endPtr = nullptr;
    double numericValue = std::strtod(value, &endPtr);

    if (endPtr == value)
    {
        // No numeric value found
        return 0;
    }

    // Skip whitespace after number
    while (*endPtr == ' ')
    {
        endPtr++;
    }

    // Read unit suffix
    char unit = *endPtr;

    // Convert to twips based on unit
    double twips = 0.0;
    switch (unit)
    {
        case 'c':
        case 'C':
        {
            // Centimeters
            twips = numericValue * TWIPSPERCM;
            break;
        }

        case 'm':
        case 'M':
        {
            // Millimeters
            twips = numericValue * TWIPSPERMM;
            break;
        }

        case 'p':
        case 'P':
        {
            // Points
            twips = numericValue * POINTSTOTWIPS;
            break;
        }

        case 'i':
        case 'I':
        case '"':
        case '\0':
        default:
        {
            // Inches (default)
            twips = numericValue * TWIPSPERINCH;
            break;
        }
    }

    return static_cast<COORD_T>(twips);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  twips [in] value in twips
/// @param  unit [in] desired unit character (i, c, m, p)
///
/// @return formatted string with unit suffix (e.g., "8.50i")
///
/// @brief
/// Format a twips value as a mMeasurement string with unit suffix.
///
/////////////////////////////////////////////////////////////////////////////
std::string cConfig::FormatMeasurement(COORD_T twips, char unit)
{
    double value = 0.0;

    switch (unit)
    {
        case 'c':
        case 'C':
        {
            value = twips / TWIPSPERCM;
            break;
        }

        case 'm':
        case 'M':
        {
            value = twips / TWIPSPERMM;
            break;
        }

        case 'p':
        case 'P':
        {
            value = twips / POINTSTOTWIPS;
            break;
        }

        case 'i':
        case 'I':
        default:
        {
            value = twips / TWIPSPERINCH;
            unit = 'i';
            break;
        }
    }

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.2f%c", value, unit);
    return std::string(buf);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  a [in] first color channel value
/// @param  b [in] second color channel value
/// @param  t [in] blend factor (0.0 = a, 1.0 = b)
///
/// @return blended channel value
///
/// @brief
/// Linear blend between two color channel values.
/// Formula: a + (b - a) * t
///
/////////////////////////////////////////////////////////////////////////////
short cConfig::BlendChannel(short a, short b, float t)
{
    float result = static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t;
    return static_cast<short>(std::max(0.0f, std::min(255.0f, result)));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ini [in] pointer to CSimpleIniA instance
/// @param  section [in] INI section name
/// @param  key [in] INI key name
/// @param  defaultVal [in] default RGB value if key not found
///
/// @return parsed sRGB value
///
/// @brief
/// Read an RGB color from the INI file.
///
/////////////////////////////////////////////////////////////////////////////
sRGB cConfig::ReadRGB(void* ini, const char* section, const char* key, sRGB defaultVal)
{
    CSimpleIniA* pIni = static_cast<CSimpleIniA*>(ini);
    const char* value = pIni->GetValue(section, key, nullptr);

    if (value == nullptr)
    {
        return defaultVal;
    }

    sRGB result;
    if (!ParseRGB(value, result.r, result.g, result.b))
    {
        return defaultVal;
    }

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ini [in] pointer to CSimpleIniA instance
/// @param  section [in] INI section name
/// @param  key [in] INI key name
/// @param  color [in] RGB color to write
///
/// @return nothing
///
/// @brief
/// Write an RGB color to the INI file as "R, G, B" string.
///
/////////////////////////////////////////////////////////////////////////////
void cConfig::WriteRGB(void* ini, const char* section, const char* key, const sRGB& color)
{
    CSimpleIniA* pIni = static_cast<CSimpleIniA*>(ini);
    std::string formatted = FormatRGB(color.r, color.g, color.b);
    pIni->SetValue(section, key, formatted.c_str());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ini [in] pointer to CSimpleIniA instance
/// @param  section [in] INI section name
/// @param  key [in] INI key name
/// @param  defaultVal [in] default mMeasurement string if key not found
///
/// @return value in twips
///
/// @brief
/// Read a mMeasurement value from the INI file.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cConfig::ReadMeasurement(void* ini, const char* section, const char* key, const char* defaultVal)
{
    CSimpleIniA* pIni = static_cast<CSimpleIniA*>(ini);
    const char* value = pIni->GetValue(section, key, defaultVal);
    return ParseMeasurement(value);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  ini [in] pointer to CSimpleIniA instance
/// @param  section [in] INI section name
/// @param  key [in] INI key name
/// @param  twips [in] value in twips
/// @param  unit [in] unit suffix character
///
/// @return nothing
///
/// @brief
/// Write a mMeasurement value to the INI file with unit suffix.
///
/////////////////////////////////////////////////////////////////////////////
void cConfig::WriteMeasurement(void* ini, const char* section, const char* key, COORD_T twips, char unit)
{
    CSimpleIniA* pIni = static_cast<CSimpleIniA*>(ini);
    std::string formatted = FormatMeasurement(twips, unit);
    pIni->SetValue(section, key, formatted.c_str());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if file loaded successfully, false if file doesn't exist
///
/// @brief
/// Load all settings from the config file. If the file doesn't exist,
/// constructor defaults are retained and false is returned.
///
/////////////////////////////////////////////////////////////////////////////
bool cConfig::Load(void)
{
    mConfigPath = GetConfigFilePath();

    CSimpleIniA ini;
    ini.SetUnicode();

    if (ini.LoadFile(mConfigPath.c_str()) < 0)
    {
        // File doesn't exist -- keep constructor defaults
        return false;
    }

    // --- [editor] ---
    mInputMode = static_cast<int>(ini.GetLongValue("editor", "inputMode", mInputMode));
    mShowControls = ini.GetBoolValue("editor", "showControls", mShowControls);
    mCodePage = static_cast<int>(ini.GetLongValue("editor", "codePage", mCodePage));
    mMeasurement = ini.GetValue("editor", "measurement", mMeasurement.c_str());
    mDefaultFont = ini.GetValue("editor", "defaultFont", mDefaultFont.c_str());
    mDefaultFontSize = ini.GetDoubleValue("editor", "defaultFontSize", mDefaultFontSize);
    mDefaultFormat = ini.GetValue("editor", "defaultFormat", mDefaultFormat.c_str());
    mAutoSaveInterval = static_cast<int>(ini.GetLongValue("editor", "autoSaveInterval", mAutoSaveInterval));
    mCaretBlinkRate = static_cast<int>(ini.GetLongValue("editor", "caretBlinkRate", mCaretBlinkRate));
    mSpellCheckLanguage = ini.GetValue("editor", "spellCheckLanguage", mSpellCheckLanguage.c_str());
    mSpellCheckDotCommands = ini.GetBoolValue("editor", "spellCheckDotCommands", mSpellCheckDotCommands);
    mDefaultDirectory = ini.GetValue("editor", "defaultDirectory", mDefaultDirectory.c_str());

    // --- [editor.pageSetup] ---
    mPaperWidth = ReadMeasurement(&ini, "editor.pageSetup", "paperWidth", "8.5i");
    mPaperHeight = ReadMeasurement(&ini, "editor.pageSetup", "paperHeight", "11.0i");
    mPageOffsetOdd = ReadMeasurement(&ini, "editor.pageSetup", "pageOffsetOdd", "1.0i");
    mPageOffsetEven = ReadMeasurement(&ini, "editor.pageSetup", "pageOffsetEven", "1.0i");
    mLeftMargin = ReadMeasurement(&ini, "editor.pageSetup", "leftMargin", "0.0i");
    mRightMargin = ReadMeasurement(&ini, "editor.pageSetup", "rightMargin", "6.5i");
    mTopMargin = ReadMeasurement(&ini, "editor.pageSetup", "topMargin", "1.0i");
    mBottomMargin = ReadMeasurement(&ini, "editor.pageSetup", "bottomMargin", "1.0i");
    mHeaderMargin = ReadMeasurement(&ini, "editor.pageSetup", "headerMargin", "0.33i");
    mFooterMargin = ReadMeasurement(&ini, "editor.pageSetup", "footerMargin", "0.33i");
    mLandscape = ini.GetBoolValue("editor.pageSetup", "landscape", mLandscape);

    // --- [user] ---
    mShortName = ini.GetValue("user", "shortName", mShortName.c_str());
    mLongName = ini.GetValue("user", "longName", mLongName.c_str());

    // --- [gui] ---
    mWindowWidth = static_cast<int>(ini.GetLongValue("gui", "windowWidth", mWindowWidth));
    mWindowHeight = static_cast<int>(ini.GetLongValue("gui", "windowHeight", mWindowHeight));

    // --- [gui.display] ---
    mGuiShowHelp = static_cast<int>(ini.GetLongValue("gui.display", "showHelp", mGuiShowHelp));
    mGuiShowRuler = ini.GetBoolValue("gui.display", "showRuler", mGuiShowRuler);
    mGuiShowScrollBar = ini.GetBoolValue("gui.display", "showScrollBar", mGuiShowScrollBar);
    mGuiShowStatusBar = ini.GetBoolValue("gui.display", "showStatusBar", mGuiShowStatusBar);
    mGuiShowStyleBar = ini.GetBoolValue("gui.display", "showStyleBar", mGuiShowStyleBar);
    mGuiShowMenu = ini.GetBoolValue("gui.display", "showMenu", mGuiShowMenu);
    mGuiAlwaysDotCommands = ini.GetBoolValue("gui.display", "alwaysDotCommands", mGuiAlwaysDotCommands);
    mGuiAlwaysFlagColumn = ini.GetBoolValue("gui.display", "alwaysFlagColumn", mGuiAlwaysFlagColumn);

    // --- [gui.colors] ---
    mGuiBackground = ReadRGB(&ini, "gui.colors", "background", mGuiBackground);
    mGuiForeground = ReadRGB(&ini, "gui.colors", "foreground", mGuiForeground);
    mGuiHighlightBackground = ReadRGB(&ini, "gui.colors", "highlightBackground", mGuiHighlightBackground);
    mGuiHighlightForeground = ReadRGB(&ini, "gui.colors", "highlightForeground", mGuiHighlightForeground);
    mGuiDotBackground = ReadRGB(&ini, "gui.colors", "dotBackground", mGuiDotBackground);
    mGuiDotForeground = ReadRGB(&ini, "gui.colors", "dotForeground", mGuiDotForeground);
    mGuiBlockBackground = ReadRGB(&ini, "gui.colors", "blockBackground", mGuiBlockBackground);
    mGuiBlockForeground = ReadRGB(&ini, "gui.colors", "blockForeground", mGuiBlockForeground);
    mGuiCommentBackground = ReadRGB(&ini, "gui.colors", "commentBackground", mGuiCommentBackground);
    mGuiCommentForeground = ReadRGB(&ini, "gui.colors", "commentForeground", mGuiCommentForeground);
    mGuiErrorBackground = ReadRGB(&ini, "gui.colors", "errorBackground", mGuiErrorBackground);
    mGuiErrorForeground = ReadRGB(&ini, "gui.colors", "errorForeground", mGuiErrorForeground);
    mGuiUnknownBackground = ReadRGB(&ini, "gui.colors", "unknownBackground", mGuiUnknownBackground);
    mGuiUnknownForeground = ReadRGB(&ini, "gui.colors", "unknownForeground", mGuiUnknownForeground);
    mGuiNotImplementedBackground = ReadRGB(&ini, "gui.colors", "notImplementedBackground", mGuiNotImplementedBackground);
    mGuiNotImplementedForeground = ReadRGB(&ini, "gui.colors", "notImplementedForeground", mGuiNotImplementedForeground);
    mGuiSearchBackground = ReadRGB(&ini, "gui.colors", "searchBackground", mGuiSearchBackground);
    mGuiSearchForeground = ReadRGB(&ini, "gui.colors", "searchForeground", mGuiSearchForeground);

    // --- [gui.screen] ---
    mGuiStatusBarForeground = ReadRGB(&ini, "gui.screen", "statusBarForeground", mGuiStatusBarForeground);
    mGuiStatusBarBackground = ReadRGB(&ini, "gui.screen", "statusBarBackground", mGuiStatusBarBackground);
    mGuiHelpPanelForeground = ReadRGB(&ini, "gui.screen", "helpPanelForeground", mGuiHelpPanelForeground);
    mGuiHelpPanelBackground = ReadRGB(&ini, "gui.screen", "helpPanelBackground", mGuiHelpPanelBackground);
    mGuiHelpPanelKeystrokeForeground = ReadRGB(&ini, "gui.screen", "helpPanelKeystrokeForeground", mGuiHelpPanelKeystrokeForeground);
    mGuiHelpPanelKeystrokeBackground = ReadRGB(&ini, "gui.screen", "helpPanelKeystrokeBackground", mGuiHelpPanelKeystrokeBackground);
    mGuiRulerForeground = ReadRGB(&ini, "gui.screen", "rulerForeground", mGuiRulerForeground);
    mGuiRulerBackground = ReadRGB(&ini, "gui.screen", "rulerBackground", mGuiRulerBackground);

    // --- [tui.display] ---
    mTuiShowTitleBar = ini.GetBoolValue("tui.display", "showTitleBar", mTuiShowTitleBar);
    mTuiShowHelp = static_cast<int>(ini.GetLongValue("tui.display", "showHelp", mTuiShowHelp));
    mTuiShowRuler = ini.GetBoolValue("tui.display", "showRuler", mTuiShowRuler);
    mTuiShowScrollBar = ini.GetBoolValue("tui.display", "showScrollBar", mTuiShowScrollBar);
    mTuiShowStatusBar = ini.GetBoolValue("tui.display", "showStatusBar", mTuiShowStatusBar);
    mTuiShowStyleBar = ini.GetBoolValue("tui.display", "showStyleBar", mTuiShowStyleBar);
    mTuiShowMenu = ini.GetBoolValue("tui.display", "showMenu", mTuiShowMenu);
    mTuiAlwaysDotCommands = ini.GetBoolValue("tui.display", "alwaysDotCommands", mTuiAlwaysDotCommands);
    mTuiAlwaysFlagColumn = ini.GetBoolValue("tui.display", "alwaysFlagColumn", mTuiAlwaysFlagColumn);
    mTuiCenterView = ini.GetBoolValue("tui.display", "centerView", mTuiCenterView);

    // --- [tui.colors] ---
    mTuiBackground = ReadRGB(&ini, "tui.colors", "background", mTuiBackground);
    mTuiForeground = ReadRGB(&ini, "tui.colors", "foreground", mTuiForeground);
    mTuiHighlightBackground = ReadRGB(&ini, "tui.colors", "highlightBackground", mTuiHighlightBackground);
    mTuiHighlightForeground = ReadRGB(&ini, "tui.colors", "highlightForeground", mTuiHighlightForeground);
    mTuiDotBackground = ReadRGB(&ini, "tui.colors", "dotBackground", mTuiDotBackground);
    mTuiDotForeground = ReadRGB(&ini, "tui.colors", "dotForeground", mTuiDotForeground);
    mTuiBlockBackground = ReadRGB(&ini, "tui.colors", "blockBackground", mTuiBlockBackground);
    mTuiBlockForeground = ReadRGB(&ini, "tui.colors", "blockForeground", mTuiBlockForeground);
    mTuiCommentBackground = ReadRGB(&ini, "tui.colors", "commentBackground", mTuiCommentBackground);
    mTuiCommentForeground = ReadRGB(&ini, "tui.colors", "commentForeground", mTuiCommentForeground);
    mTuiErrorBackground = ReadRGB(&ini, "tui.colors", "errorBackground", mTuiErrorBackground);
    mTuiErrorForeground = ReadRGB(&ini, "tui.colors", "errorForeground", mTuiErrorForeground);
    mTuiUnknownBackground = ReadRGB(&ini, "tui.colors", "unknownBackground", mTuiUnknownBackground);
    mTuiUnknownForeground = ReadRGB(&ini, "tui.colors", "unknownForeground", mTuiUnknownForeground);
    mTuiNotImplementedBackground = ReadRGB(&ini, "tui.colors", "notImplementedBackground", mTuiNotImplementedBackground);
    mTuiNotImplementedForeground = ReadRGB(&ini, "tui.colors", "notImplementedForeground", mTuiNotImplementedForeground);
    mTuiSearchBackground = ReadRGB(&ini, "tui.colors", "searchBackground", mTuiSearchBackground);
    mTuiSearchForeground = ReadRGB(&ini, "tui.colors", "searchForeground", mTuiSearchForeground);

    // --- [tui.styleColors] ---
    mTuiBoldForeground = ReadRGB(&ini, "tui.styleColors", "boldForeground", mTuiBoldForeground);
    mTuiBoldBackground = ReadRGB(&ini, "tui.styleColors", "boldBackground", mTuiBoldBackground);
    mTuiItalicForeground = ReadRGB(&ini, "tui.styleColors", "italicForeground", mTuiItalicForeground);
    mTuiItalicBackground = ReadRGB(&ini, "tui.styleColors", "italicBackground", mTuiItalicBackground);
    mTuiUnderlineForeground = ReadRGB(&ini, "tui.styleColors", "underlineForeground", mTuiUnderlineForeground);
    mTuiUnderlineBackground = ReadRGB(&ini, "tui.styleColors", "underlineBackground", mTuiUnderlineBackground);
    mTuiStrikethroughForeground = ReadRGB(&ini, "tui.styleColors", "strikethroughForeground", mTuiStrikethroughForeground);
    mTuiStrikethroughBackground = ReadRGB(&ini, "tui.styleColors", "strikethroughBackground", mTuiStrikethroughBackground);
    mTuiSuperscriptForeground = ReadRGB(&ini, "tui.styleColors", "superscriptForeground", mTuiSuperscriptForeground);
    mTuiSuperscriptBackground = ReadRGB(&ini, "tui.styleColors", "superscriptBackground", mTuiSuperscriptBackground);
    mTuiSubscriptForeground = ReadRGB(&ini, "tui.styleColors", "subscriptForeground", mTuiSubscriptForeground);
    mTuiSubscriptBackground = ReadRGB(&ini, "tui.styleColors", "subscriptBackground", mTuiSubscriptBackground);

    // --- [tui.screen] ---
    mTuiTitleBarForeground = ReadRGB(&ini, "tui.screen", "titleBarForeground", mTuiTitleBarForeground);
    mTuiTitleBarBackground = ReadRGB(&ini, "tui.screen", "titleBarBackground", mTuiTitleBarBackground);
    mTuiStatusBarForeground = ReadRGB(&ini, "tui.screen", "statusBarForeground", mTuiStatusBarForeground);
    mTuiStatusBarBackground = ReadRGB(&ini, "tui.screen", "statusBarBackground", mTuiStatusBarBackground);
    mTuiHelpPanelForeground = ReadRGB(&ini, "tui.screen", "helpPanelForeground", mTuiHelpPanelForeground);
    mTuiHelpPanelBackground = ReadRGB(&ini, "tui.screen", "helpPanelBackground", mTuiHelpPanelBackground);
    mTuiHelpPanelKeystrokeForeground = ReadRGB(&ini, "tui.screen", "helpPanelKeystrokeForeground", mTuiHelpPanelKeystrokeForeground);
    mTuiHelpPanelKeystrokeBackground = ReadRGB(&ini, "tui.screen", "helpPanelKeystrokeBackground", mTuiHelpPanelKeystrokeBackground);
    mTuiMenuBarForeground = ReadRGB(&ini, "tui.screen", "menuBarForeground", mTuiMenuBarForeground);
    mTuiMenuBarBackground = ReadRGB(&ini, "tui.screen", "menuBarBackground", mTuiMenuBarBackground);
    mTuiMenuAcceleratorForeground = ReadRGB(&ini, "tui.screen", "menuAcceleratorForeground", mTuiMenuAcceleratorForeground);
    mTuiMenuAcceleratorBackground = ReadRGB(&ini, "tui.screen", "menuAcceleratorBackground", mTuiMenuAcceleratorBackground);
    mTuiMenuHighlightForeground = ReadRGB(&ini, "tui.screen", "menuHighlightForeground", mTuiMenuHighlightForeground);
    mTuiMenuHighlightBackground = ReadRGB(&ini, "tui.screen", "menuHighlightBackground", mTuiMenuHighlightBackground);
    mTuiRulerForeground = ReadRGB(&ini, "tui.screen", "rulerForeground", mTuiRulerForeground);
    mTuiRulerBackground = ReadRGB(&ini, "tui.screen", "rulerBackground", mTuiRulerBackground);
    mTuiFlagColumnForeground = ReadRGB(&ini, "tui.screen", "flagColumnForeground", mTuiFlagColumnForeground);
    mTuiFlagColumnBackground = ReadRGB(&ini, "tui.screen", "flagColumnBackground", mTuiFlagColumnBackground);
    mTuiScrollbarForeground = ReadRGB(&ini, "tui.screen", "scrollbarForeground", mTuiScrollbarForeground);
    mTuiScrollbarBackground = ReadRGB(&ini, "tui.screen", "scrollbarBackground", mTuiScrollbarBackground);

    // --- [mRecentFiles] ---
    for (int i = 0; i < 10; i++)
    {
        std::string key = std::to_string(i + 1);
        mRecentFiles[i] = ini.GetValue("recentFiles", key.c_str(), "");
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true on success
///
/// @brief
/// Save all settings to the config file. If the file doesn't exist yet,
/// a commented template is written first to help users who hand-edit
/// the INI file. Then all values are written via SimpleIni.
///
/////////////////////////////////////////////////////////////////////////////
bool cConfig::Save(void)
{
    if (mConfigPath.empty())
    {
        mConfigPath = GetConfigFilePath();
    }

    // Check if this is a first-time creation
    bool isNewFile = !std::filesystem::exists(mConfigPath);

    // If brand new, write the commented template first
    if (isNewFile)
    {
        WriteCommentedTemplate(mConfigPath);
    }

    // Load existing file (preserves user comments from template or hand edits)
    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(mConfigPath.c_str());

    // --- [editor] ---
    ini.SetLongValue("editor", "inputMode", mInputMode);
    ini.SetBoolValue("editor", "showControls", mShowControls);
    ini.SetLongValue("editor", "codePage", mCodePage);
    ini.SetValue("editor", "measurement", mMeasurement.c_str());
    ini.SetValue("editor", "defaultFont", mDefaultFont.c_str());
    ini.SetDoubleValue("editor", "defaultFontSize", mDefaultFontSize);
    ini.SetValue("editor", "defaultFormat", mDefaultFormat.c_str());
    ini.SetLongValue("editor", "autoSaveInterval", mAutoSaveInterval);
    ini.SetLongValue("editor", "caretBlinkRate", mCaretBlinkRate);
    ini.SetValue("editor", "spellCheckLanguage", mSpellCheckLanguage.c_str());
    ini.SetBoolValue("editor", "spellCheckDotCommands", mSpellCheckDotCommands);
    ini.SetValue("editor", "defaultDirectory", mDefaultDirectory.c_str());

    // --- [editor.pageSetup] ---
    WriteMeasurement(&ini, "editor.pageSetup", "paperWidth", mPaperWidth, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "paperHeight", mPaperHeight, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "pageOffsetOdd", mPageOffsetOdd, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "pageOffsetEven", mPageOffsetEven, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "leftMargin", mLeftMargin, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "rightMargin", mRightMargin, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "topMargin", mTopMargin, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "bottomMargin", mBottomMargin, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "headerMargin", mHeaderMargin, 'i');
    WriteMeasurement(&ini, "editor.pageSetup", "footerMargin", mFooterMargin, 'i');
    ini.SetBoolValue("editor.pageSetup", "landscape", mLandscape);

    // --- [user] ---
    ini.SetValue("user", "shortName", mShortName.c_str());
    ini.SetValue("user", "longName", mLongName.c_str());

    // --- [gui] ---
    ini.SetLongValue("gui", "windowWidth", mWindowWidth);
    ini.SetLongValue("gui", "windowHeight", mWindowHeight);

    // --- [gui.display] ---
    ini.SetLongValue("gui.display", "showHelp", mGuiShowHelp);
    ini.SetBoolValue("gui.display", "showRuler", mGuiShowRuler);
    ini.SetBoolValue("gui.display", "showScrollBar", mGuiShowScrollBar);
    ini.SetBoolValue("gui.display", "showStatusBar", mGuiShowStatusBar);
    ini.SetBoolValue("gui.display", "showStyleBar", mGuiShowStyleBar);
    ini.SetBoolValue("gui.display", "showMenu", mGuiShowMenu);
    ini.SetBoolValue("gui.display", "alwaysDotCommands", mGuiAlwaysDotCommands);
    ini.SetBoolValue("gui.display", "alwaysFlagColumn", mGuiAlwaysFlagColumn);

    // --- [gui.colors] ---
    WriteRGB(&ini, "gui.colors", "background", mGuiBackground);
    WriteRGB(&ini, "gui.colors", "foreground", mGuiForeground);
    WriteRGB(&ini, "gui.colors", "highlightBackground", mGuiHighlightBackground);
    WriteRGB(&ini, "gui.colors", "highlightForeground", mGuiHighlightForeground);
    WriteRGB(&ini, "gui.colors", "dotBackground", mGuiDotBackground);
    WriteRGB(&ini, "gui.colors", "dotForeground", mGuiDotForeground);
    WriteRGB(&ini, "gui.colors", "blockBackground", mGuiBlockBackground);
    WriteRGB(&ini, "gui.colors", "blockForeground", mGuiBlockForeground);
    WriteRGB(&ini, "gui.colors", "commentBackground", mGuiCommentBackground);
    WriteRGB(&ini, "gui.colors", "commentForeground", mGuiCommentForeground);
    WriteRGB(&ini, "gui.colors", "errorBackground", mGuiErrorBackground);
    WriteRGB(&ini, "gui.colors", "errorForeground", mGuiErrorForeground);
    WriteRGB(&ini, "gui.colors", "unknownBackground", mGuiUnknownBackground);
    WriteRGB(&ini, "gui.colors", "unknownForeground", mGuiUnknownForeground);
    WriteRGB(&ini, "gui.colors", "notImplementedBackground", mGuiNotImplementedBackground);
    WriteRGB(&ini, "gui.colors", "notImplementedForeground", mGuiNotImplementedForeground);
    WriteRGB(&ini, "gui.colors", "searchBackground", mGuiSearchBackground);
    WriteRGB(&ini, "gui.colors", "searchForeground", mGuiSearchForeground);

    // --- [gui.screen] ---
    WriteRGB(&ini, "gui.screen", "statusBarForeground", mGuiStatusBarForeground);
    WriteRGB(&ini, "gui.screen", "statusBarBackground", mGuiStatusBarBackground);
    WriteRGB(&ini, "gui.screen", "helpPanelForeground", mGuiHelpPanelForeground);
    WriteRGB(&ini, "gui.screen", "helpPanelBackground", mGuiHelpPanelBackground);
    WriteRGB(&ini, "gui.screen", "helpPanelKeystrokeForeground", mGuiHelpPanelKeystrokeForeground);
    WriteRGB(&ini, "gui.screen", "helpPanelKeystrokeBackground", mGuiHelpPanelKeystrokeBackground);
    WriteRGB(&ini, "gui.screen", "rulerForeground", mGuiRulerForeground);
    WriteRGB(&ini, "gui.screen", "rulerBackground", mGuiRulerBackground);

    // --- [tui.display] ---
    ini.SetBoolValue("tui.display", "showTitleBar", mTuiShowTitleBar);
    ini.SetLongValue("tui.display", "showHelp", mTuiShowHelp);
    ini.SetBoolValue("tui.display", "showRuler", mTuiShowRuler);
    ini.SetBoolValue("tui.display", "showScrollBar", mTuiShowScrollBar);
    ini.SetBoolValue("tui.display", "showStatusBar", mTuiShowStatusBar);
    ini.SetBoolValue("tui.display", "showStyleBar", mTuiShowStyleBar);
    ini.SetBoolValue("tui.display", "showMenu", mTuiShowMenu);
    ini.SetBoolValue("tui.display", "alwaysDotCommands", mTuiAlwaysDotCommands);
    ini.SetBoolValue("tui.display", "alwaysFlagColumn", mTuiAlwaysFlagColumn);
    ini.SetBoolValue("tui.display", "centerView", mTuiCenterView);

    // --- [tui.colors] ---
    WriteRGB(&ini, "tui.colors", "background", mTuiBackground);
    WriteRGB(&ini, "tui.colors", "foreground", mTuiForeground);
    WriteRGB(&ini, "tui.colors", "highlightBackground", mTuiHighlightBackground);
    WriteRGB(&ini, "tui.colors", "highlightForeground", mTuiHighlightForeground);
    WriteRGB(&ini, "tui.colors", "dotBackground", mTuiDotBackground);
    WriteRGB(&ini, "tui.colors", "dotForeground", mTuiDotForeground);
    WriteRGB(&ini, "tui.colors", "blockBackground", mTuiBlockBackground);
    WriteRGB(&ini, "tui.colors", "blockForeground", mTuiBlockForeground);
    WriteRGB(&ini, "tui.colors", "commentBackground", mTuiCommentBackground);
    WriteRGB(&ini, "tui.colors", "commentForeground", mTuiCommentForeground);
    WriteRGB(&ini, "tui.colors", "errorBackground", mTuiErrorBackground);
    WriteRGB(&ini, "tui.colors", "errorForeground", mTuiErrorForeground);
    WriteRGB(&ini, "tui.colors", "unknownBackground", mTuiUnknownBackground);
    WriteRGB(&ini, "tui.colors", "unknownForeground", mTuiUnknownForeground);
    WriteRGB(&ini, "tui.colors", "notImplementedBackground", mTuiNotImplementedBackground);
    WriteRGB(&ini, "tui.colors", "notImplementedForeground", mTuiNotImplementedForeground);
    WriteRGB(&ini, "tui.colors", "searchBackground", mTuiSearchBackground);
    WriteRGB(&ini, "tui.colors", "searchForeground", mTuiSearchForeground);

    // --- [tui.styleColors] ---
    WriteRGB(&ini, "tui.styleColors", "boldForeground", mTuiBoldForeground);
    WriteRGB(&ini, "tui.styleColors", "boldBackground", mTuiBoldBackground);
    WriteRGB(&ini, "tui.styleColors", "italicForeground", mTuiItalicForeground);
    WriteRGB(&ini, "tui.styleColors", "italicBackground", mTuiItalicBackground);
    WriteRGB(&ini, "tui.styleColors", "underlineForeground", mTuiUnderlineForeground);
    WriteRGB(&ini, "tui.styleColors", "underlineBackground", mTuiUnderlineBackground);
    WriteRGB(&ini, "tui.styleColors", "strikethroughForeground", mTuiStrikethroughForeground);
    WriteRGB(&ini, "tui.styleColors", "strikethroughBackground", mTuiStrikethroughBackground);
    WriteRGB(&ini, "tui.styleColors", "superscriptForeground", mTuiSuperscriptForeground);
    WriteRGB(&ini, "tui.styleColors", "superscriptBackground", mTuiSuperscriptBackground);
    WriteRGB(&ini, "tui.styleColors", "subscriptForeground", mTuiSubscriptForeground);
    WriteRGB(&ini, "tui.styleColors", "subscriptBackground", mTuiSubscriptBackground);

    // --- [tui.screen] ---
    WriteRGB(&ini, "tui.screen", "titleBarForeground", mTuiTitleBarForeground);
    WriteRGB(&ini, "tui.screen", "titleBarBackground", mTuiTitleBarBackground);
    WriteRGB(&ini, "tui.screen", "statusBarForeground", mTuiStatusBarForeground);
    WriteRGB(&ini, "tui.screen", "statusBarBackground", mTuiStatusBarBackground);
    WriteRGB(&ini, "tui.screen", "helpPanelForeground", mTuiHelpPanelForeground);
    WriteRGB(&ini, "tui.screen", "helpPanelBackground", mTuiHelpPanelBackground);
    WriteRGB(&ini, "tui.screen", "helpPanelKeystrokeForeground", mTuiHelpPanelKeystrokeForeground);
    WriteRGB(&ini, "tui.screen", "helpPanelKeystrokeBackground", mTuiHelpPanelKeystrokeBackground);
    WriteRGB(&ini, "tui.screen", "menuBarForeground", mTuiMenuBarForeground);
    WriteRGB(&ini, "tui.screen", "menuBarBackground", mTuiMenuBarBackground);
    WriteRGB(&ini, "tui.screen", "menuAcceleratorForeground", mTuiMenuAcceleratorForeground);
    WriteRGB(&ini, "tui.screen", "menuAcceleratorBackground", mTuiMenuAcceleratorBackground);
    WriteRGB(&ini, "tui.screen", "menuHighlightForeground", mTuiMenuHighlightForeground);
    WriteRGB(&ini, "tui.screen", "menuHighlightBackground", mTuiMenuHighlightBackground);
    WriteRGB(&ini, "tui.screen", "rulerForeground", mTuiRulerForeground);
    WriteRGB(&ini, "tui.screen", "rulerBackground", mTuiRulerBackground);
    WriteRGB(&ini, "tui.screen", "flagColumnForeground", mTuiFlagColumnForeground);
    WriteRGB(&ini, "tui.screen", "flagColumnBackground", mTuiFlagColumnBackground);
    WriteRGB(&ini, "tui.screen", "scrollbarForeground", mTuiScrollbarForeground);
    WriteRGB(&ini, "tui.screen", "scrollbarBackground", mTuiScrollbarBackground);

    // --- [mRecentFiles] ---
    for (int i = 0; i < 10; i++)
    {
        std::string key = std::to_string(i + 1);
        ini.SetValue("recentFiles", key.c_str(), mRecentFiles[i].c_str());
    }

    // Write to disk
    return ini.SaveFile(mConfigPath.c_str()) >= 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] full path of the file to add
///
/// @return nothing
///
/// @brief
/// Push a filename to the front of the recent files list. If the filename
/// already exists in the list, it is moved to position 0 and all entries
/// between 0 and its old position shift down by one. Otherwise, all entries
/// shift down by one and the last (10th) entry is dropped.
///
/////////////////////////////////////////////////////////////////////////////
void cConfig::AddRecentFile(const std::string& filename)
{
    if (filename.empty())
    {
        return;
    }

    // Check if the filename is already in the list
    int existingIdx = -1;
    for (int i = 0; i < 10; i++)
    {
        if (mRecentFiles[i] == filename)
        {
            existingIdx = i;
            break;
        }
    }

    if (existingIdx == 0)
    {
        // Already at the front -- nothing to do
        return;
    }

    // Determine how far to shift: if found, shift up to its position;
    // otherwise, shift the entire list (dropping the last entry)
    int shiftEnd = (existingIdx >= 0) ? existingIdx : 9;

    for (int i = shiftEnd; i > 0; i--)
    {
        mRecentFiles[i] = mRecentFiles[i - 1];
    }

    mRecentFiles[0] = filename;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  path [in] full path to config file
///
/// @return nothing
///
/// @brief
/// Write a commented INI template to help users who hand-edit the file.
/// Called only on first creation (file doesn't exist yet).
///
/////////////////////////////////////////////////////////////////////////////
void cConfig::WriteCommentedTemplate(const std::string& path)
{
    std::ofstream file(path);
    if (!file.is_open())
    {
        return;
    }

    file << "; WordTsar Configuration File\n";
    file << "; Edit values below to customize WordTsar. Lines starting with ; are comments.\n";
    file << "\n";
    file << "; Editor defaults -- shared between GUI and TUI\n";
    file << "; mShowControls: show formatting codes in document (true/false)\n";
    file << "; mCodePage: WordStar code page (0 = CP437, 1 = CP737)\n";
    file << "; mMeasurement: ruler/status display units (0i = inches, 0cm = centimeters)\n";
    file << "; mDefaultFormat: file format for saving (ws, rtf)\n";
    file << "; mAutoSaveInterval: seconds between auto-saves (0 = disabled)\n";
    file << "; mCaretBlinkRate: cursor blink speed in milliseconds\n";
    file << "[editor]\n";
    file << "\n";
    file << "; Default page setup for new documents\n";
    file << "; All measurements accept a unit suffix: i or \" = inches, c = cm, m = mm, p = points\n";
    file << "; No suffix defaults to inches\n";
    file << "; pageOffset = binding/left page margin (odd and even pages for bound documents)\n";
    file << "; mLeftMargin = additional paragraph indent from page offset\n";
    file << "; mRightMargin = text column width measured from the left margin position\n";
    file << "[editor.pageSetup]\n";
    file << "\n";
    file << "; User identity -- used in document metadata\n";
    file << "[user]\n";
    file << "\n";
    file << "; GUI window settings\n";
    file << "[gui]\n";
    file << "\n";
    file << "; GUI display options -- show/hide UI elements\n";
    file << "; showHelp: 0 = off, 1 = context only, 2 = always show help panel\n";
    file << "[gui.display]\n";
    file << "\n";
    file << "; GUI editor colors -- R, G, B values (0-255)\n";
    file << "; Each component has a foreground and background color\n";
    file << "; background/foreground = normal text\n";
    file << "; highlightBackground/highlightForeground = control code overlays\n";
    file << "; searchBackground/searchForeground = search result highlighting\n";
    file << "[gui.colors]\n";
    file << "\n";
    file << "; GUI screen element colors -- R, G, B values (0-255)\n";
    file << "; Status bar, help panel, ruler, etc.\n";
    file << "[gui.screen]\n";
    file << "\n";
    file << "; TUI display options -- show/hide UI elements\n";
    file << "; showTitleBar is TUI-only (GUI uses the OS window title bar)\n";
    file << "; showHelp: 0 = off, 1 = context only, 2 = always show help panel\n";
    file << "[tui.display]\n";
    file << "\n";
    file << "; TUI editor colors -- R, G, B values (0-255)\n";
    file << "; Each component has a foreground and background color\n";
    file << "[tui.colors]\n";
    file << "\n";
    file << "; TUI style fallback colors -- R, G, B values (0-255)\n";
    file << "; Each attribute has foreground and background color\n";
    file << "; Only used when the terminal cannot render the attribute natively\n";
    file << "; (e.g., bold text uses normal colors if terminal supports SGR bold)\n";
    file << "[tui.styleColors]\n";
    file << "\n";
    file << "; TUI screen element colors -- R, G, B values (0-255)\n";
    file << "; Title bar, status bar, help panel, menu, ruler, flag column, scrollbar\n";
    file << "[tui.screen]\n";
    file << "\n";
    file << "; Recently opened files (managed by WordTsar, not intended for hand editing)\n";
    file << "[mRecentFiles]\n";
    file << "\n";

    file.close();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return vector of built-in GUI palettes
///
/// @brief
/// Returns the built-in GUI color palettes. The first palette ("Default
/// Light") matches the constructor defaults exactly.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sGUIPalette> cConfig::GetGUIPalettes(void)
{
    std::vector<sGUIPalette> palettes;

    // "Default Light" -- matches current GUI defaults
    sGUIPalette defaultLight;
    defaultLight.name = "Default Light";
    // [gui.colors]
    defaultLight.background = {245, 245, 245};
    defaultLight.foreground = {0, 0, 0};
    defaultLight.highlightBackground = {0, 150, 200};
    defaultLight.highlightForeground = {0, 0, 0};
    defaultLight.dotBackground = {100, 200, 200};
    defaultLight.dotForeground = {0, 0, 0};
    defaultLight.blockBackground = {50, 100, 200};
    defaultLight.blockForeground = {0, 0, 0};
    defaultLight.commentBackground = {255, 178, 102};
    defaultLight.commentForeground = {0, 0, 0};
    defaultLight.errorBackground = {194, 70, 65};
    defaultLight.errorForeground = {255, 255, 255};
    defaultLight.unknownBackground = {194, 100, 0};
    defaultLight.unknownForeground = {255, 255, 255};
    defaultLight.notImplementedBackground = {205, 192, 3};
    defaultLight.notImplementedForeground = {0, 0, 0};
    defaultLight.searchBackground = {50, 100, 200};
    defaultLight.searchForeground = {0, 0, 0};
    // [gui.screen]
    defaultLight.statusBarForeground = {0, 0, 0};
    defaultLight.statusBarBackground = {240, 240, 240};
    defaultLight.helpPanelForeground = {0, 0, 0};
    defaultLight.helpPanelBackground = {245, 245, 245};
    defaultLight.helpPanelKeystrokeForeground = {0, 0, 180};
    defaultLight.helpPanelKeystrokeBackground = {245, 245, 245};
    defaultLight.rulerForeground = {0, 0, 0};
    defaultLight.rulerBackground = {255, 255, 255};
    palettes.push_back(defaultLight);

    // "Dark" -- placeholder dark mode theme
    sGUIPalette dark;
    dark.name = "Dark";
    // [gui.colors]
    dark.background = {30, 30, 30};
    dark.foreground = {212, 212, 212};
    dark.highlightBackground = {0, 100, 150};
    dark.highlightForeground = {212, 212, 212};
    dark.dotBackground = {50, 120, 100};
    dark.dotForeground = {212, 212, 212};
    dark.blockBackground = {40, 80, 160};
    dark.blockForeground = {255, 255, 255};
    dark.commentBackground = {100, 80, 40};
    dark.commentForeground = {212, 212, 212};
    dark.errorBackground = {140, 40, 40};
    dark.errorForeground = {255, 255, 255};
    dark.unknownBackground = {140, 80, 0};
    dark.unknownForeground = {255, 255, 255};
    dark.notImplementedBackground = {120, 110, 10};
    dark.notImplementedForeground = {212, 212, 212};
    dark.searchBackground = {40, 80, 160};
    dark.searchForeground = {255, 255, 255};
    // [gui.screen]
    dark.statusBarForeground = {200, 200, 200};
    dark.statusBarBackground = {50, 50, 50};
    dark.helpPanelForeground = {200, 200, 200};
    dark.helpPanelBackground = {40, 40, 40};
    dark.helpPanelKeystrokeForeground = {80, 180, 240};
    dark.helpPanelKeystrokeBackground = {40, 40, 40};
    dark.rulerForeground = {180, 180, 180};
    dark.rulerBackground = {45, 45, 45};
    palettes.push_back(dark);

    return palettes;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return vector of built-in TUI palettes
///
/// @brief
/// Returns the built-in TUI color palettes. The first palette ("CGA Blue")
/// matches the constructor defaults exactly.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sTUIPalette> cConfig::GetTUIPalettes(void)
{
    std::vector<sTUIPalette> palettes;

    // ---------------------------------------------------------------
    // Palette 0: "WordTsar" -- default blue theme with custom accents
    // Values match the cConfig constructor defaults and the user's
    // hand-tuned config.ini.
    // ---------------------------------------------------------------
    sTUIPalette wordtsar;
    wordtsar.name = "WordTsar";
    // Editor colors
    wordtsar.foreground               = {170, 170, 170};
    wordtsar.background               = {0, 0, 170};
    wordtsar.highlightForeground      = {0, 0, 0};
    wordtsar.highlightBackground      = {100, 200, 255};
    wordtsar.dotForeground            = {170, 170, 170};
    wordtsar.dotBackground            = {100, 220, 180};
    wordtsar.blockForeground          = {255, 255, 255};
    wordtsar.blockBackground          = {80, 130, 255};
    wordtsar.commentForeground        = {0, 0, 0};
    wordtsar.commentBackground        = {255, 191, 0};
    wordtsar.errorForeground          = {0, 0, 0};
    wordtsar.errorBackground          = {255, 100, 100};
    wordtsar.unknownForeground        = {0, 0, 0};
    wordtsar.unknownBackground        = {255, 255, 0};
    wordtsar.notImplementedForeground = {0, 0, 0};
    wordtsar.notImplementedBackground = {220, 200, 80};
    wordtsar.searchForeground         = {255, 255, 255};
    wordtsar.searchBackground         = {80, 130, 255};
    // Attribute colors
    wordtsar.boldForeground           = {255, 255, 255};
    wordtsar.boldBackground           = {0, 0, 170};
    wordtsar.italicForeground         = {0, 128, 128};
    wordtsar.italicBackground         = {0, 0, 170};
    wordtsar.underlineForeground      = {255, 105, 180};
    wordtsar.underlineBackground      = {0, 0, 170};
    wordtsar.strikethroughForeground  = {180, 0, 0};
    wordtsar.strikethroughBackground  = {0, 0, 170};
    wordtsar.superscriptForeground    = {128, 0, 128};
    wordtsar.superscriptBackground    = {0, 0, 170};
    wordtsar.subscriptForeground      = {0, 128, 0};
    wordtsar.subscriptBackground      = {0, 0, 170};
    // Screen colors
    wordtsar.titleBarForeground              = {0, 0, 170};
    wordtsar.titleBarBackground              = {170, 170, 170};
    wordtsar.statusBarForeground             = {0, 0, 170};
    wordtsar.statusBarBackground             = {170, 170, 170};
    wordtsar.helpPanelForeground             = {0, 0, 34};
    wordtsar.helpPanelBackground             = {170, 170, 170};
    wordtsar.helpPanelKeystrokeForeground    = {0, 0, 180};
    wordtsar.helpPanelKeystrokeBackground    = {170, 170, 170};
    wordtsar.menuBarForeground               = {0, 0, 34};
    wordtsar.menuBarBackground               = {170, 170, 170};
    wordtsar.menuAcceleratorForeground       = {170, 0, 0};
    wordtsar.menuAcceleratorBackground       = {170, 170, 170};
    wordtsar.menuHighlightForeground         = {170, 170, 170};
    wordtsar.menuHighlightBackground         = {0, 0, 170};
    wordtsar.rulerForeground                 = {170, 170, 170};
    wordtsar.rulerBackground                 = {0, 0, 0};
    wordtsar.flagColumnForeground            = {170, 170, 170};
    wordtsar.flagColumnBackground            = {0, 0, 0};
    wordtsar.scrollbarForeground             = {0, 0, 170};
    wordtsar.scrollbarBackground             = {85, 85, 128};
    palettes.push_back(wordtsar);

    // Standard CGA color values
    sRGB cgaBlack       = {0, 0, 0};
    sRGB cgaBlue        = {0, 0, 170};
    sRGB cgaGreen       = {0, 170, 0};
    sRGB cgaCyan        = {0, 170, 170};
    sRGB cgaRed         = {170, 0, 0};
    sRGB cgaMagenta     = {170, 0, 170};
    sRGB cgaWhite       = {170, 170, 170};
    sRGB cgaDarkGray    = {85, 85, 85};
    sRGB cgaLightBlue   = {85, 85, 255};
    sRGB cgaLightGreen  = {85, 255, 85};
    sRGB cgaLightCyan   = {85, 255, 255};
    // cgaLightRed not used by current palettes
    sRGB cgaYellow      = {255, 255, 85};
    sRGB cgaBrightWhite = {255, 255, 255};

    // ---------------------------------------------------------------
    // Palette 1: "Standard CGA" -- white on blue editor area
    // ---------------------------------------------------------------
    sTUIPalette stdCGA;
    stdCGA.name = "Standard CGA";
    // Editor colors
    stdCGA.foreground               = cgaWhite;
    stdCGA.background               = cgaBlue;
    stdCGA.highlightForeground      = cgaCyan;       // Control codes
    stdCGA.highlightBackground      = cgaBlue;
    stdCGA.dotForeground            = cgaGreen;      // Dot commands
    stdCGA.dotBackground            = cgaBlue;
    stdCGA.blockForeground          = cgaBlue;       // Block selection
    stdCGA.blockBackground          = cgaWhite;
    stdCGA.commentForeground        = cgaDarkGray;   // Comments
    stdCGA.commentBackground        = cgaBlue;
    stdCGA.errorForeground          = cgaYellow;     // Errors
    stdCGA.errorBackground          = cgaRed;
    stdCGA.unknownForeground        = cgaRed;        // Unknown
    stdCGA.unknownBackground        = cgaBlue;
    stdCGA.notImplementedForeground = cgaDarkGray;   // Not implemented
    stdCGA.notImplementedBackground = cgaBlue;
    stdCGA.searchForeground         = cgaBlack;      // Search results
    stdCGA.searchBackground         = cgaYellow;
    // Attribute colors (from screenshot: bold=white+bold, underline=yellow,
    // italic=cyan, subscript=green, superscript=green+bold, strikeout=red)
    stdCGA.boldForeground           = cgaBrightWhite;
    stdCGA.boldBackground           = cgaBlue;
    stdCGA.italicForeground         = cgaCyan;
    stdCGA.italicBackground         = cgaBlue;
    stdCGA.underlineForeground      = cgaYellow;
    stdCGA.underlineBackground      = cgaBlue;
    stdCGA.strikethroughForeground  = cgaRed;
    stdCGA.strikethroughBackground  = cgaBlue;
    stdCGA.superscriptForeground    = cgaLightGreen;
    stdCGA.superscriptBackground    = cgaBlue;
    stdCGA.subscriptForeground      = cgaGreen;
    stdCGA.subscriptBackground      = cgaBlue;
    // Screen colors (from screenshot: title/status/ruler/flag = white on black,
    // menus = black on white, highlight = blue+bold on white,
    // buttons = cyan on blue, button highlight = cyan+bold on blue)
    stdCGA.titleBarForeground              = cgaWhite;
    stdCGA.titleBarBackground              = cgaBlack;
    stdCGA.statusBarForeground             = cgaWhite;
    stdCGA.statusBarBackground             = cgaBlack;
    stdCGA.helpPanelForeground             = cgaBlack;   // Lists
    stdCGA.helpPanelBackground             = cgaWhite;
    stdCGA.helpPanelKeystrokeForeground    = cgaCyan;    // Style buttons
    stdCGA.helpPanelKeystrokeBackground    = cgaBlue;
    stdCGA.menuBarForeground               = cgaBlack;   // Menus
    stdCGA.menuBarBackground               = cgaWhite;
    stdCGA.menuAcceleratorForeground       = cgaLightBlue; // Menu highlight
    stdCGA.menuAcceleratorBackground       = cgaWhite;
    stdCGA.menuHighlightForeground         = cgaBrightWhite; // Title highlight
    stdCGA.menuHighlightBackground         = cgaBlack;
    stdCGA.rulerForeground                 = cgaWhite;
    stdCGA.rulerBackground                 = cgaBlack;
    stdCGA.flagColumnForeground            = cgaWhite;
    stdCGA.flagColumnBackground            = cgaBlack;
    stdCGA.scrollbarForeground             = cgaWhite;
    stdCGA.scrollbarBackground             = cgaBlack;
    palettes.push_back(stdCGA);

    // ---------------------------------------------------------------
    // Palette 2: "Monochrome" -- black on white, dim colors
    // ---------------------------------------------------------------
    sTUIPalette mono;
    mono.name = "Monochrome";
    sRGB monoGray = {128, 128, 128};
    // Editor colors
    mono.foreground               = cgaBlack;
    mono.background               = cgaWhite;
    mono.highlightForeground      = cgaDarkGray;     // Control codes
    mono.highlightBackground      = cgaWhite;
    mono.dotForeground            = cgaDarkGray;     // Dot commands
    mono.dotBackground            = cgaWhite;
    mono.blockForeground          = cgaWhite;        // Block selection
    mono.blockBackground          = cgaBlack;
    mono.commentForeground        = monoGray;        // Comments
    mono.commentBackground        = cgaWhite;
    mono.errorForeground          = cgaBrightWhite;  // Errors
    mono.errorBackground          = cgaBlack;
    mono.unknownForeground        = monoGray;        // Unknown
    mono.unknownBackground        = cgaWhite;
    mono.notImplementedForeground = monoGray;        // Not implemented
    mono.notImplementedBackground = cgaWhite;
    mono.searchForeground         = cgaBrightWhite;  // Search results
    mono.searchBackground         = cgaBlack;
    // Attribute colors (from screenshot: bold/underline/italic = white+bold on black,
    // subscript/superscript/strikeout = white on black)
    mono.boldForeground           = cgaBrightWhite;
    mono.boldBackground           = cgaBlack;
    mono.italicForeground         = cgaBrightWhite;
    mono.italicBackground         = cgaBlack;
    mono.underlineForeground      = cgaBrightWhite;
    mono.underlineBackground      = cgaBlack;
    mono.strikethroughForeground  = cgaWhite;
    mono.strikethroughBackground  = cgaBlack;
    mono.superscriptForeground    = cgaWhite;
    mono.superscriptBackground    = cgaBlack;
    mono.subscriptForeground      = cgaWhite;
    mono.subscriptBackground      = cgaBlack;
    // Screen colors (from screenshot: title/highlight/ruler/status/flag = white on black,
    // menus = black on white, menu highlight = white on black,
    // buttons = white on black, button highlight = white+bold on black)
    mono.titleBarForeground              = cgaWhite;
    mono.titleBarBackground              = cgaBlack;
    mono.statusBarForeground             = cgaWhite;
    mono.statusBarBackground             = cgaBlack;
    mono.helpPanelForeground             = cgaWhite;     // Lists
    mono.helpPanelBackground             = cgaBlack;
    mono.helpPanelKeystrokeForeground    = cgaBrightWhite; // Buttons
    mono.helpPanelKeystrokeBackground    = cgaBlack;
    mono.menuBarForeground               = cgaBlack;     // Menus
    mono.menuBarBackground               = cgaWhite;
    mono.menuAcceleratorForeground       = cgaWhite;     // Menu highlight
    mono.menuAcceleratorBackground       = cgaBlack;
    mono.menuHighlightForeground         = cgaBrightWhite; // Title highlight
    mono.menuHighlightBackground         = cgaBlack;
    mono.rulerForeground                 = cgaWhite;
    mono.rulerBackground                 = cgaBlack;
    mono.flagColumnForeground            = cgaWhite;
    mono.flagColumnBackground            = cgaBlack;
    mono.scrollbarForeground             = cgaWhite;
    mono.scrollbarBackground             = cgaBlack;
    palettes.push_back(mono);

    // ---------------------------------------------------------------
    // Palette 3: "Alternate CGA" -- black on cyan editor area
    // ---------------------------------------------------------------
    sTUIPalette altCGA;
    altCGA.name = "Alternate CGA";
    // Editor colors
    altCGA.foreground               = cgaBlack;
    altCGA.background               = cgaCyan;
    altCGA.highlightForeground      = cgaBlue;       // Control codes
    altCGA.highlightBackground      = cgaCyan;
    altCGA.dotForeground            = cgaMagenta;    // Dot commands
    altCGA.dotBackground            = cgaCyan;
    altCGA.blockForeground          = cgaBrightWhite; // Block selection
    altCGA.blockBackground          = cgaBlue;
    altCGA.commentForeground        = cgaDarkGray;   // Comments
    altCGA.commentBackground        = cgaCyan;
    altCGA.errorForeground          = cgaBrightWhite; // Errors
    altCGA.errorBackground          = cgaRed;
    altCGA.unknownForeground        = cgaYellow;     // Unknown
    altCGA.unknownBackground        = cgaCyan;
    altCGA.notImplementedForeground = cgaDarkGray;   // Not implemented
    altCGA.notImplementedBackground = cgaCyan;
    altCGA.searchForeground         = cgaBlack;      // Search results
    altCGA.searchBackground         = cgaYellow;
    // Attribute colors (from screenshot: bold=white+bold, underline=blue,
    // italic=magenta, subscript=yellow, superscript=yellow+bold, strikeout=cyan+bold)
    altCGA.boldForeground           = cgaBrightWhite;
    altCGA.boldBackground           = cgaCyan;
    altCGA.italicForeground         = cgaMagenta;
    altCGA.italicBackground         = cgaCyan;
    altCGA.underlineForeground      = cgaBlue;
    altCGA.underlineBackground      = cgaCyan;
    altCGA.strikethroughForeground  = cgaLightCyan;
    altCGA.strikethroughBackground  = cgaCyan;
    altCGA.superscriptForeground    = cgaYellow;
    altCGA.superscriptBackground    = cgaCyan;
    altCGA.subscriptForeground      = cgaYellow;
    altCGA.subscriptBackground      = cgaCyan;
    // Screen colors (from screenshot: title/ruler/status/flag = white on black,
    // menus = black on white, highlight = magenta on white,
    // buttons = cyan on magenta, button highlight = cyan+bold on magenta)
    altCGA.titleBarForeground              = cgaWhite;
    altCGA.titleBarBackground              = cgaBlack;
    altCGA.statusBarForeground             = cgaWhite;
    altCGA.statusBarBackground             = cgaBlack;
    altCGA.helpPanelForeground             = cgaBlack;   // Lists
    altCGA.helpPanelBackground             = cgaWhite;
    altCGA.helpPanelKeystrokeForeground    = cgaCyan;    // Style buttons
    altCGA.helpPanelKeystrokeBackground    = cgaMagenta;
    altCGA.menuBarForeground               = cgaBlack;   // Menus
    altCGA.menuBarBackground               = cgaWhite;
    altCGA.menuAcceleratorForeground       = cgaMagenta; // Menu highlight
    altCGA.menuAcceleratorBackground       = cgaWhite;
    altCGA.menuHighlightForeground         = cgaBrightWhite; // Title highlight
    altCGA.menuHighlightBackground         = cgaBlack;
    altCGA.rulerForeground                 = cgaWhite;
    altCGA.rulerBackground                 = cgaBlack;
    altCGA.flagColumnForeground            = cgaWhite;
    altCGA.flagColumnBackground            = cgaBlack;
    altCGA.scrollbarForeground             = cgaWhite;
    altCGA.scrollbarBackground             = cgaBlack;
    palettes.push_back(altCGA);

    // ---------------------------------------------------------------
    // Palette 4: "SFWriter" -- green on black editor area
    // ---------------------------------------------------------------
    sTUIPalette sfwriter;
    sfwriter.name = "Sawyer";
    // Editor colors
    sfwriter.foreground               = cgaGreen;
    sfwriter.background               = cgaBlack;
    sfwriter.highlightForeground      = cgaDarkGray;   // Control codes
    sfwriter.highlightBackground      = cgaBlack;
    sfwriter.dotForeground            = cgaCyan;       // Dot commands
    sfwriter.dotBackground            = cgaBlack;
    sfwriter.blockForeground          = cgaBlack;      // Block selection
    sfwriter.blockBackground          = cgaGreen;
    sfwriter.commentForeground        = cgaDarkGray;   // Comments
    sfwriter.commentBackground        = cgaBlack;
    sfwriter.errorForeground          = cgaBrightWhite; // Errors
    sfwriter.errorBackground          = cgaRed;
    sfwriter.unknownForeground        = cgaRed;        // Unknown
    sfwriter.unknownBackground        = cgaBlack;
    sfwriter.notImplementedForeground = cgaDarkGray;   // Not implemented
    sfwriter.notImplementedBackground = cgaBlack;
    sfwriter.searchForeground         = cgaBlack;      // Search results
    sfwriter.searchBackground         = cgaGreen;
    // Attribute colors (from screenshot: bold=white+bold on black,
    // underline=cyan+blink on blue, italic=cyan+blink on green,
    // subscript=white on blue, superscript=black on red,
    // strikeout=cyan on black)
    sfwriter.boldForeground           = cgaBrightWhite;
    sfwriter.boldBackground           = cgaBlack;
    sfwriter.italicForeground         = cgaLightCyan;
    sfwriter.italicBackground         = cgaGreen;
    sfwriter.underlineForeground      = cgaLightCyan;
    sfwriter.underlineBackground      = cgaBlue;
    sfwriter.strikethroughForeground  = cgaCyan;
    sfwriter.strikethroughBackground  = cgaBlack;
    sfwriter.superscriptForeground    = cgaBlack;
    sfwriter.superscriptBackground    = cgaRed;
    sfwriter.subscriptForeground      = cgaBrightWhite;
    sfwriter.subscriptBackground      = cgaBlue;
    // Screen colors (from screenshot: title=black on cyan,
    // highlight=white on black, menus=green on black,
    // menu highlight=white on black, lists=cyan on black,
    // buttons=white on blue, button highlight=black on white,
    // ruler=black+bold on black, status=black on green, flag=black+bold on black)
    sfwriter.titleBarForeground              = cgaBlack;
    sfwriter.titleBarBackground              = cgaCyan;
    sfwriter.statusBarForeground             = cgaBlack;
    sfwriter.statusBarBackground             = cgaGreen;
    sfwriter.helpPanelForeground             = cgaCyan;    // Lists
    sfwriter.helpPanelBackground             = cgaBlack;
    sfwriter.helpPanelKeystrokeForeground    = cgaWhite;   // Style buttons
    sfwriter.helpPanelKeystrokeBackground    = cgaBlue;
    sfwriter.menuBarForeground               = cgaGreen;   // Menus
    sfwriter.menuBarBackground               = cgaBlack;
    sfwriter.menuAcceleratorForeground       = cgaWhite;   // Menu highlight
    sfwriter.menuAcceleratorBackground       = cgaBlack;
    sfwriter.menuHighlightForeground         = cgaWhite;   // Title highlight
    sfwriter.menuHighlightBackground         = cgaBlack;
    sfwriter.rulerForeground                 = cgaDarkGray;
    sfwriter.rulerBackground                 = cgaBlack;
    sfwriter.flagColumnForeground            = cgaDarkGray;
    sfwriter.flagColumnBackground            = cgaBlack;
    sfwriter.scrollbarForeground             = cgaDarkGray;
    sfwriter.scrollbarBackground             = cgaBlack;
    palettes.push_back(sfwriter);

    return palettes;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return vector of custom GUI palettes loaded from disk
///
/// @brief
/// Load all custom GUI palettes from the gui-palettes.ini file.
/// Returns an empty vector if the file doesn't exist or is empty.
/// Each INI section represents one palette (section name = palette name).
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sGUIPalette> cConfig::LoadCustomGUIPalettes(void)
{
    std::vector<sGUIPalette> palettes;
    std::string path = GetGUIPaletteFilePath();

    CSimpleIniA ini;
    ini.SetUnicode();
    if (ini.LoadFile(path.c_str()) < 0)
    {
        // File doesn't exist yet -- no custom palettes
        return palettes;
    }

    // Helper: read an RGB value from the INI section
    auto readRGB = [&](const char* section, const char* key, sRGB defaultVal) -> sRGB
    {
        const char* value = ini.GetValue(section, key, nullptr);
        if (value == nullptr)
        {
            return defaultVal;
        }
        sRGB result;
        if (!ParseRGB(value, result.r, result.g, result.b))
        {
            return defaultVal;
        }
        return result;
    };

    // Iterate all sections -- each is a palette
    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);
    sections.sort(CSimpleIniA::Entry::LoadOrder());

    sRGB black = {0, 0, 0};
    sRGB white = {255, 255, 255};

    for (const auto& section : sections)
    {
        const char* name = section.pItem;

        sGUIPalette pal;
        pal.name = name;

        // [gui.colors] -- 9 editor color pairs (18 sRGB)
        pal.background                    = readRGB(name, "background", white);
        pal.foreground                    = readRGB(name, "foreground", black);
        pal.highlightBackground           = readRGB(name, "highlightBackground", black);
        pal.highlightForeground           = readRGB(name, "highlightForeground", white);
        pal.dotBackground                 = readRGB(name, "dotBackground", black);
        pal.dotForeground                 = readRGB(name, "dotForeground", white);
        pal.blockBackground               = readRGB(name, "blockBackground", black);
        pal.blockForeground               = readRGB(name, "blockForeground", white);
        pal.commentBackground             = readRGB(name, "commentBackground", black);
        pal.commentForeground             = readRGB(name, "commentForeground", white);
        pal.errorBackground               = readRGB(name, "errorBackground", black);
        pal.errorForeground               = readRGB(name, "errorForeground", white);
        pal.unknownBackground             = readRGB(name, "unknownBackground", black);
        pal.unknownForeground             = readRGB(name, "unknownForeground", white);
        pal.notImplementedBackground      = readRGB(name, "notImplementedBackground", black);
        pal.notImplementedForeground      = readRGB(name, "notImplementedForeground", white);
        pal.searchBackground              = readRGB(name, "searchBackground", black);
        pal.searchForeground              = readRGB(name, "searchForeground", white);

        // [gui.screen] -- 4 screen color pairs (8 sRGB)
        pal.statusBarForeground           = readRGB(name, "statusBarForeground", black);
        pal.statusBarBackground           = readRGB(name, "statusBarBackground", white);
        pal.helpPanelForeground           = readRGB(name, "helpPanelForeground", black);
        pal.helpPanelBackground           = readRGB(name, "helpPanelBackground", white);
        pal.helpPanelKeystrokeForeground  = readRGB(name, "helpPanelKeystrokeForeground", black);
        pal.helpPanelKeystrokeBackground  = readRGB(name, "helpPanelKeystrokeBackground", white);
        pal.rulerForeground               = readRGB(name, "rulerForeground", black);
        pal.rulerBackground               = readRGB(name, "rulerBackground", white);

        palettes.push_back(pal);
    }

    return palettes;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return vector of custom TUI palettes loaded from disk
///
/// @brief
/// Load all custom TUI palettes from the tui-palettes.ini file.
/// Returns an empty vector if the file doesn't exist or is empty.
/// Each INI section represents one palette (section name = palette name).
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sTUIPalette> cConfig::LoadCustomTUIPalettes(void)
{
    std::vector<sTUIPalette> palettes;
    std::string path = GetTUIPaletteFilePath();

    CSimpleIniA ini;
    ini.SetUnicode();
    if (ini.LoadFile(path.c_str()) < 0)
    {
        // File doesn't exist yet -- no custom palettes
        return palettes;
    }

    // Helper: read an RGB value from the INI section
    auto readRGB = [&](const char* section, const char* key, sRGB defaultVal) -> sRGB
    {
        const char* value = ini.GetValue(section, key, nullptr);
        if (value == nullptr)
        {
            return defaultVal;
        }
        sRGB result;
        if (!ParseRGB(value, result.r, result.g, result.b))
        {
            return defaultVal;
        }
        return result;
    };

    // Iterate all sections -- each is a palette
    CSimpleIniA::TNamesDepend sections;
    ini.GetAllSections(sections);
    sections.sort(CSimpleIniA::Entry::LoadOrder());

    sRGB black = {0, 0, 0};
    sRGB white = {255, 255, 255};

    for (const auto& section : sections)
    {
        const char* name = section.pItem;

        sTUIPalette pal;
        pal.name = name;

        // [tui.colors] -- 9 editor color pairs (18 sRGB)
        pal.background                    = readRGB(name, "background", white);
        pal.foreground                    = readRGB(name, "foreground", black);
        pal.highlightBackground           = readRGB(name, "highlightBackground", black);
        pal.highlightForeground           = readRGB(name, "highlightForeground", white);
        pal.dotBackground                 = readRGB(name, "dotBackground", black);
        pal.dotForeground                 = readRGB(name, "dotForeground", white);
        pal.blockBackground               = readRGB(name, "blockBackground", black);
        pal.blockForeground               = readRGB(name, "blockForeground", white);
        pal.commentBackground             = readRGB(name, "commentBackground", black);
        pal.commentForeground             = readRGB(name, "commentForeground", white);
        pal.errorBackground               = readRGB(name, "errorBackground", black);
        pal.errorForeground               = readRGB(name, "errorForeground", white);
        pal.unknownBackground             = readRGB(name, "unknownBackground", black);
        pal.unknownForeground             = readRGB(name, "unknownForeground", white);
        pal.notImplementedBackground      = readRGB(name, "notImplementedBackground", black);
        pal.notImplementedForeground      = readRGB(name, "notImplementedForeground", white);
        pal.searchBackground              = readRGB(name, "searchBackground", black);
        pal.searchForeground              = readRGB(name, "searchForeground", white);

        // [tui.styleColors] -- 6 style fallback color pairs (12 sRGB)
        pal.boldForeground                = readRGB(name, "boldForeground", black);
        pal.boldBackground                = readRGB(name, "boldBackground", white);
        pal.italicForeground              = readRGB(name, "italicForeground", black);
        pal.italicBackground              = readRGB(name, "italicBackground", white);
        pal.underlineForeground           = readRGB(name, "underlineForeground", black);
        pal.underlineBackground           = readRGB(name, "underlineBackground", white);
        pal.strikethroughForeground       = readRGB(name, "strikethroughForeground", black);
        pal.strikethroughBackground       = readRGB(name, "strikethroughBackground", white);
        pal.superscriptForeground         = readRGB(name, "superscriptForeground", black);
        pal.superscriptBackground         = readRGB(name, "superscriptBackground", white);
        pal.subscriptForeground           = readRGB(name, "subscriptForeground", black);
        pal.subscriptBackground           = readRGB(name, "subscriptBackground", white);

        // [tui.screen] -- 10 screen color pairs (20 sRGB)
        pal.titleBarForeground            = readRGB(name, "titleBarForeground", black);
        pal.titleBarBackground            = readRGB(name, "titleBarBackground", white);
        pal.statusBarForeground           = readRGB(name, "statusBarForeground", black);
        pal.statusBarBackground           = readRGB(name, "statusBarBackground", white);
        pal.helpPanelForeground           = readRGB(name, "helpPanelForeground", black);
        pal.helpPanelBackground           = readRGB(name, "helpPanelBackground", white);
        pal.helpPanelKeystrokeForeground  = readRGB(name, "helpPanelKeystrokeForeground", black);
        pal.helpPanelKeystrokeBackground  = readRGB(name, "helpPanelKeystrokeBackground", white);
        pal.menuBarForeground             = readRGB(name, "menuBarForeground", black);
        pal.menuBarBackground             = readRGB(name, "menuBarBackground", white);
        pal.menuAcceleratorForeground     = readRGB(name, "menuAcceleratorForeground", black);
        pal.menuAcceleratorBackground     = readRGB(name, "menuAcceleratorBackground", white);
        pal.menuHighlightForeground       = readRGB(name, "menuHighlightForeground", black);
        pal.menuHighlightBackground       = readRGB(name, "menuHighlightBackground", white);
        pal.rulerForeground               = readRGB(name, "rulerForeground", black);
        pal.rulerBackground               = readRGB(name, "rulerBackground", white);
        pal.flagColumnForeground          = readRGB(name, "flagColumnForeground", black);
        pal.flagColumnBackground          = readRGB(name, "flagColumnBackground", white);
        pal.scrollbarForeground           = readRGB(name, "scrollbarForeground", black);
        pal.scrollbarBackground           = readRGB(name, "scrollbarBackground", white);

        palettes.push_back(pal);
    }

    return palettes;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  palette [in] the GUI palette to save
///
/// @return true on success
///
/// @brief
/// Save a single GUI palette to the custom palette file. If a palette
/// with the same name already exists, it is overwritten.
///
/////////////////////////////////////////////////////////////////////////////
bool cConfig::SaveCustomGUIPalette(const sGUIPalette& palette)
{
    std::string path = GetGUIPaletteFilePath();

    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(path.c_str());  // Load existing (may fail if new -- OK)

    const char* section = palette.name.c_str();

    // Helper: write an RGB value to the INI section
    auto writeRGB = [&](const char* key, const sRGB& color)
    {
        std::string formatted = FormatRGB(color.r, color.g, color.b);
        ini.SetValue(section, key, formatted.c_str());
    };

    // [gui.colors] -- 9 editor color pairs (18 sRGB)
    writeRGB("background",                    palette.background);
    writeRGB("foreground",                    palette.foreground);
    writeRGB("highlightBackground",           palette.highlightBackground);
    writeRGB("highlightForeground",           palette.highlightForeground);
    writeRGB("dotBackground",                 palette.dotBackground);
    writeRGB("dotForeground",                 palette.dotForeground);
    writeRGB("blockBackground",               palette.blockBackground);
    writeRGB("blockForeground",               palette.blockForeground);
    writeRGB("commentBackground",             palette.commentBackground);
    writeRGB("commentForeground",             palette.commentForeground);
    writeRGB("errorBackground",               palette.errorBackground);
    writeRGB("errorForeground",               palette.errorForeground);
    writeRGB("unknownBackground",             palette.unknownBackground);
    writeRGB("unknownForeground",             palette.unknownForeground);
    writeRGB("notImplementedBackground",      palette.notImplementedBackground);
    writeRGB("notImplementedForeground",      palette.notImplementedForeground);
    writeRGB("searchBackground",              palette.searchBackground);
    writeRGB("searchForeground",              palette.searchForeground);

    // [gui.screen] -- 4 screen color pairs (8 sRGB)
    writeRGB("statusBarForeground",           palette.statusBarForeground);
    writeRGB("statusBarBackground",           palette.statusBarBackground);
    writeRGB("helpPanelForeground",           palette.helpPanelForeground);
    writeRGB("helpPanelBackground",           palette.helpPanelBackground);
    writeRGB("helpPanelKeystrokeForeground",  palette.helpPanelKeystrokeForeground);
    writeRGB("helpPanelKeystrokeBackground",  palette.helpPanelKeystrokeBackground);
    writeRGB("rulerForeground",               palette.rulerForeground);
    writeRGB("rulerBackground",               palette.rulerBackground);

    return ini.SaveFile(path.c_str()) >= 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  palette [in] the TUI palette to save
///
/// @return true on success
///
/// @brief
/// Save a single TUI palette to the custom palette file. If a palette
/// with the same name already exists, it is overwritten.
///
/////////////////////////////////////////////////////////////////////////////
bool cConfig::SaveCustomTUIPalette(const sTUIPalette& palette)
{
    std::string path = GetTUIPaletteFilePath();

    CSimpleIniA ini;
    ini.SetUnicode();
    ini.LoadFile(path.c_str());  // Load existing (may fail if new -- OK)

    const char* section = palette.name.c_str();

    // Helper: write an RGB value to the INI section
    auto writeRGB = [&](const char* key, const sRGB& color)
    {
        std::string formatted = FormatRGB(color.r, color.g, color.b);
        ini.SetValue(section, key, formatted.c_str());
    };

    // [tui.colors] -- 9 editor color pairs (18 sRGB)
    writeRGB("background",                    palette.background);
    writeRGB("foreground",                    palette.foreground);
    writeRGB("highlightBackground",           palette.highlightBackground);
    writeRGB("highlightForeground",           palette.highlightForeground);
    writeRGB("dotBackground",                 palette.dotBackground);
    writeRGB("dotForeground",                 palette.dotForeground);
    writeRGB("blockBackground",               palette.blockBackground);
    writeRGB("blockForeground",               palette.blockForeground);
    writeRGB("commentBackground",             palette.commentBackground);
    writeRGB("commentForeground",             palette.commentForeground);
    writeRGB("errorBackground",               palette.errorBackground);
    writeRGB("errorForeground",               palette.errorForeground);
    writeRGB("unknownBackground",             palette.unknownBackground);
    writeRGB("unknownForeground",             palette.unknownForeground);
    writeRGB("notImplementedBackground",      palette.notImplementedBackground);
    writeRGB("notImplementedForeground",      palette.notImplementedForeground);
    writeRGB("searchBackground",              palette.searchBackground);
    writeRGB("searchForeground",              palette.searchForeground);

    // [tui.styleColors] -- 6 style fallback color pairs (12 sRGB)
    writeRGB("boldForeground",                palette.boldForeground);
    writeRGB("boldBackground",                palette.boldBackground);
    writeRGB("italicForeground",              palette.italicForeground);
    writeRGB("italicBackground",              palette.italicBackground);
    writeRGB("underlineForeground",           palette.underlineForeground);
    writeRGB("underlineBackground",           palette.underlineBackground);
    writeRGB("strikethroughForeground",       palette.strikethroughForeground);
    writeRGB("strikethroughBackground",       palette.strikethroughBackground);
    writeRGB("superscriptForeground",         palette.superscriptForeground);
    writeRGB("superscriptBackground",         palette.superscriptBackground);
    writeRGB("subscriptForeground",           palette.subscriptForeground);
    writeRGB("subscriptBackground",           palette.subscriptBackground);

    // [tui.screen] -- 10 screen color pairs (20 sRGB)
    writeRGB("titleBarForeground",            palette.titleBarForeground);
    writeRGB("titleBarBackground",            palette.titleBarBackground);
    writeRGB("statusBarForeground",           palette.statusBarForeground);
    writeRGB("statusBarBackground",           palette.statusBarBackground);
    writeRGB("helpPanelForeground",           palette.helpPanelForeground);
    writeRGB("helpPanelBackground",           palette.helpPanelBackground);
    writeRGB("helpPanelKeystrokeForeground",  palette.helpPanelKeystrokeForeground);
    writeRGB("helpPanelKeystrokeBackground",  palette.helpPanelKeystrokeBackground);
    writeRGB("menuBarForeground",             palette.menuBarForeground);
    writeRGB("menuBarBackground",             palette.menuBarBackground);
    writeRGB("menuAcceleratorForeground",     palette.menuAcceleratorForeground);
    writeRGB("menuAcceleratorBackground",     palette.menuAcceleratorBackground);
    writeRGB("menuHighlightForeground",       palette.menuHighlightForeground);
    writeRGB("menuHighlightBackground",       palette.menuHighlightBackground);
    writeRGB("rulerForeground",               palette.rulerForeground);
    writeRGB("rulerBackground",               palette.rulerBackground);
    writeRGB("flagColumnForeground",          palette.flagColumnForeground);
    writeRGB("flagColumnBackground",          palette.flagColumnBackground);
    writeRGB("scrollbarForeground",           palette.scrollbarForeground);
    writeRGB("scrollbarBackground",           palette.scrollbarBackground);

    return ini.SaveFile(path.c_str()) >= 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  palette [in] the GUI palette to apply
///
/// @return nothing
///
/// @brief
/// Copy all color values from a GUI palette into the config's gui.colors
/// and gui.screen members.
///
/////////////////////////////////////////////////////////////////////////////
void cConfig::ApplyGUIPalette(const sGUIPalette& palette)
{
    // [gui.colors]
    mGuiBackground = palette.background;
    mGuiForeground = palette.foreground;
    mGuiHighlightBackground = palette.highlightBackground;
    mGuiHighlightForeground = palette.highlightForeground;
    mGuiDotBackground = palette.dotBackground;
    mGuiDotForeground = palette.dotForeground;
    mGuiBlockBackground = palette.blockBackground;
    mGuiBlockForeground = palette.blockForeground;
    mGuiCommentBackground = palette.commentBackground;
    mGuiCommentForeground = palette.commentForeground;
    mGuiErrorBackground = palette.errorBackground;
    mGuiErrorForeground = palette.errorForeground;
    mGuiUnknownBackground = palette.unknownBackground;
    mGuiUnknownForeground = palette.unknownForeground;
    mGuiNotImplementedBackground = palette.notImplementedBackground;
    mGuiNotImplementedForeground = palette.notImplementedForeground;
    mGuiSearchBackground = palette.searchBackground;
    mGuiSearchForeground = palette.searchForeground;

    // [gui.screen]
    mGuiStatusBarForeground = palette.statusBarForeground;
    mGuiStatusBarBackground = palette.statusBarBackground;
    mGuiHelpPanelForeground = palette.helpPanelForeground;
    mGuiHelpPanelBackground = palette.helpPanelBackground;
    mGuiHelpPanelKeystrokeForeground = palette.helpPanelKeystrokeForeground;
    mGuiHelpPanelKeystrokeBackground = palette.helpPanelKeystrokeBackground;
    mGuiRulerForeground = palette.rulerForeground;
    mGuiRulerBackground = palette.rulerBackground;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  palette [in] the TUI palette to apply
///
/// @return nothing
///
/// @brief
/// Copy all color values from a TUI palette into the config's tui.colors,
/// tui.styleColors, and tui.screen members.
///
/////////////////////////////////////////////////////////////////////////////
void cConfig::ApplyTUIPalette(const sTUIPalette& palette)
{
    // [tui.colors]
    mTuiBackground = palette.background;
    mTuiForeground = palette.foreground;
    mTuiHighlightBackground = palette.highlightBackground;
    mTuiHighlightForeground = palette.highlightForeground;
    mTuiDotBackground = palette.dotBackground;
    mTuiDotForeground = palette.dotForeground;
    mTuiBlockBackground = palette.blockBackground;
    mTuiBlockForeground = palette.blockForeground;
    mTuiCommentBackground = palette.commentBackground;
    mTuiCommentForeground = palette.commentForeground;
    mTuiErrorBackground = palette.errorBackground;
    mTuiErrorForeground = palette.errorForeground;
    mTuiUnknownBackground = palette.unknownBackground;
    mTuiUnknownForeground = palette.unknownForeground;
    mTuiNotImplementedBackground = palette.notImplementedBackground;
    mTuiNotImplementedForeground = palette.notImplementedForeground;
    mTuiSearchBackground = palette.searchBackground;
    mTuiSearchForeground = palette.searchForeground;

    // [tui.styleColors]
    mTuiBoldForeground = palette.boldForeground;
    mTuiBoldBackground = palette.boldBackground;
    mTuiItalicForeground = palette.italicForeground;
    mTuiItalicBackground = palette.italicBackground;
    mTuiUnderlineForeground = palette.underlineForeground;
    mTuiUnderlineBackground = palette.underlineBackground;
    mTuiStrikethroughForeground = palette.strikethroughForeground;
    mTuiStrikethroughBackground = palette.strikethroughBackground;
    mTuiSuperscriptForeground = palette.superscriptForeground;
    mTuiSuperscriptBackground = palette.superscriptBackground;
    mTuiSubscriptForeground = palette.subscriptForeground;
    mTuiSubscriptBackground = palette.subscriptBackground;

    // [tui.screen]
    mTuiTitleBarForeground = palette.titleBarForeground;
    mTuiTitleBarBackground = palette.titleBarBackground;
    mTuiStatusBarForeground = palette.statusBarForeground;
    mTuiStatusBarBackground = palette.statusBarBackground;
    mTuiHelpPanelForeground = palette.helpPanelForeground;
    mTuiHelpPanelBackground = palette.helpPanelBackground;
    mTuiHelpPanelKeystrokeForeground = palette.helpPanelKeystrokeForeground;
    mTuiHelpPanelKeystrokeBackground = palette.helpPanelKeystrokeBackground;
    mTuiMenuBarForeground = palette.menuBarForeground;
    mTuiMenuBarBackground = palette.menuBarBackground;
    mTuiMenuAcceleratorForeground = palette.menuAcceleratorForeground;
    mTuiMenuAcceleratorBackground = palette.menuAcceleratorBackground;
    mTuiMenuHighlightForeground = palette.menuHighlightForeground;
    mTuiMenuHighlightBackground = palette.menuHighlightBackground;
    mTuiRulerForeground = palette.rulerForeground;
    mTuiRulerBackground = palette.rulerBackground;
    mTuiFlagColumnForeground = palette.flagColumnForeground;
    mTuiFlagColumnBackground = palette.flagColumnBackground;
    mTuiScrollbarForeground = palette.scrollbarForeground;
    mTuiScrollbarBackground = palette.scrollbarBackground;
}
