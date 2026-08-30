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
 * @class cQtTextMeasurement
 *
 * @brief Qt-based text measurement implementation using QFontMetricsF.
 *
 * Implements the cQtTextMeasurement class, the Qt backend for the
 * cTextMeasurement interface. All measurements are converted from Qt's
 * pixel-based units to twips for use by the layout engine.
 *
 * @section qtmeas_width Text Width Measurement
 * - GetTextWidth(): measures a string's width in twips using QFontMetricsF
 * - GetTextWidthArray(): returns per-grapheme width array for segment building
 * - Uses QFontMetricsF::horizontalAdvance() for accurate glyph widths
 *
 * @section qtmeas_height Line Height and Spacing
 * - GetFontLineSpacing(): returns the font's line spacing in twips
 *   (ascent + descent + leading from QFontMetricsF)
 * - GetFontAscent()/GetFontDescent(): individual metrics for baseline positioning
 *
 * @section qtmeas_fonts Font Management
 * - SetFont(): converts a pipe-delimited font descriptor to a QFont object
 *   via FontUtils::FromDescriptor() and creates a QFontMetricsF for measurement
 * - GetFont(): returns the current font descriptor string
 * - Font descriptor round-tripping preserves fractional point sizes
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cQtTextMeasurement Qt text measurement class
 * @see cTextMeasurement Base text measurement interface
 * @see FontUtils Font descriptor conversion namespace
 * @see cLayout GUI layout engine using this measurement backend
 */

#include "qttextmeasurement.h"
#include "src/gui/utils/fontutils.h"

#include <QString>

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor - initializes font to Courier New 12pt and creates
/// font metrics for text measurement.
///
/////////////////////////////////////////////////////////////////////////////
cQtTextMeasurement::cQtTextMeasurement(void)
{
    // Initialize with a fixed monospace font
    mFont.setFamily("Courier New");
    mFont.setPointSizeF(12.0);
    mFont.setStyleHint(QFont::TypeWriter);  // Fallback to monospace if Courier New not available

    // Create font metrics
    mFontMetrics = new QFontMetricsF(mFont);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor - cleans up font metrics
///
/////////////////////////////////////////////////////////////////////////////
cQtTextMeasurement::~cQtTextMeasurement(void)
{
    delete mFontMetrics;
    mFontMetrics = nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] Qt font to use for measurement
///
/// @return nothing
///
/// @brief
/// Sets the font used for text measurement.
/// Updates the font metrics to match the new font.
///
/////////////////////////////////////////////////////////////////////////////
void cQtTextMeasurement::SetFont(const QFont& font)
{
    mFont = font;

    // Recreate font metrics with new font
    delete mFontMetrics;
    mFontMetrics = new QFontMetricsF(mFont);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current QFont
///
/// @brief
/// Returns the current font used for measurement
///
/////////////////////////////////////////////////////////////////////////////
QFont cQtTextMeasurement::GetFont(void) const
{
    return mFont;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] text to measure
///
/// @return width in twips
///
/// @brief
/// Measures text width using current font.
/// Converts from pixels to twips using FONTSCALE.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cQtTextMeasurement::GetTextWidth(const std::string& text)
{
    // Convert UTF-8 string to QString for Qt measurement
    QString qtext = QString::fromUtf8(text.c_str());

    // Measure width in pixels
    qreal pixelWidth = mFontMetrics->horizontalAdvance(qtext);

    // Convert pixels to twips using FONTSCALE
    COORD_T twipWidth = static_cast<COORD_T>(pixelWidth * FONTSCALE);

    return twipWidth;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] text to measure
/// @param  font [in] font specification string (Qt format)
///
/// @return width in twips
///
/// @brief
/// Measures text width using a specific font instead of current mFont.
/// Used when segment font differs from current layout state (e.g., dot commands).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cQtTextMeasurement::GetTextWidth(const std::string& text, const std::string& font)
{
    // Parse font string to QFont
    QFont measureFont = FontUtils::FontFromDescriptor(font);

    // Create temporary metrics for this specific font
    QFontMetricsF tempMetrics(measureFont);

    // Convert UTF-8 string to QString
    QString qtext = QString::fromUtf8(text.c_str());

    // Measure width in pixels
    qreal pixelWidth = tempMetrics.horizontalAdvance(qtext);

    // Convert pixels to twips using FONTSCALE
    COORD_T twipWidth = static_cast<COORD_T>(pixelWidth * FONTSCALE);

    return twipWidth;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return height in twips
///
/// @brief
/// Returns the height of the current font in twips.
/// Converts from pixels to twips using FONTSCALE.
///
/// Font height includes ascent, descent, and leading. This is used
/// for line spacing in the layout engine.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cQtTextMeasurement::GetFontHeight(void)
{
    // Get font height in pixels
    qreal pixelHeight = mFontMetrics->height();

    // Convert pixels to twips using FONTSCALE
    COORD_T twipHeight = static_cast<COORD_T>(pixelHeight * FONTSCALE);

    return twipHeight;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return line spacing in twips
///
/// @brief
/// Returns the font's recommended line spacing (height + leading) in twips.
/// This is the natural baseline-to-baseline distance recommended by the
/// font designer, providing proper inter-line spacing for readability.
///
/// Used when line height is NOT_SET (no .LH command) to provide
/// typographically correct spacing like modern word processors.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cQtTextMeasurement::GetFontLineSpacing(void) const
{
    // Get font's recommended line spacing in pixels
    // lineSpacing() = ascent() + descent() + leading()
    qreal pixelLineSpacing = mFontMetrics->lineSpacing();

    // Convert pixels to twips using FONTSCALE
    COORD_T twipLineSpacing = static_cast<COORD_T>(pixelLineSpacing * FONTSCALE);

    return twipLineSpacing;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] font descriptor string
///
/// @return line spacing in twips
///
/// @brief
/// Returns the line spacing for a specific font (without changing current font).
/// Used for dot commands/comments which always use the default font.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cQtTextMeasurement::GetFontLineSpacing(const std::string& font) const
{
    // Parse font string to QFont
    QFont measureFont = FontUtils::FontFromDescriptor(font);

    // Create temporary metrics for this specific font
    QFontMetricsF tempMetrics(measureFont);

    // Get font's recommended line spacing in pixels
    qreal pixelLineSpacing = tempMetrics.lineSpacing();

    // Convert pixels to twips using FONTSCALE
    return static_cast<COORD_T>(pixelLineSpacing * FONTSCALE);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] QFont to convert
///
/// @return font descriptor string
///
/// @brief
/// Converts QFont to descriptor string.
/// Delegates to FontUtils for implementation.
///
/////////////////////////////////////////////////////////////////////////////
std::string cQtTextMeasurement::FontToDescriptor(const QFont& font)
{
    return FontUtils::FontToDescriptor(font);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] font descriptor string
///
/// @return QFont
///
/// @brief
/// Converts descriptor string to QFont.
/// Delegates to FontUtils for implementation.
///
/////////////////////////////////////////////////////////////////////////////
QFont cQtTextMeasurement::FontFromDescriptor(const std::string& descriptor)
{
    return FontUtils::FontFromDescriptor(descriptor);
}
