#ifndef SPELLCHECKER_H
#define SPELLCHECKER_H

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

#ifdef _WIN32
#include <spellcheck.h>
#include <objidl.h>
#include <comdef.h>
#include <combaseapi.h>
#endif

#include <string>
#include <vector>

class cSpellChecker
{
public:
    cSpellChecker(const std::string& language = "en_US");
    ~cSpellChecker(void);

    // Returns true if the word is spelled correctly.
    bool CheckWord(const std::string &word);

    // Returns a vector of suggested corrections for a misspelled word.
    std::vector<std::string> suggestions(const std::string &word);

    // Adds a custom word to the dictionary.
    // Returns true if the word was added successfully.
    bool AddWord(const std::string &word);

private:
    std::string mLanguage;      // dictionary language (e.g. "en_US")

    // Platform-specific members.
#ifdef _WIN32
    ISpellCheckerFactory* pSpellCheckerFactory; // actually an ISpellCheckerFactory*
    ISpellChecker* pSpellChecker;        // actually an ISpellChecker*
#elif defined(__APPLE__)
    // No member data needed here--the macOS implementation is provided
    // in a separate Objective-C++ file.
#else
    void* hunspell; // actually a Hunspell* instance
#endif
};

#endif // SPELLCHECKER_H
