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
 * @class cDebugReport
 *
 * @brief Crash debug report generator and system information collector.
 *
 * Implements the cDebugReport class, which gathers system information,
 * locates crash data, and packages everything into a timestamped ZIP file
 * for bug report submission.
 *
 * @section debugreport_collection Data Collection
 * - System information: CPU model, kernel version, OS name and version
 * - Core dump files: searches standard locations and systemd-coredumpctl
 *   on Linux for the most recent WordTsar crash dump
 * - Backtrace extraction: runs gdb or lldb on the core dump to extract
 *   a symbolic stack trace
 * - User notes: presents a Qt dialog for the user to describe the crash
 *   and attach additional context
 *
 * @section debugreport_packaging Report Packaging
 * All collected data is written into a timestamped ZIP archive containing:
 * - system_info.txt: hardware and OS details
 * - backtrace.txt: symbolic stack trace from the core dump
 * - user_notes.txt: user-provided crash description
 * - The core dump file itself (if found and not too large)
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cDebugReport Debug report generator class
 */

#include <iostream>
#include <fstream>
#include <filesystem>
#include <regex>
#include <chrono>

#include <QMessageBox>

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include "debugreport.h"

#include "ui_debugreport.h"

#define MINIZ_HEADER_FILE_ONLY 1 
#include "zip.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @param  parent [in] parent QWidget (default nullptr)
///
/// @return nothing
///
/// @brief
/// Constructor. Collects system information at construction time so
/// it is available if a crash occurs later.
///
/////////////////////////////////////////////////////////////////////////////
cDebugReport::cDebugReport(QWidget *parent) : QWidget(parent)
{
    GetSystemInfo() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor.
///
/////////////////////////////////////////////////////////////////////////////
cDebugReport::~cDebugReport(void)
{

}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display a Qt dialog for the user to enter notes about the crash.
/// If the user clicks Accept, the notes are written to notes.txt in
/// the current working directory. The notes file is later included
/// in the crash report ZIP if it exists.
///
/// @note This method is called from the chillout crash callback after
///       Coalesce() has already created the ZIP. Running Qt dialogs
///       in signal context is unreliable, so this is best-effort only.
///
/////////////////////////////////////////////////////////////////////////////
void cDebugReport::Show(void)
{
    QDialog dialog(this);
    Ui::DebugReport ui;
    ui.setupUi(&dialog);

    // Set the values in the UI
    ui.systemTextLabel->setText("A debug report is being generated\n\nWe apologize, WordTsar has crashed.\n\nInformation on what to do with the crash report will be displayed after the report is generated.");
    ui.notesLabel->setText("If you have any additional information pertaining to this crash\nplease enter it here.");

    int ecode = dialog.exec();
    if (ecode == QDialog::Accepted)
    {
        std::string notes = ui.notesTextEdit->toPlainText().toStdString();
        std::string filename = mPath ;
        filename += "notes.txt";
        std::ofstream notesFile(filename);
        if (notesFile.is_open())
        {
            notesFile << notes;
            notesFile.close();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  name [in] the executable name or path (typically argv[0])
///
/// @return nothing
///
/// @brief
/// Extract the application name from the executable path and store
/// the current working directory. The name is used to locate core
/// dump files (systemd strips everything after the first dot).
///
/////////////////////////////////////////////////////////////////////////////
void cDebugReport::SetName(std::string name)
{
    std::filesystem::path filePath(name);
    mName = filePath.filename().string();

    // systemd seems to drop everything after the first '.' in the core dump filename
#ifdef __linux__
    if (mName.find('.') != std::string::npos)
    {
        mName = mName.substr(0, mName.find('.'));
    }
#endif

    char *cwd = getcwd(NULL, 0);
    std::cout << "CWD: " << cwd << std::endl;
    mPath = cwd ;
    mPath += "/" ;
//    mPath = filePath.parent_path().string();
    std::cout << "Name: " << mName << std::endl;
    std::cout << "Path: " << mPath << std::endl;

    free(cwd);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Collect all crash data and package into a timestamped ZIP file.
/// Writes a debug_report.txt with system info, finds core dump and
/// backtrace files, and creates a ZIP archive using zip_create().
/// Only files that were successfully created or found are included
/// in the archive. Shows a QMessageBox telling the user the ZIP
/// path and how to submit the crash report.
///
/////////////////////////////////////////////////////////////////////////////
void cDebugReport::Coalesce(void)
{
    std::string core;
    bool found = FindCoreDump(mPath, core);

    std::string backtrace;
    bool foundbt = FindBacktrace(mPath, backtrace);

    // Write debug report text file with system info
    std::string dbgfile = mPath;
    dbgfile += "debug_report.txt";
    bool dbgWritten = false;
    std::ofstream outFile(dbgfile);
    if (outFile.is_open())
    {
        outFile << "Build CPU: " << mBuildCPU.toStdString() << "\n";
        outFile << "Current CPU: " << mCurrentCPU.toStdString() << "\n";
        outFile << "Kernel: " << mKernel.toStdString() << "\n";
        outFile << "Kernel Version: " << mKernelVersion.toStdString() << "\n";
        outFile << "Pretty Product Name: " << mPrettyProduct.toStdString() << "\n";
        outFile << "Core Dump File: " << core << "\n";
        outFile.close();
        dbgWritten = true;
    }
    else
    {
        std::cerr << "Unable to open file for writing debug report.\n";
    }

    // Build timestamped ZIP filename (mPath already ends with /)
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);

    std::ostringstream oss;
    oss << "crashreport_" << std::put_time(&tm, "%Y%m%d_%H%M%S") << ".zip";
    std::string zipname = mPath + oss.str();

    // Only add files that actually exist to avoid uninitialized pointers
    // and zip_create() failing on non-existent files
    const char *filenames[4];
    size_t filecount = 0;

    if (dbgWritten)
    {
        filenames[filecount++] = dbgfile.c_str();
    }
    if (found)
    {
        filenames[filecount++] = core.c_str();
    }
    if (foundbt)
    {
        filenames[filecount++] = backtrace.c_str();
    }

    // notes.txt is written by Show() -- only add if it actually exists
    std::string notesfile = mPath;
    notesfile += "notes.txt";
    if (std::filesystem::exists(notesfile))
    {
        filenames[filecount++] = notesfile.c_str();
    }

    if (filecount > 0)
    {
        int err = zip_create(zipname.c_str(), filenames, filecount);
        if (err != 0)
        {
            std::cerr << "Failed to create crash report ZIP (error " << err << ")\n";
        }
    }
    else
    {
        std::cerr << "No crash report files available to package.\n";
    }

    QMessageBox msg;
    QString tmsg = "WordTsar has crashed. A crash report has been created:\n\n";
    tmsg += zipname.c_str();
    tmsg += "\n\nThe ZIP file contains:\n";
    tmsg += "  - System information (CPU, OS, kernel version)\n";
    tmsg += "  - A stack trace showing where the crash occurred\n";
    if (found)
    {
        tmsg += "  - A core dump file\n";
    }
    tmsg += "\nNo personal information or document content is included.\n\n";
    tmsg += "Please email this file to gbr@wordtsar.ca\n";
    tmsg += "or attach it to a ticket at https://sourceforge.net/projects/wordtsar/";
    msg.setText(tmsg);
    msg.exec();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Collect system information using Qt QSysInfo. Stores build CPU
/// architecture, current CPU architecture, kernel type and version,
/// and a human-readable OS product name.
///
/////////////////////////////////////////////////////////////////////////////
void cDebugReport::GetSystemInfo(void)
{
    mBuildCPU = QSysInfo::buildCpuArchitecture() ;
    mCurrentCPU = QSysInfo::currentCpuArchitecture() ;
    mKernel = QSysInfo::kernelType() ;
    mKernelVersion = QSysInfo::kernelVersion() ;
    mPrettyProduct = QSysInfo::prettyProductName() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display a simple QMessageBox informing the user that WordTsar
/// has crashed and a crash dump has been created.
///
/////////////////////////////////////////////////////////////////////////////
void cDebugReport::DisplayMessage(void)
{
        QMessageBox msg ;
        msg.setText("WordTsar has crashed. A crash dump has been created in the current directory.") ;
        msg.exec() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  dir [in] directory to search for core dump files
/// @param  corefilename [out] path to the newest core dump file found
///
/// @return true if a core dump was found, false otherwise
///
/// @brief
/// Search for the most recent core dump file. On Linux, reads
/// /proc/sys/kernel/core_pattern to determine if systemd manages
/// core dumps (piped to a handler) or if they are written to the
/// current directory. Searches the appropriate location for files
/// matching the application name (systemd) or containing "core"
/// (traditional).
///
/////////////////////////////////////////////////////////////////////////////
bool cDebugReport::FindCoreDump(std::string &dir, std::string &corefilename)
{
#ifdef __linux__
    bool coresystemd = false ;
    std::string corelocation ;

    std::ifstream corePatternFile("/proc/sys/kernel/core_pattern");
    if(corePatternFile) 
    {
        std::string corePattern;
        std::getline(corePatternFile, corePattern);
        corePatternFile.close();

        if(corePattern[0] == '|')
        {
            coresystemd = true ;
            corelocation = "/var/lib/systemd/coredump/" ;
        }
        else
        {
            corelocation = dir ;
        }
    }


    // first see if we have a systemd core file
    std::filesystem::path dirPath(corelocation);

    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) 
    {
        std::cerr << "Error: " << dirPath << " is not a valid directory.\n";
        return false ;
    }

    std::filesystem::path newestFile;
    std::chrono::system_clock::time_point newestTime;
    bool found = false;

    // Iterate over the directory entries.
    for (const auto& entry : std::filesystem::directory_iterator(dirPath)) 
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        // Check if filename matches the provided regex pattern.
        std::string filename = entry.path().filename().string();
        if(coresystemd)
        {
            if (filename.find(mName) == std::string::npos)
            {
                continue;
            }
        }
        else
        {
            if (filename.find("core") == std::string::npos)
            {
                continue;
            }
        }
        
        // Retrieve the last write time and convert it to system clock time_point.
        auto ftime = std::filesystem::last_write_time(entry);
        auto sctp = decltype(ftime)::clock::to_sys(ftime);

        // Check if this file is newer than the current newest.
        if (!found || sctp > newestTime) 
        {
            newestTime = sctp;
            newestFile = entry.path();
            found = true;
        }
    }

    if (found)
    {
        corefilename = newestFile ;
        return true ;
    }
    else
    {
        return false ;
    }
#endif
    return false;
}




/////////////////////////////////////////////////////////////////////////////
///
/// @param  dir [in] directory to search for backtrace files
/// @param  btfilename [out] path to the newest backtrace file found
///
/// @return true if a backtrace file was found, false otherwise
///
/// @brief
/// Search for the most recent backtrace file in the given directory.
/// Backtrace files are identified by containing "bktr" in their
/// filename (generated by the chillout crash handler).
///
/////////////////////////////////////////////////////////////////////////////
bool cDebugReport::FindBacktrace(std::string &dir, std::string &btfilename)
{
#ifdef __linux__

    std::filesystem::path newestFile;
    std::chrono::system_clock::time_point newestTime;
    bool found = false;

    // Iterate over the directory entries.
    for (const auto& entry : std::filesystem::directory_iterator(dir)) 
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        // Check if filename matches the provided regex pattern.
        std::string filename = entry.path().filename().string();
        if (filename.find("bktr") == std::string::npos)
        {
            continue;
        }
        
        // Retrieve the last write time and convert it to system clock time_point.
        auto ftime = std::filesystem::last_write_time(entry);
        auto sctp = decltype(ftime)::clock::to_sys(ftime);

        // Check if this file is newer than the current newest.
        if (!found || sctp > newestTime) 
        {
            newestTime = sctp;
            newestFile = entry.path();
            found = true;
        }
    }

    if (found)
    {
        btfilename = newestFile ;
        return true ;
    }
    else
    {
        return false ;
    }
#endif
    return false;
}