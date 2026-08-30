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
 * @class cTUIFontCalculatorHarfBuzz
 * @brief HarfBuzz/FreeType font measurement backend for high-fidelity text shaping.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIFontCalculatorHarfBuzz, a cTUIFontCalculator subclass that
 * provides high-fidelity text shaping and glyph measurement using HarfBuzz
 * for OpenType shaping and FreeType for font rasterization. Returns widths
 * and vertical metrics in design units, converted to twips via the formula
 * twips = designUnits * pointSize * 20 / UPEM (DPI-free). This is the
 * preferred measurement backend on Linux when HarfBuzz is available.
 *
 * @section harfbuzz_pipeline Shaping Pipeline
 * Loads font files via FreeType, creates HarfBuzz font objects with
 * hb_ft_font_create_referenced(), and sets scale to UPEM for design-unit
 * output. Shapes text with hb_shape() and reads per-glyph advances from
 * the resulting glyph info buffer.
 *
 * @section harfbuzz_twips Twips Conversion
 * All horizontal advances use the DPI-free formula:
 * twips = hbUnits * fontSize * 20 / units_per_EM. Vertical metrics from
 * FreeType (ascent, descent, line gap) use integer pixel values shifted
 * right by 6 bits, then scaled by 15 to approximate twips.
 *
 * @see cTUIFontCalculatorHarfBuzz
 * @see cTUIFontCalculator
 * @see sTUIFontInfo
 * @see sTUITextMetrics
 */

#include "platform_harfbuzz.h"

// Only compile if HarfBuzz is available
#ifdef HAVE_HARFBUZZ

#include <hb.h>
#include <hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H

#include <algorithm>
#include <fstream>
#include <codecvt>
#include <locale>
#include <dirent.h>
#include <sys/stat.h>

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for HarfBuzz font calculator
/// Initializes HarfBuzz and FreeType backend for advanced text shaping
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorHarfBuzz::cTUIFontCalculatorHarfBuzz(void)
    : mHBFont(nullptr), mHBBuffer(nullptr), mHBFace(nullptr),
      mFTLibrary(nullptr), mFTFace(nullptr), mFontScale(1.0f), mFontSize(12),
      mAscent(0), mDescent(0), mLineGap(0) {
    mBackend = FONT_BACKEND_HARFBUZZ;
    mMode = FONT_MODE_DOCUMENT;
    mDocumentMode = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for HarfBuzz font calculator
/// Ensures proper cleanup of HarfBuzz and FreeType resources
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorHarfBuzz::~cTUIFontCalculatorHarfBuzz(void) {
    Shutdown();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initializes the HarfBuzz and FreeType font measurement system
/// Sets up libraries and creates default font configuration
///
/// @note Requires HarfBuzz and FreeType libraries at runtime
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorHarfBuzz::Initialize(void) {
    if (!InitializeLibraries()) {
        return false;
    }
    
    // Set default font
    sTUIFontInfo defaultFont;
    defaultFont.name = "Liberation Serif";
    defaultFont.size = 12;
    
    return SetFont(defaultFont);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shuts down the HarfBuzz font calculator and releases resources
/// Cleans up HarfBuzz/FreeType objects and clears font data
///
/////////////////////////////////////////////////////////////////////////////
void cTUIFontCalculatorHarfBuzz::Shutdown(void) {
    CleanupLibraries();
    mFontData.clear();
    mFontPath.clear();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontInfo& font [in] font information to set
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Sets the current font using HarfBuzz and FreeType font loading
/// Loads font from system paths or embedded font data
///
/// @note Supports both font file paths and font family names
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorHarfBuzz::SetFont(const sTUIFontInfo& font) {
    mCurrentFont = font;
    mFontSize = font.size;
    
    // Clean up existing font
    if (mHBFont) {
        hb_font_destroy(mHBFont);
        mHBFont = nullptr;
    }
    // mHBFace is non-owning (from hb_font_get_face), just clear pointer
    mHBFace = nullptr;
    if (mFTFace) {
        FT_Done_Face(mFTFace);
        mFTFace = nullptr;
    }
    
    // Try to load the requested font
    // If font.name contains a path (starts with / or contains /), use it directly
    bool success = false;
    if (font.name.find('/') != std::string::npos) {
        // font.name is actually a file path from fontconfig
        success = LoadFontFromFile(font.name);
    } else {
        // font.name is just a family name, search for it
        success = LoadSystemFont(font.name);
    }
    
    if (success) {
        return CreateHarfBuzzFont();
    }
    
    return false;
}

std::vector<std::string> cTUIFontCalculatorHarfBuzz::GetAvailableFonts(void) {
    return ScanSystemFonts();
}

sTUITextMetrics cTUIFontCalculatorHarfBuzz::MeasureText(const std::string& text) {
    return MeasureTextWithShaping(text);
}

sTUITextMetrics cTUIFontCalculatorHarfBuzz::MeasureTextWithShaping(const std::string& text) {
    sTUITextMetrics metrics;
    
    if (!mHBFont || !mHBBuffer || text.empty()) {
        return metrics;
    }
    
    // Convert text to UTF-32
    std::vector<char32_t> codepoints = UTF8ToUTF32(text);
    if (codepoints.empty()) {
        return metrics;
    }
    
    // Shape the text
    std::vector<unsigned int> glyphs;
    std::vector<int> advances;
    std::vector<int> offsets;
    
    ShapeText(codepoints, glyphs, advances, offsets);
    
    // Calculate total width
    int totalAdvance = 0;
    for (int advance : advances) {
        totalAdvance += advance;
    }
    
    metrics.widthTWIPS = HBUnitsToTWIPS(totalAdvance);
    metrics.heightTWIPS = GetLineHeight();
    metrics.ascentTWIPS = FTUnitsToTWIPS(mAscent);
    metrics.descentTWIPS = FTUnitsToTWIPS(mDescent);
    metrics.leadingTWIPS = FTUnitsToTWIPS(mLineGap);
    
    return metrics;
}

int cTUIFontCalculatorHarfBuzz::MeasureCharacterWidth(char32_t codepoint) {
    if (!mHBFont || !mHBBuffer) {
        return 144; // Default fallback
    }
    
    // Convert single character to string and measure
    std::string text;
    if (codepoint <= 0x7F) {
        text = static_cast<char>(codepoint);
    } else {
        // Convert to UTF-8
        std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
        text = converter.to_bytes(codepoint);
    }
    
    sTUITextMetrics metrics = MeasureTextWithShaping(text);
    return metrics.widthTWIPS;
}

int cTUIFontCalculatorHarfBuzz::GetLineHeight(void) {
    if (!mFTFace) {
        return 240; // Default fallback
    }
    
    return FTUnitsToTWIPS(mAscent - mDescent + mLineGap);
}

bool cTUIFontCalculatorHarfBuzz::SupportsCharacter(char32_t codepoint) {
    if (!mFTFace) return false;
    
    FT_UInt glyphIndex = FT_Get_Char_Index(mFTFace, codepoint);
    return glyphIndex != 0;
}

bool cTUIFontCalculatorHarfBuzz::SupportsUnicode(void) const {
    return true;
}

bool cTUIFontCalculatorHarfBuzz::LoadFontFromFile(const std::string& fontPath) {
    // Read font file
    std::ifstream file(fontPath, std::ios::binary);
    if (!file) return false;
    
    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    mFontData.resize(fileSize);
    file.read(reinterpret_cast<char*>(mFontData.data()), fileSize);
    
    if (!file) return false;
    
    mFontPath = fontPath;
    return LoadFontFromMemory(mFontData);
}

bool cTUIFontCalculatorHarfBuzz::LoadFontFromMemory(const std::vector<uint8_t>& fontData) {
    if (!mFTLibrary || fontData.empty()) return false;
    
    // Load font with FreeType
    FT_Error error = FT_New_Memory_Face(mFTLibrary, 
                                       fontData.data(), 
                                       static_cast<FT_Long>(fontData.size()),
                                       0, &mFTFace);
    
    if (error != 0) return false;
    
    // Set font size
    error = FT_Set_Char_Size(mFTFace, 0, mFontSize * 64, 96, 96);
    if (error != 0) return false;
    
    // Get font metrics
    mAscent = mFTFace->size->metrics.ascender >> 6;
    mDescent = mFTFace->size->metrics.descender >> 6;
    mLineGap = (mFTFace->size->metrics.height >> 6) - mAscent + mDescent;
    
    return true;
}

std::vector<int> cTUIFontCalculatorHarfBuzz::GetGlyphAdvances(const std::string& text) {
    std::vector<int> advances;
    
    if (!mHBFont || !mHBBuffer || text.empty()) {
        return advances;
    }
    
    std::vector<char32_t> codepoints = UTF8ToUTF32(text);
    std::vector<unsigned int> glyphs;
    std::vector<int> offsets;
    
    ShapeText(codepoints, glyphs, advances, offsets);
    
    return advances;
}

std::vector<int> cTUIFontCalculatorHarfBuzz::GetGlyphPositions(const std::string& text) {
    std::vector<int> positions;
    std::vector<int> advances = GetGlyphAdvances(text);
    
    int position = 0;
    for (int advance : advances) {
        positions.push_back(position);
        position += advance;
    }
    
    return positions;
}

bool cTUIFontCalculatorHarfBuzz::HasKerning(void) const {
    if (!mFTFace) return false;
    return FT_HAS_KERNING(mFTFace);
}

int cTUIFontCalculatorHarfBuzz::GetKerning(char32_t left, char32_t right) {
    if (!mFTFace || !HasKerning()) return 0;
    
    FT_UInt leftGlyph = FT_Get_Char_Index(mFTFace, left);
    FT_UInt rightGlyph = FT_Get_Char_Index(mFTFace, right);
    
    if (leftGlyph == 0 || rightGlyph == 0) return 0;
    
    FT_Vector kerning;
    FT_Error error = FT_Get_Kerning(mFTFace, leftGlyph, rightGlyph, 
                                   FT_KERNING_DEFAULT, &kerning);
    
    if (error != 0) return 0;
    
    return FTUnitsToTWIPS(kerning.x >> 6);
}

bool cTUIFontCalculatorHarfBuzz::InitializeLibraries(void) {
    // Initialize FreeType
    FT_Error error = FT_Init_FreeType(&mFTLibrary);
    if (error != 0) return false;
    
    // Create HarfBuzz buffer
    mHBBuffer = hb_buffer_create();
    if (!mHBBuffer) {
        FT_Done_FreeType(mFTLibrary);
        mFTLibrary = nullptr;
        return false;
    }
    
    return true;
}

void cTUIFontCalculatorHarfBuzz::CleanupLibraries(void) {
    if (mHBFont) {
        hb_font_destroy(mHBFont);
        mHBFont = nullptr;
    }

    // mHBFace is obtained via hb_font_get_face() (non-owning reference)
    // It is destroyed when mHBFont is destroyed, so just clear the pointer
    mHBFace = nullptr;

    if (mHBBuffer) {
        hb_buffer_destroy(mHBBuffer);
        mHBBuffer = nullptr;
    }
    
    if (mFTFace) {
        FT_Done_Face(mFTFace);
        mFTFace = nullptr;
    }
    
    if (mFTLibrary) {
        FT_Done_FreeType(mFTLibrary);
        mFTLibrary = nullptr;
    }
}

bool cTUIFontCalculatorHarfBuzz::CreateHarfBuzzFont(void) {
    if (!mFTFace) return false;

    // Create HarfBuzz font directly from FreeType face
    // hb_ft_font_create_referenced inherits FreeType's DPI/size scaling
    // Advances come back in 26.6 fixed-point pixels at the configured DPI (96)
    mHBFont = hb_ft_font_create_referenced(mFTFace);
    if (!mHBFont) return false;

    // Get the HarfBuzz face from the font (non-owning reference)
    mHBFace = hb_font_get_face(mHBFont);

    return true;
}

bool cTUIFontCalculatorHarfBuzz::LoadSystemFont(const std::string& fontName) {
    std::string fontPath = FindBestFontMatch(fontName);
    if (fontPath.empty()) return false;
    
    return LoadFontFromFile(fontPath);
}

std::vector<std::string> cTUIFontCalculatorHarfBuzz::ScanSystemFonts(void) {
    std::vector<std::string> fonts;
    std::vector<std::string> fontDirs = {
        "/usr/share/fonts",
        "/usr/local/share/fonts",
        "/usr/share/fonts/truetype",
        "/usr/share/fonts/TTF",
        "/usr/share/fonts/opentype",
        "/usr/share/fonts/OTF"
    };
    
    // Add user font directories
    const char* home = getenv("HOME");
    if (home) {
        fontDirs.push_back(std::string(home) + "/.fonts");
        fontDirs.push_back(std::string(home) + "/.local/share/fonts");
    }
    
    for (const std::string& dir : fontDirs) {
        DIR* directory = opendir(dir.c_str());
        if (!directory) continue;
        
        struct dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            std::string filename = entry->d_name;
            std::string fullPath = dir + "/" + filename;
            
            if (IsFontFile(fullPath)) {
                // Extract font family name (simplified - just use filename)
                size_t dotPos = filename.find_last_of('.');
                if (dotPos != std::string::npos) {
                    std::string fontName = filename.substr(0, dotPos);
                    // Convert underscores to spaces
                    std::replace(fontName.begin(), fontName.end(), '_', ' ');
                    fonts.push_back(fontName);
                }
            }
        }
        
        closedir(directory);
    }
    
    // Remove duplicates and sort
    std::sort(fonts.begin(), fonts.end());
    fonts.erase(std::unique(fonts.begin(), fonts.end()), fonts.end());
    
    return fonts;
}

std::vector<char32_t> cTUIFontCalculatorHarfBuzz::UTF8ToUTF32(const std::string& text) {
    std::vector<char32_t> result;
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    
    try {
        std::u32string utf32 = converter.from_bytes(text);
        result.assign(utf32.begin(), utf32.end());
    } catch (...) {
        // Fallback: treat as ASCII
        result.reserve(text.length());
        for (char c : text) {
            result.push_back(static_cast<char32_t>(c));
        }
    }
    
    return result;
}

void cTUIFontCalculatorHarfBuzz::ShapeText(const std::vector<char32_t>& codepoints,
                                          std::vector<unsigned int>& glyphs,
                                          std::vector<int>& advances,
                                          std::vector<int>& offsets) {
    glyphs.clear();
    advances.clear();
    offsets.clear();
    
    if (!mHBFont || !mHBBuffer || codepoints.empty()) return;
    
    // Clear buffer
    hb_buffer_clear_contents(mHBBuffer);
    
    // Add text to buffer
    hb_buffer_add_codepoints(mHBBuffer, reinterpret_cast<const hb_codepoint_t*>(codepoints.data()), 
                            static_cast<int>(codepoints.size()), 0, -1);
    
    // Set buffer properties
    hb_buffer_set_direction(mHBBuffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(mHBBuffer, HB_SCRIPT_LATIN);
    hb_buffer_set_language(mHBBuffer, hb_language_from_string("en", -1));
    
    // Shape the text
    hb_shape(mHBFont, mHBBuffer, nullptr, 0);
    
    // Get results
    unsigned int glyphCount;
    hb_glyph_info_t* glyphInfo = hb_buffer_get_glyph_infos(mHBBuffer, &glyphCount);
    hb_glyph_position_t* glyphPos = hb_buffer_get_glyph_positions(mHBBuffer, &glyphCount);
    
    glyphs.reserve(glyphCount);
    advances.reserve(glyphCount);
    offsets.reserve(glyphCount * 2);
    
    for (unsigned int i = 0; i < glyphCount; i++) {
        glyphs.push_back(glyphInfo[i].codepoint);
        advances.push_back(glyphPos[i].x_advance);
        offsets.push_back(glyphPos[i].x_offset);
        offsets.push_back(glyphPos[i].y_offset);
    }
}

int cTUIFontCalculatorHarfBuzz::FTUnitsToTWIPS(int ftUnits) const {
    // mAscent, mDescent, mLineGap are already integer pixels at 96 DPI
    // (converted from FreeType 26.6 fixed-point via >> 6 in LoadFontFromMemory)
    // Convert pixels to TWIPS: multiply by FONTSCALE (1440/96 = 15)
    return ftUnits * 15;
}

int cTUIFontCalculatorHarfBuzz::HBUnitsToTWIPS(int hbUnits) const {
    // hb_ft_font_create_referenced returns advances in 26.6 fixed-point pixels at 96 DPI
    // Convert to TWIPS: pixels * FONTSCALE (1440/96 = 15), then divide by 64 for 26.6
    // Equivalent to (hbUnits * 15) / 64
    return (hbUnits * 15) / 64;
}

std::string cTUIFontCalculatorHarfBuzz::FindBestFontMatch(const std::string& requestedFont) {
    std::vector<std::string> fontDirs = {
        "/usr/share/fonts/truetype",
        "/usr/share/fonts/TTF",
        "/usr/share/fonts",
        "/usr/local/share/fonts"
    };
    
    // Add user directories
    const char* home = getenv("HOME");
    if (home) {
        fontDirs.push_back(std::string(home) + "/.fonts");
        fontDirs.push_back(std::string(home) + "/.local/share/fonts");
    }
    
    // Normalize a string: remove spaces, hyphens, underscores, and lowercase
    // This allows "Liberation Serif" to match "LiberationSerif-Regular.ttf"
    auto normalize = [](const std::string& s) {
        std::string result;
        for (char c : s)
        {
            if (c != ' ' && c != '-' && c != '_')
            {
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        return result;
    };

    std::string normalizedRequested = normalize(requestedFont);

    // Collect all matching fonts, then pick the best one
    // Prefer "Regular" variants over Bold/Italic/BoldItalic
    std::string bestMatch;
    int bestScore = -1;

    for (const std::string& dir : fontDirs) {
        DIR* directory = opendir(dir.c_str());
        if (!directory) continue;

        struct dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            std::string filename = entry->d_name;
            std::string fullPath = dir + "/" + filename;

            if (IsFontFile(fullPath)) {
                std::string normalizedFilename = normalize(filename);

                if (normalizedFilename.find(normalizedRequested) != std::string::npos) {
                    // Score the match: prefer regular variants
                    // Higher score = better match
                    int score = 0;
                    bool hasItalic = (normalizedFilename.find("italic") != std::string::npos);
                    bool hasBold = (normalizedFilename.find("bold") != std::string::npos);
                    bool hasRegular = (normalizedFilename.find("regular") != std::string::npos);

                    // Want: match bold/italic to mCurrentFont style
                    if (hasRegular && !mCurrentFont.bold && !mCurrentFont.italic)
                    {
                        score = 100;
                    }
                    else if (!hasItalic && !hasBold)
                    {
                        // No style suffix -- treat as regular
                        score = 90;
                    }
                    else if (hasBold && !hasItalic && mCurrentFont.bold && !mCurrentFont.italic)
                    {
                        score = 100;
                    }
                    else if (hasItalic && !hasBold && !mCurrentFont.bold && mCurrentFont.italic)
                    {
                        score = 100;
                    }
                    else if (hasBold && hasItalic && mCurrentFont.bold && mCurrentFont.italic)
                    {
                        score = 100;
                    }
                    else
                    {
                        // Style mismatch penalty
                        score = 10;
                    }

                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestMatch = fullPath;
                    }
                }
            }
        }

        closedir(directory);
    }

    if (!bestMatch.empty())
    {
        return bestMatch;
    }
    
    // Fallback: return first available font
    for (const std::string& dir : fontDirs) {
        DIR* directory = opendir(dir.c_str());
        if (!directory) continue;
        
        struct dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            std::string fullPath = dir + "/" + entry->d_name;
            if (IsFontFile(fullPath)) {
                closedir(directory);
                return fullPath;
            }
        }
        
        closedir(directory);
    }
    
    return "";
}

bool cTUIFontCalculatorHarfBuzz::IsFontFile(const std::string& path) const {
    struct stat st;
    if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    
    // Check file extension
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos) return false;
    
    std::string extension = path.substr(dotPos + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    return (extension == "ttf" || extension == "otf" || 
            extension == "ttc" || extension == "otc");
}

#else // !HAVE_HARFBUZZ

// Stub implementation when HarfBuzz is not available
cTUIFontCalculatorHarfBuzz::cTUIFontCalculatorHarfBuzz(void) 
    : mHBFont(nullptr), mHBBuffer(nullptr), mHBFace(nullptr),
      mFTLibrary(nullptr), mFTFace(nullptr) {
    mBackend = FONT_BACKEND_HARFBUZZ;
}

cTUIFontCalculatorHarfBuzz::~cTUIFontCalculatorHarfBuzz(void) {}

bool cTUIFontCalculatorHarfBuzz::Initialize(void) { return false; }
void cTUIFontCalculatorHarfBuzz::Shutdown(void) {}
bool cTUIFontCalculatorHarfBuzz::SetFont(const sTUIFontInfo& font) { return false; }
std::vector<std::string> cTUIFontCalculatorHarfBuzz::GetAvailableFonts(void) { return {}; }
sTUITextMetrics cTUIFontCalculatorHarfBuzz::MeasureText(const std::string& text) { return {}; }
int cTUIFontCalculatorHarfBuzz::MeasureCharacterWidth(char32_t codepoint) { return 0; }
int cTUIFontCalculatorHarfBuzz::GetLineHeight(void) { return 0; }
bool cTUIFontCalculatorHarfBuzz::SupportsCharacter(char32_t codepoint) { return false; }
bool cTUIFontCalculatorHarfBuzz::SupportsUnicode(void) const { return false; }
sTUITextMetrics cTUIFontCalculatorHarfBuzz::MeasureTextWithShaping(const std::string& text) { return {}; }
bool cTUIFontCalculatorHarfBuzz::LoadFontFromFile(const std::string& fontPath) { return false; }
bool cTUIFontCalculatorHarfBuzz::LoadFontFromMemory(const std::vector<uint8_t>& fontData) { return false; }
std::vector<int> cTUIFontCalculatorHarfBuzz::GetGlyphAdvances(const std::string& text) { return {}; }
std::vector<int> cTUIFontCalculatorHarfBuzz::GetGlyphPositions(const std::string& text) { return {}; }
bool cTUIFontCalculatorHarfBuzz::HasKerning(void) const { return false; }
int cTUIFontCalculatorHarfBuzz::GetKerning(char32_t left, char32_t right) { return 0; }
bool cTUIFontCalculatorHarfBuzz::InitializeLibraries(void) { return false; }
void cTUIFontCalculatorHarfBuzz::CleanupLibraries(void) {}
bool cTUIFontCalculatorHarfBuzz::CreateHarfBuzzFont(void) { return false; }
bool cTUIFontCalculatorHarfBuzz::LoadSystemFont(const std::string& fontName) { return false; }
std::vector<std::string> cTUIFontCalculatorHarfBuzz::ScanSystemFonts(void) { return {}; }
std::vector<char32_t> cTUIFontCalculatorHarfBuzz::UTF8ToUTF32(const std::string& text) { return {}; }
void cTUIFontCalculatorHarfBuzz::ShapeText(const std::vector<char32_t>& codepoints, 
                                          std::vector<unsigned int>& glyphs,
                                          std::vector<int>& advances,
                                          std::vector<int>& offsets) {}
int cTUIFontCalculatorHarfBuzz::FTUnitsToTWIPS(int ftUnits) const { return 0; }
int cTUIFontCalculatorHarfBuzz::HBUnitsToTWIPS(int hbUnits) const { return 0; }
std::string cTUIFontCalculatorHarfBuzz::FindBestFontMatch(const std::string& requestedFont) { return ""; }
bool cTUIFontCalculatorHarfBuzz::IsFontFile(const std::string& path) const { return false; }

#endif // HAVE_HARFBUZZ