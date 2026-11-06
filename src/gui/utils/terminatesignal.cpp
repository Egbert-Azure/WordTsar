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

#include "terminatesignal.h"

#include "src/gui/editor/editorctrl.h"
#include "src/core/document/document.h"

#include <QSocketNotifier>

#include <cerrno>
#include <cstring>

#include <fcntl.h>
#include <unistd.h>

namespace
{

// Self-pipe: [0] read end watched by the QSocketNotifier, [1] written by Handler.
int gPipe[2] = { -1, -1 };

bool MakeNonBlockingCloseOnExec(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    if ((flags == -1) || (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1))
    {
        return false;
    }
    return fcntl(fd, F_SETFD, FD_CLOEXEC) != -1;
}

void ClosePipe(void)
{
    for (int &fd : gPipe)
    {
        if (fd != -1)
        {
            close(fd);
            fd = -1;
        }
    }
}

} // namespace


/////////////////////////////////////////////////////////////////////////////
///
/// @param  onTerminate [in] run once in the event loop after SIGTERM
///
/// @brief
/// Creates the self-pipe and its QSocketNotifier, then installs Handler as
/// the SIGTERM action, saving the previous action. IsInstalled() is false if
/// any step fails; the SIGTERM action is then unchanged.
///
/////////////////////////////////////////////////////////////////////////////
cTerminateSignal::cTerminateSignal(std::function<void()> onTerminate)
    : mOnTerminate(std::move(onTerminate))
{
    std::memset(&mPrevious, 0, sizeof(mPrevious));

    if (gPipe[0] != -1)
    {
        return;
    }
    if (pipe(gPipe) != 0)
    {
        gPipe[0] = -1;
        gPipe[1] = -1;
        return;
    }
    if (!MakeNonBlockingCloseOnExec(gPipe[0]) || !MakeNonBlockingCloseOnExec(gPipe[1]))
    {
        ClosePipe();
        return;
    }

    mNotifier = std::make_unique<QSocketNotifier>(gPipe[0], QSocketNotifier::Read);
    QObject::connect(mNotifier.get(), &QSocketNotifier::activated, [this]() { OnActivated(); });

    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = Handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTERM, &sa, &mPrevious) != 0)
    {
        mNotifier.reset();
        ClosePipe();
        return;
    }

    mInstalled = true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Restores the previous SIGTERM action and closes the self-pipe.
///
/////////////////////////////////////////////////////////////////////////////
cTerminateSignal::~cTerminateSignal()
{
    if (!mInstalled)
    {
        return;
    }

    sigaction(SIGTERM, &mPrevious, nullptr);
    mNotifier.reset();
    ClosePipe();
}


bool cTerminateSignal::IsInstalled(void) const
{
    return mInstalled;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  sig [in] signal number (unused)
///
/// @brief
/// SIGTERM handler. Writes one byte to the self-pipe; async-signal-safe.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminateSignal::Handler(int /*sig*/)
{
    int savedErrno = errno;
    if (gPipe[1] != -1)
    {
        char byte = 1;
        ssize_t ignored = write(gPipe[1], &byte, 1);
        (void)ignored;
    }
    errno = savedErrno;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Drains the self-pipe and runs the callback, once per instance.
///
/////////////////////////////////////////////////////////////////////////////
void cTerminateSignal::OnActivated(void)
{
    char buffer[64];
    while (read(gPipe[0], buffer, sizeof(buffer)) > 0)
    {
    }

    if (mFired)
    {
        return;
    }
    mFired = true;

    if (mOnTerminate)
    {
        mOnTerminate();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor [in] editor whose document is saved; may be nullptr
///
/// @brief
/// Saves the document to an emergency file next to it when it has unsaved
/// changes. Shows no dialog.
///
/////////////////////////////////////////////////////////////////////////////
void EmergencySaveOnTerminate(cEditorCtrl *editor)
{
    if ((editor == nullptr) || (editor->GetDocument() == nullptr) || !editor->GetDocument()->IsModified())
    {
        return;
    }

    editor->EmergencySaveFile(const_cast<char*>("terminated by signal"));
}
