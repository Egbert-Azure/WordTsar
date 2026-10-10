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

#ifndef HEADERFOOTERMANAGER_H
#define HEADERFOOTERMANAGER_H

#include <array>
#include <deque>
#include <vector>
#include <string>
#include "layoutstructs.h"
#include "src/core/include/config.h"

// Forward declarations
class cLayoutBase;

#define MAX_HEADER_FOOTER 5

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sHeaderFooterLine
///
/// @brief
/// Pairs a layout line with its pre-rendered graphemes.
/// Header/footer text is not part of the document, so graphemes must be
/// stored separately rather than read from cDocument during rendering.
///
/////////////////////////////////////////////////////////////////////////////
struct sHeaderFooterLine
{
    sLineLayout line;
    std::vector<std::string> graphemes;     // Pre-rendered graphemes for display
};


class cHeaderFooterManager
{
public:
    cHeaderFooterManager(cLayoutBase* layout);
    ~cHeaderFooterManager(void);

    // Definition parsing (.HE / .FO / .Hn / .Fn)
    void HandleHeaderFooterText(const std::string& text, const std::string& command, int number);

    // Lines of one page, built on demand
    std::vector<sHeaderFooterLine> GetPageLines(PAGE_T page, bool isHeader);

private:
    sHeaderFooterLine LayoutHeaderFooterText(const sHeaderFooterDefinition& definition, PARAGRAPH_T paragraph,
                                             COORD_T ypos, PAGE_T page);
    sHeaderFooterLine AutomaticPageNumber(PAGE_T page, COORD_T footerBottom);
    std::array<PARAGRAPH_T, MAX_HEADER_FOOTER> SelectDefinitions(PAGE_T page, bool isHeader) const;
    COORD_T HeaderFooterLineHeight(const sHeaderFooterDefinition& definition, const std::deque<sSegmentLayout>& segments) const;

    // References (NOT owned)
    cLayoutBase* mLayoutBase;
};

#endif // HEADERFOOTERMANAGER_H
