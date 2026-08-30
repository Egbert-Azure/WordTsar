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

#ifndef CRTFSTATE_H
#define CRTFSTATE_H

#include <string>

class cRTFCharState
{
public:
    cRTFCharState(void) ;

//    void CopyInto(cRTFCharState &newstate) ;
//    bool Compare(cRTFCharState &newstate) ;
    void Reset(void) ;

    // character format state
    bool mBold ;
    bool mItalics ;
    bool mUnderline ;
    bool mStrikethrough ;
    bool mHidden ;
    // Attribute that specifies that the text should be beneath the baseline ("down", negative) or above the baseline ("up", positive) by N.
    // RTF "dnN" move down N half-points; does not imply font size reduction, thus font size is given separately --> value negative from param, fontsize unchanged.
    // RTF "upN" move up N half-points; does not imply font size reduction, thus font size is given separately --> value positive from param, fontsize unchanged.
    int mDnup ;
    bool mSubscript ;
    bool mSuperscript ;
    bool mSmallCaps ;

    int mFontsize ;
    int mFont ;
    int mTextcolor ;
    int mBackgroundcolor ;
};


class cRTFParaState
{
public:
    cRTFParaState(void) ;

//    void CopyInto(cRTFCharState &newstate) ;
//    bool Compare(cRTFCharState &newstate) ;
    void Reset(void) ;

    // paragraph state
    double mLineSpace ;
    std::string mAlign ;
    double mSpaceAfter ;         // spacing after this paragraph /sa
    double mSpaceBefore ;        // spacing before this paragraph /sb
    double mIndentFirst ;        // first line indent
    double mIndentPara ;         // paragraph indent
    double mIndentRight ;        // right indent
    bool mHyphennate ;           // hyphenation on or off

    int mMarginLeft ;
    int mMarginRight ;
    int mMarginTop ;
    int mMarginBottom ;

};



#endif // CRTFSTATE_H
