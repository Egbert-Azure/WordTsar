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
 * @file fontutils.cpp
 *
 * @brief Font descriptor serialization utilities for the GUI layer.
 *
 * Implements the FontUtils namespace functions that convert between QFont
 * objects and a pipe-delimited string descriptor format used throughout the
 * layout and rendering systems.
 *
 * @section fontutils_format Descriptor Format
 * The pipe-delimited format is: "Name|Size|Bold|Italic|Underline|Superscript|Subscript"
 * - Name: font family name (e.g., "Courier New", "Times New Roman")
 * - Size: point size as decimal string with 1 decimal place (e.g., "12.0", "8.5")
 * - Bold/Italic/Underline/Superscript/Subscript: "0" or "1"
 * - Example: "Courier New|12.0|0|0|0|0|0"
 *
 * @section fontutils_functions Conversion Functions
 * - ToDescriptor(QFont): creates a descriptor string from a QFont object,
 *   preserving fractional point sizes that QFont::toString() would lose
 * - FromDescriptor(string): creates a QFont from a descriptor string,
 *   setting family, point size, bold, italic, and underline properties
 * - GetFontName(string): extracts just the font family name from a descriptor
 * - GetFontSize(string): extracts just the point size from a descriptor
 *
 * @section fontutils_parity GUI/TUI Parity
 * The TUI layer (TUIFontUtils namespace) uses an identical descriptor format,
 * ensuring font descriptors can be shared between GUI and TUI codebases
 * via the common cLayoutBase and cLayoutState classes.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see FontUtils Font descriptor utility namespace
 * @see cLayout GUI layout engine using font descriptors
 * @see cQtTextMeasurement Qt measurement class using font descriptors
 */

#include "fontutils.h"
#include <QString>
#include <sstream>
#include <iomanip>
#include <vector>

namespace FontUtils
{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] - QFont to convert
///
/// @return std::string - Font descriptor string
///
/// @brief
/// Converts a QFont to our custom descriptor format.
/// Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
///
/// This replaces QFont::toString() which loses fractional font sizes (8.5, 10.5, etc.)
///
/////////////////////////////////////////////////////////////////////////////
std::string FontToDescriptor(const QFont& font)
{
    std::stringstream ss;
    ss << std::fixed << std::setprecision(1);

    // Font family name
    ss << font.family().toStdString();
    ss << "|";

    // Font size (with 1 decimal place - preserves 8.5, 10.5, etc.)
    ss << font.pointSizeF();
    ss << "|";

    // Bold (weight >= 75)
    ss << (font.weight() >= QFont::Bold ? "1" : "0");
    ss << "|";

    // Italic
    ss << (font.italic() ? "1" : "0");
    ss << "|";

    // Underline
    ss << (font.underline() ? "1" : "0");
    ss << "|";

    // Superscript (placeholder - not in QFont, set by layout logic)
    ss << "0";
    ss << "|";

    // Subscript (placeholder - not in QFont, set by layout logic)
    ss << "0";

    return ss.str();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] - Font descriptor string
///
/// @return QFont - Reconstructed font
///
/// @brief
/// Converts our custom font descriptor format back to QFont.
/// Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
///
/// This replaces QFont::fromString() which loses fractional font sizes.
///
/////////////////////////////////////////////////////////////////////////////
QFont FontFromDescriptor(const std::string& descriptor)
{
    QFont font;

    if (descriptor.empty())
    {
        return font;  // Return default font
    }

    // Split descriptor on pipe
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
        font.setFamily(QString::fromStdString(fields[0]));

        // Field [1] = point size (as float with decimals!)
        try
        {
            double size = std::stod(fields[1]);
            font.setPointSizeF(size);
        }
        catch (...)
        {
            font.setPointSizeF(10.0);  // Default
        }

        // Field [2] = bold
        if (fields[2] == "1")
        {
            font.setWeight(QFont::Bold);
        }
        else
        {
            font.setWeight(QFont::Normal);
        }

        // Field [3] = italic
        font.setItalic(fields[3] == "1");

        // Field [4] = underline
        font.setUnderline(fields[4] == "1");

        // Fields [5] and [6] = superscript/subscript (not used in QFont)
        // These are handled by segment flags, not font attributes
    }

    return font;
}

} // namespace FontUtils
