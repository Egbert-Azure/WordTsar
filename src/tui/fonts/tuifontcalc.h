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

#ifndef TUIFONTCALC_H
#define TUIFONTCALC_H

#include <memory>
#include <string>
#include <vector>

// Font measurement backend types
enum eTUIFontMeasurementBackend {
    FONT_BACKEND_DIRECTWRITE,       // Windows DirectWrite (preferred)
    FONT_BACKEND_WINDOWS_NATIVE,    // Windows GDI (headless, fallback)
    FONT_BACKEND_MACOS_NATIVE,      // macOS CoreText (headless)  
    FONT_BACKEND_QT_HEADLESS,       // Linux with Qt available
    FONT_BACKEND_HARFBUZZ,          // Linux with HarfBuzz available
    FONT_BACKEND_STB_TRUETYPE,      // Cross-platform with TTF fonts
    FONT_BACKEND_BUILT_IN_METRICS   // Ultimate fallback with built-in data
};

// Font measurement modes
enum eTUIFontMeasurementMode {
    FONT_MODE_TERMINAL,     // wcwidth() for non-document mode (programming files)
    FONT_MODE_DOCUMENT,     // TWIPS precision for document mode (word processing)  
    FONT_MODE_PRINT         // High precision for print preview/output
};

// Font information structure
struct sTUIFontInfo {
    std::string name;
    int size;
    bool bold;
    bool italic;
    bool underline;
    int weight;
    
    sTUIFontInfo() : size(12), bold(false), italic(false), underline(false), weight(400) {}
};

// Font style classifications for matching
enum eTUIFontStyleHint {
    FONT_STYLE_SERIF,
    FONT_STYLE_SANS_SERIF,
    FONT_STYLE_MONOSPACE,
    FONT_STYLE_CURSIVE,
    FONT_STYLE_FANTASY,
    FONT_STYLE_SYSTEM_UI
};

enum eTUIFontSpacing {
    FONT_SPACING_PROPORTIONAL,
    FONT_SPACING_MONOSPACE,
    FONT_SPACING_DUAL_WIDTH
};

// Unified font information for font selection dialogs
struct sTUIAvailableFontInfo {
    std::string displayName;      // Human-readable name for UI
    std::string familyName;       // System font family name
    std::string filePath;         // Font file path (if available)
    eTUIFontStyleHint styleHint;     // Style classification
    eTUIFontSpacing spacing;         // Spacing type
    bool isSerif;                 // Has serifs
    bool isSansSerif;             // Sans-serif font
    bool isMonospace;             // Fixed-width font
    std::vector<int> availableSizes; // For bitmap fonts
    eTUIFontMeasurementBackend backend; // Which backend can handle this font
    
    sTUIAvailableFontInfo() : styleHint(FONT_STYLE_SERIF), spacing(FONT_SPACING_PROPORTIONAL), 
                          isSerif(false), isSansSerif(false), isMonospace(false), 
                          backend(FONT_BACKEND_BUILT_IN_METRICS) {}
};

// Text measurement result
struct sTUITextMetrics {
    int widthTWIPS;      // Width in TWIPS (1/1440 inch)
    int heightTWIPS;     // Height in TWIPS
    int ascentTWIPS;     // Ascent in TWIPS
    int descentTWIPS;    // Descent in TWIPS
    int leadingTWIPS;    // Line spacing in TWIPS
    
    sTUITextMetrics() : widthTWIPS(0), heightTWIPS(0), ascentTWIPS(0), descentTWIPS(0), leadingTWIPS(0) {}
};

// Base font measurement interface
class cTUIFontCalculator
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    virtual ~cTUIFontCalculator(void) = default;

    // Initialization
    virtual bool Initialize(void) = 0;
    virtual void Shutdown(void) = 0;

    // Font selection
    virtual bool SetFont(const sTUIFontInfo& font) = 0;
    virtual sTUIFontInfo GetCurrentFont(void) const;
    virtual std::vector<std::string> GetAvailableFonts(void) = 0;

    // Text measurement
    virtual sTUITextMetrics MeasureText(const std::string& text) = 0;
    virtual int MeasureCharacterWidth(char32_t codepoint) = 0;
    virtual int GetLineHeight(void) = 0;

    // Mode switching
    void SetDocumentMode(bool docMode);

    bool IsDocumentMode(void) const;
    eTUIFontMeasurementBackend GetBackend(void) const;
    eTUIFontMeasurementMode GetMode(void) const;

    // Unicode support
    virtual bool SupportsUnicode(void) const = 0;
    virtual bool SupportsCharacter(char32_t codepoint) = 0;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
protected:
    eTUIFontMeasurementMode mMode;
    eTUIFontMeasurementBackend mBackend;
    bool mDocumentMode;
    sTUIFontInfo mCurrentFont;
};

// Font measurement manager - detects best available backend
class cTUIFontMeasurementManager {
public:
    static eTUIFontMeasurementBackend DetectBestBackend(void);
    static bool HasQtLibraries(void);
    static bool HasHarfBuzzLibraries(void);
    static bool HasSystemFonts(void);
    
    // Factory method
    static std::unique_ptr<cTUIFontCalculator> CreateFontCalculator(bool documentMode = true);
};

// Platform-specific implementations (forward declarations)
class cTUIFontCalculatorDirectWrite;
class cTUIFontCalculatorWindowsNative;
class cTUIFontCalculatorMacOSNative;
class cTUIFontCalculatorQtHeadless;
class cTUIFontCalculatorHarfBuzz;
class cTUISTBTrueTypeFontCalculator;
class cTUIBuiltInMetricsFontCalculator;

#endif // TUIFONTCALC_H