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
 * @class cTextMeasurement
 *
 * @brief Implements the base text measurement interface for converting graphemes to display strings.
 *
 * Implements the non-virtual methods of the cTextMeasurement abstract class.
 * Provides GetDisplayCharacter(), which maps internal document graphemes to
 * their visible display representations. Derived classes (Qt, HarfBuzz,
 * STB, built-in) implement the pure virtual text width and font height
 * measurement methods.
 *
 * @section textmeas_display Display Character Mapping
 * GetDisplayCharacter() translates internal document representations into
 * visible characters based on the current control code visibility mode:
 * - MARKER_CHAR lookup: maps WordStar control codes to display symbols
 *   (^B for bold, ^Y for italic, ^S for underline, etc.)
 * - Tab types: regular tab (>), center tab (!), right-align tab ([),
 *   decimal tab (#)
 * - Font change markers: shows font name in brackets
 * - Block selection markers: begin/end block indicators
 * - Saved position markers: ^0 through ^9
 *
 * @section textmeas_inheritance Inheritance Model
 * cTextMeasurement defines pure virtual methods for platform-specific
 * measurement: GetTextWidth() for string width in twips, GetFontLineSpacing()
 * for line height, and SetFont()/GetFont() for font management. Each platform
 * backend (Qt, HarfBuzz/FreeType, STB TrueType, built-in metrics) provides
 * a concrete implementation.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cTextMeasurement Base text measurement class
 * @see cDocument Document providing graphemes and control codes
 * @see eShowControl Control code display mode enumeration
 * @see cLayoutBase Layout engine using text measurement
 */

#include "textmeasurement.h"

#include <sstream>
#include <iomanip>
#include "src/core/document/document.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @param  POSITION_T documentPos [in] position in document
/// @param  const std::string& grapheme [in] grapheme to display
/// @param  cDocument* doc [in] document for control code lookup
/// @param  eShowControl showControl [in] control code display mode
///
/// @return std::string
///
/// @brief
/// Convert grapheme to displayable string, handling control codes.
/// Maps internal MARKER_CHAR and control characters to visible symbols.
///
/// @note
/// - MARKER_CHAR (1 byte) looks up control type in document
/// - Control codes (< 32) display as ^X format (e.g., ^B for bold)
/// - STYLE_TAB displays as >, !, [, or # based on tab type
/// - STYLE_FONT1 displays as <fontname size>
/// - REPLACE_CHAR displays as < for block start marker
/// - All other graphemes are returned unchanged
///
/////////////////////////////////////////////////////////////////////////////
std::string cTextMeasurement::GetDisplayCharacter(POSITION_T documentPos,
                                                  const std::string& grapheme,
                                                  cDocument* doc,
                                                  eShowControl showControl) const
{
    (void)showControl;  // Parameter available for future use

    if (grapheme.empty())
    {
        return grapheme;
    }

    // Handle REPLACE_CHAR (block-begin marker) FIRST
    // REPLACE_CHAR is 0, which would otherwise be caught by isControlType check (0 < 32)
    if (grapheme[0] == REPLACE_CHAR)
    {
        if (doc &&
            doc->mStartBlock == documentPos &&
            doc->mBlockSet == false)
        {
            return "<";
        }
        return grapheme;
    }

    // Handle SAVE_CHAR (^K0..^K9 saved-position bookmarks)
    if (grapheme[0] == SAVE_CHAR)
    {
        if (doc)
        {
            for (int i = 0; i < 10; i++)
            {
                if (doc->mSavePosition[i] == documentPos)
                {
                    return std::string(1, '0' + i);
                }
            }
        }
        return grapheme;
    }

    // Handle control codes (two cases):
    // 1. grapheme[0] == MARKER_CHAR: internal representation, need to call GetControlChar
    // 2. grapheme[0] < 32 && length == 1: already converted to control type (when showControl == SHOW_ALL)
    bool isMarkerChar = (grapheme[0] == MARKER_CHAR && grapheme.length() == 1);
    bool isControlType = (grapheme.length() == 1 && static_cast<unsigned char>(grapheme[0]) < 32);

    if ((isMarkerChar || isControlType) && doc)
    {
        eModifiers controlType;

        if (isMarkerChar)
        {
            // Internal representation - need to look up control type
            controlType = doc->GetControlChar(documentPos);
        }
        else
        {
            // Already converted to control type by GetChar when showControl == SHOW_ALL
            controlType = static_cast<eModifiers>(grapheme[0]);
        }

        // Special handling for tabs - map to specific display characters
        if (controlType == STYLE_TAB)
        {
            // When control codes are hidden, tabs display as spaces (invisible)
            if (showControl != SHOW_ALL)
            {
                return " ";
            }

            sWSTab tabInfo = doc->GetTab(documentPos);

            // Determine display character based on tab type
            char displayChar;
            switch (tabInfo.type)
            {
                case TAB_TAB:
                {
                    displayChar = '>';
                    break;
                }
                case TAB_CENTER:
                {
                    displayChar = '!';
                    break;
                }
                case TAB_RIGHT:
                case TAB_RIGHT1:
                {
                    displayChar = '[';
                    break;
                }
                case TAB_DECIMAL:
                {
                    displayChar = '#';
                    break;
                }
                default:
                {
                    displayChar = '?';  // Default to TAB_TAB display
                    break;
                }
            }

            return std::string(1, displayChar);
        }

        // Special handling for font changes - show font name and size
        if (controlType == STYLE_FONT1)
        {
            sInternalFonts fontInfo;
            if (doc->GetFont(documentPos, fontInfo))
            {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(1);
                ss << "<" << fontInfo.name << " " << fontInfo.size << ">";
                return ss.str();
            }
            else
            {
                return "<Unknown>";
            }
        }

        // Generic handling for other control codes (bold, italic, etc.)
        // ^B (STYLE_BOLD=2) becomes 2+64='B'
        // ^Y (STYLE_ITALICS=25) becomes 25+64='Y'
        char displayChar = static_cast<char>(controlType) + '@';
        return std::string(1, displayChar);
    }

    // Return original grapheme for all other cases
    return grapheme;
}
