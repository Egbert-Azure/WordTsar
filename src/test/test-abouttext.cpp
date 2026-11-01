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

#include "doctest.h"
#include "src/core/utils/abouttext.h"
#include "src/core/include/version.h"

#include <string>
#include <vector>


namespace
{

bool HasLine(const std::vector<std::string>& lines, const std::string& text)
{
    for (const std::string& line : lines)
    {
        if (line == text)
        {
            return true;
        }
    }
    return false;
}

std::string Joined(const std::vector<std::string>& lines)
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += line + "\n";
    }
    return text;
}

// Every text the startup screen and the About box show.
std::string AllText(void)
{
    abouttext::sSystemInfo info;
    info.macosVersion = "15.5";
    info.chip = "Apple M2";
    info.terminal = "Apple_Terminal";
    std::string text = abouttext::Tagline() + "\n";
    text += Joined(abouttext::ProductLines());
    text += Joined(abouttext::SystemLines(info, 80, 25));
    text += Joined(abouttext::TechnologyLines());
    text += Joined(abouttext::LegalLines());
    text += abouttext::ComponentsText() + "\n";
    text += abouttext::AboutText();
    return text;
}

}


TEST_CASE("About text - product block carries the version, credit and copyright lines")
{
    std::vector<std::string> lines = abouttext::ProductLines("9.8.7");
    REQUIRE(lines.size() == 4);
    CHECK(lines[0] == "WordTsar for macOS Version 9.8.7");
    CHECK(lines[1] == "macOS edition by Egbert H. Schroeer, based on WordTsar by Gerald Brandt");
    CHECK(lines[2] == "Copyright (C) 2018 Gerald Brandt");
    CHECK(lines[3] == "Portions Copyright (C) 2026 Egbert H. Schroeer");

    std::vector<std::string> current = abouttext::ProductLines();
    REQUIRE(current.empty() == false);
    CHECK(current[0] == std::string("WordTsar for macOS Version ") + FULLVERSION_STRING);
}


TEST_CASE("About text - tagline and legal lines")
{
    CHECK(abouttext::Tagline() == "A WordStar 7 clone for the 21st century.");

    std::string legal = Joined(abouttext::LegalLines());
    CHECK(legal.find("NO WARRANTY") != std::string::npos);
    CHECK(legal.find("AGPL 3.0") != std::string::npos);
    CHECK(legal.find("github.com/Egbert-Azure/WordTsar") != std::string::npos);
    CHECK(legal.find("WordStar is a trademark of its owner.") != std::string::npos);
    CHECK(legal.find("WordTsar is not affiliated with its owner.") != std::string::npos);
}


TEST_CASE("About text - technology and trademark wording")
{
    std::vector<std::string> technology = abouttext::TechnologyLines();
    REQUIRE(technology.size() == 3);
    CHECK(technology[0] == "Spelling, hyphenation and thesaurus by macOS");
    CHECK(technology[1] == "Printing to PDF by Core Text and Quartz");
    CHECK(technology[2] == "Open-source components: see Help > About WordTsar");

    CHECK(abouttext::TrademarkText() ==
          "WordStar is a trademark of its owner. WordTsar is not affiliated with its owner.");
    std::vector<std::string> legal = abouttext::LegalLines();
    REQUIRE(legal.size() == 5);
    CHECK(legal[3] + " " + legal[4] == abouttext::TrademarkText());
}


TEST_CASE("About text - no serial number, no WordStar copyright wording, WordStar spelled correctly")
{
    std::string text = AllText();
    CHECK(text.find("WordStar") != std::string::npos);
    CHECK(text.find("Wordstar") == std::string::npos);
    CHECK(text.find("WORDSTAR") == std::string::npos);
    CHECK(text.find("Serial") == std::string::npos);
    CHECK(text.find("(C) 1992") == std::string::npos);
    CHECK(text.find("International Incorporated") == std::string::npos);
    CHECK(text.find("All rights reserved") == std::string::npos);
}


TEST_CASE("About text - the About message starts with the tagline and lists the components")
{
    std::string about = abouttext::AboutText("9.8.7");
    CHECK(about.rfind(abouttext::Tagline() + "\n", 0) == 0);
    CHECK(about.find("WordTsar for macOS Version 9.8.7") != std::string::npos);
    CHECK(about.find("Copyright (C) 2018 Gerald Brandt") != std::string::npos);
    CHECK(about.find("Portions Copyright (C) 2026 Egbert H. Schroeer") != std::string::npos);
    CHECK(about.find("ABSOLUTELY NO WARRANTY") != std::string::npos);
    CHECK(about.find("GNU Affero General Public License v3.0") != std::string::npos);
    CHECK(about.find("https://github.com/Egbert-Azure/WordTsar") != std::string::npos);
    CHECK(about.find(abouttext::TrademarkText()) != std::string::npos);

    std::vector<abouttext::sComponent> components = abouttext::Components();
    REQUIRE(components.size() == 7);
    for (const abouttext::sComponent& component : components)
    {
        INFO("component: ", component.name);
        CHECK(component.licence.empty() == false);
        CHECK(about.find(component.name + " (" + component.licence + ")") != std::string::npos);
    }
}


TEST_CASE("About text - components and licences as the bundled licence files state them")
{
    std::vector<abouttext::sComponent> components = abouttext::Components();
    REQUIRE(components.size() == 7);
    const char* names[] = {"pugixml", "SimpleIni", "cpp-unicodelib", "PicoMath", "zip", "miniz", "chillout"};
    const char* licences[] = {"MIT", "MIT", "MIT", "BSD 3-Clause", "MIT", "Unlicense", "MIT"};
    for (size_t index = 0; index < components.size(); ++index)
    {
        CHECK(components[index].name == names[index]);
        CHECK(components[index].licence == licences[index]);
    }
}


TEST_CASE("About text - system lines from injected values")
{
    abouttext::sSystemInfo info;
    info.macosVersion = "15.5";
    info.chip = "Apple M2 Pro";
    info.terminal = "Apple_Terminal";
    info.printer = "HP_LaserJet_4";

    std::vector<std::string> lines = abouttext::SystemLines(info, 80, 25);
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "macOS 15.5 on Apple M2 Pro, Apple_Terminal 80x25");
    CHECK(lines[1] == "Default printer: HP_LaserJet_4");

    SUBCASE("TERM_PROGRAM unset names the terminal generically")
    {
        info.terminal.clear();
        CHECK(abouttext::SystemLines(info, 120, 40)[0] == "macOS 15.5 on Apple M2 Pro, terminal 120x40");
    }

    SUBCASE("no default printer prints to PDF")
    {
        info.printer.clear();
        CHECK(abouttext::SystemLines(info, 80, 25)[1] == "Printing to PDF");
        CHECK(abouttext::PrinterLine("") == "Printing to PDF");
    }

    SUBCASE("unknown version and chip are left out")
    {
        info.macosVersion.clear();
        info.chip.clear();
        CHECK(abouttext::SystemLines(info, 80, 25)[0] == "macOS, Apple_Terminal 80x25");
    }
}


TEST_CASE("About text - lpstat -d output parser")
{
    CHECK(abouttext::ParseLpstatDefault("system default destination: HP_LaserJet_4\n") == "HP_LaserJet_4");
    CHECK(abouttext::ParseLpstatDefault("system default destination: Brother  \n") == "Brother");
    CHECK(abouttext::ParseLpstatDefault("no system default destination\n") == "");
    CHECK(abouttext::ParseLpstatDefault("") == "");
    CHECK(abouttext::ParseLpstatDefault("lpstat: No destinations added.\n") == "");
    CHECK(abouttext::ParseLpstatDefault("system default destination: \n") == "");
}


TEST_CASE("About text - TERM_PROGRAM value")
{
    CHECK(abouttext::TerminalName(nullptr) == "");
    CHECK(abouttext::TerminalName("") == "");
    CHECK(abouttext::TerminalName("iTerm.app") == "iTerm.app");
}


TEST_CASE("About text - system queries return without crashing")
{
#ifdef __APPLE__
    CHECK(abouttext::QueryMacOSVersion().empty() == false);
    CHECK(abouttext::QueryChip().empty() == false);
#endif
    std::string printer = abouttext::QueryDefaultPrinter();
    CHECK(printer.find('\n') == std::string::npos);
    abouttext::sSystemInfo info = abouttext::QuerySystemInfo();
    CHECK(info.printer == printer);
}
