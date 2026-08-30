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
 * @class cTUIBuiltInMetricsFontCalculator
 * @brief Built-in font metrics backend providing hardcoded measurements as a last-resort fallback.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIBuiltInMetricsFontCalculator, a cTUIFontCalculator subclass that
 * provides hardcoded width, height, ascent, descent, and line-gap values
 * for eight common fonts (Courier New, Consolas, Liberation Mono, Times
 * New Roman, Arial, Liberation Serif, Liberation Sans, Terminal) at 12pt.
 * Used as the last-resort fallback when no TrueType font files or shaping
 * libraries are available on the system.
 *
 * @section builtin_metrics Metric Tables
 * Each supported font has a sTUIBuiltInFontMetrics entry containing per-character
 * advance widths and vertical metrics measured at 12pt. Font matching is
 * case-insensitive with normalized names (spaces and hyphens removed).
 *
 * @see cTUIBuiltInMetricsFontCalculator
 * @see cTUIFontCalculator
 * @see sTUIBuiltInFontMetrics
 * @see sTUIFontInfo
 * @see sTUITextMetrics
 */

#include "builtinmetrics.h"
#include <algorithm>
#include <cstring>
#include <codecvt>
#include <locale>

#ifndef _WIN32
#include <wchar.h>
extern "C" {
    int wcwidth(wchar_t wc);
}
#endif

// Built-in font metrics table - realistic measurements for common fonts at 12pt
const std::array<sTUIBuiltInFontMetrics, 8> cTUIBuiltInMetricsFontCalculator::mBuiltInFonts = {{
    // Monospace fonts (exact character widths)
    {"Courier New",     144, 240, 180, 60, 24, true},   // Classic monospace
    {"Consolas",        138, 230, 172, 58, 20, true},   // Modern programming font
    {"Liberation Mono", 144, 240, 180, 60, 24, true},   // Open source Courier alternative
    
    // Proportional fonts (average character widths)
    {"Times New Roman", 108, 240, 180, 60, 24, false},  // Classic serif
    {"Arial",           108, 240, 180, 60, 24, false},  // Classic sans-serif
    {"Liberation Serif", 108, 240, 180, 60, 24, false}, // Open source Times alternative
    {"Liberation Sans",  108, 240, 180, 60, 24, false}, // Open source Arial alternative
    
    // Terminal fallback (monospace)
    {"Terminal",        144, 240, 180, 60, 24, true}    // Generic terminal font
}};

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for built-in metrics font calculator
/// Initializes with Courier New font and document mode settings
///
/////////////////////////////////////////////////////////////////////////////
cTUIBuiltInMetricsFontCalculator::cTUIBuiltInMetricsFontCalculator(void)
    : mCurrentFontMetrics(nullptr) {
    mBackend = FONT_BACKEND_BUILT_IN_METRICS;
    mMode = FONT_MODE_DOCUMENT;
    mDocumentMode = true;
    
    // Set default font
    mCurrentFont.name = "Courier New";
    mCurrentFont.size = 12;
    mCurrentFontMetrics = FindFontMetrics(mCurrentFont.name);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initializes the built-in metrics font calculator
/// Always returns true as built-in metrics are always available
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIBuiltInMetricsFontCalculator::Initialize(void) {
    // Built-in metrics are always available
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shuts down the font calculator and cleans up resources
/// Clears character width cache and resets font metrics pointer
///
/////////////////////////////////////////////////////////////////////////////
void cTUIBuiltInMetricsFontCalculator::Shutdown(void) {
    mCharWidthCache.clear();
    mCurrentFontMetrics = nullptr;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontInfo& font [in] font information to set
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Sets the current font using built-in metrics
/// Falls back to first monospace font if requested font not found
///
/// @note Clears character width cache when font changes
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIBuiltInMetricsFontCalculator::SetFont(const sTUIFontInfo& font) {
    mCurrentFont = font;
    
    // Find best matching built-in font
    mCurrentFontMetrics = FindFontMetrics(font.name);
    if (!mCurrentFontMetrics) {
        // Default to first monospace font if not found
        mCurrentFontMetrics = &mBuiltInFonts[0];
        mCurrentFont.name = mCurrentFontMetrics->name;
    }
    
    // Clear character width cache when font changes
    mCharWidthCache.clear();
    
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of built-in font names
///
/// @brief
/// Gets a list of all available built-in fonts
/// Returns the names of fonts with hardcoded metrics data
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUIBuiltInMetricsFontCalculator::GetAvailableFonts(void) {
    return GetBuiltInFontNames();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] text to measure
///
/// @return sTUITextMetrics [out] text measurement metrics in TWIPS
///
/// @brief
/// Measures text dimensions using built-in font metrics
/// Converts UTF-8 text to UTF-32 and calculates total width and height
///
/// @note Returns empty metrics if no font is set or text is empty
///
/////////////////////////////////////////////////////////////////////////////
sTUITextMetrics cTUIBuiltInMetricsFontCalculator::MeasureText(const std::string& text) {
    sTUITextMetrics metrics;
    
    if (!mCurrentFontMetrics || text.empty()) {
        return metrics;
    }
    
    // Convert UTF-8 to UTF-32 for proper character handling
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string utf32text;
    
    try {
        utf32text = converter.from_bytes(text);
    } catch (...) {
        // Fallback: treat as ASCII
        utf32text.reserve(text.length());
        for (char c : text) {
            utf32text.push_back(static_cast<char32_t>(c));
        }
    }
    
    int totalWidth = 0;
    for (char32_t codepoint : utf32text) {
        totalWidth += MeasureCharacterWidth(codepoint);
    }
    
    metrics.widthTWIPS = totalWidth;
    metrics.heightTWIPS = GetLineHeight();
    metrics.ascentTWIPS = ScaleMetric(mCurrentFontMetrics->ascentTWIPS, mCurrentFont.size);
    metrics.descentTWIPS = ScaleMetric(mCurrentFontMetrics->descentTWIPS, mCurrentFont.size);
    metrics.leadingTWIPS = ScaleMetric(mCurrentFontMetrics->leadingTWIPS, mCurrentFont.size);
    
    return metrics;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to measure
///
/// @return int [out] character width in TWIPS
///
/// @brief
/// Measures the width of a single Unicode character
/// Uses caching for performance and handles both monospace and proportional fonts
///
/// @note Uses wcwidth() in terminal mode, font-specific metrics in document mode
///
/////////////////////////////////////////////////////////////////////////////
int cTUIBuiltInMetricsFontCalculator::MeasureCharacterWidth(char32_t codepoint) {
    if (!mCurrentFontMetrics) {
        return 144; // Default character width (12pt Courier)
    }
    
    // Check cache first
    auto it = mCharWidthCache.find(codepoint);
    if (it != mCharWidthCache.end()) {
        return it->second;
    }
    
    int width;
    
    if (mMode == FONT_MODE_TERMINAL && !mDocumentMode) {
        // Terminal mode: use wcwidth() for character width
        width = GetTerminalCharWidth(codepoint);
        width *= ScaleMetric(mCurrentFontMetrics->baseWidthTWIPS, mCurrentFont.size);
    } else {
        // Document mode: use font-specific measurements
        if (mCurrentFontMetrics->monospace) {
            // Monospace: all characters have same width
            width = ScaleMetric(mCurrentFontMetrics->baseWidthTWIPS, mCurrentFont.size);
        } else {
            // Proportional: estimate character width
            width = GetProportionalCharWidth(codepoint);
            width = ScaleMetric(width, mCurrentFont.size);
        }
    }
    
    // Cache the result
    mCharWidthCache[codepoint] = width;
    return width;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return int [out] line height in TWIPS
///
/// @brief
/// Gets the line height for the current font
/// Scales base metrics according to current font size
///
/////////////////////////////////////////////////////////////////////////////
int cTUIBuiltInMetricsFontCalculator::GetLineHeight(void) {
    if (!mCurrentFontMetrics) {
        return 240; // Default line height (12pt)
    }
    
    return ScaleMetric(mCurrentFontMetrics->heightTWIPS, mCurrentFont.size);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to check
///
/// @return bool [out] true if character is supported
///
/// @brief
/// Checks if a Unicode character is supported by built-in metrics
/// Covers ASCII, Latin extensions, punctuation, currency, and basic CJK
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIBuiltInMetricsFontCalculator::SupportsCharacter(char32_t codepoint) {
    // Basic Unicode support - covers most common characters
    if (codepoint <= 0x007F) return true;        // ASCII
    if (codepoint >= 0x00A0 && codepoint <= 0x00FF) return true; // Latin-1 Supplement
    if (codepoint >= 0x0100 && codepoint <= 0x017F) return true; // Latin Extended-A
    if (codepoint >= 0x0180 && codepoint <= 0x024F) return true; // Latin Extended-B
    if (codepoint >= 0x2000 && codepoint <= 0x206F) return true; // General Punctuation
    if (codepoint >= 0x20A0 && codepoint <= 0x20CF) return true; // Currency Symbols
    
    // CJK basic support (full-width characters)
    if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) return true; // CJK Unified Ideographs
    if (codepoint >= 0x3000 && codepoint <= 0x303F) return true; // CJK Symbols and Punctuation
    
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int baseMetric [in] base metric value at 12pt
/// @param  int fontSize [in] target font size in points
///
/// @return int [out] scaled metric value
///
/// @brief
/// Scales a metric from 12pt base size to the requested font size
/// All built-in metrics are defined at 12pt and scaled as needed
///
/////////////////////////////////////////////////////////////////////////////
int cTUIBuiltInMetricsFontCalculator::ScaleMetric(int baseMetric, int fontSize) const {
    // Scale from 12pt base to requested font size
    return (baseMetric * fontSize) / 12;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to measure
///
/// @return int [out] estimated character width for proportional fonts
///
/// @brief
/// Estimates character width for proportional fonts
/// Uses predefined ratios based on typical character widths in fonts
///
/// @note Returns width relative to base font metrics, not scaled
///
/////////////////////////////////////////////////////////////////////////////
int cTUIBuiltInMetricsFontCalculator::GetProportionalCharWidth(char32_t codepoint) const {
    // Estimate character width for proportional fonts
    // Based on average character widths in typical fonts
    
    if (!mCurrentFontMetrics) {
        return 108; // Default proportional character width
    }
    
    int baseWidth = mCurrentFontMetrics->baseWidthTWIPS;
    
    // ASCII character width estimates (relative to average)
    if (codepoint <= 0x007F) {
        switch (codepoint) {
            case ' ': return baseWidth * 50 / 100;      // Space (50%)
            case 'i': case 'l': case '1': case '!': case '|':
                return baseWidth * 40 / 100;             // Narrow characters (40%)
            case 'f': case 'j': case 't':
                return baseWidth * 60 / 100;             // Slightly narrow (60%)
            case 'm': case 'w': case 'M': case 'W':
                return baseWidth * 140 / 100;            // Wide characters (140%)
            case 'A': case 'B': case 'C': case 'D': case 'G': case 'H': case 'N': case 'O': case 'Q': case 'R': case 'S': case 'U': case 'V': case 'X': case 'Y': case 'Z':
                return baseWidth * 120 / 100;            // Wide capitals (120%)
            default:
                return baseWidth;                        // Average width (100%)
        }
    }
    
    // CJK characters (full-width)
    if ((codepoint >= 0x4E00 && codepoint <= 0x9FFF) || // CJK Unified Ideographs
        (codepoint >= 0x3000 && codepoint <= 0x303F)) { // CJK Symbols
        return baseWidth * 200 / 100;                    // Double width (200%)
    }
    
    // Default for other Unicode characters
    return baseWidth;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to measure
///
/// @return int [out] terminal character width (0, 1, or 2)
///
/// @brief
/// Gets terminal character width for wcwidth() compatibility
/// Uses platform-specific methods - wcwidth() on Unix, simple rules on Windows
///
/// @note Returns 0 for non-printable, 1 for normal, 2 for wide characters
///
/////////////////////////////////////////////////////////////////////////////
int cTUIBuiltInMetricsFontCalculator::GetTerminalCharWidth(char32_t codepoint) const {
#ifdef _WIN32
    // Windows: simple approach for terminal width
    if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) return 2; // CJK
    if (codepoint > 0x007F) return 1; // Non-ASCII
    return 1; // ASCII
#else
    // Unix: use wcwidth() for proper terminal width calculation
    int width = wcwidth(static_cast<wchar_t>(codepoint));
    if (width < 0) return 0;  // Non-printable
    if (width == 0) return 0; // Zero-width (combining characters)
    return width;             // 1 for normal, 2 for wide characters
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] font name to search for
///
/// @return const sTUIBuiltInFontMetrics* [out] pointer to font metrics or nullptr
///
/// @brief
/// Finds font metrics for a given font name
/// Performs case-insensitive search through built-in font table
///
/// @note Returns nullptr if font is not found in built-in metrics
///
/////////////////////////////////////////////////////////////////////////////
const sTUIBuiltInFontMetrics* cTUIBuiltInMetricsFontCalculator::FindFontMetrics(const std::string& fontName) {
    // Case-insensitive font name search
    for (const auto& font : mBuiltInFonts) {
        std::string builtInName = font.name;
        std::string searchName = fontName;
        
        // Convert both to lowercase for comparison
        std::transform(builtInName.begin(), builtInName.end(), builtInName.begin(), ::tolower);
        std::transform(searchName.begin(), searchName.end(), searchName.begin(), ::tolower);
        
        if (builtInName == searchName) {
            return &font;
        }
    }
    
    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of built-in font names
///
/// @brief
/// Gets a list of all built-in font names
/// Returns the names of fonts with hardcoded metrics data
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUIBuiltInMetricsFontCalculator::GetBuiltInFontNames(void) {
    std::vector<std::string> names;
    names.reserve(mBuiltInFonts.size());
    
    for (const auto& font : mBuiltInFonts) {
        names.emplace_back(font.name);
    }
    
    return names;
}