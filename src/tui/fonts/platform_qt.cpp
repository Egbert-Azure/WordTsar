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
 * @class cTUIFontCalculatorQtHeadless
 * @brief Qt headless font measurement backend (currently disabled in TUI builds).
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUIFontCalculatorQtHeadless, a cTUIFontCalculator subclass that
 * uses a headless QCoreApplication with QFontMetrics to measure glyph
 * widths, line heights, ascent, and descent without requiring a display
 * server. Conditionally compiled only when Qt is available (HAVE_QT_CORE1).
 * Currently disabled in the TUI build to avoid QGuiApplication crashes;
 * HarfBuzz or STB backends are preferred instead.
 *
 * @note This backend is gated by the HAVE_QT_CORE1 preprocessor define,
 * which is intentionally different from HAVE_QT_CORE to prevent accidental
 * activation. The TUI build does not define this symbol.
 *
 * @see cTUIFontCalculatorQtHeadless
 * @see cTUIFontCalculator
 */

#include "platform_qt.h"

// Only compile if Qt is available - detected at build time
#ifdef HAVE_QT_CORE1

#include <QCoreApplication>
#include <QFontMetrics>
#include <QFont>
#include <QFontDatabase>
#include <QFontInfo>
#include <QString>

// Static members for Qt application
int cTUIFontCalculatorQtHeadless::sArgc = 1;
char* cTUIFontCalculatorQtHeadless::sArgv[] = {const_cast<char*>("ws")};

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor for Qt headless font calculator
/// Initializes Qt backend without GUI components for server environments
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorQtHeadless::cTUIFontCalculatorQtHeadless(void)
    : mQtApp(nullptr), mMetrics(nullptr), mFont(nullptr), mOwnsApplication(false) {
    mBackend = FONT_BACKEND_QT_HEADLESS;
    mMode = FONT_MODE_DOCUMENT;
    mDocumentMode = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for Qt headless font calculator
/// Ensures proper cleanup of Qt objects and application instance
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontCalculatorQtHeadless::~cTUIFontCalculatorQtHeadless(void) {
    Shutdown();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initializes the Qt headless font measurement system
/// Creates Qt application instance and sets default font
///
/// @note Requires Qt libraries to be available at runtime
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorQtHeadless::Initialize(void) {
    if (!InitializeQtApplication()) {
        return false;
    }
    
    // Set default font
    sTUIFontInfo defaultFont;
    defaultFont.name = "Liberation Serif";
    defaultFont.size = 12;
    
    return SetFont(defaultFont);
}

void cTUIFontCalculatorQtHeadless::Shutdown(void) {
    if (mMetrics) {
        delete mMetrics;
        mMetrics = nullptr;
    }
    
    if (mFont) {
        delete mFont;
        mFont = nullptr;
    }
    
    // Don't delete QCoreApplication if we don't own it
    if (mOwnsApplication && mQtApp) {
        delete mQtApp;
        mQtApp = nullptr;
        mOwnsApplication = false;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sTUIFontInfo& font [in] font information to set
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Sets the current font using Qt font system
/// Creates new QFont and QFontMetrics objects for measurement
///
/// @note Recreates Qt font objects when font changes
///
/////////////////////////////////////////////////////////////////////////////
bool cTUIFontCalculatorQtHeadless::SetFont(const sTUIFontInfo& font) {
    mCurrentFont = font;
    
    if (mMetrics) {
        delete mMetrics;
        mMetrics = nullptr;
    }
    
    if (mFont) {
        delete mFont;
        mFont = nullptr;
    }
    
    return CreateQtFontWithStyle(font.name, font.size, font.bold, font.italic);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::vector<std::string> [out] list of available font family names
///
/// @brief
/// Gets a list of all available font families using Qt font database
/// Uses QFontDatabase to query system fonts
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cTUIFontCalculatorQtHeadless::GetAvailableFonts(void) {
    std::vector<std::string> fonts;
    
    if (!mQtApp) return fonts;
    
    QFontDatabase database;
    QStringList families = database.families();
    
    for (const QString& family : families) {
        fonts.push_back(family.toStdString());
    }
    
    return fonts;
}

sTUITextMetrics cTUIFontCalculatorQtHeadless::MeasureText(const std::string& text) {
    sTUITextMetrics metrics;
    
    if (!mMetrics) {
        return metrics;
    }
    
    QString qtext = QString::fromStdString(text);
    QRect bounds = mMetrics->boundingRect(qtext);
    
    metrics.widthTWIPS = PixelsToTWIPS(bounds.width());
    metrics.heightTWIPS = PixelsToTWIPS(mMetrics->height());
    metrics.ascentTWIPS = PixelsToTWIPS(mMetrics->ascent());
    metrics.descentTWIPS = PixelsToTWIPS(mMetrics->descent());
    metrics.leadingTWIPS = PixelsToTWIPS(mMetrics->leading());
    
    return metrics;
}

int cTUIFontCalculatorQtHeadless::MeasureCharacterWidth(char32_t codepoint) {
    if (!mMetrics) {
        return 120; // Default fallback
    }
    
    // Convert codepoint to QString
    QString ch;
    if (codepoint <= 0xFFFF) {
        ch = QString(QChar(static_cast<ushort>(codepoint)));
    } else {
        // Handle surrogate pairs for codepoints > 0xFFFF
        codepoint -= 0x10000;
        ch = QString(QChar(static_cast<ushort>((codepoint >> 10) + 0xD800)));
        ch += QString(QChar(static_cast<ushort>((codepoint & 0x3FF) + 0xDC00)));
    }
    
    return PixelsToTWIPS(mMetrics->horizontalAdvance(ch));
}

int cTUIFontCalculatorQtHeadless::GetLineHeight(void) {
    if (!mMetrics) {
        return 240; // Default fallback
    }
    
    return PixelsToTWIPS(mMetrics->height());
}

bool cTUIFontCalculatorQtHeadless::SupportsCharacter(char32_t codepoint) {
    if (!mFont || !mMetrics) return false;
    
    // Convert codepoint to QString
    QString ch;
    if (codepoint <= 0xFFFF) {
        ch = QString(QChar(static_cast<ushort>(codepoint)));
    } else {
        // Handle surrogate pairs for codepoints > 0xFFFF
        codepoint -= 0x10000;
        ch = QString(QChar(static_cast<ushort>((codepoint >> 10) + 0xD800)));
        ch += QString(QChar(static_cast<ushort>((codepoint & 0x3FF) + 0xDC00)));
    }
    
    // Qt automatically uses font fallback, so check if we get valid metrics
    int width = mMetrics->horizontalAdvance(ch);
    return width > 0;
}

int cTUIFontCalculatorQtHeadless::PixelsToTWIPS(int pixels) const {
    // Convert pixels to TWIPS (1/1440 inch)
    // Assuming 96 DPI: 1 pixel = 96/1440 = 1/15 TWIPS
    return pixels * 15;
}

bool cTUIFontCalculatorQtHeadless::CreateQtFont(const std::string& fontName, int pointSize) {
    if (!mQtApp) return false;
    
    mFont = new QFont(QString::fromStdString(fontName), pointSize);
    if (!mFont) return false;
    
    mMetrics = new QFontMetrics(*mFont);
    if (!mMetrics) {
        delete mFont;
        mFont = nullptr;
        return false;
    }
    
    return true;
}

bool cTUIFontCalculatorQtHeadless::InitializeQtApplication(void) {
    // Check if QCoreApplication already exists
    if (QCoreApplication::instance()) {
        mQtApp = QCoreApplication::instance();
        mOwnsApplication = false;
        return true;
    }

    // Create minimal Qt application - no GUI widgets required
    mQtApp = new QCoreApplication(sArgc, sArgv);
    mOwnsApplication = true;

    return mQtApp != nullptr;
}

std::vector<sTUIAvailableFontInfo> cTUIFontCalculatorQtHeadless::GetAvailableFontsWithStyle(void) {
    std::vector<sTUIAvailableFontInfo> fonts;
    
    if (!mQtApp) return fonts;
    
    QFontDatabase database;
    QStringList families = database.families();
    
    for (const QString& family : families) {
        sTUIAvailableFontInfo info;
        info.displayName = family.toStdString();
        info.familyName = family.toStdString();
        info.styleHint = ClassifyFontStyle(family);
        info.spacing = GetFontSpacing(family);
        info.backend = FONT_BACKEND_QT_HEADLESS;
        
        // Set style flags based on classification
        info.isMonospace = (info.spacing == FONT_SPACING_MONOSPACE);
        info.isSerif = (info.styleHint == FONT_STYLE_SERIF);
        info.isSansSerif = (info.styleHint == FONT_STYLE_SANS_SERIF);
        
        fonts.push_back(info);
    }
    
    return fonts;
}

bool cTUIFontCalculatorQtHeadless::CreateQtFontWithStyle(const std::string& fontName, int pointSize, 
                                                        bool bold, bool italic) {
    if (!mQtApp) return false;
    
    mFont = new QFont(QString::fromStdString(fontName), pointSize);
    if (!mFont) return false;
    
    // Apply style hints to help Qt choose the best font
    mFont->setBold(bold);
    mFont->setItalic(italic);
    
    // Let Qt handle font fallback and substitution automatically
    mFont->setStyleStrategy(QFont::PreferDefault);
    
    mMetrics = new QFontMetrics(*mFont);
    if (!mMetrics) {
        delete mFont;
        mFont = nullptr;
        return false;
    }
    
    return true;
}

eTUIFontStyleHint cTUIFontCalculatorQtHeadless::ClassifyFontStyle(const QString& family) const {
    QString lowerFamily = family.toLower();
    
    // Check for monospace fonts first
    if (lowerFamily.contains("mono") || lowerFamily.contains("courier") || 
        lowerFamily.contains("console") || lowerFamily.contains("terminal") ||
        lowerFamily.contains("fixed")) {
        return FONT_STYLE_MONOSPACE;
    }
    
    // Check for serif fonts
    if (lowerFamily.contains("serif") && !lowerFamily.contains("sans")) {
        return FONT_STYLE_SERIF;
    }
    
    // Check for specific serif font families
    if (lowerFamily.contains("times") || lowerFamily.contains("georgia") ||
        lowerFamily.contains("palatino") || lowerFamily.contains("baskerville") ||
        lowerFamily.contains("garamond")) {
        return FONT_STYLE_SERIF;
    }
    
    // Check for sans-serif
    if (lowerFamily.contains("sans") || lowerFamily.contains("arial") ||
        lowerFamily.contains("helvetica") || lowerFamily.contains("verdana") ||
        lowerFamily.contains("calibri") || lowerFamily.contains("trebuchet")) {
        return FONT_STYLE_SANS_SERIF;
    }
    
    // Default to sans-serif for unknown fonts
    return FONT_STYLE_SANS_SERIF;
}

eTUIFontSpacing cTUIFontCalculatorQtHeadless::GetFontSpacing(const QString& family) const {
    if (!mQtApp) return FONT_SPACING_PROPORTIONAL;
    
    // Create a temporary font to check if it's monospace
    QFont testFont(family);
    QFontInfo fontInfo(testFont);
    
    if (fontInfo.fixedPitch()) {
        return FONT_SPACING_MONOSPACE;
    }
    
    return FONT_SPACING_PROPORTIONAL;
}

#else // !HAVE_QT_CORE

// Stub implementation when Qt is not available
cTUIFontCalculatorQtHeadless::cTUIFontCalculatorQtHeadless(void)
    : mQtApp(nullptr), mMetrics(nullptr), mFont(nullptr), mOwnsApplication(false) {
    mBackend = FONT_BACKEND_QT_HEADLESS;
}

cTUIFontCalculatorQtHeadless::~cTUIFontCalculatorQtHeadless(void) {}

bool cTUIFontCalculatorQtHeadless::Initialize(void) { return false; }
void cTUIFontCalculatorQtHeadless::Shutdown(void) {}
bool cTUIFontCalculatorQtHeadless::SetFont(const sTUIFontInfo&) { return false; }
std::vector<std::string> cTUIFontCalculatorQtHeadless::GetAvailableFonts(void) { return {}; }
sTUITextMetrics cTUIFontCalculatorQtHeadless::MeasureText(const std::string&) { return {}; }
int cTUIFontCalculatorQtHeadless::MeasureCharacterWidth(char32_t) { return 0; }
int cTUIFontCalculatorQtHeadless::GetLineHeight(void) { return 0; }
bool cTUIFontCalculatorQtHeadless::SupportsCharacter(char32_t) { return false; }
int cTUIFontCalculatorQtHeadless::PixelsToTWIPS(int) const { return 0; }
bool cTUIFontCalculatorQtHeadless::CreateQtFont(const std::string&, int) { return false; }
bool cTUIFontCalculatorQtHeadless::CreateQtFontWithStyle(const std::string&, int, bool, bool) { return false; }
bool cTUIFontCalculatorQtHeadless::InitializeQtApplication(void) { return false; }
std::vector<sTUIAvailableFontInfo> cTUIFontCalculatorQtHeadless::GetAvailableFontsWithStyle(void) { return {}; }
eTUIFontStyleHint cTUIFontCalculatorQtHeadless::ClassifyFontStyle(const std::string&) const { return FONT_STYLE_SANS_SERIF; }
eTUIFontSpacing cTUIFontCalculatorQtHeadless::GetFontSpacing(const std::string&) const { return FONT_SPACING_PROPORTIONAL; }

#endif // HAVE_QT_CORE