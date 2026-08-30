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

#ifndef TUI_PLATFORM_QT_H
#define TUI_PLATFORM_QT_H

#include "tuifontcalc.h"
#include <vector>

#ifdef HAVE_QT_CORE1
#include <QString>
#endif

// Forward declarations to avoid Qt dependency in header
class QCoreApplication;
class QFontMetrics;
class QFont;

// Qt Headless font calculator - uses Qt without GUI widgets
class cTUIFontCalculatorQtHeadless : public cTUIFontCalculator
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cTUIFontCalculatorQtHeadless(void);
    virtual ~cTUIFontCalculatorQtHeadless(void);

    // cTUIFontCalculator interface
    bool Initialize(void) override;
    void Shutdown(void) override;

    bool SetFont(const sTUIFontInfo& font) override;
    std::vector<std::string> GetAvailableFonts(void) override;

    // Enhanced font enumeration with style information
    std::vector<sTUIAvailableFontInfo> GetAvailableFontsWithStyle(void);

    sTUITextMetrics MeasureText(const std::string& text) override;
    int MeasureCharacterWidth(char32_t codepoint) override;
    int GetLineHeight(void) override;

    bool SupportsUnicode() const override { return true; }
    bool SupportsCharacter(char32_t codepoint) override;

private:
    // Helper functions
    int PixelsToTWIPS(int pixels) const;
    bool CreateQtFont(const std::string& fontName, int pointSize);
    bool CreateQtFontWithStyle(const std::string& fontName, int pointSize,
                              bool bold = false, bool italic = false);
    bool InitializeQtApplication(void);

    // Style classification helpers (Qt-specific implementations use QString)
#ifdef HAVE_QT_CORE
    eTUIFontStyleHint ClassifyFontStyle(const QString& family) const;
    eTUIFontSpacing GetFontSpacing(const QString& family) const;
#else
    eTUIFontStyleHint ClassifyFontStyle(const std::string& family) const;
    eTUIFontSpacing GetFontSpacing(const std::string& family) const;
#endif

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    QCoreApplication* mQtApp;
    QFontMetrics* mMetrics;
    QFont* mFont;
    bool mOwnsApplication;

    static int sArgc;
    static char* sArgv[];
};

#endif // TUI_PLATFORM_QT_H