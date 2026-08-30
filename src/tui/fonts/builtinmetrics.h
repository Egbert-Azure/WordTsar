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

#ifndef TUI_BUILTINMETRICS_H
#define TUI_BUILTINMETRICS_H

#include "tuifontcalc.h"
#include <map>
#include <array>

// Built-in font metrics data structure
struct sTUIBuiltInFontMetrics {
    const char* name;
    int baseWidthTWIPS;     // Average character width in TWIPS at 12pt
    int heightTWIPS;        // Line height in TWIPS at 12pt
    int ascentTWIPS;        // Ascent in TWIPS at 12pt
    int descentTWIPS;       // Descent in TWIPS at 12pt
    int leadingTWIPS;       // Leading (line spacing) in TWIPS at 12pt
    bool monospace;         // Whether this is a monospace font
};

// Built-in metrics calculator - ultimate fallback
class cTUIBuiltInMetricsFontCalculator : public cTUIFontCalculator
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cTUIBuiltInMetricsFontCalculator(void);
    virtual ~cTUIBuiltInMetricsFontCalculator(void) = default;

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

    // Built-in specific methods
    static const sTUIBuiltInFontMetrics* FindFontMetrics(const std::string& fontName);
    static std::vector<std::string> GetBuiltInFontNames(void);

private:
    // Scale metrics based on font size
    int ScaleMetric(int baseMetric, int fontSize) const;

    // Get character width for proportional fonts
    int GetProportionalCharWidth(char32_t codepoint) const;

    // wcwidth() wrapper for terminal mode
    int GetTerminalCharWidth(char32_t codepoint) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    // Built-in font metrics table
    static const std::array<sTUIBuiltInFontMetrics, 8> mBuiltInFonts;

    const sTUIBuiltInFontMetrics* mCurrentFontMetrics;

    // Character width cache for proportional fonts
    std::map<char32_t, int> mCharWidthCache;
};

#endif // TUI_BUILTINMETRICS_H