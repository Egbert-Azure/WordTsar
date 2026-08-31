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
#import <AppKit/AppKit.h>
#include <stdlib.h>

extern "C"
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  language [in] UTF-8 encoded language identifier (e.g. "en_US", "de_DE")
///
/// @return true if the shared spell checker recognized and applied the language
///
/// @brief
/// Sets the language the macOS NSSpellChecker checks against. Applies to all
/// subsequent macCheckSpelling()/macGetSuggestions() calls, since they share
/// the same NSSpellChecker instance. If the language isn't one of
/// NSSpellChecker's installed dictionaries, this is a no-op and the checker
/// keeps whatever language it already had.
///
/////////////////////////////////////////////////////////////////////////////
bool macSetSpellingLanguage(const char *language)
{
    @autoreleasepool
    {
        if (language == nullptr || language[0] == '\0')
        {
            return false;
        }
        NSString *nsLanguage = [NSString stringWithUTF8String:language];
        return [[NSSpellChecker sharedSpellChecker] setLanguage:nsLanguage];
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  word [in] UTF-8 encoded word to check
///
/// @return true if the word is spelled correctly, false otherwise
///
/// @brief
/// Checks if a word is spelled correctly using the macOS NSSpellChecker.
///
/////////////////////////////////////////////////////////////////////////////
bool macCheckSpelling(const char *word)
{
    @autoreleasepool
    {
        NSString *nsWord = [NSString stringWithUTF8String:word];
        NSRange range = [[NSSpellChecker sharedSpellChecker] checkSpellingOfString:nsWord startingAt:0];
        return (range.location == NSNotFound);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  word [in] UTF-8 encoded misspelled word
/// @param  count [out] number of suggestions returned
///
/// @return array of C strings with suggestions, or nullptr if none
///
/// @brief
/// Returns spelling suggestions for a word using macOS NSSpellChecker.
/// The caller is responsible for freeing the returned array using
/// macFreeSuggestions().
///
/////////////////////////////////////////////////////////////////////////////
char** macGetSuggestions(const char *word, int *count)
{
    @autoreleasepool
    {
        NSString *nsWord = [NSString stringWithUTF8String:word];
        NSArray *suggestions = [[NSSpellChecker sharedSpellChecker] guessesForWord:nsWord];
        if (!suggestions)
        {
            *count = 0;
            return nullptr;
        }
        *count = (int)[suggestions count];

        // Allocate an array of C strings.
        char **result = (char**)malloc(sizeof(char*) * (*count));
        for (int i = 0; i < *count; i++)
        {
            NSString *suggestion = [suggestions objectAtIndex:i];
            const char *cStr = [suggestion UTF8String];

            // Duplicate the string for later free.
            result[i] = strdup(cStr);
        }
        return result;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  suggestions [in] array of C strings to free
/// @param  count [in] number of strings in the array
///
/// @return nothing
///
/// @brief
/// Frees the suggestions array allocated by macGetSuggestions().
///
/////////////////////////////////////////////////////////////////////////////
void macFreeSuggestions(char** suggestions, int count)
{
    if (!suggestions)
    {
        return;
    }
    for (int i = 0; i < count; i++)
    {
        free(suggestions[i]);
    }
    free(suggestions);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  word [in] UTF-8 encoded word to add
///
/// @return true on success
///
/// @brief
/// Adds a word to the macOS spell checker's ignored words list.
///
/////////////////////////////////////////////////////////////////////////////
bool macAddWord(const char *word)
{
    @autoreleasepool
    {
        // macCheckSpelling() above uses the untagged checkSpellingOfString:startingAt:,
        // which does not consult ignoreWord:inSpellDocumentWithTag: (that's scoped to a
        // spell-document tag this code never creates). learnWord: adds to the user's
        // system-wide personal dictionary, which the untagged check *does* consult, so
        // it's the call that actually keeps this word from being re-flagged.
        NSString *nsWord = [NSString stringWithUTF8String:word];
        [[NSSpellChecker sharedSpellChecker] learnWord:nsWord];
        return true;
    }
}


} // extern "C"
