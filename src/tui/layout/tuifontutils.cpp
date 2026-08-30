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
 * @file tuifontutils.cpp
 * @brief TUI font descriptor serialization between sTUIFontInfo and pipe-delimited strings.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements the TUIFontUtils namespace, which converts between sTUIFontInfo
 * structs and pipe-delimited descriptor strings of the form
 * "FontName|Size|Bold|Italic|Underline|Superscript|Subscript". The format
 * is identical to the GUI's FontUtils::FontToDescriptor() so that descriptors
 * stored in layout segments are interchangeable between the GUI and TUI builds.
 *
 * @section tuifontutils_format Descriptor Format
 * Seven pipe-separated fields: font family name (string), point size (float),
 * bold (0/1), italic (0/1), underline (0/1), superscript (0/1), subscript
 * (0/1). Parsing is tolerant of missing trailing fields, defaulting them
 * to zero.
 *
 * @see TUIFontUtils
 * @see sTUIFontInfo
 */

#include "tuifontutils.h"

#include <sstream>
#include <iomanip>
#include <vector>

namespace TUIFontUtils
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] - sTUIFontInfo to convert
///
/// @return std::string - Font descriptor string
///
/// @brief
/// Converts an sTUIFontInfo to our custom descriptor format.
/// Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
///
/// This produces the identical format as the GUI's FontUtils::FontToDescriptor().
/// Both GUI and TUI must produce compatible descriptors because they are stored
/// in segment.font fields read by the core layout engine.
///
/////////////////////////////////////////////////////////////////////////////
std::string FontToDescriptor(const sTUIFontInfo& font)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    // Font family name
    ss << font.name;
    ss << "|";

    // Font size (with 1 decimal place - preserves 8.5, 10.5, etc.)
    ss << static_cast<double>(font.size);
    ss << "|";

    // Bold
    ss << (font.bold ? "1" : "0");
    ss << "|";

    // Italic
    ss << (font.italic ? "1" : "0");
    ss << "|";

    // Underline
    ss << (font.underline ? "1" : "0");
    ss << "|";

    // Superscript (placeholder - stored in segment flags, not font)
    ss << "0";
    ss << "|";

    // Subscript (placeholder - stored in segment flags, not font)
    ss << "0";

    return ss.str();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  family [in] - Font family name
/// @param  pointSize [in] - Font size in points
/// @param  bold [in] - Bold flag
/// @param  italic [in] - Italic flag
/// @param  underline [in] - Underline flag
///
/// @return std::string - Font descriptor string
///
/// @brief
/// Convenience overload that builds descriptor from individual parameters.
///
/////////////////////////////////////////////////////////////////////////////
std::string FontToDescriptor(const std::string& family, double pointSize,
                              bool bold, bool italic, bool underline)
{
    sTUIFontInfo font;
    font.name = family;
    font.size = static_cast<int>(pointSize);
    font.bold = bold;
    font.italic = italic;
    font.underline = underline;
    return FontToDescriptor(font);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return sTUIFontInfo - Parsed font information
///
/// @brief
/// Converts our custom font descriptor format to sTUIFontInfo.
/// Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
///
/// Handles empty or malformed descriptors by returning default values.
///
/////////////////////////////////////////////////////////////////////////////
sTUIFontInfo FontInfoFromDescriptor(const std::string& descriptor)
{
    sTUIFontInfo font;

    if (descriptor.empty())
    {
        return font;  // Return defaults (Courier New 12pt, no styles)
    }

    // Split descriptor on pipe delimiter
    std::vector<std::string> fields;
    std::string field;
    for (char ch : descriptor)
    {
        if (ch == '|')
        {
            fields.push_back(field);
            field.clear();
        }
        else
        {
            field += ch;
        }
    }
    fields.push_back(field);  // Add last field

    // Need at least 5 fields (name, size, bold, italic, underline)
    if (fields.size() >= 5)
    {
        // Field [0] = font family name
        font.name = fields[0];

        // Field [1] = point size (as float with decimals)
        try
        {
            double size = std::stod(fields[1]);
            font.size = static_cast<int>(size);
        }
        catch (...)
        {
            font.size = 12;  // Default
        }

        // Field [2] = bold
        font.bold = (fields[2] == "1");

        // Field [3] = italic
        font.italic = (fields[3] == "1");

        // Field [4] = underline
        font.underline = (fields[4] == "1");

        // Fields [5] and [6] = superscript/subscript
        // These are handled by segment flags, not font attributes
    }

    return font;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return std::string - Font family name
///
/// @brief
/// Extracts just the font family name from a descriptor string.
///
/////////////////////////////////////////////////////////////////////////////
std::string GetFamilyFromDescriptor(const std::string& descriptor)
{
    if (descriptor.empty())
    {
        return "";
    }

    // Family is everything before the first pipe
    size_t pipePos = descriptor.find('|');
    if (pipePos == std::string::npos)
    {
        return descriptor;
    }
    return descriptor.substr(0, pipePos);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return double - Point size
///
/// @brief
/// Extracts the point size from a descriptor string.
///
/////////////////////////////////////////////////////////////////////////////
double GetSizeFromDescriptor(const std::string& descriptor)
{
    if (descriptor.empty())
    {
        return 12.0;
    }

    // Size is the second field (after first pipe)
    size_t firstPipe = descriptor.find('|');
    if (firstPipe == std::string::npos)
    {
        return 12.0;
    }

    size_t secondPipe = descriptor.find('|', firstPipe + 1);
    std::string sizeStr;
    if (secondPipe == std::string::npos)
    {
        sizeStr = descriptor.substr(firstPipe + 1);
    }
    else
    {
        sizeStr = descriptor.substr(firstPipe + 1, secondPipe - firstPipe - 1);
    }

    try
    {
        return std::stod(sizeStr);
    }
    catch (...)
    {
        return 12.0;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return bool - True if bold flag is set
///
/// @brief
/// Checks if the bold flag is set in a font descriptor string.
///
/////////////////////////////////////////////////////////////////////////////
bool IsBoldInDescriptor(const std::string& descriptor)
{
    sTUIFontInfo info = FontInfoFromDescriptor(descriptor);
    return info.bold;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return bool - True if italic flag is set
///
/// @brief
/// Checks if the italic flag is set in a font descriptor string.
///
/////////////////////////////////////////////////////////////////////////////
bool IsItalicInDescriptor(const std::string& descriptor)
{
    sTUIFontInfo info = FontInfoFromDescriptor(descriptor);
    return info.italic;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return bool - True if underline flag is set
///
/// @brief
/// Checks if the underline flag is set in a font descriptor string.
///
/////////////////////////////////////////////////////////////////////////////
bool IsUnderlineInDescriptor(const std::string& descriptor)
{
    sTUIFontInfo info = FontInfoFromDescriptor(descriptor);
    return info.underline;
}

} // namespace TUIFontUtils
