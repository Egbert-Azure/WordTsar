#ifndef CDOCXFILE_H
#define CDOCXFILE_H

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

#include "src/core/include/config.h"
#include "src/core/document/document.h"
#include "../../third-party/pugixml/src/pugixml.hpp"

#include "file.h"


/// @ingroup Editor
/// @{

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sCharacterProperties
///
/// @brief
/// DOCX character-level formatting properties.
/// Tracks font size, color, and text attributes (bold, italic, etc.)
/// as parsed from DOCX run properties (w:rPr).
///
/////////////////////////////////////////////////////////////////////////////
struct sCharacterProperties
{
    std::string size ;           // double
    std::string cssize ;         // double

    std::string color ;

    bool bold ;
    bool italics ;
    bool underline ;
    bool strikethrough ;
    bool superscript ;
    bool subscript ;
    bool smallcaps ;
    bool shadow ;
};



/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDOCXParagraphStyle
///
/// @brief
/// DOCX paragraph style definition.
/// Stores style ID, name, base style, fonts, spacing, indents,
/// justification, and embedded character properties as parsed
/// from the DOCX styles.xml (w:style w:type="paragraph").
///
/////////////////////////////////////////////////////////////////////////////
struct sDOCXParagraphStyle
{
    std::string id ;
    std::string name ;
    std::string basedon ;
    std::string rsid ;

    std::string asciifont ;
    std::string ansifont ;
    std::string csfont ;

    std::string before ;             // space beforee (int twips)
    std::string after ;              // space after (int twips)
    std::string linespace ;          // line spacing If the value of the linetype attribute is
                                // 'atLeast' or 'exactly', then the value of the line attribute
                                // is interpreted as 240th of a point. If the value of lineRule
                                // is 'auto', then the value of line is interpreted as 240th of
                                // a line.
    std::string linetype ;
    std::string outlinelevel ;       // int twips

    // indents
    std::string left ;               // left indent (int twips)
    std::string right ;              // right indent (int twips)
    std::string hanging ;            // remove first line indent
    std::string firstline ;          // first line (int twips)

    std::string justify ;

    struct sCharacterProperties charprops ;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDOCXCharacterStyle
///
/// @brief
/// DOCX character style definition.
/// Stores style ID, name, base style, and character properties
/// as parsed from the DOCX styles.xml (w:style w:type="character").
///
/////////////////////////////////////////////////////////////////////////////
struct sDOCXCharacterStyle
{
    std::string id ;
    std::string name ;
    std::string basedon ;
    std::string rsid ;

    sCharacterProperties charprops ;
};



class cDOCXFile : public cFile
{
public:
    cDOCXFile(cEditorBase *editor);
    ~cDOCXFile(void);

    bool CheckType(std::string filename) ;

    bool LoadFile(std::string filename) ;
    bool SaveFile(std::string filename, POSITION_T size) ;

    bool CanLoad(void) ;
    bool CanSave(void) ;

    std::string GetExtensions(void) ;

private :
    void ParseStyles(pugi::xml_node styles) ;
    void GetCharacterStyle(pugi::xml_node &style, sCharacterProperties &cstyle) ;

    void HandleParagraphNode(pugi::xml_node node, int depth) ;
    void HandleTableNode(pugi::xml_node node, int depth) ;
    void HandleSection(pugi::xml_node node) ;

    int FindParagraphStyle(std::string &style) ;
    int FindCharacterStyle(std::string &style) ;
    sDOCXParagraphStyle MergeParagraphStyles(int style) ;
    sDOCXCharacterStyle MergeCharacterStyles(int style) ;
    sDOCXParagraphStyle MergeStyles(sDOCXParagraphStyle &pstyle, sDOCXCharacterStyle &cstyle) ;

    void EmitParagraphSpace(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style) ;
    void EmitFont(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style) ;
    void EmitAttributes(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style) ;
    void EmitJustify(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style) ;
    void EmitPage(void) ;
    void EmitIndent(pugi::xml_node &node, pugi::xml_node run, sDOCXParagraphStyle &style) ;

private :
    pugi::xml_document document;
    pugi::xml_document styles;

    std::vector<sDOCXParagraphStyle> mParagraphStyles ;
    std::vector<sDOCXCharacterStyle> mCharacterStyles ;

    std::string mFontName ;
    double mFontSize ;

    int mSpaceBefore ;
    int mSpaceAfter ;
    int mLineSpace ;
    std::string mLineType ;

    int mMarginLeft ;
    int mMarginRight ;
    int mMarginTop ;
    int mMarginBottom ;

    int mLeft ;
    int mRight ;
    int mFirstline ;

    eJustification mAlign ;

    bool mBold ;
    bool mItalics ;
    bool mUnderline ;
    bool mStrikethrough ;
    bool mSuperscript ;
    bool mSubscript ;
    bool mSmallcaps ;
    bool mShadow ;
};


/// @}



#endif // CWORDSTARFILE_H
