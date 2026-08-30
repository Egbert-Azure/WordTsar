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
 * @class cRTFText
 *
 * @brief RTF plain text parse tree node.
 *
 * Implements the cRTFText class, which represents a run of literal
 * text content in the RTF parse tree. Stores the text string (mText)
 * and provides debug dump output for parse tree inspection.
 *
 * @section rtftext_content Text Content
 * Text nodes contain the literal character content found between RTF
 * control sequences and group delimiters. During parsing, special
 * characters (backslash, braces) have already been resolved by the
 * tokenizer. During tree walking, the parser inserts text node content
 * into the document with the current character formatting state applied.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFText Text node class
 * @see cRTFElement Base parse tree element class
 * @see eRTFElementType Element type enumeration
 * @see cRTFParser Parser that creates and processes text nodes
 */

#include "rtftext.h"

cRTFText::cRTFText(void)
{
    mType = eRTFTypeText ;
}

void cRTFText::dump(int level)
{
    indent(level) ;
    printf("TEXT %s\n", mText.c_str()) ;
    fflush(0) ;
}
