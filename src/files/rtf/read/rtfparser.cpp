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
 * @class cRTFParser
 *
 * @brief Recursive-descent RTF parser and document inserter.
 *
 * Implements the cRTFParser class, which tokenizes raw RTF input into
 * a parse tree of groups, control words, control symbols, and text,
 * then walks the tree to insert formatted content into the cDocument.
 *
 * @section rtfparser_phases Parsing Phases
 * - Phase 1 (Tokenization): reads the raw RTF byte stream and builds a
 *   hierarchical parse tree of cRTFGroup, cRTFControlWord, cRTFControlSymbol,
 *   and cRTFText nodes
 * - Phase 2 (Tree Walking): traverses the parse tree to extract formatting
 *   and text, inserting into the cDocument with proper attribute tracking
 *
 * @section rtfparser_tables Table Processing
 * - Font table ({\fonttbl}): maps font IDs to font names and character sets,
 *   stored as sRTFFontTable entries
 * - Color table ({\colortbl}): maps color indices to RGB values via sRTFColorEntry
 * - Stylesheet ({\stylesheet}): paragraph and character style definitions
 *
 * @section rtfparser_formatting Formatting Handling
 * Character formatting state (cRTFCharState) and paragraph formatting state
 * (cRTFParaState) are tracked on a stack corresponding to RTF group nesting.
 * Bold, italic, underline, strikethrough, super/subscript, font, size, and
 * color changes are applied to the document via its formatting API.
 *
 * @section rtfparser_encoding Character Encoding
 * - Unicode escapes (\uN): direct Unicode codepoint insertion
 * - Code page translation: CP437, CP850, CP1252, Mac Roman for legacy encodings
 * - Symbol font PUA remapping: converts Private Use Area codepoints to
 *   standard Unicode equivalents via SymbolPUA functions
 *
 * @section rtfparser_sections Section and Page Properties
 * Handles section properties (sSectionProperties) including page size, margins,
 * orientation, headers/footers, and column layout.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFParser RTF parser class
 * @see cRTFGroup Brace-delimited group node
 * @see cRTFControlWord Named control word node
 * @see cRTFControlSymbol Control symbol node
 * @see cRTFText Literal text node
 * @see cRTFCharState Character formatting state tracker
 * @see cRTFParaState Paragraph formatting state tracker
 * @see sRTFFontTable Font table entry structure
 * @see sRTFColorEntry Color table entry structure
 * @see sSectionProperties Page/section property structure
 * @see cDocument Document model receiving parsed content
 */

#include "rtfparser.h"

#include <cstdint>

#include "src/core/include/utils.h"
#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp850.h"
#include "src/core/codepage/cp1252.h"
#include "src/core/codepage/cpmacroman.h"
#include "src/files/rtf/symbolpua.h"


cRTFParser::cRTFParser(FILE *fp, cDocument *doc, cRTFFile *rtffile)
{
    Init(doc, rtffile) ;

    // move RTF file to a string
    while(true)
    {
        char ch = static_cast<char>(fgetc(fp)) ;
        if(ch == EOF)
        {
            break ;
        }
//        if(ch != 10 && ch != 13)
        {
            mRTF.push_back(ch) ;
        }
//        else
//        {
//            mRTF.push_back(' ') ;
//        }
    }

    Parse() ;

    if(mRoot != nullptr)
    {
        // mRoot->dump(0) ;
        InsertRTF() ;
    }
}


cRTFParser::cRTFParser(std::string input, cDocument *doc, cRTFFile *rtffile)
{

    Init(doc, rtffile) ;

    mRTF = input ;

    Parse() ;

    if(mRoot != nullptr)
    {
        // mRoot->dump(0) ;
        InsertRTF() ;
    }
}


void cRTFParser::Init(cDocument *doc, cRTFFile *rtffile)
{
    mDocument = doc ;
    mRTFFile = rtffile ;

    mGroup = nullptr ;
    mRoot = nullptr ;

    mFirstColumn = true ;
    /*
    mFontIndex = -1 ;

    mLastFontIndex = -1 ;
    mLastFontSize = -1 ;

    mLastLineSpace = 0 ;
*/
    mRequireHardReturn = false ;
    mIgnoreTable = 0 ;
    mHitRowCommand = false ;
    mInTable = false ;

    mRTFCharState.Reset() ;
    mRTFPrevCharState.Reset() ;
    mRTFParaState.Reset() ;
    mRTFPrevParaState.Reset() ;

    // RTF defaults are now set by PreScanSections() calls ApplySectionProperties().
    // Prev state stays at 0 so the first comparison always triggers emission.
    mPaperwidth = 12240 ;       // 8.5 inches in twips (document-level default)
    mPaperHeight = 15840 ;      // 11 inches in twips (document-level default)

    mCurrentSection = 0 ;
    mAppliedSection = SIZE_MAX ;        // force first ApplySectionProperties call
    mSections.clear() ;

    mRTF.clear() ;
    if(mGroup != nullptr)
    {
        delete mGroup ;
    }
    mGroup = nullptr ;

    // Page setup state
    mHeaderMargin = 0 ;
    mFooterMargin = 0 ;
    mGutter = 0 ;
    mColumns = 1 ;
    mColumnSpacing = 0 ;
    mPageNumberStart = 0 ;
    mLandscape = false ;
    mFacingPages = false ;

    mHeaderMarginEmitted = false ;
    mFooterMarginEmitted = false ;
    mGutterEmitted = false ;
    mColumnsEmitted = false ;
    mPageNumberEmitted = false ;
    mLandscapeEmitted = false ;

    // Tab stop state
    mTabStops.clear() ;
    mNextTabType = 'l' ;
    mNextTabDotLeader = false ;

    mInHeaderFooter = false ;
    mHeaderLineNum = 0 ;
    mHeaderPrefix.clear() ;
    mHeaderNeedAlignTab = false ;

    // Unicode/codepage handling
    mCodePage = 437 ;               // default to CP437 (\pc) for WordStar compatibility
    mHighSurrogate = 0 ;
}

void cRTFParser::GetChar(void)
{
    if(mRTFIndex < mRTF.length())
    {
        mChar = mRTF[mRTFIndex] ;
        mRTFIndex++ ;
    }
    else
    {
        mChar = 0 ;
    }
}


void cRTFParser::Parse(void)
{
    mRTFIndex = 0 ;
    mGroup = nullptr ;
    mRoot = nullptr ;

    int lastPercent = -1;
    while(mRTFIndex < mRTF.length())
    {
        // Throttle progress updates: only call when integer percent changes
        int percent = static_cast<int>(static_cast<double>(mRTFIndex) / static_cast<double>(mRTF.length()) * 100.0) ;
        if(percent != lastPercent && mRTFFile != nullptr)
        {
            lastPercent = percent ;
            mRTFFile->UpdateProgress(percent) ;
        }

        GetChar() ;
        if(mChar == 0)
        {
            break ;
        }

        // ignore carriage returns and linefeeds
        if(mChar == '\n' || mChar == '\r')
        {
            continue ;
        }

        switch(mChar)
        {
            case '{' :
                ParseStartGroup() ;
                break ;

            case '}' :
                ParseEndGroup() ;
                break ;

            case '\\' :
                ParseControl() ;
                break ;

            default :
                ParseText() ;
                break ;

        }
    }
}


void cRTFParser::ParseStartGroup(void)
{
    cRTFGroup *newgroup = new cRTFGroup ;
    if(mGroup != nullptr)
    {
        newgroup->mParent = mGroup ;
    }

    if(mRoot == nullptr)
    {
        mGroup = newgroup ;
        mRoot = newgroup ;
    }
    else
    {
        mGroup->mChildren.push_back(newgroup) ;
        mGroup = newgroup ;
    }
}


void cRTFParser::ParseEndGroup(void)
{
    mGroup = mGroup->mParent ;
}


void cRTFParser::ParseControl(void)
{
    GetChar() ;
    mRTFIndex-- ;

    if(isalpha(mChar))
    {
        ParseControlWord() ;
    }
    else
    {
        ParseControlSymbol() ;
    }
}



void cRTFParser::ParseControlWord(void)
{
    GetChar() ;

    std::string word ;

    while(isalpha(mChar))
    {
        word += mChar ;
        GetChar() ;
    }

    int parameter = -1 ;
    bool negative = false ;

    if(mChar == '-')
    {
        GetChar() ;
        negative = true ;
    }

    while(isdigit(mChar))
    {
        if(parameter == -1)
        {
            parameter = 0 ;
        }
        parameter = parameter * 10 + (mChar - '0') ;
        GetChar() ;
    }

    if(parameter == -1)
    {
        parameter = 1 ;
    }
    if(negative)
    {
        parameter = -parameter ;
    }


    // if this is a 'u', then parameter will be folowed by a character
    if(word == "u")
    {
        // ignore space delimiter
        if(mChar == ' ')
        {
            GetChar() ;
        }

        // if the replacement charcater is in hex \'hh then skip it
        if(mChar == '\\' && mRTF[mRTFIndex] == '\'')
        {
            mRTFIndex += 3 ;
        }

        // Convert to UTF unsigned decimal
        if(negative)
        {
            parameter += 65536 ;
        }
    }
    // if the current char is a space, then its a delimiter. It's consumed
    // if its not a space, then its part of the next item in the text, so put character back
    else
    {
        if(mChar != ' ')
        {
            mRTFIndex-- ;
        }
    }

    cRTFControlWord *RTFWord = new cRTFControlWord ;
    RTFWord->mWord = word ;
    RTFWord->mParameter = parameter ;
    mGroup->mChildren.push_back(RTFWord) ;
}



void cRTFParser::ParseControlSymbol(void)
{
    GetChar() ;

    char symbol = mChar ;

    // symbols normally have no parameter. But, if this is a ', then it is followed by a two digit hex code
    int parameter = 0 ;
    if(symbol == '\'')
    {
        GetChar() ;
        std::string hex ;
        hex += mChar ;

        GetChar() ;
        hex += mChar ;

        parameter =  static_cast<int>(strtol(hex.c_str(), nullptr, 16)) ; //  hexdec(hex) ;
    }

    cRTFControlSymbol *RTFSymbol = new cRTFControlSymbol ;
    RTFSymbol->mSymbol = symbol ;
    RTFSymbol->mParameter = parameter ;
    mGroup->mChildren.push_back(RTFSymbol) ;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true success else fail
///
/// @brief
/// parse plain text up to a back slash (\) or brace (unless escaped)
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::ParseText(void)
{
    std::string text ;
    bool quit = false ;

    do
    {
        quit = false ;

//        // ignore carriage returns and linefeeds
//        if(mChar == '\n' || mChar == '\r')
//        {
//            GetChar() ;
//            continue ;
//        }

        // is this an escaped char?
        if(mChar == '\\')
        {
            GetChar() ;

            switch(mChar)
            {
                case '\\' :
                case '{' :
                case '}' :
                    break ;

                default :
                    // not an escape char, roll back
                    mRTFIndex -= 2 ;
                    quit = true ;
                    break ;

            }
        }
        else if(mChar == '{' || mChar == '}')
        {
            mRTFIndex-- ;
            quit = true ;
        }

        if(!quit)
        {
            if(mChar != 10 && mChar != 13)
            {
                text += mChar ;
            }
            GetChar() ;
        }
    } while(!quit && mRTFIndex < mRTF.length()) ;


    if(mGroup != nullptr)
    {
        cRTFText *rtftext = new cRTFText ;
        rtftext->mText = text ;

        mGroup->mChildren.push_back(rtftext) ;
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Pre-scan the AST to collect section-level properties for each section.
/// RTF section properties (\margl, \margr, \margt, \margb, \paperw, etc.)
/// apply to the entire section they appear in, not just from the point of
/// occurrence. This pass walks the tree to find section boundaries (\sect)
/// and collects all section-level properties within each section.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::PreScanSections(void)
{
    mSections.clear() ;

    // Create section 0 with RTF spec defaults
    sSectionProperties defaults ;
    mSections.push_back(defaults) ;

    size_t sectionIdx = 0 ;
    PreScanGroup(mRoot, sectionIdx) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  group [in] - AST group node to scan
/// @param  sectionIdx [in/out] - current section index, incremented on \sect
///
/// @return nothing
///
/// @brief
/// Recursively walk an AST group collecting section-level properties.
/// Skips destination groups (fonttbl, colortbl, stylesheet, info, pict,
/// header/footer, field, footnote) since section properties don't appear
/// inside those.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::PreScanGroup(cRTFGroup *group, size_t &sectionIdx)
{
    std::string type = group->GetType() ;

    // Skip destination groups -- section properties don't appear inside these
    if(type == "fonttbl" || type == "colortbl" || type == "stylesheet"
       || type == "info" || type == "pict" || type == "field"
       || type == "footnote" || type == "annotation"
       || type.substr(0, 6) == "footer" || type.substr(0, 6) == "header")
    {
        return ;
    }

    // Skip ignorable destinations (\*)
    if(group->IsDestination())
    {
        return ;
    }

    // Walk all children
    for(size_t loop = 0 ; loop < group->mChildren.size() ; loop++)
    {
        cRTFElement *child = group->mChildren[loop] ;

        if(child->mType == eRTFTypeGroup)
        {
            // Recurse into sub-groups
            PreScanGroup(static_cast<cRTFGroup *>(child), sectionIdx) ;
        }
        else if(child->mType == eRTFTypeControlWord)
        {
            cRTFControlWord *word = static_cast<cRTFControlWord *>(child) ;
            std::string control = word->mWord ;
            int parameter = word->mParameter ;

            // Section boundary -- start a new section inheriting from the previous one
            if(control == "sect")
            {
                sSectionProperties inherited = mSections[sectionIdx] ;
                mSections.push_back(inherited) ;
                sectionIdx++ ;
            }
            // Section defaults reset -- reset current section to RTF defaults
            else if(control == "sectd")
            {
                sSectionProperties defaults ;
                // Preserve paper size from the document level (not reset by \sectd)
                defaults.paperw = mSections[sectionIdx].paperw ;
                defaults.paperh = mSections[sectionIdx].paperh ;
                mSections[sectionIdx] = defaults ;
            }
            // Collect section-level properties
            else if(control == "margl" || control == "marglsxn")
            {
                mSections[sectionIdx].margl = parameter ;
            }
            else if(control == "margr" || control == "margrsxn")
            {
                mSections[sectionIdx].margr = parameter ;
            }
            else if(control == "margt" || control == "margtsxn")
            {
                mSections[sectionIdx].margt = parameter ;
            }
            else if(control == "margb" || control == "margbsxn")
            {
                mSections[sectionIdx].margb = parameter ;
            }
            else if(control == "paperw")
            {
                // Paper size is document-level -- apply to all sections
                for(size_t s = 0 ; s <= sectionIdx ; s++)
                {
                    mSections[s].paperw = parameter ;
                }
            }
            else if(control == "paperh")
            {
                // Paper size is document-level -- apply to all sections
                for(size_t s = 0 ; s <= sectionIdx ; s++)
                {
                    mSections[s].paperh = parameter ;
                }
            }
            else if(control == "gutter")
            {
                mSections[sectionIdx].gutter = parameter ;
            }
            else if(control == "landscape")
            {
                mSections[sectionIdx].landscape = true ;
            }
            else if(control == "facingp")
            {
                mSections[sectionIdx].facingp = true ;
            }
            else if(control == "headery")
            {
                mSections[sectionIdx].headery = parameter ;
            }
            else if(control == "footery")
            {
                mSections[sectionIdx].footery = parameter ;
            }
            else if(control == "cols")
            {
                mSections[sectionIdx].cols = parameter ;
            }
            else if(control == "colsx")
            {
                mSections[sectionIdx].colsx = parameter ;
            }
            else if(control == "pgnstarts")
            {
                mSections[sectionIdx].pgnstarts = parameter ;
            }
        }
        // Text and control symbols are ignored during pre-scan
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Apply the pre-scanned section properties for mCurrentSection into the
/// existing state variables. Resets emit-once flags so section-level
/// properties are re-emitted for the new section.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::ApplySectionProperties(void)
{
    if(mCurrentSection >= mSections.size())
    {
        return ;
    }

    const sSectionProperties &sec = mSections[mCurrentSection] ;

    // Page margins to para state (used by EmitPageMargins comparison)
    mRTFParaState.mMarginLeft = sec.margl ;
    mRTFParaState.mMarginRight = sec.margr ;
    mRTFParaState.mMarginTop = sec.margt ;
    mRTFParaState.mMarginBottom = sec.margb ;

    // Paper size
    mPaperwidth = sec.paperw ;
    mPaperHeight = sec.paperh ;

    // Page setup properties
    mGutter = sec.gutter ;
    mLandscape = sec.landscape ;
    mFacingPages = sec.facingp ;
    mHeaderMargin = sec.headery ;
    mFooterMargin = sec.footery ;
    mColumns = sec.cols ;
    mColumnSpacing = sec.colsx ;
    mPageNumberStart = sec.pgnstarts ;

    // Reset emit-once flags so section properties are emitted for this section
    mLandscapeEmitted = false ;
    mHeaderMarginEmitted = false ;
    mFooterMarginEmitted = false ;
    mGutterEmitted = false ;
    mColumnsEmitted = false ;
    mPageNumberEmitted = false ;
}


void cRTFParser::InsertRTF(void)
{
    mRTFCharState.Reset() ;
    mIgnoreTable = 0 ;
    mInTable = false ;

    // Force first font emission: prev font must differ from default so
    // EmitFont() detects a change and inserts a font marker for paragraph 1
    mRTFPrevCharState.mFont = -1 ;

    // Pass 1: pre-scan AST to collect section-level properties per section
    PreScanSections() ;

    // Apply section 0 properties before walking the tree
    mCurrentSection = 0 ;
    ApplySectionProperties() ;
    mAppliedSection = 0 ;

    // Pass 2: walk the AST emitting content with section properties already set
    FormatGroup(mRoot) ;
}


void cRTFParser::FormatGroup(cRTFGroup *group)
{
    std::string type = group->GetType() ;

    PushState() ;

    if(type == "fonttbl")
    {
        GetFontTable(group->mChildren) ;
//        return ;
    }

    else if(type == "colortbl")
    {
        GetColorTable(group->mChildren) ;
    }

    else if(type == "stylesheet")
    {
//        GetStyleSheet(group->mChildren) ;
//        return ;
    }

    else if(type == "info")
    {
//        GetInfo(group->mChildren) ;
//        return ;
    }

    else if(type == "pict")
    {
//        GetPicture(group->mChildren) ;
//        return ;
    }

    else if(type.substr(0, 6) == "footer" || type.substr(0, 6) == "header")
    {
        GetHeaderFooter(group, type) ;
    }

    else if(type == "field")
    {
        // Parse field destinations: {\field{\fldinst PAGE}{\fldrslt 1}}
        // Walk children looking for fldinst group to determine field type
        std::string fieldType ;
        for(size_t loop = 0; loop < group->mChildren.size(); loop++)
        {
            if(group->mChildren[loop]->mType == eRTFTypeGroup)
            {
                cRTFGroup *subgroup = static_cast<cRTFGroup *>(group->mChildren[loop]) ;
                std::string subtype = subgroup->GetType() ;
                if(subtype == "fldinst")
                {
                    // Extract field instruction text
                    std::string instrText = ExtractGroupText(subgroup) ;

                    if(instrText.find("PAGE") != std::string::npos)
                    {
                        fieldType = "PAGE" ;
                    }
                    else if(instrText.find("DATE") != std::string::npos)
                    {
                        fieldType = "DATE" ;
                    }
                    else if(instrText.find("TIME") != std::string::npos)
                    {
                        fieldType = "TIME" ;
                    }
                    else if(instrText.find("FILENAME") != std::string::npos)
                    {
                        fieldType = "FILENAME" ;
                    }
                }
            }
        }

        // Emit appropriate WordStar variable
        if(fieldType == "PAGE")
        {
            DoChanges() ;
            mDocument->InsertVariable(VAR_PAGE_NUMBER) ;
            mFirstColumn = false ;
        }
        else if(fieldType == "DATE")
        {
            DoChanges() ;
            mDocument->InsertVariable(VAR_DATE) ;
            mFirstColumn = false ;
        }
        else if(fieldType == "TIME")
        {
            DoChanges() ;
            mDocument->InsertVariable(VAR_TIME) ;
            mFirstColumn = false ;
        }
        else if(fieldType == "FILENAME")
        {
            DoChanges() ;
            mDocument->InsertVariable(VAR_FILENAME) ;
            mFirstColumn = false ;
        }
    }

    else if(type == "footnote")
    {
        // Phase 4 stub: footnote/endnote destination
        // Check for \ftnalt which indicates endnote
        DoChanges() ;
        std::string text = "<<< FOOTNOTE >>>" ;
        mDocument->Insert(text) ;
        mFirstColumn = false ;
    }

    else if(group->IsDestination())
    {
        // Check for known \* destinations
        if(group->mChildren.size() >= 2)
        {
            cRTFElement *second = group->mChildren[1] ;
            if(second->mType == eRTFTypeControlWord)
            {
                std::string destWord = static_cast<cRTFControlWord *>(second)->mWord ;

                if(destWord == "annotation")
                {
                    // Phase 4 stub: comment/annotation
                    DoChanges() ;
                    std::string text = "<<< COMMENT >>>" ;
                    mDocument->Insert(text) ;
                    mFirstColumn = false ;
                }
                // Other \* destinations (generator, etc.) are silently skipped
            }
        }
    }

    else
    {
        // take care of all child nodes
        for(size_t loop = 0; loop < group->mChildren.size(); loop++)
        {
            switch(group->mChildren[loop]->mType)
            {
                case eRTFTypeGroup :
                {
                    // Always recurse into groups -- they may contain \trowd or \row
                    FormatGroup(static_cast<cRTFGroup *>(group->mChildren[loop])) ;
                    break ;
                }

                case eRTFTypeControlWord :
                {
                    cRTFControlWord *word = static_cast<cRTFControlWord *>(group->mChildren[loop]) ;
                    if(mInTable && mIgnoreTable > 0)
                    {
                        // Inside a table row: only process table structure commands
                        if(word->mWord == "trowd" || word->mWord == "row")
                        {
                            FormatControlWord(word) ;
                        }
                        // else: skip this control word
                    }
                    else
                    {
                        // Between rows (mInTable && mIgnoreTable==0) or outside table:
                        // process normally -- formatting state like \pard is harmless
                        FormatControlWord(word) ;
                    }
                    break ;
                }

                case eRTFTypeControlSymbol :
                {
                    if(mInTable)
                    {
                        if(mIgnoreTable == 0)
                        {
                            // Table ended -- process normally
                            mInTable = false ;
                            FormatControlSymbol(static_cast<cRTFControlSymbol *>(group->mChildren[loop])) ;
                        }
                        // else: skip
                    }
                    else
                    {
                        FormatControlSymbol(static_cast<cRTFControlSymbol *>(group->mChildren[loop])) ;
                    }
                    break ;
                }

                case eRTFTypeText :
                {
                    if(mInTable)
                    {
                        if(mIgnoreTable == 0)
                        {
                            // Table ended -- process normally
                            mInTable = false ;
                            EmitText(static_cast<cRTFText *>(group->mChildren[loop])->mText) ;
                        }
                        // else: skip
                    }
                    else
                    {
                        EmitText(static_cast<cRTFText *>(group->mChildren[loop])->mText) ;
                    }
                    break ;
                }

                case eRTFElementNone :
                    break ;
            }
        }
    }

    PopState() ;
}



// this is the big one. every RTF command comes through here
void cRTFParser::FormatControlWord(cRTFControlWord *word)
{
    std::string control = word->mWord ;
    int parameter = word->mParameter ;

    switch(control[0])
    {
        case 'a' :
            if(control == "ansi")
            {
                mCodePage = 1252 ;
            }
            else if(control == "ansicpg")
            {
                mCodePage = parameter ;
            }
            break ;

        case 'b' :
            if(control == "b")
            {
            mRTFCharState.mBold = parameter ;
//                EmitBold(parameter) ;
            }
            break ;

        case 'c' :
            if(control == "cf")
            {
                mRTFCharState.mTextcolor = parameter ;
            }
            else if(control == "cb")
            {
                mRTFCharState.mBackgroundcolor = parameter ;
            }
            // \cols and \colsx are section-level -- handled by pre-scan
            else if(control == "chpgn")
            {
                // Page number variable -- insert # in header/footer, or &#& in body
                DoChanges() ;
                mDocument->InsertVariable(VAR_PAGE_NUMBER) ;
                mFirstColumn = false ;
            }
            else if(control == "chdate")
            {
                DoChanges() ;
                mDocument->InsertVariable(VAR_DATE) ;
                mFirstColumn = false ;
            }
            else if(control == "chtime")
            {
                DoChanges() ;
                mDocument->InsertVariable(VAR_TIME) ;
                mFirstColumn = false ;
            }
            break ;

        case 'd' :
            if(control == "deff")
            {
            mRTFCharState.mFont = parameter ;
            mRTFCharState.mFontsize = 24 ;
            }
            break ;

        case 'e' :
            break ;

        case 'f' :
            if(control == "fi")             // first line indent .pm
            {
                DoDotChanges() ;

                mRTFParaState.mIndentFirst = parameter ;
            }
            else if(control == "fs")
            {
                // Minimum 2 half-points (1 point). Ignore \fs0 and \fs1.
                if (parameter >= 2)
                {
                    mRTFCharState.mFontsize = parameter ;
                }
            }
            else if(control == "f")
            {
                mRTFCharState.mFont = parameter ;
            }
            // \facingp and \footery are section-level -- handled by pre-scan
            break ;

        case 'g' :
            // \gutter is section-level -- handled by pre-scan
            break ;

        case 'h' :
            // \headery is section-level -- handled by pre-scan
            if(control == "hyphauto")
            {
                // Hyphenation auto control -- no dot commands in header/footer
                if(!mInHeaderFooter)
                {
                    CheckFirstChar() ;
                    if(parameter != 0)
                    {
                        mDocument->Insert(".hy on\n") ;
                    }
                    else
                    {
                        mDocument->Insert(".hy off\n") ;
                    }
                    mFirstColumn = true ;
                }
            }
            break ;

        case 'i' :
            if(control == "i")
            {
                mRTFCharState.mItalics = parameter ;
            }
            break ;

        case 'j' :
            break ;

        case 'k' :
            if(control == "kerning")
            {
                // Kerning control -- no dot commands in header/footer
                if(!mInHeaderFooter)
                {
                    CheckFirstChar() ;
                    if(parameter != 0)
                    {
                        mDocument->Insert(".kr on\n") ;
                    }
                    else
                    {
                        mDocument->Insert(".kr off\n") ;
                    }
                    mFirstColumn = true ;
                }
            }
            break ;

        case 'l' :
            if(control == "li")             // pargarph indent .lm
            {
                DoDotChanges() ;

                mRTFParaState.mIndentPara = parameter ;
            }
            // \mLandscape is section-level -- handled by pre-scan
            else if(control == "line")
            {
                // Soft line break -- emit hard return (WordStar equivalent)
                EmitParagraph() ;
            }
            break ;

        case 'm' :
            if(control == "mac")
            {
                mCodePage = 10000 ;
            }
            // \margl, \margr, \margt, \margb are section-level -- handled by pre-scan
            break ;

        case 'n' :
            if(control == "nosupersub")
            {
                mRTFCharState.mSubscript = false ;
                mRTFCharState.mSuperscript = false ;
//                EmitSubScript(0) ;
//                EmitSuperScript(0) ;
            }
            break ;

        case 'o' :
            break ;

        case 'p' :
            if(control == "plain")
            {
                mRTFCharState.Reset() ;
            }
            else if(control == "pard")
            {
                mRTFCharState.Reset() ;

                // Save section-level margins -- not reset by \pard
                int ml = mRTFParaState.mMarginLeft ;
                int mr = mRTFParaState.mMarginRight ;
                int mt = mRTFParaState.mMarginTop ;
                int mb = mRTFParaState.mMarginBottom ;

                mRTFParaState.Reset() ;

                // Restore section-level margins
                mRTFParaState.mMarginLeft = ml ;
                mRTFParaState.mMarginRight = mr ;
                mRTFParaState.mMarginTop = mt ;
                mRTFParaState.mMarginBottom = mb ;
            }
            else if(control == "par")
            {
                // we only emit a hard return when new text is displayed
                // unless we have multiple \par in a row
                if(mRequireHardReturn == true)
                {
                    EmitParagraph() ;
                }
                mRequireHardReturn = true ;
//                EmitParagraph() ;
            }
            else if(control == "page")
            {
                EmitPage() ;
            }
            else if(control == "pc")
            {
                mCodePage = 437 ;
            }
            else if(control == "pca")
            {
                mCodePage = 850 ;
            }
            // \paperw, \paperh, \pgnstarts are section/document-level -- handled by pre-scan
            break ;

        case 'q' :
            if(control == "qc")
            {
                mRTFParaState.mAlign = control ;
//                EmitCenter(ALIGNCENTER) ;
            }
            else if(control == "qr")
            {
                mRTFParaState.mAlign = control ;
//                 EmitRight(ALIGNRIGHT) ;
            }
            else if(control == "ql")
            {
                mRTFParaState.mAlign = control ;
//                EmitLeft(ALIGNLEFT) ;
            }
            else if(control == "qj")
            {
                mRTFParaState.mAlign = control ;
//                EmitJustify(ALIGNJUSTIFY) ;
            }
            break ;

        case 'r' :
            if(control == "ri")
            {
                mRTFParaState.mIndentRight = parameter ;
//                EmitIndentRight(parameter) ;
            }
            else if(control == "row")
            {
                mHitRowCommand = true ;
                if(mIgnoreTable != 0)
                {
                    mIgnoreTable-- ;
                }
            }
            break ;

        case 's' :
            if(control == "sect")
            {
                // Section break -- advance to next pre-scanned section
                // Flush pending changes before switching sections
                DoDotChanges() ;
                mCurrentSection++ ;
                // ApplySectionProperties is deferred to DoDotChanges() because
                // \sect is typically inside a group that closes immediately,
                // and PopState would overwrite the new section's margins
            }
            else if(control == "sectd")
            {
                // Section defaults reset -- handled by pre-scan, no-op here
            }
            else if(control == "sbknone" || control == "sbkpage"
                    || control == "sbkcol" || control == "sbkeven"
                    || control == "sbkodd")
            {
                // Section break types -- handled by pre-scan, no-op here
            }
            else if(control == "strike")
            {
                mRTFCharState.mStrikethrough = parameter ;
//                EmitStrikeThrough(parameter) ;
            }
            else if(control == "sub")
            {
                mRTFCharState.mSubscript = parameter ;
//                EmitSubScript(parameter) ;
            }
            else if(control == "super")
            {
                mRTFCharState.mSuperscript = parameter ;
//                EmitSuperScript(parameter) ;
            }
            else if(control == "scaps")
            {
                mRTFCharState.mSmallCaps = parameter ;
//                EmitSmallCaps(parameter) ;
            }
            else if(control == "sl")
            {
                mRTFParaState.mLineSpace = parameter ;
//                EmitLineSpace(parameter) ;
            }
            else if(control == "sa")
            {
                mRTFParaState.mSpaceAfter = parameter ;
//                EmitSpaceAfter(parameter) ;
            }
            else if(control == "sb")
            {
                mRTFParaState.mSpaceBefore = parameter ;
//                EmitSpaceBefore(parameter) ;
            }
            break ;

        case 't' :
            if(control == "tab")
            {
                EmitTab() ;
            }
            else if(control == "tx")
            {
                // Tab stop position -- accumulate for later .tb emission
                sTabStop stop ;
                stop.position = parameter ;
                stop.type = mNextTabType ;
                stop.dotLeader = mNextTabDotLeader ;
                mTabStops.push_back(stop) ;

                // Reset next-tab modifiers
                mNextTabType = 'l' ;
                mNextTabDotLeader = false ;
            }
            else if(control == "tqc")
            {
                mNextTabType = 'c' ;
            }
            else if(control == "tqr")
            {
                mNextTabType = 'r' ;
            }
            else if(control == "tqdec")
            {
                mNextTabType = 'd' ;
            }
            else if(control == "tldot")
            {
                mNextTabDotLeader = true ;
            }
            else if(control == "tlhyph")
            {
                // Hyphen leader -- WordStar doesn't have this, treat as regular
                mNextTabDotLeader = false ;
            }
            else if(control == "trowd")
            {
                if(!mInTable)
                {
                    // First row of a new table -- insert placeholder, start suppression
                    DoChanges() ;
                    mDocument->Insert("\n<<< TABLE >>>\n") ;
                    mInTable = true ;
                    mIgnoreTable = 1 ;
                }
                else if(mHitRowCommand)
                {
                    // Continuation row or nested table
                    mIgnoreTable++ ;
                }
                mHitRowCommand = false ;
            }
            break ;

        case 'u' :
            if(control == "u")
            {
                DoChanges() ;

                // Handle UTF-16 surrogate pairs (emoji and supplementary characters)
                if(parameter >= 0xD800 && parameter <= 0xDBFF)
                {
                    // High surrogate: buffer it, wait for the low surrogate
                    mHighSurrogate = parameter ;
                }
                else if(parameter >= 0xDC00 && parameter <= 0xDFFF && mHighSurrogate != 0)
                {
                    // Low surrogate: combine with buffered high surrogate
                    CHAR_T codepoint = 0x10000
                        + ((static_cast<CHAR_T>(mHighSurrogate) - 0xD800) << 10)
                        + (static_cast<CHAR_T>(parameter) - 0xDC00) ;
                    mDocument->Insert(codepoint) ;
                    mHighSurrogate = 0 ;
                }
                else
                {
                    // Regular BMP character (or orphaned surrogate)
                    mHighSurrogate = 0 ;
                    // Remap Symbol font PUA (U+F020-U+F0FF) to standard Unicode
                    CHAR_T ch = SymbolPUAToUnicode(static_cast<CHAR_T>(parameter)) ;
                    mDocument->Insert(ch) ;
                }
            }
            else if(control == "ul")
            {
                mRTFCharState.mUnderline = parameter ;
//                EmitUnderline(parameter) ;
            }
            else if(control == "ulnone")
            {
                mRTFCharState.mUnderline = false ;
//                EmitUnderline(0) ;
            }
            break ;

        case 'v' :
            break ;

        case 'w' :
            break ;

        case 'x' :
            break ;

        case 'y' :
            break ;

        case 'z' :
            break ;

    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  symbol [in] - RTF control symbol to process
///
/// @return nothing
///
/// @brief
/// Handle RTF control symbols: \~ (non-breaking space), \- (soft hyphen),
/// \' (hex character)
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::FormatControlSymbol(cRTFControlSymbol *symbol)
{
    switch(symbol->mSymbol)
    {
        case '~' :
        {
            // Non-breaking space (binding space)
            // Insert Unicode non-breaking space U+00A0
            DoChanges() ;
            mDocument->Insert(0x00A0) ;
            mFirstColumn = false ;
            break ;
        }

        case '-' :
        {
            // Optional/soft hyphen -- skip (not needed for display)
            break ;
        }

        case '\'' :
        {
            // Hex-encoded character \'hh -- parameter contains the byte value
            // Convert through the active codepage (set by \pc, \ansi, \pca, \mac)
            DoChanges() ;
            if(symbol->mParameter > 0 && symbol->mParameter < 256)
            {
                unsigned char byte = static_cast<unsigned char>(symbol->mParameter) ;
                unsigned long unicode = UINT32_MAX ;
                switch(mCodePage)
                {
                    case 437 :
                    {
                        cCodePage437 cp ;
                        unicode = cp.toUTF8(byte) ;
                        break ;
                    }
                    case 850 :
                    {
                        cCodePage850 cp ;
                        unicode = cp.toUTF8(byte) ;
                        break ;
                    }
                    case 1252 :
                    {
                        cCodePageWin1252 cp ;
                        unicode = cp.toUTF8(byte) ;
                        break ;
                    }
                    case 10000 :
                    {
                        cCodePageMacRoman cp ;
                        unicode = cp.toUTF8(byte) ;
                        break ;
                    }
                    default :
                    {
                        // Unknown codepage: treat as Latin-1 (byte = codepoint)
                        unicode = byte ;
                        break ;
                    }
                }
                if(unicode != UINT32_MAX)
                {
                    mDocument->Insert(static_cast<CHAR_T>(unicode)) ;
                }
                else
                {
                    // Unmapped byte: insert as Latin-1 fallback
                    mDocument->Insert(symbol->mParameter) ;
                }
            }
            mFirstColumn = false ;
            break ;
        }

        case '\\' :
        case '{' :
        case '}' :
        {
            // Literal backslash, brace -- emit the character
            DoChanges() ;
            std::string s(1, symbol->mSymbol) ;
            mDocument->Insert(s) ;
            mFirstColumn = false ;
            break ;
        }

        default :
            break ;
    }
}


/*
void cRTFParser::FormatText(cRTFText *text)
{
    if(mFontSize != mRTFState.mFontsize || mFontIndex != mRTFState.mFont)
    {
        EmitFont(mFontIndex, mFontSize) ;
        mRTFState.mFont = mFontIndex ;
        mRTFState.mFontsize = mFontSize ;
    }
    ApplyStyle(text->mText) ;
}
*/

void cRTFParser::PushState(void)
{
    mCharState.push(mRTFCharState) ;
    mParaState.push(mRTFParaState) ;
}



void cRTFParser::PopState(void)
{
    cRTFCharState state ;
    cRTFParaState pstate ;

    if(mCharState.empty() == false)
    {
        state = mCharState.top() ;
        mCharState.pop() ;
        mRTFCharState = state ;

        pstate = mParaState.top() ;
        mParaState.pop() ;
        mRTFParaState = pstate ;
    }
    else
    {
        mRTFCharState.Reset() ;
        mRTFParaState.Reset() ;
    }
}





void cRTFParser::GetFontTable(std::vector<cRTFElement *>element)
{
    sRTFFontTable fonttable ;

    // go through the entries in the font table
    // skip 0 since that is 'fonttbl'
    for(size_t loop = 1; loop < element.size(); loop++)
    {
        if(element[loop]->mType == eRTFTypeGroup)
        {
            cRTFGroup *group = static_cast<cRTFGroup *>(element[loop]) ;

            bool alt = false ;      // alternate font name

            // go through the font iself
            for(size_t attribloop = 0; attribloop < group->mChildren.size(); attribloop++)
            {
                cRTFElement *felement = group->mChildren[attribloop] ;
                if(felement->mType == eRTFTypeControlWord)
                {
                    cRTFControlWord *control = static_cast<cRTFControlWord *>(felement) ;

                    // check font family
                    if(control->mWord == "fnil"
                            || control->mWord == "froman"
                            || control->mWord == "fswiss"
                            || control->mWord == "fmodern"
                            || control->mWord == "fscript"
                            || control->mWord == "fdecor"
                            || control->mWord == "ftech"
                            || control->mWord == "fbidi"
                            )
                    {
                        fonttable.family = control->mWord ;
                    }
                    else if(control->mWord == "fcharset")
                    {
                        fonttable.charset = control->mParameter ;
                    }
                    else if(control->mWord == "falt")
                    {
                        alt = true ;
                    }
                    else if(control->mWord == "f")
                    {
                        fonttable.number = control->mParameter ;
                    }
                }
                else if(felement->mType == eRTFTypeText)                // font name
                {
                    cRTFText *name = static_cast<cRTFText *>(felement) ;

                    std::string fname = name->mText ;
                    size_t semi = fname.find_first_of(";") ;
                    if(semi != std::string::npos)
                    {
                        fname = fname.substr(0, semi) ;
                    }
                    if(alt == false)
                    {
                        fonttable.name = fname ;
                    }
                    else
                    {
                        fonttable.altname = fname ;
                    }
                }
            }
            mFontTable.push_back(fonttable) ;
        }
    }
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  element [in] - children of the \colortbl group
///
/// @return nothing
///
/// @brief
/// Parse the RTF color table. Format: {\colortbl ;\red0\green0\blue0;...}
/// First entry (before first semicolon) is auto/default color (usually empty).
/// Each subsequent entry has \red N \green N \blue N followed by semicolon.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::GetColorTable(std::vector<cRTFElement *>element)
{
    mColorTable.clear() ;

    // The color table format is:
    // {\colortbl ;\red0\green0\blue0;\red255\green0\blue0;...}
    // The first semicolon with no RGB values = auto/default color (index 0)
    // Subsequent entries have \red \green \blue control words

    sRTFColorEntry currentEntry ;
    bool hasRGB = false ;

    // Skip element[0] which is the "colortbl" control word
    for(size_t loop = 1; loop < element.size(); loop++)
    {
        if(element[loop]->mType == eRTFTypeControlWord)
        {
            cRTFControlWord *control = static_cast<cRTFControlWord *>(element[loop]) ;

            if(control->mWord == "red")
            {
                currentEntry.red = control->mParameter ;
                hasRGB = true ;
            }
            else if(control->mWord == "green")
            {
                currentEntry.green = control->mParameter ;
                hasRGB = true ;
            }
            else if(control->mWord == "blue")
            {
                currentEntry.blue = control->mParameter ;
                hasRGB = true ;
            }
        }
        else if(element[loop]->mType == eRTFTypeText)
        {
            // Text elements contain semicolons (possibly with whitespace)
            // Each semicolon terminates a color entry
            cRTFText *text = static_cast<cRTFText *>(element[loop]) ;
            for(size_t ch = 0; ch < text->mText.size(); ch++)
            {
                if(text->mText[ch] == ';')
                {
                    if(!hasRGB)
                    {
                        // Empty entry = auto/default color
                        currentEntry.isAuto = true ;
                    }
                    mColorTable.push_back(currentEntry) ;

                    // Reset for next entry
                    currentEntry = sRTFColorEntry() ;
                    hasRGB = false ;
                }
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Emit color change when \cf index has changed. Looks up RGB in color table
/// and calls InsertColor() with nearest CGA palette match.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::EmitColor(void)
{
    if(mRTFCharState.mTextcolor != mRTFPrevCharState.mTextcolor)
    {
        int index = mRTFCharState.mTextcolor ;

        // Index 0 in RTF color table = auto/default, skip it
        if(index > 0 && static_cast<size_t>(index) < mColorTable.size())
        {
            sRTFColorEntry &entry = mColorTable[index] ;

            if(!entry.isAuto)
            {
                sSeqRGBColor color ;
                color.red = static_cast<short>(entry.red) ;
                color.green = static_cast<short>(entry.green) ;
                color.blue = static_cast<short>(entry.blue) ;
                color.alpha = 255 ;
                mDocument->InsertColor(color) ;
            }
        }

        mFirstColumn = false ;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  group [in] - the RTF group containing the header/footer content
/// @param  type [in] - group type string ("header", "headerl", "headerr",
///                      "footer", "footerl", "footerr")
///
/// @return nothing
///
/// @brief
/// Parse an RTF header or footer destination group and emit corresponding
/// WordStar dot commands (.h1/.f1 for both, .h1e/.f1e for even, .h1o/.f1o
/// for odd pages). Multiple \par within the group generate .h2/.h3 etc.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::GetHeaderFooter(cRTFGroup *group, const std::string &type)
{
    // Determine the dot command prefix and line counter
    std::string prefix ;
    if(type == "header")
    {
        prefix = ".h" ;
    }
    else if(type == "headerl")
    {
        // headerl = left page = even pages in book layout
        // Without \facingp, \headerl is ignored per RTF spec
        if(mFacingPages)
        {
            prefix = ".he" ;
        }
        else
        {
            return ;
        }
    }
    else if(type == "headerr")
    {
        // headerr = right page = odd pages in book layout
        // Without \facingp, headerr applies to ALL pages per RTF spec
        if(mFacingPages)
        {
            prefix = ".ho" ;
        }
        else
        {
            prefix = ".h" ;
        }
    }
    else if(type == "footer")
    {
        prefix = ".f" ;
    }
    else if(type == "footerl")
    {
        // footerl = left page = even pages in book layout
        // Without \facingp, \footerl is ignored per RTF spec
        if(mFacingPages)
        {
            prefix = ".fe" ;
        }
        else
        {
            return ;
        }
    }
    else if(type == "footerr")
    {
        // footerr = right page = odd pages in book layout
        // Without \facingp, footerr applies to ALL pages per RTF spec
        if(mFacingPages)
        {
            prefix = ".fo" ;
        }
        else
        {
            prefix = ".f" ;
        }
    }
    else
    {
        return ;
    }

    // Ensure we are at start of line before entering header mode
    // (must be before mInHeaderFooter is set, since EmitParagraph is guarded)
    CheckFirstChar() ;

    mInHeaderFooter = true ;
    mHeaderLineNum = 1 ;
    mHeaderPrefix = prefix ;
    mHeaderNeedAlignTab = true ;

    // Reset prev char state to force emission of initial formatting
    cRTFCharState savedPrevChar = mRTFPrevCharState ;
    mRTFPrevCharState.Reset() ;
    mRTFPrevCharState.mFont = -1 ;

    // Insert first header line dot command
    EmitHeaderPrefix() ;

    // Process header content inline with full formatting
    ProcessHeaderChildren(group) ;

    // Terminate last line
    mDocument->Insert(HARD_RETURN) ;
    mFirstColumn = true ;

    // Restore prev char state so body text emission works correctly
    mRTFPrevCharState = savedPrevChar ;

    mInHeaderFooter = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Insert the dot command prefix for the current header/footer line.
/// Builds prefixes like ".h1 ", ".h2e ", ".f3o ", etc.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::EmitHeaderPrefix(void)
{
    std::string dotprefix ;
    if(mHeaderPrefix.length() == 2)
    {
        // .h or .f -- use numbered form .h1, .h2, etc.
        dotprefix = mHeaderPrefix + std::to_string(mHeaderLineNum) + " " ;
    }
    else
    {
        // .he, .ho, .fe, .fo -- use .h1e, .h2e, etc.
        std::string suffix = mHeaderPrefix.substr(2) ;
        dotprefix = mHeaderPrefix.substr(0, 2) + std::to_string(mHeaderLineNum) + suffix + " " ;
    }
    mDocument->Insert(dotprefix) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  group [in] - RTF group containing header/footer content
///
/// @return nothing
///
/// @brief
/// Process header/footer group children inline with full formatting.
/// Routes control words through FormatControlWord() for state tracking,
/// emits font/color/attribute control codes before text, handles \par
/// by closing the current line and starting the next dot command,
/// and inserts alignment tabs per-line.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::ProcessHeaderChildren(cRTFGroup *group)
{
    for(size_t loop = 0; loop < group->mChildren.size(); loop++)
    {
        cRTFElement *child = group->mChildren[loop] ;

        switch(child->mType)
        {
            case eRTFTypeGroup :
            {
                cRTFGroup *subgroup = static_cast<cRTFGroup *>(child) ;
                std::string subtype = subgroup->GetType() ;

                if(subtype == "field")
                {
                    // Handle field codes (PAGE, DATE, TIME, FILENAME)
                    for(size_t j = 0; j < subgroup->mChildren.size(); j++)
                    {
                        if(subgroup->mChildren[j]->mType == eRTFTypeGroup)
                        {
                            cRTFGroup *fieldGroup = static_cast<cRTFGroup *>(subgroup->mChildren[j]) ;
                            if(fieldGroup->GetType() == "fldinst")
                            {
                                std::string instrText = ExtractGroupText(fieldGroup) ;

                                // Emit alignment tab before variable if needed
                                if(mHeaderNeedAlignTab)
                                {
                                    mHeaderNeedAlignTab = false ;
                                    if(mRTFParaState.mAlign == "qc")
                                    {
                                        sWSTab tab ;
                                        tab.tabsize = 0 ;
                                        tab.abstabsize = 0 ;
                                        tab.size = 0 ;
                                        tab.type = TAB_CENTER ;
                                        mDocument->InsertTab(tab) ;
                                    }
                                    else if(mRTFParaState.mAlign == "qr")
                                    {
                                        sWSTab tab ;
                                        tab.tabsize = 0 ;
                                        tab.abstabsize = 0 ;
                                        tab.size = 0 ;
                                        tab.type = TAB_RIGHT ;
                                        mDocument->InsertTab(tab) ;
                                    }
                                }

                                // Emit formatting before variable
                                EmitFont() ;
                                EmitColor() ;
                                EmitAttributes() ;
                                mRTFPrevCharState = mRTFCharState ;

                                if(instrText.find("PAGE") != std::string::npos)
                                {
                                    mDocument->InsertVariable(VAR_PAGE_NUMBER) ;
                                }
                                else if(instrText.find("DATE") != std::string::npos)
                                {
                                    mDocument->InsertVariable(VAR_DATE) ;
                                }
                                else if(instrText.find("TIME") != std::string::npos)
                                {
                                    mDocument->InsertVariable(VAR_TIME) ;
                                }
                                else if(instrText.find("FILENAME") != std::string::npos)
                                {
                                    mDocument->InsertVariable(VAR_FILENAME) ;
                                }

                                mFirstColumn = false ;
                            }
                        }
                    }
                }
                else
                {
                    // Recurse into sub-groups with state push/pop
                    PushState() ;
                    ProcessHeaderChildren(subgroup) ;
                    PopState() ;
                }
                break ;
            }

            case eRTFTypeControlWord :
            {
                cRTFControlWord *word = static_cast<cRTFControlWord *>(child) ;

                if(word->mWord == "par")
                {
                    // End current header line, start next
                    mDocument->Insert(HARD_RETURN) ;
                    mFirstColumn = true ;
                    mHeaderLineNum++ ;
                    if(mHeaderLineNum > 5)
                    {
                        return ;
                    }
                    CheckFirstChar() ;
                    EmitHeaderPrefix() ;
                    mHeaderNeedAlignTab = true ;

                    // Reset prev char state to force re-emission of formatting on new line
                    mRTFPrevCharState.Reset() ;
                    mRTFPrevCharState.mFont = -1 ;
                }
                else if(word->mWord == "chpgn")
                {
                    // Legacy page number field
                    mDocument->InsertVariable(VAR_PAGE_NUMBER) ;
                    mFirstColumn = false ;
                }
                else if(word->mWord == "chdate")
                {
                    mDocument->InsertVariable(VAR_DATE) ;
                    mFirstColumn = false ;
                }
                else if(word->mWord == "chtime")
                {
                    mDocument->InsertVariable(VAR_TIME) ;
                    mFirstColumn = false ;
                }
                else
                {
                    // Process all other control words normally for state tracking
                    // This handles \b, \i, \ul, \f, \fs, \cf, \qc, \qr, \pard, etc.
                    FormatControlWord(word) ;
                }
                break ;
            }

            case eRTFTypeControlSymbol :
            {
                FormatControlSymbol(static_cast<cRTFControlSymbol *>(child)) ;
                break ;
            }

            case eRTFTypeText :
            {
                // Emit alignment tab before first text on this line
                if(mHeaderNeedAlignTab)
                {
                    mHeaderNeedAlignTab = false ;
                    if(mRTFParaState.mAlign == "qc")
                    {
                        sWSTab tab ;
                        tab.tabsize = 0 ;
                        tab.abstabsize = 0 ;
                        tab.size = 0 ;
                        tab.type = TAB_CENTER ;
                        mDocument->InsertTab(tab) ;
                    }
                    else if(mRTFParaState.mAlign == "qr")
                    {
                        sWSTab tab ;
                        tab.tabsize = 0 ;
                        tab.abstabsize = 0 ;
                        tab.size = 0 ;
                        tab.type = TAB_RIGHT ;
                        mDocument->InsertTab(tab) ;
                    }
                }

                // Emit formatting changes (font, color, attributes) -- no DoDotChanges
                EmitFont() ;
                EmitColor() ;
                EmitAttributes() ;
                mRTFPrevCharState = mRTFCharState ;

                std::string &text = static_cast<cRTFText *>(child)->mText ;
                mDocument->Insert(text) ;
                mFirstColumn = false ;
                break ;
            }

            case eRTFElementNone :
            {
                break ;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  group [in] - RTF group to extract plain text from
///
/// @return std::string - concatenated text content of group
///
/// @brief
/// Recursively extract all text from an RTF group, ignoring formatting.
/// Used for field instruction parsing and header/footer text extraction.
///
/////////////////////////////////////////////////////////////////////////////
std::string cRTFParser::ExtractGroupText(cRTFGroup *group)
{
    std::string result ;

    for(size_t loop = 0; loop < group->mChildren.size(); loop++)
    {
        cRTFElement *child = group->mChildren[loop] ;

        if(child->mType == eRTFTypeText)
        {
            result += static_cast<cRTFText *>(child)->mText ;
        }
        else if(child->mType == eRTFTypeControlWord)
        {
            cRTFControlWord *cw = static_cast<cRTFControlWord *>(child) ;
            if(cw->mWord == "chpgn")
            {
                result += "#" ;
            }
        }
        else if(child->mType == eRTFTypeGroup)
        {
            result += ExtractGroupText(static_cast<cRTFGroup *>(child)) ;
        }
    }

    return result ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  group [in] - RTF group to walk recursively
/// @param  currentLine [in/out] - text being accumulated for current line
/// @param  lines [out] - completed lines (split on \par)
///
/// @return nothing
///
/// @brief
/// Recursively walk an RTF group tree, collecting text and splitting on
/// \par control words at any nesting depth. Handles \chpgn, \chdate,
/// \chtime, and field groups (PAGE, DATE, TIME, FILENAME). Used by
/// GetHeaderFooter() to support multi-line headers from real-world RTF
/// where \par is nested inside formatting groups.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::CollectHeaderFooterContent(cRTFGroup *group,
                                             std::string &currentLine,
                                             std::vector<std::string> &lines,
                                             std::string &alignment)
{
    for(size_t loop = 0; loop < group->mChildren.size(); loop++)
    {
        cRTFElement *child = group->mChildren[loop] ;

        if(child->mType == eRTFTypeControlWord)
        {
            cRTFControlWord *cw = static_cast<cRTFControlWord *>(child) ;

            if(cw->mWord == "par")
            {
                // Line break -- push accumulated text as a completed line
                lines.push_back(currentLine) ;
                currentLine.clear() ;
            }
            else if(cw->mWord == "chpgn")
            {
                currentLine += "&#&" ;
            }
            else if(cw->mWord == "chdate")
            {
                currentLine += "&@&" ;
            }
            else if(cw->mWord == "chtime")
            {
                currentLine += "&!&" ;
            }
            else if(cw->mWord == "qc")
            {
                alignment = "qc" ;
            }
            else if(cw->mWord == "qr")
            {
                alignment = "qr" ;
            }
            else if(cw->mWord == "ql")
            {
                alignment = "ql" ;
            }
            else if(cw->mWord == "qj")
            {
                alignment = "qj" ;
            }
            // Ignore other formatting control words
        }
        else if(child->mType == eRTFTypeText)
        {
            currentLine += static_cast<cRTFText *>(child)->mText ;
        }
        else if(child->mType == eRTFTypeGroup)
        {
            cRTFGroup *subgroup = static_cast<cRTFGroup *>(child) ;
            std::string subtype = subgroup->GetType() ;

            if(subtype == "field")
            {
                // Parse field: look for fldinst to determine type
                for(size_t j = 0; j < subgroup->mChildren.size(); j++)
                {
                    if(subgroup->mChildren[j]->mType == eRTFTypeGroup)
                    {
                        cRTFGroup *fieldGroup = static_cast<cRTFGroup *>(subgroup->mChildren[j]) ;
                        if(fieldGroup->GetType() == "fldinst")
                        {
                            std::string instrText = ExtractGroupText(fieldGroup) ;
                            if(instrText.find("PAGE") != std::string::npos)
                            {
                                currentLine += "&#&" ;
                            }
                            else if(instrText.find("DATE") != std::string::npos)
                            {
                                currentLine += "&@&" ;
                            }
                            else if(instrText.find("TIME") != std::string::npos)
                            {
                                currentLine += "&!&" ;
                            }
                            else if(instrText.find("FILENAME") != std::string::npos)
                            {
                                currentLine += "&*&" ;
                            }
                        }
                    }
                }
            }
            else
            {
                // Recurse into sub-groups to find \par at any depth
                CollectHeaderFooterContent(subgroup, currentLine, lines, alignment) ;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// Emit page setup dot commands that were collected from document-level
/// control words. Called once when first text is about to be emitted.
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::EmitPageSetup(void)
{
    // Landscape
    if(mLandscape && !mLandscapeEmitted)
    {
        mLandscapeEmitted = true ;
        CheckFirstChar() ;
        mDocument->Insert(".pr or=l\n") ;
        mFirstColumn = true ;
    }

    // Header margin
    if(mHeaderMargin != 0 && !mHeaderMarginEmitted)
    {
        mHeaderMarginEmitted = true ;
        CheckFirstChar() ;
        double in = static_cast<double>(mHeaderMargin) / TWIPSPERINCH ;
        std::string out = string_sprintf(".hm %.2fi\n", in) ;
        mDocument->Insert(out) ;
        mFirstColumn = true ;
    }

    // Footer margin
    if(mFooterMargin != 0 && !mFooterMarginEmitted)
    {
        mFooterMarginEmitted = true ;
        CheckFirstChar() ;
        double in = static_cast<double>(mFooterMargin) / TWIPSPERINCH ;
        std::string out = string_sprintf(".fm %.2fi\n", in) ;
        mDocument->Insert(out) ;
        mFirstColumn = true ;
    }

    // Gutter is folded into the \margl to .po emission in EmitPageMargins.
    // In RTF, gutter is additional binding space added to the left margin.

    // Columns (always emit, including .co 1 to explicitly turn off columns)
    if(!mColumnsEmitted)
    {
        mColumnsEmitted = true ;
        CheckFirstChar() ;
        if(mColumnSpacing != 0)
        {
            double in = static_cast<double>(mColumnSpacing) / TWIPSPERINCH ;
            std::string out = string_sprintf(".co %d,%.2fi\n", mColumns, in) ;
            mDocument->Insert(out) ;
        }
        else
        {
            std::string out = string_sprintf(".co %d\n", mColumns) ;
            mDocument->Insert(out) ;
        }
        mFirstColumn = true ;
    }

    // Page number start
    if(mPageNumberStart != 0 && !mPageNumberEmitted)
    {
        mPageNumberEmitted = true ;
        CheckFirstChar() ;
        std::string out = string_sprintf(".pn %d\n", mPageNumberStart) ;
        mDocument->Insert(out) ;
        mFirstColumn = true ;
    }

    // Tab stops -- emit .tb with type prefixes (^=center, >=right, #=decimal)
    if(!mTabStops.empty())
    {
        CheckFirstChar() ;

        // Build .tb line with type prefixes for all tab types
        std::string tbOut = ".tb" ;
        for(size_t i = 0; i < mTabStops.size(); i++)
        {
            double in = static_cast<double>(mTabStops[i].position) / TWIPSPERINCH ;

            switch(mTabStops[i].type)
            {
                case 'c' :
                    tbOut += string_sprintf(" ^%.2fi", in) ;
                    break ;
                case 'r' :
                    tbOut += string_sprintf(" >%.2fi", in) ;
                    break ;
                case 'd' :
                    tbOut += string_sprintf(" #%.2fi", in) ;
                    break ;
                default :
                    tbOut += string_sprintf(" %.2fi", in) ;
                    break ;
            }
        }
        tbOut += "\n" ;
        mDocument->Insert(tbOut) ;
        mFirstColumn = true ;

        mTabStops.clear() ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// If this is a dot command, make sure we are at the beginning of a line
///
/// This may introduce extra line feeds into the document, but we'll get
/// all our formatting
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::CheckFirstChar(void)
{
    if(mFirstColumn == false)
    {
        EmitParagraph() ;
        mRequireHardReturn = false ;
    }
}



void cRTFParser::DoChanges(void)
{
    DoDotChanges() ;

    EmitFont() ;
    EmitColor() ;
    EmitAttributes() ;

    mRTFPrevCharState = mRTFCharState ;
}


void cRTFParser::DoDotChanges(void)
{
    // No dot commands inside header/footer lines -- only MARKER_CHAR entities allowed
    if(mInHeaderFooter)
    {
        return ;
    }

    // Apply deferred section properties. \sect increments mCurrentSection,
    // but ApplySectionProperties can't run there because \sect is typically
    // inside a group -- PopState would overwrite the new margins. Instead,
    // apply lazily here, after the group has closed.
    if(mCurrentSection != mAppliedSection && mCurrentSection < mSections.size())
    {
        ApplySectionProperties() ;
        mAppliedSection = mCurrentSection ;
    }

    EmitPageSetup() ;
    EmitPageMargins() ;
    EmitParagraphSpace() ;
    //            EmitKeepLines(node, run, newstyle) ;
    //            EmitKeepNext(node, run, newstyle) ;
    EmitIndent() ;
    EmitIndentParagraph() ;
    EmitIndentRight() ;
    //            EmitNumbering(node, run, newstyle) ;
    //            EmitOutlineLevel(node, run, newstyle) ;
    //            EmitBorder(node, run, newstyle) ;
    //            EmitShading(node, run, newstyle) ;
    //            EmitTabs(node, run, newstyle) ;

    // EmitJustify must be BEFORE the state copy so alignment comparisons work
    EmitJustify() ;

    mRTFPrevParaState = mRTFParaState ;
}

void cRTFParser::EmitText(std::string &text)
{
    if(mRequireHardReturn)
    {
        EmitParagraph() ;
        mRequireHardReturn = false ;
    }
    DoChanges() ;
    mDocument->Insert(text) ;
    mFirstColumn = false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// do the work to get a new paragraph
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::EmitParagraph(void)
{
    // Don't insert stray hard returns inside header/footer lines
    if(mInHeaderFooter)
    {
        return ;
    }

    mDocument->Insert(HARD_RETURN) ;
    mFirstColumn = true ;
    mRequireHardReturn = false ;
}


void cRTFParser::EmitPage(void)
{
    // No dot commands inside header/footer lines
    if(mInHeaderFooter)
    {
        return ;
    }

    CheckFirstChar() ;
    mDocument->Insert(".pa") ;
    mDocument->Insert(HARD_RETURN) ;
    mFirstColumn = true ;
}


void cRTFParser::EmitTab(void)
{
    sWSTab tab ;
    tab.abstabsize = 0 ;
    tab.size = 0 ;
    tab.tabsize = 0 ;
    tab.type = TAB_TAB ;

//    CheckParagraphFormatting() ;
    mDocument->InsertTab(tab) ;
//    mFirstColumn = false ;

    mFirstColumn = false ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return  nothing
///
/// @brief
/// Emit page margin dot commands when \margt, \margb, or \margl change.
/// \margt maps to .mt, \margb maps to .mb, \margl maps to .po (page offset)
///
/////////////////////////////////////////////////////////////////////////////
void cRTFParser::EmitPageMargins(void)
{
    // Top margin: \margt maps to .mt
    if(mRTFParaState.mMarginTop != mRTFPrevParaState.mMarginTop)
    {
        CheckFirstChar() ;

        double in = static_cast<double>(mRTFParaState.mMarginTop) / TWIPSPERINCH ;
        std::string out = string_sprintf(".mt %.2fi\n", in) ;
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }

    // Bottom margin: \margb maps to .mb
    if(mRTFParaState.mMarginBottom != mRTFPrevParaState.mMarginBottom)
    {
        CheckFirstChar() ;

        double in = static_cast<double>(mRTFParaState.mMarginBottom) / TWIPSPERINCH ;
        std::string out = string_sprintf(".mb %.2fi\n", in) ;
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }

    // Left page margin: \margl + \gutter maps to .po (page offset)
    // In RTF, \gutter is additional binding space added to the left margin.
    if(mRTFParaState.mMarginLeft != mRTFPrevParaState.mMarginLeft)
    {
        CheckFirstChar() ;

        double in = static_cast<double>(mRTFParaState.mMarginLeft + mGutter) / TWIPSPERINCH ;
        std::string out = string_sprintf(".po %.2fi\n", in) ;
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }
}



void cRTFParser::EmitParagraphSpace(void)
{
    if(mRTFParaState.mSpaceBefore != mRTFPrevParaState.mSpaceBefore)
    {
        CheckFirstChar() ;

        double in = static_cast<double>(mRTFParaState.mSpaceBefore) / TWIPSPERINCH ;
        char out[100] ;
        snprintf(out, 99, ".psb %.2fi%c", in, HARD_RETURN) ;
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }

    if(mRTFParaState.mSpaceAfter != mRTFPrevParaState.mSpaceAfter)
    {
        CheckFirstChar() ;

        double in = static_cast<double>(mRTFParaState.mSpaceAfter) / TWIPSPERINCH ;
        char out[100] ;
        snprintf(out, 99, ".psa %.2fi%c", in, HARD_RETURN) ;
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }

    if(mRTFParaState.mLineSpace != mRTFPrevParaState.mLineSpace)
    {
        CheckFirstChar() ;

        if(mRTFParaState.mLineSpace == 0)
        {
            // \sl0 or \pard reset -- restore single line spacing
            char out[100] ;
            snprintf(out, 99, ".ls 1%c", HARD_RETURN) ;
            mDocument->Insert(out) ;
        }
        else
        {
            double nls = static_cast<double>(mRTFParaState.mLineSpace) / 240.0 ;
            if(nls < 0.0)
            {
                nls *= -1 ;
            }

            char out[100] ;
            snprintf(out, 99, ".ls %.2fi%c", nls, HARD_RETURN) ;
            mDocument->Insert(out) ;
        }

        mFirstColumn = true ;
    }
}



void cRTFParser::EmitIndent(void)
{
    if(mRTFParaState.mIndentFirst != mRTFPrevParaState.mIndentFirst
       || mRTFParaState.mIndentPara != mRTFPrevParaState.mIndentPara)
    {
        CheckFirstChar() ;

        // RTF \fi is the first-line indent relative to \li.
        // WordStar .pm is the paragraph margin (first-line indent).
        // .pm = (\li + \fi) converted to inches.
        double space = (mRTFParaState.mIndentPara + mRTFParaState.mIndentFirst) / TWIPSPERINCH ;

        // Negative .pm is invalid in WordStar -- disable paragraph margin
        std::string out ;
        if (space < 0.0)
        {
            out = ".pm 0\n" ;
        }
        else
        {
            out = string_sprintf(".pm %.2fi\n", space) ;
        }
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }
}



void cRTFParser::EmitIndentParagraph(void)
{

    if(mRTFParaState.mIndentPara != mRTFPrevParaState.mIndentPara)
    {
        CheckFirstChar() ;

        double in = static_cast<double>(mRTFParaState.mIndentPara) / TWIPSPERINCH ;

        std::string out = string_sprintf(".lm %.2fi\n", in) ;
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }
}



void cRTFParser::EmitIndentRight(void)
{

    // Emit .rm whenever any of its inputs change: \ri, \margl, or \margr.
    // In WordStar, .rm is relative to .po, so when .po changes (from \margl),
    // .rm must be re-emitted to preserve the RTF right margin distance.
    if(mRTFParaState.mIndentRight != mRTFPrevParaState.mIndentRight
       || mRTFParaState.mMarginLeft != mRTFPrevParaState.mMarginLeft
       || mRTFParaState.mMarginRight != mRTFPrevParaState.mMarginRight)
    {
        CheckFirstChar() ;

        // RTF \ri is the right indent from the right page margin.
        // WordStar .rm is the absolute right margin position from page offset.
        // page offset = \margl + \gutter, so:
        // .rm = (paperwidth - \margl - \gutter - \margr - \ri) converted to inches.
        double rm = (static_cast<double>(mPaperwidth)
                     - static_cast<double>(mRTFParaState.mMarginLeft)
                     - static_cast<double>(mGutter)
                     - static_cast<double>(mRTFParaState.mMarginRight)
                     - mRTFParaState.mIndentRight) / TWIPSPERINCH ;

        std::string out = string_sprintf(".rm %.2fi\n", rm) ;
        mDocument->Insert(out) ;

        mFirstColumn = true ;
    }
}



void cRTFParser::EmitJustify(void)
{
    if(mRTFParaState.mAlign != mRTFPrevParaState.mAlign)
    {
        if(mRTFParaState.mAlign == "qc")
        {
            CheckFirstChar() ;
            mDocument->BeginCenter() ;
            mFirstColumn = false ;
        }
        else if(mRTFParaState.mAlign == "qr")
        {
            CheckFirstChar() ;
            mDocument->BeginRight() ;
            mFirstColumn = false ;
        }
        else if(mRTFParaState.mAlign == "ql")
        {
            CheckFirstChar() ;
            mDocument->BeginLeft() ;
            mFirstColumn = false ;
        }
        else if(mRTFParaState.mAlign == "qj")
        {
            CheckFirstChar() ;
            mDocument->BeginJustify() ;
            mFirstColumn = false ;
        }
    }
}



void cRTFParser::EmitFont(void)
{
    if(mRTFPrevCharState.mFontsize != mRTFCharState.mFontsize || mRTFPrevCharState.mFont != mRTFCharState.mFont)
    {
        size_t loop ;
        int index = mRTFCharState.mFont ;
        for(loop = 0 ; loop < mFontTable.size(); loop++)
        {
            if(mFontTable[loop].number == index)
            {
                index = loop ;
                break ;
            }
        }

        if(loop == mFontTable.size())
        {
            index = 0 ;
        }

        // No font table -- skip emission
        if (mFontTable.empty())
        {
            return ;
        }

        sInternalFonts internalfont ;

        internalfont.name = mFontTable[index].name ;

        // Guard against 0-point font -- clamp to 12 points default
        int fontSize = mRTFCharState.mFontsize ;
        if (fontSize < 2)
        {
            fontSize = 24 ;  // Default 12 points (24 half-points)
        }
        internalfont.size = fontSize / 2 ;
        internalfont.haveWSFont = false ;
        mDocument->InsertFont(internalfont) ;

        mFirstColumn = false ;
    }
}



void cRTFParser::EmitAttributes(void)
{
    if(mRTFCharState.mBold != mRTFPrevCharState.mBold)
    {
        mDocument->BeginBold() ;
    }
    if(mRTFCharState.mItalics != mRTFPrevCharState.mItalics)
    {
        mDocument->BeginItalics() ;
    }
    if(mRTFCharState.mUnderline != mRTFPrevCharState.mUnderline)
    {
        mDocument->BeginUnderline() ;
    }
    if(mRTFCharState.mStrikethrough != mRTFPrevCharState.mStrikethrough)
    {
        mDocument->BeginStrikeThrough() ;
    }
    if(mRTFCharState.mSuperscript != mRTFPrevCharState.mSuperscript)
    {
        mDocument->BeginSuperscript() ;
    }
    if(mRTFCharState.mSubscript != mRTFPrevCharState.mSubscript)
    {
        mDocument->BeginSubscript() ;
    }
    if(mRTFCharState.mSmallCaps != mRTFPrevCharState.mSmallCaps)
    {
//        mDocument->BeginSmallCaps() ;
    }
//    if(mRTFState.mShadow != mRTFPrevState.mShadow)
//    {
//        mDocument->BeginShadow() ;
//    }
}


