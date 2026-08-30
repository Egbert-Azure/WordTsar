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

#include "wsfontclassifier.h"
#include "fontclassifier.h"
#include "wordstarfile.h"

// Include stb_truetype implementation. The TUI's copy in stbtruetype.cpp
// uses STBTT_STATIC (file-local), so there is no symbol conflict.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_TRUETYPE_IMPLEMENTATION
#include "third-party/stb/stb_truetype.h"
#pragma GCC diagnostic pop

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <vector>

#if defined(__linux__) && defined(HAVE_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

#ifdef __APPLE__
#include <dirent.h>
#include <sys/stat.h>
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// @class cWSFontClassifier
///
/// @brief
/// Full font classifier for WordStar typestyle bitfields. Uses
/// stb_truetype to read OS/2, PANOSE, and cmap data from font files.
/// Falls back to keyword-based classification when font file is not
/// available. See DEV_DOCUMENTS/Design/ws-font-classifier.md for the
/// complete design.
///
/////////////////////////////////////////////////////////////////////////////


// Access the global font table from wordstarfile.cpp
extern std::vector<sOrgFont> gOrgFonts;


// =========================================================================
// Layer 5: Bitfield Assembler
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @return uint16_t - the assembled WordStar typestyle bitfield
///
/// @brief
/// Combines all classified properties into a single 16-bit value.
/// Bit layout: 15=proportional, 14=letter quality (1), 13-12=symbol,
/// 11-10=style, 9=version (1), 8-0=font index.
///
/////////////////////////////////////////////////////////////////////////////
uint16_t sWSFontClassification::Assemble(void) const
{
    uint16_t bits = 0;

    // Bit 15: proportional
    if (proportional)
    {
        bits |= (1 << 15);
    }

    // Bit 14: letter quality (always 1 for modern outline fonts)
    bits |= (1 << 14);

    // Bits 13-12: symbol mapping
    bits |= (static_cast<uint16_t>(symbolMapping) & 0x03) << 12;

    // Bits 11-10: generic style
    bits |= (static_cast<uint16_t>(genericStyle) & 0x03) << 10;

    // Bit 9: version flag (always 1 for new format)
    bits |= (1 << 9);

    // Bits 8-0: font index
    bits |= (fontIndex & 0x01FF);

    return bits;
}


// =========================================================================
// Layer 1: Font File Resolution
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] font family name to resolve
///
/// @return std::string - absolute path to font file, or empty if not found
///
/// @brief
/// Platform-specific font file resolution. Uses fontconfig on Linux,
/// system font directory search on Windows and macOS.
///
/////////////////////////////////////////////////////////////////////////////
std::string cWSFontClassifier::FindFontFile(const std::string& fontName)
{
    if (fontName.empty())
    {
        return "";
    }

#if defined(__linux__) && defined(HAVE_FONTCONFIG)
    // Use fontconfig to resolve font name to file path
    FcConfig* config = FcInitLoadConfigAndFonts();
    if (config == nullptr)
    {
        return "";
    }

    FcPattern* pattern = FcNameParse(
        reinterpret_cast<const FcChar8*>(fontName.c_str()));
    if (pattern == nullptr)
    {
        FcConfigDestroy(config);
        return "";
    }

    FcConfigSubstitute(config, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result;
    FcPattern* match = FcFontMatch(config, pattern, &result);

    std::string filePath;
    if (match != nullptr)
    {
        FcChar8* file = nullptr;
        if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch)
        {
            filePath = reinterpret_cast<const char*>(file);
        }
        FcPatternDestroy(match);
    }

    FcPatternDestroy(pattern);
    FcConfigDestroy(config);
    return filePath;

#elif defined(_WIN32)
    // Search Windows font directory
    char winDir[MAX_PATH];
    if (GetWindowsDirectoryA(winDir, MAX_PATH) == 0)
    {
        return "";
    }

    std::string fontDir = std::string(winDir) + "\\Fonts\\";
    // Try common extensions with lowercase font name
    std::string lowerName = fontName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Remove spaces for filename matching
    std::string noSpaces;
    for (char c : lowerName)
    {
        if (c != ' ')
        {
            noSpaces += c;
        }
    }

    // Try common file name patterns
    std::vector<std::string> candidates = {
        fontDir + noSpaces + ".ttf",
        fontDir + noSpaces + ".otf",
        fontDir + lowerName + ".ttf",
        fontDir + lowerName + ".otf",
    };

    for (const auto& path : candidates)
    {
        FILE* f = fopen(path.c_str(), "rb");
        if (f != nullptr)
        {
            fclose(f);
            return path;
        }
    }
    return "";

#elif defined(__APPLE__)
    // Search macOS font directories
    std::vector<std::string> fontDirs = {
        "/System/Library/Fonts/",
        "/Library/Fonts/",
    };

    // Build lowercase name for matching
    std::string lowerName = fontName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
        [](unsigned char c) { return std::tolower(c); });

    for (const auto& dir : fontDirs)
    {
        DIR* d = opendir(dir.c_str());
        if (d == nullptr)
        {
            continue;
        }
        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr)
        {
            std::string fname = entry->d_name;
            std::string lowerFname = fname;
            std::transform(lowerFname.begin(), lowerFname.end(),
                lowerFname.begin(),
                [](unsigned char c) { return std::tolower(c); });

            if (lowerFname.find(lowerName) != std::string::npos)
            {
                if (lowerFname.find(".ttf") != std::string::npos ||
                    lowerFname.find(".otf") != std::string::npos ||
                    lowerFname.find(".ttc") != std::string::npos)
                {
                    closedir(d);
                    return dir + fname;
                }
            }
        }
        closedir(d);
    }
    return "";

#else
    return "";
#endif
}


// =========================================================================
// Layer 2: Font Container Loader
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const unsigned char* data [in] raw font file bytes
/// @param  size_t dataSize [in] size of font data
///
/// @return bool - true if font was loaded successfully
///
/// @brief
/// Initialize stb_truetype with the font data. Handles TTF, OTF, and
/// TTC (collection) formats.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSFontClassifier::LoadFont(const unsigned char* data, size_t dataSize)
{
    mData = data;
    mDataSize = dataSize;
    mFontLoaded = false;
    mOS2Offset = 0;
    mOS2Length = 0;

    if (data == nullptr || dataSize < 64)
    {
        // Font files are at minimum several KB; reject anything too small
        return false;
    }

    // Validate SFNT magic number before passing to stb_truetype.
    // Valid signatures: 00010000 (TrueType), 4F54544F (OTTO/OpenType),
    // 74727565 (true), 74797031 (typ1), 74746366 (ttcf collection)
    uint32_t sig = (static_cast<uint32_t>(data[0]) << 24) |
                   (static_cast<uint32_t>(data[1]) << 16) |
                   (static_cast<uint32_t>(data[2]) << 8) |
                   static_cast<uint32_t>(data[3]);

    bool validSig = (sig == 0x00010000 ||  // TrueType
                     sig == 0x4F54544F ||  // OTTO (OpenType CFF)
                     sig == 0x74727565 ||  // true
                     sig == 0x74797031 ||  // typ1
                     sig == 0x74746366);   // ttcf (collection)

    if (!validSig)
    {
        return false;
    }

    // Reject files that are too small to contain a real font
    // (minimum: header + at least one table directory entry + table data)
    if (dataSize < 256)
    {
        return false;
    }

    // Allocate stbtt_fontinfo
    stbtt_fontinfo* info = new stbtt_fontinfo();
    mFontInfo = info;

    // Detect TTC and get font offset
    int numFonts = stbtt_GetNumberOfFonts(data);
    if (numFonts > 1)
    {
        // TTC collection: use first face
        mFontStart = static_cast<uint32_t>(
            stbtt_GetFontOffsetForIndex(data, 0));
    }
    else
    {
        mFontStart = 0;
    }

    // Initialize stb_truetype
    if (!stbtt_InitFont(info, data, static_cast<int>(mFontStart)))
    {
        delete info;
        mFontInfo = nullptr;
        return false;
    }

    // Validate that stb produced sane offsets (protects against garbage data).
    // Check required tables: head, hhea, hmtx, and numGlyphs.
    if (info->numGlyphs <= 0 ||
        static_cast<uint32_t>(info->head) >= dataSize ||
        static_cast<uint32_t>(info->hhea) >= dataSize ||
        static_cast<uint32_t>(info->hmtx) >= dataSize ||
        info->loca <= 0)
    {
        delete info;
        mFontInfo = nullptr;
        return false;
    }

    // Find OS/2 table
    mOS2Offset = FindTable("OS/2");
    // Validate OS/2 offset is within data bounds
    if (mOS2Offset > 0 && mOS2Offset + 42 > mDataSize)
    {
        mOS2Offset = 0;  // invalid, treat as missing
    }

    mFontLoaded = true;
    return true;
}


// =========================================================================
// Layer 3: Raw SFNT Table Reader
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const char* tag [in] 4-character table tag (e.g. "OS/2")
///
/// @return uint32_t - offset to table data, or 0 if not found
///
/// @brief
/// Searches the SFNT table directory for the given tag. Parses the
/// table directory header and entries manually since stbtt__find_table
/// is internal to stb_truetype.
///
/////////////////////////////////////////////////////////////////////////////
uint32_t cWSFontClassifier::FindTable(const char* tag) const
{
    if (mData == nullptr || mDataSize < mFontStart + 12)
    {
        return 0;
    }

    const unsigned char* data = mData + mFontStart;

    // Read number of tables from the SFNT header (offset 4, uint16 BE)
    uint16_t numTables = static_cast<uint16_t>((data[4] << 8) | data[5]);

    // Table directory starts at offset 12
    for (uint16_t i = 0; i < numTables; i++)
    {
        uint32_t entryOffset = 12 + i * 16;
        if (mFontStart + entryOffset + 16 > mDataSize)
        {
            break;
        }

        // Compare 4-byte tag
        if (data[entryOffset + 0] == static_cast<unsigned char>(tag[0]) &&
            data[entryOffset + 1] == static_cast<unsigned char>(tag[1]) &&
            data[entryOffset + 2] == static_cast<unsigned char>(tag[2]) &&
            data[entryOffset + 3] == static_cast<unsigned char>(tag[3]))
        {
            // Table offset is at entry + 8 (uint32 BE)
            uint32_t tableOffset =
                (static_cast<uint32_t>(data[entryOffset + 8]) << 24) |
                (static_cast<uint32_t>(data[entryOffset + 9]) << 16) |
                (static_cast<uint32_t>(data[entryOffset + 10]) << 8) |
                static_cast<uint32_t>(data[entryOffset + 11]);
            return tableOffset;
        }
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  uint32_t offset [in] byte offset into font data
///
/// @return uint16_t - big-endian 16-bit value at offset
///
/// @brief
/// Read a big-endian uint16 from the font data with bounds checking.
///
/////////////////////////////////////////////////////////////////////////////
uint16_t cWSFontClassifier::ReadU16BE(uint32_t offset) const
{
    if (offset + 2 > mDataSize)
    {
        return 0;
    }
    return static_cast<uint16_t>((mData[offset] << 8) | mData[offset + 1]);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  uint32_t offset [in] byte offset into font data
///
/// @return uint8_t - byte value at offset
///
/// @brief
/// Read a single byte from the font data with bounds checking.
///
/////////////////////////////////////////////////////////////////////////////
uint8_t cWSFontClassifier::ReadU8(uint32_t offset) const
{
    if (offset >= mDataSize)
    {
        return 0;
    }
    return mData[offset];
}


// =========================================================================
// Layer 4a: Pitch Classifier (bit 15)
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string& source [out] diagnostic: which method decided
///
/// @return bool - true if proportional, false if monospace
///
/// @brief
/// Classifies pitch by sampling actual glyph advance widths. If all
/// sampled advances are equal, the font is monospace. Requires at
/// least 4 valid samples.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSFontClassifier::ClassifyPitch(std::string& source)
{
    if (!mFontLoaded || mFontInfo == nullptr)
    {
        source = "default";
        return true;  // default proportional
    }

    stbtt_fontinfo* info = static_cast<stbtt_fontinfo*>(mFontInfo);

    // Sample set: space, i, m, W, 0, 1, A, -, .
    int sampleCodes[] = { ' ', 'i', 'm', 'W', '0', '1', 'A', '-', '.' };
    int sampleCount = 9;

    int firstAdvance = -1;
    int validSamples = 0;
    bool allEqual = true;

    for (int s = 0; s < sampleCount; s++)
    {
        int glyphIndex = stbtt_FindGlyphIndex(info, sampleCodes[s]);
        if (glyphIndex == 0)
        {
            // Glyph not in font
            continue;
        }

        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(info, sampleCodes[s], &advance, &lsb);

        if (advance <= 0)
        {
            continue;
        }

        validSamples++;
        if (firstAdvance < 0)
        {
            firstAdvance = advance;
        }
        else if (advance != firstAdvance)
        {
            allEqual = false;
        }
    }

    // Need at least 4 valid samples
    if (validSamples < 4)
    {
        source = "default";
        return true;  // default proportional
    }

    if (allEqual)
    {
        source = "metrics";
        return false;  // monospace
    }

    source = "metrics";
    return true;  // proportional
}


// =========================================================================
// Layer 4b: Style Classifier (bits 11-10)
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] font family name for keyword fallback
/// @param  std::string& source [out] diagnostic: which method decided
///
/// @return eWSGenericStyle - the classified style
///
/// @brief
/// Classifies font style using precedence: PANOSE, then sFamilyClass,
/// then name keywords, then default Sans.
///
/////////////////////////////////////////////////////////////////////////////
eWSGenericStyle cWSFontClassifier::ClassifyStyle(const std::string& fontName,
                                                  std::string& source)
{
    if (mFontLoaded && mOS2Offset > 0)
    {
        // Try PANOSE first
        eWSGenericStyle panoseResult = ClassifyStyleFromPANOSE();
        if (panoseResult != WS_STYLE_SANS || mOS2Offset == 0)
        {
            // PANOSE gave a non-default answer, trust it
            // (Sans is also valid, but we check familyClass to confirm)
        }

        // Check if PANOSE was meaningful (not all zeros)
        if (mOS2Offset > 0)
        {
            bool allZero = true;
            for (int i = 0; i < 10; i++)
            {
                if (ReadU8(mOS2Offset + 32 + i) != 0)
                {
                    allZero = false;
                    break;
                }
            }
            if (!allZero)
            {
                source = "panose";
                return panoseResult;
            }
        }

        // Try sFamilyClass
        eWSGenericStyle fcResult = ClassifyStyleFromFamilyClass();
        if (fcResult != WS_STYLE_SANS)
        {
            // sFamilyClass gave a non-default answer
            source = "familyclass";
            return fcResult;
        }

        // sFamilyClass was 0 (unclassified) or Sans
        // Check if sFamilyClass actually had a value
        uint16_t familyClass = ReadU16BE(mOS2Offset + 30);
        if ((familyClass >> 8) != 0)
        {
            // sFamilyClass had a real value (8 = Sans), use it
            source = "familyclass";
            return fcResult;
        }
    }

    // Fall back to name keywords
    eWSGenericStyle nameResult = ClassifyStyleFromName(fontName);
    source = "keyword";
    return nameResult;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return eWSGenericStyle - style from PANOSE data, or WS_STYLE_SANS as default
///
/// @brief
/// Reads PANOSE bytes from OS/2 table. bFamilyType at offset 32,
/// bSerifStyle at offset 33.
///
/////////////////////////////////////////////////////////////////////////////
eWSGenericStyle cWSFontClassifier::ClassifyStyleFromPANOSE(void)
{
    if (mOS2Offset == 0)
    {
        return WS_STYLE_SANS;
    }

    uint8_t familyType = ReadU8(mOS2Offset + 32);
    uint8_t serifStyle = ReadU8(mOS2Offset + 33);

    if (familyType == 2)
    {
        // Latin Text: inspect bSerifStyle
        // Values 2-4 are no-serif (cove, square, thin, etc. are ALL serif subtypes)
        // Actually in PANOSE: 2-10 are various serif types, 11-15 are sans variants
        // bSerifStyle: 2-10 = serif subtypes, 11-15 = sans subtypes
        if (serifStyle >= 11 && serifStyle <= 15)
        {
            return WS_STYLE_SANS;
        }
        if (serifStyle >= 2 && serifStyle <= 10)
        {
            return WS_STYLE_SERIF;
        }
        // serifStyle 0 or 1 = any/no fit
        return WS_STYLE_SANS;
    }
    else if (familyType == 3)
    {
        // Latin Hand Written
        return WS_STYLE_SCRIPT;
    }
    else if (familyType == 4 || familyType == 5)
    {
        // Latin Decorative or Latin Pictorial
        return WS_STYLE_DISPLAY;
    }

    return WS_STYLE_SANS;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return eWSGenericStyle - style from sFamilyClass, or WS_STYLE_SANS
///
/// @brief
/// Reads sFamilyClass (2 bytes at OS/2 offset 30). High byte is the
/// class ID.
///
/////////////////////////////////////////////////////////////////////////////
eWSGenericStyle cWSFontClassifier::ClassifyStyleFromFamilyClass(void)
{
    if (mOS2Offset == 0)
    {
        return WS_STYLE_SANS;
    }

    uint16_t familyClass = ReadU16BE(mOS2Offset + 30);
    int classID = familyClass >> 8;

    switch (classID)
    {
        case 1:  // Oldstyle Serifs
        case 2:  // Transitional Serifs
        case 3:  // Modern Serifs
        case 4:  // Clarendon Serifs
        case 5:  // Slab Serifs
        case 7:  // Freeform Serifs
        {
            return WS_STYLE_SERIF;
        }
        case 8:  // Sans Serif
        {
            return WS_STYLE_SANS;
        }
        case 9:  // Ornamentals
        {
            return WS_STYLE_DISPLAY;
        }
        case 10: // Scripts
        {
            return WS_STYLE_SCRIPT;
        }
        default:
        {
            return WS_STYLE_SANS;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] font family name
///
/// @return eWSGenericStyle - style from name keywords
///
/// @brief
/// Case-insensitive keyword matching against font family name.
/// Last resort before default Sans.
///
/////////////////////////////////////////////////////////////////////////////
eWSGenericStyle cWSFontClassifier::ClassifyStyleFromName(const std::string& fontName)
{
    std::string lower = fontName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Script/cursive keywords (check first -- beats serif/sans)
    if (lower.find("script") != std::string::npos ||
        lower.find("cursive") != std::string::npos ||
        lower.find("hand") != std::string::npos ||
        lower.find("brush") != std::string::npos)
    {
        return WS_STYLE_SCRIPT;
    }

    // Display/decorative keywords
    if (lower.find("display") != std::string::npos ||
        lower.find("decorative") != std::string::npos ||
        lower.find("ornament") != std::string::npos ||
        lower.find("poster") != std::string::npos)
    {
        return WS_STYLE_DISPLAY;
    }

    // Serif keywords (check "serif" but exclude "sans serif")
    if (lower.find("serif") != std::string::npos &&
        lower.find("sans") == std::string::npos)
    {
        return WS_STYLE_SERIF;
    }

    // Specific serif font families
    if (lower.find("times") != std::string::npos ||
        lower.find("georgia") != std::string::npos ||
        lower.find("palatino") != std::string::npos ||
        lower.find("baskerville") != std::string::npos ||
        lower.find("garamond") != std::string::npos)
    {
        return WS_STYLE_SERIF;
    }

    // Sans keywords
    if (lower.find("sans") != std::string::npos ||
        lower.find("arial") != std::string::npos ||
        lower.find("helvetica") != std::string::npos ||
        lower.find("verdana") != std::string::npos ||
        lower.find("calibri") != std::string::npos ||
        lower.find("trebuchet") != std::string::npos)
    {
        return WS_STYLE_SANS;
    }

    // Default: Sans
    return WS_STYLE_SANS;
}


// =========================================================================
// Layer 4c: Symbol Classifier (bits 13-12)
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] font family name for keyword fallback
/// @param  std::string& source [out] diagnostic: which method decided
///
/// @return eWSSymbolMapping - the classified symbol mapping
///
/// @brief
/// Classifies symbol mapping by probing cmap coverage for math and
/// symbol glyphs, then falling back to name keywords.
///
/////////////////////////////////////////////////////////////////////////////
eWSSymbolMapping cWSFontClassifier::ClassifySymbol(const std::string& fontName,
                                                    std::string& source)
{
    if (mFontLoaded && mFontInfo != nullptr)
    {
        stbtt_fontinfo* info = static_cast<stbtt_fontinfo*>(mFontInfo);

        // Probe for math symbols
        int mathProbes[] = {
            0x00B1,  // plus-minus
            0x00D7,  // multiplication
            0x00F7,  // division
            0x2211,  // summation
            0x222B,  // integral
            0x221A   // square root
        };
        int mathHits = 0;
        for (int i = 0; i < 6; i++)
        {
            if (stbtt_FindGlyphIndex(info, mathProbes[i]) != 0)
            {
                mathHits++;
            }
        }

        // Probe for symbol glyphs
        int symbolProbes[] = {
            0x2190,  // left arrow
            0x2191,  // up arrow
            0x2192,  // right arrow
            0x2193,  // down arrow
            0x25A0,  // black square
            0x25CF,  // black circle
            0x2702,  // scissors
            0x2714   // check mark
        };
        int symbolHits = 0;
        for (int i = 0; i < 8; i++)
        {
            if (stbtt_FindGlyphIndex(info, symbolProbes[i]) != 0)
            {
                symbolHits++;
            }
        }

        // Probe for basic Latin (A-Z)
        int latinHits = 0;
        for (int c = 'A'; c <= 'Z'; c++)
        {
            if (stbtt_FindGlyphIndex(info, c) != 0)
            {
                latinHits++;
            }
        }

        // Decision: strong math + weak Latin = Math font
        if (mathHits >= 4 && latinHits < 20)
        {
            source = "cmap";
            return WS_SYMBOL_MATH;
        }

        // Decision: strong symbol + weak Latin = Symbol font
        if (symbolHits >= 4 && latinHits < 20)
        {
            source = "cmap";
            return WS_SYMBOL_SYMBOLS;
        }
    }

    // Fall back to name keywords
    std::string lower = fontName;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return std::tolower(c); });

    if (lower.find("symbol") != std::string::npos ||
        lower.find("wingdings") != std::string::npos ||
        lower.find("dingbat") != std::string::npos)
    {
        source = "keyword";
        return WS_SYMBOL_SYMBOLS;
    }

    if (lower.find("math") != std::string::npos ||
        lower.find("stix") != std::string::npos)
    {
        source = "keyword";
        return WS_SYMBOL_MATH;
    }

    source = "default";
    return WS_SYMBOL_CP437;
}


// =========================================================================
// Layer 5: Font Index Lookup
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] modern font family name
/// @param  eWSGenericStyle style [in] classified style
/// @param  bool proportional [in] classified pitch
///
/// @return uint16_t - index into gOrgFonts (0-247), or 0 as fallback
///
/// @brief
/// Reverse lookup in gOrgFonts table. Finds the best matching entry
/// by systemName, preferring entries that match both style and pitch.
///
/////////////////////////////////////////////////////////////////////////////
uint16_t cWSFontClassifier::LookupFontIndex(const std::string& fontName,
                                              eWSGenericStyle style,
                                              bool proportional)
{
    if (gOrgFonts.empty())
    {
        return 0;
    }

    // Map our style enum to the old eFontStyle enum used in gOrgFonts
    eFontStyle targetStyle = STYLE_UNKNOWN;
    if (style == WS_STYLE_SANS)
    {
        targetStyle = STYLE_SANS;
    }
    else if (style == WS_STYLE_SERIF)
    {
        targetStyle = STYLE_SERIF;
    }
    else if (style == WS_STYLE_SCRIPT)
    {
        targetStyle = STYLE_SCRIPT;
    }

    // Build lowercase font name for matching
    std::string lowerName = fontName;
    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // First pass: exact systemName match with matching properties
    for (size_t i = 0; i < gOrgFonts.size(); i++)
    {
        std::string lowerSys = gOrgFonts[i].systemName;
        std::transform(lowerSys.begin(), lowerSys.end(), lowerSys.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (lowerSys == lowerName &&
            gOrgFonts[i].style == targetStyle &&
            gOrgFonts[i].proportional == proportional)
        {
            return static_cast<uint16_t>(i);
        }
    }

    // Second pass: systemName match regardless of properties
    for (size_t i = 0; i < gOrgFonts.size(); i++)
    {
        std::string lowerSys = gOrgFonts[i].systemName;
        std::transform(lowerSys.begin(), lowerSys.end(), lowerSys.begin(),
            [](unsigned char c) { return std::tolower(c); });

        if (lowerSys == lowerName)
        {
            return static_cast<uint16_t>(i);
        }
    }

    // Third pass: match by style + proportional (generic fallback)
    for (size_t i = 0; i < gOrgFonts.size(); i++)
    {
        if (gOrgFonts[i].style == targetStyle &&
            gOrgFonts[i].proportional == proportional)
        {
            return static_cast<uint16_t>(i);
        }
    }

    // Fallback: index 0 (LinePrinter)
    return 0;
}


// =========================================================================
// Keyword-only Fallback
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] font family name
///
/// @return sWSFontClassification - classification using keyword matching only
///
/// @brief
/// Delegates to the existing cFontClassifier for keyword-based
/// classification when font file is not available.
///
/////////////////////////////////////////////////////////////////////////////
sWSFontClassification cWSFontClassifier::ClassifyByKeywords(const std::string& fontName)
{
    cFontClassifier fc;
    sFontProperties props = fc.classify(fontName);

    sWSFontClassification result;

    // Pitch
    result.proportional = props.proportional;
    result.pitchSource = "keyword";

    // Style
    if (props.style == STYLE_SANS)
    {
        result.genericStyle = WS_STYLE_SANS;
    }
    else if (props.style == STYLE_SERIF)
    {
        result.genericStyle = WS_STYLE_SERIF;
    }
    else if (props.style == STYLE_SCRIPT)
    {
        result.genericStyle = WS_STYLE_SCRIPT;
    }
    else
    {
        result.genericStyle = WS_STYLE_DISPLAY;
    }
    result.styleSource = "keyword";

    // Symbol
    if (props.symbol)
    {
        result.symbolMapping = WS_SYMBOL_SYMBOLS;
    }
    else if (props.math)
    {
        result.symbolMapping = WS_SYMBOL_MATH;
    }
    else
    {
        result.symbolMapping = WS_SYMBOL_CP437;
    }
    result.symbolSource = "keyword";

    // Font index
    result.fontIndex = LookupFontIndex(fontName, result.genericStyle,
                                        result.proportional);

    return result;
}


// =========================================================================
// Public API
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& fontName [in] font family name
///
/// @return sWSFontClassification - complete classification result
///
/// @brief
/// Main entry point. Resolves font name to file, loads it with
/// stb_truetype, runs all classifiers, and assembles the bitfield.
/// Falls back to keyword classification if font file not available.
///
/////////////////////////////////////////////////////////////////////////////
sWSFontClassification cWSFontClassifier::Classify(const std::string& fontName)
{
    // Initialize
    mData = nullptr;
    mDataSize = 0;
    mFontStart = 0;
    mFontLoaded = false;
    mOS2Offset = 0;
    mOS2Length = 0;
    mFontInfo = nullptr;

    // Try to find and load the font file
    std::string filePath = FindFontFile(fontName);
    if (filePath.empty())
    {
        return ClassifyByKeywords(fontName);
    }

    // Read the font file
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return ClassifyByKeywords(fontName);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<unsigned char> fontData(fileSize);
    if (!file.read(reinterpret_cast<char*>(fontData.data()),
                   static_cast<std::streamsize>(fileSize)))
    {
        return ClassifyByKeywords(fontName);
    }
    file.close();

    // Classify from the loaded data
    sWSFontClassification result = ClassifyFromData(
        fontData.data(), fontData.size(), fontName);

    // Clean up stbtt_fontinfo
    if (mFontInfo != nullptr)
    {
        delete static_cast<stbtt_fontinfo*>(mFontInfo);
        mFontInfo = nullptr;
    }

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  const unsigned char* fontData [in] raw font file bytes
/// @param  size_t dataSize [in] size of font data
/// @param  const std::string& fontName [in] font family name for keyword fallback
///
/// @return sWSFontClassification - complete classification result
///
/// @brief
/// Classify from raw font bytes. Used by Classify() after loading
/// the file, or directly for testing.
///
/////////////////////////////////////////////////////////////////////////////
sWSFontClassification cWSFontClassifier::ClassifyFromData(
    const unsigned char* fontData, size_t dataSize,
    const std::string& fontName)
{
    if (!LoadFont(fontData, dataSize))
    {
        return ClassifyByKeywords(fontName);
    }

    sWSFontClassification result;

    // Run all three classifiers independently
    result.proportional = ClassifyPitch(result.pitchSource);
    result.genericStyle = ClassifyStyle(fontName, result.styleSource);
    result.symbolMapping = ClassifySymbol(fontName, result.symbolSource);

    // Font index lookup
    result.fontIndex = LookupFontIndex(fontName, result.genericStyle,
                                        result.proportional);

    return result;
}
