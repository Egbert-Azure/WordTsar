#ifndef SYMBOLPUA_H
#define SYMBOLPUA_H
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

#include "src/core/include/config.h"


/// Map Symbol font PUA codepoint (U+F020-U+F0FF) to standard Unicode.
/// Returns standard Unicode equivalent, or the input unchanged if not in PUA range.
CHAR_T SymbolPUAToUnicode(CHAR_T codepoint) ;

/// Map standard Unicode codepoint to Symbol font PUA (U+F020-U+F0FF).
/// Returns PUA equivalent for Word compatibility, or 0 if no mapping exists.
CHAR_T UnicodeToSymbolPUA(CHAR_T codepoint) ;


#endif // SYMBOLPUA_H
