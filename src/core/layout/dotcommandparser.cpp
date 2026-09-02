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
 * @class cDotCommandParser
 *
 * @brief Implements the WordStar dot command parser for the layout engine.
 *
 * Parses WordStar-style dot commands (lines beginning with a period) that
 * control page formatting, margins, headers/footers, page numbering, line
 * height, and other document-level settings. Each dot command (e.g., .LM,
 * .RM, .MT, .PL, .PN) modifies the shared cLayoutState rather than layout
 * internals directly. Commands that require access to boxes, paragraph layout,
 * or header/footer storage remain in layoutbase.cpp.
 *
 * @section dotcmd_categories Supported Dot Command Categories
 * - Page geometry: .PL (page length), .MT/.MB (top/bottom margins),
 *   .PO (page offset), .HM/.FM (header/footer margins)
 * - Paragraph formatting: .LM/.RM (left/right margins), .PM (paragraph margin),
 *   .LS (line spacing), .LH (line height), .OJ (justify on/off)
 * - Headers/footers: .HE/.FO (header/footer text), .H1-.H4/.F1-.F4 (levels)
 * - Page numbering: .PN (page number), .PC (page number column),
 *   .OP (omit page numbers)
 * - Conditional operations: .CP (conditional page break)
 * - Comments: .IG (ignore/comment line), .. (alternate comment)
 *
 * @section dotcmd_dispatch Centralized dispatch
 * All dot command dispatch is centralized here. Commands that need layout
 * internals (boxes, paragraph layout, header/footer storage) call back into
 * cLayoutBase via the mLayout pointer: ParsePageBreak, ParseConditionalPageBreak,
 * ParseHeader, ParseFooter.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cDotCommandParser Dot command parser class
 * @see cLayoutState Formatting state modified by dot commands
 * @see cLayoutBase Layout engine owning this parser
 * @see cDocument Document providing paragraph text for parsing
 * @see eDotCommandStatus Return status enumeration for dot commands
 */

#include "dotcommandparser.h"
#include "layoutbase.h"
#include "layoutstate.h"
#include "src/core/document/document.h"
#include "src/core/include/utils.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdlib>
#include <cstring>

//////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION NOTES:
//
// All dot command dispatch is centralized in ParseDotCommand() below.
// Commands needing layout internals call back into cLayoutBase via mLayout:
//   - mLayout->ParsePageBreak()          - box/page management
//   - mLayout->ParseConditionalPageBreak() - remaining space calculation
//   - mLayout->ParseHeader()             - header storage
//   - mLayout->ParseFooter()             - footer storage
//
// TODO/FIXME items in this file:
//   1. GetLineHeight() - Parser calls a layoutbase method that wraps state access
//      with font-based fallback. Need to either:
//      - Add callback to layoutbase's GetLineHeight()
//      - Duplicate logic here (not ideal)
//      - Pass pre-calculated line height to parser
//
//   2. mLogicalPageNumber, mCurrentPage - Parser modifies layout runtime variables
//      in ParsePageNumber(). This violates separation of concerns (parser should
//      only modify state, not runtime). Needs refactoring but left as-is for now.
//
//////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in] pointer to layout state (not owned)
/// @param  document [in] pointer to document (not owned)
/// @param  currentPage [in/out] reference to current page number (layout runtime)
/// @param  logicalPageNumber [in/out] reference to logical page number (layout runtime)
/// @param  layout [in] pointer to layout instance for calling GetLineHeight()
///
/// @return nothing
///
/// @brief
/// Constructor for dot command parser
///
/// @note
/// The currentPage and logicalPageNumber parameters are temporary workarounds.
/// They should not be in the parser (they're layout runtime, not state), but
/// ParsePageNumber() currently modifies them. This needs refactoring.
///
/////////////////////////////////////////////////////////////////////////////
cDotCommandParser::cDotCommandParser(cLayoutState* state, cDocument* document,
                                     PAGE_T& currentPage, PAGE_T& logicalPageNumber,
                                     cLayoutBase* layout)
    : mLayoutState(state),
      mDocument(document),
      mCurrentPage(currentPage),
      mLogicalPageNumber(logicalPageNumber),
      mLayout(layout)
{
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor for dot command parser
///
/////////////////////////////////////////////////////////////////////////////
cDotCommandParser::~cDotCommandParser(void)
{
    // mLayoutState and mDocument not owned, no cleanup needed
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  document [in] pointer to document (not owned)
///
/// @return nothing
///
/// @brief
/// Sets the document pointer for parsing operations
///
/////////////////////////////////////////////////////////////////////////////
void cDotCommandParser::SetDocument(cDocument* document)
{
    mDocument = document;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string command [in] - Dot command text to parse
///
/// @return eDotCommandStatus - Command recognition status
///
/// @brief
/// Parse WordStar dot command and apply settings to layout state.
/// Handles most WordStar dot commands by dispatching to specialized parse methods.
/// Commands that require layout internals (ParsePageBreak, ParseConditionalPageBreak,
/// ParseHeader, ParseFooter) are not handled here and return DOT_NOTIMPLEMENTED.
///
/// Returns:
///   DOT_GOOD - Command recognized and successfully parsed
///   DOT_ERROR - Command recognized but has syntax/parameter errors
///   DOT_NOTIMPLEMENTED - Command is valid WordStar but not yet coded
///   DOT_UNKNOWN - Command not recognized (not in WordStar spec)
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseDotCommand(const std::string& command)
{


    if (command.length() < 3 || command[0] != '.' || mDocument == nullptr)
    {
        return DOT_ERROR;
    }

    // ".." is the other WordStar comment-line prefix, same meaning as ".IG"
    // below. It has no letter command code for the switch to dispatch on, so
    // it must be handled here rather than falling through to DOT_UNKNOWN.
    if (command[1] == '.')
    {
        return DOT_GOOD;
    }

    // Uppercase the entire command for case-insensitive parsing
    // WordStar allows both upper and lowercase unit types (i/I, c/C, m/M, etc.)
    std::string upperCmd = command;
    for (size_t i = 0; i < upperCmd.length(); i++)
    {
        upperCmd[i] = toupper(upperCmd[i]);
    }

    // Extract 2-char command code
    std::string cmdCode;
    cmdCode += upperCmd[1];
    cmdCode += upperCmd[2];

    // Dispatch based on first letter
    switch (cmdCode[0])
    {
        case 'A':
        {
            if (cmdCode == "AW")
            {
                return ParseWordWrap(upperCmd);
            }
            if (cmdCode == "AV")  // Ask for Variable (merge print)
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'B':
        {
            if (cmdCode == "BN")  // Bin Select (printer)
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "BP")  // Bidirectional Print (obsolete)
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'C':
        {
            if (cmdCode == "CP")
            {
                // Conditional page break needs layout internals, call back into layoutbase
                return mLayout->ParseConditionalPageBreak(command);
            }
            if (cmdCode == "CO")  // Columns
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "CC")  // Conditional Column Break
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "CS")  // Clear Screen (obsolete)
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "CW")  // Character Width
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "CV")  // Convert Note Type
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'D':
        {
            if (cmdCode == "DM")  // Display Message
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "DF")  // Data File (merge print)
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'E':
        {
            if (cmdCode[0] == 'E' && cmdCode[1] == '#')  // Set Endnote Value (.E#)
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "EL")  // Else (conditional logic)
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "EI")  // End If (conditional logic)
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'F':
        {
            // Footer commands need layout internals (footer storage), call back into layoutbase
            if (cmdCode == "FO" || cmdCode == "F1" || cmdCode == "F2" ||
                cmdCode == "F3" || cmdCode == "F4" || cmdCode == "F5")
            {
                return mLayout->ParseFooter(command);
            }
            if (cmdCode == "FM")
            {
                return ParseFooterMargin(upperCmd);
            }
            if (cmdCode[0] == 'F' && cmdCode[1] == '#')  // Set Footnote Numbering (.F#)
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "FI")  // File Insert
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'G':
        {
            if (cmdCode == "GO")  // Go To Top/Bottom
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'H':
        {
            // Header commands need layout internals (header storage), call back into layoutbase
            if (cmdCode == "HE" || cmdCode == "H1" || cmdCode == "H2" ||
                cmdCode == "H3" || cmdCode == "H4" || cmdCode == "H5")
            {
                return mLayout->ParseHeader(command);
            }
            if (cmdCode == "HM")
            {
                return ParseHeaderMargin(upperCmd);
            }
            if (cmdCode == "HY")  // Auto-Hyphenation
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'I':
        {
            if (cmdCode == "IF")  // If (conditional logic)
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "IG")  // Ignore (treated same as comment)
            {
                return DOT_GOOD;
            }
            if (cmdCode == "IX")  // Index entry -- collected by cTOCIndexGenerator, doesn't print
            {
                return DOT_GOOD;
            }
            break;
        }

        case 'K':
        {
            if (cmdCode == "KR")  // Kerning
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'L':
        {
            if (cmdCode == "LM")
            {
                return ParseLeftMargin(upperCmd);
            }
            if (cmdCode == "LH")
            {
                return ParseLineHeight(upperCmd);
            }
            if (cmdCode == "LS")
            {
                return ParseLineSpacing(upperCmd);
            }
            if (cmdCode == "LQ")  // Letter Quality (obsolete)
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode[0] == 'L' && cmdCode[1] == '#')  // Line Numbering (.L#)
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'M':
        {
            if (cmdCode == "MT")
            {
                return ParseMarginTop(upperCmd);
            }
            else if (cmdCode == "MB")
            {
                return ParseMarginBottom(upperCmd);
            }
            else if (cmdCode == "MA")  // Math
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'O':
        {
            if (cmdCode == "OJ")
            {
                return ParseJustification(upperCmd);
            }
            else if (cmdCode == "OC")
            {
                return ParseCenter(upperCmd);
            }
            else if (cmdCode == "OP")
            {
                return ParseOmitPageNumbers(upperCmd);
            }
            break;
        }

        case 'P':
        {
            if (cmdCode == "PM")
            {
                return ParseParagraphMargin(upperCmd);
            }
            if (cmdCode == "PO")
            {
                return ParsePageOffset(upperCmd);
            }
            if (cmdCode == "PA")
            {
                // Page break needs layout internals (box/page management), call back into layoutbase
                return mLayout->ParsePageBreak(command);
            }
            if (cmdCode == "PL")
            {
                return ParsePageLength(upperCmd);
            }
            if (cmdCode == "PN")
            {
                return ParsePageNumber(command);  // Use original case for 'i'/'I' detection
            }
            if (cmdCode == "PR")
            {
                return ParsePrinterOrientation(upperCmd);
            }
            if (cmdCode == "PG")
            {
                return ParsePrintPageNumbers(upperCmd);
            }
            if (cmdCode == "PS")
            {
                return ParseParagraphSpacing(upperCmd);
            }
            if (cmdCode == "PC")  // Page Column
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "PE")  // Print Endnotes
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "PF")  // Paragraph Realignment
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode[0] == 'P' && cmdCode[1] == '#')  // Paragraph Number (.P#)
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'R':
        {
            if (cmdCode == "RM")
            {
                return ParseRightMargin(upperCmd);
            }
            if (cmdCode == "RR")
            {
                return ParseRuler(command);  // Use original case for ruler text
            }
            if (cmdCode == "RP")  // Repeat
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "RV")  // Read Variable (merge print)
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'S':
        {
            if (cmdCode == "SR")
            {
                return ParseSubSuperRoll(upperCmd);
            }
            if (cmdCode == "SB")  // Suppress Blank Lines
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "SV")  // Set Variable (merge print)
            {
                // NOTE: takes a numeric value -- use EvaluateExpression for math support
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'T':
        {
            if (cmdCode == "TB")
            {
                return ParseTabs(upperCmd);
            }
            if (cmdCode == "TC")  // Table of contents entry (.tc, .tc1-.tc9) -- collected by cTOCIndexGenerator, doesn't print
            {
                return DOT_GOOD;
            }
            break;
        }

        case 'U':
        {
            if (cmdCode == "UJ")  // Micro Justify
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "UL")  // Continuous Underlining
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }

        case 'X':
        {
            if (cmdCode == "XE" || cmdCode == "XQ" || cmdCode == "XR" || cmdCode == "XW")
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "XL")  // Form Feed
            {
                return DOT_NOTIMPLEMENTED;
            }
            if (cmdCode == "XX")  // Strikeout Character
            {
                return DOT_NOTIMPLEMENTED;
            }
            break;
        }
    }

    // Command not recognized - this is NOT a WordStar command
    return DOT_UNKNOWN;

    // Cleanup macro
    
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .LM (left margin) command.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseLeftMargin(const std::string& command)
{
    bool incdec;
    bool hasUnits;
    std::string valueStr = command.substr(3);
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    COORD_T newMargin;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newMargin = static_cast<COORD_T>(value);
    }
    else
    {
        // bare number: 0 resets left margin, non-zero is an error
        if (value == 0.0)
        {
            mLayoutState->SetLeftMargin(0);
            return DOT_GOOD;
        }
        else
        {
            return DOT_ERROR;
        }
    }

    if (incdec)
    {
        mLayoutState->SetLeftMargin(mLayoutState->GetLeftMargin() + newMargin);
        if (mLayoutState->GetLeftMargin() < 0)
        {
            mLayoutState->SetLeftMargin(0);
        }
    }
    else
    {
        mLayoutState->SetLeftMargin(newMargin);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .RM (right margin) command.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseRightMargin(const std::string& command)
{
    bool incdec;
    bool hasUnits;
    double value = mDocument->EvaluateExpression(command.substr(3), incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    COORD_T newMargin;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newMargin = static_cast<COORD_T>(value);
    }
    else
    {
        // bare number -- treat as inches (WS7 default)
        newMargin = mDocument->ConvertToTwips(value, 'I', 0);
    }

    if (incdec)
    {
        mLayoutState->SetRightMargin(mLayoutState->GetRightMargin() + newMargin);
    }
    else
    {
        mLayoutState->SetRightMargin(newMargin);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .PM (paragraph margin) command.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseParagraphMargin(const std::string& command)
{
    bool incdec;
    bool hasUnits;
    std::string valueStr = command.substr(3);
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    COORD_T newMargin;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newMargin = static_cast<COORD_T>(value);
    }
    else
    {
        // bare number: 0 disables paragraph margin, non-zero is an error
        if (value == 0.0)
        {
            mLayoutState->SetValidParagraphMargin(false);
            return DOT_GOOD;
        }
        else
        {
            return DOT_ERROR;
        }
    }

    if (incdec)
    {
        mLayoutState->SetParagraphMargin(mLayoutState->GetParagraphMargin() + newMargin);
    }
    else
    {
        mLayoutState->SetParagraphMargin(newMargin);
    }

    mLayoutState->SetValidParagraphMargin(true);
    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .PO, .POO, .POE (page offset) commands.
/// .PO sets both odd and even, .POO sets odd only, .POE sets even only.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParsePageOffset(const std::string& command)
{
    // Check for .POO or .POE (3-letter commands)
    size_t startPos = 3;
    char thirdChar = (command.length() >= 4) ? toupper(command[3]) : '\0';

    if (thirdChar == 'O' || thirdChar == 'E')
    {
        startPos = 4;  // Skip the third letter
    }

    bool incdec;
    bool hasUnits;
    double value = mDocument->EvaluateExpression(command.substr(startPos), incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    COORD_T newOffset;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newOffset = static_cast<COORD_T>(value);
    }
    else
    {
        // bare number -- treat as inches (WS7 default)
        newOffset = mDocument->ConvertToTwips(value, 'I', 0);
    }

    if (thirdChar == 'O')
    {
        // .POO - odd pages only
        if (incdec)
        {
            mLayoutState->SetPageOffsetOdd(mLayoutState->GetPageOffsetOdd() + newOffset);
        }
        else
        {
            mLayoutState->SetPageOffsetOdd(newOffset);
        }
    }
    else if (thirdChar == 'E')
    {
        // .POE - even pages only
        if (incdec)
        {
            mLayoutState->SetPageOffsetEven(mLayoutState->GetPageOffsetEven() + newOffset);
        }
        else
        {
            mLayoutState->SetPageOffsetEven(newOffset);
        }
    }
    else
    {
        // .PO - both odd and even
        if (incdec)
        {
            mLayoutState->SetPageOffsetOdd(mLayoutState->GetPageOffsetOdd() + newOffset);
            mLayoutState->SetPageOffsetEven(mLayoutState->GetPageOffsetEven() + newOffset);
        }
        else
        {
            mLayoutState->SetPageOffsetOdd(newOffset);
            mLayoutState->SetPageOffsetEven(newOffset);
        }
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.pl 66)
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .PL (page length) command and sets printable page length in lines.
/// Standard WordStar assumes 6 lines per inch (8/48 inch per line).
///
/// Formula: mLayoutState->SetPageLength(lines * (8/48 inch) * TWIPSPERINCH
///
/// When set, page length overrides paper height if smaller:
///   bottom = min(mLayoutState->GetTopMargin() + mLayoutState->GetHeaderMargin() + mLayoutState->GetPageLength(),
///                mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin() - mLayoutState->GetFooterMargin())
///
/// Example: .pl 66 = 66 lines = 11 inches (standard for 11" paper at 6 lpi)
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParsePageLength(const std::string& command)
{
    bool incdec;
    bool hasUnits;
    std::string valueStr = command.substr(3);
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    if (value < 1.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // .PL values: with units, value is already in twips; without units, treat as lines
    COORD_T twips;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        twips = static_cast<COORD_T>(value);
    }
    else
    {
        // No unit specified -- treat as lines (WordStar default: 6 lines per inch)
        const double TWIPS_PER_LINE = (8.0 / 48.0) * TWIPSPERINCH;  // 240 twips
        twips = static_cast<COORD_T>(value * TWIPS_PER_LINE);
    }

    if (incdec)
    {
        mLayoutState->SetPageLength(mLayoutState->GetPageLength() + twips);
    }
    else
    {
        mLayoutState->SetPageLength(twips);
    }

    // Ensure non-negative
    if (mLayoutState->GetPageLength() < 0)
    {
        mLayoutState->SetPageLength(0);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.lh 8 or .lh a)
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .LH (line height) command. Sets line height in 1/48 inch increments.
/// Default is 8/48 inch (6 lines per inch).
///
/// Special value 'a' or 'A' enables auto-leading mode where line height is
/// determined by the tallest font in the line.
///
/// This is different from .LS (line spacing) which is a multiplier.
///
/// Example: .lh 10 = 10/48 inch per line (tighter than default)
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseLineHeight(const std::string& command)
{
    std::string param = command.substr(3);

    // Trim whitespace
    param.erase(0, param.find_first_not_of(" \t"));
    if (!param.empty())
    {
        param.erase(param.find_last_not_of(" \t") + 1);
    }

    // Check for auto-leading mode
    if (param == "a" || param == "A")
    {
        mLayoutState->SetAutoLeading(true);
        return DOT_GOOD;
    }

    bool incdec;
    double height = mDocument->GetValue(param, incdec);

    if (height < 1.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // Convert from 1/48 inch to twips
    const double TWIPS_PER_48TH = TWIPSPERINCH / 48.0;

    if (incdec)
    {
        mLayoutState->SetLineHeight(mLayoutState->GetLineHeight() + height * TWIPS_PER_48TH);
    }
    else
    {
        mLayoutState->SetLineHeight(height * TWIPS_PER_48TH);
    }

    // Ensure reasonable bounds
    if (mLayoutState->GetLineHeight() < 120)  // Minimum ~1/12 inch
    {
        mLayoutState->SetLineHeight(120);
    }

    mLayoutState->SetAutoLeading(false);
    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.sr 3)
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .SR (subscript/superscript roll) command. Sets vertical offset
/// for subscript and superscript text in 1/48 inch increments.
/// Default is 3/48 inch (90 twips), matching WordStar 7.0 default.
///
/// The value specifies how far up (superscript) or down (subscript) the
/// text is shifted from the baseline.
///
/// Example: .sr 3 = 3/48 inch vertical offset (default)
/// Example: .sr 5 = 5/48 inch vertical offset (more pronounced)
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseSubSuperRoll(const std::string& command)
{
    std::string param = command.substr(3);

    // Trim whitespace
    param.erase(0, param.find_first_not_of(" \t"));
    if (!param.empty())
    {
        param.erase(param.find_last_not_of(" \t") + 1);
    }

    bool incdec;
    double roll = mDocument->GetValue(param, incdec);

    if (roll < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // Convert from 1/48 inch to twips
    const double TWIPS_PER_48TH = TWIPSPERINCH / 48.0;

    if (incdec)
    {
        mLayoutState->SetSubSuperRoll(mLayoutState->GetSubSuperRoll() + roll * TWIPS_PER_48TH);
    }
    else
    {
        mLayoutState->SetSubSuperRoll(roll * TWIPS_PER_48TH);
    }

    // Ensure reasonable bounds (min 1/48 inch, max 10/48 inch)
    if (mLayoutState->GetSubSuperRoll() < 30)
    {
        mLayoutState->SetSubSuperRoll(30);
    }
    if (mLayoutState->GetSubSuperRoll() > 300)
    {
        mLayoutState->SetSubSuperRoll(300);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] - Dot command text (.pn 5 or .pn i or .pn I)
///
/// @return true if successful, false otherwise
///
/// @brief
/// Parses .PN (page number) command. Sets the starting page number and format.
/// Headers and footers will use this offset when displaying page numbers.
///
/// Example: .pn 5 makes first page display as page 5 (Arabic)
/// Example: .pn i sets Roman lowercase numbering (i, ii, iii, ...)
/// Example: .pn I sets Roman uppercase numbering (I, II, III, ...)
/// Example: .pn +2 increments page number offset by 2
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParsePageNumber(const std::string& command)
{
    std::string param = command.substr(3);

    // Trim whitespace
    param.erase(0, param.find_first_not_of(" \t"));
    if (!param.empty())
    {
        param.erase(param.find_last_not_of(" \t") + 1);
    }

    // Check for format specification (Roman numerals)
    // Must check BEFORE calling GetValue(), as 'i' and 'I' are not numeric
    if (!param.empty())
    {
        if (param[0] == 'i' && (param.length() == 1 || !isdigit(param[1])))
        {
            // .pn i - Roman lowercase, this page becomes i (1)
            PAGE_T offset = 1 - mCurrentPage;
            mLayoutState->SetPageNumFormat(PAGE_NUM_ROMAN_LOWER);
            mLayoutState->SetPageNumberOffset(offset);
            mLayoutState->AddPageNumOverride(mCurrentPage, offset, PAGE_NUM_ROMAN_LOWER);

            mLogicalPageNumber = mCurrentPage + offset;
            return DOT_GOOD;
        }
        else if (param[0] == 'I' && (param.length() == 1 || !isdigit(param[1])))
        {
            // .pn I - Roman uppercase, this page becomes I (1)
            PAGE_T offset = 1 - mCurrentPage;
            mLayoutState->SetPageNumFormat(PAGE_NUM_ROMAN_UPPER);
            mLayoutState->SetPageNumberOffset(offset);
            mLayoutState->AddPageNumOverride(mCurrentPage, offset, PAGE_NUM_ROMAN_UPPER);

            mLogicalPageNumber = mCurrentPage + offset;
            return DOT_GOOD;
        }
    }

    // Not Roman format, parse as Arabic number
    mLayoutState->SetPageNumFormat(PAGE_NUM_ARABIC);

    bool incdec;
    double pageNum = mDocument->GetValue(param, incdec);

    if (pageNum < 1.0 && !incdec)
    {
        return DOT_ERROR;
    }

    if (incdec)
    {
        // Relative change (+2 or -1)
        PAGE_T newOffset = mLayoutState->GetPageNumberOffset() + static_cast<PAGE_T>(pageNum);
        mLayoutState->SetPageNumberOffset(newOffset);
        mLayoutState->AddPageNumOverride(mCurrentPage, newOffset, PAGE_NUM_ARABIC);
    }
    else
    {
        // Absolute value (.pn 5 on page 3 means page 3 displays as 5)
        // Offset is difference between desired number and current physical page
        PAGE_T offset = static_cast<PAGE_T>(pageNum) - mCurrentPage;
        mLayoutState->SetPageNumberOffset(offset);
        mLayoutState->AddPageNumOverride(mCurrentPage, offset, PAGE_NUM_ARABIC);
    }

    // Recalculate logical page number after offset change
    mLogicalPageNumber = mCurrentPage + mLayoutState->GetPageNumberOffset();

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  num [in] - Number to convert (1-3999)
///
/// @return Roman numeral string (lowercase)
///
/// @brief
/// Converts integer to lowercase Roman numerals.
/// Uses standard subtractive notation (iv, ix, xl, xc, cd, cm).
///
/////////////////////////////////////////////////////////////////////////////
std::string cDotCommandParser::ToRomanNumeralLower(PAGE_T num) const
{
    return ::ToRomanNumeralLower(static_cast<long>(num)) ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  num [in] - Number to convert (1-3999)
///
/// @return Roman numeral string (uppercase)
///
/// @brief
/// Converts integer to uppercase Roman numerals.
/// Uses standard subtractive notation (IV, IX, XL, XC, CD, CM).
///
/////////////////////////////////////////////////////////////////////////////
std::string cDotCommandParser::ToRomanNumeralUpper(PAGE_T num) const
{
    return ::ToRomanNumeralUpper(static_cast<long>(num)) ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] - Page number to format (physical page)
/// @param  format [in] - Format type (Arabic, Roman lower/upper)
///
/// @return Formatted page number string
///
/// @brief
/// Converts page number to string in specified format.
/// Applies mLayoutState->GetPageNumberOffset() to get logical page number.
///
/// Example: page=1, offset=4 produces "5" (Arabic) or "v" (Roman lower)
///
/////////////////////////////////////////////////////////////////////////////
std::string cDotCommandParser::FormatPageNumber(PAGE_T page, ePageNumberFormat format) const
{
    // Per-page overrides cover every page: GetPageNumOffsetForPage returns the
    // sticky-forward offset, or 0 for pages before any .pn. The running global
    // offset must NOT be used here -- it reflects the layout scan position and
    // bleeds backward to earlier pages (e.g. .pn i on page 5 would push page 1
    // to "0").
    PAGE_T logicalPage = page + mLayoutState->GetPageNumOffsetForPage(page);
    ePageNumberFormat effectiveFormat;
    if (mLayoutState->HasPageNumOverrideForPage(page))
    {
        // Use the override's format for this page
        effectiveFormat = mLayoutState->GetPageNumFormatForPage(page);
    }
    else
    {
        // No override applies -- use the caller-supplied format
        effectiveFormat = format;
    }

    switch (effectiveFormat)
    {
        case PAGE_NUM_ROMAN_LOWER:
        {
            return ToRomanNumeralLower(logicalPage);
        }
        case PAGE_NUM_ROMAN_UPPER:
        {
            return ToRomanNumeralUpper(logicalPage);
        }
        case PAGE_NUM_ARABIC:
        default:
        {
            return std::to_string(logicalPage);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.aw on or .aw off)
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .AW (aligning and word wrap) command. When disabled, text is
/// treated as preformatted and will not wrap or justify.
///
/// Useful for:
/// - ASCII art tables
/// - Code blocks
/// - Preformatted text
///
/// Example: .aw off disables wrapping, .aw on re-enables
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseWordWrap(const std::string& command)
{
    std::string param = command.substr(3);

    // Trim whitespace
    param.erase(0, param.find_first_not_of(" \t"));
    if (!param.empty())
    {
        param.erase(param.find_last_not_of(" \t") + 1);
    }

    // Convert to uppercase for case-insensitive comparison
    for (size_t i = 0; i < param.length(); i++)
    {
        param[i] = toupper(param[i]);
    }

    // Check for "off" or "OFF"
    if (param.find("OFF") != std::string::npos)
    {
        mLayoutState->SetWordWrapEnabled(false);
    }
    else if (param.find("ON") != std::string::npos)
    {
        mLayoutState->SetWordWrapEnabled(true);
    }
    else if (param.empty())
    {
        // .aw with no parameter defaults to "on"
        mLayoutState->SetWordWrapEnabled(true);
    }
    else
    {
        // Unknown parameter
        return DOT_ERROR;
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.pr or=l or .pr or=p)
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .PR (printer orientation) command.
/// .pr or=l sets landscape mode (swaps width/height)
/// .pr or=p sets portrait mode (ensures width < height)
///
/// Used to rotate page dimensions for landscape printing.
///
/// @note Currently swaps global paper dimensions. Mid-document orientation
/// changes are parsed but not fully supported for rendering/printing.
///
/// @todo Future enhancement: Store per-page dimensions in sBoxes structure
/// to support mixed portrait/landscape pages. This requires:
/// - Adding paperWidth, paperHeight, landscape fields to sBoxes
/// - Updating CreatePageBox/CreateMarginBox to store dimensions
/// - Fixing DrawPageMode() to use per-page dimensions
/// - Updating print engine to handle per-page QPrinter orientation
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParsePrinterOrientation(const std::string& command)
{
    std::string param = command.substr(3);

    // Trim whitespace
    param.erase(0, param.find_first_not_of(" \t"));
    if (!param.empty())
    {
        param.erase(param.find_last_not_of(" \t") + 1);
    }

    // Convert to uppercase for case-insensitive comparison
    for (size_t i = 0; i < param.length(); i++)
    {
        param[i] = toupper(param[i]);
    }

    // Check for landscape: or=l or OR=L
    if (param.find("OR=L") != std::string::npos)
    {
        mLayoutState->SetLandscapeMode(true);
        // Swap paper width and height if currently in portrait
        if (mLayoutState->GetPaperWidth() < mLayoutState->GetPaperHeight())
        {
            COORD_T tempWidth = mLayoutState->GetPaperWidth();
            mLayoutState->SetPaperWidth(mLayoutState->GetPaperHeight());
            mLayoutState->SetPaperHeight(tempWidth);
        }
        return DOT_GOOD;
    }
    // Check for portrait: or=p or OR=P
    else if (param.find("OR=P") != std::string::npos)
    {
        mLayoutState->SetLandscapeMode(false);
        // Ensure portrait orientation (width < height)
        if (mLayoutState->GetPaperWidth() > mLayoutState->GetPaperHeight())
        {
            COORD_T tempWidth = mLayoutState->GetPaperWidth();
            mLayoutState->SetPaperWidth(mLayoutState->GetPaperHeight());
            mLayoutState->SetPaperHeight(tempWidth);
        }
        return DOT_GOOD;
    }

    // Unknown or missing parameter
    return DOT_ERROR;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.op)
///
/// @return true (always succeeds)
///
/// @brief
/// Parses .OP (omit page numbers) command.
/// Disables automatic page numbering for printed documents.
/// When disabled, no page numbers are automatically added to footers.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseOmitPageNumbers(const std::string& command)
{
    // Unused parameter
    (void)command;

    mLayoutState->SetPrintPageNumbers(false);
    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.pg)
///
/// @return true (always succeeds)
///
/// @brief
/// Parses .PG (print page numbers) command.
/// Enables automatic page numbering for printed documents.
/// When enabled and no footers defined, page numbers are added at
/// bottom center of each page.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParsePrintPageNumbers(const std::string& command)
{
    // Unused parameter
    (void)command;

    mLayoutState->SetPrintPageNumbers(true);
    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return bool - true if parsed successfully
///
/// @brief
/// Parses .MT (top margin) command. Value is in lines, multiplied by
/// current line height to get twips. Supports absolute and incremental.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseMarginTop(const std::string& command)
{
    if (command.length() < 4)
    {
        return DOT_ERROR;
    }

    bool incdec;
    bool hasUnits;
    std::string valueStr = command.substr(3);
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // .MT values: with units, value is already in twips; without units, treat as lines
    COORD_T newMargin;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newMargin = static_cast<COORD_T>(value);
    }
    else
    {
        // No unit specified -- treat as lines
        newMargin = static_cast<COORD_T>(value * GetLineHeight());
    }

    if (incdec)
    {
        mLayoutState->SetTopMargin(mLayoutState->GetTopMargin() + newMargin);
        if (mLayoutState->GetTopMargin() < 0)
        {
            mLayoutState->SetTopMargin(0);
        }
    }
    else
    {
        mLayoutState->SetTopMargin((newMargin < 0) ? 0 : newMargin);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] dot command string (e.g., ".mb 8", ".mb 1i", ".mb +2")
///
/// @return true if command parsed successfully, false otherwise
///
/// @brief
/// Parse .MB (bottom margin) command and update mLayoutState->GetBottomMargin().
/// Supports absolute values, incremental (+/-) values, and multiple units.
/// Default unit is LINES (not inches). Values are clamped to >= 0.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseMarginBottom(const std::string& command)
{
    if (command.length() < 4)
    {
        return DOT_ERROR;
    }

    bool incdec;
    bool hasUnits;
    std::string valueStr = command.substr(3);
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // .MB values: with units, value is already in twips; without units, treat as lines
    COORD_T newMargin;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newMargin = static_cast<COORD_T>(value);
    }
    else
    {
        // No unit specified -- treat as lines
        newMargin = static_cast<COORD_T>(value * GetLineHeight());
    }

    if (incdec)
    {
        mLayoutState->SetBottomMargin(mLayoutState->GetBottomMargin() + newMargin);
        if (mLayoutState->GetBottomMargin() < 0)
        {
            mLayoutState->SetBottomMargin(0);
        }
    }
    else
    {
        mLayoutState->SetBottomMargin((newMargin < 0) ? 0 : newMargin);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] dot command string (e.g., ".hm 2", ".hm 1i", ".hm +1")
///
/// @return true if command parsed successfully, false otherwise
///
/// @brief
/// Parse .HM (header margin) command and update mLayoutState->GetHeaderMargin().
/// Values without explicit units are in LINES (not inches!).
/// Supports increment/decrement with +/- prefix.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseHeaderMargin(const std::string& command)
{
    if (command.length() < 4)
    {
        return DOT_ERROR;
    }

    bool incdec;
    bool hasUnits;
    std::string valueStr = command.substr(3);
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // .HM values: with units, value is already in twips; without units, treat as lines
    COORD_T newMargin;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newMargin = static_cast<COORD_T>(value);
    }
    else
    {
        // No unit specified -- treat as lines
        newMargin = static_cast<COORD_T>(value * GetLineHeight());
    }

    if (incdec)
    {
        mLayoutState->SetHeaderMargin(mLayoutState->GetHeaderMargin() + newMargin);
        if (mLayoutState->GetHeaderMargin() < 0)
        {
            mLayoutState->SetHeaderMargin(0);
        }
    }
    else
    {
        mLayoutState->SetHeaderMargin((newMargin < 0) ? 0 : newMargin);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] dot command string (e.g., ".fm 2", ".fm 1i", ".fm +1")
///
/// @return true if command parsed successfully, false otherwise
///
/// @brief
/// Parse .FM (footer margin) command and update mLayoutState->GetFooterMargin().
/// Values without explicit units are in LINES (not inches!).
/// Supports increment/decrement with +/- prefix.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseFooterMargin(const std::string& command)
{
    if (command.length() < 4)
    {
        return DOT_ERROR;
    }

    bool incdec;
    bool hasUnits;
    std::string valueStr = command.substr(3);
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // .FM values: with units, value is already in twips; without units, treat as lines
    COORD_T newMargin;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        newMargin = static_cast<COORD_T>(value);
    }
    else
    {
        // No unit specified -- treat as lines
        newMargin = static_cast<COORD_T>(value * GetLineHeight());
    }

    if (incdec)
    {
        mLayoutState->SetFooterMargin(mLayoutState->GetFooterMargin() + newMargin);
        if (mLayoutState->GetFooterMargin() < 0)
        {
            mLayoutState->SetFooterMargin(0);
        }
    }
    else
    {
        mLayoutState->SetFooterMargin((newMargin < 0) ? 0 : newMargin);
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] dot command string (e.g., ".oj on", ".oj off", ".oj c", ".oj r")
///
/// @return true if command parsed successfully, false otherwise
///
/// @brief
/// Parse .OJ (justification) command and update mLayoutState->GetModifiers().
/// Supports on/off/c/r (case insensitive). Empty = off.
/// Only one alignment can be active at a time (mutual exclusion).
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseJustification(const std::string& command)
{
    if (command.length() < 3)
    {
        return DOT_ERROR;
    }

    // Extract argument after ".oj"
    std::string arg = command.substr(3);

    // Trim leading/trailing whitespace (including \r and \n)
    size_t start = arg.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        arg = "";
    }
    else
    {
        size_t end = arg.find_last_not_of(" \t\r\n");
        arg = arg.substr(start, end - start + 1);
    }

    // Convert to uppercase for case-insensitive comparison
    std::string upperArg = arg;
    for (char& c : upperArg)
    {
        c = std::toupper(c);
    }

    // Reset all alignment flags (mutual exclusion)
    sModifiers modifiers = mLayoutState->GetModifiers();
    modifiers.justify = false;
    modifiers.left = false;
    modifiers.right = false;
    modifiers.center = false;
    mLayoutState->SetModifiers(modifiers);

    // Parse argument and set appropriate flag
    if (upperArg == "ON")
    {
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.justify = true;
        mLayoutState->SetModifiers(modifiers);
    }
    else if (upperArg == "OFF" || upperArg.empty())
    {
        // Empty or "off" = left align (default)
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.left = true;
        mLayoutState->SetModifiers(modifiers);
    }
    else if (upperArg == "C")
    {
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.center = true;
        mLayoutState->SetModifiers(modifiers);
    }
    else if (upperArg == "R")
    {
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.right = true;
        mLayoutState->SetModifiers(modifiers);
    }
    else
    {
        // Invalid argument - default to left align
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.left = true;
        mLayoutState->SetModifiers(modifiers);
        return DOT_ERROR;
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return bool - true if parsed successfully
///
/// @brief
/// Parses .OC (centering on/off) command. Enables or disables text centering.
/// Empty or "ON" enables centering, "OFF" disables it.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseCenter(const std::string& command)
{
    if (command.length() < 3)
    {
        return DOT_ERROR;
    }

    // Extract argument after ".oc"
    std::string arg = command.substr(3);

    // Trim leading/trailing whitespace (including \r and \n)
    size_t start = arg.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        arg = "";
    }
    else
    {
        size_t end = arg.find_last_not_of(" \t\r\n");
        arg = arg.substr(start, end - start + 1);
    }

    // Convert to uppercase for case-insensitive comparison
    std::string upperArg = arg;
    for (char& c : upperArg)
    {
        c = std::toupper(c);
    }

    // Reset all alignment flags (mutual exclusion)
    sModifiers modifiers = mLayoutState->GetModifiers();
    modifiers.justify = false;
    modifiers.left = false;
    modifiers.right = false;
    modifiers.center = false;
    mLayoutState->SetModifiers(modifiers);

    // Parse argument
    if (upperArg.empty() || upperArg == "ON")
    {
        // Empty or "ON" = enable centering
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.center = true;
        mLayoutState->SetModifiers(modifiers);
    }
    else if (upperArg == "OFF")
    {
        // "OFF" = disable centering, default to left align
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.left = true;
        mLayoutState->SetModifiers(modifiers);
    }
    else
    {
        // Invalid argument - default to left align
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.left = true;
        mLayoutState->SetModifiers(modifiers);
        return DOT_ERROR;
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return bool - true if parsed successfully
///
/// @brief
/// Parses .LS (line spacing) command. Sets line spacing multiplier.
/// WordStar spec: values 1-9 (integer)
/// WordTsar extension: any value > 0.25 (allows fractional like 1.5)
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseLineSpacing(const std::string& command)
{
    if (command.length() < 4)
    {
        return DOT_ERROR;
    }

    bool incdec;
    double value = mDocument->GetValue(command.substr(3), incdec);

    // WordStar spec: 1-9
    // WordTsar extension: any value > 0.25 for modern use (allows 1.5 spacing, etc.)
    if (value > 0.25)
    {
        sModifiers modifiers = mLayoutState->GetModifiers();
        modifiers.linespace = value;
        mLayoutState->SetModifiers(modifiers);
        return DOT_GOOD;
    }

    return DOT_ERROR;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return bool - true if parsed successfully
///
/// @brief
/// Parses .PS (paragraph spacing) command. WordTsar extension.
/// .psa sets space after paragraph, .psb sets space before paragraph.
/// Values in any unit (i, c, m, p, "), converted to twips.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseParagraphSpacing(const std::string& command)
{
    if (command.length() < 5)
    {
        return DOT_ERROR;
    }

    // Extract the type ('a' or 'b') and value string
    std::string str = command.substr(3);

    // Trim leading whitespace
    size_t pos = str.find_first_not_of(" \t");
    if (pos == std::string::npos)
    {
        return DOT_ERROR;
    }
    str = str.substr(pos);

    if (str.length() < 2)
    {
        return DOT_ERROR;
    }

    char type = std::tolower(str[0]);
    std::string valueStr = str.substr(1);

    // Parse value and unit via math expression evaluator
    bool incdec;
    bool hasUnits;
    double value = mDocument->EvaluateExpression(valueStr, incdec, hasUnits);

    // Negative absolute values are invalid
    if (value < 0.0 && !incdec)
    {
        return DOT_ERROR;
    }

    // Convert to twips
    COORD_T spacing;

    if (hasUnits)
    {
        // value is already in twips from EvaluateExpression
        spacing = static_cast<COORD_T>(value);
    }
    else
    {
        // bare number -- treat as inches (WS7 default for .PS)
        spacing = mDocument->ConvertToTwips(value, 'I', 0);
    }

    // Apply to appropriate variable
    if (type == 'a')
    {
        if (incdec)
        {
            mLayoutState->SetParagraphSpacingAfter(mLayoutState->GetParagraphSpacingAfter() + spacing);
            if (mLayoutState->GetParagraphSpacingAfter() < 0)
            {
                mLayoutState->SetParagraphSpacingAfter(0);
            }
        }
        else
        {
            mLayoutState->SetParagraphSpacingAfter((spacing < 0) ? 0 : spacing);
        }
        return DOT_GOOD;
    }
    else if (type == 'b')
    {
        if (incdec)
        {
            mLayoutState->SetParagraphSpacingBefore(mLayoutState->GetParagraphSpacingBefore() + spacing);
            if (mLayoutState->GetParagraphSpacingBefore() < 0)
            {
                mLayoutState->SetParagraphSpacingBefore(0);
            }
        }
        else
        {
            mLayoutState->SetParagraphSpacingBefore((spacing < 0) ? 0 : spacing);
        }
        return DOT_GOOD;
    }

    return DOT_ERROR;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (e.g., ".rr L.....!.....R")
///
/// @return bool - true if parsed successfully, false otherwise
///
/// @brief
/// Parses .RR (ruler) command to extract margins and tab stops from ruler text.
/// The ruler string represents visual character positions where:
///   L or l = left margin position
///   R or r = right margin position
///   P or p = paragraph margin position
///   ! = normal tab stop (or center/right if followed by special chars)
///   # = decimal tab stop
///
/// Position is calculated as: character_index * RULER_CHAR_WIDTH
///
/// @note
/// Ruler character width uses Courier New 12pt monospace width (144 twips
/// = 1/10 inch = 10 pitch). Each character position in the ruler maps to
/// one column at this width.
///
/// @see
/// Old system: layoutbase.cpp:2113-2182 ParseRuler()
/// Uses SegmentParagraphText() to get actual character widths
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseRuler(const std::string& command)
{
    // Minimum valid ruler: ".rr X" (4 characters)
    if (command.length() < 4)
    {
        return DOT_ERROR;
    }

    // Extract ruler text after ".rr "
    std::string ruler = command.substr(3);

    // Trim leading whitespace
    size_t pos = ruler.find_first_not_of(" \t");
    if (pos != std::string::npos)
    {
        ruler = ruler.substr(pos);
    }
    else
    {
        // Entire string is whitespace
        ruler.clear();
    }

    // Strip trailing whitespace and control characters (\r, \n)
    // Paragraph text includes \r terminator which must not be parsed as ruler content
    while (!ruler.empty() && (ruler.back() == ' ' || ruler.back() == '\t' ||
           ruler.back() == '\r' || ruler.back() == '\n'))
    {
        ruler.pop_back();
    }

    // Empty ruler is invalid
    if (ruler.empty())
    {
        return DOT_ERROR;
    }

    // Fixed character width for ruler positioning (Courier New 12pt monospace)
    const COORD_T RULER_CHAR_WIDTH = 144;  // 144 twips = 1/10 inch

    // Minimum ruler width in twips (0.5 inch = 6 character positions)
    // A single character ruler always produces at least this width
    const COORD_T MIN_RULER_WIDTH = 720;

    // Save original state so we can restore on validation failure
    std::vector<sTabStop> savedTabs = mLayoutState->GetTabs();
    COORD_T savedLeft = mLayoutState->GetLeftMargin();
    COORD_T savedRight = mLayoutState->GetRightMargin();
    COORD_T savedPara = mLayoutState->GetParagraphMargin();
    bool savedValidPara = mLayoutState->IsValidParagraphMargin();

    // Build new tab stop vector from ruler characters
    std::vector<sTabStop> tabs;

    // Track whether explicit L and R markers were found
    bool foundLeft = false;
    bool foundRight = false;

    // Parse ruler character by character
    for (size_t i = 0; i < ruler.length(); ++i)
    {
        char c = ruler[i];

        // Calculate position based on character index
        COORD_T position = static_cast<COORD_T>(i) * RULER_CHAR_WIDTH;

        // Check for margin markers
        if (c == 'L' || c == 'l')
        {
            mLayoutState->SetLeftMargin(position);
            foundLeft = true;
        }
        else if (c == 'R' || c == 'r')
        {
            mLayoutState->SetRightMargin(position);
            foundRight = true;
        }
        else if (c == 'P' || c == 'p')
        {
            mLayoutState->SetParagraphMargin(position);
            mLayoutState->SetValidParagraphMargin(true);
        }
        // Check for tab stop markers
        else if (c == '!')
        {
            // ! = normal tab
            tabs.push_back(sTabStop(position, TAB_TAB));
        }
        else if (c == '#')
        {
            // # = decimal tab
            tabs.push_back(sTabStop(position, TAB_DECIMAL));
        }
        else if (c == '^')
        {
            // ^ = center tab
            tabs.push_back(sTabStop(position, TAB_CENTER));
        }
        else if (c == '>')
        {
            // > = right tab
            tabs.push_back(sTabStop(position, TAB_RIGHT));
        }
    }

    // Default missing margin markers
    // If no L found, left margin defaults to 0
    if (!foundLeft)
    {
        mLayoutState->SetLeftMargin(0);
    }

    // If no R found, set right margin to end of ruler or left + MIN_RULER_WIDTH,
    // whichever is larger. This guarantees a minimum usable ruler width.
    if (!foundRight)
    {
        COORD_T rulerEnd = static_cast<COORD_T>(ruler.length()) * RULER_CHAR_WIDTH;
        COORD_T minRight = mLayoutState->GetLeftMargin() + MIN_RULER_WIDTH;
        if (rulerEnd > minRight)
        {
            mLayoutState->SetRightMargin(rulerEnd);
        }
        else
        {
            mLayoutState->SetRightMargin(minRight);
        }
    }

    // Enforce minimum ruler width of 720 twips (0.5 inch)
    // Even with explicit L and R, the ruler must be at least this wide
    if (mLayoutState->GetRightMargin() - mLayoutState->GetLeftMargin() < MIN_RULER_WIDTH)
    {
        mLayoutState->SetRightMargin(mLayoutState->GetLeftMargin() + MIN_RULER_WIDTH);
    }

    // Right margin is always the last tab stop in the vector.
    // This guarantees the tab vector is never empty (the ruler widget
    // calls pop_back to remove it, so it must always be present).
    tabs.push_back(sTabStop(mLayoutState->GetRightMargin(), TAB_TAB));
    mLayoutState->SetTabs(tabs);

    // Safety net: validate margins (should always pass after minimum enforcement)
    if (mLayoutState->GetRightMargin() <= mLayoutState->GetLeftMargin())
    {
        // Restore original state on failure
        mLayoutState->SetTabs(savedTabs);
        mLayoutState->SetLeftMargin(savedLeft);
        mLayoutState->SetRightMargin(savedRight);
        mLayoutState->SetParagraphMargin(savedPara);
        mLayoutState->SetValidParagraphMargin(savedValidPara);
        return DOT_ERROR;
    }

    return DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return bool - true if parsed successfully
///
/// @brief
/// Parses .TB (tab stops) command. Sets multiple tab stops for editing/printing.
/// Format: .tb 1i 2i 3i 4.5i (space-separated absolute positions with units).
/// Supports type prefixes: ^ = center tab, > = right tab, # = decimal tab.
/// No prefix = normal left tab. Example: .tb 1i ^3i >5i #6i
/// Clears existing tabs and replaces with new list.
/// Always adds left margin as first tab and right margin as last tab.
///
/// @note
/// Tab stops do not support incremental values (no +/-).
/// All values must be absolute positions.
///
/// @see
/// Old system: layoutbase.cpp:2034-2087 ParseTabs()
/// Shares mLayoutState->GetTabs() vector with ParseRuler() - last command wins
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cDotCommandParser::ParseTabs(const std::string& command)
{
    // Minimum valid command: ".tb " (4 characters)
    if (command.length() < 4)
    {
        return DOT_ERROR;
    }

    // Extract text after ".tb "
    std::string str = command.substr(3);

    // Trim leading whitespace
    size_t start = str.find_first_not_of(" \t");
    if (start != std::string::npos)
    {
        str = str.substr(start);
    }
    else
    {
        str.clear();
    }

    // Trim trailing whitespace
    if (!str.empty())
    {
        size_t end = str.find_last_not_of(" \t");
        if (end != std::string::npos)
        {
            str = str.substr(0, end + 1);
        }
    }

    // Clear existing tab stops
    std::vector<sTabStop> tabs;
    mLayoutState->SetTabs(tabs);

    // Empty command is valid - creates just left/right margin tabs
    if (str.empty())
    {
        // Add left and right margins as tabs
        std::vector<sTabStop> tabs;
        tabs.push_back(sTabStop(mLayoutState->GetLeftMargin(), TAB_TAB));
        tabs.push_back(sTabStop(mLayoutState->GetRightMargin(), TAB_TAB));
        mLayoutState->SetTabs(tabs);

        return DOT_GOOD;
    }

    // Parse space/comma-separated values
    // Format: .tb 1i 2.5i #3i
    // # prefix = decimal tab, no prefix = regular left tab
    std::string token;
    size_t pos = 0;

    while (pos < str.length())
    {
        // Skip whitespace and commas (WordStar accepts both as separators)
        while (pos < str.length() && (str[pos] == ' ' || str[pos] == '\t' || str[pos] == ','))
        {
            pos++;
        }

        // Collect token characters (not whitespace, not comma)
        token.clear();
        while (pos < str.length() && str[pos] != ' ' && str[pos] != '\t' && str[pos] != ',')
        {
            token.push_back(str[pos]);
            pos++;
        }

        // Process token if not empty
        if (!token.empty())
        {
            // Check for type prefix: ^ = center, > = right, # = decimal
            eTabTypes tabType = TAB_TAB;

            if (token[0] == '^')
            {
                tabType = TAB_CENTER;
                token = token.substr(1);
            }
            else if (token[0] == '>')
            {
                tabType = TAB_RIGHT;
                token = token.substr(1);
            }
            else if (token[0] == '#')
            {
                tabType = TAB_DECIMAL;
                token = token.substr(1);
            }

            // Skip if prefix consumed entire token
            if (token.empty())
            {
                continue;
            }

            bool incdec;
            bool hasUnits;
            double value = mDocument->EvaluateExpression(token, incdec, hasUnits);

            // Skip incremental values (tabs don't support +/-)
            if (incdec)
            {
                continue;
            }

            // Skip negative absolute values
            if (value < 0.0)
            {
                continue;
            }

            // Convert to twips position
            COORD_T position;

            if (hasUnits)
            {
                // value is already in twips from EvaluateExpression
                position = static_cast<COORD_T>(value);
            }
            else
            {
                // bare number -- treat as columns (Courier New 12pt = 144 twips per column)
                const COORD_T RULER_CHAR_WIDTH = 144;
                position = mDocument->ConvertToTwips(value, ' ', RULER_CHAR_WIDTH);
            }

            // Add tab stop with detected type
            std::vector<sTabStop> currentTabs = mLayoutState->GetTabs();
            currentTabs.push_back(sTabStop(position, tabType));
            mLayoutState->SetTabs(currentTabs);
        }
    }

    // Always add left margin as first tab and right margin as last tab
    std::vector<sTabStop> finalTabs = mLayoutState->GetTabs();
    finalTabs.insert(finalTabs.begin(), sTabStop(mLayoutState->GetLeftMargin(), TAB_TAB));
    finalTabs.push_back(sTabStop(mLayoutState->GetRightMargin(), TAB_TAB));
    mLayoutState->SetTabs(finalTabs);

    return DOT_GOOD;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @return current line height in twips
///
/// @brief
/// Get the current line height, with fallback to auto-calculated value
///
/// @note
/// This is a temporary implementation. GetLineHeight() should ideally be
/// a callback to layoutbase's CalculateFontBasedLineHeight() method when
/// auto-leading is enabled. For now, this just returns the state value.
///
/// TODO: Add callback to layoutbase::CalculateFontBasedLineHeight() for
///       auto-leading mode, or pass pre-calculated line height to parser.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cDotCommandParser::GetLineHeight(void) const
{
    COORD_T height = mLayoutState->GetLineHeight();

    if (height == NOT_SET)
    {
        // Call back to layout for font-based calculation
        if (mLayout != nullptr)
        {
            return mLayout->GetLineHeight();
        }
        // Fallback (should not happen)
        return 240;
    }

    return height;
}


