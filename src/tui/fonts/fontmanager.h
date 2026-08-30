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

#ifndef TUI_FONTMANAGER_H
#define TUI_FONTMANAGER_H

#include "tuifontcalc.h"
#include <memory>
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <atomic>

// Font selection criteria
struct sTUIFontSelectionCriteria {
    std::string family;
    int size;
    bool bold;
    bool italic;
    bool underline;
    bool monospace;

    sTUIFontSelectionCriteria() : size(12), bold(false), italic(false), underline(false), monospace(false) {}
};

// Font enumeration result
struct sTUIAvailableFont {
    std::string family;
    std::string fullName;
    bool bold;
    bool italic;
    bool monospace;
    eTUIFontMeasurementBackend backend;

    sTUIAvailableFont() : bold(false), italic(false), monospace(false), backend(FONT_BACKEND_BUILT_IN_METRICS) {}
};

// TUI Font Manager - high-level font management interface
class cTUIFontManager {

    // =================================================================
    // METHODS
    // =================================================================

public:
    /**
     * @brief Constructor - creates font manager instance
     */
    cTUIFontManager(void);

    /**
     * @brief Destructor - cleans up font resources
     */
    ~cTUIFontManager(void);

    // Initialization
    /**
     * @brief Initializes the font system and discovers available fonts
     * @param documentMode True for document mode (high precision), false for terminal mode
     * @return True if initialization successful
     * @see mDocumentMode for mode differences
     */
    bool Initialize(bool documentMode = true);

    /**
     * @brief Shuts down font system and releases resources
     */
    void Shutdown(void);

    // Background font enumeration
    /**
     * @brief Check if background font enumeration has completed
     * @return True if all system fonts have been discovered
     */
    bool IsFontEnumerationDone(void) const;

    /**
     * @brief Block until background font enumeration finishes
     */
    void WaitForFontEnumeration(void);

    // Font selection
    /**
     * @brief Sets current font using detailed selection criteria
     * @param criteria Font selection parameters including family, size, style
     * @return True if font was successfully set
     * @see sTUIFontSelectionCriteria for available options
     */
    bool SetFont(const sTUIFontSelectionCriteria& criteria);

    /**
     * @brief Sets current font using simple parameters
     * @param family Font family name (e.g., "Arial", "Courier New")
     * @param size Font size in points
     * @param bold Whether font should be bold
     * @param italic Whether font should be italic
     * @return True if font was successfully set
     */
    bool SetFont(const std::string& family, int size, bool bold = false, bool italic = false);
    sTUIFontInfo GetCurrentFont(void) const;

    // Font enumeration
    std::vector<sTUIAvailableFont> GetAvailableFonts(void) const;
    std::vector<sTUIAvailableFontInfo> GetAvailableFontDetails(void) const; // New detailed font info
    std::vector<std::string> GetFontFamilies(void) const;
    std::vector<sTUIAvailableFont> GetFontsByFamily(const std::string& family) const;

    // Font categories
    std::vector<sTUIAvailableFont> GetMonospaceFonts(void) const;
    std::vector<sTUIAvailableFont> GetProportionalFonts(void) const;
    std::vector<sTUIAvailableFont> GetSystemFonts(void) const;
    std::vector<sTUIAvailableFont> GetBuiltInFonts(void) const;

    // Text measurement (delegates to current calculator)
    sTUITextMetrics MeasureText(const std::string& text);
    int MeasureCharacterWidth(char32_t codepoint);
    int GetLineHeight(void);

    // Mode switching
    void SetDocumentMode(bool documentMode);
    bool IsDocumentMode(void) const;

    // Backend information
    eTUIFontMeasurementBackend GetCurrentBackend(void) const;
    std::string GetBackendName(void) const;
    std::vector<eTUIFontMeasurementBackend> GetAvailableBackends(void) const;

    // Font validation
    bool IsValidFont(const std::string& family, int size) const;
    bool SupportsCharacter(char32_t codepoint);
    bool SupportsUnicode(void) const;

    // Font information
    std::string GetFontNameUsedForCharacter(char32_t codepoint) const;

    // Font style matching
    std::vector<sTUIAvailableFontInfo> FindFontsByStyle(eTUIFontStyleHint styleHint) const;
    std::string FindBestStyleMatch(const std::string& requestedFont, eTUIFontStyleHint preferredStyle) const;

    // Utility methods
    static std::string GetDefaultFontFamily(bool monospace = false);
    static int GetDefaultFontSize(void);
    static std::vector<int> GetCommonFontSizes(void);

    // Font preview and information
    std::string GetFontSampleText(const std::string& family) const;
    sTUITextMetrics GetFontMetrics(const std::string& family, int size);

private:
    // Font enumeration
    void EnumerateAllFonts(void);
    void AddBuiltInFonts(void);
    void AddSystemFonts(void);
    std::vector<std::string> DiscoverSystemFontFiles(void);
    std::string ExtractFontFamilyName(const std::string& fontPath);

    // Font discovery methods
    struct FontInfo {
        std::string family;
        std::string style;
        std::string filepath;
        int spacing = 0; // FC_MONO = 100, FC_DUAL = 90, FC_PROPORTIONAL = 0
    };
    std::vector<FontInfo> DiscoverSystemFonts(void);
    std::vector<FontInfo> DiscoverSystemFontsByScanning(void);

    // Font matching
    const sTUIAvailableFont* FindBestFontMatch(const sTUIFontSelectionCriteria& criteria);
    int CalculateFontScore(const sTUIAvailableFont& font, const sTUIFontSelectionCriteria& criteria);

    // Helper methods
    bool CreateFontCalculator(eTUIFontMeasurementBackend preferredBackend);
    void RefreshFontList(void);
    std::string NormalizeFontName(const std::string& name) const;

    // Font fallback helpers
    bool HasNativeFontFallback(void) const;
    std::string FindFallbackFontForCharacter(char32_t codepoint) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    std::unique_ptr<cTUIFontCalculator> mCurrentCalculator;
    // std::deque (not std::vector) so the raw pointers cached in
    // mFontMatchCache stay valid when the background enumeration thread
    // push_back-s system fonts -- vector would reallocate on growth.
    std::deque<sTUIAvailableFont> mAvailableFonts;
    sTUIFontInfo mCurrentFont;
    bool mDocumentMode;
    bool mInitialized = false;
    eTUIFontMeasurementBackend mCurrentBackend;

    // Font fallback cache
    mutable std::map<char32_t, std::string> mFallbackCache;
    mutable std::map<std::string, std::unique_ptr<cTUIFontCalculator>> mFallbackCalculators;

    // Background font enumeration threading
    std::thread mEnumThread;
    mutable std::mutex mFontMutex;
    std::atomic<bool> mEnumDone{true};

    // Font matching cache
    std::unordered_map<std::string, const sTUIAvailableFont*> mFontMatchCache;
};

#endif // TUI_FONTMANAGER_H
