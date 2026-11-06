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
#include "src/gui/utils/terminatesignal.h"
#include "src/gui/editor/editorctrl.h"
#include "src/core/document/document.h"

#include <QApplication>
#include <QElapsedTimer>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <signal.h>


namespace
{

int gArgc = 0;
char* gArgv[] = {nullptr};

void EnsureApplication()
{
    if (!QApplication::instance())
    {
        new QApplication(gArgc, gArgv);
    }
}

volatile sig_atomic_t gSentinelHits = 0;

void SentinelHandler(int /*sig*/)
{
    gSentinelHits = gSentinelHits + 1;
}

// Replaces the SIGTERM action with SentinelHandler for the lifetime of the
// object, so a raise(SIGTERM) can never end the test process.
class cSigtermSentinel
{
public:
    cSigtermSentinel()
    {
        struct sigaction sa;
        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = SentinelHandler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGTERM, &sa, &mOriginal);
        gSentinelHits = 0;
    }

    ~cSigtermSentinel()
    {
        sigaction(SIGTERM, &mOriginal, nullptr);
    }

private:
    struct sigaction mOriginal;
};

void* CurrentHandler(int sig)
{
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sigaction(sig, nullptr, &sa);
    return reinterpret_cast<void*>(sa.sa_handler);
}

void ProcessEventsFor(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds)
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
}

bool HasEmergencyFile(const std::filesystem::path& dir)
{
    std::filesystem::directory_iterator entries(dir);
    return std::any_of(std::filesystem::begin(entries), std::filesystem::end(entries),
                       [](const std::filesystem::directory_entry& entry)
    {
        return entry.path().filename().string().rfind("emergency-", 0) == 0;
    });
}

} // namespace


TEST_CASE("SIGTERM runs the terminate callback once, from the event loop")
{
    EnsureApplication();
    cSigtermSentinel sentinel;

    int calls = 0;
    {
        cTerminateSignal terminate([&calls]() { calls++; });
        CHECK(terminate.IsInstalled());

        raise(SIGTERM);
        raise(SIGTERM);
        CHECK(calls == 0);      // the handler itself never runs the callback

        ProcessEventsFor(200);
        CHECK(calls == 1);

        raise(SIGTERM);
        ProcessEventsFor(100);
        CHECK(calls == 1);
    }
    CHECK(gSentinelHits == 0);
}


TEST_CASE("cTerminateSignal replaces only the SIGTERM action and restores it")
{
    EnsureApplication();
    cSigtermSentinel sentinel;

    const int others[] = { SIGABRT, SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGPIPE, SIGINT, SIGHUP };
    void* before[sizeof(others) / sizeof(others[0])];
    for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++)
    {
        before[i] = CurrentHandler(others[i]);
    }

    {
        cTerminateSignal terminate([]() {});
        CHECK(CurrentHandler(SIGTERM) == reinterpret_cast<void*>(&cTerminateSignal::Handler));
        for (size_t i = 0; i < sizeof(others) / sizeof(others[0]); i++)
        {
            CAPTURE(others[i]);
            CHECK(CurrentHandler(others[i]) == before[i]);
        }
    }

    CHECK(CurrentHandler(SIGTERM) == reinterpret_cast<void*>(&SentinelHandler));
}


TEST_CASE("EmergencySaveOnTerminate saves a modified document next to it")
{
    EnsureApplication();

    std::filesystem::path root = std::filesystem::canonical(std::filesystem::temp_directory_path()) / "wordtsar_terminate_save_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::path file = root / "doc.txt";
    {
        std::ofstream out(file, std::ios::binary);
        out << "Original text";
    }

    cEditorCtrl editor;
    REQUIRE(editor.LoadFile(file.string()));
    editor.GetDocument()->SetPosition(0);
    editor.GetDocument()->Insert("EDITED ");
    REQUIRE(editor.GetDocument()->IsModified());

    EmergencySaveOnTerminate(&editor);
    CHECK(HasEmergencyFile(root));

    std::filesystem::remove_all(root);
}


TEST_CASE("EmergencySaveOnTerminate writes nothing for an unmodified document")
{
    EnsureApplication();

    std::filesystem::path root = std::filesystem::canonical(std::filesystem::temp_directory_path()) / "wordtsar_terminate_nosave_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    std::filesystem::path file = root / "doc.txt";
    {
        std::ofstream out(file, std::ios::binary);
        out << "Original text";
    }

    cEditorCtrl editor;
    REQUIRE(editor.LoadFile(file.string()));
    REQUIRE_FALSE(editor.GetDocument()->IsModified());

    EmergencySaveOnTerminate(&editor);
    CHECK_FALSE(HasEmergencyFile(root));
    EmergencySaveOnTerminate(nullptr);

    std::filesystem::remove_all(root);
}
