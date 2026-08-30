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
 * @class cCodePage850
 *
 * @brief DOS code page 850 (Latin-1) to Unicode conversion.
 *
 * Implements the cCodePage850 class which provides bidirectional mapping
 * between CP850 byte values and Unicode codepoints. The constructor populates
 * an sExtendedChars lookup table for the full 0x80-0xFF range.
 *
 * @section cp850_coverage Character Coverage
 * - Accented Latin characters for Western European languages (0x80-0xA5)
 * - Currency symbols: cent, pound, yen, florin, generic currency (0x9B-0xCF)
 * - Box-drawing elements (subset of CP437) (0xB0-0xDA)
 * - Typographic marks: pilcrow, section sign, macron (0xDB-0xFF)
 *
 * @section cp850_differences Differences from CP437
 * Unlike CP437, CP850 replaces Greek letters and mathematical symbols with
 * additional accented Latin characters (e.g., Icelandic thorn, eth, and
 * additional diacritics), providing broader Western European language support.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCodePage850 Code page 850 converter class
 * @see sExtendedChars Byte-to-Unicode mapping entry structure
 */

#include "cp850.h"

#include <cstdint>



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Populates the CP850 to Unicode mapping table.
/// CP850 (DOS Latin-1) maps bytes 0x80-0xFF to Western European characters.
///
/////////////////////////////////////////////////////////////////////////////
cCodePage850::cCodePage850(void)
{
    static const sExtendedChars tchars[] =
    {
          { 0x80, 0x00C7 }               // latin capital letter c with cedilla
        , { 0x81, 0x00FC }               // latin small letter u with diaeresis
        , { 0x82, 0x00E9 }               // latin small letter e with acute
        , { 0x83, 0x00E2 }               // latin small letter a with circumflex
        , { 0x84, 0x00E4 }               // latin small letter a with diaeresis
        , { 0x85, 0x00E0 }               // latin small letter a with grave
        , { 0x86, 0x00E5 }               // latin small letter a with ring above
        , { 0x87, 0x00E7 }               // latin small letter c with cedilla
        , { 0x88, 0x00EA }               // latin small letter e with circumflex
        , { 0x89, 0x00EB }               // latin small letter e with diaeresis
        , { 0x8A, 0x00E8 }               // latin small letter e with grave
        , { 0x8B, 0x00EF }               // latin small letter i with diaeresis
        , { 0x8C, 0x00EE }               // latin small letter i with circumflex
        , { 0x8D, 0x00EC }               // latin small letter i with grave
        , { 0x8E, 0x00C4 }               // latin capital letter a with diaeresis
        , { 0x8F, 0x00C5 }               // latin capital letter a with ring above
        , { 0x90, 0x00C9 }               // latin capital letter e with acute
        , { 0x91, 0x00E6 }               // latin small letter ae
        , { 0x92, 0x00C6 }               // latin capital letter ae
        , { 0x93, 0x00F4 }               // latin small letter o with circumflex
        , { 0x94, 0x00F6 }               // latin small letter o with diaeresis
        , { 0x95, 0x00F2 }               // latin small letter o with grave
        , { 0x96, 0x00FB }               // latin small letter u with circumflex
        , { 0x97, 0x00F9 }               // latin small letter u with grave
        , { 0x98, 0x00FF }               // latin small letter y with diaeresis
        , { 0x99, 0x00D6 }               // latin capital letter o with diaeresis
        , { 0x9A, 0x00DC }               // latin capital letter u with diaeresis
        , { 0x9B, 0x00F8 }               // latin small letter o with stroke
        , { 0x9C, 0x00A3 }               // pound sign
        , { 0x9D, 0x00D8 }               // latin capital letter o with stroke
        , { 0x9E, 0x00D7 }               // multiplication sign
        , { 0x9F, 0x0192 }               // latin small letter f with hook
        , { 0xA0, 0x00E1 }               // latin small letter a with acute
        , { 0xA1, 0x00ED }               // latin small letter i with acute
        , { 0xA2, 0x00F3 }               // latin small letter o with acute
        , { 0xA3, 0x00FA }               // latin small letter u with acute
        , { 0xA4, 0x00F1 }               // latin small letter n with tilde
        , { 0xA5, 0x00D1 }               // latin capital letter n with tilde
        , { 0xA6, 0x00AA }               // feminine ordinal indicator
        , { 0xA7, 0x00BA }               // masculine ordinal indicator
        , { 0xA8, 0x00BF }               // inverted question mark
        , { 0xA9, 0x00AE }               // registered sign
        , { 0xAA, 0x00AC }               // not sign
        , { 0xAB, 0x00BD }               // vulgar fraction one half
        , { 0xAC, 0x00BC }               // vulgar fraction one quarter
        , { 0xAD, 0x00A1 }               // inverted exclamation mark
        , { 0xAE, 0x00AB }               // left-pointing double angle quotation mark
        , { 0xAF, 0x00BB }               // right-pointing double angle quotation mark
        , { 0xB0, 0x2591 }               // light shade
        , { 0xB1, 0x2592 }               // medium shade
        , { 0xB2, 0x2593 }               // dark shade
        , { 0xB3, 0x2502 }               // box drawings light vertical
        , { 0xB4, 0x2524 }               // box drawings light vertical and left
        , { 0xB5, 0x00C1 }               // latin capital letter a with acute
        , { 0xB6, 0x00C2 }               // latin capital letter a with circumflex
        , { 0xB7, 0x00C0 }               // latin capital letter a with grave
        , { 0xB8, 0x00A9 }               // copyright sign
        , { 0xB9, 0x2563 }               // box drawings double vertical and left
        , { 0xBA, 0x2551 }               // box drawings double vertical
        , { 0xBB, 0x2557 }               // box drawings double down and left
        , { 0xBC, 0x255D }               // box drawings double up and left
        , { 0xBD, 0x00A2 }               // cent sign
        , { 0xBE, 0x00A5 }               // yen sign
        , { 0xBF, 0x2510 }               // box drawings light down and left
        , { 0xC0, 0x2514 }               // box drawings light up and right
        , { 0xC1, 0x2534 }               // box drawings light up and horizontal
        , { 0xC2, 0x252C }               // box drawings light down and horizontal
        , { 0xC3, 0x251C }               // box drawings light vertical and right
        , { 0xC4, 0x2500 }               // box drawings light horizontal
        , { 0xC5, 0x253C }               // box drawings light vertical and horizontal
        , { 0xC6, 0x00E3 }               // latin small letter a with tilde
        , { 0xC7, 0x00C3 }               // latin capital letter a with tilde
        , { 0xC8, 0x255A }               // box drawings double up and right
        , { 0xC9, 0x2554 }               // box drawings double down and right
        , { 0xCA, 0x2569 }               // box drawings double up and horizontal
        , { 0xCB, 0x2566 }               // box drawings double down and horizontal
        , { 0xCC, 0x2560 }               // box drawings double vertical and right
        , { 0xCD, 0x2550 }               // box drawings double horizontal
        , { 0xCE, 0x256C }               // box drawings double vertical and horizontal
        , { 0xCF, 0x00A4 }               // currency sign
        , { 0xD0, 0x00F0 }               // latin small letter eth
        , { 0xD1, 0x00D0 }               // latin capital letter eth
        , { 0xD2, 0x00CA }               // latin capital letter e with circumflex
        , { 0xD3, 0x00CB }               // latin capital letter e with diaeresis
        , { 0xD4, 0x00C8 }               // latin capital letter e with grave
        , { 0xD5, 0x0131 }               // latin small letter dotless i
        , { 0xD6, 0x00CD }               // latin capital letter i with acute
        , { 0xD7, 0x00CE }               // latin capital letter i with circumflex
        , { 0xD8, 0x00CF }               // latin capital letter i with diaeresis
        , { 0xD9, 0x2518 }               // box drawings light up and left
        , { 0xDA, 0x250C }               // box drawings light down and right
        , { 0xDB, 0x2588 }               // full block
        , { 0xDC, 0x2584 }               // lower half block
        , { 0xDD, 0x00A6 }               // broken bar
        , { 0xDE, 0x00CC }               // latin capital letter i with grave
        , { 0xDF, 0x2580 }               // upper half block
        , { 0xE0, 0x00D3 }               // latin capital letter o with acute
        , { 0xE1, 0x00DF }               // latin small letter sharp s
        , { 0xE2, 0x00D4 }               // latin capital letter o with circumflex
        , { 0xE3, 0x00D2 }               // latin capital letter o with grave
        , { 0xE4, 0x00F5 }               // latin small letter o with tilde
        , { 0xE5, 0x00D5 }               // latin capital letter o with tilde
        , { 0xE6, 0x00B5 }               // micro sign
        , { 0xE7, 0x00FE }               // latin small letter thorn
        , { 0xE8, 0x00DE }               // latin capital letter thorn
        , { 0xE9, 0x00DA }               // latin capital letter u with acute
        , { 0xEA, 0x00DB }               // latin capital letter u with circumflex
        , { 0xEB, 0x00D9 }               // latin capital letter u with grave
        , { 0xEC, 0x00FD }               // latin small letter y with acute
        , { 0xED, 0x00DD }               // latin capital letter y with acute
        , { 0xEE, 0x00AF }               // macron
        , { 0xEF, 0x00B4 }               // acute accent
        , { 0xF0, 0x00AD }               // soft hyphen
        , { 0xF1, 0x00B1 }               // plus-minus sign
        , { 0xF2, 0x2017 }               // double low line
        , { 0xF3, 0x00BE }               // vulgar fraction three quarters
        , { 0xF4, 0x00B6 }               // pilcrow sign
        , { 0xF5, 0x00A7 }               // section sign
        , { 0xF6, 0x00F7 }               // division sign
        , { 0xF7, 0x00B8 }               // cedilla
        , { 0xF8, 0x00B0 }               // degree sign
        , { 0xF9, 0x00A8 }               // diaeresis
        , { 0xFA, 0x00B7 }               // middle dot
        , { 0xFB, 0x00B9 }               // superscript one
        , { 0xFC, 0x00B3 }               // superscript three
        , { 0xFD, 0x00B2 }               // superscript two
        , { 0xFE, 0x25A0 }               // black square
        , { 0xFF, 0x00A0 }               // no-break space
    } ;

    int size = sizeof(tchars) / sizeof(sExtendedChars) ;
    for(int loop = 0 ; loop < size ; loop++)
    {
        mCodePage850.push_back(tchars[loop]) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned long utf8char [in] Unicode codepoint
///
/// @return CP850 byte value, or 0 if not found
///
/// @brief
/// Converts a Unicode codepoint to a CP850 byte value
///
/////////////////////////////////////////////////////////////////////////////
unsigned char cCodePage850::toChar(unsigned long utf8char)
{
    // ASCII identity (U+0000..U+007F)
    if(utf8char < 0x80)
    {
        return static_cast<unsigned char>(utf8char) ;
    }

    size_t len = mCodePage850.size() ;

    for(size_t loop = 0 ; loop < len ; loop++)
    {
        if(mCodePage850[loop].utf8char == utf8char)
        {
            return mCodePage850[loop].wordstarchar ;
        }
    }

    return 0 ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned char wschar [in] CP850 byte value
///
/// @return Unicode codepoint, or 0 if not found
///
/// @brief
/// Converts a CP850 byte value to a Unicode codepoint
///
/////////////////////////////////////////////////////////////////////////////
unsigned long cCodePage850::toUTF8(unsigned char wschar)
{
    // ASCII identity (0x00-0x7F)
    if(wschar < 0x80)
    {
        return static_cast<unsigned long>(wschar) ;
    }

    size_t len = mCodePage850.size() ;

    for(size_t loop = 0 ; loop < len ; loop++)
    {
        if(mCodePage850[loop].wordstarchar == wschar)
        {
            return mCodePage850[loop].utf8char ;
        }
    }

    return UINT32_MAX ;
}
