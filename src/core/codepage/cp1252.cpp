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
 * @class cCodePage1252
 *
 * @brief Windows-1252 code page to Unicode conversion.
 *
 * Implements the cCodePageWin1252 class which provides bidirectional mapping
 * between Windows-1252 (Western European) byte values and Unicode codepoints.
 * The constructor populates an sExtendedChars lookup table for the 0x80-0x9F
 * range only; bytes 0x00-0x7F and 0xA0-0xFF are identity-mapped to Unicode
 * Latin-1 Supplement and require no explicit entries.
 *
 * @section cp1252_mapping Mapping Details
 * - Only 27 bytes in the 0x80-0x9F range need explicit mapping
 * - Bytes 0x81, 0x8D, 0x8F, and 0x90 are undefined in Windows-1252
 * - Includes Euro sign (0x80 -> U+20AC), smart quotes, and typographic marks
 * - Bytes 0xA0-0xFF map directly to their Unicode equivalents (no table needed)
 *
 * @section cp1252_usage Usage Context
 * Windows-1252 is the default encoding for Western European Windows systems.
 * WordStar documents created or converted on Windows platforms may use this
 * encoding for extended characters.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cCodePageWin1252 Windows-1252 converter class
 * @see sExtendedChars Byte-to-Unicode mapping entry structure
 */

#include "cp1252.h"

#include <cstdint>



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Populates the Windows-1252 to Unicode mapping table.
/// Only bytes 0x80-0x9F differ from Unicode/Latin-1.
/// Bytes 0x81, 0x8D, 0x8F, 0x90 are undefined in Windows-1252.
///
/////////////////////////////////////////////////////////////////////////////
cCodePageWin1252::cCodePageWin1252(void)
{
    // Windows-1252 bytes 0x80-0x9F that differ from Unicode
    // Bytes 0xA0-0xFF are identical to Unicode (Latin-1 Supplement)
    static const sExtendedChars tchars[] =
    {
          { 0x80, 0x20AC }               // euro sign
        , { 0x82, 0x201A }               // single low-9 quotation mark
        , { 0x83, 0x0192 }               // latin small letter f with hook
        , { 0x84, 0x201E }               // double low-9 quotation mark
        , { 0x85, 0x2026 }               // horizontal ellipsis
        , { 0x86, 0x2020 }               // dagger
        , { 0x87, 0x2021 }               // double dagger
        , { 0x88, 0x02C6 }               // modifier letter circumflex accent
        , { 0x89, 0x2030 }               // per mille sign
        , { 0x8A, 0x0160 }               // latin capital letter s with caron
        , { 0x8B, 0x2039 }               // single left-pointing angle quotation mark
        , { 0x8C, 0x0152 }               // latin capital ligature oe
        , { 0x8E, 0x017D }               // latin capital letter z with caron
        , { 0x91, 0x2018 }               // left single quotation mark
        , { 0x92, 0x2019 }               // right single quotation mark
        , { 0x93, 0x201C }               // left double quotation mark
        , { 0x94, 0x201D }               // right double quotation mark
        , { 0x95, 0x2022 }               // bullet
        , { 0x96, 0x2013 }               // en dash
        , { 0x97, 0x2014 }               // em dash
        , { 0x98, 0x02DC }               // small tilde
        , { 0x99, 0x2122 }               // trade mark sign
        , { 0x9A, 0x0161 }               // latin small letter s with caron
        , { 0x9B, 0x203A }               // single right-pointing angle quotation mark
        , { 0x9C, 0x0153 }               // latin small ligature oe
        , { 0x9E, 0x017E }               // latin small letter z with caron
        , { 0x9F, 0x0178 }               // latin capital letter y with diaeresis
    } ;

    int size = sizeof(tchars) / sizeof(sExtendedChars) ;
    for(int loop = 0 ; loop < size ; loop++)
    {
        mCodePage1252.push_back(tchars[loop]) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned long utf8char [in] Unicode codepoint
///
/// @return Windows-1252 byte value, or 0 if not found
///
/// @brief
/// Converts a Unicode codepoint to a Windows-1252 byte value.
/// Only handles the 0x80-0x9F range; 0xA0-0xFF are identity mapped.
///
/////////////////////////////////////////////////////////////////////////////
unsigned char cCodePageWin1252::toChar(unsigned long utf8char)
{
    // ASCII identity (U+0000..U+007F)
    if(utf8char < 0x80)
    {
        return static_cast<unsigned char>(utf8char) ;
    }

    // Identity map for 0xA0-0xFF (Latin-1 Supplement matches Unicode)
    if(utf8char >= 0xA0 && utf8char <= 0xFF)
    {
        return static_cast<unsigned char>(utf8char) ;
    }

    // Search the 0x80-0x9F mapping table
    size_t len = mCodePage1252.size() ;
    for(size_t loop = 0 ; loop < len ; loop++)
    {
        if(mCodePage1252[loop].utf8char == utf8char)
        {
            return mCodePage1252[loop].wordstarchar ;
        }
    }

    return 0 ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  unsigned char wschar [in] Windows-1252 byte value
///
/// @return Unicode codepoint, or 0 if undefined
///
/// @brief
/// Converts a Windows-1252 byte value to a Unicode codepoint.
/// Bytes 0xA0-0xFF are identity mapped (Latin-1 = Unicode).
/// Bytes 0x81, 0x8D, 0x8F, 0x90 are undefined and return 0.
///
/////////////////////////////////////////////////////////////////////////////
unsigned long cCodePageWin1252::toUTF8(unsigned char wschar)
{
    // ASCII identity (0x00-0x7F)
    if(wschar < 0x80)
    {
        return static_cast<unsigned long>(wschar) ;
    }

    // Identity map for 0xA0-0xFF (Latin-1 Supplement matches Unicode)
    if(wschar >= 0xA0)
    {
        return static_cast<unsigned long>(wschar) ;
    }

    // Search the 0x80-0x9F mapping table
    size_t len = mCodePage1252.size() ;
    for(size_t loop = 0 ; loop < len ; loop++)
    {
        if(mCodePage1252[loop].wordstarchar == wschar)
        {
            return mCodePage1252[loop].utf8char ;
        }
    }

    return UINT32_MAX ;
}
