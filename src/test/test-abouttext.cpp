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

#include <cerrno>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>


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
    // miniz.h carries the Unlicense dedication and an MIT licence block.
    const char* licences[] = {"MIT", "MIT", "MIT", "BSD 3-Clause", "MIT", "Unlicense and MIT", "MIT"};
    for (size_t index = 0; index < components.size(); ++index)
    {
        CHECK(components[index].name == names[index]);
        CHECK(components[index].licence == licences[index]);
    }
    CHECK(abouttext::ComponentsText().find("miniz (Unlicense and MIT)") != std::string::npos);
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


namespace
{

// A unique temporary folder for scratch scripts, removed with its contents.
class cScriptDir
{
public:
    cScriptDir(void)
    {
        std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-lpstat-XXXXXX").string();
        REQUIRE(mkdtemp(pattern.data()) != nullptr);
        mPath = pattern;
    }

    ~cScriptDir(void)
    {
        std::error_code ec;
        std::filesystem::remove_all(mPath, ec);
    }

    // An executable shell script with the given body.
    std::string Script(const std::string& name, const std::string& body)
    {
        std::filesystem::path file = mPath / name;
        {
            std::ofstream out(file, std::ios::binary);
            out << "#!/bin/sh\n" << body;
        }
        REQUIRE(chmod(file.c_str(), 0700) == 0);
        return file.string();
    }

    std::string File(const std::string& name) const
    {
        return (mPath / name).string();
    }

private:
    std::filesystem::path mPath;
};

int ReadPid(const std::string& path)
{
    std::ifstream in(path);
    int pid = 0;
    in >> pid;
    return pid;
}

// True when no process with the pid exists, a zombie included; waits up
// to 1 s for a process that is being torn down.
bool ProcessGone(int pid)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        if ((kill(pid, 0) != 0) && (errno == ESRCH))
        {
            return true;
        }
        usleep(10 * 1000);
    }
    return false;
}

}


TEST_CASE("About text - lpstat runs from its absolute path with a 1-second timeout")
{
    CHECK(abouttext::LpstatPath() == "/usr/bin/lpstat");
    CHECK(abouttext::kLpstatTimeoutMs == 1000);
}


TEST_CASE("About text - default printer from an injected lpstat")
{
    cScriptDir dir;

    SUBCASE("lpstat -d is run with LC_ALL=C and its output parsed")
    {
        std::string lpstat = dir.Script("lpstat",
            "if [ \"$1\" = \"-d\" ] && [ \"$LC_ALL\" = \"C\" ]; then\n"
            "  echo \"system default destination: Fake_Printer\"\n"
            "fi\n");
        CHECK(abouttext::QueryDefaultPrinter(lpstat, 1000) == "Fake_Printer");
    }

    SUBCASE("no default destination")
    {
        std::string lpstat = dir.Script("lpstat", "echo \"no system default destination\"\n");
        CHECK(abouttext::QueryDefaultPrinter(lpstat, 1000) == "");
    }

    SUBCASE("a missing program")
    {
        CHECK(abouttext::QueryDefaultPrinter(dir.File("missing"), 1000) == "");
    }

    SUBCASE("output on standard error is not read")
    {
        std::string lpstat = dir.Script("lpstat", "echo \"system default destination: Stderr_Printer\" >&2\n");
        CHECK(abouttext::QueryDefaultPrinter(lpstat, 1000) == "");
    }
}


TEST_CASE("About text - an lpstat that hangs is killed after the timeout and means no printer")
{
    cScriptDir dir;
    std::string shellPid = dir.File("shell.pid");
    std::string sleepPid = dir.File("sleep.pid");
    std::string lpstat = dir.Script("lpstat",
        "echo $$ > '" + shellPid + "'\n"
        "echo \"system default destination: Too_Late\"\n"
        "sleep 5 &\n"
        "echo $! > '" + sleepPid + "'\n"
        "wait\n");

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    std::string printer = abouttext::QueryDefaultPrinter(lpstat, 1000);
    std::chrono::milliseconds elapsed =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);

    CHECK(printer == "");
    CHECK(elapsed.count() >= 900);
    CHECK(elapsed.count() < 1500);
    CHECK(abouttext::PrinterLine(printer) == "Printing to PDF");

    int shell = ReadPid(shellPid);
    int sleeper = ReadPid(sleepPid);
    REQUIRE(shell > 0);
    REQUIRE(sleeper > 0);
    // The shell was reaped, not left a zombie, and its child was killed.
    CHECK((kill(shell, 0) != 0) == true);
    CHECK(ProcessGone(sleeper) == true);
}
