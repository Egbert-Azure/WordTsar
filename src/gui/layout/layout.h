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

#ifndef LAYOUT_H
#define LAYOUT_H

#include <QFont>
#include <QFontMetricsF>

#include "src/core/layout/layoutbase.h"

// Forward declarations
class cQtTextMeasurement;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cLayout
///
/// @brief
/// Qt-based implementation of the layout engine.
/// Provides text measurement using Qt font metrics.
///
///
/////////////////////////////////////////////////////////////////////////////
class cLayout : public cLayoutBase
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cLayout(void);
    virtual ~cLayout(void);

    // Font management
    void SetFont(const QFont& font);
    QFont GetFont(void) const;

    // Override base class to update Qt font when string font changes
    void SetDefaultFont(const std::string& font) override;

    // Public for testing
    std::vector<sSegmentLayout> BuildParagraphSegments(PARAGRAPH_T paragraphNum) override;

    // Header/footer segmentation - same logic as BuildParagraphSegments
    // Processes text from sourceParagraph starting at sourceStartPos
    // Returns segments AND pre-rendered graphemes (with # expanded to page number)
    std::vector<sSegmentLayout> BuildHeaderFooterSegments(
        PARAGRAPH_T sourceParagraph,
        POSITION_T sourceStartPos,
        PAGE_T page,
        std::vector<std::string>& outGraphemes) override;

    // Font formatting helper - returns font descriptor with formatting applied
    std::string GetFontWithFormatting(bool bold, bool italic, bool underline) override;

    // Font descriptor conversion (replaces QFont::toString/fromString)
    static std::string FontToDescriptor(const QFont& font);
    static QFont FontFromDescriptor(const std::string& descriptor);

protected:
    // Font management helper
    void UpdateCurrentFont(void);

private:
    // Segment building helpers - extract common code from Build*Segments methods
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

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    QFont mFont;                        // Current font
    QFontMetricsF* mFontMetrics;        // Font metrics for text measurement
    cQtTextMeasurement* mQtTextMeasurement;  // Qt text measurement implementation (OWNED)
};

#endif // LAYOUT_H
