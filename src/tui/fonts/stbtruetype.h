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

#ifndef TUI_STBTRUETYPE_H
#define TUI_STBTRUETYPE_H

#include "tuifontcalc.h"
#include <vector>
#include <map>
#include <string>
#include <memory>

// Forward declaration of STB structs
struct stbtt_fontinfo;

// Font file information
struct sTUIFontFile {
    std::string path;
    std::string family;
    bool bold;
    bool italic;
    std::vector<uint8_t> data;  // Font file data
};

// STB TrueType font calculator - high precision font measurement
class cTUISTBTrueTypeFontCalculator : public cTUIFontCalculator
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cTUISTBTrueTypeFontCalculator(void);
    virtual ~cTUISTBTrueTypeFontCalculator(void);

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

    // STB TrueType specific methods
    bool HasFontLoaded(void) const;
    std::string GetLoadedFontFamily(void) const;
    int GetFontUnitsPerEM(void) const;

    // Advanced metrics
    int GetKerning(char32_t left, char32_t right);
    bool GetGlyphMetrics(char32_t codepoint, int& advance, int& leftBearing);

private:
    // Font enumeration
    bool EnumerateSystemFonts(void);
    bool LoadFontFile(const std::string& fontPath);
    bool LoadFontFromMemory(const std::vector<uint8_t>& data);

    // Font matching
    sTUIFontFile* FindBestFontMatch(const sTUIFontInfo& requestedFont);
    int GetFontStyleScore(const sTUIFontFile& font, const sTUIFontInfo& requested);

    // Measurement helpers
    float PixelsToTWIPS(float pixels, float dpi = 96.0f) const;
    float TWIPSToPixels(float twips, float dpi = 96.0f) const;
    int ScaleMetric(int fontUnits) const;

    // Platform-specific font directory scanning
    void ScanFontDirectory(const std::string& directory);
    std::vector<std::string> GetSystemFontDirectories(void);

    // Font file parsing
    bool ParseFontFile(const std::string& path, std::vector<sTUIFontFile>& fonts);
    std::string ExtractFontFamily(const std::vector<uint8_t>& fontData);
    bool IsFontBold(const std::vector<uint8_t>& fontData);
    bool IsFontItalic(const std::vector<uint8_t>& fontData);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    std::unique_ptr<stbtt_fontinfo> mFontInfo;
    std::vector<uint8_t> mFontData;
    std::map<std::string, sTUIFontFile> mAvailableFonts;

    float mScale;               // Font scale factor
    int mAscent, mDescent;      // Font metrics in font units
    int mLineGap;               // Line gap in font units
};

#endif // TUI_STBTRUETYPE_H