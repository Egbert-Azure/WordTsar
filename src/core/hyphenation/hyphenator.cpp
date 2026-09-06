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
 * @class cHyphenator
 *
 * @brief Platform-specific dictionary hyphenation lookup.
 *
 * @section hyphenator_macos macOS Backend
 * Delegates to CoreFoundation's CFStringGetHyphenationLocationBeforeIndex()
 * via Objective-C++ helper functions declared externally (macHyphenationPoints,
 * macHyphenationAvailable), implemented in hyphenator_mac.mm -- same split as
 * cSpellChecker's macCheckSpelling()/macGetSuggestions().
 *
 * @section hyphenator_other Other Platforms
 * WordTsar currently ships macOS only (see README); other platforms return
 * no candidates rather than guessing at hyphenation without a dictionary.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cHyphenator Hyphenation lookup class
 */

#include "hyphenator.h"

#ifdef __APPLE__
extern "C"
{
    bool macHyphenationAvailable(const char *language) ;
    int* macHyphenationPoints(const char *word, const char *language, int *count) ;
    void macFreeHyphenationPoints(int *points) ;
}
#endif

cHyphenator::cHyphenator(const std::string& language)
    : mLanguage(language)
{
}

std::vector<size_t> cHyphenator::HyphenationPoints(const std::string& word) const
{
    std::vector<size_t> result ;

#ifdef __APPLE__
    int count = 0 ;
    int *points = macHyphenationPoints(word.c_str(), mLanguage.c_str(), &count) ;
    for (int i = 0 ; i < count ; i++)
    {
        result.push_back(static_cast<size_t>(points[i])) ;
    }
    macFreeHyphenationPoints(points) ;
#else
    (void)word ;
#endif

    return result ;
}
