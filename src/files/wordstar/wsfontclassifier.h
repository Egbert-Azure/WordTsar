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

#ifndef WSFONTCLASSIFIER_H
#define WSFONTCLASSIFIER_H

#include <string>
#include <vector>
#include <cstdint>


// WordStar generic style values (bits 11-10)
enum eWSGenericStyle
{
    WS_STYLE_SANS    = 0,    // 00
    WS_STYLE_SERIF   = 1,    // 01
    WS_STYLE_SCRIPT  = 2,    // 10
    WS_STYLE_DISPLAY = 3     // 11
};

// WordStar symbol mapping values (bits 13-12)
enum eWSSymbolMapping
{
    WS_SYMBOL_CP437   = 0,   // 00
    WS_SYMBOL_CP850   = 1,   // 01
    WS_SYMBOL_MATH    = 2,   // 10
    WS_SYMBOL_SYMBOLS = 3    // 11
};

// Classification result with diagnostic info
struct sWSFontClassification
{
    // Final bitfield values
    bool proportional;             // bit 15
    eWSSymbolMapping symbolMapping; // bits 13-12
    eWSGenericStyle genericStyle;   // bits 11-10
    uint16_t fontIndex;             // bits 8-0

    // Diagnostic: which source decided each field
    std::string pitchSource;       // "metrics", "keyword", "default"
    std::string styleSource;       // "panose", "familyclass", "keyword", "default"
    std::string symbolSource;      // "cmap", "keyword", "default"

    // Assemble into the full 16-bit typestyle value
    uint16_t Assemble(void) const;
};


class cWSFontClassifier
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    // Classify a font by name. Resolves to file, reads OS/2 table,
    // measures glyph widths, and produces a complete typestyle bitfield.
    sWSFontClassification Classify(const std::string& fontName);

    // Classify from raw font file bytes (for testing or when file is already loaded)
    sWSFontClassification ClassifyFromData(
        const unsigned char* fontData, size_t dataSize,
        const std::string& fontName);

    // Layer 1: Font file resolution (platform-specific, public for testing)
    static std::string FindFontFile(const std::string& fontName);

private:

    // Layer 2: Font container loader
    bool LoadFont(const unsigned char* data, size_t dataSize);

    // Layer 3: Raw SFNT table reader
    uint32_t FindTable(const char* tag) const;
    uint16_t ReadU16BE(uint32_t offset) const;
    uint8_t ReadU8(uint32_t offset) const;

    // Layer 4a: Pitch classifier (bit 15)
    bool ClassifyPitch(std::string& source);

    // Layer 4b: Style classifier (bits 11-10)
    eWSGenericStyle ClassifyStyle(const std::string& fontName, std::string& source);
    eWSGenericStyle ClassifyStyleFromPANOSE(void);
    eWSGenericStyle ClassifyStyleFromFamilyClass(void);
    eWSGenericStyle ClassifyStyleFromName(const std::string& fontName);

    // Layer 4c: Symbol classifier (bits 13-12)
    eWSSymbolMapping ClassifySymbol(const std::string& fontName, std::string& source);

    // Layer 5: Font index lookup
    uint16_t LookupFontIndex(const std::string& fontName,
                              eWSGenericStyle style, bool proportional);

    // Keyword fallback (delegates to existing cFontClassifier)
    sWSFontClassification ClassifyByKeywords(const std::string& fontName);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    const unsigned char* mData;    // raw font file bytes (not owned)
    size_t mDataSize;              // size of font data
    uint32_t mFontStart;           // offset to start of font in data (for TTC)
    bool mFontLoaded;              // true if stb_truetype init succeeded
    uint32_t mOS2Offset;           // offset to OS/2 table (0 = not found)
    uint32_t mOS2Length;           // length of OS/2 table

    // stb_truetype font info (stored as opaque bytes to avoid header dependency)
    // Actual stbtt_fontinfo is allocated in the .cpp file
    void* mFontInfo;
};

#endif // WSFONTCLASSIFIER_H
