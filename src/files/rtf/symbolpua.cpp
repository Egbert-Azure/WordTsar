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
 * @file symbolpua.cpp
 *
 * @brief Adobe Symbol font PUA-to-Unicode mapping table and converters.
 *
 * Provides bidirectional mapping between Adobe Symbol font Private Use
 * Area codepoints (U+F020-U+F0FF) and their standard Unicode equivalents.
 * Many RTF documents and Word files encode Symbol font characters using
 * PUA codepoints; this module translates them to standard Unicode for
 * correct display with any Unicode font, and back to PUA for
 * Word-compatible RTF output.
 *
 * @section symbolpua_table Mapping Table
 * The gSymbolPUATable array contains 224 entries (U+F020 through U+F0FF),
 * indexed by (PUA_codepoint - 0xF020). Each entry holds the standard
 * Unicode equivalent codepoint, or 0 if no standard equivalent exists.
 * The table is sourced from unicode.org/Public/MAPPINGS/VENDORS/ADOBE/symbol.txt.
 *
 * @section symbolpua_characters Character Categories Covered
 * - Greek alphabet: capital and small letters (alpha through omega)
 * - Mathematical operators: summation, product, integral, square root,
 *   infinity, partial derivative, nabla, element-of, etc.
 * - Arrows: left, right, up, down, bidirectional
 * - Card suits: spade, heart, diamond, club
 * - Miscellaneous: bullet, degree, prime, copyright, trademark
 *
 * @section symbolpua_functions Conversion Functions
 * - SymbolPUAToUnicode(): converts a PUA codepoint (U+F0xx) to its
 *   standard Unicode equivalent for display
 * - UnicodeToSymbolPUA(): converts a standard Unicode codepoint back
 *   to the PUA equivalent for RTF output targeting Word compatibility
 * - IsSymbolPUA(): tests whether a codepoint falls in the Symbol PUA range
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see SymbolPUAToUnicode() PUA-to-Unicode converter function
 * @see UnicodeToSymbolPUA() Unicode-to-PUA converter function
 * @see IsSymbolPUA() PUA range test function
 * @see cRTFParser RTF parser using PUA conversion during import
 * @see cRTFWriter RTF writer using PUA conversion during export
 */

#include "symbolpua.h"


// Adobe Symbol font encoding to Unicode mapping table.
// Indexed by (PUA_codepoint - 0xF020), covering U+F020 through U+F0FF.
// Entry = standard Unicode codepoint, or 0 if no standard equivalent exists.
// Source: unicode.org/Public/MAPPINGS/VENDORS/ADOBE/symbol.txt
static const CHAR_T gSymbolPUATable[224] =
{
    // 0xF020-0xF02F: punctuation and operators
    0x0020,     // F020  SPACE
    0x0021,     // F021  EXCLAMATION MARK
    0x2200,     // F022  FOR ALL
    0x0023,     // F023  NUMBER SIGN
    0x2203,     // F024  THERE EXISTS
    0x0025,     // F025  PERCENT SIGN
    0x0026,     // F026  AMPERSAND
    0x220B,     // F027  CONTAINS AS MEMBER
    0x0028,     // F028  LEFT PARENTHESIS
    0x0029,     // F029  RIGHT PARENTHESIS
    0x2217,     // F02A  ASTERISK OPERATOR
    0x002B,     // F02B  PLUS SIGN
    0x002C,     // F02C  COMMA
    0x2212,     // F02D  MINUS SIGN
    0x002E,     // F02E  FULL STOP
    0x002F,     // F02F  SOLIDUS

    // 0xF030-0xF03F: digits, punctuation, relational
    0x0030,     // F030  DIGIT ZERO
    0x0031,     // F031  DIGIT ONE
    0x0032,     // F032  DIGIT TWO
    0x0033,     // F033  DIGIT THREE
    0x0034,     // F034  DIGIT FOUR
    0x0035,     // F035  DIGIT FIVE
    0x0036,     // F036  DIGIT SIX
    0x0037,     // F037  DIGIT SEVEN
    0x0038,     // F038  DIGIT EIGHT
    0x0039,     // F039  DIGIT NINE
    0x003A,     // F03A  COLON
    0x003B,     // F03B  SEMICOLON
    0x003C,     // F03C  LESS-THAN SIGN
    0x003D,     // F03D  EQUALS SIGN
    0x003E,     // F03E  GREATER-THAN SIGN
    0x003F,     // F03F  QUESTION MARK

    // 0xF040-0xF04F: Greek uppercase
    0x2245,     // F040  APPROXIMATELY EQUAL TO
    0x0391,     // F041  GREEK CAPITAL LETTER ALPHA
    0x0392,     // F042  GREEK CAPITAL LETTER BETA
    0x03A7,     // F043  GREEK CAPITAL LETTER CHI
    0x0394,     // F044  GREEK CAPITAL LETTER DELTA
    0x0395,     // F045  GREEK CAPITAL LETTER EPSILON
    0x03A6,     // F046  GREEK CAPITAL LETTER PHI
    0x0393,     // F047  GREEK CAPITAL LETTER GAMMA
    0x0397,     // F048  GREEK CAPITAL LETTER ETA
    0x0399,     // F049  GREEK CAPITAL LETTER IOTA
    0x03D1,     // F04A  GREEK THETA SYMBOL
    0x039A,     // F04B  GREEK CAPITAL LETTER KAPPA
    0x039B,     // F04C  GREEK CAPITAL LETTER LAMDA
    0x039C,     // F04D  GREEK CAPITAL LETTER MU
    0x039D,     // F04E  GREEK CAPITAL LETTER NU
    0x039F,     // F04F  GREEK CAPITAL LETTER OMICRON

    // 0xF050-0xF05F: Greek uppercase continued, brackets
    0x03A0,     // F050  GREEK CAPITAL LETTER PI
    0x0398,     // F051  GREEK CAPITAL LETTER THETA
    0x03A1,     // F052  GREEK CAPITAL LETTER RHO
    0x03A3,     // F053  GREEK CAPITAL LETTER SIGMA
    0x03A4,     // F054  GREEK CAPITAL LETTER TAU
    0x03A5,     // F055  GREEK CAPITAL LETTER UPSILON
    0x03C2,     // F056  GREEK SMALL LETTER FINAL SIGMA
    0x03A9,     // F057  GREEK CAPITAL LETTER OMEGA
    0x039E,     // F058  GREEK CAPITAL LETTER XI
    0x03A8,     // F059  GREEK CAPITAL LETTER PSI
    0x0396,     // F05A  GREEK CAPITAL LETTER ZETA
    0x005B,     // F05B  LEFT SQUARE BRACKET
    0x2234,     // F05C  THEREFORE
    0x005D,     // F05D  RIGHT SQUARE BRACKET
    0x22A5,     // F05E  UP TACK
    0x005F,     // F05F  LOW LINE

    // 0xF060-0xF06F: radical extender, Greek lowercase
    0,          // F060  (radical extender -- no standard equivalent)
    0x03B1,     // F061  GREEK SMALL LETTER ALPHA
    0x03B2,     // F062  GREEK SMALL LETTER BETA
    0x03C7,     // F063  GREEK SMALL LETTER CHI
    0x03B4,     // F064  GREEK SMALL LETTER DELTA
    0x03B5,     // F065  GREEK SMALL LETTER EPSILON
    0x03C6,     // F066  GREEK SMALL LETTER PHI
    0x03B3,     // F067  GREEK SMALL LETTER GAMMA
    0x03B7,     // F068  GREEK SMALL LETTER ETA
    0x03B9,     // F069  GREEK SMALL LETTER IOTA
    0x03D5,     // F06A  GREEK PHI SYMBOL
    0x03BA,     // F06B  GREEK SMALL LETTER KAPPA
    0x03BB,     // F06C  GREEK SMALL LETTER LAMDA
    0x03BC,     // F06D  GREEK SMALL LETTER MU
    0x03BD,     // F06E  GREEK SMALL LETTER NU
    0x03BF,     // F06F  GREEK SMALL LETTER OMICRON

    // 0xF070-0xF07F: Greek lowercase continued
    0x03C0,     // F070  GREEK SMALL LETTER PI
    0x03B8,     // F071  GREEK SMALL LETTER THETA
    0x03C1,     // F072  GREEK SMALL LETTER RHO
    0x03C3,     // F073  GREEK SMALL LETTER SIGMA
    0x03C4,     // F074  GREEK SMALL LETTER TAU
    0x03C5,     // F075  GREEK SMALL LETTER UPSILON
    0x03D6,     // F076  GREEK PI SYMBOL
    0x03C9,     // F077  GREEK SMALL LETTER OMEGA
    0x03BE,     // F078  GREEK SMALL LETTER XI
    0x03C8,     // F079  GREEK SMALL LETTER PSI
    0x03B6,     // F07A  GREEK SMALL LETTER ZETA
    0x007B,     // F07B  LEFT CURLY BRACKET
    0x007C,     // F07C  VERTICAL LINE
    0x007D,     // F07D  RIGHT CURLY BRACKET
    0x223C,     // F07E  TILDE OPERATOR
    0,          // F07F  (undefined)

    // 0xF080-0xF09F: undefined range (32 entries)
    0, 0, 0, 0, 0, 0, 0, 0,    // F080-F087
    0, 0, 0, 0, 0, 0, 0, 0,    // F088-F08F
    0, 0, 0, 0, 0, 0, 0, 0,    // F090-F097
    0, 0, 0, 0, 0, 0, 0, 0,    // F098-F09F

    // 0xF0A0-0xF0AF: special symbols, arrows
    0x20AC,     // F0A0  EURO SIGN
    0x03D2,     // F0A1  GREEK UPSILON WITH HOOK SYMBOL
    0x2032,     // F0A2  PRIME
    0x2264,     // F0A3  LESS-THAN OR EQUAL TO
    0x2044,     // F0A4  FRACTION SLASH
    0x221E,     // F0A5  INFINITY
    0x0192,     // F0A6  LATIN SMALL LETTER F WITH HOOK
    0x2663,     // F0A7  BLACK CLUB SUIT
    0x2666,     // F0A8  BLACK DIAMOND SUIT
    0x2665,     // F0A9  BLACK HEART SUIT
    0x2660,     // F0AA  BLACK SPADE SUIT
    0x2194,     // F0AB  LEFT RIGHT ARROW
    0x2190,     // F0AC  LEFTWARDS ARROW
    0x2191,     // F0AD  UPWARDS ARROW
    0x2192,     // F0AE  RIGHTWARDS ARROW
    0x2193,     // F0AF  DOWNWARDS ARROW

    // 0xF0B0-0xF0BF: math operators, bullet, ellipsis
    0x00B0,     // F0B0  DEGREE SIGN
    0x00B1,     // F0B1  PLUS-MINUS SIGN
    0x2033,     // F0B2  DOUBLE PRIME
    0x2265,     // F0B3  GREATER-THAN OR EQUAL TO
    0x00D7,     // F0B4  MULTIPLICATION SIGN
    0x221D,     // F0B5  PROPORTIONAL TO
    0x2202,     // F0B6  PARTIAL DIFFERENTIAL
    0x2022,     // F0B7  BULLET
    0x00F7,     // F0B8  DIVISION SIGN
    0x2260,     // F0B9  NOT EQUAL TO
    0x2261,     // F0BA  IDENTICAL TO
    0x2248,     // F0BB  ALMOST EQUAL TO
    0x2026,     // F0BC  HORIZONTAL ELLIPSIS
    0,          // F0BD  (vertical arrow extender -- no standard equivalent)
    0,          // F0BE  (horizontal arrow extender -- no standard equivalent)
    0x21B5,     // F0BF  DOWNWARDS ARROW WITH CORNER LEFTWARDS

    // 0xF0C0-0xF0CF: set theory, logic
    0x2135,     // F0C0  ALEF SYMBOL
    0x2111,     // F0C1  BLACK-LETTER CAPITAL I
    0x211C,     // F0C2  BLACK-LETTER CAPITAL R
    0x2118,     // F0C3  SCRIPT CAPITAL P (WEIERSTRASS)
    0x2297,     // F0C4  CIRCLED TIMES
    0x2295,     // F0C5  CIRCLED PLUS
    0x2205,     // F0C6  EMPTY SET
    0x2229,     // F0C7  INTERSECTION
    0x222A,     // F0C8  UNION
    0x2283,     // F0C9  SUPERSET OF
    0x2287,     // F0CA  SUPERSET OF OR EQUAL TO
    0x2284,     // F0CB  NOT A SUBSET OF
    0x2282,     // F0CC  SUBSET OF
    0x2286,     // F0CD  SUBSET OF OR EQUAL TO
    0x2208,     // F0CE  ELEMENT OF
    0x2209,     // F0CF  NOT AN ELEMENT OF

    // 0xF0D0-0xF0DF: math, arrows, logic
    0x2220,     // F0D0  ANGLE
    0x2207,     // F0D1  NABLA
    0x00AE,     // F0D2  REGISTERED SIGN (serif variant)
    0x00A9,     // F0D3  COPYRIGHT SIGN (serif variant)
    0x2122,     // F0D4  TRADE MARK SIGN (serif variant)
    0x220F,     // F0D5  N-ARY PRODUCT
    0x221A,     // F0D6  SQUARE ROOT
    0x22C5,     // F0D7  DOT OPERATOR
    0x00AC,     // F0D8  NOT SIGN
    0x2227,     // F0D9  LOGICAL AND
    0x2228,     // F0DA  LOGICAL OR
    0x21D4,     // F0DB  LEFT RIGHT DOUBLE ARROW
    0x21D0,     // F0DC  LEFTWARDS DOUBLE ARROW
    0x21D1,     // F0DD  UPWARDS DOUBLE ARROW
    0x21D2,     // F0DE  RIGHTWARDS DOUBLE ARROW
    0x21D3,     // F0DF  DOWNWARDS DOUBLE ARROW

    // 0xF0E0-0xF0EF: lozenge, brackets, summation, fragments
    0x25CA,     // F0E0  LOZENGE
    0x2329,     // F0E1  LEFT-POINTING ANGLE BRACKET
    0x00AE,     // F0E2  REGISTERED SIGN (sans-serif variant)
    0x00A9,     // F0E3  COPYRIGHT SIGN (sans-serif variant)
    0x2122,     // F0E4  TRADE MARK SIGN (sans-serif variant)
    0x2211,     // F0E5  N-ARY SUMMATION
    0,          // F0E6  (left parenthesis top -- rendering fragment)
    0,          // F0E7  (left parenthesis extension -- rendering fragment)
    0,          // F0E8  (left parenthesis bottom -- rendering fragment)
    0,          // F0E9  (left square bracket top -- rendering fragment)
    0,          // F0EA  (left square bracket extension -- rendering fragment)
    0,          // F0EB  (left square bracket bottom -- rendering fragment)
    0,          // F0EC  (left curly bracket top -- rendering fragment)
    0,          // F0ED  (left curly bracket mid -- rendering fragment)
    0,          // F0EE  (left curly bracket bottom -- rendering fragment)
    0,          // F0EF  (curly bracket extension -- rendering fragment)

    // 0xF0F0-0xF0FF: integral, right brackets, fragments
    0,          // F0F0  (undefined)
    0x232A,     // F0F1  RIGHT-POINTING ANGLE BRACKET
    0x222B,     // F0F2  INTEGRAL
    0x2320,     // F0F3  TOP HALF INTEGRAL
    0,          // F0F4  (integral extension -- rendering fragment)
    0x2321,     // F0F5  BOTTOM HALF INTEGRAL
    0,          // F0F6  (right parenthesis top -- rendering fragment)
    0,          // F0F7  (right parenthesis extension -- rendering fragment)
    0,          // F0F8  (right parenthesis bottom -- rendering fragment)
    0,          // F0F9  (right square bracket top -- rendering fragment)
    0,          // F0FA  (right square bracket extension -- rendering fragment)
    0,          // F0FB  (right square bracket bottom -- rendering fragment)
    0,          // F0FC  (right curly bracket top -- rendering fragment)
    0,          // F0FD  (right curly bracket mid -- rendering fragment)
    0,          // F0FE  (right curly bracket bottom -- rendering fragment)
    0           // F0FF  (undefined)
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @param  CHAR_T codepoint [in] - Unicode codepoint, possibly in PUA range
///
/// @return standard Unicode codepoint, or the input unchanged if not mappable
///
/// @brief
/// Map Symbol font PUA codepoints (U+F020-U+F0FF) to their standard
/// Unicode equivalents. RTF files from Word/LibreOffice use PUA encoding
/// for Symbol font characters. This converts them to renderable codepoints.
///
/////////////////////////////////////////////////////////////////////////////
CHAR_T SymbolPUAToUnicode(CHAR_T codepoint)
{
    if(codepoint < 0xF020 || codepoint > 0xF0FF)
    {
        return codepoint ;
    }

    CHAR_T mapped = gSymbolPUATable[codepoint - 0xF020] ;
    if(mapped == 0)
    {
        // No standard equivalent (rendering fragment or undefined)
        return codepoint ;
    }
    return mapped ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  CHAR_T codepoint [in] - standard Unicode codepoint
///
/// @return Symbol font PUA codepoint (U+F0xx), or 0 if no mapping exists
///
/// @brief
/// Reverse mapping: convert standard Unicode to Symbol font PUA encoding
/// for RTF export compatibility with Word. Scans the mapping table for
/// a match and returns the corresponding PUA codepoint.
///
/////////////////////////////////////////////////////////////////////////////
CHAR_T UnicodeToSymbolPUA(CHAR_T codepoint)
{
    // Only remap characters that are clearly Symbol font equivalents,
    // not ASCII characters that happen to appear in the table
    if(codepoint < 0x0080)
    {
        return 0 ;
    }

    // Linear scan of the table looking for a Unicode match
    for(int i = 0; i < 224; i++)
    {
        if(gSymbolPUATable[i] == codepoint)
        {
            return static_cast<CHAR_T>(0xF020 + i) ;
        }
    }

    return 0 ;
}
