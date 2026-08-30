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

#ifndef CRTFPARSER_H
#define CRTFPARSER_H

#include <string>
#include <stack>
#include <vector>

#include "src/files/rtffile.h"

#include "rtftext.h"
#include "rtfcontrolsymbol.h"
#include "rtfcontrolword.h"
#include "rtfgroup.h"
#include "rtfstate.h"

#include "src/core/document/document.h"


//enum eAlign
//{
//    ALIGNCENTER,
//    ALIGNLEFT,
//    ALIGNRIGHT,
//    ALIGNJUSTIFY
//} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sRTFFontTable
///
/// @brief
/// RTF font table entry.
/// Stores font number, name, alternate name, family, and charset
/// as parsed from the \fonttbl group in an RTF file.
///
/////////////////////////////////////////////////////////////////////////////
struct sRTFFontTable
{
    int number ;
    std::string name ;
    std::string altname ;
    std::string family ;
//    eControls family ;
    int charset ;               ///< @TODO not used yet
//    int pitch ;                 ///< not used yet
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sRTFColorEntry
///
/// @brief
/// RTF color table entry.
/// Stores RGB values parsed from the \colortbl group.
/// An empty entry (isAuto=true) represents the auto/default color.
///
/////////////////////////////////////////////////////////////////////////////
struct sRTFColorEntry
{
    int red ;
    int green ;
    int blue ;
    bool isAuto ;               ///< true if this is the auto/default color (empty entry)

    sRTFColorEntry() : red(0), green(0), blue(0), isAuto(false) {}
} ;

class cRTFFile ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sSectionProperties
///
/// @brief
/// RTF section-level properties collected during pre-scan pass.
/// Stores margins, paper size, gutter, header/footer offsets, column
/// settings, and page numbering. RTF section properties apply to the
/// entire section they appear in, not just from the point of occurrence.
///
/////////////////////////////////////////////////////////////////////////////
struct sSectionProperties
{
    int margl ;             ///< \margl -- left page margin in twips
    int margr ;             ///< \margr -- right page margin in twips
    int margt ;             ///< \margt -- top page margin in twips
    int margb ;             ///< \margb -- bottom page margin in twips
    int paperw ;            ///< \paperw -- paper width in twips
    int paperh ;            ///< \paperh -- paper height in twips
    int gutter ;            ///< \gutter -- binding space in twips
    int headery ;           ///< \headery -- header margin in twips
    int footery ;           ///< \footery -- footer margin in twips
    int cols ;              ///< \cols -- number of columns
    int colsx ;             ///< \colsx -- column spacing in twips
    int pgnstarts ;         ///< \pgnstarts -- starting page number
    bool landscape ;        ///< \landscape flag
    bool facingp ;          ///< \facingp -- facing pages flag

    // Initialize with RTF spec defaults (section 1.9.1)
    sSectionProperties()
        : margl(1800), margr(1800), margt(1440), margb(1440)
        , paperw(12240), paperh(15840)
        , gutter(0), headery(0), footery(0)
        , cols(1), colsx(0), pgnstarts(0)
        , landscape(false), facingp(false)
    {}
} ;


class cRTFParser
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cRTFParser(FILE *fp, cDocument *doc, cRTFFile *rtffile);
    cRTFParser(std::string input, cDocument *doc, cRTFFile *rtffile);

private:
    // Initialization
    void Init(cDocument *doc, cRTFFile *rtffile) ;

    // Tokenizer / character-level parsing
    void GetChar(void) ;
    void Parse(void) ;

    // Group and control parsing
    void ParseStartGroup(void) ;
    void ParseEndGroup(void) ;
    void ParseControl(void) ;
    void ParseControlWord(void) ;
    void ParseControlSymbol(void) ;
    void ParseText(void) ;

    // RTF tree insertion
    void InsertRTF(void) ;

    // Two-pass section property handling
    void PreScanSections(void) ;
    void PreScanGroup(cRTFGroup *group, size_t &sectionIdx) ;
    void ApplySectionProperties(void) ;

    // Tree formatting / interpretation
    void FormatGroup(cRTFGroup *group) ;
    void FormatControlWord(cRTFControlWord *word) ;
    void FormatControlSymbol(cRTFControlSymbol *symbol) ;
//    void FormatText(cRTFText *text) ;

    // State stack management
    void PushState(void) ;
    void PopState(void) ;

    // Table extraction (font, color)
    void GetFontTable(std::vector<cRTFElement *>element) ;
    void GetColorTable(std::vector<cRTFElement *>element) ;

    // Header/footer processing
    void GetHeaderFooter(cRTFGroup *group, const std::string &type) ;
    void ProcessHeaderChildren(cRTFGroup *group) ;
    void EmitHeaderPrefix(void) ;
    std::string ExtractGroupText(cRTFGroup *group) ;
    void CollectHeaderFooterContent(cRTFGroup *group, std::string &currentLine, std::vector<std::string> &lines, std::string &alignment) ;

    // First-character handling
    void CheckFirstChar(void) ;

    // Change application
    void DoChanges(void) ;
    void DoDotChanges(void) ;

    // Text emission
    void EmitText(std::string &text) ;

    // Structural emission (paragraph, page, tab)
    void EmitParagraph(void) ;
    void EmitPage(void) ;
    void EmitTab(void) ;

    // Page/section property emission
    void EmitPageMargins(void) ;
    void EmitPageSetup(void) ;
    void EmitParagraphSpace(void) ;
//            void EmitKeepLines(void) ;
//            void EmitKeepNext(void) ;
    void EmitIndent(void) ;
    void EmitIndentParagraph(void) ;
    void EmitIndentRight(void) ;
//            void EmitNumbering(void) ;
//            void EmitOutlineLevel(void) ;
//            void EmitBorder(void) ;
//            void EmitShading(void) ;
//            void EmitTabs(void) ;
    void EmitJustify(void) ;

    // Font, color, and attribute emission
    void EmitFont(void) ;
    void EmitColor(void) ;
    void EmitAttributes(void) ;

/*
    void EmitBold(int param) ;
    void EmitItalic(int param) ;
    void EmitUnderline(int param) ;
    void EmitSubScript(int paramd) ;
    void EmitSuperScript(int param) ;
    void EmitSmallCaps(int param) ;

    void EmitTextColor(int index) ;
    void EmitBackgroundColor(int index) ;
    void EmitPage(void) ;
    void EmitTab(void) ;
    void EmitCenter(eAlign param) ;
    void EmitLeft(eAlign param) ;
    void EmitRight(eAlign param) ;
    void EmitJustify(eAlign param) ;
    void EmitLineSpace(int ls) ;
    void EmitIndentFirst(int index) ;
    void EmitIndentRight(int index) ;
    void EmitFont(int index, int size) ;
    void EmitParagraph(void) ;
    void EmitSpaceAfter(int param) ;
    void EmitSpaceBefore(int param) ;
*/

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    // Core document references
    cDocument *mDocument ;
    cRTFFile *mRTFFile ;                    // for progress updates

    // RTF input state
    bool mRequireHardReturn ;       ///< true if we need a hard return, else false
    std::string mRTF ;                   // the RTF std::string
    char mChar ;                    // the current character from the RTF std::string
    size_t mRTFIndex ;              // index into the RTF std::string

//    int mFontIndex ;                // the font index when we see a 'f' command
//    int mFontSize ;                 // the font size when we see a 'fs' command
//    int mLastFontIndex ;                // the last emitted font index
//    int mLastFontSize ;                 // the last emitted font size

//    int mLastLineSpace ;            // the last line space emitted

    bool mFirstColumn ;             ///< true if in first column, else false

    // Paper dimensions
    int mPaperwidth ;
    int mPaperHeight ;
//    int mMarginLeft ;
//    int mMarginRight ;
//    int mMarginTop ;
//    int mMarginBottom ;

    // RTF tree structure
    cRTFGroup *mGroup ;          // current working group
    cRTFGroup *mRoot ;          // root of tree

    // Character state tracking
    cRTFCharState mRTFCharState ;
    cRTFCharState mRTFPrevCharState ;
    std::stack<cRTFCharState> mCharState ;

    // Paragraph state tracking
    cRTFParaState mRTFParaState ;
    cRTFParaState mRTFPrevParaState ;
    std::stack<cRTFParaState> mParaState ;

    // Font and color tables
    std::vector<sRTFFontTable> mFontTable ;
    std::vector<sRTFColorEntry> mColorTable ;

    // Table handling state
    long mIgnoreTable ;
    bool mHitRowCommand ;
    bool mInTable ;

    // Page setup state (from document-level control words)
    int mHeaderMargin ;             ///< \headery value in twips
    int mFooterMargin ;             ///< \footery value in twips
    int mGutter ;                   ///< \gutter value in twips
    int mColumns ;                  ///< \cols value
    int mColumnSpacing ;            ///< \colsx value in twips
    int mPageNumberStart ;          ///< \pgnstarts value
    bool mLandscape ;               ///< \landscape flag
    bool mFacingPages ;             ///< \facingp flag

    // Page setup emission tracking
    bool mHeaderMarginEmitted ;     ///< true if .hm already emitted
    bool mFooterMarginEmitted ;     ///< true if .fm already emitted
    bool mGutterEmitted ;           ///< true if .po from \gutter already emitted
    bool mColumnsEmitted ;          ///< true if .co already emitted
    bool mPageNumberEmitted ;       ///< true if .pn already emitted
    bool mLandscapeEmitted ;        ///< true if .pr or=l already emitted

    // Tab stop accumulation state
    struct sTabStop
    {
        int position ;              ///< tab position in twips
        char type ;                 ///< 'l'=left, 'c'=center, 'r'=right, 'd'=decimal
        bool dotLeader ;            ///< true if dot leader
    } ;
    std::vector<sTabStop> mTabStops ;
    char mNextTabType ;             ///< type for next \tx
    bool mNextTabDotLeader ;        ///< dot leader for next \tx

    // Header/footer processing state
    bool mInHeaderFooter ;          ///< true while processing header/footer destination
    int mHeaderLineNum ;            ///< current header line number (1-5)
    std::string mHeaderPrefix ;     ///< dot command prefix (.h, .he, .ho, .f, .fe, .fo)
    bool mHeaderNeedAlignTab ;      ///< true until alignment tab emitted for current line

    // Unicode/codepage handling
    int mCodePage ;                 ///< active codepage (437=\pc, 1252=\ansi, 850=\pca, 10000=\mac)
    int mHighSurrogate ;            ///< buffered high surrogate from \u (0 = none pending)

    // Two-pass section handling
    std::vector<sSectionProperties> mSections ;     ///< pre-scanned section properties
    size_t mCurrentSection ;                        ///< current section index during pass 2
    size_t mAppliedSection ;                        ///< last section whose properties were applied to mRTFParaState
};

#endif // CRTFPARSER_H
