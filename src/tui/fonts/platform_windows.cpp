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
 * @class cTUIFontCalculatorWindowsNative
 * @brief Windows GDI font measurement backend as a fallback for older Windows systems.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIFontCalculatorWindowsNative, a cTUIFontCalculator subclass that
 * uses the Windows GDI API for font metrics and text measurement. Provides
 * glyph advance widths, line heights, and font enumeration as a fallback to
 * the DirectWrite backend on older Windows systems. The entire file is
 * conditionally compiled under _WIN32.
 *
 * @see cTUIFontCalculatorWindowsNative
 * @see cTUIFontCalculator
 * @see sTUIFontInfo
 * @see sTUITextMetrics
 */

#include "platform_windows.h"

#ifdef _WIN32

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for Windows GDI native font calculator
/// Initializes backend type and document mode settings
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorWindowsNative::cTUIFontCalculatorWindowsNative(void) 
    : mMemoryDC(nullptr), mCurrentFont(nullptr) {
    mBackend = FONT_BACKEND_WINDOWS_NATIVE;
    mMode = FONT_MODE_DOCUMENT;
    mDocumentMode = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for Windows GDI native font calculator
/// Ensures proper cleanup of GDI resources
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorWindowsNative::~cTUIFontCalculatorWindowsNative(void) {
    Shutdown();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initializes the Windows GDI font system
/// Creates a memory device context for headless font operations
///
/// @note Uses memory DC to avoid requiring a window handle
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorWindowsNative::Initialize(void) {
    // Create memory DC - no window required, completely headless
    mMemoryDC = CreateCompatibleDC(NULL);
    if (!mMemoryDC) {
        return false;
    }
    
    // Set default font
    sTUIFontInfo defaultFont;
    defaultFont.name = "Times New Roman";
    defaultFont.size = 12;
    
    return SetFont(defaultFont);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shuts down the Windows GDI font calculator and releases resources
/// Deletes font objects and device context to prevent GDI handle leaks
///
/////////////////////////////////////////////////////////////////////////////
void cTUIFontCalculatorWindowsNative::Shutdown(void) {
    if (mCurrentFont) {
        DeleteObject(mCurrentFont);
        mCurrentFont = nullptr;
    }
    if (mMemoryDC) {
        DeleteDC(mMemoryDC);
        mMemoryDC = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontInfo& font [in] font information to set
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Sets the current font using Windows GDI font creation
/// Creates a native Windows font handle for text measurement
///
/// @note Deletes previous font handle before creating new one
/// @see CreateWindowsFont for font handle creation
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorWindowsNative::SetFont(const sTUIFontInfo& font) {
    mCurrentFont = font;
    
    if (mCurrentFont) {
        DeleteObject(mCurrentFont);
        mCurrentFont = nullptr;
    }
    
    return CreateWindowsFont(font.name, font.size);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of available font family names
///
/// @brief
/// Gets a list of all available font families using Windows font enumeration
/// Uses EnumFontFamilies API to query system fonts
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUIFontCalculatorWindowsNative::GetAvailableFonts(void) {
    std::vector<std::string> fonts;
    
    if (mMemoryDC) {
        EnumFontFamiliesA(mMemoryDC, NULL, EnumFontFamProc, 
                         reinterpret_cast<LPARAM>(&fonts));
    }
    
    return fonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] text to measure
///
/// @return sTUITextMetrics [out] text measurement metrics in TWIPS
///
/// @brief
/// Measures text dimensions using Windows GDI text measurement
/// Uses GetTextExtentPoint32A for accurate text measurement
///
/// @note Converts pixel measurements to TWIPS for consistent units
///
/////////////////////////////////////////////////////////////////////////////
sTUITextMetrics cTUIFontCalculatorWindowsNative::MeasureText(const std::string& text) {
    sTUITextMetrics metrics;
    
    if (!mMemoryDC || !mCurrentFont) {
        return metrics;
    }
    
    SIZE textSize;
    if (GetTextExtentPoint32A(mMemoryDC, text.c_str(), 
                             static_cast<int>(text.length()), &textSize)) {
        metrics.widthTWIPS = PixelsToTWIPS(textSize.cx);
        metrics.heightTWIPS = PixelsToTWIPS(mTextMetric.tmHeight);
        metrics.ascentTWIPS = PixelsToTWIPS(mTextMetric.tmAscent);
        metrics.descentTWIPS = PixelsToTWIPS(mTextMetric.tmDescent);
        metrics.leadingTWIPS = PixelsToTWIPS(mTextMetric.tmExternalLeading);
    }
    
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
/// Converts UTF-32 to UTF-8 for Windows GDI measurement
///
/// @note Falls back to 'M' width for complex Unicode characters
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontCalculatorWindowsNative::MeasureCharacterWidth(char32_t codepoint) {
    if (!mMemoryDC || !mCurrentFont) {
        return 144; // Default fallback
    }
    
    // Convert to string and measure
    std::string ch;
    if (codepoint <= 0x7F) {
        ch = static_cast<char>(codepoint);
    } else {
        // Simple UTF-8 conversion for basic characters
        if (codepoint <= 0x7FF) {
            ch += static_cast<char>(0xC0 | (codepoint >> 6));
            ch += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            // Fallback to 'M' for complex characters
            ch = "M";
        }
    }
    
    SIZE size;
    if (GetTextExtentPoint32A(mMemoryDC, ch.c_str(), 
                             static_cast<int>(ch.length()), &size)) {
        return PixelsToTWIPS(size.cx);
    }
    
    return PixelsToTWIPS(mTextMetric.tmAveCharWidth);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return int [out] line height in TWIPS
///
/// @brief
/// Gets the line height for the current font
/// Uses Windows text metrics for accurate line height measurement
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontCalculatorWindowsNative::GetLineHeight(void) {
    if (!mMemoryDC || !mCurrentFont) {
        return 240; // Default fallback
    }
    
    return PixelsToTWIPS(mTextMetric.tmHeight);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to check
///
/// @return bool [out] true if character is supported
///
/// @brief
/// Checks if a Unicode character is supported by Windows GDI
/// Supports Basic Multilingual Plane characters (U+0000 to U+FFFF)
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorWindowsNative::SupportsCharacter(char32_t codepoint) {
    // Windows GDI supports most Unicode characters
    return codepoint <= 0xFFFF; // Basic Multilingual Plane
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int pixels [in] pixel value to convert
///
/// @return int [out] equivalent value in TWIPS
///
/// @brief
/// Converts pixels to TWIPS (1/1440 inch) units
/// Uses Windows default 96 DPI scaling factor
///
/// @note 1 pixel = 15 TWIPS at 96 DPI
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontCalculatorWindowsNative::PixelsToTWIPS(int pixels) const {
    // Convert pixels to TWIPS (1/1440 inch)
    // Windows uses 96 DPI by default: 1 pixel = 96/1440 = 1/15 TWIPS
    return pixels * 15;
}

bool cTUIFontCalculatorWindowsNative::CreateWindowsFont(const std::string& fontName, int pointSize) {
    if (!mMemoryDC) {
        return false;
    }
    
    mFontName = fontName;
    mPointSize = pointSize;
    
    // Create Windows font - completely headless
    mCurrentFont = CreateFontA(
        -MulDiv(pointSize, GetDeviceCaps(mMemoryDC, LOGPIXELSY), 72), // Height
        0,                          // Width (auto)
        0,                          // Escapement
        0,                          // Orientation
        FW_NORMAL,                  // Weight
        FALSE,                      // Italic
        FALSE,                      // Underline
        FALSE,                      // StrikeOut
        ANSI_CHARSET,               // CharSet
        OUT_TT_PRECIS,              // OutputPrecision
        CLIP_DEFAULT_PRECIS,        // ClipPrecision
        DEFAULT_QUALITY,            // Quality
        DEFAULT_PITCH | FF_DONTCARE,// PitchAndFamily
        fontName.c_str()            // Font name
    );
    
    if (!mCurrentFont) {
        return false;
    }
    
    // Select font and get metrics
    HGDIOBJ oldFont = SelectObject(mMemoryDC, mCurrentFont);
    GetTextMetricsA(mMemoryDC, &mTextMetric);
    
    return true;
}

int CALLBACK cTUIFontCalculatorWindowsNative::EnumFontFamProc(const LOGFONTA* lf, 
                                                             const TEXTMETRICA* tm,
                                                             DWORD fontType, 
                                                             LPARAM lParam) {
    auto* fontList = reinterpret_cast<std::vector<std::string>*>(lParam);
    fontList->push_back(lf->lfFaceName);
    return 1; // Continue enumeration
}

#endif // _WIN32