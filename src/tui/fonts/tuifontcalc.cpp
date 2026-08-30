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
 * @class cTUIFontCalculator
 * @brief Font measurement backend detection and selection system.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIFontMeasurementManager, which probes for available font
 * backends at runtime and selects the highest-quality option. Provides
 * platform-specific detection helpers that check for shared libraries
 * and system font directories.
 *
 * @section fontcalc_priority Backend Priority Order
 * Backends are probed in descending quality order: DirectWrite (Windows) >
 * CoreText (macOS) > Qt Headless > HarfBuzz > STB TrueType > Built-in
 * metrics. The first backend that initializes successfully is used for all
 * subsequent font measurement operations. The eTUIFontMeasurementBackend enum
 * identifies the selected backend.
 *
 * @see cTUIFontMeasurementManager
 * @see eTUIFontMeasurementBackend
 * @see cTUIFontCalculator
 */

#include "tuifontcalc.h"
#include "builtinmetrics.h"
#include "stbtruetype.h"
#include "platform_windows.h"
#include "platform_directwrite.h"
#include "platform_macos.h"
#include "platform_qt.h"
#include "platform_harfbuzz.h"
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#include <wingdi.h>
#elif defined(__APPLE__)
#include <CoreText/CoreText.h>
#include <CoreFoundation/CoreFoundation.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#include <sys/stat.h>
#include <wchar.h>
#endif

// Font measurement manager implementation
/////////////////////////////////////////////////////////////////////////////
///
/// @return eTUIFontMeasurementBackend [out] detected optimal font backend for platform
///
/// @brief
/// Detects the best available font measurement backend for current platform
/// Returns DirectWrite for Windows, CoreText for macOS, Qt/HarfBuzz for Linux
///
/// @note Falls back through backends in priority order based on availability
///
/////////////////////////////////////////////////////////////////////////////
eTUIFontMeasurementBackend cTUIFontMeasurementManager::DetectBestBackend(void) {
#ifdef _WIN32
    // Try DirectWrite first (Windows 7+), fallback to GDI
    return FONT_BACKEND_DIRECTWRITE;
#elif defined(__APPLE__)
    return FONT_BACKEND_MACOS_NATIVE;    // Always available on macOS
#else // Linux - detect in priority order: Qt -> HarfBuzz -> STB -> Built-in
    if (HasQtLibraries()) {
        return FONT_BACKEND_QT_HEADLESS;
    }
    if (HasHarfBuzzLibraries()) {
        return FONT_BACKEND_HARFBUZZ;
    }
    if (HasSystemFonts()) {
        return FONT_BACKEND_STB_TRUETYPE;
    }
    return FONT_BACKEND_BUILT_IN_METRICS;
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if Qt libraries are available and usable
///
/// @brief
/// Checks if Qt Core libraries are available for font measurement
/// Platform-specific detection based on build configuration
///
/// @note Only available on Linux builds with Qt support enabled
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontMeasurementManager::HasQtLibraries(void) {
#ifdef _WIN32
    return false;  // Don't use Qt on Windows for TUI
#elif defined(__APPLE__)
    return false;  // Don't use Qt on macOS for TUI
#else
    // Only available if Qt was found at build time
#ifdef HAVE_QT_CORE
    return true;
#else
    return false;
#endif
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if HarfBuzz libraries are available
///
/// @brief
/// Checks if HarfBuzz text shaping libraries are available
/// Dynamically loads libharfbuzz.so.0 to test availability
///
/// @note Currently only implemented for Linux platforms
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontMeasurementManager::HasHarfBuzzLibraries(void) {
#ifdef _WIN32
    return false;  // TODO: Implement Windows HarfBuzz detection
#elif defined(__APPLE__)
    return false;  // TODO: Implement macOS HarfBuzz detection
#else
    void* handle = dlopen("libharfbuzz.so.0", RTLD_LAZY);
    if (handle) {
        dlclose(handle);
        return true;
    }
    return false;
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if system fonts are available
///
/// @brief
/// Checks if system font directories exist and are accessible
/// Scans common font directories on each platform
///
/// @note Always returns true as STB TrueType can work with embedded fonts
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontMeasurementManager::HasSystemFonts(void) {
#ifdef _WIN32
    // Windows always has system fonts
    return true;
#elif defined(__APPLE__)
    // macOS always has system fonts
    return true;
#else
    // Check for common Linux font directories
    struct stat st;
    if (stat("/usr/share/fonts", &st) == 0 && S_ISDIR(st.st_mode)) return true;
    if (stat("/usr/local/share/fonts", &st) == 0 && S_ISDIR(st.st_mode)) return true;
    if (stat("/home/.fonts", &st) == 0 && S_ISDIR(st.st_mode)) return true;
    if (stat("~/.local/share/fonts", &st) == 0 && S_ISDIR(st.st_mode)) return true;
    if (stat("/var/lib/defoma/fontconfig.d/", &st) == 0 && S_ISDIR(st.st_mode)) return true;
    // Even if no font directories found, STB can work with embedded fonts
    return true;  // STB TrueType should always be available
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool documentMode [in] true for document mode, false for terminal mode
///
/// @return std::unique_ptr<cTUIFontCalculator> [out] font calculator instance
///
/// @brief
/// Creates the best available font calculator for the current platform
/// Uses platform-specific backends with fallbacks for failed initialization
///
/// @note Document mode uses precision backends, terminal mode uses built-in metrics
///
/////////////////////////////////////////////////////////////////////////////
std::unique_ptr<cTUIFontCalculator> cTUIFontMeasurementManager::CreateFontCalculator(bool documentMode) {
    eTUIFontMeasurementBackend backend = DetectBestBackend();
    
    std::unique_ptr<cTUIFontCalculator> calculator;
    
    if (documentMode) {
        // Document mode: try best available precision backend
        switch (backend) {
#ifdef _WIN32
            case FONT_BACKEND_DIRECTWRITE:
                calculator = std::make_unique<cTUIFontCalculatorDirectWrite>();
                break;
            case FONT_BACKEND_WINDOWS_NATIVE:
                calculator = std::make_unique<cTUIFontCalculatorWindowsNative>();
                break;
#elif defined(__APPLE__)
            case FONT_BACKEND_MACOS_NATIVE:
                calculator = std::make_unique<cTUIFontCalculatorMacOSNative>();
                break;
#else
            case FONT_BACKEND_QT_HEADLESS:
                calculator = std::make_unique<cTUIFontCalculatorQtHeadless>();
                break;
            case FONT_BACKEND_HARFBUZZ:
                calculator = std::make_unique<cTUIFontCalculatorHarfBuzz>();
                break;
            case FONT_BACKEND_STB_TRUETYPE:
                calculator = std::make_unique<cTUISTBTrueTypeFontCalculator>();
                break;
#endif
            default:
                // Fall back to built-in metrics
                calculator = std::make_unique<cTUIBuiltInMetricsFontCalculator>();
                break;
        }
    } else {
        // Terminal mode: always use built-in metrics for simplicity
        calculator = std::make_unique<cTUIBuiltInMetricsFontCalculator>();
    }
    
    // If no specific calculator was created, use built-in metrics as final fallback
    if (!calculator) {
        calculator = std::make_unique<cTUIBuiltInMetricsFontCalculator>();
    }
    
    // If initialization failed, try fallbacks in order
    if (calculator && !calculator->Initialize()) {
        calculator.reset();
        
#ifdef _WIN32
        // If DirectWrite failed, try Windows GDI
        if (backend == FONT_BACKEND_DIRECTWRITE) {
            calculator = std::make_unique<cTUIFontCalculatorWindowsNative>();
            if (calculator && calculator->Initialize()) {
                return calculator;
            }
            calculator.reset();
        }
#endif
        
        // Try STB TrueType as fallback (should always be available)
        calculator = std::make_unique<cTUISTBTrueTypeFontCalculator>();
        if (!calculator->Initialize()) {
            calculator.reset();
            
            // Final fallback to built-in metrics (guaranteed to succeed)
            calculator = std::make_unique<cTUIBuiltInMetricsFontCalculator>();
            if (!calculator->Initialize()) {
                // This should never happen - built-in metrics must always work
                return nullptr;
            }
        }
    }
    
    // Calculator already initialized above -- just set document mode
    if (calculator) {
        calculator->SetDocumentMode(documentMode);
        return calculator;
    }

    return nullptr;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return sTUIFontInfo [out] current font information
///
/// @brief
/// Gets the currently set font information
/// Returns font family name, size, and style attributes
///
/////////////////////////////////////////////////////////////////////////////
sTUIFontInfo cTUIFontCalculator::GetCurrentFont(void) const 
{
    return mCurrentFont;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if in document mode
///
/// @brief
/// Gets the current measurement mode
/// Document mode provides higher precision, terminal mode uses simpler metrics
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculator::IsDocumentMode(void) const 
{
    return mDocumentMode;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return eTUIFontMeasurementBackend [out] current font measurement backend
///
/// @brief
/// Gets the font measurement backend being used by this calculator
/// Identifies which platform-specific backend is active
///
/////////////////////////////////////////////////////////////////////////////
eTUIFontMeasurementBackend cTUIFontCalculator::GetBackend(void) const 
{
    return mBackend;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return eTUIFontMeasurementMode [out] current measurement mode
///
/// @brief
/// Gets the current font measurement mode
/// Returns FONT_MODE_DOCUMENT or FONT_MODE_TERMINAL
///
/////////////////////////////////////////////////////////////////////////////
eTUIFontMeasurementMode cTUIFontCalculator::GetMode(void) const 
{
    return mMode;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool docMode [in] true for document mode, false for terminal mode
///
/// @return nothing
///
/// @brief
/// Sets the font measurement mode (document or terminal)
/// Document mode provides higher precision measurements
///
/////////////////////////////////////////////////////////////////////////////
void cTUIFontCalculator::SetDocumentMode(bool docMode) 
{ 
    mDocumentMode = docMode;
    mMode = docMode ? FONT_MODE_DOCUMENT : FONT_MODE_TERMINAL;
}