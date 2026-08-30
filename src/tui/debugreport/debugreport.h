#ifndef TUI_DEBUGREPORT_H
#define TUI_DEBUGREPORT_H

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

#include <string>

/// @ingroup TUI
/// @{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTUIDebugReport
///
/// @brief
/// TUI crash debug report generator and system information collector.
/// Qt-free equivalent of the GUI cDebugReport class. Gathers system
/// information via POSIX uname(), locates crash data (core dumps and
/// backtraces), and packages everything into a timestamped ZIP file
/// for bug report submission. Output goes to stderr and files since
/// no interactive dialog is available during a crash in terminal mode.
///
/////////////////////////////////////////////////////////////////////////////
class cTUIDebugReport
{
    // =================================================================
    // METHODS
    // =================================================================
public:
    cTUIDebugReport(void);
    ~cTUIDebugReport(void);

    void SetName(std::string name);
    void Show(void);
    void Coalesce(void);

private:
    void GetSystemInfo(void);

    bool FindCoreDump(std::string &dir, std::string &corefilename);
    bool FindBacktrace(std::string &dir, std::string &backtrace);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================
private:
    std::string mBuildCPU;
    std::string mCurrentCPU;
    std::string mKernel;
    std::string mKernelVersion;
    std::string mPrettyProduct;

    std::string mName;
    std::string mPath;
};

/// @}

#endif // TUI_DEBUGREPORT_H
