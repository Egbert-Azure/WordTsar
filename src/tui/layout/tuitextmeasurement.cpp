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
 * @class cTUITextMeasurement
 * @brief TUI text measurement delegating to cTUIFontManager for glyph and line metrics.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Implements cTUITextMeasurement, a cTextMeasurement subclass that delegates
 * to cTUIFontManager for all font measurement operations. Provides glyph
 * width measurement, line height and spacing queries, font selection by
 * family/size/style, and descriptor-based font setting. Operates in document
 * mode (twips precision) to match the core layout engine's coordinate system.
 *
 * @see cTUITextMeasurement
 * @see cTextMeasurement
 * @see cTUIFontManager
 */

#include "tuitextmeasurement.h"
#include "tuifontutils.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor - initializes default font state
///
/////////////////////////////////////////////////////////////////////////////
cTUITextMeasurement::cTUITextMeasurement(void)
    : mCurrentPointSize(12.0), mCurrentBold(false), mCurrentItalic(false)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor - shuts down font system
///
/////////////////////////////////////////////////////////////////////////////
cTUITextMeasurement::~cTUITextMeasurement(void)
{
    Shutdown();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if initialization successful
///
/// @brief
/// Initialize the font system. Must be called before any measurements.
/// Sets up the font manager in document mode (TWIPS precision) and
/// configures the default font (Courier New 12pt).
///
/////////////////////////////////////////////////////////////////////////////
bool cTUITextMeasurement::Initialize(void)
{
    if (!mFontManager.Initialize(true))  // true = document mode (TWIPS precision)
    {
        return false;
    }

    // Set default font (Courier New 12pt to match GUI default)
    mCurrentFontFamily = "Courier New";
    mCurrentPointSize = 12.0;
    mFontManager.SetFont("Courier New", 12, false, false);

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Shutdown the font system and release resources
///
/////////////////////////////////////////////////////////////////////////////
void cTUITextMeasurement::Shutdown(void)
{
    mFontManager.Shutdown();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  family [in] Font family name
/// @param  pointSize [in] Font size in points
/// @param  bold [in] Bold flag
/// @param  italic [in] Italic flag
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Set font using explicit parameters. Updates the internal font manager
/// with the new font settings.
///
/////////////////////////////////////////////////////////////////////////////
bool cTUITextMeasurement::SetFont(const std::string& family, double pointSize,
                                   bool bold, bool italic)
{
    mCurrentFontFamily = family;
    mCurrentPointSize = pointSize;
    mCurrentBold = bold;
    mCurrentItalic = italic;
    mManagerFontDescriptor.clear();  // Invalidate descriptor cache
    return mFontManager.SetFont(family, static_cast<int>(pointSize), bold, italic);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  descriptor [in] Font descriptor string (pipe-delimited format)
///
/// @return bool [out] true if font was set successfully
///
/// @brief
/// Set font from descriptor string.
/// Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
///
/////////////////////////////////////////////////////////////////////////////
bool cTUITextMeasurement::SetFontFromDescriptor(const std::string& descriptor)
{
    sTUIFontInfo info = TUIFontUtils::FontInfoFromDescriptor(descriptor);
    mCurrentFontFamily = info.name;
    mCurrentPointSize = info.size;
    mCurrentBold = info.bold;
    mCurrentItalic = info.italic;

    // Call font manager directly (not through SetFont) to preserve descriptor cache
    bool result = mFontManager.SetFont(info.name, static_cast<int>(info.size),
                                        info.bold, info.italic);
    if (result)
    {
        mManagerFontDescriptor = descriptor;
    }
    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] Text to measure
///
/// @return COORD_T [out] Width in twips
///
/// @brief
/// Measure text width in twips using current font.
/// Delegates to cTUIFontManager which returns measurements in twips.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cTUITextMeasurement::GetTextWidth(const std::string& text)
{
    sTUITextMetrics metrics = mFontManager.MeasureText(text);
    return static_cast<COORD_T>(metrics.widthTWIPS);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] Text to measure
/// @param  font [in] Font descriptor string
///
/// @return COORD_T [out] Width in twips
///
/// @brief
/// Measure text width in twips using a specific font descriptor.
/// Temporarily switches to the requested font, measures, then restores
/// the original font.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cTUITextMeasurement::GetTextWidth(const std::string& text, const std::string& font)
{
    // Skip font switch if descriptor matches the currently active font.
    // Consecutive characters in the same font (common in LayoutDotCommandText)
    // avoid the expensive FindBestFontMatch loop entirely.
    if (font != mManagerFontDescriptor)
    {
        SetFontFromDescriptor(font);
    }

    sTUITextMetrics metrics = mFontManager.MeasureText(text);
    return static_cast<COORD_T>(metrics.widthTWIPS);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T [out] Font height in twips
///
/// @brief
/// Get font height in twips. The font manager's GetLineHeight() includes
/// ascent, descent, and leading -- matching Qt's QFontMetrics::height().
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cTUITextMeasurement::GetFontHeight(void)
{
    return static_cast<COORD_T>(mFontManager.GetLineHeight());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T [out] Line spacing in twips
///
/// @brief
/// Get font line spacing in twips for the current font.
/// Returns the font manager's line height which includes leading.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cTUITextMeasurement::GetFontLineSpacing(void) const
{
    // Cast away constness -- GetLineHeight() doesn't modify logical state
    // but is not marked const on cTUIFontManager
    cTUIFontManager& mgr = const_cast<cTUIFontManager&>(mFontManager);
    return static_cast<COORD_T>(mgr.GetLineHeight());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] Font descriptor string
///
/// @return COORD_T [out] Line spacing in twips
///
/// @brief
/// Get font line spacing in twips for a specific font.
/// Since this method is const, we temporarily switch fonts on the
/// non-const font manager by casting away constness. This matches
/// the Qt implementation which creates temporary QFontMetrics.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cTUITextMeasurement::GetFontLineSpacing(const std::string& font) const
{
    // Skip font switch if descriptor matches the currently active font
    cTUITextMeasurement* self = const_cast<cTUITextMeasurement*>(this);
    if (font != mManagerFontDescriptor)
    {
        self->SetFontFromDescriptor(font);
    }

    cTUIFontManager& mgr = const_cast<cTUIFontManager&>(mFontManager);
    return static_cast<COORD_T>(mgr.GetLineHeight());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return cTUIFontManager* [out] Pointer to font manager
///
/// @brief
/// Returns pointer to the internal font manager for direct access
/// by cLayout when querying font capabilities.
///
/////////////////////////////////////////////////////////////////////////////
cTUIFontManager* cTUITextMeasurement::GetFontManager(void)
{
    return &mFontManager;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool [out] true if background font enumeration has completed
///
/// @brief
/// Delegates to cTUIFontManager::IsFontEnumerationDone().
///
/////////////////////////////////////////////////////////////////////////////
bool cTUITextMeasurement::IsFontEnumerationDone(void) const
{
    return mFontManager.IsFontEnumerationDone();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Blocks until background font enumeration finishes.
/// Delegates to cTUIFontManager::WaitForFontEnumeration().
///
/////////////////////////////////////////////////////////////////////////////
void cTUITextMeasurement::WaitForFontEnumeration(void)
{
    mFontManager.WaitForFontEnumeration();
}
