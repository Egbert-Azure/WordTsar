#ifndef CRTFWRITER_H
#define CRTFWRITER_H

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
#include <fstream>
#include <map>
#include <array>

#include "src/core/document/document.h"
#include "src/core/layout/layoutbase.h"

#include "src/files/rtf/structs.h"
#include "src/files/wordstar/fontclassifier.h"

// Forward declaration
class cEditorBase;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sRTFHeaderFooter
///
/// @brief
/// Header or footer content for RTF output.
/// Stores the text, paragraph formatting, and document position
/// for writing headers and footers to RTF files.
///
/////////////////////////////////////////////////////////////////////////////
struct sRTFHeaderFooter
{
    std::string text ;
    sRTFParaFormat paraformat ;
    POSITION_T docStartPos ;    ///< document position where text content starts (for control code lookup)
} ;


class cRTFWriter
{
public:
    cRTFWriter(cEditorBase *editor);
    ~cRTFWriter(void);

    bool Start(std::string &filename) ;

private :
    void CreateRTFHeader(void) ;
    void CreateRTFClose(void) ;
    void CreateFontTable(void) ;
    void CreateColorTable(void) ;
    void CreateStyleSheet(void) ;
    void CreateGenerator(void) ;
    void CreateRTF(void) ;
    void CreateText(std::string &text) ;
    void CreateDot(std::string &dot) ;
    void PreScanMargins(void) ;

    void CreateModifiers(unsigned char ch) ;
    void CreateFont1(void) ;                // name changed to CreateFont1 from CreateFont, because VS2019 screws up and spews tons of errors about CreatFontW
    void CreateColor(void) ;
    void CreateHeadersFooters(int index, enum eHeaderFooter which, std::string &rest, int prefixLen) ;

    void ControlWord(const std::string &control) ;
    void ControlWord(const std::string &control, const long parameter) ;
    void ControlWord(const std::string &control, const std::string &text) ;
    void ControlText(const std::string &text) ;
    void ControlSpace(void) ;
    void StartGroup(void) ;
    void EndGroup(void) ;
    void NewLine(void) ;

    void ParagraphFormat(void) ;
    void IndexText(std::string text) ;

private :
    std::ofstream mFile ;
    cDocument *mDocument ;
    cEditorBase *mEditor ;

//    bool mBoldOn, mItalicsOn, mUnderlineOn, mSubscriptOn, mSuperscriptOn, mStrikeOn ;
    unsigned long mCommentCount ;
    unsigned long mGroupCount ;
    bool mNewLine ;

    POSITION_T mCurrentPosition ;

    int mPaperWidth ;                   // paper width in twips (from layout)
    int mPaperHeight ;                  // paper height in twips (from layout)
    int mCurrentParagraphMargin ;       // tracks .pm (first-line indent in twips, from .po)
    int mCurrentLeftMargin ;            // tracks .lm (paragraph left indent in twips, from .po)
    int mCurrentPageOffset ;            // tracks .po (page offset in twips, from paper edge)
    int mCurrentRightMarginPos ;        // tracks .rm (right text boundary in twips, from .po)
    int mCurrentMargr ;                 // current RTF \margr (computed: paperw - .po - .rm)
    int mCurrentTopMargin ;             // tracks .mt (top page margin in twips)
    int mCurrentBottomMargin ;          // tracks .mb (bottom page margin in twips)

    std::map<std::string, std::string> mFinalfont ;     // we use a map because it autmatically gets rid of duplicates for us

    std::string mFootnote1, mFootnote2, mFootnote3, mFootnote4, mFootnote5 ;

    sRTFParaFormat paragraph ;

    std::array<sRTFHeaderFooter, MAX_HEADER_FOOTER> mHeaders ;
    std::array<sRTFHeaderFooter, MAX_HEADER_FOOTER> mFooters ;
    std::array<sRTFHeaderFooter, MAX_HEADER_FOOTER> mHeadersEven ;
    std::array<sRTFHeaderFooter, MAX_HEADER_FOOTER> mFootersEven ;
    std::array<sRTFHeaderFooter, MAX_HEADER_FOOTER> mHeadersOdd ;
    std::array<sRTFHeaderFooter, MAX_HEADER_FOOTER> mFootersOdd ;

    bool mDoIndex ;             // create index at end of document?
    bool mDonefacingp ;         // have we done a facingp?

    cFontClassifier mFontClassifier ;   // font family classification (replaces QFont::styleHint)
};


#endif // CRTFWRITER_H
