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
 *
 * @brief GUI application entry point for WordTsar.
 *
 * Initializes the Qt application, installs crash handlers, displays a splash
 * screen, and creates the main application window.
 *
 * @section main_startup Startup Sequence
 * 1. Creates QApplication with command-line arguments
 * 2. Installs crash handlers via the chillout library for crash reporting
 * 3. Displays a timed splash screen with the WordTsar logo and version info
 * 4. Creates the main cWordTsar window (QMainWindow)
 * 5. If a filename is passed on the command line, loads it after the window
 *    is shown
 * 6. Enters the Qt event loop (QApplication::exec())
 *
 * @section main_crash Crash Handling
 * The chillout crash callback is invoked on unhandled exceptions and signals.
 * It generates a cDebugReport (system info, backtrace, core dump) and writes
 * a dump file before exiting. The crash handler runs outside the normal event
 * loop to maximize the chance of successful report generation.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cWordTsar Main application window class
 * @see cDebugReport Crash report generator
 */

#include <QApplication>
#include <QSplashScreen>
#include <QPixmap>
#include <QTimer>
#include <QMessageBox>
#include <QDialog>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "src/gui/wordtsar.h"
#include "src/core/include/version.h"
#include "src/gui/debugreport/debugreport.h"


#include "chillout/chillout.h"

cDebugReport *gReport ;

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int argc [in] number of command-line arguments
/// @param  char *argv[] [in] command-line argument strings
///
/// @return application exit code
///
/// @brief
/// Application entry point. Initializes Qt, installs crash handlers,
/// shows splash screen, creates the main WordTsar window, and enters
/// the event loop. Loads a file from the command line if provided.
///
/////////////////////////////////////////////////////////////////////////////
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // install crash handler
    cDebugReport mReport ;
    gReport = &mReport ;
    gReport->SetName(argv[0]) ;

#ifndef DEBUG
    auto &chillout = Debug::Chillout::getInstance();
    // install various crash handlers
#ifdef _WIN32
    wchar_t *buffer = _wgetcwd(NULL, 0);
    chillout.init(L"WordTsar", buffer);
#else
    chillout.init("WordTsar", "./");
#endif
    chillout.setBacktraceCallback([](const char * const stackEntry) {
        fprintf(stderr, "my trace:  %s", stackEntry);
    });

    chillout.setCrashCallback([&chillout]()
    {
        // Generate backtrace and core dump first
        chillout.backtrace();
        chillout.createCrashDump();

        // Create the ZIP crash report before attempting Qt dialog.
        // Coalesce() must run first because Show() calls QDialog::exec()
        // which is unsafe in signal context and may crash or hang.
        gReport->Coalesce();

        // Best-effort: show dialog for user notes (may fail in signal context)
        gReport->Show();

        exit(1);
    });
#endif


    QScreen *screen = QApplication::primaryScreen() ;

    QPixmap pixmap(":/gui/images/splash.png") ;
    QSplashScreen splash(screen, pixmap) ;
    splash.setWindowFlag(Qt::WindowStaysOnTopHint, true) ;
    QString vers = FULLVERSION_STRING ;
    vers += " " ;
    vers += STATUS ;
    splash.showMessage(vers, Qt::AlignBottom | Qt::AlignCenter, QColor(152, 114, 14)) ;
    splash.show() ;
    app.processEvents();

    cWordTsar w(argc, argv);

    // Center the window on the same screen the splash is centered on --
    // otherwise the two are placed independently (the splash always at
    // screen-center, the window wherever the OS/window manager defaults to)
    // and can end up looking like they belong to two different launches.
    QRect avail = screen->availableGeometry() ;
    w.move(avail.x() + (avail.width() - w.width()) / 2,
           avail.y() + (avail.height() - w.height()) / 2) ;

    QTimer::singleShot(5000, &splash, SLOT(close())) ;

    w.show();
    app.processEvents();

    // filename as argument
    if(argc > 1)
    {
        QString arg(argv[1]) ;
        app.processEvents() ;

        w.LoadFile(arg) ;
    }

#ifdef DO_TEST
    cTest test ;
    test.StartTest() ;
#endif

    return app.exec();
}
