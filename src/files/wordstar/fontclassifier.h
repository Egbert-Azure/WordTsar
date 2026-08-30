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

#ifndef FONTCLASSIFIER_H
#define FONTCLASSIFIER_H

#include <string>
#include <vector>

//---------------------------------------------------------------------
// Enumerations and Structures
//---------------------------------------------------------------------

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eFontStyle
///
/// @brief
/// Font visual style categories for font classification.
/// Used by cFontClassifier to map font names to style families.
///
/////////////////////////////////////////////////////////////////////////////
enum eFontStyle
{
    STYLE_UNKNOWN,
    STYLE_SANS,
    STYLE_SERIF,
    STYLE_SCRIPT
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sFontProperties
///
/// @brief
/// Classified font properties returned by cFontClassifier.
/// Identifies a font's visual style (sans, serif, script),
/// whether it is proportional, and whether it is a math or symbol font.
///
/////////////////////////////////////////////////////////////////////////////
struct sFontProperties
{
    eFontStyle style;
    bool proportional;
    bool math;
    bool symbol;
};

//---------------------------------------------------------------------
// cFontClassifier Class Declaration
//---------------------------------------------------------------------

class cFontClassifier {
public:
    sFontProperties classify(const std::string & fontName);

private:
    eFontStyle classifyStyle(const std::string & fontName);
    bool isProportional(const std::string & fontName);
    bool checkMath(const std::string & fontName);
    bool checkSymbol(const std::string & fontName);
};

#endif // FONTCLASSIFIER_H
