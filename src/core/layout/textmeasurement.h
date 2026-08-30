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

#ifndef TEXTMEASUREMENT_H
#define TEXTMEASUREMENT_H

#include <string>

#include "src/core/include/config.h"

// Forward declarations
class cDocument;

class cTextMeasurement
{
public:
    virtual ~cTextMeasurement(void) = default;

    // Pure virtual methods for text measurement (platform-specific)
    virtual COORD_T GetTextWidth(const std::string& text) = 0;
    virtual COORD_T GetTextWidth(const std::string& text, const std::string& font) = 0;
    virtual COORD_T GetFontHeight(void) = 0;
    virtual COORD_T GetFontLineSpacing(void) const = 0;
    virtual COORD_T GetFontLineSpacing(const std::string& font) const = 0;

    // Display character conversion (implemented in base class)
    virtual std::string GetDisplayCharacter(POSITION_T documentPos,
                                           const std::string& grapheme,
                                           cDocument* doc,
                                           eShowControl showControl) const;
};

#endif // TEXTMEASUREMENT_H
