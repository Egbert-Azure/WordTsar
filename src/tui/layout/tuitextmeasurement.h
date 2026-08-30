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

#ifndef TUITEXTMEASUREMENT_H
#define TUITEXTMEASUREMENT_H

#include "src/core/layout/textmeasurement.h"
#include "src/tui/fonts/fontmanager.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTUITextMeasurement
///
/// @brief
/// TUI implementation of text measurement interface.
/// Uses cTUIFontManager for font metrics instead of Qt.
///
/// This class provides the TUI platform implementation of text measurement,
/// allowing the core layout engine to measure text without any Qt dependency.
/// All measurements are in twips (1/1440 inch).
///
/////////////////////////////////////////////////////////////////////////////
class cTUITextMeasurement : public cTextMeasurement
{
public:
    cTUITextMeasurement(void);
    virtual ~cTUITextMeasurement(void);

    // Initialization
    bool Initialize(void);
    void Shutdown(void);

    // Font management (TUI-specific)
    bool SetFont(const std::string& family, double pointSize,
                 bool bold = false, bool italic = false);
    bool SetFontFromDescriptor(const std::string& descriptor);

    // Text measurement (override abstract interface from cTextMeasurement)
    COORD_T GetTextWidth(const std::string& text) override;
    COORD_T GetTextWidth(const std::string& text, const std::string& font) override;
    COORD_T GetFontHeight(void) override;
    COORD_T GetFontLineSpacing(void) const override;
    COORD_T GetFontLineSpacing(const std::string& font) const override;

    // Font manager access (for cLayout to query font capabilities)
    cTUIFontManager* GetFontManager(void);

    // Background font enumeration status (delegates to font manager)
    bool IsFontEnumerationDone(void) const;
    void WaitForFontEnumeration(void);

private:
    cTUIFontManager mFontManager;       // Font measurement backend (OWNED)
    std::string mCurrentFontFamily;     // Current font family name
    double mCurrentPointSize;           // Current font size
    bool mCurrentBold;                  // Current bold state
    bool mCurrentItalic;                // Current italic state
    std::string mManagerFontDescriptor; // Tracks active font descriptor in manager (cache)
};

#endif // TUITEXTMEASUREMENT_H
