//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2026 Egbert H. Schroeer
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

#ifndef WORDTSAR_ABOUTTEXT_H
#define WORDTSAR_ABOUTTEXT_H

#include <string>
#include <vector>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Text of the startup screen and the About box: product, credit and
/// copyright lines, the system description, the macOS technology in use,
/// legal notices and the open-source components. The text functions take
/// every environmental value as a parameter; the Query functions read the
/// running system.
///
/////////////////////////////////////////////////////////////////////////////
namespace abouttext
{

// Values describing the machine and terminal. An empty string is unknown.
struct sSystemInfo
{
    std::string macosVersion;   // kern.osproductversion, e.g. "15.5"
    std::string chip;           // machdep.cpu.brand_string, e.g. "Apple M2"
    std::string terminal;       // TERM_PROGRAM
    std::string printer;        // default printer name, empty when none is set
};

// An open-source component built into WordTsar and its licence.
struct sComponent
{
    std::string name;
    std::string licence;
};

std::string Tagline(void);

// "WordTsar for macOS Version <version>", the credit line and both
// copyright lines.
std::vector<std::string> ProductLines(void);
std::vector<std::string> ProductLines(const std::string& version);

// macOS version, chip, terminal name and size; the default printer or
// "Printing to PDF".
std::vector<std::string> SystemLines(const sSystemInfo& info, int cols, int rows);
std::string PrinterLine(const std::string& printer);

// What WordTsar uses from macOS, and where the component list is shown.
std::vector<std::string> TechnologyLines(void);

// No-warranty, licence, source and trademark lines.
std::vector<std::string> LegalLines(void);
std::string TrademarkText(void);

std::vector<sComponent> Components(void);
std::string ComponentsText(void);

// Message of the About WordTsar box; the tagline is its first line.
std::string AboutText(void);
std::string AboutText(const std::string& version);

// Name of the default printer in `lpstat -d` output; empty when none is set.
std::string ParseLpstatDefault(const std::string& output);

// TERM_PROGRAM value, empty when unset.
std::string TerminalName(const char* termProgram);

std::string QueryMacOSVersion(void);
std::string QueryChip(void);
std::string QueryDefaultPrinter(void);
sSystemInfo QuerySystemInfo(void);

}

#endif // WORDTSAR_ABOUTTEXT_H
