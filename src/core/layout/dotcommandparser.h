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

#ifndef SRC_CORE_LAYOUT_DOTCOMMANDPARSER_H
#define SRC_CORE_LAYOUT_DOTCOMMANDPARSER_H

#include <string>

#include "src/core/include/config.h"
#include "src/core/layout/layoutstructs.h"

// Forward declarations
class cLayoutState;
class cDocument;
class cLayoutBase;


class cDotCommandParser
{
public:
    cDotCommandParser(cLayoutState* state, cDocument* document,
                      PAGE_T& currentPage, PAGE_T& logicalPageNumber,
                      cLayoutBase* layout);
    ~cDotCommandParser(void);

    // Main entry point for dot command parsing
    eDotCommandStatus ParseDotCommand(const std::string& command);

    // Document management
    void SetDocument(cDocument* document);

    // Page number formatting helpers (public for testing)
    std::string ToRomanNumeralLower(PAGE_T num) const;
    std::string ToRomanNumeralUpper(PAGE_T num) const;
    std::string FormatPageNumber(PAGE_T page, ePageNumberFormat format) const;

protected:
    // Individual dot command parsers (protected for testing)
    // NOTE: Complex methods (ParsePageBreak, ParseConditionalPageBreak, ParseHeader,
    //       ParseFooter) remain in cLayoutBase because they access layout internals
    eDotCommandStatus ParseLeftMargin(const std::string& command);
    eDotCommandStatus ParseRightMargin(const std::string& command);
    eDotCommandStatus ParseParagraphMargin(const std::string& command);
    eDotCommandStatus ParsePageOffset(const std::string& command);
    eDotCommandStatus ParsePageLength(const std::string& command);
    eDotCommandStatus ParseLineHeight(const std::string& command);
    eDotCommandStatus ParsePageNumber(const std::string& command);
    eDotCommandStatus ParseWordWrap(const std::string& command);
    eDotCommandStatus ParsePrinterOrientation(const std::string& command);
    eDotCommandStatus ParseOmitPageNumbers(const std::string& command);
    eDotCommandStatus ParsePrintPageNumbers(const std::string& command);
    eDotCommandStatus ParseMarginTop(const std::string& command);
    eDotCommandStatus ParseMarginBottom(const std::string& command);
    eDotCommandStatus ParseHeaderMargin(const std::string& command);
    eDotCommandStatus ParseFooterMargin(const std::string& command);
    eDotCommandStatus ParseJustification(const std::string& command);
    eDotCommandStatus ParseCenter(const std::string& command);
    eDotCommandStatus ParseLineSpacing(const std::string& command);
    eDotCommandStatus ParseParagraphSpacing(const std::string& command);
    eDotCommandStatus ParseRuler(const std::string& command);
    eDotCommandStatus ParseTabs(const std::string& command);
    eDotCommandStatus ParseSubSuperRoll(const std::string& command);

    // Helper method for GetLineHeight() - needs callback or direct access
    COORD_T GetLineHeight(void) const;

private:
    cLayoutState* mLayoutState;  // NOT owned (passed in constructor)
    cDocument* mDocument;         // NOT owned (passed in constructor)

    // TODO/FIXME: These are layout runtime variables that should NOT be in the parser
    // They're here temporarily because ParsePageNumber() modifies them
    // This violates separation of concerns and needs refactoring
    PAGE_T& mCurrentPage;         // Reference to layoutbase's mCurrentPage
    PAGE_T& mLogicalPageNumber;   // Reference to layoutbase's mLogicalPageNumber

    // Layout instance for calling GetLineHeight() when needed
    cLayoutBase* mLayout;        // NOT owned (passed in constructor)
};

#endif // SRC_CORE_LAYOUT_DOTCOMMANDPARSER_H
