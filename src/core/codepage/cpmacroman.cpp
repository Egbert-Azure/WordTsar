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
 * @class cCodePageMacRoman
 *
 * @brief Mac Roman code page to Unicode conversion.
 *
 * Implements the cCodePageMacRoman class which provides bidirectional mapping
 * between Mac Roman (the classic Macintosh character encoding) byte values
 * and Unicode codepoints. The constructor populates an sExtendedChars lookup
 * table for the full 0x80-0xFF range.
 *
 * @section macroman_coverage Character Coverage
 * - Accented Latin characters for Western European languages (0x80-0xAF)
 * - Typographic symbols: smart quotes, em/en dashes, ligatures (0xC7-0xD5)
 * - Mathematical operators: not-equal, less/greater-or-equal, pi (0xAD-0xC6)
 * - Greek letters commonly used in mathematics (0xB0-0xBC)
 * - Apple logo private-use character (0xF0 -> U+F8FF)
 *
 * @section macroman_usage Usage Context
 * Mac Roman was the default encoding on classic Mac OS (pre-OS X) systems.
 * WordStar documents originating from Macintosh platforms use this encoding
 * for extended characters and typographic symbols.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCodePageMacRoman Mac Roman converter class
 * @see sExtendedChars Byte-to-Unicode mapping entry structure
 */

#include "cpmacroman.h"

#include <cstdint>



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Populates the Mac Roman to Unicode mapping table.
/// Mac Roman maps bytes 0x80-0xFF to various characters.
///
/////////////////////////////////////////////////////////////////////////////
cCodePageMacRoman::cCodePageMacRoman(void)
{
    static const sExtendedChars tchars[] =
    {
          { 0x80, 0x00C4 }               // latin capital letter a with diaeresis
        , { 0x81, 0x00C5 }               // latin capital letter a with ring above
        , { 0x82, 0x00C7 }               // latin capital letter c with cedilla
        , { 0x83, 0x00C9 }               // latin capital letter e with acute
        , { 0x84, 0x00D1 }               // latin capital letter n with tilde
        , { 0x85, 0x00D6 }               // latin capital letter o with diaeresis
        , { 0x86, 0x00DC }               // latin capital letter u with diaeresis
        , { 0x87, 0x00E1 }               // latin small letter a with acute
        , { 0x88, 0x00E0 }               // latin small letter a with grave
        , { 0x89, 0x00E2 }               // latin small letter a with circumflex
        , { 0x8A, 0x00E4 }               // latin small letter a with diaeresis
        , { 0x8B, 0x00E3 }               // latin small letter a with tilde
        , { 0x8C, 0x00E5 }               // latin small letter a with ring above
        , { 0x8D, 0x00E7 }               // latin small letter c with cedilla
        , { 0x8E, 0x00E9 }               // latin small letter e with acute
        , { 0x8F, 0x00E8 }               // latin small letter e with grave
        , { 0x90, 0x00EA }               // latin small letter e with circumflex
        , { 0x91, 0x00EB }               // latin small letter e with diaeresis
        , { 0x92, 0x00ED }               // latin small letter i with acute
        , { 0x93, 0x00EC }               // latin small letter i with grave
        , { 0x94, 0x00EE }               // latin small letter i with circumflex
        , { 0x95, 0x00EF }               // latin small letter i with diaeresis
        , { 0x96, 0x00F1 }               // latin small letter n with tilde
        , { 0x97, 0x00F3 }               // latin small letter o with acute
        , { 0x98, 0x00F2 }               // latin small letter o with grave
        , { 0x99, 0x00F4 }               // latin small letter o with circumflex
        , { 0x9A, 0x00F6 }               // latin small letter o with diaeresis
        , { 0x9B, 0x00F5 }               // latin small letter o with tilde
        , { 0x9C, 0x00FA }               // latin small letter u with acute
        , { 0x9D, 0x00F9 }               // latin small letter u with grave
        , { 0x9E, 0x00FB }               // latin small letter u with circumflex
        , { 0x9F, 0x00FC }               // latin small letter u with diaeresis
        , { 0xA0, 0x2020 }               // dagger
        , { 0xA1, 0x00B0 }               // degree sign
        , { 0xA2, 0x00A2 }               // cent sign
        , { 0xA3, 0x00A3 }               // pound sign
        , { 0xA4, 0x00A7 }               // section sign
        , { 0xA5, 0x2022 }               // bullet
        , { 0xA6, 0x00B6 }               // pilcrow sign
        , { 0xA7, 0x00DF }               // latin small letter sharp s
        , { 0xA8, 0x00AE }               // registered sign
        , { 0xA9, 0x00A9 }               // copyright sign
        , { 0xAA, 0x2122 }               // trade mark sign
        , { 0xAB, 0x00B4 }               // acute accent
        , { 0xAC, 0x00A8 }               // diaeresis
        , { 0xAD, 0x2260 }               // not equal to
        , { 0xAE, 0x00C6 }               // latin capital letter ae
        , { 0xAF, 0x00D8 }               // latin capital letter o with stroke
        , { 0xB0, 0x221E }               // infinity
        , { 0xB1, 0x00B1 }               // plus-minus sign
        , { 0xB2, 0x2264 }               // less-than or equal to
        , { 0xB3, 0x2265 }               // greater-than or equal to
        , { 0xB4, 0x00A5 }               // yen sign
        , { 0xB5, 0x00B5 }               // micro sign
        , { 0xB6, 0x2202 }               // partial differential
        , { 0xB7, 0x2211 }               // n-ary summation
        , { 0xB8, 0x220F }               // n-ary product
        , { 0xB9, 0x03C0 }               // greek small letter pi
        , { 0xBA, 0x222B }               // integral
        , { 0xBB, 0x00AA }               // feminine ordinal indicator
        , { 0xBC, 0x00BA }               // masculine ordinal indicator
        , { 0xBD, 0x03A9 }               // greek capital letter omega
        , { 0xBE, 0x00E6 }               // latin small letter ae
        , { 0xBF, 0x00F8 }               // latin small letter o with stroke
        , { 0xC0, 0x00BF }               // inverted question mark
        , { 0xC1, 0x00A1 }               // inverted exclamation mark
        , { 0xC2, 0x00AC }               // not sign
        , { 0xC3, 0x221A }               // square root
        , { 0xC4, 0x0192 }               // latin small letter f with hook
        , { 0xC5, 0x2248 }               // almost equal to
        , { 0xC6, 0x2206 }               // increment
        , { 0xC7, 0x00AB }               // left-pointing double angle quotation mark
        , { 0xC8, 0x00BB }               // right-pointing double angle quotation mark
        , { 0xC9, 0x2026 }               // horizontal ellipsis
        , { 0xCA, 0x00A0 }               // no-break space
        , { 0xCB, 0x00C0 }               // latin capital letter a with grave
        , { 0xCC, 0x00C3 }               // latin capital letter a with tilde
        , { 0xCD, 0x00D5 }               // latin capital letter o with tilde
        , { 0xCE, 0x0152 }               // latin capital ligature oe
        , { 0xCF, 0x0153 }               // latin small ligature oe
        , { 0xD0, 0x2013 }               // en dash
        , { 0xD1, 0x2014 }               // em dash
        , { 0xD2, 0x201C }               // left double quotation mark
        , { 0xD3, 0x201D }               // right double quotation mark
        , { 0xD4, 0x2018 }               // left single quotation mark
        , { 0xD5, 0x2019 }               // right single quotation mark
        , { 0xD6, 0x00F7 }               // division sign
        , { 0xD7, 0x25CA }               // lozenge
        , { 0xD8, 0x00FF }               // latin small letter y with diaeresis
        , { 0xD9, 0x0178 }               // latin capital letter y with diaeresis
        , { 0xDA, 0x2044 }               // fraction slash
        , { 0xDB, 0x20AC }               // euro sign
        , { 0xDC, 0x2039 }               // single left-pointing angle quotation mark
        , { 0xDD, 0x203A }               // single right-pointing angle quotation mark
        , { 0xDE, 0xFB01 }               // latin small ligature fi
        , { 0xDF, 0xFB02 }               // latin small ligature fl
        , { 0xE0, 0x2021 }               // double dagger
        , { 0xE1, 0x00B7 }               // middle dot
        , { 0xE2, 0x201A }               // single low-9 quotation mark
        , { 0xE3, 0x201E }               // double low-9 quotation mark
        , { 0xE4, 0x2030 }               // per mille sign
        , { 0xE5, 0x00C2 }               // latin capital letter a with circumflex
        , { 0xE6, 0x00CA }               // latin capital letter e with circumflex
        , { 0xE7, 0x00C1 }               // latin capital letter a with acute
        , { 0xE8, 0x00CB }               // latin capital letter e with diaeresis
        , { 0xE9, 0x00C8 }               // latin capital letter e with grave
        , { 0xEA, 0x00CD }               // latin capital letter i with acute
        , { 0xEB, 0x00CE }               // latin capital letter i with circumflex
        , { 0xEC, 0x00CF }               // latin capital letter i with diaeresis
        , { 0xED, 0x00CC }               // latin capital letter i with grave
        , { 0xEE, 0x00D3 }               // latin capital letter o with acute
        , { 0xEF, 0x00D4 }               // latin capital letter o with circumflex
        , { 0xF0, 0xF8FF }               // apple logo (private use area)
        , { 0xF1, 0x00D2 }               // latin capital letter o with grave
        , { 0xF2, 0x00DA }               // latin capital letter u with acute
        , { 0xF3, 0x00DB }               // latin capital letter u with circumflex
        , { 0xF4, 0x00D9 }               // latin capital letter u with grave
        , { 0xF5, 0x0131 }               // latin small letter dotless i
        , { 0xF6, 0x02C6 }               // modifier letter circumflex accent
        , { 0xF7, 0x02DC }               // small tilde
        , { 0xF8, 0x00AF }               // macron
        , { 0xF9, 0x02D8 }               // breve
        , { 0xFA, 0x02D9 }               // dot above
        , { 0xFB, 0x02DA }               // ring above
        , { 0xFC, 0x00B8 }               // cedilla
        , { 0xFD, 0x02DD }               // double acute accent
        , { 0xFE, 0x02DB }               // ogonek
        , { 0xFF, 0x02C7 }               // caron
    } ;

    int size = sizeof(tchars) / sizeof(sExtendedChars) ;
    for(int loop = 0 ; loop < size ; loop++)
    {
        mCodePageMacRoman.push_back(tchars[loop]) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned long utf8char [in] Unicode codepoint
///
/// @return Mac Roman byte value, or 0 if not found
///
/// @brief
/// Converts a Unicode codepoint to a Mac Roman byte value
///
/////////////////////////////////////////////////////////////////////////////
unsigned char cCodePageMacRoman::toChar(unsigned long utf8char)
{
    // ASCII identity (U+0000..U+007F)
    if(utf8char < 0x80)
    {
        return static_cast<unsigned char>(utf8char) ;
    }

    size_t len = mCodePageMacRoman.size() ;

    for(size_t loop = 0 ; loop < len ; loop++)
    {
        if(mCodePageMacRoman[loop].utf8char == utf8char)
        {
            return mCodePageMacRoman[loop].wordstarchar ;
        }
    }

    return 0 ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned char wschar [in] Mac Roman byte value
///
/// @return Unicode codepoint, or 0 if not found
///
/// @brief
/// Converts a Mac Roman byte value to a Unicode codepoint
///
/////////////////////////////////////////////////////////////////////////////
unsigned long cCodePageMacRoman::toUTF8(unsigned char wschar)
{
    // ASCII identity (0x00-0x7F)
    if(wschar < 0x80)
    {
        return static_cast<unsigned long>(wschar) ;
    }

    size_t len = mCodePageMacRoman.size() ;

    for(size_t loop = 0 ; loop < len ; loop++)
    {
        if(mCodePageMacRoman[loop].wordstarchar == wschar)
        {
            return mCodePageMacRoman[loop].utf8char ;
        }
    }

    return UINT32_MAX ;
}
