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

/**
 * @file abouttext.cpp
 *
 * @brief Text of the startup screen and the About box.
 */

#include "abouttext.h"
#include "src/core/include/version.h"

#include <cstdio>
#include <cstdlib>

#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

namespace abouttext
{

namespace
{

// Leading and trailing spaces, tabs and line ends removed.
std::string Trim(const std::string& text)
{
    const char* blanks = " \t\r\n";
    size_t first = text.find_first_not_of(blanks);
    if (first == std::string::npos)
    {
        return std::string();
    }
    size_t last = text.find_last_not_of(blanks);
    return text.substr(first, last - first + 1);
}

#ifdef __APPLE__
// String value of a sysctl, empty when it is not available.
std::string SysctlString(const char* name)
{
    size_t size = 0;
    if ((sysctlbyname(name, nullptr, &size, nullptr, 0) != 0) || (size == 0))
    {
        return std::string();
    }
    std::string value(size, '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0)
    {
        return std::string();
    }
    return Trim(std::string(value.c_str()));
}
#endif

}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the one-line description of WordTsar
///
/////////////////////////////////////////////////////////////////////////////
std::string Tagline(void)
{
    return "A WordStar 7 clone for the 21st century.";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return product, credit and copyright lines for this build's version
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> ProductLines(void)
{
    return ProductLines(FULLVERSION_STRING);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  version [in] version text, e.g. "0.14.3"
///
/// @return product, credit and copyright lines
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> ProductLines(const std::string& version)
{
    return {
        "WordTsar for macOS Version " + version,
        "macOS edition by Egbert H. Schroeer, based on WordTsar by Gerald Brandt",
        "Copyright (C) 2018 Gerald Brandt",
        "Portions Copyright (C) 2026 Egbert H. Schroeer",
    };
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  info [in] machine and terminal values
/// @param  cols [in] terminal columns
/// @param  rows [in] terminal rows
///
/// @return "macOS <version> on <chip>, <terminal> <cols>x<rows>" and the
///         printer line. Unknown values are left out; without TERM_PROGRAM
///         the terminal is called "terminal".
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> SystemLines(const sSystemInfo& info, int cols, int rows)
{
    std::string machine = "macOS";
    if (info.macosVersion.empty() == false)
    {
        machine += " " + info.macosVersion;
    }
    if (info.chip.empty() == false)
    {
        machine += " on " + info.chip;
    }

    std::string terminal = info.terminal.empty() ? std::string("terminal") : info.terminal;
    machine += ", " + terminal + " " + std::to_string(cols) + "x" + std::to_string(rows);

    return { machine, PrinterLine(info.printer) };
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  printer [in] default printer name, empty when none is set
///
/// @return "Default printer: <name>" or "Printing to PDF"
///
/////////////////////////////////////////////////////////////////////////////
std::string PrinterLine(const std::string& printer)
{
    if (printer.empty() == true)
    {
        return "Printing to PDF";
    }
    return "Default printer: " + printer;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the macOS services WordTsar uses: NSSpellChecker for spelling,
///         Core Foundation hyphenation, the Dictionary app for the
///         thesaurus, Core Text and Quartz for PDF output.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> TechnologyLines(void)
{
    return {
        "Spelling and hyphenation by macOS, thesaurus by the macOS Dictionary",
        "Printing to PDF by Core Text and Quartz",
        "Open-source components: see Help > About WordTsar",
    };
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return no-warranty, licence, source and trademark lines
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> LegalLines(void)
{
    return {
        "This program comes with ABSOLUTELY NO WARRANTY.",
        "This is free software under the GNU AGPL 3.0.",
        "Source: github.com/Egbert-Azure/WordTsar",
        "WordStar is a trademark of its owner.",
        "WordTsar is not affiliated with its owner.",
    };
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the trademark notice as one sentence pair
///
/////////////////////////////////////////////////////////////////////////////
std::string TrademarkText(void)
{
    return "WordStar is a trademark of its owner. WordTsar is not affiliated with its owner.";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the third-party components built into WordTsar with the
///         licence their bundled licence files name
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sComponent> Components(void)
{
    return {
        { "pugixml", "MIT" },
        { "SimpleIni", "MIT" },
        { "cpp-unicodelib", "MIT" },
        { "PicoMath", "BSD 3-Clause" },
        { "zip", "MIT" },
        { "miniz", "Unlicense" },
        { "chillout", "MIT" },
    };
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return "Open-source components: name (licence), ..."
///
/////////////////////////////////////////////////////////////////////////////
std::string ComponentsText(void)
{
    std::string text = "Open-source components: ";
    std::vector<sComponent> components = Components();
    for (size_t index = 0; index < components.size(); ++index)
    {
        if (index > 0)
        {
            text += ", ";
        }
        text += components[index].name + " (" + components[index].licence + ")";
    }
    return text + ".";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the About box message for this build's version
///
/////////////////////////////////////////////////////////////////////////////
std::string AboutText(void)
{
    return AboutText(FULLVERSION_STRING);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  version [in] version text
///
/// @return the About box message: tagline, product block, fork note,
///         licence notice, source, trademark and the component list
///
/////////////////////////////////////////////////////////////////////////////
std::string AboutText(const std::string& version)
{
    std::string text = Tagline() + "\n\n";

    for (const std::string& line : ProductLines(version))
    {
        text += line + "\n";
    }
    text += "\n";

    text += "This is a macOS-focused fork of Gerald Brandt's WordTsar -- the "
            "WordStar-clone editing engine, document formats, and original "
            "design are his work; this fork adds macOS-specific packaging and "
            "features on top. Not endorsed by or affiliated with the upstream "
            "WordTsar project.\n\n";

    text += "This program comes with ABSOLUTELY NO WARRANTY.\n";
    text += "This is free software, and you are welcome to redistribute it "
            "under the terms of the GNU Affero General Public License v3.0. "
            "See LICENSE.md in the source distribution, or "
            "https://www.gnu.org/licenses/agpl-3.0.html for the full licence "
            "text.\n\n";

    text += "https://github.com/Egbert-Azure/WordTsar\n";
    text += TrademarkText() + "\n\n";

    text += ComponentsText();

    return text;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  output [in] text printed by `LC_ALL=C lpstat -d`
///
/// @return the destination named after "system default destination:",
///         empty when the output names none
///
/////////////////////////////////////////////////////////////////////////////
std::string ParseLpstatDefault(const std::string& output)
{
    const std::string prefix = "system default destination:";
    size_t at = output.find(prefix);
    if (at == std::string::npos)
    {
        return std::string();
    }
    size_t start = at + prefix.size();
    size_t end = output.find('\n', start);
    return Trim(output.substr(start, (end == std::string::npos) ? std::string::npos : end - start));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  termProgram [in] TERM_PROGRAM value, or nullptr when unset
///
/// @return the terminal name, empty when unset
///
/////////////////////////////////////////////////////////////////////////////
std::string TerminalName(const char* termProgram)
{
    if (termProgram == nullptr)
    {
        return std::string();
    }
    return Trim(termProgram);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return macOS product version (sysctl kern.osproductversion)
///
/////////////////////////////////////////////////////////////////////////////
std::string QueryMacOSVersion(void)
{
#ifdef __APPLE__
    return SysctlString("kern.osproductversion");
#else
    return std::string();
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return processor name (sysctl machdep.cpu.brand_string)
///
/////////////////////////////////////////////////////////////////////////////
std::string QueryChip(void)
{
#ifdef __APPLE__
    return SysctlString("machdep.cpu.brand_string");
#else
    return std::string();
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return name of the default printer from `lpstat -d`, empty when none
///         is set or lpstat cannot run
///
/////////////////////////////////////////////////////////////////////////////
std::string QueryDefaultPrinter(void)
{
    FILE* pipe = popen("LC_ALL=C lpstat -d 2>/dev/null", "r");
    if (pipe == nullptr)
    {
        return std::string();
    }
    std::string output;
    char buffer[256];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        output += buffer;
    }
    pclose(pipe);
    return ParseLpstatDefault(output);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return macOS version, chip, TERM_PROGRAM and default printer of the
///         running system
///
/////////////////////////////////////////////////////////////////////////////
sSystemInfo QuerySystemInfo(void)
{
    sSystemInfo info;
    info.macosVersion = QueryMacOSVersion();
    info.chip = QueryChip();
    info.terminal = TerminalName(std::getenv("TERM_PROGRAM"));
    info.printer = QueryDefaultPrinter();
    return info;
}

}
