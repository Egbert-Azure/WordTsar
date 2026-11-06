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

#ifndef TERMINATESIGNAL_H
#define TERMINATESIGNAL_H

#include <functional>
#include <memory>

#include <signal.h>

class QSocketNotifier;
class cEditorCtrl;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTerminateSignal
///
/// @brief
/// Handles SIGTERM as a request to quit. The signal handler only writes a
/// byte to a self-pipe; a QSocketNotifier picks it up in the Qt event loop
/// and runs the callback once. Construction replaces the current SIGTERM
/// action, destruction restores it. Other signals are not touched. Only one
/// instance may exist at a time.
///
/////////////////////////////////////////////////////////////////////////////
class cTerminateSignal
{
public:
    explicit cTerminateSignal(std::function<void()> onTerminate);
    ~cTerminateSignal();

    cTerminateSignal(const cTerminateSignal&) = delete;
    cTerminateSignal& operator=(const cTerminateSignal&) = delete;

    bool IsInstalled(void) const;

    static void Handler(int sig);

private:
    void OnActivated(void);

    std::function<void()> mOnTerminate;
    std::unique_ptr<QSocketNotifier> mNotifier;
    struct sigaction mPrevious;
    bool mInstalled = false;
    bool mFired = false;
};

// Saves the document to an emergency file when it has unsaved changes.
void EmergencySaveOnTerminate(cEditorCtrl *editor);

#endif // TERMINATESIGNAL_H
