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
 * @class cTUIFontCalculatorMacOSNative
 * @brief macOS CoreText font measurement backend for native Apple font metrics.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIFontCalculatorMacOSNative, a cTUIFontCalculator subclass that
 * uses macOS CoreText for native font metrics and text measurement. Provides
 * glyph advance widths, line heights, and font enumeration on Apple platforms.
 * The entire file is conditionally compiled under __APPLE__.
 *
 * @see cTUIFontCalculatorMacOSNative
 * @see cTUIFontCalculator
 * @see sTUIFontInfo
 * @see sTUITextMetrics
 */

#include "platform_macos.h"

#ifdef __APPLE__

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for macOS CoreText native font calculator
/// Initializes CoreText backend with default document mode settings
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorMacOSNative::cTUIFontCalculatorMacOSNative(void) 
    : mCTFont(nullptr), mFontSize(12.0f), mAscent(0), mDescent(0), mLeading(0) {
    mBackend = FONT_BACKEND_MACOS_NATIVE;
    mMode = FONT_MODE_DOCUMENT;
    mDocumentMode = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for macOS CoreText font calculator
/// Ensures proper cleanup of CoreText font references
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorMacOSNative::~cTUIFontCalculatorMacOSNative(void) {
    Shutdown();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initializes the macOS CoreText font system
/// Sets up default Times font for text measurement operations
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorMacOSNative::Initialize(void) {
    // Set default font
    sTUIFontInfo defaultFont;
    defaultFont.name = "Times";
    defaultFont.size = 12;
    
    return SetFont(defaultFont);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shuts down the CoreText font calculator and releases resources
/// Properly releases CoreText font references to prevent memory leaks
///
/////////////////////////////////////////////////////////////////////////////
void cTUIFontCalculatorMacOSNative::Shutdown(void) {
    if (mCTFont) {
        CFRelease(mCTFont);
        mCTFont = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontInfo& font [in] font information to set
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Sets the current font using CoreText font creation
/// Creates a CoreText font reference for text measurement
///
/// @note Releases previous font reference before creating new one
/// @see CreateCTFont for CoreText font creation
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorMacOSNative::SetFont(const sTUIFontInfo& font) {
    mCurrentFont = font;
    
    if (mCTFont) {
        CFRelease(mCTFont);
        mCTFont = nullptr;
    }
    
    return CreateCTFont(font.name, static_cast<CGFloat>(font.size));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of available font family names
///
/// @brief
/// Gets a list of all available font families using CoreText enumeration
/// Uses CTFontCollection to query system fonts on macOS
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUIFontCalculatorMacOSNative::GetAvailableFonts(void) {
    std::vector<std::string> fonts;
    
    // Use CoreText to enumerate fonts
    CTFontCollectionRef collection = CTFontCollectionCreateFromAvailableFonts(nullptr);
    if (!collection) return fonts;
    
    CFArrayRef descriptors = CTFontCollectionCreateMatchingFontDescriptors(collection);
    if (!descriptors) {
        CFRelease(collection);
        return fonts;
    }
    
    CFIndex count = CFArrayGetCount(descriptors);
    
    for (CFIndex i = 0; i < count; i++) {
        CTFontDescriptorRef descriptor = 
            (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, i);
        CFStringRef name = (CFStringRef)CTFontDescriptorCopyAttribute(
            descriptor, kCTFontFamilyNameAttribute);
        
        if (name) {
            std::string fontName = CFStringToStdString(name);
            fonts.push_back(fontName);
            CFRelease(name);
        }
    }
    
    CFRelease(descriptors);
    CFRelease(collection);
    
    return fonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] text to measure
///
/// @return sTUITextMetrics [out] text measurement metrics in TWIPS
///
/// @brief
/// Measures text dimensions using CoreText typography
/// Creates attributed string and CTLine for accurate measurement
///
/// @note Converts measurements from points to TWIPS for consistency
///
/////////////////////////////////////////////////////////////////////////////
sTUITextMetrics cTUIFontCalculatorMacOSNative::MeasureText(const std::string& text) {
    sTUITextMetrics metrics;
    
    if (!mCTFont || text.empty()) {
        return metrics;
    }
    
    // Convert UTF-8 to CFString
    CFStringRef textRef = StdStringToCFString(text);
    if (!textRef) return metrics;
    
    // Create attributed string with font
    CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attributes, kCTFontAttributeName, mCTFont);
    
    CFAttributedStringRef attrString = CFAttributedStringCreate(
        kCFAllocatorDefault, textRef, attributes);
    
    // Create CTLine and get typographic bounds
    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    double width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    
    metrics.widthTWIPS = PointsToTWIPS(static_cast<CGFloat>(width));
    metrics.heightTWIPS = GetLineHeight();
    metrics.ascentTWIPS = PointsToTWIPS(mAscent);
    metrics.descentTWIPS = PointsToTWIPS(mDescent);
    metrics.leadingTWIPS = PointsToTWIPS(mLeading);
    
    // Cleanup
    CFRelease(line);
    CFRelease(attrString);
    CFRelease(attributes);
    CFRelease(textRef);
    
    return metrics;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to measure
///
/// @return int [out] character width in TWIPS
///
/// @brief
/// Measures the width of a single Unicode character using CoreText
/// Converts UTF-32 codepoint to CFString for measurement
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontCalculatorMacOSNative::MeasureCharacterWidth(char32_t codepoint) {
    if (!mCTFont) {
        return 120; // Default ~12pt char width in TWIPS
    }
    
    // Use CTFontCreateForString to get the best font for this character
    return MeasureCharacterWithFallback(codepoint);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return int [out] line height in TWIPS
///
/// @brief
/// Gets the line height for the current font
/// Combines ascent, descent, and leading measurements
///
/////////////////////////////////////////////////////////////////////////////
int cTUIFontCalculatorMacOSNative::GetLineHeight(void) {
    if (!mCTFont) {
        return 240; // Default ~12pt line height in TWIPS
    }
    
    CGFloat lineHeight = mAscent + mDescent + mLeading;
    return PointsToTWIPS(lineHeight);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to check
///
/// @return bool [out] true if character is supported
///
/// @brief
/// Checks if a Unicode character is supported by CoreText font system
/// Uses CoreText fallback mechanism to determine character support
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorMacOSNative::SupportsCharacter(char32_t codepoint) {
    if (!mCTFont) return false;
    
    // Use CTFontCreateForString to check if there's a font for this character
    CTFontRef fallbackFont = GetFallbackFontForCharacter(codepoint);
    if (fallbackFont) {
        CFRelease(fallbackFont);
        return true;
    }
    
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if current font is monospace
///
/// @brief
/// Determines if the current font is monospace using CoreText traits
/// Checks kCTFontMonoSpaceTrait symbolic trait
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorMacOSNative::IsMonospaceFont(void) const {
    if (!mCTFont) return false;
    
    CTFontSymbolicTraits traits = CTFontGetSymbolicTraits(mCTFont);
    return (traits & kCTFontMonoSpaceTrait) != 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string [out] font family name
///
/// @brief
/// Gets the family name of the current CoreText font
/// Returns "Unknown" if no font is set or name cannot be retrieved
///
/////////////////////////////////////////////////////////////////////////////
std::string cTUIFontCalculatorMacOSNative::GetFontFamilyName(void) const {
    if (!mCTFont) return "Unknown";
    
    CFStringRef familyName = CTFontCopyFamilyName(mCTFont);
    if (!familyName) return "Unknown";
    
    std::string name = CFStringToStdString(familyName);
    CFRelease(familyName);
    
    return name;
}

int cTUIFontCalculatorMacOSNative::CalculateTextWidthWithKerning(const std::string& text) {
    if (!mCTFont || text.empty()) return 0;
    
    CFStringRef textRef = StdStringToCFString(text);
    if (!textRef) return 0;
    
    // Create attributed string with kerning enabled
    CFMutableDictionaryRef attributes = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(attributes, kCTFontAttributeName, mCTFont);
    
    // Enable kerning
    CGFloat kernValue = 0.0f; // 0 = use font's default kerning
    CFNumberRef kerningValue = CFNumberCreate(kCFAllocatorDefault, 
                                             kCFNumberCGFloatType, &kernValue);
    CFDictionarySetValue(attributes, kCTKernAttributeName, kerningValue);
    
    CFAttributedStringRef attrString = CFAttributedStringCreate(
        kCFAllocatorDefault, textRef, attributes);
    
    // Create line and measure
    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    double width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    
    // Cleanup
    CFRelease(line);
    CFRelease(attrString);
    CFRelease(attributes);
    CFRelease(kerningValue);
    CFRelease(textRef);
    
    return PointsToTWIPS(static_cast<CGFloat>(width));
}

int cTUIFontCalculatorMacOSNative::PointsToTWIPS(CGFloat points) const {
    // Convert points to TWIPS (1/1440 inch)
    // macOS uses 72 DPI as standard, so 1 point = 20 TWIPS
    return static_cast<int>(points * 20.0);
}

std::string cTUIFontCalculatorMacOSNative::CFStringToStdString(CFStringRef cfString) const {
    if (!cfString) return "";
    
    CFIndex length = CFStringGetLength(cfString);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    
    char* buffer = new char[maxSize + 1];
    Boolean success = CFStringGetCString(cfString, buffer, maxSize + 1, 
                                        kCFStringEncodingUTF8);
    
    std::string result = success ? std::string(buffer) : "";
    delete[] buffer;
    
    return result;
}

CFStringRef cTUIFontCalculatorMacOSNative::StdStringToCFString(const std::string& str) const {
    return CFStringCreateWithCString(kCFAllocatorDefault, str.c_str(), 
                                    kCFStringEncodingUTF8);
}

bool cTUIFontCalculatorMacOSNative::CreateCTFont(const std::string& fontName, CGFloat pointSize) {
    mFontSize = pointSize;
    
    // Create CTFont - completely headless, no GUI required
    CFStringRef fontNameRef = StdStringToCFString(fontName);
    if (fontName.empty() || !fontNameRef) {
        mCTFont = CTFontCreateWithName(CFSTR("Times"), mFontSize, nullptr);
    } else {
        mCTFont = CTFontCreateWithName(fontNameRef, mFontSize, nullptr);
        CFRelease(fontNameRef);
    }
    
    if (!mCTFont) {
        // Fallback to system font
        mCTFont = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, mFontSize, nullptr);
    }
    
    if (mCTFont) {
        // Get font metrics
        mAscent = CTFontGetAscent(mCTFont);
        mDescent = CTFontGetDescent(mCTFont);
        mLeading = CTFontGetLeading(mCTFont);
        return true;
    }
    
    return false;
}

int cTUIFontCalculatorMacOSNative::MeasureCharacterWithFallback(char32_t codepoint) {
    if (!mCTFont) {
        return 120; // Default fallback
    }
    
    // Convert codepoint to UTF-16 for CoreText
    UniChar characters[2];
    CFIndex length = 1;
    
    if (codepoint <= 0xFFFF) {
        characters[0] = static_cast<UniChar>(codepoint);
    } else {
        // Convert to surrogate pair for characters > U+FFFF
        codepoint -= 0x10000;
        characters[0] = static_cast<UniChar>((codepoint >> 10) + 0xD800);
        characters[1] = static_cast<UniChar>((codepoint & 0x3FF) + 0xDC00);
        length = 2;
    }
    
    // Create string for this character
    CFStringRef string = CFStringCreateWithCharacters(kCFAllocatorDefault, characters, length);
    if (!string) return 120;
    
    // Use CTFontCreateForString to get the best font for this character
    CTFontRef fallbackFont = CTFontCreateForString(mCTFont, string, CFRangeMake(0, length));
    CFRelease(string);
    
    if (!fallbackFont) {
        return 120; // Fallback width
    }
    
    // Create attributed string with the fallback font
    CFStringRef charString = CFStringCreateWithCharacters(kCFAllocatorDefault, characters, length);
    if (!charString) {
        CFRelease(fallbackFont);
        return 120;
    }
    
    CFStringRef keys[] = { kCTFontAttributeName };
    CFTypeRef values[] = { fallbackFont };
    CFDictionaryRef attributes = CFDictionaryCreate(
        kCFAllocatorDefault, (const void**)&keys, (const void**)&values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    
    CFAttributedStringRef attrString = CFAttributedStringCreate(kCFAllocatorDefault, charString, attributes);
    CFRelease(charString);
    CFRelease(attributes);
    CFRelease(fallbackFont);
    
    if (!attrString) {
        return 120;
    }
    
    // Create line and get its width
    CTLineRef line = CTLineCreateWithAttributedString(attrString);
    CFRelease(attrString);
    
    if (!line) {
        return 120;
    }
    
    double width = CTLineGetTypographicBounds(line, nullptr, nullptr, nullptr);
    CFRelease(line);
    
    return PointsToTWIPS(static_cast<CGFloat>(width));
}

CTFontRef cTUIFontCalculatorMacOSNative::GetFallbackFontForCharacter(char32_t codepoint) const {
    if (!mCTFont) {
        return nullptr;
    }
    
    // Convert codepoint to UTF-16
    UniChar characters[2];
    CFIndex length = 1;
    
    if (codepoint <= 0xFFFF) {
        characters[0] = static_cast<UniChar>(codepoint);
    } else {
        // Convert to surrogate pair
        codepoint -= 0x10000;
        characters[0] = static_cast<UniChar>((codepoint >> 10) + 0xD800);
        characters[1] = static_cast<UniChar>((codepoint & 0x3FF) + 0xDC00);
        length = 2;
    }
    
    CFStringRef string = CFStringCreateWithCharacters(kCFAllocatorDefault, characters, length);
    if (!string) return nullptr;
    
    CTFontRef fallbackFont = CTFontCreateForString(mCTFont, string, CFRangeMake(0, length));
    CFRelease(string);
    
    return fallbackFont; // Caller must release
}

#endif // __APPLE__