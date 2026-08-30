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
 * @class cRTFControlSymbol
 *
 * @brief RTF control symbol parse tree node.
 *
 * Implements the cRTFControlSymbol class, which represents a single RTF
 * control symbol (e.g., \~, \-, \*, \\) in the parse tree. Stores the
 * symbol character and its optional numeric parameter. Provides debug
 * dump output for parse tree inspection.
 *
 * @section rtfcontrolsymbol_symbols Common Control Symbols
 * - \~ : non-breaking space
 * - \- : optional hyphen
 * - \* : ignorable destination marker (next group is optional)
 * - \\ : literal backslash
 * - \{ : literal opening brace
 * - \} : literal closing brace
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFControlSymbol Control symbol node class
 * @see cRTFElement Base parse tree element class
 * @see eRTFElementType Element type enumeration
 * @see cRTFParser Parser that creates these nodes
 */

#include <cstdio>

#include "rtfcontrolsymbol.h"

cRTFControlSymbol::cRTFControlSymbol(void)
    : cRTFElement()
{
    mType = eRTFTypeControlSymbol ;
    mParameter = 0 ;
}


void cRTFControlSymbol::dump(int level)
{
    indent(level) ;
    printf("SYMBOL %c %d\n", mSymbol, mParameter) ;
    fflush(0) ;
}
