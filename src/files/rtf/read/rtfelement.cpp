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
 * @class cRTFElement
 *
 * @brief Abstract base class for RTF parse tree elements.
 *
 * Implements the cRTFElement base class, which is the polymorphic root
 * of all RTF parse tree nodes. Provides the indent() helper used by all
 * subclasses for hierarchical debug dump output, and defines the mType
 * member for runtime type identification.
 *
 * @section rtfelement_hierarchy Parse Tree Node Hierarchy
 * - cRTFElement (base): type tag and indent helper
 *   - cRTFGroup: brace-delimited group ({...}) with child elements
 *   - cRTFControlWord: named control word (\b, \par, \fonttbl, etc.)
 *   - cRTFControlSymbol: single-character control symbol (\~, \-, \*, etc.)
 *   - cRTFText: literal text content between control sequences
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFElement Base parse tree element class
 * @see eRTFElementType Element type enumeration (eRTFTypeGroup, eRTFTypeControlWord, etc.)
 * @see cRTFGroup Group node subclass
 * @see cRTFControlWord Control word node subclass
 * @see cRTFControlSymbol Control symbol node subclass
 * @see cRTFText Text node subclass
 */

#include <cstdio>

#include "rtfelement.h"

cRTFElement::cRTFElement(void)
{
}


void cRTFElement::indent(int level)
{
    for(int i = 0; i < level; i++)
    {
        printf("-") ;
    }
}
