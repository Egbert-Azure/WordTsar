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

#ifndef CRTFELEMENT_H
#define CRTFELEMENT_H

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eRTFElementType
///
/// @brief
/// RTF parse tree element types.
/// Identifies whether an RTF element is a control word, control symbol,
/// group, or plain text during recursive-descent parsing.
///
/////////////////////////////////////////////////////////////////////////////
enum eRTFElementType
{
    eRTFElementNone,
    eRTFTypeControlWord,
    eRTFTypeControlSymbol,
    eRTFTypeGroup,
    eRTFTypeText
};

class cRTFElement
{
public:
    cRTFElement(void);

    virtual void dump(int level) = 0 ;

    void indent(int level) ;

public:
    eRTFElementType mType ;
};

#endif // CRTFELEMENT_H
