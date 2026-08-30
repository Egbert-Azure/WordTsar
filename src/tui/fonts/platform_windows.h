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

#ifndef TUI_PLATFORM_WINDOWS_H
#define TUI_PLATFORM_WINDOWS_H

#ifdef _WIN32

#include "tuifontcalc.h"
#include <windows.h>
#include <vector>

// Windows Native GDI font calculator (headless)
class cTUIFontCalculatorWindowsNative : public cTUIFontCalculator
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cTUIFontCalculatorWindowsNative(void);
    virtual ~cTUIFontCalculatorWindowsNative(void);

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
    int PixelsToTWIPS(int pixels) const;
    bool CreateWindowsFont(const std::string& fontName, int pointSize);

    // Windows font enumeration callback
    static int CALLBACK EnumFontFamProc(const LOGFONTA* lf, const TEXTMETRICA* tm,
                                       DWORD fontType, LPARAM lParam);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    HDC mMemoryDC;              // Memory device context (no window required)
    HFONT mCurrentFont;         // Currently selected font
    TEXTMETRIC mTextMetric;     // Font metrics
    std::string mFontName;
    int mPointSize;
};

#endif // _WIN32

#endif // TUI_PLATFORM_WINDOWS_H