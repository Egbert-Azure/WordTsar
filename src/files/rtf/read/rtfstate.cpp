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
 * @class cRTFCharState
 *
 * @brief RTF character and paragraph formatting state tracking.
 *
 * Implements the cRTFCharState and cRTFParaState classes used by the
 * RTF parser to track the current formatting context during parse tree
 * traversal. Both classes provide Reset() to restore defaults when
 * entering new scopes or encountering reset control words (\plain, \pard).
 *
 * @section rtfstate_char Character State (cRTFCharState)
 * Tracks character-level attributes that apply to text runs:
 * - Toggle attributes: bold, italic, underline, strikethrough, hidden
 * - Positional: superscript, subscript, vertical offset (dn/up)
 * - Font: font ID (index into font table), font size in half-points
 * - Color: foreground text color, background/highlight color
 * - Special: small caps, alignment override
 *
 * @section rtfstate_para Paragraph State (cRTFParaState)
 * Tracks paragraph-level attributes that apply to whole paragraphs:
 * - Alignment: left, center, right, justified
 * - Spacing: line spacing, space before/after paragraph
 * - Indentation: left indent, right indent, first-line indent
 * - Margins: paragraph-level margin overrides
 * - Hyphenation: automatic hyphenation on/off
 *
 * @section rtfstate_scope Scope Management
 * The RTF parser maintains a stack of state objects. When entering a new
 * group ({), the current state is pushed. When leaving (}), it is popped.
 * Reset() is called in response to \plain (character) and \pard (paragraph)
 * control words.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFCharState Character formatting state class
 * @see cRTFParaState Paragraph formatting state class
 * @see cRTFParser RTF parser using these state classes
 */

#include "rtfstate.h"

cRTFCharState::cRTFCharState(void)
{
    Reset() ;
}


/*
void cRTFCharState::CopyInto(cRTFCharState &newstate)
{
    newstate.mBold = mBold ;
    newstate.mItalics = mItalics ;
    newstate.mUnderline = mUnderline ;
    newstate.mStrikethrough = mStrikethrough ;
    newstate.mHidden = mHidden ;
    newstate.mDnup = mDnup ;
    newstate.mSubscript = mSubscript ;
    newstate.mSuperscript = mSuperscript ;
    newstate.mFontsize = mFontsize ;
    newstate.mFont = mFont ;
    newstate.mTextcolor = mTextcolor ;
    newstate.mBackgroundcolor = mBackgroundcolor ;
    newstate.mSmallCaps = mSmallCaps ;
    newstate.mAlign = mAlign ;

}



bool cRTFCharState::Compare(cRTFCharState &newstate)
{
    return    mBold == newstate.mBold
           && mItalics == newstate.mItalics
            && mUnderline == newstate.mUnderline
            && mStrikethrough == newstate.mStrikethrough
            && mHidden == newstate.mHidden
            && mDnup == newstate.mDnup
            && mSubscript == newstate.mSubscript
            && mSuperscript == newstate.mSuperscript
            && mFontsize == newstate.mFontsize
            && mFont == newstate.mFont
            && mTextcolor == newstate.mTextcolor
            && mBackgroundcolor == newstate.mBackgroundcolor
            && mSmallCaps == newstate.mSmallCaps ;
}
*/



void cRTFCharState::Reset(void)
{
    // character format
    mBold = false ;
    mItalics = false ;
    mUnderline = false ;
    mStrikethrough = false ;
    mHidden = false ;
    mDnup = 0 ;
    mSubscript = false ;
    mSuperscript = false ;
    mSmallCaps = false ;

    // paragraph format
    mFontsize = 24 ;  // 12 points default (24 half-points)
    mFont = 0 ;
    mTextcolor = 0 ;
    mBackgroundcolor = 0 ;
}


cRTFParaState::cRTFParaState(void)
{
    Reset() ;
}


void cRTFParaState::Reset(void)
{
    mLineSpace = 0 ;
    mAlign = "ql" ;
    mSpaceAfter = 0.0 ;
    mSpaceBefore = 0.0 ;
    mIndentFirst = 0.0 ;
    mIndentPara = 0.0 ;
    mIndentRight = 0.0 ;
    mHyphennate = false ;
    mMarginLeft = 0 ;
    mMarginRight = 0 ;
    mMarginBottom = 0 ;
    mMarginTop = 0 ;
}
