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

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

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
        "Spelling, hyphenation and thesaurus by macOS",
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
///         licences their bundled licence files and headers name
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
        { "miniz", "Unlicense and MIT" },
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
/// @return absolute path of lpstat
///
/////////////////////////////////////////////////////////////////////////////
std::string LpstatPath(void)
{
    return "/usr/bin/lpstat";
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  path [in] absolute path of the program
/// @param  args [in] arguments after the program name
/// @param  timeoutMs [in] how long the program may run
///
/// @return standard output of the program, empty when it cannot start or
///         has not finished after timeoutMs
///
/// @brief
/// The program runs in its own process group with LC_ALL=C and standard
/// input and error on /dev/null. Where POSIX_SPAWN_CLOEXEC_DEFAULT exists
/// (macOS) it has no other open descriptors. Elsewhere the read end of the
/// pipe is closed in the program, and it inherits every other descriptor
/// of the caller that is not close-on-exec, the pipe's write end among
/// them. Output is read through a pipe with poll() until end of file and
/// the program is reaped with waitpid(); past the deadline the process
/// group is killed with SIGKILL and the program reaped.
///
/////////////////////////////////////////////////////////////////////////////
std::string RunCommand(const std::string& path, const std::vector<std::string>& args, int timeoutMs)
{
#ifdef _WIN32
    (void)path;
    (void)args;
    (void)timeoutMs;
    return std::string();
#else
    using tClock = std::chrono::steady_clock;
    const tClock::time_point deadline = tClock::now() + std::chrono::milliseconds(timeoutMs);

    auto remainingMs = [&deadline](void) -> int
    {
        long long left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - tClock::now()).count();
        return (left > 0) ? static_cast<int>(left) : 0;
    };

    int fds[2];
    if (pipe(fds) != 0)
    {
        return std::string();
    }

    std::vector<std::string> argStore;
    argStore.push_back(path);
    argStore.insert(argStore.end(), args.begin(), args.end());
    std::vector<char*> argv;
    for (std::string& arg : argStore)
    {
        argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    std::vector<std::string> envStore;
    for (char** entry = environ; (entry != nullptr) && (*entry != nullptr); ++entry)
    {
        if (std::strncmp(*entry, "LC_ALL=", 7) != 0)
        {
            envStore.push_back(*entry);
        }
    }
    envStore.push_back("LC_ALL=C");
    std::vector<char*> envp;
    for (std::string& entry : envStore)
    {
        envp.push_back(entry.data());
    }
    envp.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addopen(&actions, STDERR_FILENO, "/dev/null", O_WRONLY, 0);

    posix_spawnattr_t attr;
    posix_spawnattr_init(&attr);
    short flags = POSIX_SPAWN_SETPGROUP;
#ifdef POSIX_SPAWN_CLOEXEC_DEFAULT
    flags = static_cast<short>(flags | POSIX_SPAWN_CLOEXEC_DEFAULT);
#else
    posix_spawn_file_actions_addclose(&actions, fds[0]);
#endif
    posix_spawnattr_setflags(&attr, flags);
    posix_spawnattr_setpgroup(&attr, 0);

    pid_t pid = 0;
    int spawned = posix_spawn(&pid, path.c_str(), &actions, &attr, argv.data(), envp.data());
    posix_spawn_file_actions_destroy(&actions);
    posix_spawnattr_destroy(&attr);
    close(fds[1]);

    if (spawned != 0)
    {
        close(fds[0]);
        return std::string();
    }

    std::string output;
    bool finished = false;
    bool atEnd = false;

    while ((atEnd == false) && (remainingMs() > 0))
    {
        pollfd descriptor;
        descriptor.fd = fds[0];
        descriptor.events = POLLIN;
        descriptor.revents = 0;
        int ready = poll(&descriptor, 1, remainingMs());
        if (ready < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            break;
        }
        if (ready == 0)
        {
            break;
        }
        char buffer[256];
        ssize_t count = read(fds[0], buffer, sizeof(buffer));
        if (count > 0)
        {
            output.append(buffer, static_cast<size_t>(count));
        }
        else if ((count == 0) || (errno != EINTR))
        {
            atEnd = true;
        }
    }
    close(fds[0]);

    int status = 0;
    while (atEnd == true)
    {
        pid_t reaped = waitpid(pid, &status, WNOHANG);
        if (reaped == pid)
        {
            finished = true;
            break;
        }
        if ((reaped < 0) && (errno != EINTR))
        {
            break;
        }
        if (remainingMs() == 0)
        {
            break;
        }
        poll(nullptr, 0, 10);
    }

    if (finished == false)
    {
        kill(-pid, SIGKILL);
        kill(pid, SIGKILL);
        while ((waitpid(pid, &status, 0) < 0) && (errno == EINTR))
        {
        }
        return std::string();
    }

    return output;
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return name of the default printer from /usr/bin/lpstat -d, empty
///         when none is set, lpstat fails or it runs longer than
///         kLpstatTimeoutMs
///
/////////////////////////////////////////////////////////////////////////////
std::string QueryDefaultPrinter(void)
{
    return QueryDefaultPrinter(LpstatPath(), kLpstatTimeoutMs);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  lpstatPath [in] path of the lpstat program
/// @param  timeoutMs [in] how long lpstat may run
///
/// @return name of the default printer from `lpstatPath -d`, empty when
///         none is set, lpstat fails or it runs longer than timeoutMs
///
/////////////////////////////////////////////////////////////////////////////
std::string QueryDefaultPrinter(const std::string& lpstatPath, int timeoutMs)
{
    return ParseLpstatDefault(RunCommand(lpstatPath, {"-d"}, timeoutMs));
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
