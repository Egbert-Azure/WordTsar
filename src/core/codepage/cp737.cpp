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
 * @class cCodePage737
 *
 * @brief DOS code page 737 (Greek) to Unicode conversion.
 *
 * Implements the cCodePage737 class which provides bidirectional mapping
 * between CP737 byte values and Unicode codepoints. The constructor populates
 * an sExtendedChars lookup table covering the full extended range.
 *
 * @section cp737_coverage Character Coverage
 * - Full Greek alphabet: capital and small letters (0x80-0xAF)
 * - Greek letters with tonos and dialytika diacritics (0xD0-0xF0)
 * - Box-drawing characters and block elements shared with CP437 (0xB0-0xDF)
 * - Mathematical symbols and miscellaneous characters (0xF1-0xFE)
 * - Smiley faces, card suits, and control symbols (0x01-0x1F)
 *
 * @section cp737_usage Usage Context
 * CP737 is the DOS Greek code page used on IBM PCs configured for Greek
 * locales. WordStar documents created on Greek-locale DOS systems use this
 * encoding for Greek text and extended characters.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCodePage737 Code page 737 converter class
 * @see sExtendedChars Byte-to-Unicode mapping entry structure
 */

#include "cp737.h"

#include <cstdint>



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Populates the CP737 to Unicode mapping table.
/// Maps DOS code page 737 (Greek) byte values to their Unicode equivalents.
///
/////////////////////////////////////////////////////////////////////////////
cCodePage737::cCodePage737(void)
{
    static const sExtendedChars tchars[] =
    {
          {   1, 0x263A }               // smiley
        , {   2, 0x263B }               // black smiley
        , {   3, 0x2665 }               // heart
        , {   4, 0x2666 }               // diamond
        , {   5, 0x2663 }               // club
        , {   6, 0x2660 }               // spade
        , {   7, 0x2022 }               // black circle
        , {   8, 0x25D8 }               // white box with black circle
        , {   9, 0x25CB }               // white circle
        , {  10, 0x25D9 }               // white box with outline circle
        , {  11, 0x2642 }               // male sign
        , {  12, 0x2640 }               // female sign
        , {  13, 0x266A }               // eigth note
        , {  14, 0x266C }               // beamed 16th note
        , {  15, 0x263C }               // sun
        , {  16, 0x25BA }               // black right pointing triangle
        , {  17, 0x25C4 }               // black left pointing triangle
        , {  18, 0x2195 }               // up down arrow
        , {  19, 0x203C }               // double exclamation
        , {  20, 0x00B6 }               // pilcrow
        , {  21, 0x00a7 }               // section
        , {  22, 0x25AC }               // black rectangle
        , {  23, 0x21A8 }               // up down arrow with base
        , {  24, 0x2191 }               // up arrow
        , {  25, 0x2193 }               // down arrow
        , {  26, 0x2192 }               // right arrow
        , {  27, 0x2190 }               // left arrow
        , {  28, 0x221F }               // right angle
        , {  29, 0x2194 }               // left right arrow
        , {  30, 0x25B2 }               // black upward pointing triangle
        , {  31, 0x25BC }               // black downward pointing triangle
        , { 127, 0x2302 }               // nothing
        , { 0x80,	0x0391 }	   // GREEK CAPITAL LETTER ALPHA
        , { 0x81,	0x0392 }	   // GREEK CAPITAL LETTER BETA
        , { 0x82,	0x0393 }	   // GREEK CAPITAL LETTER GAMMA
        , { 0x83,	0x0394 }	   // GREEK CAPITAL LETTER DELTA
        , { 0x84,	0x0395 }	   // GREEK CAPITAL LETTER EPSILON
        , { 0x85,	0x0396 }	   // GREEK CAPITAL LETTER ZETA
        , { 0x86,	0x0397 }	   // GREEK CAPITAL LETTER ETA
        , { 0x87,	0x0398 }	   // GREEK CAPITAL LETTER THETA
        , { 0x88,	0x0399 }	   // GREEK CAPITAL LETTER IOTA
        , { 0x89,	0x039a }	   // GREEK CAPITAL LETTER KAPPA
        , { 0x8a,	0x039b }	   // GREEK CAPITAL LETTER LAMDA
        , { 0x8b,	0x039c }	   // GREEK CAPITAL LETTER MU
        , { 0x8c,	0x039d }	   // GREEK CAPITAL LETTER NU
        , { 0x8d,	0x039e }	   // GREEK CAPITAL LETTER XI
        , { 0x8e,	0x039f }	   // GREEK CAPITAL LETTER OMICRON
        , { 0x8f,	0x03a0 }	   // GREEK CAPITAL LETTER PI
        , { 0x90,	0x03a1 }	   // GREEK CAPITAL LETTER RHO
        , { 0x91,	0x03a3 }	   // GREEK CAPITAL LETTER SIGMA
        , { 0x92,	0x03a4 }	   // GREEK CAPITAL LETTER TAU
        , { 0x93,	0x03a5 }	   // GREEK CAPITAL LETTER UPSILON
        , { 0x94,	0x03a6 }	   // GREEK CAPITAL LETTER PHI
        , { 0x95,	0x03a7 }	   // GREEK CAPITAL LETTER CHI
        , { 0x96,	0x03a8 }	   // GREEK CAPITAL LETTER PSI
        , { 0x97,	0x03a9 }	   // GREEK CAPITAL LETTER OMEGA
        , { 0x98,	0x03b1 }	   // GREEK SMALL LETTER ALPHA
        , { 0x99,	0x03b2 }	   // GREEK SMALL LETTER BETA
        , { 0x9a,	0x03b3 }	   // GREEK SMALL LETTER GAMMA
        , { 0x9b,	0x03b4 }	   // GREEK SMALL LETTER DELTA
        , { 0x9c,	0x03b5 }	   // GREEK SMALL LETTER EPSILON
        , { 0x9d,	0x03b6 }	   // GREEK SMALL LETTER ZETA
        , { 0x9e,	0x03b7 }	   // GREEK SMALL LETTER ETA
        , { 0x9f,	0x03b8 }	   // GREEK SMALL LETTER THETA
        , { 0xa0,	0x03b9 }	   // GREEK SMALL LETTER IOTA
        , { 0xa1,	0x03ba }	   // GREEK SMALL LETTER KAPPA
        , { 0xa2,	0x03bb }	   // GREEK SMALL LETTER LAMDA
        , { 0xa3,	0x03bc }	   // GREEK SMALL LETTER MU
        , { 0xa4,	0x03bd }	   // GREEK SMALL LETTER NU
        , { 0xa5,	0x03be }	   // GREEK SMALL LETTER XI
        , { 0xa6,	0x03bf }	   // GREEK SMALL LETTER OMICRON
        , { 0xa7,	0x03c0 }	   // GREEK SMALL LETTER PI
        , { 0xa8,	0x03c1 }	   // GREEK SMALL LETTER RHO
        , { 0xa9,	0x03c3 }	   // GREEK SMALL LETTER SIGMA
        , { 0xaa,	0x03c2 }	   // GREEK SMALL LETTER FINAL SIGMA
        , { 0xab,	0x03c4 }	   // GREEK SMALL LETTER TAU
        , { 0xac,	0x03c5 }	   // GREEK SMALL LETTER UPSILON
        , { 0xad,	0x03c6 }	   // GREEK SMALL LETTER PHI
        , { 0xae,	0x03c7 }	   // GREEK SMALL LETTER CHI
        , { 0xaf,	0x03c8 }	   // GREEK SMALL LETTER PSI
        , { 176, 0x2591 }               // light shade
        , { 177, 0x2592 }               // medium shade
        , { 178, 0x2593 }               // dark shade
        , { 179, 0x2502 }               // light vertical line
        , { 180, 0x2524 }               // light vertical and left
        , { 181, 0x2561 }               // single vertical and double left
        , { 182, 0x2562 }               // double verticle and single left
        , { 183, 0x2556 }               // double down and left single
        , { 184, 0x2555 }               // single down and left double
        , { 185, 0x2563 }               // double verticle and double left
        , { 186, 0x2551 }               // double verticle
        , { 187, 0x2557 }               // double down and double left
        , { 188, 0x255D }               // double up and double left
        , { 189, 0x255C }               // double up and single left
        , { 190, 0x255B }               // single up and doubke left
        , { 191, 0x2510 }               // light down and left
        , { 192, 0x2514 }               // light up and right
        , { 193, 0x2534 }               // light up and horizontal
        , { 194, 0x252C }               // light down and horizontal
        , { 195, 0x251C }               // light vertical and right
        , { 196, 0x2500 }               // light horizontal
        , { 197, 0x253C }               // light vertical and horizontal
        , { 198, 0x255E }               // single vertical and double right
        , { 199, 0x255F }               // double vertical and single right
        , { 200, 0x255A }               // double up and double right
        , { 201, 0x2554 }               // double down and double right
        , { 202, 0x2569 }               // double horizontal and double up
        , { 203, 0x2566 }               // double horizontal and double down
        , { 204, 0x2560 }               // double vertical and double right
        , { 205, 0x2550 }               // double horizontal
        , { 206, 0x256C }               // double horizontal and double vertical
        , { 207, 0x2567 }               // double horizontal and single up
        , { 208, 0x2568 }               // single horizontal and double up
        , { 209, 0x2564 }               // double horizontal and single down
        , { 210, 0x2565 }               // single horizontal and double down
        , { 211, 0x2559 }               // double up and single right
        , { 212, 0x2558 }               // single up and double right
        , { 213, 0x2552 }               // single down and double right
        , { 214, 0x2553 }               // double down and single right
        , { 215, 0x256B }               // double vertical and single horizontal
        , { 216, 0x256A }               // single vertical and double horizontal
        , { 217, 0x2518 }               // light up and left
        , { 218, 0x250C }               // light down and left
        , { 219, 0x2588 }               // full block
        , { 220, 0x2584 }               // lower half block
        , { 221, 0x258C }               // left half block
        , { 222, 0x2590 }               // right half block
        , { 223, 0x2580 }               // upper half block
        , { 0xe0,	0x03c9 }	   // GREEK SMALL LETTER OMEGA
        , { 0xe1,	0x03ac }	   // GREEK SMALL LETTER ALPHA WITH TONOS
        , { 0xe2,	0x03ad }	   // GREEK SMALL LETTER EPSILON WITH TONOS
        , { 0xe3,	0x03ae }	   // GREEK SMALL LETTER ETA WITH TONOS
        , { 0xe4,	0x03ca }	   // GREEK SMALL LETTER IOTA WITH DIALYTIKA
        , { 0xe5,	0x03af }	   // GREEK SMALL LETTER IOTA WITH TONOS
        , { 0xe6,	0x03cc }	   // GREEK SMALL LETTER OMICRON WITH TONOS
        , { 0xe7,	0x03cd }	   // GREEK SMALL LETTER UPSILON WITH TONOS
        , { 0xe8,	0x03cb }	   // GREEK SMALL LETTER UPSILON WITH DIALYTIKA
        , { 0xe9,	0x03ce }	   // GREEK SMALL LETTER OMEGA WITH TONOS
        , { 0xea,	0x0386 }	   // GREEK CAPITAL LETTER ALPHA WITH TONOS
        , { 0xeb,	0x0388 }	   // GREEK CAPITAL LETTER EPSILON WITH TONOS
        , { 0xec,	0x0389 }	   // GREEK CAPITAL LETTER ETA WITH TONOS
        , { 0xed,	0x038a }	   // GREEK CAPITAL LETTER IOTA WITH TONOS
        , { 0xee,	0x038c }	   // GREEK CAPITAL LETTER OMICRON WITH TONOS
        , { 0xef,	0x038e }	   // GREEK CAPITAL LETTER UPSILON WITH TONOS
        , { 0xf0,	0x038f }	   // GREEK CAPITAL LETTER OMEGA WITH TONOS
        , { 0xf1,	0x00b1 }	   // PLUS-MINUS SIGN
        , { 0xf2,	0x2265 }	   // GREATER-THAN OR EQUAL TO
        , { 0xf3,	0x2264 }	   // LESS-THAN OR EQUAL TO
        , { 0xf4,	0x03aa }	   // GREEK CAPITAL LETTER IOTA WITH DIALYTIKA
        , { 0xf5,	0x03ab }	   // GREEK CAPITAL LETTER UPSILON WITH DIALYTIKA
        , { 246, 0x00F7 }               // divide sign
        , { 247, 0x2248 }               // almost equal to
        , { 248, 0x00B0 }               // degrees
        , { 249, 0x2219 }               // bullet operator
        , { 250, 0x00B7 }               // middle dot
        , { 251, 0x221A }               // square root
        , { 252, 0x207F}                // super script lowercase n
        , { 253, 0x00B2 }               // superscript 2
        , { 254, 0x25A0}                // middle block
    } ;

    int size = sizeof(tchars) / sizeof(sExtendedChars) ;
    for(int loop = 0 ; loop < size; loop++)
    {
        mCodePage737.push_back(tchars[loop]) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned long utf8char [in] Unicode code point to look up
///
/// @return the CP737 byte value, or 0 if not found
///
/// @brief
/// Convert a Unicode code point to its CP737 byte equivalent.
///
/////////////////////////////////////////////////////////////////////////////
unsigned char cCodePage737::toChar(unsigned long utf8char)
{
    // ASCII identity (U+0000..U+007F)
    if(utf8char < 0x80)
    {
        return static_cast<unsigned char>(utf8char) ;
    }

    size_t len = mCodePage737.size() ;

    for(size_t loop = 0; loop < len; loop++)
    {
        if(mCodePage737[loop].utf8char == utf8char)
        {
            return mCodePage737[loop].wordstarchar ;
        }
    }

    return 0 ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned char wschar [in] CP737 byte value to look up
///
/// @return the Unicode code point, or 0 if not found
///
/// @brief
/// Convert a CP737 byte value to its Unicode code point equivalent.
///
/////////////////////////////////////////////////////////////////////////////
unsigned long cCodePage737::toUTF8(unsigned char wschar)
{
    // ASCII identity (0x00-0x7F)
    if(wschar < 0x80)
    {
        return static_cast<unsigned long>(wschar) ;
    }

    size_t len = mCodePage737.size() ;

    for(size_t loop = 0; loop < len; loop++)
    {
        if(mCodePage737[loop].wordstarchar == wschar)
        {
            return mCodePage737[loop].utf8char ;
        }
    }

    return UINT32_MAX ;
}
