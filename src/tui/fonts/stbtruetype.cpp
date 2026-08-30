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
 * @class cTUISTBTrueTypeFontCalculator
 * @brief STB TrueType font measurement backend using stb_truetype.h for lightweight font parsing.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUISTBTrueTypeFontCalculator, a cTUIFontCalculator subclass that
 * uses Sean Barrett's stb_truetype.h single-header library to parse TrueType
 * font files and extract glyph metrics (advance widths, ascent, descent, line
 * gap) directly from font tables. Serves as a lightweight fallback when
 * HarfBuzz is unavailable. Converts design-unit advances to twips via
 * twips = designUnits * pointSize * 20 / UPEM.
 *
 * @section stb_loading Font Loading
 * Loads font files into an sTUIFontFile buffer, initializes stbtt_fontinfo, and
 * reads the units-per-EM value from the head table for DPI-free twips
 * conversion. Supports loading by file path (fontconfig results) or by
 * font name lookup against the discovered font list.
 *
 * @section stb_measurement Measurement
 * Retrieves per-glyph horizontal metrics via stbtt_GetGlyphHMetrics() and
 * vertical metrics via stbtt_GetFontVMetrics(). All values are in design
 * units and converted to twips using the UPEM-based formula.
 *
 * @see cTUISTBTrueTypeFontCalculator
 * @see cTUIFontCalculator
 * @see sTUIFontFile
 * @see sTUIFontInfo
 * @see sTUITextMetrics
 */

#include "stbtruetype.h"
#include "builtinmetrics.h"
#include <algorithm>
#include <fstream>
#include <codecvt>
#include <locale>
#include <cmath>

// STB TrueType implementation (third-party single-header; silence its warnings)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#pragma GCC diagnostic pop

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for STB TrueType font calculator
/// Initializes STB TrueType backend for cross-platform font rendering
///
/////////////////////////////////////////////////////////////////////////////
cTUISTBTrueTypeFontCalculator::cTUISTBTrueTypeFontCalculator(void)
    : mFontInfo(nullptr), mScale(0.0f), mAscent(0), mDescent(0), mLineGap(0) {
    mBackend = FONT_BACKEND_STB_TRUETYPE;
    mMode = FONT_MODE_DOCUMENT;
    mDocumentMode = true;
    
    // Default font settings
    mCurrentFont.name = "Liberation Serif";
    mCurrentFont.size = 12;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for STB TrueType font calculator
/// Ensures proper cleanup of font data and resources
///
/////////////////////////////////////////////////////////////////////////////
cTUISTBTrueTypeFontCalculator::~cTUISTBTrueTypeFontCalculator(void) {
    Shutdown();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initializes the STB TrueType font measurement system
/// Enumerates system fonts and loads a default font for measurements
///
/// @note Falls back to built-in metrics if no system fonts are available
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::Initialize(void) {
    // Enumerate available system fonts
    if (!EnumerateSystemFonts()) {
        // If no system fonts found, fall back to built-in metrics
        return false;
    }
    
    // Try to load a default font
    sTUIFontInfo defaultFont;
    defaultFont.name = "Liberation Serif";
    defaultFont.size = 12;
    
    // Try common fallback fonts if Liberation Serif not found
    std::vector<std::string> fallbackFonts = {
        "Liberation Serif", "Times New Roman", "DejaVu Serif", 
        "Liberation Sans", "Arial", "DejaVu Sans",
        "Liberation Mono", "Courier New", "DejaVu Sans Mono"
    };
    
    bool fontLoaded = false;
    for (const auto& fontName : fallbackFonts) {
        defaultFont.name = fontName;
        if (SetFont(defaultFont)) {
            fontLoaded = true;
            break;
        }
    }
    
    return fontLoaded;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shuts down the STB TrueType font system and cleans up resources
/// Clears font data and resets internal state
///
/////////////////////////////////////////////////////////////////////////////
void cTUISTBTrueTypeFontCalculator::Shutdown(void) {
    mFontInfo.reset();
    mFontData.clear();
    mAvailableFonts.clear();
    mScale = 0.0f;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontInfo& font [in] font information to set
///
/// @return bool [out] true if font was loaded successfully
///
/// @brief
/// Sets the current font using STB TrueType font loading
/// Finds best matching font file and loads it for measurements
///
/// @note Uses font style scoring to find the best match for requested font
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::SetFont(const sTUIFontInfo& font) {
    mCurrentFont = font;

    // If font.name is a file path (contains '/'), load directly from file
    // The font manager passes file paths from fontconfig as fontInfo.name
    if (font.name.find('/') != std::string::npos)
    {
        std::ifstream file(font.name, std::ios::binary);
        if (file)
        {
            file.seekg(0, std::ios::end);
            size_t fileSize = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> data(fileSize);
            file.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(fileSize));

            if (file && LoadFontFromMemory(data))
            {
                mFontData = std::move(data);

                // Convert point size to pixel height at 96 DPI
                float pixelHeight = static_cast<float>(font.size) * 96.0f / 72.0f;
                mScale = stbtt_ScaleForPixelHeight(mFontInfo.get(), pixelHeight);

                // Get font metrics
                stbtt_GetFontVMetrics(mFontInfo.get(), &mAscent, &mDescent, &mLineGap);
                return true;
            }
        }
        // Fall through to family name matching if file load fails
    }

    // Find best matching font file by family name
    sTUIFontFile* fontFile = FindBestFontMatch(font);
    if (!fontFile)
    {
        return false;
    }

    // Load the font data
    if (!LoadFontFromMemory(fontFile->data))
    {
        return false;
    }

    // Calculate scale for requested font size
    // Convert point size to pixel height at 96 DPI
    float pixelHeight = static_cast<float>(font.size) * 96.0f / 72.0f;
    mScale = stbtt_ScaleForPixelHeight(mFontInfo.get(), pixelHeight);

    // Get font metrics
    stbtt_GetFontVMetrics(mFontInfo.get(), &mAscent, &mDescent, &mLineGap);

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of available font family names
///
/// @brief
/// Gets a list of all available font families from STB TrueType enumeration
/// Returns alphabetically sorted list of font family names
///
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of available font family names
///
/// @brief
/// Gets a list of all available font families using STB TrueType
/// Returns font family names from enumerated system fonts
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUISTBTrueTypeFontCalculator::GetAvailableFonts(void) {
    std::vector<std::string> fonts;
    
    for (const auto& pair : mAvailableFonts) {
        fonts.push_back(pair.first);
    }
    
    std::sort(fonts.begin(), fonts.end());
    return fonts;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] text to measure
///
/// @return sTUITextMetrics [out] text measurement metrics in TWIPS
///
/// @brief
/// Measures text dimensions using STB TrueType glyph metrics
/// Converts UTF-8 to UTF-32 and measures each character individually
///
/// @note Falls back to ASCII interpretation if UTF-8 conversion fails
///
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& text [in] UTF-8 text to measure
///
/// @return sTUITextMetrics [out] text measurement metrics in TWIPS
///
/// @brief
/// Measures text dimensions using STB TrueType font metrics
/// Calculates width, height, ascent, descent, and leading values
///
/// @note Handles UTF-8 text conversion and kerning calculations
///
/////////////////////////////////////////////////////////////////////////////
sTUITextMetrics cTUISTBTrueTypeFontCalculator::MeasureText(const std::string& text) {
    sTUITextMetrics metrics;
    
    if (!HasFontLoaded() || text.empty()) {
        return metrics;
    }
    
    // Convert UTF-8 to UTF-32
    std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    std::u32string utf32text;
    
    try {
        utf32text = converter.from_bytes(text);
    } catch (...) {
        // Fallback: treat as ASCII
        utf32text.reserve(text.length());
        for (char c : text) {
            utf32text.push_back(static_cast<char32_t>(c));
        }
    }
    
    // DPI-free conversion: twips = designUnits * pointSize * 20 / UPEM
    // (1 point = 20 twips, no screen resolution involved)
    int upem = GetFontUnitsPerEM();
    int pointSize = mCurrentFont.size;
    long totalDesignUnits = 0;
    char32_t prevCodepoint = 0;

    for (char32_t codepoint : utf32text)
    {
        // Get character advance in font design units
        int advance, leftBearing;
        if (GetGlyphMetrics(codepoint, advance, leftBearing))
        {
            totalDesignUnits += advance;

            // Add kerning if available (also in design units)
            if (prevCodepoint != 0)
            {
                int kerning = GetKerning(prevCodepoint, codepoint);
                totalDesignUnits += kerning;
            }
        }
        else
        {
            // Fallback: use half the UPEM as average character width
            totalDesignUnits += upem / 2;
        }

        prevCodepoint = codepoint;
    }

    metrics.widthTWIPS = static_cast<int>(totalDesignUnits * pointSize * 20 / upem);
    metrics.heightTWIPS = GetLineHeight();
    metrics.ascentTWIPS = static_cast<int>(mAscent * pointSize * 20 / upem);
    metrics.descentTWIPS = static_cast<int>(-mDescent * pointSize * 20 / upem);
    metrics.leadingTWIPS = static_cast<int>(mLineGap * pointSize * 20 / upem);
    
    return metrics;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to measure
///
/// @return int [out] character width in TWIPS
///
/// @brief
/// Measures the width of a single Unicode character
/// Uses STB TrueType glyph metrics for precise width calculation
///
/////////////////////////////////////////////////////////////////////////////
int cTUISTBTrueTypeFontCalculator::MeasureCharacterWidth(char32_t codepoint) {
    if (!HasFontLoaded())
    {
        return 144; // Default character width
    }

    int advance, leftBearing;
    if (GetGlyphMetrics(codepoint, advance, leftBearing))
    {
        // DPI-free conversion: twips = designUnits * pointSize * 20 / UPEM
        int upem = GetFontUnitsPerEM();
        return advance * mCurrentFont.size * 20 / upem;
    }

    // Fallback: half-em width
    return mCurrentFont.size * 10;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return int [out] line height in TWIPS
///
/// @brief
/// Gets the line height for the current font
/// Calculates total line spacing including ascent, descent, and line gap
///
/////////////////////////////////////////////////////////////////////////////
int cTUISTBTrueTypeFontCalculator::GetLineHeight(void) {
    if (!HasFontLoaded())
    {
        return 240; // Default line height
    }

    // DPI-free conversion: twips = designUnits * pointSize * 20 / UPEM
    int upem = GetFontUnitsPerEM();
    int lineHeightDesignUnits = mAscent - mDescent + mLineGap;
    return lineHeightDesignUnits * mCurrentFont.size * 20 / upem;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to test
///
/// @return bool [out] true if character is supported by current font
///
/// @brief
/// Tests whether the current font supports a specific Unicode character
/// Uses STB TrueType glyph finding to determine support
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::SupportsCharacter(char32_t codepoint) {
    if (!HasFontLoaded()) {
        return false;
    }
    
    int glyphIndex = stbtt_FindGlyphIndex(mFontInfo.get(), static_cast<int>(codepoint));
    return glyphIndex != 0;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if system fonts were successfully enumerated
///
/// @brief
/// Enumerates all available system fonts from standard directories
/// Scans platform-specific font directories and builds font database
///
/// @note Platform-specific implementation for Windows, macOS, and Linux
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::EnumerateSystemFonts(void) {
    mAvailableFonts.clear();
    
    // Get system font directories
    std::vector<std::string> fontDirs = GetSystemFontDirectories();
    
    for (const std::string& dir : fontDirs) {
        ScanFontDirectory(dir);
    }
    
    return !mAvailableFonts.empty();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of system font directory paths
///
/// @brief
/// Gets platform-specific system font directory paths
/// Returns standard font locations for Windows, macOS, and Linux
///
/// @note Includes user-specific and system-wide font directories
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUISTBTrueTypeFontCalculator::GetSystemFontDirectories(void) {
    std::vector<std::string> directories;
    
#ifdef _WIN32
    // Windows font directories
    char winDir[MAX_PATH];
    if (GetWindowsDirectoryA(winDir, MAX_PATH)) {
        directories.push_back(std::string(winDir) + "\\Fonts");
    }
    
    // User font directory
    char* localAppData = nullptr;
    size_t len = 0;
    if (_dupenv_s(&localAppData, &len, "LOCALAPPDATA") == 0 && localAppData) {
        directories.push_back(std::string(localAppData) + "\\Microsoft\\Windows\\Fonts");
        free(localAppData);
    }
    
#elif defined(__APPLE__)
    // macOS font directories
    directories.push_back("/System/Library/Fonts");
    directories.push_back("/Library/Fonts");
    
    // User font directory
    const char* home = getenv("HOME");
    if (home) {
        directories.push_back(std::string(home) + "/Library/Fonts");
    }
    
#else
    // Linux font directories
    directories.push_back("/usr/share/fonts");
    directories.push_back("/usr/local/share/fonts");
    directories.push_back("/usr/share/fonts/truetype");
    directories.push_back("/usr/share/fonts/TTF");
    directories.push_back("/usr/share/fonts/opentype");
    directories.push_back("/usr/share/fonts/OTF");
    
    // User font directories
    const char* home = getenv("HOME");
    if (home) {
        directories.push_back(std::string(home) + "/.fonts");
        directories.push_back(std::string(home) + "/.local/share/fonts");
    }
    
    // XDG font directories
    const char* xdgDataHome = getenv("XDG_DATA_HOME");
    if (xdgDataHome) {
        directories.push_back(std::string(xdgDataHome) + "/fonts");
    }
#endif
    
    return directories;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& directory [in] directory path to scan for fonts
///
/// @return nothing
///
/// @brief
/// Recursively scans a directory for font files
/// Processes TTF, OTF, and other supported font formats
///
/// @note Adds discovered fonts to the internal font database
///
/////////////////////////////////////////////////////////////////////////////
void cTUISTBTrueTypeFontCalculator::ScanFontDirectory(const std::string& directory) {
#ifdef _WIN32
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA((directory + "\\*.ttf").c_str(), &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string fontPath = directory + "\\" + findData.cFileName;
            std::vector<sTUIFontFile> fonts;
            if (ParseFontFile(fontPath, fonts)) {
                for (const auto& font : fonts) {
                    mAvailableFonts[font.family] = font;
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
    // Also scan for OTF files
    hFind = FindFirstFileA((directory + "\\*.otf").c_str(), &findData);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::string fontPath = directory + "\\" + findData.cFileName;
            std::vector<sTUIFontFile> fonts;
            if (ParseFontFile(fontPath, fonts)) {
                for (const auto& font : fonts) {
                    mAvailableFonts[font.family] = font;
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
    
#else
    DIR* dir = opendir(directory.c_str());
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;
        std::string extension;
        
        // Get file extension
        size_t dotPos = filename.find_last_of('.');
        if (dotPos != std::string::npos) {
            extension = filename.substr(dotPos + 1);
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
        }
        
        // Check for font file extensions
        if (extension == "ttf" || extension == "otf" || extension == "ttc") {
            std::string fontPath = directory + "/" + filename;
            std::vector<sTUIFontFile> fonts;
            if (ParseFontFile(fontPath, fonts)) {
                for (const auto& font : fonts) {
                    mAvailableFonts[font.family] = font;
                }
            }
        }
    }
    
    closedir(dir);
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& path [in] path to font file
/// @param  std::vector<sTUIFontFile>& fonts [out] vector to store parsed font information
///
/// @return bool [out] true if font file was successfully parsed
///
/// @brief
/// Parses a font file and extracts font family and style information
/// Handles TTF, OTF, and font collection files
///
/// @note Adds all font families found in the file to the fonts vector
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::ParseFontFile(const std::string& path, std::vector<sTUIFontFile>& fonts) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    
    // Get file size
    file.seekg(0, std::ios::end);
    size_t fileSize = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    // Read file data
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    
    if (!file) return false;
    
    // Try to parse font info
    stbtt_fontinfo fontInfo;
    if (stbtt_InitFont(&fontInfo, data.data(), 0) == 0) {
        return false; // Not a valid font file
    }
    
    sTUIFontFile font;
    font.path = path;
    font.data = std::move(data);
    font.family = ExtractFontFamily(font.data);
    font.bold = IsFontBold(font.data);
    font.italic = IsFontItalic(font.data);
    
    if (!font.family.empty()) {
        fonts.push_back(std::move(font));
        return true;
    }
    
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<uint8_t>& fontData [in] raw font file data
///
/// @return std::string [out] extracted font family name
///
/// @brief
/// Extracts the font family name from raw font data
/// Uses STB TrueType to parse font tables and retrieve family name
///
/// @note Returns empty string if family name cannot be extracted
///
/////////////////////////////////////////////////////////////////////////////
std::string cTUISTBTrueTypeFontCalculator::ExtractFontFamily(const std::vector<uint8_t>& fontData) {
    stbtt_fontinfo fontInfo;
    if (stbtt_InitFont(&fontInfo, fontData.data(), 0) == 0) {
        return "";
    }
    
    // Try to get font family name from name table
    int length;
    const char* name = stbtt_GetFontNameString(&fontInfo, &length, STBTT_PLATFORM_ID_MICROSOFT,
                                              STBTT_MS_EID_UNICODE_BMP, STBTT_MS_LANG_ENGLISH, 1);
    
    if (name && length > 0) {
        // Convert from UTF-16 to UTF-8 (simplified - assumes ASCII)
        std::string result;
        for (int i = 0; i < length; i += 2) {
            if (i + 1 < length) {
                char c = name[i + 1]; // Take low byte (assumes ASCII)
                if (c != 0) {
                    result += c;
                }
            }
        }
        return result;
    }
    
    return "";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<uint8_t>& fontData [in] raw font file data
///
/// @return bool [out] true if font is bold style
///
/// @brief
/// Determines if a font has bold weight characteristics
/// Analyzes font metadata to detect bold styling
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::IsFontBold(const std::vector<uint8_t>& fontData) {
    stbtt_fontinfo fontInfo;
    if (stbtt_InitFont(&fontInfo, fontData.data(), 0) == 0) {
        return false;
    }
    
    // Check OS/2 table for weight information
    // This is a simplified check - a full implementation would parse the OS/2 table
    return false; // TODO: Implement proper weight detection
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<uint8_t>& fontData [in] raw font file data
///
/// @return bool [out] true if font is italic style
///
/// @brief
/// Determines if a font has italic style characteristics
/// Analyzes font metadata to detect italic styling
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::IsFontItalic(const std::vector<uint8_t>& fontData) {
    stbtt_fontinfo fontInfo;
    if (stbtt_InitFont(&fontInfo, fontData.data(), 0) == 0) {
        return false;
    }
    
    // Check head table for italic flag
    // This is a simplified check - a full implementation would parse the head table
    return false; // TODO: Implement proper italic detection
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::vector<uint8_t>& data [in] font data to load
///
/// @return bool [out] true if font was loaded successfully
///
/// @brief
/// Loads a font from memory data using STB TrueType
/// Initializes font info and calculates scaling metrics
///
/// @note Sets up font metrics for subsequent text measurements
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::LoadFontFromMemory(const std::vector<uint8_t>& data) {
    mFontData = data;
    mFontInfo = std::make_unique<stbtt_fontinfo>();
    
    if (stbtt_InitFont(mFontInfo.get(), mFontData.data(), 0) == 0) {
        mFontInfo.reset();
        mFontData.clear();
        return false;
    }
    
    return true;
}

sTUIFontFile* cTUISTBTrueTypeFontCalculator::FindBestFontMatch(const sTUIFontInfo& requestedFont) {
    if (mAvailableFonts.empty()) {
        return nullptr;
    }
    
    // Look for exact family name match first
    auto it = mAvailableFonts.find(requestedFont.name);
    if (it != mAvailableFonts.end()) {
        return &it->second;
    }
    
    // Find best match by scoring
    sTUIFontFile* bestMatch = nullptr;
    int bestScore = -1;
    
    for (auto& pair : mAvailableFonts) {
        int score = GetFontStyleScore(pair.second, requestedFont);
        if (score > bestScore) {
            bestScore = score;
            bestMatch = &pair.second;
        }
    }
    
    return bestMatch;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontFile& font [in] available font file to score
/// @param  const sTUIFontInfo& requested [in] requested font characteristics
///
/// @return int [out] style matching score (higher is better match)
///
/// @brief
/// Calculates a style matching score between available and requested fonts
/// Considers family name similarity, weight, and style attributes
///
/// @note Used for font substitution when exact matches are not available
///
/////////////////////////////////////////////////////////////////////////////
int cTUISTBTrueTypeFontCalculator::GetFontStyleScore(const sTUIFontFile& font, const sTUIFontInfo& requested) {
    int score = 0;
    
    // Family name similarity (case-insensitive)
    std::string fontName = font.family;
    std::string requestedName = requested.name;
    std::transform(fontName.begin(), fontName.end(), fontName.begin(), ::tolower);
    std::transform(requestedName.begin(), requestedName.end(), requestedName.begin(), ::tolower);
    
    if (fontName.find(requestedName) != std::string::npos ||
        requestedName.find(fontName) != std::string::npos) {
        score += 100;
    }
    
    // Style matching
    if (font.bold == requested.bold) score += 50;
    if (font.italic == requested.italic) score += 50;
    
    return score;
}

float cTUISTBTrueTypeFontCalculator::PixelsToTWIPS(float pixels, float dpi) const {
    return pixels * 1440.0f / dpi;
}

float cTUISTBTrueTypeFontCalculator::TWIPSToPixels(float twips, float dpi) const {
    return twips * dpi / 1440.0f;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return int [out] units per EM for the loaded font (typically 2048 or 1000)
///
/// @brief
/// Gets the units-per-EM value from the font's head table
/// This is the proper denominator for converting font design units to TWIPS
///
/////////////////////////////////////////////////////////////////////////////
int cTUISTBTrueTypeFontCalculator::GetFontUnitsPerEM(void) const {
    if (!HasFontLoaded())
    {
        return 2048;
    }
    // STB stores head table offset in mFontInfo->head
    // unitsPerEm is at offset 18 in the head table (unsigned short, big-endian)
    const unsigned char* data = mFontInfo->data + mFontInfo->head + 18;
    return (data[0] << 8) | data[1];
}

bool cTUISTBTrueTypeFontCalculator::HasFontLoaded(void) const {
    return mFontInfo != nullptr;
}

std::string cTUISTBTrueTypeFontCalculator::GetLoadedFontFamily(void) const {
    // TODO: Extract actual font family from loaded font
    return mCurrentFont.name;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t left [in] left character codepoint
/// @param  char32_t right [in] right character codepoint
///
/// @return int [out] kerning adjustment in font units
///
/// @brief
/// Gets kerning adjustment between two characters
/// Uses STB TrueType kerning tables for precise character spacing
///
/// @note Returns 0 if no kerning adjustment is needed
///
/////////////////////////////////////////////////////////////////////////////
int cTUISTBTrueTypeFontCalculator::GetKerning(char32_t left, char32_t right) {
    if (!HasFontLoaded()) {
        return 0;
    }
    
    int leftGlyph = stbtt_FindGlyphIndex(mFontInfo.get(), static_cast<int>(left));
    int rightGlyph = stbtt_FindGlyphIndex(mFontInfo.get(), static_cast<int>(right));
    
    return stbtt_GetGlyphKernAdvance(mFontInfo.get(), leftGlyph, rightGlyph);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char32_t codepoint [in] Unicode codepoint to get metrics for
/// @param  int& advance [out] horizontal advance width in font units
/// @param  int& leftBearing [out] left side bearing in font units
///
/// @return bool [out] true if glyph metrics were retrieved successfully
///
/// @brief
/// Gets detailed glyph metrics for a specific character
/// Retrieves advance width and bearing information from STB TrueType
///
/// @note Used internally for precise character width calculations
///
/////////////////////////////////////////////////////////////////////////////
bool cTUISTBTrueTypeFontCalculator::GetGlyphMetrics(char32_t codepoint, int& advance, int& leftBearing) {
    if (!HasFontLoaded()) {
        return false;
    }
    
    int glyphIndex = stbtt_FindGlyphIndex(mFontInfo.get(), static_cast<int>(codepoint));
    if (glyphIndex == 0) {
        return false; // Glyph not found
    }
    
    stbtt_GetGlyphHMetrics(mFontInfo.get(), glyphIndex, &advance, &leftBearing);
    return true;
}