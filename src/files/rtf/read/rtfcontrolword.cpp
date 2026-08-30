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
 * @class cRTFControlWord
 *
 * @brief RTF control word parse tree node.
 *
 * Implements the cRTFControlWord class, which represents a named RTF
 * control word (e.g., \b, \par, \fonttbl) in the parse tree. Stores
 * the word string and its optional numeric parameter. Provides debug
 * dump output for parse tree inspection.
 *
 * @section rtfcontrolword_examples Common Control Words
 * - Character formatting: \b (bold), \i (italic), \ul (underline),
 *   \strike (strikethrough), \fs (font size), \f (font number)
 * - Paragraph formatting: \par (paragraph break), \pard (reset paragraph),
 *   \ql/\qc/\qr/\qj (alignment), \li/\ri (indents)
 * - Document structure: \fonttbl (font table), \colortbl (color table),
 *   \stylesheet (styles), \sectd (section defaults)
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFControlWord Control word node class
 * @see cRTFElement Base parse tree element class
 * @see eRTFElementType Element type enumeration
 * @see cRTFParser Parser that creates these nodes
 */

#include <cstdio>

#include "rtfcontrolword.h"

cRTFControlWord::cRTFControlWord(void)
{
    mType = eRTFTypeControlWord ;
    mParameter = 0 ;
}


void cRTFControlWord::dump(int level)
{
    indent(level) ;
    printf("WORD %s %d\n", mWord.c_str(), mParameter) ;
    fflush(0) ;
}
