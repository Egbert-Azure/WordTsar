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
 * @file main.cpp
 * @brief Entry point for the WordTsar wordstartui terminal interface (wsw).
 *
 * Builds the terminal interface on the wordstartui
 * toolkit. Installs chillout crash handlers that restore the terminal and
 * package a crash report, then creates and runs cWSWordTsar.
 */

#include <iostream>
#include <cstdio>
#include <cstdlib>

#ifndef _WIN32
#include <termios.h>
#include <unistd.h>
#endif

#include "wordtsar.h"
#include "src/tui/debugreport/debugreport.h"
#include "chillout/chillout.h"


// Global pointer for crash callback access
static cTUIDebugReport *gReport = nullptr;


/////////////////////////////////////////////////////////////////////////////
///
/// @param  argc [in] number of command-line arguments
/// @param  argv [in] command-line argument strings
///
/// @return int [out] exit code (0 for success, 1 for error)
///
/// @brief
/// Main entry point. Installs crash handlers, then creates and runs the
/// wordstartui terminal user interface.
///
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    cTUIDebugReport report;
    gReport = &report;
    gReport->SetName(argv[0]);

#ifndef DEBUG
    auto &chillout = Debug::Chillout::getInstance();

#ifdef _WIN32
    wchar_t *buffer = _wgetcwd(NULL, 0);
    chillout.init(L"wsw", buffer);
#else
    chillout.init("wsw", "./");
#endif

    chillout.setBacktraceCallback([](const char * const stackEntry)
    {
        fprintf(stderr, "trace: %s", stackEntry);
    });

    chillout.setCrashCallback([&chillout]()
    {
#ifndef _WIN32
        // Restore the terminal to a sane state before printing.
        std::cerr << "\033[?1049l"
                  << "\033[?1004l"
                  << "\033[?25h"
                  << "\033[0m"
                  << std::flush;

        struct termios term;
        if (tcgetattr(STDIN_FILENO, &term) == 0)
        {
            term.c_lflag |= (ECHO | ICANON | ISIG);
            term.c_iflag |= ICRNL;
            tcsetattr(STDIN_FILENO, TCSANOW, &term);
        }
#endif

        std::cerr << "\n*** WordTsar (wsw) has crashed ***\n" << std::flush;

        chillout.backtrace();
        chillout.createCrashDump();

        if (gReport != nullptr)
        {
            gReport->Coalesce();
        }

        exit(1);
    });
#endif

    cWSWordTsar app;
    return app.Run(argc, argv);
}
