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
 * @class cTUIFontCalculatorDirectWrite
 * @brief Windows DirectWrite font measurement backend for high-quality text metrics.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIFontCalculatorDirectWrite, a cTUIFontCalculator subclass that
 * uses the Windows DirectWrite API for high-quality font metrics and text
 * measurement. Provides glyph advance widths, line heights, and font
 * enumeration on Windows platforms. The entire file is conditionally compiled
 * under _WIN32.
 *
 * @see cTUIFontCalculatorDirectWrite
 * @see cTUIFontCalculator
 * @see sTUIFontInfo
 * @see sTUITextMetrics
 */

#include "platform_directwrite.h"

#ifdef _WIN32

#include <codecvt>
#include <locale>

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for DirectWrite font calculator
/// Initializes DirectWrite backend with default document mode settings
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorDirectWrite::cTUIFontCalculatorDirectWrite(void) 
    : mCurrentFontSize(12.0f), mCurrentWeight(DWRITE_FONT_WEIGHT_NORMAL), 
      mCurrentStyle(DWRITE_FONT_STYLE_NORMAL) {
    mBackend = FONT_BACKEND_DIRECTWRITE;
    mMode = FONT_MODE_DOCUMENT;
    mDocumentMode = true;
    mCurrentFontFamily = "Times New Roman";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for DirectWrite font calculator
/// Ensures proper cleanup of DirectWrite COM objects
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorDirectWrite::~cTUIFontCalculatorDirectWrite(void) {
    Shutdown();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initializes the DirectWrite font measurement system
/// Creates DirectWrite factory, font collection, and fallback objects
///
/// @note Requires Windows Vista+ with DirectWrite support
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorDirectWrite::Initialize(void) {
    HRESULT hr;
    
    // Initialize DirectWrite factory
    hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(mWriteFactory.GetAddressOf())
    );
    if (FAILED(hr)) {
        return false;
    }
    
    // Get system font collection
    hr = mWriteFactory->GetSystemFontCollection(mFontCollection.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }
    
    // Create font fallback (Windows 8.1+)
    Microsoft::WRL::ComPtr<IDWriteFactory2> factory2;
    hr = mWriteFactory.As(&factory2);
    if (SUCCEEDED(hr)) {
        hr = factory2->GetSystemFontFallback(mFontFallback.GetAddressOf());
        // Font fallback is optional - continue even if it fails
    }
    
    // Create default rendering parameters
    hr = mWriteFactory->CreateRenderingParams(mRenderingParams.GetAddressOf());
    if (FAILED(hr)) {
        return false;
    }
    
    // Set default font
    sTUIFontInfo defaultFont;
    defaultFont.name = "Times New Roman";
    defaultFont.size = 12;
    
    return SetFont(defaultFont);
}

void cTUIFontCalculatorDirectWrite::Shutdown(void) {
    mRenderingParams.Reset();
    mTextFormat.Reset();
    mFontFallback.Reset();
    mFontCollection.Reset();
    mWriteFactory.Reset();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontInfo& font [in] font information to set
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Sets the current font using DirectWrite text format creation
/// Creates DirectWrite text format with specified font properties
///
/// @note Uses helper functions to map font weight and style
/// @see CreateTextFormat for DirectWrite format creation
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorDirectWrite::SetFont(const sTUIFontInfo& font) {
    mCurrentFont = font;
    mCurrentFontFamily = font.name;
    mCurrentFontSize = static_cast<float>(font.size);
    mCurrentWeight = FontWeightFromInfo(font);
    mCurrentStyle = FontStyleFromInfo(font);
    
    return CreateTextFormat(font.name, static_cast<float>(font.size), 
                           mCurrentWeight, mCurrentStyle);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of available font family names
///
/// @brief
/// Gets a list of all available font families using DirectWrite enumeration
/// Uses DirectWrite font collection to query system fonts
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUIFontCalculatorDirectWrite::GetAvailableFonts(void) {
    std::vector<std::string> fonts;
    
    if (mFontCollection) {
        EnumerateSystemFonts(fonts);
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
/// Measures text dimensions using DirectWrite text layout
/// Converts UTF-8 to wide string and uses fallback for missing characters
///
/////////////////////////////////////////////////////////////////////////////
sTUITextMetrics cTUIFontCalculatorDirectWrite::MeasureText(const std::string& text) {
    std::wstring wtext = UTF8ToWString(text);
    return MeasureTextWithFallback(wtext);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to measure
///
/// @return int [out] character width in TWIPS
///
/// @brief
/// Measures the width of a single Unicode character using DirectWrite
/// Uses DirectWrite font fallback system for unsupported characters
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontCalculatorDirectWrite::MeasureCharacterWidth(char32_t codepoint) {
    return MeasureCharacterWithFallback(codepoint);
}

int cTUIFontCalculatorDirectWrite::GetLineHeight(void) {
    sTUITextMetrics metrics;
    
    if (mTextFormat) {
        DWRITE_FONT_METRICS fontMetrics;
        Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
        Microsoft::WRL::ComPtr<IDWriteFont> font;
        
        HRESULT hr = mFontCollection->GetFontFamily(0, fontFamily.GetAddressOf());
        if (SUCCEEDED(hr)) {
            hr = fontFamily->GetFirstMatchingFont(mCurrentWeight, DWRITE_FONT_STRETCH_NORMAL, 
                                                 mCurrentStyle, font.GetAddressOf());
            if (SUCCEEDED(hr)) {
                font->GetMetrics(&fontMetrics);
                float lineHeight = (fontMetrics.ascent + fontMetrics.descent + fontMetrics.lineGap) 
                                 * mCurrentFontSize / fontMetrics.designUnitsPerEm;
                return static_cast<int>(PointsToTWIPS(lineHeight));
            }
        }
    }
    
    return 288; // Default fallback (24pt in TWIPS)
}

bool cTUIFontCalculatorDirectWrite::SupportsCharacter(char32_t codepoint) {
    if (!mFontFallback || !mTextFormat) {
        // Without fallback, assume basic Unicode support
        return codepoint <= 0x10FFFF;
    }
    
    // Use DirectWrite's fallback system to check character support
    std::wstring text;
    if (codepoint <= 0xFFFF) {
        text = static_cast<wchar_t>(codepoint);
    } else {
        // Convert to UTF-16 surrogate pair
        codepoint -= 0x10000;
        text += static_cast<wchar_t>((codepoint >> 10) + 0xD800);
        text += static_cast<wchar_t>((codepoint & 0x3FF) + 0xDC00);
    }
    
    Microsoft::WRL::ComPtr<IDWriteFont> fallbackFont;
    UINT32 mappedLength;
    float scale;
    
    HRESULT hr = mFontFallback->MapCharacters(
        nullptr, // No analyzer
        text.c_str(),
        static_cast<UINT32>(text.length()),
        mFontCollection.Get(),
        mCurrentFontFamily.c_str(),
        mCurrentWeight,
        mCurrentStyle,
        DWRITE_FONT_STRETCH_NORMAL,
        &mappedLength,
        fallbackFont.GetAddressOf(),
        &scale
    );
    
    return SUCCEEDED(hr) && fallbackFont != nullptr;
}

float cTUIFontCalculatorDirectWrite::PointsToTWIPS(float points) const {
    return points * 20.0f; // 1 point = 20 TWIPS
}

float cTUIFontCalculatorDirectWrite::TWIPSToPoints(float twips) const {
    return twips / 20.0f;
}

std::wstring cTUIFontCalculatorDirectWrite::UTF8ToWString(const std::string& utf8) {
    if (utf8.empty()) return std::wstring();
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), NULL, 0);
    std::wstring result(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &utf8[0], (int)utf8.size(), &result[0], size_needed);
    return result;
}

bool cTUIFontCalculatorDirectWrite::CreateTextFormat(const std::string& fontFamily, float fontSize,
                                                    DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style) {
    if (!mWriteFactory) return false;
    
    std::wstring wfontFamily = UTF8ToWString(fontFamily);
    
    HRESULT hr = mWriteFactory->CreateTextFormat(
        wfontFamily.c_str(),
        mFontCollection.Get(),
        weight,
        style,
        DWRITE_FONT_STRETCH_NORMAL,
        fontSize,
        L"en-US",
        mTextFormat.GetAddressOf()
    );
    
    return SUCCEEDED(hr);
}

sTUITextMetrics cTUIFontCalculatorDirectWrite::MeasureTextWithFallback(const std::wstring& text) {
    sTUITextMetrics metrics;
    
    if (!mWriteFactory || !mTextFormat || text.empty()) {
        return metrics;
    }
    
    Microsoft::WRL::ComPtr<IDWriteTextLayout> textLayout;
    HRESULT hr = mWriteFactory->CreateTextLayout(
        text.c_str(),
        static_cast<UINT32>(text.length()),
        mTextFormat.Get(),
        1000.0f, // Max width
        1000.0f, // Max height
        textLayout.GetAddressOf()
    );
    
    if (SUCCEEDED(hr)) {
        DWRITE_TEXT_METRICS layoutMetrics;
        hr = textLayout->GetMetrics(&layoutMetrics);
        
        if (SUCCEEDED(hr)) {
            metrics.widthTWIPS = static_cast<int>(PointsToTWIPS(layoutMetrics.width));
            metrics.heightTWIPS = static_cast<int>(PointsToTWIPS(layoutMetrics.height));
            
            // Get font metrics for ascent/descent
            DWRITE_LINE_METRICS lineMetrics;
            UINT32 actualLineCount;
            hr = textLayout->GetLineMetrics(&lineMetrics, 1, &actualLineCount);
            
            if (SUCCEEDED(hr) && actualLineCount > 0) {
                metrics.ascentTWIPS = static_cast<int>(PointsToTWIPS(lineMetrics.baseline));
                metrics.descentTWIPS = static_cast<int>(PointsToTWIPS(lineMetrics.height - lineMetrics.baseline));
            }
        }
    }
    
    return metrics;
}

int cTUIFontCalculatorDirectWrite::MeasureCharacterWithFallback(char32_t codepoint) {
    std::wstring text;
    if (codepoint <= 0xFFFF) {
        text = static_cast<wchar_t>(codepoint);
    } else {
        // Convert to UTF-16 surrogate pair
        codepoint -= 0x10000;
        text += static_cast<wchar_t>((codepoint >> 10) + 0xD800);
        text += static_cast<wchar_t>((codepoint & 0x3FF) + 0xDC00);
    }
    
    sTUITextMetrics metrics = MeasureTextWithFallback(text);
    return metrics.widthTWIPS;
}

DWRITE_FONT_WEIGHT cTUIFontCalculatorDirectWrite::FontWeightFromInfo(const sTUIFontInfo& font) const {
    if (font.bold) {
        return DWRITE_FONT_WEIGHT_BOLD;
    }
    return DWRITE_FONT_WEIGHT_NORMAL;
}

DWRITE_FONT_STYLE cTUIFontCalculatorDirectWrite::FontStyleFromInfo(const sTUIFontInfo& font) const {
    if (font.italic) {
        return DWRITE_FONT_STYLE_ITALIC;
    }
    return DWRITE_FONT_STYLE_NORMAL;
}

void cTUIFontCalculatorDirectWrite::EnumerateSystemFonts(std::vector<std::string>& fonts) {
    if (!mFontCollection) return;
    
    UINT32 familyCount = mFontCollection->GetFontFamilyCount();
    
    for (UINT32 i = 0; i < familyCount; i++) {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
        HRESULT hr = mFontCollection->GetFontFamily(i, fontFamily.GetAddressOf());
        
        if (SUCCEEDED(hr)) {
            Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> familyNames;
            hr = fontFamily->GetFamilyNames(familyNames.GetAddressOf());
            
            if (SUCCEEDED(hr)) {
                UINT32 index = 0;
                BOOL exists = FALSE;
                hr = familyNames->FindLocaleName(L"en-US", &index, &exists);
                
                if (!exists) {
                    // Fallback to first available name
                    index = 0;
                }
                
                UINT32 length = 0;
                hr = familyNames->GetStringLength(index, &length);
                
                if (SUCCEEDED(hr)) {
                    std::wstring familyName(length + 1, L'\0');
                    hr = familyNames->GetString(index, &familyName[0], length + 1);
                    
                    if (SUCCEEDED(hr)) {
                        // Convert to UTF-8
                        int size_needed = WideCharToMultiByte(CP_UTF8, 0, familyName.c_str(), -1, NULL, 0, NULL, NULL);
                        std::string utf8Name(size_needed - 1, 0);
                        WideCharToMultiByte(CP_UTF8, 0, familyName.c_str(), -1, &utf8Name[0], size_needed, NULL, NULL);
                        
                        fonts.push_back(utf8Name);
                    }
                }
            }
        }
    }
}

#endif // _WIN32