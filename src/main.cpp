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
 * 1. Creates cWordTsarApplication (a QApplication subclass that also catches
 *    QEvent::FileOpen -- how Finder/Launch Services-routed opens actually
 *    arrive, as opposed to a direct command-line invocation's argv)
 * 2. Installs crash handlers via the chillout library for crash reporting
 * 3. Displays a timed splash screen with the WordTsar logo and version info
 * 4. Creates the main cWordTsar window (QMainWindow) and registers it with
 *    the application object so a FileOpen event has somewhere to deliver to
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
#include <QFileOpenEvent>
#include <QStringList>

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
/// @class cWordTsarApplication
///
/// @brief
/// QApplication subclass that catches QEvent::FileOpen -- how macOS actually
/// delivers "open this file" requests routed through Launch Services (Finder
/// double-click, drag-onto-dock-icon, or "open -a WordTsar.app file.docx"
/// when the app isn't already running). Plain argv only covers direct
/// command-line invocation of the binary itself; Finder-style opens never
/// reach argc/argv at all. Qt queues a FileOpen event that arrives before
/// the event loop starts and delivers it once exec() runs, so setting
/// mMainWindow before exec() (done in main(), below) is sufficient even
/// though the event itself may arrive earlier.
///
/////////////////////////////////////////////////////////////////////////////
class cWordTsarApplication : public QApplication
{
public:
    cWordTsarApplication(int &argc, char **argv) : QApplication(argc, argv) {}

    // Called once the main window exists. Flushes any FileOpen event that
    // arrived (and was queued) before this point -- a cold `open -a
    // WordTsar.app file` launch can deliver the Apple Event early enough
    // that an explicit app.processEvents() call made before the window is
    // constructed (see main(), for the splash screen) processes and
    // discards it while mMainWindow is still null. Qt only delivers a given
    // FileOpen event once, so silently dropping it there would lose the
    // open request entirely.
    void SetMainWindow(cWordTsar *window)
    {
        mMainWindow = window;
        for (const QString &file : mPendingFiles)
        {
            mMainWindow->LoadFile(file);
        }
        mPendingFiles.clear();
    }

    bool event(QEvent *e) override
    {
        if (e->type() == QEvent::FileOpen)
        {
            QFileOpenEvent *openEvent = static_cast<QFileOpenEvent*>(e);
            if (mMainWindow != nullptr)
            {
                mMainWindow->LoadFile(openEvent->file());
            }
            else
            {
                mPendingFiles.append(openEvent->file());
            }
            return true;
        }
        return QApplication::event(e);
    }

private:
    cWordTsar *mMainWindow = nullptr;
    QStringList mPendingFiles;
};

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
    cWordTsarApplication app(argc, argv);

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
    app.SetMainWindow(&w);

    // Centering the window (on the same screen the splash is centered on)
    // happens inside cWordTsar::ReadConfig()'s own deferred resize callback
    // now, not here -- w.width()/w.height() at this point are still the
    // pre-resize construction-time size (that resize is itself deferred to
    // the same 0ms singleShot, for the same "Qt's first layout pass
    // overrides anything set before show()" reason), so centering against
    // them here used the wrong size and made the window visibly jump after
    // show(). Doing both together, once, after the real size is applied,
    // keeps them in sync.

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
