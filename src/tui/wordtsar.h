//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
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

#ifndef WORDTSAR_WSTUI_APP_H
#define WORDTSAR_WSTUI_APP_H

#include "src/tui/wordstartui/src/tuiapplication.h"
#include "src/tui/wordstartui/src/menubar.h"
#include "src/tui/wordstartui/src/progressdialog.h"
#include "src/tui/dialogs/dialoghost.h"
#include "src/tui/dialogs/filebrowser.h"
#include "src/core/include/config.h"
#include "src/core/utils/abouttext.h"

#include <chrono>
#include <string>
#include <vector>

class cWSEditorCtrl;
class cWSEditorView;
class cConfig;
class cWordStarInput;

// Actions of the Opening screen, run from its menu panel.
enum class eOpeningAction
{
    OPEN_DOCUMENT,
    SPEED_WRITE,
    OPEN_NONDOCUMENT,
    PRINT_FILE,
    FAX,
    PRINT_FROM_KEYBOARD,
    INDEX_DOCUMENT,
    TABLE_OF_CONTENTS,
    EXIT,
    CHANGE_DIRECTORY,
    PROTECT_UNPROTECT,
    RENAME_FILE,
    COPY_FILE,
    DELETE_FILE,
    TURN_DIRECTORY_OFF,
    MACROS,
    RUN_COMMAND,
    ADDITIONAL,
    ABOUT,
    DISPLAY_STATUS,
    RECENT_FILES,
    PREFERENCES,
};

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

#ifdef WORDTSAR_TUI_TEST_HOOKS
    // Test-only: build the real menu tree and expose it for walking (menu
    // dispatch-resolution regression test). Not compiled into ws/WordTsar.
    void TestBuildMenus(void) { BuildMenus(); }
    const wordstartui::cMenuBar& TestGetMenu(void) const { return mMenu; }
    cWSEditorCtrl* TestGetEditor(void) { return mEditor; }
    void TestApplyConfig(cConfig& config) { ApplyConfig(config, true); }
    std::string TestTitleText(void) const { return TitleText(); }
    // Load a file the way the command line and the Opening screen do.
    void TestOpenFromOpening(const std::string& path);
    // Opening screen state, driven through HandleEvent.
    void TestEnterOpening(void) { mMode = APP_OPENING; }
    bool TestHandleEvent(const wordstartui::sInputEvent& event) { return HandleEvent(event); }
    bool TestInOpening(void) const { return mMode == APP_OPENING; }
    bool TestInEditor(void) const { return mMode == APP_EDITOR; }
    bool TestOpeningBrowserFocus(void) const { return mOpeningBrowserFocus; }
    bool TestOpeningWantPrint(void) const { return mOpeningWantPrint; }
    bool TestOpeningWantTOC(void) const { return mOpeningWantTOC; }
    bool TestOpeningWantIndex(void) const { return mOpeningWantIndex; }
    bool TestOpeningKeyAction(char typed, eOpeningAction& action) const
    {
        int index = 0;
        return OpeningKeyAction(typed, action, index);
    }
    const std::string& TestFilename(void) const { return mFilename; }
    const wordstartui::cMenuBar& TestGetOpeningMenu(void) const { return mOpeningMenu; }
    void TestSetOpeningDirectory(const std::string& path) { mBrowser.SetDirectory(path); }
    std::string TestOpeningDirectory(void) const { return mBrowser.GetCurrentDirectory(); }
    std::string TestOpeningSelectedName(void) const { return mBrowser.GetSelectedName(); }
    void TestDrawOpening(wordstartui::cScreen& screen)
    {
        mRows = screen.GetRows();
        mCols = screen.GetCols();
        DrawOpeningMenu(screen);
    }
    // Startup screen with fixed system values instead of the running system's.
    void TestSetSystemInfo(const abouttext::sSystemInfo& info)
    {
        mStartupSystem = info;
        mStartupSystemKnown = true;
    }
    void TestDrawStartup(wordstartui::cScreen& screen)
    {
        mRows = screen.GetRows();
        mCols = screen.GetCols();
        DrawStartupScreen(screen);
    }
    // Start-up the way Run() does before the event loop, without the loop.
    void TestStartSession(int argc, char* argv[]) { StartSession(argc, argv); }
    bool TestInStartup(void) const { return mMode == APP_STARTUP; }
    void TestDraw(wordstartui::cScreen& screen)
    {
        mRows = screen.GetRows();
        mCols = screen.GetCols();
        Draw(screen, GetTheme());
    }
    void TestTickStartup(void) { TickStartupScreen(); }
    std::chrono::milliseconds TestStartupDuration(void) const { return mStartupDuration; }
    void TestSetStartupDuration(std::chrono::milliseconds duration) { mStartupDuration = duration; }
    // Modal dialogs are counted and cancelled instead of run.
    void TestCountModals(void) { mTestCountModals = true; }
    int TestModalCount(void) const { return mTestModalCount; }
    std::string TestAboutMessage(void) const { return AboutMessage(); }
#endif

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
        APP_STARTUP,
        APP_OPENING,
        APP_EDITOR
    };

    void SetMode(eAppMode mode);
    void StartSession(int argc, char* argv[]);
    void TickStartupScreen(void);
    void EndStartupScreen(void);
    void LayoutChrome(void);
    void BuildMenus(void);
    void BuildOpeningMenus(void);
    void InjectChord(int controlCode, char follow);
    void InjectControl(int controlCode);
    void CheckCommandSignals(void);
    void OpenPreferences(void);
    void OpenSystemPreferences(void);
    void ApplyConfig(cConfig& config, bool applyColors);
    void WriteConfig(void);
    void OpenRecentFiles(void);
    // File > Open/Read: prompts for a file and replaces the whole current
    // document with it (real WS7 "Open a document"). Distinct from ^K R /
    // Insert > File, which now inserts a file at the cursor instead -- see
    // cWordStarInput::OnControlKChar case 'r'.
    void OpenFile(void);
    // Present the character-style selector and apply the chosen style.
    void SelectStyle(void);

    void DrawStartupScreen(wordstartui::cScreen& screen);
    void DrawOpeningMenu(wordstartui::cScreen& screen);
    void DrawOpeningBars(wordstartui::cScreen& screen);
    void DrawOpeningPanel(wordstartui::cScreen& screen);
    void DrawOpeningTitle(wordstartui::cScreen& screen);
    void DrawOpeningFiles(wordstartui::cScreen& screen, int headerRow, int listRow, const char* upperRule,
                          const wordstartui::sStyle& upperStyle);
    wordstartui::sStyle OpeningBarStyle(void) const;
    bool HandleOpeningKey(const wordstartui::sInputEvent& event);
    bool HandleOpeningMenuKey(const wordstartui::sInputEvent& event);
    bool OpeningKeyAction(char typed, eOpeningAction& action, int& index) const;
    void RunOpeningAction(eOpeningAction action);
    void HandleOpeningClick(const wordstartui::sInputEvent& event);
    void OpenOpeningSelection(void);
    void ShowNotAvailable(void);
    void ShowOpeningStatus(void);
    void ShowOpeningHelp(void);
    // Real legal notices (copyright, no-warranty, redistribution rights, how
    // to view the licence) -- distinct from About()'s memory/status screen,
    // which stays reachable on its own via ^O? and the Opening Menu's "?".
    void ShowAboutWordTsar(void);
    std::string AboutMessage(void) const;
    void GenerateTOCFromFile(void);
    void GenerateIndexFromFile(void);
    void InsertTOCEntry(void);
    void InsertIndexEntry(void);

    void DrawEditor(wordstartui::cScreen& screen, const wordstartui::cTheme& theme);
    void DrawTitleBar(wordstartui::cScreen& screen, int row);
    std::string TitleText(void) const;
    void DrawTopStatus(wordstartui::cScreen& screen, int row);
    // Dispatch a left click on the top status bar (font/style label, B/I/U,
    // command-tags "*", and L/C/R/J alignment). Returns true if a region was hit.
    bool HandleTopStatusClick(int col);
    // Dispatch a left click on the bottom status bar (Insert/Overwrite mode
    // field toggles the mode; page field opens Go To Page). True if a hit.
    bool HandleBottomStatusClick(int col);
    void DrawRuler(wordstartui::cScreen& screen, int row);
    void DrawBottomStatus(wordstartui::cScreen& screen);

    // When ^O T centering is on, returns the left-margin-to-right-margin
    // width in columns, clamped to [10, mCols], and the column offset that
    // centers it in the terminal. Both are 0/mCols (no centering) otherwise,
    // or when the layout/font metrics aren't available yet. Shared by
    // DrawEditor (editing pane bounds) and DrawRuler (marker positions) so
    // they can never drift out of sync with each other.
    void GetEditorHorizontalExtent(int& offset, int& contentCols) const;

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
    // Dedicated WordStar-chord input handler for menu dispatch only -- never
    // the active mEditor->GetInput(), so a menu click always runs the real
    // WordStar-mode command regardless of the user's selected keyboard mode,
    // and can't collide with a chord the user has half-typed on the real one.
    cWordStarInput* mMenuInput; // owned
    wordstartui::cMenuBar mMenu;
    wordstartui::cMenuBar mOpeningMenu;   // Opening screen menu bar (help level 4)
    std::string mFilename;
    int mRows;
    int mCols;
    wordstartui::sTerminalCapabilities mCapabilities{};

    eAppMode mMode;            // startup screen / opening menu / editor
    bool mHaveFileArg;         // a file was given on the command line
    abouttext::sSystemInfo mStartupSystem;   // system values shown on the startup screen
    bool mStartupSystemKnown;                // mStartupSystem has been filled
    bool mShowStartupScreen;                 // [tui.display] showStartupScreen
    std::chrono::milliseconds mStartupDuration;              // how long the startup screen stays
    std::chrono::steady_clock::time_point mStartupShownAt;   // first frame of the startup screen
    bool mStartupTiming;                     // mStartupShownAt is set
#ifdef WORDTSAR_TUI_TEST_HOOKS
    bool mTestCountModals = false;
    int mTestModalCount = 0;
#endif
    wsui::cFileBrowser mBrowser;  // file picker on the opening screen
    bool mOpeningBrowserFocus;    // true when the browser has focus (not the menu)
    bool mOpeningListActive;      // true when arrows have activated the list without focus

    // A clickable Opening panel entry: columns [col, end) of row; item is
    // the kOpeningItems index, -1 for F1 help.
    struct sOpeningHit
    {
        int row;
        int col;
        int end;
        int item;
    };
    std::vector<sOpeningHit> mOpeningPanelHits;
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
