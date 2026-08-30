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
 * @class cSpellChecker
 *
 * @brief Platform-specific spell checking implementation.
 *
 * Implements the cSpellChecker class with three platform backends providing
 * word checking, spelling suggestions, and dictionary word addition through
 * a single unified interface.
 *
 * @section spellcheck_windows Windows Backend
 * Uses the COM ISpellChecker API (Windows 8+) with UTF-8 to wide-string
 * conversion. Initializes COM via CoInitializeEx(), creates a spell checker
 * for the specified language tag, and queries ISpellingError results for
 * suggestions. Supports adding words to the user dictionary.
 *
 * @section spellcheck_macos macOS Backend
 * Delegates to NSSpellChecker via Objective-C helper functions declared
 * externally (macos_spell_check, macos_spell_suggest, macos_spell_add_word).
 * These helpers bridge the Objective-C NSSpellChecker API to C++ strings.
 *
 * @section spellcheck_linux Linux Backend
 * Uses the Hunspell library with dictionaries located in /usr/share/hunspell.
 * Searches for .aff and .dic files matching the requested language code
 * (e.g., en_US). Falls back to any available dictionary if the requested
 * language is not found. Supports spell checking, suggestions, and adding
 * words to the runtime dictionary.
 *
 * @section spellcheck_language Language Configuration
 * The constructor accepts a language parameter (default "en_US") that
 * selects the appropriate dictionary on all platforms. The language code
 * follows BCP 47 format (e.g., "en_US", "fr_FR", "de_DE").
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cSpellChecker Spell checker class
 */

#include <iostream>

#include "spellcheck.h"

#ifdef _WIN32
  #include <windows.h>
  #include <spellcheck.h>
  #include <objidl.h>
  #include <comdef.h>
  #include <combaseapi.h>
//  #pragma comment(lib, "spellcheck.lib")  // Link against Spellcheck.lib

  // Helper functions for UTF-8 conversion.
  std::wstring utf8_to_wstring(const std::string &str) {
      if (str.empty()) return std::wstring();
      int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), nullptr, 0);
      std::wstring wstr(size_needed, 0);
      MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.size(), &wstr[0], size_needed);
      return wstr;
  }

  std::string wstring_to_utf8(const std::wstring &wstr) {
      if (wstr.empty()) return std::string();
      int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
      std::string str(size_needed, 0);
      WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
      return str;
  }

#elif defined(__APPLE__)

  // Declare external C functions implemented in SpellCheckerMac.mm.
  extern "C" {
      bool macCheckSpelling(const char *word);
      char** macGetSuggestions(const char *word, int *count);
      void macFreeSuggestions(char** suggestions, int count);
      bool macAddWord(const char *word);  // New helper for adding a word.
  }

#else

  // Linux: use Hunspell.
  #include <hunspell/hunspell.hxx>

#endif

// ---------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Initializes the platform-specific spell checker.
/// Windows: Creates COM SpellCheckerFactory and spell checker for "en-US".
/// macOS: No initialization needed (uses NSSpellChecker).
/// Linux: Creates Hunspell instance with en_US dictionary.
///
/////////////////////////////////////////////////////////////////////////////
cSpellChecker::cSpellChecker(const std::string& language)
    : mLanguage(language)
{
#ifdef _WIN32

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        std::cerr << "CoInitializeEx failed: " << std::hex << hr << std::endl;
    }
    // Explicitly request IID_ISpellCheckerFactory.
    hr = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pSpellCheckerFactory));
//    hr = CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER,
//        IID_ISpellCheckerFactory, (void**)&pSpellCheckerFactory);
    if (FAILED(hr))
    {
        std::cerr << "CoCreateInstance for SpellCheckerFactory failed: " << std::hex << hr << std::endl;
        pSpellCheckerFactory = nullptr;
    }
    if (pSpellCheckerFactory)
    {
        ISpellCheckerFactory* factory = reinterpret_cast<ISpellCheckerFactory*>(pSpellCheckerFactory);
        // Convert language to wide string with underscore to hyphen (Windows uses "en-US" format)
        std::string winLang = mLanguage;
        for (auto &ch : winLang)
        {
            if (ch == '_')
            {
                ch = '-';
            }
        }
        std::wstring wLang = utf8_to_wstring(winLang);
        hr = factory->CreateSpellChecker(wLang.c_str(), (ISpellChecker**)&pSpellChecker);
        if (FAILED(hr))
        {
            std::cerr << "CreateSpellChecker failed: " << std::hex << hr << std::endl;
            pSpellChecker = nullptr;
        }
    }

#elif defined(__APPLE__)

    // On macOS, no initialization is needed here.

#else

    // Linux: Initialize Hunspell using configured language for dictionary paths
    // Hunspell handles missing files internally (prints to stderr) and does not throw
    std::string affPath = "/usr/share/hunspell/" + mLanguage + ".aff";
    std::string dicPath = "/usr/share/hunspell/" + mLanguage + ".dic";
    hunspell = new Hunspell(affPath.c_str(), dicPath.c_str());
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor. Releases platform-specific spell checker resources.
/// Windows: Releases COM objects and calls CoUninitialize.
/// macOS: No cleanup needed.
/// Linux: Deletes Hunspell instance.
///
/////////////////////////////////////////////////////////////////////////////
cSpellChecker::~cSpellChecker(void)
{

#ifdef _WIN32
    if (pSpellChecker) 
    {
        reinterpret_cast<ISpellChecker*>(pSpellChecker)->Release();
        pSpellChecker = nullptr;
    }
    if (pSpellCheckerFactory) 
    {
        reinterpret_cast<ISpellCheckerFactory*>(pSpellCheckerFactory)->Release();
        pSpellCheckerFactory = nullptr;
    }
    CoUninitialize();

#elif defined(__APPLE__)

    // Nothing to clean up for macOS.

#else

    delete reinterpret_cast<Hunspell*>(hunspell);
#endif
}

// ---------------------------------------------------------------------
// Spell-check and Suggestion Functions
// ---------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string &word [in] word to check
///
/// @return true if the word is spelled correctly, false otherwise
///
/// @brief
/// Check if a word is spelled correctly using the platform spell checker.
///
/////////////////////////////////////////////////////////////////////////////
bool cSpellChecker::CheckWord(const std::string &word)
{

#ifdef _WIN32
    if (!pSpellChecker)
    {
        return false;
    }

    std::wstring wword = utf8_to_wstring(word);
    IEnumSpellingError* pErrors = nullptr;
    HRESULT hr = reinterpret_cast<ISpellChecker*>(pSpellChecker)->Check(wword.c_str(), &pErrors);
    if (FAILED(hr) || !pErrors) 
    {
        return false;
    }
    ISpellingError* pError = nullptr;
    
    // Next() returns S_OK if an error is found, S_FALSE if no errors.
    hr = pErrors->Next(&pError);
    if (hr == S_OK && pError != nullptr) 
    {
        pError->Release();
        pErrors->Release();
        return false; // Word is misspelled.
    }
    pErrors->Release();
    return true;

#elif defined(__APPLE__)

    // Call the macOS helper function.
    return macCheckSpelling(word.c_str());

#else
    return reinterpret_cast<Hunspell*>(hunspell)->spell(word) != 0;

#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string &word [in] misspelled word to get suggestions for
///
/// @return vector of suggested correct spellings
///
/// @brief
/// Get spelling suggestions for a misspelled word from the platform
/// spell checker.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cSpellChecker::suggestions(const std::string &word)
{
    std::vector<std::string> result;

#ifdef _WIN32
    if (!pSpellChecker)
    {
        return result;
    }

    std::wstring wword = utf8_to_wstring(word);
    IEnumString* pEnumString = nullptr;
    HRESULT hr = reinterpret_cast<ISpellChecker*>(pSpellChecker)->Suggest(wword.c_str(), &pEnumString);
    if (FAILED(hr) || !pEnumString) 
    {
        return result;
    }
    LPOLESTR pSuggestion = nullptr;
    
    // Loop until Next() does not return S_OK.
    while (pEnumString->Next(1, &pSuggestion, nullptr) == S_OK && pSuggestion) 
    {
        std::wstring wsuggestion(pSuggestion);
        result.push_back(wstring_to_utf8(wsuggestion));
        CoTaskMemFree(pSuggestion);
    }
    pEnumString->Release();
    return result;

#elif defined(__APPLE__)

    int count = 0;
    char **suggestionsC = macGetSuggestions(word.c_str(), &count);
    for (int i = 0; i < count; ++i) 
    {
        result.push_back(suggestionsC[i]);
    }
    macFreeSuggestions(suggestionsC, count);

#else

    std::vector<std::string> sugs = reinterpret_cast<Hunspell*>(hunspell)->suggest(word);
    result.insert(result.end(), sugs.begin(), sugs.end());
#endif

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string &word [in] word to add to the dictionary
///
/// @return true if the word was added successfully
///
/// @brief
/// Add a word to the spell checker dictionary so it is no longer
/// flagged as misspelled.
/// Windows: Uses the Ignore() method to suppress the word.
/// macOS: Calls macAddWord() helper.
/// Linux: Calls Hunspell add().
///
/////////////////////////////////////////////////////////////////////////////
bool cSpellChecker::AddWord(const std::string &word)
{
#ifdef _WIN32
    if (!pSpellChecker)
        return false;
    // Use the temporary Ignore() method.
    std::wstring wword = utf8_to_wstring(word);
    HRESULT hr = reinterpret_cast<ISpellChecker*>(pSpellChecker)->Ignore(wword.c_str());
    return SUCCEEDED(hr);
#elif defined(__APPLE__)
    return macAddWord(word.c_str());
#else
    if (!hunspell)
        return false;
    reinterpret_cast<Hunspell*>(hunspell)->add(word.c_str());
    return true;
#endif
}