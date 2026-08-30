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

#ifndef TUI_LAYOUT_H
#define TUI_LAYOUT_H

#include "src/core/layout/layoutbase.h"
#include "src/tui/fonts/tuifontcalc.h"    // sTUIFontInfo

// Forward declarations
class cTUITextMeasurement;
class cTUIFontManager;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cLayout
///
/// @brief
/// TUI implementation of the layout engine.
/// Provides text measurement using the TUI font system (HarfBuzz,
/// STB TrueType, or built-in metrics) instead of Qt.
///
/// This is the TUI counterpart of gui/layout/layout.h. Both classes
/// extend cLayoutBase and implement the same three pure virtual methods.
/// The TUI version uses cTUIFontManager for all font operations instead
/// of QFont/QFontMetricsF.
///
/////////////////////////////////////////////////////////////////////////////
class cLayout : public cLayoutBase
{
public:
    cLayout(void);
    virtual ~cLayout(void);

    // Initialization (must be called before LayoutDocument)
    bool InitializeFontSystem(void);
    void ShutdownFontSystem(void);

    // Font management (TUI-specific)
    void SetFont(const std::string& family, double pointSize,
                 bool bold = false, bool italic = false);

    // Access to font manager (for PDF font embedding)
    cTUIFontManager* GetFontManager(void);

    // Font enumeration (delegates to font manager)
    std::vector<std::string> GetFontFamilies(void);

    // Background font enumeration status (delegates to text measurement)
    bool IsFontEnumerationDone(void) const;
    void WaitForFontEnumeration(void);

    // Override base class to update TUI font when string font changes
    void SetDefaultFont(const std::string& font) override;

    // Pure virtual implementations from cLayoutBase
    // Font-aware paragraph segmentation with measurement
    std::vector<sSegmentLayout> BuildParagraphSegments(PARAGRAPH_T paragraphNum) override;

    // Header/footer segmentation
    std::vector<sSegmentLayout> BuildHeaderFooterSegments(
        PARAGRAPH_T sourceParagraph,
        POSITION_T sourceStartPos,
        PAGE_T page,
        std::vector<std::string>& outGraphemes) override;

    // Font formatting helper
    std::string GetFontWithFormatting(bool bold, bool italic, bool underline) override;

protected:
    // Font management helper (mirrors GUI's UpdateCurrentFont)
    void UpdateCurrentFont(void);

private:
    // Segment building helpers (same interface as GUI)
    void InitializeSegment(sSegmentLayout& seg, PARAGRAPH_T paragraph,
                           POSITION_T startPosition, COORD_T lineHeight);
    void ApplyFormattingControlCode(eModifiers controlType, POSITION_T documentPos);
    sSegmentLayout CreateTabSegment(PARAGRAPH_T paragraph, POSITION_T startPosition,
                                      POSITION_T documentPos, const sWSTab& tabInfo,
                                      COORD_T lineHeight);
    void AddGraphemeToSegment(sSegmentLayout& seg, COORD_T& currentX,
                              const std::string& grapheme);
    void FlushSegmentIfNonEmpty(std::vector<sSegmentLayout>& segments,
                                sSegmentLayout& segment, COORD_T currentX);

    // Font state
    sTUIFontInfo mCurrentFontInfo;             // Current font (replaces QFont mFont)

    // Text measurement
    cTUITextMeasurement* mTUITextMeasurement;   // TUI text measurement (OWNED)
};

#endif // TUI_LAYOUT_H
