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

#ifndef QTTEXTMEASUREMENT_H
#define QTTEXTMEASUREMENT_H

#include <QFont>
#include <QFontMetricsF>

#include "src/core/layout/textmeasurement.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @class cQtTextMeasurement
///
/// @brief
/// Qt-based implementation of text measurement interface.
/// Uses QFont and QFontMetricsF for accurate text width and height.
///
/// This class provides the Qt platform implementation of text measurement,
/// allowing the core layout engine to measure text without depending on Qt.
///
/////////////////////////////////////////////////////////////////////////////
class cQtTextMeasurement : public cTextMeasurement
{
public:
    cQtTextMeasurement(void);
    virtual ~cQtTextMeasurement(void);

    // Font management
    void SetFont(const QFont& font);
    QFont GetFont(void) const;

    // Text measurement (override abstract interface)
    COORD_T GetTextWidth(const std::string& text) override;
    COORD_T GetTextWidth(const std::string& text, const std::string& font) override;
    COORD_T GetFontHeight(void) override;
    COORD_T GetFontLineSpacing(void) const override;
    COORD_T GetFontLineSpacing(const std::string& font) const override;

    // Font descriptor conversion (replaces QFont::toString/fromString)
    static std::string FontToDescriptor(const QFont& font);
    static QFont FontFromDescriptor(const std::string& descriptor);

private:
    QFont mFont;                    // Current font
    QFontMetricsF* mFontMetrics;    // Font metrics for text measurement
};

#endif // QTTEXTMEASUREMENT_H
