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

#ifndef UTILS_H
#define UTILS_H

#include <cstddef>
#include <cstdint>
#include <string>

std::string string_sprintf(const char *fmt, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 1, 2)))
#endif
    ;

// Standard subtractive-notation Roman numerals (1-3999; falls back to plain
// Arabic digits outside that range). Shared by cDotCommandParser (WordStar
// page numbering) and the DOCX reader (list numbering), so a correctness fix
// only needs to happen once.
std::string ToRomanNumeralLower(long num) ;
std::string ToRomanNumeralUpper(long num) ;

// Recent-files style path shortening -- shared by the GUI's Recent Files menu
// and the TUI's Recent Files dialog so a long directory doesn't push the
// filename (the part that actually tells two entries apart) off screen.
std::string AbbreviatePathForDisplay(const std::string &fullPath, size_t maxWidth) ;

// Decodes one UTF-8 character at p (remaining bytes available) into cp and
// returns the number of bytes it takes, 1 with cp = U+FFFD for an invalid
// lead byte. Shared by the RTF and DOCX writers, which advance one document
// position per character.
size_t DecodeUtf8CodePoint(const char *p, size_t remaining, uint32_t &cp) ;

#endif // UTILS_H
