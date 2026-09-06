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

#import <Foundation/Foundation.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <algorithm>

namespace
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  language [in] UTF-8 encoded language identifier (e.g. "en_US")
///
/// @return the cached CFLocaleRef for this thread and language; owned by
///         the cache, never released by the caller
///
/// @brief
/// Layout can run on a background thread (see cLayoutBase's incremental
/// background layout), so the cache is thread_local -- each thread gets
/// its own locale, no locking needed. Recreated only when the requested
/// language changes, so a paragraph with several unbreakable words pays
/// CFLocaleCreate() once instead of once per word.
///
/////////////////////////////////////////////////////////////////////////////
CFLocaleRef CachedHyphenationLocale(const char *language)
{
    static thread_local CFLocaleRef sLocale = nullptr ;
    static thread_local std::string sLanguage ;

    std::string requested = (language != nullptr && language[0] != '\0') ? language : "en_US" ;
    if (sLocale != nullptr && sLanguage == requested)
    {
        return sLocale ;
    }

    if (sLocale != nullptr)
    {
        CFRelease(sLocale) ;
        sLocale = nullptr ;
    }

    NSString *nsLanguage = [NSString stringWithUTF8String:requested.c_str()] ;
    sLocale = CFLocaleCreate(kCFAllocatorDefault, (__bridge CFStringRef)nsLanguage) ;
    sLanguage = requested ;
    return sLocale ;
}

} // anonymous namespace


extern "C"
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  language [in] UTF-8 encoded language identifier (e.g. "en_US")
///
/// @return true if macOS has a hyphenation dictionary installed for the
///         language
///
/// @brief
/// Checks whether CoreFoundation can hyphenate the given locale, the same
/// dictionaries system apps (TextEdit, Pages) use.
///
/////////////////////////////////////////////////////////////////////////////
bool macHyphenationAvailable(const char *language)
{
    @autoreleasepool
    {
        CFLocaleRef locale = CachedHyphenationLocale(language) ;
        if (locale == nullptr)
        {
            return false ;
        }
        return CFStringIsHyphenationAvailableForLocale(locale) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  word     [in]  UTF-8 encoded word to find hyphenation points in
/// @param  language [in]  UTF-8 encoded language identifier (e.g. "en_US")
/// @param  count    [out] number of points returned
///
/// @return array of ascending Unicode-codepoint offsets (from the start of
///         the word) where a hyphen may be inserted, or nullptr if none.
///         Caller must free the array with macFreeHyphenationPoints().
///
/// @brief
/// Walks CFStringGetHyphenationLocationBeforeIndex() backward from the end
/// of the word, collecting every candidate break point the system
/// hyphenation dictionary offers.
///
/////////////////////////////////////////////////////////////////////////////
int* macHyphenationPoints(const char *word, const char *language, int *count)
{
    @autoreleasepool
    {
        *count = 0 ;

        if (word == nullptr || word[0] == '\0')
        {
            return nullptr ;
        }

        NSString *nsWord = [NSString stringWithUTF8String:word] ;

        CFLocaleRef locale = CachedHyphenationLocale(language) ;
        if (locale == nullptr)
        {
            return nullptr ;
        }

        CFStringRef cfWord = (__bridge CFStringRef)nsWord ;
        CFIndex length = CFStringGetLength(cfWord) ;

        std::vector<int> points ;
        CFIndex searchBefore = length ;
        while (searchBefore > 0)
        {
            UTF32Char hyphenChar = 0 ;
            CFIndex hyphenAt = CFStringGetHyphenationLocationBeforeIndex(
                cfWord, searchBefore, CFRangeMake(0, length), 0, locale, &hyphenChar) ;

            if (hyphenAt == kCFNotFound || hyphenAt <= 0 || hyphenAt >= searchBefore)
            {
                break ;
            }

            points.push_back(static_cast<int>(hyphenAt)) ;
            searchBefore = hyphenAt ;
        }

        // locale is owned by the thread-local cache -- not released here.

        if (points.empty())
        {
            return nullptr ;
        }

        // Collected from the end backward -- flip to ascending order.
        std::reverse(points.begin(), points.end()) ;

        *count = static_cast<int>(points.size()) ;
        int *result = (int*)malloc(sizeof(int) * (*count)) ;
        for (int i = 0 ; i < *count ; i++)
        {
            result[i] = points[i] ;
        }
        return result ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  points [in] array returned by macHyphenationPoints()
///
/// @return nothing
///
/// @brief
/// Frees the points array allocated by macHyphenationPoints().
///
/////////////////////////////////////////////////////////////////////////////
void macFreeHyphenationPoints(int *points)
{
    free(points) ;
}


} // extern "C"
