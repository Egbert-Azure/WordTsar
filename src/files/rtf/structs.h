#ifndef RTFSTRUCTS_H
#define RTFSTRUCTS_H
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
///
/// @enum eAlign
///
/// @brief
/// RTF text alignment types.
/// Used during RTF parsing and writing for paragraph alignment.
///
/////////////////////////////////////////////////////////////////////////////
enum eAlign
{
    ALIGNCENTER,
    ALIGNLEFT,
    ALIGNRIGHT,
    ALIGNJUSTIFY
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sRTFCharFormat
///
/// @brief
/// RTF character formatting state.
/// Tracks bold, italics, underline, super/subscript, smallcaps,
/// and strikethrough attributes during RTF parsing and writing.
///
/////////////////////////////////////////////////////////////////////////////
struct sRTFCharFormat
{
    bool bold ;
    bool italics ;
    bool underline ;
    bool superscript ;
    bool subscript ;
    bool smallcaps ;
    bool strikethrough ;
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sRTFParaFormat
///
/// @brief
/// RTF paragraph formatting state.
/// Tracks alignment, spacing, indents, font/color table context,
/// and embedded character formatting during RTF parsing and writing.
///
/////////////////////////////////////////////////////////////////////////////
struct sRTFParaFormat
{
    int align ;
    double linespace ;
    long spaceafter ;           // spacing after this paragraph
    long spacebefore ;          // spacing before this paragraph
    long indentfirst ;          // first line indent
    long indentpara ;           // paragraph indent
    long indentright ;          // right indent
    bool hyphennate ;           // hyphenation on or off
    bool fonttable ;            // true if we are in a font table block
    int fontindex ;             // current font 
    long fontsize ;             // currentfont size
    bool colortable ;           // true if we are in a color table
    sRTFCharFormat character ;  // charcater format
} ;





#endif
