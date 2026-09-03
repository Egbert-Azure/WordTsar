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

#ifndef TUI_PLATFORM_MACOS_H
#define TUI_PLATFORM_MACOS_H

#ifdef __APPLE__

#include "tuifontcalc.h"
#include <CoreText/CoreText.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreFoundation/CoreFoundation.h>
#include <vector>

// macOS Native CoreText font calculator (headless)
class cTUIFontCalculatorMacOSNative : public cTUIFontCalculator
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cTUIFontCalculatorMacOSNative(void);
    virtual ~cTUIFontCalculatorMacOSNative(void);

    // cTUIFontCalculator interface
    bool Initialize(void) override;
    void Shutdown(void) override;

    bool SetFont(const sTUIFontInfo& font) override;
    std::vector<std::string> GetAvailableFonts(void) override;

    sTUITextMetrics MeasureText(const std::string& text) override;
    int MeasureCharacterWidth(char32_t codepoint) override;
    int GetLineHeight(void) override;

    bool SupportsUnicode() const override { return true; }
    bool SupportsCharacter(char32_t codepoint) override;

    // macOS-specific methods
    bool IsMonospaceFont(void) const;
    std::string GetFontFamilyName(void) const;
    int CalculateTextWidthWithKerning(const std::string& text);

private:
    // Helper functions
    int PointsToTWIPS(CGFloat points) const;
    std::string CFStringToStdString(CFStringRef cfString) const;
    CFStringRef StdStringToCFString(const std::string& str) const;
    bool CreateCTFont(const std::string& fontName, CGFloat pointSize);

    // Font fallback with CoreText
    int MeasureCharacterWithFallback(char32_t codepoint);
    CTFontRef GetFallbackFontForCharacter(char32_t codepoint) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    CTFontRef mCTFont;
    CGFloat mFontSize;
    CGFloat mAscent, mDescent, mLeading;
};

#endif // __APPLE__

#endif // TUI_PLATFORM_MACOS_H