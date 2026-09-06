#ifndef HYPHENATOR_H
#define HYPHENATOR_H

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

#include <string>
#include <vector>

/////////////////////////////////////////////////////////////////////////////
/// @class cHyphenator
///
/// @brief
/// Finds dictionary-based hyphenation points for real WordStar 7's
/// auto-hyphenation (^OH / .hy). Mirrors cSpellChecker's per-platform split
/// (spellcheck.h/.cpp/macspellcheck.mm): the language-specific dictionary
/// work happens in a platform helper, this class is the thin, portable
/// front end.
///
/////////////////////////////////////////////////////////////////////////////
class cHyphenator
{
public:
    explicit cHyphenator(const std::string& language = "en_US");

    // Candidate break points within `word`, as Unicode codepoint offsets
    // from the start of the word (a hyphen may be inserted just before the
    // character at each offset). Ascending order. Empty if no hyphenation
    // dictionary is available for the configured language, or the word has
    // no valid break point.
    std::vector<size_t> HyphenationPoints(const std::string& word) const;

private:
    std::string mLanguage;      // hyphenation dictionary language (e.g. "en_US")
};

#endif // HYPHENATOR_H
