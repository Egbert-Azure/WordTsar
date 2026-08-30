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

#ifndef TUI_PLATFORM_DIRECTWRITE_H
#define TUI_PLATFORM_DIRECTWRITE_H

#ifdef _WIN32

#include "tuifontcalc.h"
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl.h>
#include <vector>

// Windows DirectWrite font calculator with native font fallback
class cTUIFontCalculatorDirectWrite : public cTUIFontCalculator
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cTUIFontCalculatorDirectWrite(void);
    virtual ~cTUIFontCalculatorDirectWrite(void);

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

private:
    // Helper functions
    float PointsToTWIPS(float points) const;
    float TWIPSToPoints(float twips) const;
    std::wstring UTF8ToWString(const std::string& utf8);
    bool CreateTextFormat(const std::string& fontFamily, float fontSize,
                         DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL,
                         DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL);

    // DirectWrite font measurement with fallback
    sTUITextMetrics MeasureTextWithFallback(const std::wstring& text);
    int MeasureCharacterWithFallback(char32_t codepoint);

    // Font style conversion helpers
    DWRITE_FONT_WEIGHT FontWeightFromInfo(const sTUIFontInfo& font) const;
    DWRITE_FONT_STYLE FontStyleFromInfo(const sTUIFontInfo& font) const;

    // Font enumeration helpers
    void EnumerateSystemFonts(std::vector<std::string>& fonts);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    Microsoft::WRL::ComPtr<IDWriteFactory> mWriteFactory;
    Microsoft::WRL::ComPtr<IDWriteFontFallback> mFontFallback;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> mTextFormat;
    Microsoft::WRL::ComPtr<IDWriteFontCollection> mFontCollection;
    Microsoft::WRL::ComPtr<IDWriteRenderingParams> mRenderingParams;

    std::string mCurrentFontFamily;
    float mCurrentFontSize;
    DWRITE_FONT_WEIGHT mCurrentWeight;
    DWRITE_FONT_STYLE mCurrentStyle;
};

#endif // _WIN32

#endif // TUI_PLATFORM_DIRECTWRITE_H