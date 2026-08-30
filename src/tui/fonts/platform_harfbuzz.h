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

#ifndef TUI_PLATFORM_HARFBUZZ_H
#define TUI_PLATFORM_HARFBUZZ_H

#include "tuifontcalc.h"
#include <vector>

// Forward declarations to avoid HarfBuzz dependency in header
struct hb_font_t;
struct hb_buffer_t;
struct hb_face_t;
struct FT_FaceRec_;
typedef struct FT_FaceRec_* FT_Face;
typedef struct FT_LibraryRec_* FT_Library;

// HarfBuzz + FreeType font calculator - high precision text shaping
class cTUIFontCalculatorHarfBuzz : public cTUIFontCalculator {

    // =================================================================
    // METHODS
    // =================================================================

public:
    cTUIFontCalculatorHarfBuzz(void);
    virtual ~cTUIFontCalculatorHarfBuzz(void);

    // cTUIFontCalculator interface
    bool Initialize(void) override;
    void Shutdown(void) override;

    bool SetFont(const sTUIFontInfo& font) override;
    std::vector<std::string> GetAvailableFonts(void) override;

    sTUITextMetrics MeasureText(const std::string& text) override;
    int MeasureCharacterWidth(char32_t codepoint) override;
    int GetLineHeight(void) override;

    bool SupportsUnicode(void) const override;
    bool SupportsCharacter(char32_t codepoint) override;

    // HarfBuzz-specific methods
    sTUITextMetrics MeasureTextWithShaping(const std::string& text);
    bool LoadFontFromFile(const std::string& fontPath);
    bool LoadFontFromMemory(const std::vector<uint8_t>& fontData);

    // Advanced typography features
    std::vector<int> GetGlyphAdvances(const std::string& text);
    std::vector<int> GetGlyphPositions(const std::string& text);
    bool HasKerning(void) const;
    int GetKerning(char32_t left, char32_t right);

private:
    // Helper functions
    bool InitializeLibraries(void);
    void CleanupLibraries(void);
    bool CreateHarfBuzzFont(void);
    bool LoadSystemFont(const std::string& fontName);
    std::vector<std::string> ScanSystemFonts(void);

    // Text processing
    std::vector<char32_t> UTF8ToUTF32(const std::string& text);
    void ShapeText(const std::vector<char32_t>& codepoints,
                   std::vector<unsigned int>& glyphs,
                   std::vector<int>& advances,
                   std::vector<int>& offsets);

    // Unit conversion
    int FTUnitsToTWIPS(int ftUnits) const;
    int HBUnitsToTWIPS(int hbUnits) const;

    // Font discovery
    std::string FindBestFontMatch(const std::string& requestedFont);
    bool IsFontFile(const std::string& path) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    // HarfBuzz objects
    hb_font_t* mHBFont;
    hb_buffer_t* mHBBuffer;
    hb_face_t* mHBFace;

    // FreeType objects
    FT_Library mFTLibrary;
    FT_Face mFTFace;

    // Font data
    std::vector<uint8_t> mFontData;
    std::string mFontPath;
    float mFontScale;
    int mFontSize;

    // Font metrics cache
    int mAscent, mDescent, mLineGap;
};

#endif // TUI_PLATFORM_HARFBUZZ_H
