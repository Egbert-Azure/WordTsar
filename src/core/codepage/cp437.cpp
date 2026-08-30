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
 * @class cCodePage437
 *
 * @brief DOS code page 437 to Unicode conversion.
 *
 * Implements the cCodePage437 class which provides bidirectional mapping
 * between CP437 (the original IBM PC character set) byte values and Unicode
 * codepoints. The constructor populates an sExtendedChars lookup table
 * covering the full extended range (bytes 1-254).
 *
 * @section cp437_coverage Character Coverage
 * - Box-drawing characters and block elements (0xB0-0xDF)
 * - Greek letters and mathematical symbols (0xE0-0xF4)
 * - Accented Latin characters (0x80-0xAF)
 * - Currency, punctuation, and miscellaneous symbols (0xF5-0xFE)
 * - Smiley faces, card suits, and line-drawing characters (0x01-0x1F)
 *
 * @section cp437_usage Usage Context
 * CP437 was the standard character set on the original IBM PC and is the
 * encoding used by most DOS-era WordStar documents. This conversion table
 * is essential for preserving special characters when loading legacy files.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCodePage437 Code page 437 converter class
 * @see sExtendedChars Byte-to-Unicode mapping entry structure
 */

#include "cp437.h"

#include <cstdint>



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Populates the CP437 to Unicode mapping table.
/// Maps DOS code page 437 byte values (1-254) to their Unicode equivalents.
///
/////////////////////////////////////////////////////////////////////////////
cCodePage437::cCodePage437(void)
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
        , { 128, 0x00C7 }               // capital C with cedilla
        , { 129, 0x00FC }               // lowercase U with diaeresis
        , { 130, 0x00e9 }               // lowercase E with acute
        , { 131, 0x00E2 }               // lowercase A with circumflex
        , { 132, 0x00E4 }               // lowercase A with diaeresis
        , { 133, 0x00E0 }               // lowercase A with grave
        , { 134, 0x00E5 }               // lowercase A with ring above
        , { 135, 0x00E7 }               // lowercase C with cedilla
        , { 136, 0x00EA }               // lowercase E with circumflex
        , { 137, 0x00EB }               // lowercase E with diaeresis
        , { 138, 0x00E8 }               // lowercase E with grave
        , { 139, 0x00EF }               // lowercase I with diaeresis
        , { 140, 0x00EE }               // lowercase I with acute
        , { 141, 0x00EC }               // lowercase I with grave
        , { 142, 0x00C4 }               // uppercase A with diaeresis
        , { 143, 0x00C5 }               // uppercase A with ring above
        , { 144, 0x00C9 }               // uppercase E with acute
        , { 145, 0x00E6 }               // lowercase AE
        , { 146, 0x00C6 }               // uppercase AE
        , { 147, 0x00F4 }               // lowercase O with acute
        , { 148, 0x00F6 }               // lowercase O with diaeresis
        , { 149, 0x00F2 }               // lowercase O with grave
        , { 150, 0x00FB }               // lowercase U with curcumflex
        , { 151, 0x00F9 }               // lowercase U with grave
        , { 152, 0x00FF }               // lowercase Y with diaeresis
        , { 153, 0x00D6 }               // uppercase O with diaeresis
        , { 154, 0x00DC }               // uppercase U with diaeresis
        , { 155, 0x00A2 }               // cent sign
        , { 156, 0x00A3 }               // pound sign
        , { 157, 0x00A5 }               // yen sign
        , { 158, 0x20A7 }               // peseta sign
        , { 159, 0x0192 }               // lowercase F with hook
        , { 160, 0x00E1 }               // lowercase A with acute
        , { 161, 0x00ED }               // lowercase I with acute
        , { 162, 0x00F3 }               // lowercase O with acute
        , { 163, 0x00FA }               // lowercase U with acute
        , { 164, 0x00F1 }               // lowercase N with tilde
        , { 165, 0x00D1 }               // uppercase N with tilde
        , { 166, 0x00AA }               // femininie ordinal indicator
        , { 167, 0x00BA }               // masculine ordinal indicator
        , { 168, 0x00BF }               // inverted question mark
        , { 169, 0x2310 }               // reversed not sign
        , { 170, 0x00AC }               // not sign
        , { 171, 0x00BD }               // 1/2
        , { 172, 0x00BC }               // 1/4
        , { 173, 0x00A1 }               // inverted exclamation mark
        , { 174, 0x00AB }               // left pointing double angle quotation mark
        , { 175, 0x00BB }               // right pointing double angle quotation mark
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
        , { 224, 0x03B1 }               // greek small alpha
        , { 225, 0x00DF }               // greek small beta
        , { 226, 0x0393 }               // light down and right
        , { 227, 0x03C0 }               // pi symbol
        , { 228, 0x03A3 }               // n-arry summation
        , { 229, 0x03C3 }               // greek small sigma
        , { 230, 0x00B5 }               // greek small mu
        , { 231, 0x03C4 }               // greek small tau
        , { 232, 0x03A6 }               // greek capital phi
        , { 233, 0x0398 }               // greek capital theta
        , { 234, 0x03A9 }               // greek capital omega
        , { 235, 0x03B4 }               // greek small delta
        , { 236, 0x221E }               // infinity
        , { 237, 0x03C6 }               // greek small phi
        , { 238, 0x03B5 }               // greek small epsilon
        , { 239, 0x2229 }               // cap or intersection
        , { 240, 0x2261 }               // greek uppercase xi
        , { 241, 0x00B1 }               // plus minus sign
        , { 242, 0x2265 }               // greater than or equal to
        , { 243, 0x2264 }               // less than or equal to
        , { 244, 0x2320 }               // top half integral
        , { 245, 0x2321 }               // bottom half integral
        , { 246, 0x00F7 }               // divide sign
        , { 247, 0x2248 }               // almost equal to
        , { 248, 0x00B0 }               // degrees
        , { 249, 0x2219 }               // bullet operator
        , { 250, 0x00B7 }               // inter punct
        , { 251, 0x221A }               // square root
        , { 252, 0x207F }                // super script lowercase n
        , { 253, 0x00B2 }               // superscript 2
        , { 254, 0x25A0 }                // middle block
    } ;

    int size = sizeof(tchars) / sizeof(sExtendedChars) ;
    for(int loop = 0 ; loop < size; loop++)
    {
        mCodePage437.push_back(tchars[loop]) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned long utf8char [in] Unicode code point to look up
///
/// @return the CP437 byte value, or 0 if not found
///
/// @brief
/// Convert a Unicode code point to its CP437 byte equivalent.
///
/////////////////////////////////////////////////////////////////////////////
unsigned char cCodePage437::toChar(unsigned long utf8char)
{
    // ASCII identity (U+0000..U+007F)
    if(utf8char < 0x80)
    {
        return static_cast<unsigned char>(utf8char) ;
    }

    size_t len = mCodePage437.size() ;

    for(size_t loop = 0; loop < len; loop++)
    {
        if(mCodePage437[loop].utf8char == utf8char)
        {
            return mCodePage437[loop].wordstarchar ;
        }
    }

    return 0 ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned char wschar [in] CP437 byte value to look up
///
/// @return the Unicode code point, or 0 if not found
///
/// @brief
/// Convert a CP437 byte value to its Unicode code point equivalent.
///
/////////////////////////////////////////////////////////////////////////////
unsigned long cCodePage437::toUTF8(unsigned char wschar)
{
    // ASCII identity (0x00-0x7F)
    if(wschar < 0x80)
    {
        return static_cast<unsigned long>(wschar) ;
    }

    size_t len = mCodePage437.size() ;

    for(size_t loop = 0; loop < len; loop++)
    {
        if(mCodePage437[loop].wordstarchar == wschar)
        {
            return mCodePage437[loop].utf8char ;
        }
    }

    return UINT32_MAX ;
}
