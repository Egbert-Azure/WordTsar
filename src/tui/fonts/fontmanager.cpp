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

/**
 * @class cTUIFontManager
 * @brief TUI font manager for system font discovery, backend selection, and measurement delegation.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIFontManager, which discovers system fonts (via fontconfig on
 * Linux, platform APIs elsewhere), selects the best available measurement
 * backend (HarfBuzz, STB TrueType, or built-in metrics), and provides font
 * enumeration, selection, and measurement to the TUI layout engine. Supports
 * background font discovery and best-match font resolution with style fallback.
 *
 * @section fontmgr_discovery Font Discovery
 * On Linux, queries fontconfig for all installed TrueType and OpenType fonts,
 * building an sTUIAvailableFont list with family, style, and file path. Applies
 * sTUIFontSelectionCriteria to find the closest match by family name, weight,
 * and slant.
 *
 * @section fontmgr_backend Backend Selection
 * Delegates to cTUIFontMeasurementManager to probe for the highest-quality
 * available backend. Once selected, all measurement calls route through the
 * chosen cTUIFontCalculator subclass.
 *
 * @see cTUIFontManager
 * @see cTUIFontMeasurementManager
 * @see sTUIAvailableFont
 * @see sTUIFontSelectionCriteria
 * @see sTUIFontInfo
 */

#include "fontmanager.h"
#include "builtinmetrics.h"
#ifdef _WIN32
#include "platform_windows.h"
#elif defined(__APPLE__)
#include "platform_macos.h"
#else
#include "platform_qt.h"
#include "platform_harfbuzz.h"
#endif
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cstring>
#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#endif
#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for TUI Font Manager
/// Initializes font manager with default settings and Courier New font
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontManager::cTUIFontManager(void)
    : mDocumentMode(true), mCurrentBackend(FONT_BACKEND_BUILT_IN_METRICS) {
    // Initialize with default font
    mCurrentFont.name = "Courier New";
    mCurrentFont.size = 12;
    mCurrentFont.bold = false;
    mCurrentFont.italic = false;
    mCurrentFont.underline = false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for TUI Font Manager
/// Shuts down the font system and releases all resources
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontManager::~cTUIFontManager(void) {
    Shutdown();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool documentMode [in] true for document mode (high precision), false for terminal mode
///
/// @return bool [out] true if initialization successful, false otherwise
///
/// @brief
/// Initializes the font system and discovers available fonts
/// Sets up the font enumeration and creates the appropriate font calculator backend
///
/// @note Document mode provides higher precision measurements for printing
/// @see mDocumentMode for current mode setting
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontManager::Initialize(bool documentMode) {
    // Idempotent: skip if already initialized
    if (mInitialized)
    {
        return true;
    }

    mDocumentMode = documentMode;

    // Add built-in fonts synchronously (instant, provides fallback font set)
    AddBuiltInFonts();

    // Create the best available font calculator
    eTUIFontMeasurementBackend backend = cTUIFontMeasurementManager::DetectBestBackend();
    if (!CreateFontCalculator(backend)) {
        // Fall back to built-in metrics
        if (!CreateFontCalculator(FONT_BACKEND_BUILT_IN_METRICS)) {
            return false;
        }
    }

    // Set default font (works with built-in fonts immediately)
    sTUIFontSelectionCriteria defaultCriteria;
    defaultCriteria.family = GetDefaultFontFamily();
    defaultCriteria.size = GetDefaultFontSize();

    SetFont(defaultCriteria);

    // Start background thread for system font discovery
    mEnumDone = false;
    mEnumThread = std::thread([this]()
    {
        // Discover system fonts (slow -- fontconfig or directory scanning)
        std::vector<FontInfo> discoveredFonts = DiscoverSystemFonts();

        // Get the best available backend for system fonts
        eTUIFontMeasurementBackend bestBackend = cTUIFontMeasurementManager::DetectBestBackend();

        // Build the list of sTUIAvailableFont entries from discovered fonts
        std::vector<sTUIAvailableFont> systemFonts;
        for (const FontInfo& fontInfo : discoveredFonts)
        {
            // Parse style information
            std::string lowerStyle = fontInfo.style;
            std::transform(lowerStyle.begin(), lowerStyle.end(), lowerStyle.begin(), ::tolower);
            bool isBold = (lowerStyle.find("bold") != std::string::npos);
            bool isItalic = (lowerStyle.find("italic") != std::string::npos ||
                             lowerStyle.find("oblique") != std::string::npos);

            // Check for duplicates within systemFonts
            bool exists = false;
            std::string normalizedName = NormalizeFontName(fontInfo.family);
            for (const auto& existingFont : systemFonts)
            {
                std::string existingNormalizedName = NormalizeFontName(existingFont.family);
                if (existingNormalizedName == normalizedName &&
                    existingFont.bold == isBold &&
                    existingFont.italic == isItalic)
                {
                    exists = true;
                    break;
                }
            }

            if (!exists)
            {
                sTUIAvailableFont font;
                font.family = fontInfo.family;
                font.fullName = fontInfo.filepath;
                font.backend = bestBackend;
                font.bold = isBold;
                font.italic = isItalic;

                // Determine monospace from fontconfig spacing or name keywords
                if (fontInfo.spacing == 100)
                {
                    font.monospace = true;
                }
                else if (fontInfo.spacing == 0)
                {
                    std::string lowerName = fontInfo.family;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                    font.monospace = (lowerName.find("courier") != std::string::npos ||
                                     lowerName.find("consolas") != std::string::npos ||
                                     lowerName.find("mono") != std::string::npos ||
                                     lowerName.find("fixed") != std::string::npos ||
                                     lowerName.find("terminal") != std::string::npos ||
                                     lowerName.find("inconsolata") != std::string::npos ||
                                     lowerName.find("iosevka") != std::string::npos ||
                                     lowerName.find("fira code") != std::string::npos ||
                                     lowerName.find("dejavu sans mono") != std::string::npos ||
                                     lowerName.find("liberation mono") != std::string::npos);
                }
                else
                {
                    // FC_DUAL = 90, treat as monospace
                    font.monospace = true;
                }

                systemFonts.push_back(font);
            }
        }

        // Append system fonts to mAvailableFonts under lock
        {
            std::lock_guard<std::mutex> lock(mFontMutex);
            for (auto& f : systemFonts)
            {
                mAvailableFonts.push_back(std::move(f));
            }
            // Clear caches so subsequent lookups see system fonts
            mFontMatchCache.clear();
        }

        mEnumDone = true;
    });

    mInitialized = true;
    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  nothing
///
/// @return nothing
///
/// @brief
/// Shuts down the font system and releases all resources
/// Cleans up font calculators and clears font caches
///
/////////////////////////////////////////////////////////////////////////////
void cTUIFontManager::Shutdown(void) {
    // Wait for background enumeration thread to finish before cleanup
    WaitForFontEnumeration();

    mCurrentCalculator.reset();
    mAvailableFonts.clear();
    mFontMatchCache.clear();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if background font enumeration has completed
///
/// @brief
/// Check whether the background system font discovery thread has finished.
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontManager::IsFontEnumerationDone(void) const
{
    return mEnumDone.load();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Block until the background font enumeration thread finishes.
/// Safe to call multiple times or when no thread is running.
///
/////////////////////////////////////////////////////////////////////////////
void cTUIFontManager::WaitForFontEnumeration(void)
{
    if (mEnumThread.joinable())
    {
        mEnumThread.join();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontSelectionCriteria& criteria [in] font selection parameters
///
/// @return bool [out] true if font was successfully set, false otherwise
///
/// @brief
/// Sets the current font using detailed selection criteria
/// Finds the best matching font and creates font calculator instance
///
/// @see FindBestFontMatch for font matching algorithm
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontManager::SetFont(const sTUIFontSelectionCriteria& criteria) {
    if (!mCurrentCalculator) {
        return false;
    }

    // Find best matching font (acquires mFontMutex internally)
    std::lock_guard<std::mutex> lock(mFontMutex);
    const sTUIAvailableFont* bestMatch = FindBestFontMatch(criteria);
    if (!bestMatch) {
        return false;
    }

    // Create font info structure
    sTUIFontInfo fontInfo;
    fontInfo.name = bestMatch->fullName; // Use full path instead of just family name
    fontInfo.size = criteria.size;
    fontInfo.bold = criteria.bold;
    fontInfo.italic = criteria.italic;
    fontInfo.underline = criteria.underline;

    // Set font in calculator
    if (mCurrentCalculator->SetFont(fontInfo)) {
        mCurrentFont = fontInfo;
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& family [in] font family name (e.g., "Arial", "Courier New")
/// @param  int size [in] font size in points
/// @param  bool bold [in] whether font should be bold
/// @param  bool italic [in] whether font should be italic
///
/// @return bool [out] true if font was successfully set, false otherwise
///
/// @brief
/// Sets the current font using simple parameters
/// Converts parameters to selection criteria and calls the main SetFont method
///
/// @see SetFont(const sTUIFontSelectionCriteria&) for detailed font selection
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontManager::SetFont(const std::string& family, int size, bool bold, bool italic) {
    sTUIFontSelectionCriteria criteria;
    criteria.family = family;
    criteria.size = size;
    criteria.bold = bold;
    criteria.italic = italic;
    
    return SetFont(criteria);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of unique font family names
///
/// @brief
/// Gets a list of all available font families
/// Returns alphabetically sorted list without duplicates
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUIFontManager::GetFontFamilies(void) const {
    std::lock_guard<std::mutex> lock(mFontMutex);
    std::vector<std::string> families;

    for (const auto& font : mAvailableFonts) {
        // Add family if not already in list
        if (std::find(families.begin(), families.end(), font.family) == families.end()) {
            families.push_back(font.family);
        }
    }

    std::sort(families.begin(), families.end());
    return families;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& family [in] font family name to search for
///
/// @return std::vector<sTUIAvailableFont> [out] list of fonts in the specified family
///
/// @brief
/// Gets all available fonts for a specific font family
/// Returns all styles (bold, italic, etc.) available for the family
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sTUIAvailableFont> cTUIFontManager::GetFontsByFamily(const std::string& family) const {
    std::lock_guard<std::mutex> lock(mFontMutex);
    std::vector<sTUIAvailableFont> fonts;

    for (const auto& font : mAvailableFonts) {
        if (font.family == family) {
            fonts.push_back(font);
        }
    }

    return fonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<sTUIAvailableFont> [out] list of monospace fonts
///
/// @brief
/// Gets all available monospace (fixed-width) fonts
/// Useful for code editing and terminal applications
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sTUIAvailableFont> cTUIFontManager::GetMonospaceFonts(void) const {
    std::lock_guard<std::mutex> lock(mFontMutex);
    std::vector<sTUIAvailableFont> fonts;

    for (const auto& font : mAvailableFonts) {
        if (font.monospace) {
            fonts.push_back(font);
        }
    }

    return fonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<sTUIAvailableFont> [out] list of proportional fonts
///
/// @brief
/// Gets all available proportional (variable-width) fonts
/// Suitable for document editing and general text display
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sTUIAvailableFont> cTUIFontManager::GetProportionalFonts(void) const {
    std::lock_guard<std::mutex> lock(mFontMutex);
    std::vector<sTUIAvailableFont> fonts;

    for (const auto& font : mAvailableFonts) {
        if (!font.monospace) {
            fonts.push_back(font);
        }
    }

    return fonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<sTUIAvailableFont> [out] list of system fonts
///
/// @brief
/// Gets all system-installed fonts (excludes built-in metrics)
/// These fonts use platform-specific rendering backends
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sTUIAvailableFont> cTUIFontManager::GetSystemFonts(void) const {
    std::lock_guard<std::mutex> lock(mFontMutex);
    std::vector<sTUIAvailableFont> fonts;

    for (const auto& font : mAvailableFonts) {
        if (font.backend != FONT_BACKEND_BUILT_IN_METRICS) {
            fonts.push_back(font);
        }
    }

    return fonts;
}

std::vector<sTUIAvailableFont> cTUIFontManager::GetBuiltInFonts(void) const {
    std::lock_guard<std::mutex> lock(mFontMutex);
    std::vector<sTUIAvailableFont> fonts;

    for (const auto& font : mAvailableFonts) {
        if (font.backend == FONT_BACKEND_BUILT_IN_METRICS) {
            fonts.push_back(font);
        }
    }

    return fonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<sTUIAvailableFontInfo> [out] detailed font information list
///
/// @brief
/// Gets detailed information about all available fonts
/// Includes style classification, spacing type, and backend information
///
/// @note Automatically classifies fonts as serif/sans-serif/monospace based on name patterns
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sTUIAvailableFontInfo> cTUIFontManager::GetAvailableFontDetails(void) const {
    std::lock_guard<std::mutex> lock(mFontMutex);
    std::vector<sTUIAvailableFontInfo> detailedFonts;

    for (const auto& font : mAvailableFonts) {
        sTUIAvailableFontInfo info;
        info.displayName = font.family;
        info.familyName = font.family;
        info.filePath = font.fullName; // May contain file path from fontconfig
        info.backend = font.backend;
        info.isMonospace = font.monospace;
        
        // Classify font style based on name patterns
        std::string lowerName = font.family;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        
        if (font.monospace) {
            info.styleHint = FONT_STYLE_MONOSPACE;
            info.spacing = FONT_SPACING_MONOSPACE;
        } else if (lowerName.find("serif") != std::string::npos && 
                   lowerName.find("sans") == std::string::npos) {
            info.styleHint = FONT_STYLE_SERIF;
            info.spacing = FONT_SPACING_PROPORTIONAL;
            info.isSerif = true;
        } else if (lowerName.find("sans") != std::string::npos) {
            info.styleHint = FONT_STYLE_SANS_SERIF;
            info.spacing = FONT_SPACING_PROPORTIONAL;
            info.isSansSerif = true;
        } else if (lowerName.find("times") != std::string::npos ||
                   lowerName.find("georgia") != std::string::npos ||
                   lowerName.find("palatino") != std::string::npos) {
            info.styleHint = FONT_STYLE_SERIF;
            info.spacing = FONT_SPACING_PROPORTIONAL;
            info.isSerif = true;
        } else if (lowerName.find("arial") != std::string::npos ||
                   lowerName.find("helvetica") != std::string::npos ||
                   lowerName.find("trebuchet") != std::string::npos) {
            info.styleHint = FONT_STYLE_SANS_SERIF;
            info.spacing = FONT_SPACING_PROPORTIONAL;
            info.isSansSerif = true;
        } else {
            // Default to sans-serif for unknown fonts
            info.styleHint = FONT_STYLE_SANS_SERIF;
            info.spacing = FONT_SPACING_PROPORTIONAL;
            info.isSansSerif = true;
        }
        
        detailedFonts.push_back(info);
    }
    
    return detailedFonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] text to measure
///
/// @return sTUITextMetrics [out] text measurement metrics
///
/// @brief
/// Measures text dimensions using the current font calculator
/// Delegates to the active backend for precise measurements
///
/////////////////////////////////////////////////////////////////////////////
sTUITextMetrics cTUIFontManager::MeasureText(const std::string& text) {
    if (mCurrentCalculator) {
        return mCurrentCalculator->MeasureText(text);
    }
    
    return sTUITextMetrics();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to measure
///
/// @return int [out] character width in measurement units
///
/// @brief
/// Measures the width of a single Unicode character
/// Handles font fallback for unsupported characters on Linux systems
///
/// @note Uses native fallback on Windows/macOS, manual fallback on Linux
/// @see HasNativeFontFallback for platform fallback capabilities
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontManager::MeasureCharacterWidth(char32_t codepoint) {
    if (mCurrentCalculator) {
        // Check if native fallback is available
        if (HasNativeFontFallback()) {
            // Windows DirectWrite, macOS CoreText, and Qt handle fallback automatically
            return mCurrentCalculator->MeasureCharacterWidth(codepoint);
        }
        
        // For Linux without Qt/DirectWrite/CoreText: use manual fallback system
        // Try primary font first
        if (mCurrentCalculator->SupportsCharacter(codepoint)) {
            return mCurrentCalculator->MeasureCharacterWidth(codepoint);
        }
        
        // If not supported, try to find a fallback font
        // Check fallback cache first
        std::string fallbackPath;
        auto cacheIt = mFallbackCache.find(codepoint);
        if (cacheIt != mFallbackCache.end()) {
            fallbackPath = cacheIt->second;
        } else {
            fallbackPath = FindFallbackFontForCharacter(codepoint);
            mFallbackCache[codepoint] = fallbackPath; // Cache the result
        }
        
        if (!fallbackPath.empty()) {
            // Check if we have a cached calculator for this font
            auto calcIt = mFallbackCalculators.find(fallbackPath);
            if (calcIt == mFallbackCalculators.end()) {
                // Create new calculator and cache it
                auto fallbackCalc = cTUIFontMeasurementManager::CreateFontCalculator(mDocumentMode);
                if (fallbackCalc && fallbackCalc->Initialize()) {
                    sTUIFontInfo fallbackFont = mCurrentFont; // Copy current font settings
                    fallbackFont.name = fallbackPath; // Use fallback font path
                    
                    if (fallbackCalc->SetFont(fallbackFont)) {
                        mFallbackCalculators[fallbackPath] = std::move(fallbackCalc);
                    }
                }
            }
            
            // Use cached calculator
            auto& calculator = mFallbackCalculators[fallbackPath];
            if (calculator) {
                return calculator->MeasureCharacterWidth(codepoint);
            }
        }
        
        // Fallback failed, use primary font's measurement anyway
        return mCurrentCalculator->MeasureCharacterWidth(codepoint);
    }
    
    return 144; // Default character width
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return int [out] line height in measurement units
///
/// @brief
/// Gets the line height for the current font
/// Delegates to the active font calculator backend
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontManager::GetLineHeight(void) {
    if (mCurrentCalculator) {
        return mCurrentCalculator->GetLineHeight();
    }
    
    return 240; // Default line height
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool documentMode [in] true for document mode, false for terminal mode
///
/// @return nothing
///
/// @brief
/// Sets the font measurement mode (document or terminal)
/// Updates the current calculator if the mode changes
///
/// @note Document mode provides higher precision, terminal mode uses wcwidth()
///
/////////////////////////////////////////////////////////////////////////////
void cTUIFontManager::SetDocumentMode(bool documentMode) {
    if (mDocumentMode != documentMode) {
        mDocumentMode = documentMode;
        
        if (mCurrentCalculator) {
            mCurrentCalculator->SetDocumentMode(documentMode);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string [out] human-readable backend name
///
/// @brief
/// Gets the name of the currently active font measurement backend
/// Returns descriptive name for display purposes
///
/////////////////////////////////////////////////////////////////////////////
std::string cTUIFontManager::GetBackendName(void) const {
    switch (mCurrentBackend) {
        case FONT_BACKEND_DIRECTWRITE: return "DirectWrite";
        case FONT_BACKEND_WINDOWS_NATIVE: return "Windows GDI";
        case FONT_BACKEND_MACOS_NATIVE: return "macOS CoreText";
        case FONT_BACKEND_QT_HEADLESS: return "Qt Headless";
        case FONT_BACKEND_HARFBUZZ: return "HarfBuzz";
        case FONT_BACKEND_STB_TRUETYPE: return "STB TrueType";
        case FONT_BACKEND_BUILT_IN_METRICS: return "Built-in Metrics";
        default: return "Unknown";
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<eTUIFontMeasurementBackend> [out] list of available backends
///
/// @brief
/// Gets a list of all font measurement backends available on current platform
/// Checks library availability and platform-specific support
///
/// @note Built-in metrics backend is always available as fallback
///
/////////////////////////////////////////////////////////////////////////////
std::vector<eTUIFontMeasurementBackend> cTUIFontManager::GetAvailableBackends(void) const {
    std::vector<eTUIFontMeasurementBackend> backends;
    
    // Always available
    backends.push_back(FONT_BACKEND_BUILT_IN_METRICS);
    
    // Check platform-specific backends
#ifdef _WIN32
    backends.push_back(FONT_BACKEND_DIRECTWRITE);
    backends.push_back(FONT_BACKEND_WINDOWS_NATIVE);
#elif defined(__APPLE__)
    backends.push_back(FONT_BACKEND_MACOS_NATIVE);
#else
    if (cTUIFontMeasurementManager::HasQtLibraries()) {
        backends.push_back(FONT_BACKEND_QT_HEADLESS);
    }
    if (cTUIFontMeasurementManager::HasHarfBuzzLibraries()) {
        backends.push_back(FONT_BACKEND_HARFBUZZ);
    }
#endif
    
    if (cTUIFontMeasurementManager::HasSystemFonts()) {
        backends.push_back(FONT_BACKEND_STB_TRUETYPE);
    }
    
    return backends;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& family [in] font family name to validate
/// @param  int size [in] font size to validate
///
/// @return bool [out] true if font specification is valid
///
/// @brief
/// Validates font family name and size parameters
/// Checks if family exists and size is within reasonable range (6-72pt)
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontManager::IsValidFont(const std::string& family, int size) const {
    // Basic validation
    if (family.empty() || size < 6 || size > 72) {
        return false;
    }

    // Check if font family exists
    std::lock_guard<std::mutex> lock(mFontMutex);
    for (const auto& font : mAvailableFonts) {
        if (font.family == family) {
            return true;
        }
    }

    return false;
}

bool cTUIFontManager::SupportsCharacter(char32_t codepoint) {
    if (mCurrentCalculator) {
        // Check primary font first
        if (mCurrentCalculator->SupportsCharacter(codepoint)) {
            return true;
        }
        
        // If not supported by primary font, check if fallback exists
        // Check cache first
        auto cacheIt = mFallbackCache.find(codepoint);
        if (cacheIt != mFallbackCache.end()) {
            return !cacheIt->second.empty();
        } else {
            std::string fallbackPath = FindFallbackFontForCharacter(codepoint);
            mFallbackCache[codepoint] = fallbackPath; // Cache the result
            return !fallbackPath.empty();
        }
    }
    
    return false;
}

bool cTUIFontManager::SupportsUnicode(void) const {
    if (mCurrentCalculator) {
        return mCurrentCalculator->SupportsUnicode();
    }
    
    return false;
}

std::string cTUIFontManager::GetDefaultFontFamily(bool monospace) {
    if (monospace) {
#ifdef _WIN32
        return "Courier New";
#elif defined(__APPLE__)
        return "Monaco";
#else
        return "Liberation Mono";
#endif
    } else {
#ifdef _WIN32
        return "Times New Roman";
#elif defined(__APPLE__)
        return "Times";
#else
        return "Liberation Serif";
#endif
    }
}

std::vector<int> cTUIFontManager::GetCommonFontSizes(void) {
    return {8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72};
}

std::string cTUIFontManager::GetFontSampleText(const std::string& family) const {
    // Return appropriate sample text based on font type
    auto fonts = GetFontsByFamily(family);
    if (!fonts.empty() && fonts[0].monospace) {
        return "The quick brown fox jumps over the lazy dog. 1234567890";
    } else {
        return "The quick brown fox jumps over the lazy dog.\nABCDEFGHIJKLMNOPQRSTUVWXYZ\nabcdefghijklmnopqrstuvwxyz\n1234567890 !@#$%^&*()";
    }
}

sTUITextMetrics cTUIFontManager::GetFontMetrics(const std::string& family, int size) {
    // Save current font
    sTUIFontInfo savedFont = mCurrentFont;
    
    // Temporarily set requested font
    if (SetFont(family, size)) {
        sTUITextMetrics metrics = MeasureText("Test");
        
        // Restore original font
        if (mCurrentCalculator) {
            mCurrentCalculator->SetFont(savedFont);
        }
        
        return metrics;
    }
    
    return sTUITextMetrics();
}

void cTUIFontManager::EnumerateAllFonts(void) {
    mAvailableFonts.clear();
    
    // Try to add system fonts first
    AddSystemFonts();
    
    // Only add built-in fonts if no system fonts were found
    if (mAvailableFonts.empty()) {
        AddBuiltInFonts();
    }
}

void cTUIFontManager::AddBuiltInFonts(void) {
    std::vector<std::string> builtInFonts = cTUIBuiltInMetricsFontCalculator::GetBuiltInFontNames();
    
    for (const std::string& fontName : builtInFonts) {
        sTUIAvailableFont font;
        font.family = fontName;
        font.fullName = fontName;
        font.backend = FONT_BACKEND_BUILT_IN_METRICS;
        
        // Determine if monospace based on name
        std::string lowerName = fontName;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
        font.monospace = (lowerName.find("courier") != std::string::npos ||
                         lowerName.find("consolas") != std::string::npos ||
                         lowerName.find("mono") != std::string::npos ||
                         lowerName.find("terminal") != std::string::npos);
        
        mAvailableFonts.push_back(font);
    }
}

void cTUIFontManager::AddSystemFonts(void) {
    // Discover all fonts using fontconfig or directory scanning
    std::vector<FontInfo> discoveredFonts = DiscoverSystemFonts();
    
    // Get the best available backend to use for all fonts
    eTUIFontMeasurementBackend bestBackend = cTUIFontMeasurementManager::DetectBestBackend();
    
    for (const FontInfo& fontInfo : discoveredFonts) {
        // Parse style information first (need bold/italic for dedup key)
        std::string lowerStyle = fontInfo.style;
        std::transform(lowerStyle.begin(), lowerStyle.end(), lowerStyle.begin(), ::tolower);
        bool isBold = (lowerStyle.find("bold") != std::string::npos);
        bool isItalic = (lowerStyle.find("italic") != std::string::npos ||
                         lowerStyle.find("oblique") != std::string::npos);

        // Check if this exact family+style combination already exists (avoid duplicates)
        // Include bold/italic in key so all style variants of a family are kept
        bool exists = false;
        std::string normalizedName = NormalizeFontName(fontInfo.family);
        for (const auto& existingFont : mAvailableFonts) {
            std::string existingNormalizedName = NormalizeFontName(existingFont.family);
            if (existingNormalizedName == normalizedName &&
                existingFont.bold == isBold &&
                existingFont.italic == isItalic) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            sTUIAvailableFont font;
            font.family = fontInfo.family;
            font.fullName = fontInfo.filepath; // Store the full path for reference
            font.backend = bestBackend; // All system fonts use the same best backend

            font.bold = isBold;
            font.italic = isItalic;
            
            // Use fontconfig spacing information if available, otherwise fall back to keyword detection
            if (fontInfo.spacing == 100) {
                // Explicitly monospace according to fontconfig
                font.monospace = true;
            } else if (fontInfo.spacing == 0) {
                // Explicitly proportional according to fontconfig, or spacing unknown
                // Check if this is a known monospace font by name
                std::string lowerName = fontInfo.family;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                font.monospace = (lowerName.find("courier") != std::string::npos ||
                                 lowerName.find("consolas") != std::string::npos ||
                                 lowerName.find("mono") != std::string::npos ||
                                 lowerName.find("fixed") != std::string::npos ||
                                 lowerName.find("terminal") != std::string::npos ||
                                 lowerName.find("inconsolata") != std::string::npos ||
                                 lowerName.find("iosevka") != std::string::npos ||
                                 lowerName.find("fira code") != std::string::npos ||
                                 lowerName.find("dejavu sans mono") != std::string::npos ||
                                 lowerName.find("liberation mono") != std::string::npos);
            } else {
                // FC_DUAL = 90, treat as monospace
                font.monospace = true;
            }
            
            mAvailableFonts.push_back(font);
        }
    }
}

const sTUIAvailableFont* cTUIFontManager::FindBestFontMatch(const sTUIFontSelectionCriteria& criteria) {
    if (mAvailableFonts.empty()) {
        return nullptr;
    }

    // Check cache first -- avoids looping through all available fonts
    std::string key = criteria.family + "|" + std::to_string(criteria.size) + "|"
                    + (criteria.bold ? "1" : "0") + "|" + (criteria.italic ? "1" : "0");

    auto it = mFontMatchCache.find(key);
    if (it != mFontMatchCache.end()) {
        return it->second;
    }

    // Full search: score every available font and pick the best match
    const sTUIAvailableFont* bestMatch = nullptr;
    int bestScore = -1;

    for (const auto& font : mAvailableFonts) {
        int score = CalculateFontScore(font, criteria);
        if (score > bestScore) {
            bestScore = score;
            bestMatch = &font;
        }
    }

    mFontMatchCache[key] = bestMatch;
    return bestMatch;
}

int cTUIFontManager::CalculateFontScore(const sTUIAvailableFont& font, const sTUIFontSelectionCriteria& criteria) {
    int score = 0;
    
    // Family name matching (case-insensitive)
    std::string fontFamily = NormalizeFontName(font.family);
    std::string requestedFamily = NormalizeFontName(criteria.family);
    
    if (fontFamily == requestedFamily) {
        score += 1000; // Exact match
    } else if (fontFamily.find(requestedFamily) != std::string::npos ||
               requestedFamily.find(fontFamily) != std::string::npos) {
        score += 500; // Partial match
    }
    
    // Style matching
    if (font.bold == criteria.bold) score += 100;
    if (font.italic == criteria.italic) score += 100;
    if (font.monospace == criteria.monospace) score += 200;
    
    // Backend preference (system fonts preferred over built-in)
    if (font.backend != FONT_BACKEND_BUILT_IN_METRICS) {
        score += 50;
    }
    
    return score;
}

bool cTUIFontManager::CreateFontCalculator(eTUIFontMeasurementBackend /*preferredBackend*/) {
    mCurrentCalculator = cTUIFontMeasurementManager::CreateFontCalculator(mDocumentMode);
    
    if (mCurrentCalculator) {
        mCurrentBackend = mCurrentCalculator->GetBackend();
        return true;
    }
    
    return false;
}

std::vector<cTUIFontManager::FontInfo> cTUIFontManager::DiscoverSystemFonts(void) {
    std::vector<FontInfo> fonts;
    
#ifdef HAVE_FONTCONFIG
    // Use fontconfig if available
    std::cout << "Using fontconfig for font discovery...\n";
    
    if (FcInit()) {
        FcPattern *pat = FcPatternCreate();
        FcObjectSet *os = FcObjectSetBuild(FC_FAMILY, FC_STYLE, FC_FILE, FC_SPACING, (char *)0);
        FcFontSet *fs = FcFontList(0, pat, os);
        
        for (int i = 0; i < fs->nfont; i++) {
            FcChar8 *family, *style, *file;
            int spacing = 0; // Default to proportional (0) if spacing not available
            if (FcPatternGetString(fs->fonts[i], FC_FAMILY, 0, &family) == FcResultMatch &&
                FcPatternGetString(fs->fonts[i], FC_STYLE, 0, &style) == FcResultMatch &&
                FcPatternGetString(fs->fonts[i], FC_FILE, 0, &file) == FcResultMatch) {
                
                // Try to get spacing information, but don't require it
                FcPatternGetInteger(fs->fonts[i], FC_SPACING, 0, &spacing);
                
                FontInfo font;
                font.family = (char*)family;
                font.style = (char*)style;
                font.filepath = (char*)file;
                
                // Store spacing information for later use
                // FC_MONO = 100, FC_DUAL = 90, FC_PROPORTIONAL = 0
                font.spacing = spacing;
                
                // Only include TTF/OTF fonts
                std::string filepath = font.filepath;
                std::string extension = filepath.substr(filepath.find_last_of(".") + 1);
                std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                
                if (extension == "ttf" || extension == "otf" || extension == "ttc") {
                    fonts.push_back(font);
                }
            }
        }
        
        FcFontSetDestroy(fs);
        FcObjectSetDestroy(os);
        FcPatternDestroy(pat);
        FcFini();
    }
#elif defined(__APPLE__)
    // CoreText enumerates every installed font with a real file URL directly --
    // no directory list to keep in sync with macOS's actual font locations
    // (the Linux-oriented DiscoverSystemFontsByScanning() fallback below only
    // knows about paths like /usr/share/fonts, which don't exist here).
    std::cout << "Using CoreText for font discovery...\n";

    CTFontCollectionRef collection = CTFontCollectionCreateFromAvailableFonts(nullptr);
    if (collection)
    {
        CFArrayRef descriptors = CTFontCollectionCreateMatchingFontDescriptors(collection);
        if (descriptors)
        {
            CFIndex count = CFArrayGetCount(descriptors);
            for (CFIndex i = 0; i < count; i++)
            {
                CTFontDescriptorRef descriptor =
                    (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, i);

                CFURLRef urlRef = (CFURLRef)CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute);
                if (!urlRef)
                {
                    continue;
                }

                UInt8 pathBuffer[PATH_MAX];
                bool gotPath = CFURLGetFileSystemRepresentation(urlRef, true, pathBuffer, sizeof(pathBuffer));
                CFRelease(urlRef);
                if (!gotPath)
                {
                    continue;
                }

                std::string filePath(reinterpret_cast<char*>(pathBuffer));

                // Only TTF/OTF/TTC, matching the filter every other backend applies
                std::string extension;
                size_t dotPos = filePath.find_last_of('.');
                if (dotPos != std::string::npos)
                {
                    extension = filePath.substr(dotPos + 1);
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                }
                if (extension != "ttf" && extension != "otf" && extension != "ttc")
                {
                    continue;
                }

                CFStringRef familyRef = (CFStringRef)CTFontDescriptorCopyAttribute(
                    descriptor, kCTFontFamilyNameAttribute);
                if (!familyRef)
                {
                    continue;
                }

                CFIndex nameLen = CFStringGetLength(familyRef);
                CFIndex nameMax = CFStringGetMaximumSizeForEncoding(nameLen, kCFStringEncodingUTF8) + 1;
                std::vector<char> nameBuffer(static_cast<size_t>(nameMax));
                bool gotName = CFStringGetCString(familyRef, nameBuffer.data(), nameMax, kCFStringEncodingUTF8);
                CFRelease(familyRef);
                if (!gotName)
                {
                    continue;
                }

                // Symbolic traits give bold/italic/monospace without loading the font
                CFDictionaryRef traitsDict = (CFDictionaryRef)CTFontDescriptorCopyAttribute(
                    descriptor, kCTFontTraitsAttribute);
                bool bold = false;
                bool italic = false;
                bool monospace = false;
                if (traitsDict)
                {
                    CFNumberRef symbolicRef = (CFNumberRef)CFDictionaryGetValue(
                        traitsDict, kCTFontSymbolicTrait);
                    if (symbolicRef)
                    {
                        int32_t symbolicTraits = 0;
                        CFNumberGetValue(symbolicRef, kCFNumberSInt32Type, &symbolicTraits);
                        bold = (symbolicTraits & kCTFontBoldTrait) != 0;
                        italic = (symbolicTraits & kCTFontItalicTrait) != 0;
                        monospace = (symbolicTraits & kCTFontMonoSpaceTrait) != 0;
                    }
                    CFRelease(traitsDict);
                }

                FontInfo font;
                font.family = nameBuffer.data();
                font.style = std::string(bold ? "Bold" : "") + (italic ? "Italic" : "");
                if (font.style.empty())
                {
                    font.style = "Regular";
                }
                font.filepath = filePath;
                font.spacing = monospace ? 100 : 0; // matches fontconfig's FC_MONO/FC_PROPORTIONAL scale

                fonts.push_back(std::move(font));
            }
            CFRelease(descriptors);
        }
        CFRelease(collection);
    }
#else
    // Fallback to directory scanning if fontconfig is not available
    fonts = DiscoverSystemFontsByScanning();
#endif
    
    return fonts;
}

std::vector<cTUIFontManager::FontInfo> cTUIFontManager::DiscoverSystemFontsByScanning(void) {
    std::vector<FontInfo> fonts;
    std::cout << "Using directory scanning for font discovery...\n";
    std::vector<std::string> fontDirs = {
        "/usr/share/fonts",
        "/usr/local/share/fonts"
    };
    
    // Add user font directories
    const char* home = getenv("HOME");
    if (home) {
        fontDirs.push_back(std::string(home) + "/.fonts");
        fontDirs.push_back(std::string(home) + "/.local/share/fonts");
    }
    
    for (const std::string& dir : fontDirs) {
        DIR* directory = opendir(dir.c_str());
        if (!directory) continue;
        
        struct dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
                std::string filename = entry->d_name;
                std::string fullPath = dir + "/" + filename;
                
                // Check if it's a font file by extension
                if (filename.length() >= 4) {
                    std::string extension = filename.substr(filename.length() - 4);
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                    
                    if (extension == ".ttf" || extension == ".otf" || extension == ".ttc") {
                        // Verify it's actually a file
                        struct stat fileStat;
                        if (stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                            FontInfo font;
                            font.family = ExtractFontFamilyName(fullPath);
                            font.style = "Regular"; // Default style when scanning
                            font.filepath = fullPath;
                            fonts.push_back(font);
                        }
                    }
                }
            }
        }
        
        closedir(directory);
    }
    
    return fonts;
}

std::vector<std::string> cTUIFontManager::DiscoverSystemFontFiles(void) {
    std::vector<std::string> fontFiles;
    std::vector<std::string> fontDirs = {
        "/usr/share/fonts",
        "/usr/local/share/fonts",
        "/usr/share/fonts/truetype",
        "/usr/share/fonts/TTF",
        "/usr/share/fonts/opentype",
        "/usr/share/fonts/OTF"
    };
    
    // Add user font directories
    const char* home = getenv("HOME");
    if (home) {
        fontDirs.push_back(std::string(home) + "/.fonts");
        fontDirs.push_back(std::string(home) + "/.local/share/fonts");
    }
    
    for (const std::string& dir : fontDirs) {
        DIR* directory = opendir(dir.c_str());
        if (!directory) continue;
        
        struct dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            if (entry->d_type == DT_REG || entry->d_type == DT_UNKNOWN) {
                std::string filename = entry->d_name;
                std::string fullPath = dir + "/" + filename;
                
                // Check if it's a font file by extension
                if (filename.length() >= 4) {
                    std::string extension = filename.substr(filename.length() - 4);
                    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
                    
                    if (extension == ".ttf" || extension == ".otf" || extension == ".ttc") {
                        // Verify it's actually a file
                        struct stat fileStat;
                        if (stat(fullPath.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                            fontFiles.push_back(fullPath);
                        }
                    }
                }
            } else if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                // Skip subdirectories for now to avoid infinite recursion and complexity
                // In a full implementation, you'd make this properly recursive
                continue;
            }
        }
        
        closedir(directory);
    }
    
    return fontFiles;
}

std::string cTUIFontManager::ExtractFontFamilyName(const std::string& fontPath) {
    // Extract family name from file path - simplified version
    // In a real implementation, you'd parse the font file headers
    size_t lastSlash = fontPath.find_last_of('/');
    size_t lastDot = fontPath.find_last_of('.');
    
    if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
        std::string filename = fontPath.substr(lastSlash + 1, lastDot - lastSlash - 1);
        
        // Replace hyphens and underscores with spaces
        std::replace(filename.begin(), filename.end(), '-', ' ');
        std::replace(filename.begin(), filename.end(), '_', ' ');
        
        return filename;
    }
    
    return "Unknown Font";
}

std::string cTUIFontManager::NormalizeFontName(const std::string& name) const {
    std::string normalized = name;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
    
    // Remove hyphens, underscores, and extra spaces
    std::replace(normalized.begin(), normalized.end(), '-', ' ');
    std::replace(normalized.begin(), normalized.end(), '_', ' ');
    
    // Remove common suffixes and prefixes
    size_t pos = normalized.find(" regular");
    if (pos != std::string::npos) {
        normalized.erase(pos, 8);
    }
    
    pos = normalized.find(" normal");
    if (pos != std::string::npos) {
        normalized.erase(pos, 7);
    }
    
    // Remove extra whitespace
    std::string result;
    bool lastWasSpace = false;
    for (char c : normalized) {
        if (c == ' ') {
            if (!lastWasSpace) {
                result += c;
                lastWasSpace = true;
            }
        } else {
            result += c;
            lastWasSpace = false;
        }
    }
    
    // Trim leading/trailing spaces
    if (!result.empty() && result.back() == ' ') result.pop_back();
    if (!result.empty() && result.front() == ' ') result.erase(0, 1);
    
    return result;
}

bool cTUIFontManager::HasNativeFontFallback(void) const {
    if (!mCurrentCalculator) return false;
    
    // Check if current backend supports native font fallback
    switch (mCurrentBackend) {
        case FONT_BACKEND_DIRECTWRITE:    // Windows DirectWrite has IDWriteFontFallback
        case FONT_BACKEND_MACOS_NATIVE:   // macOS CoreText has CTFontCreateForString with cascades
        case FONT_BACKEND_QT_HEADLESS:    // Qt automatically handles font fallback
            return true;
            
        case FONT_BACKEND_WINDOWS_NATIVE: // Windows GDI has some fallback but limited
        case FONT_BACKEND_HARFBUZZ:       // HarfBuzz + fontconfig manual fallback
        case FONT_BACKEND_STB_TRUETYPE:   // No automatic fallback
        case FONT_BACKEND_BUILT_IN_METRICS: // No automatic fallback
        default:
            return false;
    }
}

std::string cTUIFontManager::FindFallbackFontForCharacter(char32_t codepoint) const {
#ifdef HAVE_FONTCONFIG
    if (FcInit()) {
        // Create a pattern for the character
        FcPattern *pat = FcPatternCreate();
        FcCharSet *charset = FcCharSetCreate();
        FcCharSetAddChar(charset, codepoint);
        FcPatternAddCharSet(pat, FC_CHARSET, charset);
        
        // Find the best matching font
        FcConfigSubstitute(NULL, pat, FcMatchPattern);
        FcDefaultSubstitute(pat);
        
        FcResult result;
        FcPattern *match = FcFontMatch(NULL, pat, &result);
        
        std::string fontPath;
        if (match && result == FcResultMatch) {
            FcChar8 *file = NULL;
            if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch) {
                fontPath = (char*)file;
            }
        }
        
        // Clean up
        if (match) FcPatternDestroy(match);
        FcPatternDestroy(pat);
        FcCharSetDestroy(charset);
        FcFini();
        
        return fontPath;
    }
#endif
    return ""; // No fallback available
}

std::string cTUIFontManager::GetFontNameUsedForCharacter(char32_t codepoint) const {
    // For systems with native fallback, we can't determine the exact font used
    // The native system (DirectWrite/CoreText/Qt) handles it internally
    if (HasNativeFontFallback()) {
        return mCurrentFont.name + " (or fallback)";
    }
    
    // For manual fallback systems (Linux without Qt)
    if (mCurrentCalculator && mCurrentCalculator->SupportsCharacter(codepoint)) {
        // Character supported by primary font
        return mCurrentFont.name;
    }
    
    // Check fallback cache
    auto cacheIt = mFallbackCache.find(codepoint);
    if (cacheIt != mFallbackCache.end() && !cacheIt->second.empty()) {
        // Extract font name from path
        std::string path = cacheIt->second;
        size_t lastSlash = path.find_last_of('/');
        if (lastSlash != std::string::npos) {
            std::string filename = path.substr(lastSlash + 1);
            size_t lastDot = filename.find_last_of('.');
            if (lastDot != std::string::npos) {
                return filename.substr(0, lastDot);
            }
            return filename;
        }
        return path;
    }
    
    return mCurrentFont.name; // Default to current font
}

std::vector<sTUIAvailableFontInfo> cTUIFontManager::FindFontsByStyle(eTUIFontStyleHint styleHint) const {
    std::vector<sTUIAvailableFontInfo> matchingFonts;
    std::vector<sTUIAvailableFontInfo> allFonts = GetAvailableFontDetails();
    
    for (const auto& font : allFonts) {
        if (font.styleHint == styleHint) {
            matchingFonts.push_back(font);
        }
    }
    
    return matchingFonts;
}

std::string cTUIFontManager::FindBestStyleMatch(const std::string& requestedFont, eTUIFontStyleHint preferredStyle) const {
    std::vector<sTUIAvailableFontInfo> allFonts = GetAvailableFontDetails();
    
    // First try: exact name match
    for (const auto& font : allFonts) {
        if (font.familyName == requestedFont || font.displayName == requestedFont) {
            return font.familyName;
        }
    }
    
    // Second try: find fonts with the preferred style
    std::vector<sTUIAvailableFontInfo> styleMatches = FindFontsByStyle(preferredStyle);
    if (!styleMatches.empty()) {
        return styleMatches[0].familyName; // Return first match of preferred style
    }
    
    // Third try: fallback based on style hierarchy
    std::vector<eTUIFontStyleHint> fallbackOrder;
    switch (preferredStyle) {
        case FONT_STYLE_MONOSPACE:
            fallbackOrder = {FONT_STYLE_MONOSPACE, FONT_STYLE_SANS_SERIF, FONT_STYLE_SERIF};
            break;
        case FONT_STYLE_SERIF:
            fallbackOrder = {FONT_STYLE_SERIF, FONT_STYLE_SANS_SERIF, FONT_STYLE_MONOSPACE};
            break;
        case FONT_STYLE_SANS_SERIF:
        default:
            fallbackOrder = {FONT_STYLE_SANS_SERIF, FONT_STYLE_SERIF, FONT_STYLE_MONOSPACE};
            break;
    }
    
    for (eTUIFontStyleHint fallbackStyle : fallbackOrder) {
        std::vector<sTUIAvailableFontInfo> fallbackFonts = FindFontsByStyle(fallbackStyle);
        if (!fallbackFonts.empty()) {
            return fallbackFonts[0].familyName;
        }
    }
    
    // Final fallback: return any available font
    if (!allFonts.empty()) {
        return allFonts[0].familyName;
    }
    
    // Ultimate fallback
    return GetDefaultFontFamily();
}

sTUIFontInfo cTUIFontManager::GetCurrentFont(void) const 
{
    return mCurrentFont;
}

std::vector<sTUIAvailableFont> cTUIFontManager::GetAvailableFonts(void) const
{
    std::lock_guard<std::mutex> lock(mFontMutex);
    return std::vector<sTUIAvailableFont>(mAvailableFonts.begin(), mAvailableFonts.end());
}

bool cTUIFontManager::IsDocumentMode(void) const 
{
    return mDocumentMode;
}

eTUIFontMeasurementBackend cTUIFontManager::GetCurrentBackend(void) const 
{
    return mCurrentBackend;
}

int cTUIFontManager::GetDefaultFontSize(void) 
{
    return 12;
}