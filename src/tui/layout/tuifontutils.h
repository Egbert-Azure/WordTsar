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

#ifndef TUIFONTUTILS_H
#define TUIFONTUTILS_H

#include <string>
#include "src/tui/fonts/tuifontcalc.h"   // sTUIFontInfo

namespace TUIFontUtils
{
    // Convert font info to descriptor string
    // Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
    // Identical format to FontUtils::FontToDescriptor() in GUI
    std::string FontToDescriptor(const sTUIFontInfo& font);

    // Convenience overload matching GUI cLayout usage
    std::string FontToDescriptor(const std::string& family, double pointSize,
                                  bool bold, bool italic, bool underline);

    // Convert descriptor string to sTUIFontInfo
    sTUIFontInfo FontInfoFromDescriptor(const std::string& descriptor);

    // Extract just the font family from a descriptor
    std::string GetFamilyFromDescriptor(const std::string& descriptor);

    // Extract just the point size from a descriptor
    double GetSizeFromDescriptor(const std::string& descriptor);

    // Check if descriptor has bold flag set
    bool IsBoldInDescriptor(const std::string& descriptor);

    // Check if descriptor has italic flag set
    bool IsItalicInDescriptor(const std::string& descriptor);

    // Check if descriptor has underline flag set
    bool IsUnderlineInDescriptor(const std::string& descriptor);

} // namespace TUIFontUtils

#endif // TUIFONTUTILS_H
