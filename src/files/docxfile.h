#ifndef CDOCXFILE_H
#define CDOCXFILE_H

//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
// Copyright (C) 2026 Egbert H. Schroeer
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

#include <map>
#include <string>
#include <utility>
#include <vector>

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
/// @struct sDOCXTabStop
///
/// @brief
/// A single custom tab stop (w:pPr/w:tabs/w:tab), position in twips plus
/// its alignment type.
///
/////////////////////////////////////////////////////////////////////////////
struct sDOCXTabStop
{
    std::string pos ;    // twips, absolute from left margin
    std::string val ;    // "left" (default), "center", "right", "decimal", "clear", "bar"
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDOCXParagraphStyle
///
/// @brief
/// DOCX paragraph style definition.
/// Stores style ID, name, base style, fonts, spacing, indents,
/// justification, custom tab stops, and embedded character properties as
/// parsed from the DOCX styles.xml (w:style w:type="paragraph").
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

    std::vector<sDOCXTabStop> tabs ;   // custom tab stops (w:pPr/w:tabs), style-level default

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

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDOCXNumberingLevel
///
/// @brief
/// One level (w:lvl) of a DOCX numbering definition: its number format
/// (decimal, bullet, lowerRoman, etc.), the literal marker text pattern
/// (w:lvlText, e.g. "%1."), and its starting value.
///
/////////////////////////////////////////////////////////////////////////////
struct sDOCXNumberingLevel
{
    std::string format ;   // w:numFmt: "decimal", "bullet", "lowerLetter", "upperLetter", "lowerRoman", "upperRoman", "none", ...
    std::string text ;     // w:lvlText, e.g. "%1)" -- %N is replaced with the level-N counter
    int start ;            // w:start, default 1
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sDOCXNumberingDefinition
///
/// @brief
/// One DOCX abstract numbering definition (w:abstractNum): its levels,
/// keyed by w:ilvl.
///
/////////////////////////////////////////////////////////////////////////////
struct sDOCXNumberingDefinition
{
    std::map<int, sDOCXNumberingLevel> levels ;
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
    void ParseNumbering(pugi::xml_node numbering) ;

    void HandleParagraphNode(pugi::xml_node node, int depth) ;
    void HandleTableNode(pugi::xml_node node, int depth) ;
    void HandleSection(pugi::xml_node node) ;
    std::string ExtractCellText(pugi::xml_node cellNode) ;

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
    void EmitTabs(pugi::xml_node &node, sDOCXParagraphStyle &style) ;
    void EmitNumbering(pugi::xml_node &node) ;
    std::string FormatNumberingCounter(int count, const std::string &format) ;

    // --- DOCX writing (Save As Word) ---
    bool WriteDocx(const std::string &filename) ;
    std::string BuildDocumentXml(void) ;
    static std::string BuildContentTypesXml(void) ;
    static std::string BuildRelsXml(void) ;
    static std::string BuildDocumentRelsXml(void) ;
    static std::string BuildStylesXml(void) ;
    static std::string BuildCoreXml(void) ;
    static std::string BuildAppXml(void) ;

    void SaveDotCommand(pugi::xml_node &body, std::string &text) ;
    void SaveParagraph(pugi::xml_node &body, std::string &text) ;
    void FlushRun(pugi::xml_node &paragraph, std::string &buffer) ;
    static std::string ColorToHex(sSeqRGBColor &color) ;

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

    std::vector<sDOCXTabStop> mCurrentTabs ;    // last-emitted custom tab stops, to avoid redundant .tb lines

    // Numbering (list) state: abstractNum definitions keyed by their id,
    // w:num -> w:abstractNumId mapping, and a running per-(numId,ilvl)
    // counter as paragraphs are walked in document order.
    std::map<std::string, std::string> mNumIdToAbstractId ;
    std::map<std::string, sDOCXNumberingDefinition> mNumberingDefs ;
    std::map<std::pair<std::string, int>, int> mNumberingCounters ;

    eJustification mAlign ;

    bool mBold ;
    bool mItalics ;
    bool mUnderline ;
    bool mStrikethrough ;
    bool mSuperscript ;
    bool mSubscript ;
    bool mSmallcaps ;
    bool mShadow ;

    // --- state tracked while writing (Save As Word) -- kept separate from the
    // read-time members above since both paths run through the same object,
    // just never at the same time.
    int mWPageOffset ;              // .po -- section left margin (twips)
    int mWLeftMargin ;              // .lm -- paragraph left indent (twips)
    int mWFirstLine ;               // .pm -- paragraph first-line position, absolute from page offset (twips)
    int mWRightMarginPos ;          // .rm -- absolute right text boundary from page offset (twips)
    int mWTopMargin ;               // .mt (twips)
    int mWBottomMargin ;            // .mb (twips)
    int mWPaperWidth ;              // twips
    int mWPaperHeight ;             // twips
    int mWSpaceBefore ;             // .psb (twips)
    int mWSpaceAfter ;              // .psa (twips)
    double mWLineSpaceMult ;        // .ls -- multiplier, 0 = not set

    eJustification mWAlign ;

    bool mWBold ;
    bool mWItalics ;
    bool mWUnderline ;
    bool mWStrikethrough ;
    bool mWSuperscript ;
    bool mWSubscript ;
    std::string mWFontName ;
    double mWFontSize ;
    sSeqRGBColor mWColor ;

    POSITION_T mWCurrentPosition ;
};


/// @}



#endif // CWORDSTARFILE_H
