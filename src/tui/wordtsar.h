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

#ifndef WORDTSAR_WSTUI_APP_H
#define WORDTSAR_WSTUI_APP_H

#include "src/tui/wordstartui/src/tuiapplication.h"
#include "src/tui/wordstartui/src/menubar.h"
#include "src/tui/wordstartui/src/progressdialog.h"
#include "src/tui/dialogs/dialoghost.h"
#include "src/tui/dialogs/filebrowser.h"
#include "src/core/include/config.h"

#include <chrono>
#include <string>

class cWSEditorCtrl;
class cWSEditorView;
class cConfig;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cWSWordTsar
///
/// @brief
/// Main wordstartui application: owns the editor control and its view, draws
/// the chrome (menu bar, status bars) and the editor area, and routes input.
/// This is the TUI application shell.
///
/////////////////////////////////////////////////////////////////////////////
class cWSWordTsar final : public wordstartui::cTuiApplication, public iWSDialogHost
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cWSWordTsar(void);
    ~cWSWordTsar(void) override;

    int Run(int argc, char* argv[]);

    // ---- iWSDialogHost ----
    void HostRunModal(wordstartui::cDialog& dialog) override;
    void HostRunModalRaw(
        const std::function<void(wordstartui::cScreen&, const wordstartui::cTheme&)>& draw,
        const std::function<bool(const wordstartui::sInputEvent&)>& handle) override;
    const wordstartui::cTheme& HostTheme(void) override;
    wordstartui::sTerminalSize HostScreenSize(void) override;
    wordstartui::sTerminalCapabilities HostCapabilities(void) override;
    void HostShowLoadProgress(const std::string& message, int percent) override;
    void HostHideLoadProgress(void) override;

protected:
    void Draw(wordstartui::cScreen& screen, const wordstartui::cTheme& theme) override;
    bool HandleEvent(const wordstartui::sInputEvent& event) override;
    void OnResize(int rows, int cols) override;
    void OnAfterDraw(wordstartui::cTerminalDriver& driver) override;

private:
    // Startup phases before the editor takes over.
    enum eAppMode
    {
        APP_SPLASH,
        APP_OPENING,
        APP_EDITOR
    };

    void LayoutChrome(void);
    void BuildMenus(void);
    void InjectChord(int controlCode, char follow);
    void InjectControl(int controlCode);
    void CheckCommandSignals(void);
    void OpenPreferences(void);
    void OpenSystemPreferences(void);
    void ApplyConfig(cConfig& config, bool applyColors);
    void WriteConfig(void);
    void OpenRecentFiles(void);
    // Present the character-style selector and apply the chosen style.
    void SelectStyle(void);

    void DrawSplash(wordstartui::cScreen& screen);
    void DrawOpeningMenu(wordstartui::cScreen& screen);
    bool HandleOpeningKey(const wordstartui::sInputEvent& event);
    void ShowOpeningStatus(void);
    void ShowOpeningHelp(void);
    void GenerateTOCFromFile(void);
    void GenerateIndexFromFile(void);

    void DrawEditor(wordstartui::cScreen& screen, const wordstartui::cTheme& theme);
    void DrawTitleBar(wordstartui::cScreen& screen, int row);
    void DrawTopStatus(wordstartui::cScreen& screen, int row);
    // Dispatch a left click on the top status bar (font/style label, B/I/U,
    // command-tags "*", and L/C/R/J alignment). Returns true if a region was hit.
    bool HandleTopStatusClick(int col);
    // Dispatch a left click on the bottom status bar (Insert/Overwrite mode
    // field toggles the mode; page field opens Go To Page). True if a hit.
    bool HandleBottomStatusClick(int col);
    void DrawRuler(wordstartui::cScreen& screen, int row);
    void DrawBottomStatus(wordstartui::cScreen& screen);

    bool IsHelpVisible(void);
    int HelpPanelRows(void);
    void DrawHelpPanel(wordstartui::cScreen& screen, int startRow, int rows);
    std::string GetHelpText(eHelpDisplay help) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
    cWSEditorCtrl* mEditor;    // owned
    cWSEditorView* mView;      // owned
    wordstartui::cMenuBar mMenu;
    std::string mFilename;
    int mRows;
    int mCols;
    wordstartui::sTerminalCapabilities mCapabilities{};

    eAppMode mMode;            // splash / opening menu / editor
    int mSplashFrames;         // frames the splash has been shown
    int mOpeningSel;           // selected opening-menu item index
    bool mHaveFileArg;         // a file was given on the command line
    wsui::cFileBrowser mBrowser;  // file picker on the opening screen
    bool mOpeningBrowserFocus;    // true when the browser has focus (not the menu)
    bool mOpeningWantPrint;       // true when the browser was opened via "print a file"
    bool mOpeningWantTOC;         // true when the browser was opened via "table of contents"
    bool mOpeningWantIndex;       // true when the browser was opened via "index a document"

    // Chrome visibility (Preferences > Display On Screen).
    bool mShowTitle;
    bool mShowMenu;
    bool mShowTopStatus;
    bool mShowRuler;
    bool mShowBottomStatus;

    // Context-help delay (^K/^Q/etc. panels appear ~1s after the chord; the main
    // edit menu shows immediately).
    eHelpDisplay mLastHelpMode;
    std::chrono::steady_clock::time_point mHelpModeChangedAt;

    // Timestamp of the last auto-save; the frame loop saves the -bak backup
    // once the configured interval elapses.
    std::chrono::steady_clock::time_point mLastAutoSave;

    // True once the terminal attribute capabilities have been read from the
    // driver and applied to the editor.
    bool mCapsApplied;

    // True while the post-loop emergency save runs, so the load-progress
    // overlay is suppressed (the event loop and terminal are already gone).
    bool mEmergencySaving;

    // Animation frame for the bottom-status busy indicator.
    int mBusyFrame;

    // Row of the top status bar in the last drawn frame (-1 when hidden), used
    // to hit-test clicks on it.
    int mTopStatusRow;

    // Bottom status bar row (-1 when hidden) and the absolute click ranges of
    // its mode and page fields, recorded during DrawBottomStatus for hit-testing.
    int mBottomStatusRow;
    int mModeClickStart;
    int mModeClickEnd;
    int mPageClickStart;
    int mPageClickEnd;

    // Overlay shown while a file loads (driven by the editor's FileIOProgress).
    wordstartui::cProgressDialog mLoadProgress;

    // Chrome colors read from the config ([tui.screen] section).
    wordstartui::sColor mTitleFg;
    wordstartui::sColor mTitleBg;
    wordstartui::sColor mStatusFg;
    wordstartui::sColor mStatusBg;
    wordstartui::sColor mRulerFg;
    wordstartui::sColor mRulerBg;
    wordstartui::sColor mHelpFg;
    wordstartui::sColor mHelpBg;
    wordstartui::sColor mHelpKeyFg;
    wordstartui::sColor mHelpKeyBg;
};

#endif // WORDTSAR_WSTUI_APP_H
