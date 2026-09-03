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

#include "wordtsar.h"
#include "editor/editorctrl.h"
#include "editor/editorview.h"
#include "dialogs/dialogs.h"

#include "src/tui/wordstartui/src/screen.h"
#include "src/tui/wordstartui/src/theme.h"
#include "src/tui/wordstartui/src/terminaldriver.h"
#include "src/tui/wordstartui/src/dialog.h"
#include "src/tui/wordstartui/src/tuidefs.h"
#include "src/tui/wordstartui/src/checkbox.h"
#include "src/tui/wordstartui/src/button.h"
#include "src/tui/wordstartui/src/label.h"
#include "src/tui/wordstartui/src/dropdown.h"

#include "src/core/include/version.h"
#include "src/core/generate/tocindexgenerator.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/layout/layoutstructs.h"
#include "src/core/document/document.h"
#include "src/core/utils/config.h"
#include "src/tui/wordstartui/src/listbox.h"

#include <algorithm>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>

using wordstartui::cScreen;
using wordstartui::cTheme;
using wordstartui::cTerminalDriver;
using wordstartui::sInputEvent;
using wordstartui::sRect;
using wordstartui::sStyle;
using wordstartui::sColor;

namespace
{

// Set by the SIGTERM/SIGHUP/SIGINT handler. The frame loop polls it and runs
// the emergency save in normal context (the handler stays async-signal-safe).
volatile sig_atomic_t gShutdownPending = 0;

void EmergencySignalHandler(int /*sig*/)
{
    gShutdownPending = 1;
}

// WordStar control-key prefixes.
constexpr int CTRL_B = 2;
constexpr int CTRL_K = 11;
constexpr int CTRL_L = 12;
constexpr int CTRL_M = 13;
constexpr int CTRL_O = 15;
constexpr int CTRL_P = 16;
constexpr int CTRL_Q = 17;
constexpr int CTRL_T = 20;
constexpr int CTRL_U = 21;
constexpr int CTRL_Y = 25;

// Splash frames before auto-advancing (~50ms per loop tick).
constexpr int SPLASH_TICKS = 12;

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
    ABOUT,
    DISPLAY_STATUS,
    RECENT_FILES,
    PREFERENCES,
};

struct sOpeningItem
{
    char key;           // typed hotkey; 0 = cursor + Enter only, no direct keypress
    const char* label;
    bool enabled;
    eOpeningAction action;
};

// The WordStar 7 opening menu, two columns, letter-for-letter matching the
// real Classic Opening Menu (WordStar 7 manual, "Classic Menus and
// Commands"). The array is ordered left column first, then right column --
// see kOpeningLeftCount. The last two rows (no letter shown) are WordTsar's
// own additions, reachable only by cursor + Enter so they can't collide with
// a real WordStar 7 binding. F1 (help) is real WS7's own 20th item; it's
// drawn and handled separately since it arrives as a function-key event, not
// a typed character (see DrawOpeningMenu / HandleOpeningKey).
const sOpeningItem kOpeningItems[] =
{
    // Left column -- 9 rows, matches real WS7 exactly.
    { 'D',  "open a document",        true,  eOpeningAction::OPEN_DOCUMENT },
    { 'S',  "speed write (new file)", true,  eOpeningAction::SPEED_WRITE },
    { 'N',  "open a nondocument",     false, eOpeningAction::OPEN_NONDOCUMENT },
    { 'P',  "print a file",           true,  eOpeningAction::PRINT_FILE },
    { '\\', "fax",                    false, eOpeningAction::FAX },
    { 'K',  "print from keyboard",    false, eOpeningAction::PRINT_FROM_KEYBOARD },
    { 'I',  "index a document",       true,  eOpeningAction::INDEX_DOCUMENT },
    { 'T',  "table of contents",      true,  eOpeningAction::TABLE_OF_CONTENTS },
    { 'X',  "exit WordTsar",          true,  eOpeningAction::EXIT },

    // Right column -- 10 rows, matches real WS7 exactly, plus two
    // WordTsar-only rows appended at the end (no letter -- see above).
    { 'L', "change drive/directory", true,  eOpeningAction::CHANGE_DIRECTORY },
    { 'C', "protect/unprotect",      false, eOpeningAction::PROTECT_UNPROTECT },
    { 'E', "rename a file",          false, eOpeningAction::RENAME_FILE },
    { 'O', "copy a file",            false, eOpeningAction::COPY_FILE },
    { 'Y', "delete a file",          false, eOpeningAction::DELETE_FILE },
    { 'F', "turn directory off",     false, eOpeningAction::TURN_DIRECTORY_OFF },
    { 'M', "macros",                 false, eOpeningAction::MACROS },
    { 'R', "run a DOS command",      false, eOpeningAction::RUN_COMMAND },
    { 'A', "about WordTsar",         true,  eOpeningAction::ABOUT },
    { '?', "display status",         true,  eOpeningAction::DISPLAY_STATUS },
    {  0,  "recent files",           true,  eOpeningAction::RECENT_FILES },
    {  0,  "preferences",            true,  eOpeningAction::PREFERENCES },
};

constexpr int kOpeningLeftCount = 9;
constexpr int kOpeningCount = 21;

}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Construct the application, its editor control, and the editor view.
///
/////////////////////////////////////////////////////////////////////////////
cWSWordTsar::cWSWordTsar(void)
{
    mEditor = new cWSEditorCtrl();
    mView = new cWSEditorView(mEditor);
    mRows = 24;
    mCols = 80;
    mMode = APP_SPLASH;
    mSplashFrames = 0;
    mOpeningSel = 0;
    mHaveFileArg = false;
    mOpeningBrowserFocus = false;
    mOpeningWantPrint = false;
    mOpeningWantTOC = false;
    mOpeningWantIndex = false;
    mShowTitle = true;
    mShowMenu = true;
    mShowTopStatus = true;
    mShowRuler = true;
    mShowBottomStatus = true;
    mLastHelpMode = HELP_NONE;
    mHelpModeChangedAt = std::chrono::steady_clock::now();
    mLastAutoSave = std::chrono::steady_clock::now();
    mCapsApplied = false;
    mEmergencySaving = false;
    mBusyFrame = 0;
    mTopStatusRow = -1;
    mBottomStatusRow = -1;
    mModeClickStart = 0;
    mModeClickEnd = 0;
    mPageClickStart = 0;
    mPageClickEnd = 0;

    mTitleFg = wordstartui::MakeRgb(0, 0, 170);
    mTitleBg = wordstartui::MakeRgb(170, 170, 170);
    mStatusFg = wordstartui::MakeRgb(0, 0, 170);
    mStatusBg = wordstartui::MakeRgb(170, 170, 170);
    mRulerFg = wordstartui::MakeRgb(170, 170, 170);
    mRulerBg = wordstartui::MakeRgb(170, 170, 170);
    mHelpFg = wordstartui::MakeRgb(0, 0, 34);
    mHelpBg = wordstartui::MakeRgb(170, 170, 170);
    mHelpKeyFg = wordstartui::MakeRgb(0, 0, 180);
    mHelpKeyBg = wordstartui::MakeRgb(170, 170, 170);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Release the view and editor control.
///
/////////////////////////////////////////////////////////////////////////////
cWSWordTsar::~cWSWordTsar(void)
{
    // Persist the session settings on normal exit (crashes call exit() and skip
    // this, which is intentional).
    if (mEditor != nullptr)
    {
        WriteConfig();
    }

    delete mView;
    delete mEditor;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int argc [in] argument count
/// @param  char* argv[] [in] argument values
///
/// @return process exit code
///
/// @brief
/// Load an optional file from the command line, build the menu, and run the
/// wordstartui event loop (splash, then opening menu or editor).
///
/////////////////////////////////////////////////////////////////////////////
int cWSWordTsar::Run(int argc, char* argv[])
{
    mEditor->SetDialogHost(this);
    mEditor->SetPreferencesHook([this](void) { OpenPreferences(); });

    // Load persisted settings and apply them before opening any file.
    cConfig startupConfig;
    startupConfig.Load();
    ApplyConfig(startupConfig, true);

    if (argc > 1)
    {
        mFilename = argv[1];
        mHaveFileArg = true;
        mEditor->LoadFile(mFilename);
        mEditor->RelayoutAndRedraw();
    }

    BuildMenus();

    // Save the document if the app is killed or the terminal closes. The
    // handler only sets a flag; OnAfterDraw performs the actual save. Installed
    // with sigaction after chillout's crash handlers (which claim SIGTERM) so
    // this takes over those signals for a graceful save instead of a crash dump.
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = EmergencySignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);

    int result = cTuiApplication::Run();

    // A termination signal broke the loop: save the document now that the loop
    // has stopped and the terminal is restored. mEmergencySaving suppresses the
    // load-progress UI so the save does not draw to the closed terminal.
    if (gShutdownPending != 0)
    {
        mEmergencySaving = true;
        mEditor->EmergencySaveFile(const_cast<char*>("terminated by signal"));
    }

    return result;
}

// =========================================================================
// iWSDialogHost
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cDialog& dialog [in,out] the dialog to run
///
/// @return nothing
///
/// @brief
/// Run a plain dialog modally over the editor until it reports a result.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::HostRunModal(wordstartui::cDialog& dialog)
{
    dialog.FocusFirst();

    RunModalLoop(
        [this, &dialog](cScreen& screen, const cTheme& theme)
        {
            Draw(screen, theme);
            dialog.Draw(screen, theme);
        },
        [&dialog](const sInputEvent& event)
        {
            dialog.HandleEvent(event);
            return dialog.GetResult() == wordstartui::DIALOG_RESULT_NONE;
        });
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  draw [in] frame drawer
/// @param  handle [in] event handler (returns false to close)
///
/// @return nothing
///
/// @brief
/// Run a custom modal (non-cDialog) over the editor.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::HostRunModalRaw(
    const std::function<void(cScreen&, const cTheme&)>& draw,
    const std::function<bool(const sInputEvent&)>& handle)
{
    // Redraw the editor/opening background (with the menu closed) each frame so
    // custom modals never show a stale frame -- e.g. an open menu pull-down
    // left behind when a menu item launched the dialog.
    RunModalLoop(
        [this, &draw](cScreen& screen, const cTheme& theme)
        {
            Draw(screen, theme);
            draw(screen, theme);
        },
        handle);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the application theme
///
/// @brief
/// Expose the theme so dialogs can style their widgets.
///
/////////////////////////////////////////////////////////////////////////////
const cTheme& cWSWordTsar::HostTheme(void)
{
    return GetTheme();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the current terminal size (rows, cols)
///
/// @brief
/// Report the screen geometry so dialogs can be centred.
///
/////////////////////////////////////////////////////////////////////////////
wordstartui::sTerminalSize cWSWordTsar::HostScreenSize(void)
{
    wordstartui::sTerminalSize size;
    size.rows = mRows;
    size.cols = mCols;
    return size;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the terminal's color capabilities
///
/// @brief
/// Report the cached terminal color capabilities so dialogs can pick a default
/// color depth.
///
/////////////////////////////////////////////////////////////////////////////
wordstartui::sTerminalCapabilities cWSWordTsar::HostCapabilities(void)
{
    return mCapabilities;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const std::string& message [in] progress message (the filename)
/// @param  int percent [in] progress 0..100
///
/// @return nothing
///
/// @brief
/// Paint the load-progress overlay on top of the editor and flush it. Called
/// synchronously from the editor's FileIOProgress during a blocking load.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::HostShowLoadProgress(const std::string& message, int percent)
{
    if (mEmergencySaving == true)
    {
        return;
    }

    mLoadProgress.SetTitle("Loading");
    mLoadProgress.SetMessage(message);
    mLoadProgress.SetPercent(percent);

    PresentFrame(
        [this](cScreen& screen, const cTheme& theme)
        {
            Draw(screen, theme);
            mLoadProgress.Draw(screen, theme);
        });
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Remove the load-progress overlay by repainting the editor without it.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::HostHideLoadProgress(void)
{
    if (mEmergencySaving == true)
    {
        return;
    }

    PresentFrame(
        [this](cScreen& screen, const cTheme& theme)
        {
            Draw(screen, theme);
        });
}

// =========================================================================
// Command injection
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int controlCode [in] WordStar control prefix (e.g. CTRL_K)
/// @param  char follow [in] the follow-up key completing the command
///
/// @return nothing
///
/// @brief
/// Fire a two-key WordStar command through the shared input handler, exactly as
/// if the user typed it. Used by the menu bar so menu items run real commands.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::InjectChord(int controlCode, char follow)
{
    IInputHandler* input = mEditor->GetInput();
    if (input == nullptr)
    {
        return;
    }

    input->HandleKey(static_cast<char>(controlCode), false, false);
    input->HandleKey(follow, false, false);

    mView->EnsureCaretVisible();

    CheckCommandSignals();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int controlCode [in] a single WordStar control key
///
/// @return nothing
///
/// @brief
/// Fire a single-key WordStar command through the shared input handler.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::InjectControl(int controlCode)
{
    IInputHandler* input = mEditor->GetInput();
    if (input == nullptr)
    {
        return;
    }

    input->HandleKey(static_cast<char>(controlCode), false, false);

    mView->EnsureCaretVisible();

    CheckCommandSignals();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// After a command runs, act on editor signals: quit (^KX) or return to the
/// opening menu (^KQ abandon).
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::CheckCommandSignals(void)
{
    if (mEditor->QuitRequested() == true)
    {
        Quit();
        return;
    }

    if (mEditor->ReturnToOpeningRequested() == true)
    {
        mEditor->ClearReturnToOpening();
        mFilename.clear();
        mOpeningBrowserFocus = false;
        mBrowser.SetDirectory(mBrowser.GetCurrentDirectory());
        mMode = APP_OPENING;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the Preferences dialog: on-screen chrome toggles and measurement units.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::OpenPreferences(void)
{
    cConfig config;
    config.Load();

    // Seed from the editor's live state, as OpenSystemPreferences() does --
    // otherwise ApplyConfig() below resets the file browser to the on-disk
    // default directory, discarding wherever the user navigated this session.
    config.mDefaultDirectory = mEditor->mFileDir;

    wordstartui::sTerminalSize size = HostScreenSize();

    sRect rect;
    rect.rows = 17;
    rect.cols = 46;
    rect.row = (size.rows - rect.rows) / 2;
    rect.col = (size.cols - rect.cols) / 2;
    if (rect.row < 0)
    {
        rect.row = 0;
    }
    if (rect.col < 0)
    {
        rect.col = 0;
    }

    wordstartui::cDialog dialog(rect, "Preferences");

    auto addToggle = [&](int offset, const std::string& label, bool on) -> wordstartui::cCheckBox*
    {
        wordstartui::eTriState state = wordstartui::TRI_STATE_OFF;
        if (on == true)
        {
            state = wordstartui::TRI_STATE_ON;
        }

        sRect cr;
        cr.row = rect.row + offset;
        cr.col = rect.col + 3;
        cr.rows = 1;
        cr.cols = 38;

        std::unique_ptr<wordstartui::cCheckBox> box =
            std::make_unique<wordstartui::cCheckBox>(cr, label, state);
        wordstartui::cCheckBox* ptr = box.get();
        dialog.AddWidget(std::move(box));
        return ptr;
    };

    wordstartui::cCheckBox* cbFlag = addToggle(1, "Always show flag column", config.mTuiAlwaysFlagColumn);
    wordstartui::cCheckBox* cbDot = addToggle(2, "Always show dot commands", config.mTuiAlwaysDotCommands);
    wordstartui::cCheckBox* cbScroll = addToggle(3, "Show scroll bar", config.mTuiShowScrollBar);
    wordstartui::cCheckBox* cbTitle = addToggle(4, "Show title bar", config.mTuiShowTitleBar);
    wordstartui::cCheckBox* cbMenu = addToggle(5, "Show menu bar", config.mTuiShowMenu);
    wordstartui::cCheckBox* cbStyle = addToggle(6, "Show style bar", config.mTuiShowStyleBar);
    wordstartui::cCheckBox* cbRuler = addToggle(7, "Show ruler", config.mTuiShowRuler);
    wordstartui::cCheckBox* cbStatus = addToggle(8, "Show status bar", config.mTuiShowStatusBar);

    sRect lr;
    lr.row = rect.row + 10;
    lr.col = rect.col + 3;
    lr.rows = 1;
    lr.cols = 14;
    dialog.AddWidget(std::make_unique<wordstartui::cLabel>(lr, "Measurement:"));

    sRect dr;
    dr.row = rect.row + 10;
    dr.col = rect.col + 17;
    dr.rows = 1;
    dr.cols = 16;
    std::unique_ptr<wordstartui::cDropdown> drop = std::make_unique<wordstartui::cDropdown>(dr);
    drop->SetItems({"Inches", "Centimeters", "Millimeters"});

    int measureIndex = 0;
    if (config.mMeasurement.find("mm") != std::string::npos)
    {
        measureIndex = 2;
    }
    else if (config.mMeasurement.find("cm") != std::string::npos)
    {
        measureIndex = 1;
    }
    drop->SetSelectedIndex(measureIndex);
    wordstartui::cDropdown* dropPtr = drop.get();
    dialog.AddWidget(std::move(drop));

    sRect okr;
    okr.row = rect.row + 13;
    okr.col = rect.col + 9;
    okr.rows = 1;
    okr.cols = 10;
    dialog.AddWidget(std::make_unique<wordstartui::cButton>(okr, "OK", [&dialog](void)
    {
        dialog.SetResult(wordstartui::DIALOG_RESULT_OK);
    }));

    sRect cnr;
    cnr.row = rect.row + 13;
    cnr.col = rect.col + 23;
    cnr.rows = 1;
    cnr.cols = 12;
    dialog.AddWidget(std::make_unique<wordstartui::cButton>(cnr, "Cancel", [&dialog](void)
    {
        dialog.SetResult(wordstartui::DIALOG_RESULT_CANCEL);
    }));

    HostRunModal(dialog);

    if (dialog.GetResult() != wordstartui::DIALOG_RESULT_OK)
    {
        return;
    }

    config.mTuiAlwaysFlagColumn = (cbFlag->GetState() == wordstartui::TRI_STATE_ON);
    config.mTuiAlwaysDotCommands = (cbDot->GetState() == wordstartui::TRI_STATE_ON);
    config.mTuiShowScrollBar = (cbScroll->GetState() == wordstartui::TRI_STATE_ON);
    config.mTuiShowTitleBar = (cbTitle->GetState() == wordstartui::TRI_STATE_ON);
    config.mTuiShowMenu = (cbMenu->GetState() == wordstartui::TRI_STATE_ON);
    config.mTuiShowStyleBar = (cbStyle->GetState() == wordstartui::TRI_STATE_ON);
    config.mTuiShowRuler = (cbRuler->GetState() == wordstartui::TRI_STATE_ON);
    config.mTuiShowStatusBar = (cbStatus->GetState() == wordstartui::TRI_STATE_ON);

    int index = dropPtr->GetSelectedIndex();
    if (index == 2)
    {
        config.mMeasurement = "0mm";
    }
    else if (index == 1)
    {
        config.mMeasurement = "0cm";
    }
    else
    {
        config.mMeasurement = "0i";
    }

    // Apply to the live app + editor and persist.
    ApplyConfig(config, false);
    config.Save();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cConfig& config [in] loaded configuration
///
/// @return nothing
///
/// @brief
/// Apply configuration to the editor colors, chrome visibility, and settings.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::ApplyConfig(cConfig& config, bool applyColors)
{
    mShowTitle = config.mTuiShowTitleBar;
    mShowMenu = config.mTuiShowMenu;
    mShowTopStatus = config.mTuiShowStyleBar;
    mShowRuler = config.mTuiShowRuler;
    mShowBottomStatus = config.mTuiShowStatusBar;
    mView->SetShowScrollbar(config.mTuiShowScrollBar);
    mEditor->SetCenterView(config.mTuiCenterView);

    mEditor->SetMeasurement(config.mMeasurement);
    mEditor->SetCodePage(static_cast<eCodePage>(config.mCodePage));
    mEditor->SetInputMode(static_cast<eInputMode>(config.mInputMode));
    if (config.mShowControls == true)
    {
        mEditor->SetShowControls(SHOW_ALL);
    }
    else
    {
        mEditor->SetShowControls(SHOW_NONE);
    }
    mEditor->mAlwaysFlag = config.mTuiAlwaysFlagColumn;
    mEditor->mHelpLevel = std::clamp(config.mTuiShowHelp, 0, 4);
    mEditor->SetHelpDisplay((mEditor->mHelpLevel == 3) ? HELP_MAIN : HELP_NONE);

    // Shared [editor]/[user] settings (match the GUI's ReadConfig).
    mEditor->mSpellCheckLanguage = config.mSpellCheckLanguage;
    mEditor->mSpellCheckDotCommands = config.mSpellCheckDotCommands;
    mEditor->mCaretBlinkRate = config.mCaretBlinkRate;
    mEditor->mAutoSaveIntervalSec = config.mAutoSaveInterval;
    mEditor->mDefaultFormat = config.mDefaultFormat;
    {
        // Default to ~/Documents when the user hasn't set a custom default
        // directory in Preferences -- without this fallback, the opening-menu
        // file browser (seeded from the process cwd at construction) lands
        // wherever the shell happened to be when ws was launched, not
        // anywhere predictable.
        std::string startDir = config.mDefaultDirectory.empty() == false
            ? config.mDefaultDirectory
            : cEditorBase::DefaultFileDir();
        mEditor->mFileDir = startDir;
        mBrowser.SetDirectory(startDir);
    }
    mEditor->mShortName = config.mShortName;
    mEditor->mLongName = config.mLongName;
    mEditor->mAlwaysDot = config.mTuiAlwaysDotCommands;
    if ((mEditor->GetShowControls() == SHOW_DOT) && (mEditor->mAlwaysDot == false))
    {
        mEditor->SetShowDot(false);   // parity with the GUI (wordtsar.cpp:1166-1168)
    }

    // Editor content/style colors are applied only through System Preferences,
    // (the editor keeps its built-in defaults at
    // startup).
    if (applyColors == false)
    {
        LayoutChrome();
        return;
    }

    auto rgb = [](const sRGB& color) -> wordstartui::sColor
    {
        return wordstartui::MakeRgb(static_cast<uint8_t>(color.r),
                                    static_cast<uint8_t>(color.g),
                                    static_cast<uint8_t>(color.b));
    };

    mEditor->mBGroundColour = rgb(config.mTuiBackground);
    mEditor->mTextColour = rgb(config.mTuiForeground);
    mEditor->mHighlightColour = rgb(config.mTuiHighlightBackground);
    mEditor->mHighlightFgColour = rgb(config.mTuiHighlightForeground);
    mEditor->mDotColour = rgb(config.mTuiDotBackground);
    mEditor->mDotFgColour = rgb(config.mTuiDotForeground);
    mEditor->mBlockColour = rgb(config.mTuiBlockBackground);
    mEditor->mBlockFgColour = rgb(config.mTuiBlockForeground);
    mEditor->mCommentColour = rgb(config.mTuiCommentBackground);
    mEditor->mCommentFgColour = rgb(config.mTuiCommentForeground);
    mEditor->mErrorColour = rgb(config.mTuiErrorBackground);
    mEditor->mErrorFgColour = rgb(config.mTuiErrorForeground);
    mEditor->mUnknownColour = rgb(config.mTuiUnknownBackground);
    mEditor->mUnknownFgColour = rgb(config.mTuiUnknownForeground);
    mEditor->mNotImplementedColour = rgb(config.mTuiNotImplementedBackground);
    mEditor->mNotImplementedFgColour = rgb(config.mTuiNotImplementedForeground);
    mEditor->mSearchColour = rgb(config.mTuiSearchBackground);
    mEditor->mSearchFgColour = rgb(config.mTuiSearchForeground);

    mEditor->mBoldColour = rgb(config.mTuiBoldForeground);
    mEditor->mBoldBgColour = rgb(config.mTuiBoldBackground);
    mEditor->mItalicColour = rgb(config.mTuiItalicForeground);
    mEditor->mItalicBgColour = rgb(config.mTuiItalicBackground);
    mEditor->mUnderlineColour = rgb(config.mTuiUnderlineForeground);
    mEditor->mUnderlineBgColour = rgb(config.mTuiUnderlineBackground);
    mEditor->mStrikethroughColour = rgb(config.mTuiStrikethroughForeground);
    mEditor->mStrikethroughBgColour = rgb(config.mTuiStrikethroughBackground);
    mEditor->mSuperscriptColour = rgb(config.mTuiSuperscriptForeground);
    mEditor->mSuperscriptBgColour = rgb(config.mTuiSuperscriptBackground);
    mEditor->mSubscriptColour = rgb(config.mTuiSubscriptForeground);
    mEditor->mSubscriptBgColour = rgb(config.mTuiSubscriptBackground);

    mEditor->mScrollbarFg = rgb(config.mTuiScrollbarForeground);
    mEditor->mScrollbarBg = rgb(config.mTuiScrollbarBackground);
    mEditor->mFlagFg = rgb(config.mTuiFlagColumnForeground);
    mEditor->mFlagBg = rgb(config.mTuiFlagColumnBackground);

    // Chrome bar colors ([tui.screen]).
    mTitleFg = rgb(config.mTuiTitleBarForeground);
    mTitleBg = rgb(config.mTuiTitleBarBackground);
    mStatusFg = rgb(config.mTuiStatusBarForeground);
    mStatusBg = rgb(config.mTuiStatusBarBackground);
    mRulerFg = rgb(config.mTuiRulerForeground);
    mRulerBg = rgb(config.mTuiRulerBackground);
    mHelpFg = rgb(config.mTuiHelpPanelForeground);
    mHelpBg = rgb(config.mTuiHelpPanelBackground);
    mHelpKeyFg = rgb(config.mTuiHelpPanelKeystrokeForeground);
    mHelpKeyBg = rgb(config.mTuiHelpPanelKeystrokeBackground);

    // Build the whole toolkit theme by extrapolating every widget role from the
    // WordTsar screen + editor colors ([tui.screen]/[tui.colors]). This drives
    // the menu bar AND all dialog/button/field/list widgets from the ini palette.
    wordstartui::sThemePalette palette;
    palette.name = "WordTsar";
    palette.menuFg = rgb(config.mTuiMenuBarForeground);
    palette.menuBg = rgb(config.mTuiMenuBarBackground);
    palette.menuActiveFg = rgb(config.mTuiMenuHighlightForeground);
    palette.menuActiveBg = rgb(config.mTuiMenuHighlightBackground);
    palette.menuAccelFg = rgb(config.mTuiMenuAcceleratorForeground);
    palette.menuAccelBg = rgb(config.mTuiMenuBarBackground);
    palette.titleFg = rgb(config.mTuiTitleBarForeground);
    palette.titleBg = rgb(config.mTuiTitleBarBackground);
    palette.statusFg = rgb(config.mTuiStatusBarForeground);
    palette.statusBg = rgb(config.mTuiStatusBarBackground);
    palette.helpFg = rgb(config.mTuiHelpPanelForeground);
    palette.helpBg = rgb(config.mTuiHelpPanelBackground);
    palette.helpKeyFg = rgb(config.mTuiHelpPanelKeystrokeForeground);
    palette.helpKeyBg = rgb(config.mTuiHelpPanelKeystrokeBackground);
    palette.rulerFg = rgb(config.mTuiRulerForeground);
    palette.rulerBg = rgb(config.mTuiRulerBackground);
    palette.flagFg = rgb(config.mTuiFlagColumnForeground);
    palette.flagBg = rgb(config.mTuiFlagColumnBackground);
    palette.scrollFg = rgb(config.mTuiScrollbarForeground);
    palette.scrollBg = rgb(config.mTuiScrollbarBackground);
    palette.editorFg = rgb(config.mTuiForeground);
    palette.editorBg = rgb(config.mTuiBackground);
    palette.blockFg = rgb(config.mTuiBlockForeground);
    palette.blockBg = rgb(config.mTuiBlockBackground);
    GetTheme().LoadPalette(palette);

    LayoutChrome();
    mEditor->RelayoutAndRedraw();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Harvest the editor's current settings back into the INI and save it, so the
/// session persists across restarts (mirrors the GUI's WriteConfig). Loads the
/// existing file first to preserve the GUI sections, TUI colors, and comments.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::WriteConfig(void)
{
    cConfig config;
    config.Load();

    // Shared [editor]/[user] settings.
    config.mShowControls = (mEditor->GetShowControls() == SHOW_ALL);
    config.mMeasurement = mEditor->GetMeasurement();
    config.mCodePage = static_cast<int>(mEditor->GetCodePage());
    config.mInputMode = static_cast<int>(mEditor->GetInputMode());
    config.mSpellCheckLanguage = mEditor->mSpellCheckLanguage;
    config.mSpellCheckDotCommands = mEditor->mSpellCheckDotCommands;
    config.mCaretBlinkRate = mEditor->mCaretBlinkRate;
    config.mAutoSaveInterval = mEditor->mAutoSaveIntervalSec;
    config.mDefaultFormat = mEditor->mDefaultFormat;
    config.mDefaultDirectory = mEditor->mFileDir;
    config.mShortName = mEditor->mShortName;
    config.mLongName = mEditor->mLongName;

    // TUI chrome flags (as set by ApplyConfig / System Preferences).
    config.mTuiShowHelp = mEditor->mHelpLevel;
    config.mTuiShowTitleBar = mShowTitle;
    config.mTuiShowMenu = mShowMenu;
    config.mTuiShowStyleBar = mShowTopStatus;
    config.mTuiShowRuler = mShowRuler;
    config.mTuiShowStatusBar = mShowBottomStatus;
    config.mTuiAlwaysFlagColumn = mEditor->mAlwaysFlag;
    config.mTuiAlwaysDotCommands = mEditor->mAlwaysDot;
    config.mTuiCenterView = mEditor->IsCenterViewEnabled();

    config.Save();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the full System Preferences dialog, then apply and persist the result.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::OpenSystemPreferences(void)
{
    cConfig config;
    config.Load();

    // Seed the dialog from the editor's live state (as with measurement) so it
    // reflects the current session, not just the last-saved INI. These members
    // live on cEditorBase and are applied to the editor at startup.
    config.mMeasurement = mEditor->GetMeasurement();
    config.mShortName = mEditor->mShortName;
    config.mLongName = mEditor->mLongName;
    config.mTuiAlwaysDotCommands = mEditor->mAlwaysDot;
    config.mDefaultFormat = mEditor->mDefaultFormat;
    config.mDefaultDirectory = mEditor->mFileDir;

    if (wsdialogs::SystemPreferences(this, config) == true)
    {
        ApplyConfig(config, true);
        config.Save();

        // Rebuild the menu bar so its shortcut text matches the (possibly
        // changed) keyboard mode.
        BuildMenus();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the recent-files list and open the chosen file.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::OpenRecentFiles(void)
{
    cConfig config;
    config.Load();

    std::vector<std::string> files;
    for (int index = 0; index < 10; ++index)
    {
        if (config.mRecentFiles[index].empty() == false)
        {
            files.push_back(config.mRecentFiles[index]);
        }
    }

    if (files.empty() == true)
    {
        wsdialogs::MessageBox(this, "Recent Files", "No recent files.");
        return;
    }

    wordstartui::sTerminalSize size = HostScreenSize();
    sRect rect;
    rect.rows = static_cast<int>(files.size()) + 6;
    rect.cols = 60;
    rect.row = (size.rows - rect.rows) / 2;
    rect.col = (size.cols - rect.cols) / 2;
    if (rect.row < 0)
    {
        rect.row = 0;
    }
    if (rect.col < 0)
    {
        rect.col = 0;
    }

    wordstartui::cDialog dialog(rect, "Recent Files");

    sRect lr;
    lr.row = rect.row + 2;
    lr.col = rect.col + 2;
    lr.rows = static_cast<int>(files.size());
    lr.cols = rect.cols - 4;
    std::unique_ptr<wordstartui::cListBox> list = std::make_unique<wordstartui::cListBox>(lr);
    list->SetItems(files);
    wordstartui::cListBox* listPtr = list.get();
    dialog.AddWidget(std::move(list));

    sRect okr;
    okr.row = rect.row + rect.rows - 2;
    okr.col = rect.col + 4;
    okr.rows = 1;
    okr.cols = 10;
    dialog.AddWidget(std::make_unique<wordstartui::cButton>(okr, "Open", [&dialog](void)
    {
        dialog.SetResult(wordstartui::DIALOG_RESULT_OK);
    }));

    sRect cnr;
    cnr.row = rect.row + rect.rows - 2;
    cnr.col = rect.col + 18;
    cnr.rows = 1;
    cnr.cols = 12;
    dialog.AddWidget(std::make_unique<wordstartui::cButton>(cnr, "Cancel", [&dialog](void)
    {
        dialog.SetResult(wordstartui::DIALOG_RESULT_CANCEL);
    }));

    HostRunModal(dialog);

    if (dialog.GetResult() != wordstartui::DIALOG_RESULT_OK)
    {
        return;
    }

    int selected = listPtr->GetSelectedIndex();
    if ((selected >= 0) && (selected < static_cast<int>(files.size())))
    {
        mFilename = files[static_cast<size_t>(selected)];
        mEditor->LoadFile(mFilename);
        mEditor->RelayoutAndRedraw();
        mMode = APP_EDITOR;
    }
}

// =========================================================================
// Menu bar
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Build the WordTsar menu bar, Each item fires
/// the corresponding WordStar command through the shared input handler.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::BuildMenus(void)
{
    // Rebuild from scratch so the bar can refresh when the keyboard mode
    // changes. Shortcut text (and a few labels) depend on WordStar vs Modern
    // mode, matching the GUI. SC() appends the mode's right-aligned shortcut
    // (empty when the command has none in that mode); LB() picks the label.
    mMenu.Clear();

    bool modern = (mEditor->GetInputMode() == INPUT_MODERN);
    auto SC = [modern](const std::string& ws, const std::string& mod) -> std::string
    {
        const std::string& text = modern ? mod : ws;
        if (text.empty() == true)
        {
            return std::string();
        }
        return "  " + text;
    };
    auto LB = [modern](const std::string& ws, const std::string& mod) -> std::string
    {
        return modern ? mod : ws;
    };
    auto none = [](void) {};

    // ------------------------------- File -------------------------------
    int file = mMenu.AddMenu("File", 'F');
    mMenu.AddSubItem(file, LB("&Open/Read...", "&Open...") + SC("^KR", "Ctrl+O"), [this](void) { InjectChord(CTRL_K, 'r'); });
    mMenu.AddSubItem(file, std::string("&Save") + SC("^KS", "Ctrl+S"), [this](void) { InjectChord(CTRL_K, 's'); });
    mMenu.AddSubItem(file, std::string("Save &As...") + SC("^KT", "Ctrl+Shift+S"), [this](void) { InjectChord(CTRL_K, 't'); });
    mMenu.AddSubItem(file, std::string("Save and &Close") + SC("^KD", ""), [this](void) { InjectChord(CTRL_K, 'd'); });
    mMenu.AddSeparator(file);
    mMenu.AddSubItem(file, std::string("&Print...") + SC("^KP", "Ctrl+P"), [this](void) { mEditor->Print(); });
    mMenu.AddSubItem(file, std::string("Print Pre&view") + SC("^OP", ""), [this](void) { mEditor->PrintPreview(); });
    mMenu.AddSeparator(file);
    mMenu.AddSubItem(file, std::string("&Recent Files..."), [this](void) { OpenRecentFiles(); });
    mMenu.AddSubItem(file, std::string("Pr&eferences..."), [this](void) { OpenSystemPreferences(); });
    mMenu.AddSeparator(file);
    mMenu.AddSubItem(file, std::string("E&xit WordTsar") + SC("^KX", "Alt+F4"), [this](void) { InjectChord(CTRL_K, 'x'); });

    // ------------------------------- Edit -------------------------------
    int edit = mMenu.AddMenu("Edit", 'E');
    mMenu.AddSubItem(edit, std::string("&Undo") + SC("^U", "Ctrl+Z"), [this](void) { InjectControl(CTRL_U); });
    mMenu.AddSubItem(edit, std::string("&Redo") + SC("Ctrl+Alt+U", "Ctrl+Y"), [this](void) { mEditor->Redo(); });
    mMenu.AddSeparator(edit);
    mMenu.AddSubItem(edit, std::string("Mark Block &Beginning") + SC("^KB", "Alt+K, B"), [this](void) { InjectChord(CTRL_K, 'b'); });
    mMenu.AddSubItem(edit, std::string("Mark Block En&d") + SC("^KK", "Alt+K, K"), [this](void) { InjectChord(CTRL_K, 'k'); });

    int editMove = mMenu.AddSubMenu(edit, "&Move");
    mMenu.AddSubMenuItem(editMove, std::string("Block") + SC("^KV", "Alt+K, V"), [this](void) { InjectChord(CTRL_K, 'v'); });

    int editCopy = mMenu.AddSubMenu(edit, "&Copy");
    mMenu.AddSubMenuItem(editCopy, std::string("Block") + SC("^KC", "Alt+K, C"), [this](void) { InjectChord(CTRL_K, 'c'); });
    mMenu.AddSubMenuItem(editCopy, LB("&From Clipboard", "&Paste") + SC("^K[", "Ctrl+V"), [this](void) { InjectChord(CTRL_K, '['); });
    mMenu.AddSubMenuItem(editCopy, LB("&To Clipboard", "&Copy") + SC("^K]", "Ctrl+C"), [this](void) { InjectChord(CTRL_K, ']'); });

    int editDelete = mMenu.AddSubMenu(edit, "De&lete");
    mMenu.AddSubMenuItem(editDelete, std::string("Block") + SC("^KY", "Alt+K, Y"), [this](void) { InjectChord(CTRL_K, 'y'); });
    mMenu.AddSubMenuItem(editDelete, std::string("Word") + SC("^T", "Ctrl+Del"), [this](void) { InjectControl(CTRL_T); });
    mMenu.AddSubMenuItem(editDelete, std::string("Line") + SC("^Y", ""), [this](void) { InjectControl(CTRL_Y); });
    mMenu.AddSubMenuItem(editDelete, std::string("Line Left") + SC("^QDel", "Alt+Q, DEL"), [this](void) { mEditor->DeleteLineLeft(); mEditor->PerformPostCommandUpdate(); });
    mMenu.AddSubMenuItem(editDelete, std::string("Line Right") + SC("^QY", "Alt+Q, Y"), [this](void) { mEditor->DeleteLineRight(); mEditor->PerformPostCommandUpdate(); });
    mMenu.AddSubMenuItem(editDelete, std::string("To Character...") + SC("^QT", "Alt+Q, T"), [this](void) { InjectChord(CTRL_Q, 't'); });

    mMenu.AddSubItem(edit, std::string("Mark Previo&us Block") + SC("^KU", "Alt+K, U"), [this](void) { InjectChord(CTRL_K, 'u'); });
    mMenu.AddSeparator(edit);
    mMenu.AddSubItem(edit, std::string("&Find...") + SC("^QF", "Ctrl+F"), [this](void) { InjectChord(CTRL_Q, 'f'); });
    mMenu.AddSubItem(edit, LB("Find and R&eplace...", "Find and &Replace...") + SC("^QA", "Ctrl+H"), [this](void) { InjectChord(CTRL_Q, 'a'); });
    mMenu.AddSubItem(edit, std::string("Find Ne&xt") + SC("^L", "F3"), [this](void) { InjectControl(CTRL_L); });
    mMenu.AddSubItem(edit, std::string("&Go to Character...") + SC("^QG", "Alt+Q, G"), [this](void) { InjectChord(CTRL_Q, 'G'); });
    mMenu.AddSubItem(edit, std::string("Goto Pa&ge...") + SC("^QI", "Ctrl+G"), [this](void) { InjectChord(CTRL_Q, 'I'); });

    int editGotoMarker = mMenu.AddSubMenu(edit, "Go &to Marker");
    for (char digit = '1'; digit <= '9'; ++digit)
    {
        std::string label = std::string("Marker ") + digit + SC(std::string("^Q") + digit, std::string("Alt+Q, ") + digit);
        mMenu.AddSubMenuItem(editGotoMarker, label, [this, digit](void) { InjectChord(CTRL_Q, digit); });
    }
    mMenu.AddSubMenuItem(editGotoMarker, std::string("Marker 0") + SC("^Q0", "Alt+Q, 0"), [this](void) { InjectChord(CTRL_Q, '0'); });

    int editGotoOther = mMenu.AddSubMenu(edit, "Go to &Other");
    mMenu.AddSubMenuItem(editGotoOther, std::string("Font Tag") + SC("^Q=", "Alt+Q, ="), [this](void) { InjectChord(CTRL_Q, '='); });
    mMenu.AddSubMenuItem(editGotoOther, std::string("Style Tag") + SC("^Q<", ""), none, false);
    mMenu.AddSubMenuItem(editGotoOther, std::string("Note...") + SC("^ONG", ""), none, false);
    mMenu.AddSubMenuItem(editGotoOther, std::string("Previous Position") + SC("^QP", "Alt+Q, P"), [this](void) { InjectChord(CTRL_Q, 'p'); });
    mMenu.AddSubMenuItem(editGotoOther, std::string("Last Find/Replace") + SC("^QV", "Alt+Q, V"), [this](void) { InjectChord(CTRL_Q, 'v'); });
    mMenu.AddSubMenuItem(editGotoOther, std::string("Beginning of Block") + SC("^QB", "Alt+Q, B"), [this](void) { InjectChord(CTRL_Q, 'b'); });
    mMenu.AddSubMenuItem(editGotoOther, std::string("End of Block") + SC("^QK", "Alt+Q, K"), [this](void) { InjectChord(CTRL_Q, 'k'); });
    mMenu.AddSubMenuItem(editGotoOther, std::string("Document Beginning") + SC("^QR", "Ctrl+Home"), [this](void) { InjectChord(CTRL_Q, 'r'); });
    mMenu.AddSubMenuItem(editGotoOther, std::string("Document End") + SC("^QC", "Ctrl+End"), [this](void) { InjectChord(CTRL_Q, 'c'); });
    mMenu.AddSubMenuItem(editGotoOther, std::string("Scroll Up") + SC("^QW", ""), none, false);
    mMenu.AddSubMenuItem(editGotoOther, std::string("Scroll Down") + SC("^QZ", ""), none, false);

    int editSetMarker = mMenu.AddSubMenu(edit, "&Set Marker");
    for (char digit = '1'; digit <= '9'; ++digit)
    {
        std::string label = std::string("Marker ") + digit + SC(std::string("^K") + digit, std::string("Alt+K, ") + digit);
        mMenu.AddSubMenuItem(editSetMarker, label, [this, digit](void) { InjectChord(CTRL_K, digit); });
    }
    mMenu.AddSubMenuItem(editSetMarker, std::string("Marker 0") + SC("^K0", "Alt+K, 0"), [this](void) { InjectChord(CTRL_K, '0'); });

    mMenu.AddSubItem(edit, std::string("Edit &Note") + SC("^OND", ""), none, false);

    int editNoteOpt = mMenu.AddSubMenu(edit, "Note Optio&ns");
    mMenu.AddSubMenuItem(editNoteOpt, "Starting Number...", none, false);
    mMenu.AddSubMenuItem(editNoteOpt, std::string("Convert Note...") + SC("^ONV", ""), none, false);
    mMenu.AddSubMenuItem(editNoteOpt, std::string("Convert at Print...") + SC(".cv", ""), none, false);
    mMenu.AddSubMenuItem(editNoteOpt, std::string("Endnote Location") + SC(".pe", ""), none, false);

    int editSettings = mMenu.AddSubMenu(edit, "Editing Se&ttings");
    mMenu.AddSubMenuItem(editSettings, std::string("Column Block Mode") + SC("^KN", ""), none, false);
    mMenu.AddSubMenuItem(editSettings, std::string("Column Replace") + SC("^KI", ""), none, false);
    mMenu.AddSubMenuItem(editSettings, std::string("Auto Align") + SC("^OA", ""), none, false);

    // ------------------------------- View -------------------------------
    int view = mMenu.AddMenu("View", 'V');
    mMenu.AddSubItem(view, std::string("Print Pre&view") + SC("^OP", ""), [this](void) { mEditor->PrintPreview(); });
    mMenu.AddSeparator(view);
    mMenu.AddSubItem(view, LB("&Command Tags", "Show &Formatting") + SC("^OD", "Alt+O, D"), [this](void) { InjectChord(CTRL_O, 'd'); });
    mMenu.AddSubItem(view, std::string("Block Hig&hlighting") + SC("^KH", "Alt+K, H"), [this](void) { InjectChord(CTRL_K, 'H'); });
    mMenu.AddSeparator(view);
    mMenu.AddSubItem(view, std::string("Sc&reen Settings...") + SC("^OB", ""), [this](void) { OpenPreferences(); });
    mMenu.AddSeparator(view);
    mMenu.AddSubItem(view, std::string("Switch Modes") + SC("^OT", "Alt+O, T"), none, false);

    // ------------------------------ Insert ------------------------------
    int insert = mMenu.AddMenu("Insert", 'I');
    mMenu.AddSubItem(insert, std::string("&Page Break") + SC(".pa", ""), [this](void) { mEditor->InsertText(".pa\r"); });
    mMenu.AddSubItem(insert, std::string("&Column Break") + SC(".cb", ""), none, false);
    mMenu.AddSeparator(insert);
    mMenu.AddSubItem(insert, std::string("&Today's Date") + SC("^M@", ""), [this](void) { InjectChord(CTRL_M, '@'); });

    int insertOther = mMenu.AddSubMenu(insert, "&Other Value");
    mMenu.AddSubMenuItem(insertOther, std::string("Current Time") + SC("^M!", ""), [this](void) { InjectChord(CTRL_M, '!'); });
    mMenu.AddSubMenuItem(insertOther, std::string("Last Math Result") + SC("^M=", ""), none, false);
    mMenu.AddSubMenuItem(insertOther, std::string("Last Math Expr.") + SC("^M#", ""), none, false);
    mMenu.AddSubMenuItem(insertOther, std::string("Last Math Dollar") + SC("^M$", ""), none, false);
    mMenu.AddSubMenuItem(insertOther, std::string("Current Filename") + SC("^M*", ""), [this](void) { InjectChord(CTRL_M, '*'); });
    mMenu.AddSubMenuItem(insertOther, std::string("Current Drive") + SC("^M:", ""), [this](void) { InjectChord(CTRL_M, ':'); });
    mMenu.AddSubMenuItem(insertOther, std::string("Current Directory") + SC("^M.", ""), [this](void) { InjectChord(CTRL_M, '.'); });
    mMenu.AddSubMenuItem(insertOther, std::string("Current Path") + SC("^M\\", ""), [this](void) { InjectChord(CTRL_M, '\\'); });

    int insertVar = mMenu.AddSubMenu(insert, "&Variable");
    mMenu.AddSubMenuItem(insertVar, "Date", [this](void) { mEditor->InsertText("&@&"); });
    mMenu.AddSubMenuItem(insertVar, "Time", [this](void) { mEditor->InsertText("&!&"); });
    mMenu.AddSubMenuItem(insertVar, "Page", [this](void) { mEditor->InsertText("&#&"); });
    mMenu.AddSubMenuItem(insertVar, "Line", [this](void) { mEditor->InsertText("&_&"); });
    mMenu.AddSubMenuItem(insertVar, "Filename", [this](void) { mEditor->InsertText("&*&"); });
    mMenu.AddSubMenuItem(insertVar, "Drive", [this](void) { mEditor->InsertText("&:&"); });
    mMenu.AddSubMenuItem(insertVar, "Directory", [this](void) { mEditor->InsertText("&.&"); });
    mMenu.AddSubMenuItem(insertVar, "Path", [this](void) { mEditor->InsertText("&\\&"); });
    mMenu.AddSubMenuItem(insertVar, "Word Count", [this](void) { mEditor->InsertText("&?&"); });

    mMenu.AddSubItem(insert, std::string("&Extended Char...") + SC("^PO", ""), none, false);
    mMenu.AddSeparator(insert);
    mMenu.AddSubItem(insert, std::string("&File...") + SC("^KR", ""), [this](void) { InjectChord(CTRL_K, 'r'); });
    mMenu.AddSubItem(insert, std::string("File at Print Time") + SC(".fi", ""), none, false);
    mMenu.AddSubItem(insert, std::string("&Graphic...") + SC("^P*", ""), none, false);

    int insertNote = mMenu.AddSubMenu(insert, "&Note");
    mMenu.AddSubMenuItem(insertNote, std::string("Comment...") + SC("^ONC", ""), none, false);
    mMenu.AddSubMenuItem(insertNote, std::string("Footnote...") + SC("^ONF", ""), none, false);
    mMenu.AddSubMenuItem(insertNote, std::string("Endnote...") + SC("^ONE", ""), none, false);
    mMenu.AddSubMenuItem(insertNote, std::string("Annotation") + SC("^ONA", ""), none, false);

    mMenu.AddSeparator(insert);
    int insertIndex = mMenu.AddSubMenu(insert, "&Index/TOC Entry");
    mMenu.AddSubMenuItem(insertIndex, std::string("TOC Entry...") + SC(".tc", ""), [this](void) { InsertTOCEntry(); });
    mMenu.AddSubMenuItem(insertIndex, std::string("Index Entry...") + SC("^ONI", ""), [this](void) { InsertIndexEntry(); });
    mMenu.AddSubMenuItem(insertIndex, std::string("Mark for Index") + SC("^PK", ""), none, false);
    mMenu.AddSubMenuItem(insertIndex, std::string("Dot Leader to Tab") + SC("^P.", ""), none, false);

    mMenu.AddSubItem(insert, std::string("Outline Number...") + SC("^OZ", ""), none, false);

    // ------------------------------- Style ------------------------------
    int style = mMenu.AddMenu("Style", 'S');
    mMenu.AddSubItem(style, std::string("&Bold") + SC("^PB", "Ctrl+B"), [this](void) { InjectChord(CTRL_P, 'b'); });
    mMenu.AddSubItem(style, std::string("&Italic") + SC("^PY", "Ctrl+I"), [this](void) { InjectChord(CTRL_P, 'y'); });
    mMenu.AddSubItem(style, std::string("&Underline") + SC("^PS", "Ctrl+U"), [this](void) { InjectChord(CTRL_P, 's'); });
    mMenu.AddSubItem(style, std::string("&Font...") + SC("^P=", "Ctrl+D"), [this](void) { InjectChord(CTRL_P, '='); });

    int styleOther = mMenu.AddSubMenu(style, "&Other");
    mMenu.AddSubMenuItem(styleOther, std::string("Strikeout") + SC("^PX", "Alt+P, X"), [this](void) { InjectChord(CTRL_P, 'x'); });
    mMenu.AddSubMenuItem(styleOther, std::string("Subscript") + SC("^PV", "Alt+P, V"), [this](void) { InjectChord(CTRL_P, 'v'); });
    mMenu.AddSubMenuItem(styleOther, std::string("Superscript") + SC("^PT", "Alt+P, T"), [this](void) { InjectChord(CTRL_P, 't'); });
    mMenu.AddSubMenuItem(styleOther, std::string("Doublestrike") + SC("^PD", ""), none, false);
    mMenu.AddSubMenuItem(styleOther, std::string("Color...") + SC("^P-", "Alt+P, -"), [this](void) { InjectChord(CTRL_P, '-'); });

    mMenu.AddSeparator(style);
    mMenu.AddSubItem(style, std::string("Select Para Style") + SC("^OFS", ""), none, false);
    mMenu.AddSubItem(style, std::string("Previous Style") + SC("^OFP", ""), none, false);
    mMenu.AddSubItem(style, std::string("Define Para Style") + SC("^OFD", ""), none, false);

    int styleManage = mMenu.AddSubMenu(style, "&Manage Paragraph Styles");
    mMenu.AddSubMenuItem(styleManage, std::string("Copy to Library") + SC("^OFO", ""), none, false);
    mMenu.AddSubMenuItem(styleManage, std::string("Delete Library") + SC("^OFY", ""), none, false);
    mMenu.AddSubMenuItem(styleManage, std::string("Rename Library") + SC("^OFR", ""), none, false);
    mMenu.AddSubMenuItem(styleManage, std::string("Rename Document") + SC("^OFE", ""), none, false);

    int styleCase = mMenu.AddSubMenu(style, "Convert &Case");
    mMenu.AddSubMenuItem(styleCase, std::string("Uppercase") + SC("^K\"", "Alt+K, \""), [this](void) { InjectChord(CTRL_K, '"'); });
    mMenu.AddSubMenuItem(styleCase, std::string("Lowercase") + SC("^K'", "Alt+K, '"), [this](void) { InjectChord(CTRL_K, '\''); });
    mMenu.AddSubMenuItem(styleCase, std::string("Sentence Case") + SC("^K.", "Alt+K, ."), [this](void) { InjectChord(CTRL_K, '.'); });

    mMenu.AddSeparator(style);
    mMenu.AddSubItem(style, "Settings", none, false);

    // ------------------------------ Layout ------------------------------
    int layout = mMenu.AddMenu("Layout", 'L');
    mMenu.AddSubItem(layout, LB("&Center Line", "&Center Paragraph") + SC("^OC", "Alt+O, C"), [this](void) { InjectChord(CTRL_O, 'c'); });
    mMenu.AddSubItem(layout, LB("R&ight Align Line", "R&ight Align Paragraph") + SC("^OJ", "Alt+O, ]"), [this](void) { InjectChord(CTRL_O, ']'); });
    mMenu.AddSeparator(layout);
    mMenu.AddSubItem(layout, std::string("&Left Align Para") + SC("^O<", "Ctrl+L"), [this](void) { InjectChord(CTRL_O, '<'); });
    mMenu.AddSubItem(layout, std::string("Center &Paragraph") + SC("^O=", "Ctrl+E"), [this](void) { InjectChord(CTRL_O, '='); });
    mMenu.AddSubItem(layout, std::string("Right Align Para") + SC("^O>", "Ctrl+R"), [this](void) { InjectChord(CTRL_O, '>'); });
    mMenu.AddSubItem(layout, std::string("&Justify Paragraph") + SC("^O+", "Ctrl+J"), [this](void) { InjectChord(CTRL_O, '+'); });
    mMenu.AddSeparator(layout);
    mMenu.AddSubItem(layout, std::string("Ruler Line...") + SC("^OL", ""), none, false);
    mMenu.AddSubItem(layout, std::string("Columns...") + SC("^OU", ""), none, false);
    mMenu.AddSubItem(layout, std::string("Pa&ge...") + SC("^OY", "Alt+O, Y"), [this](void) { InjectChord(CTRL_O, 'Y'); });

    int layoutHF = mMenu.AddSubMenu(layout, "&Headers/Footers");
    mMenu.AddSubMenuItem(layoutHF, std::string("Header...") + SC(".he", ""), none, false);
    mMenu.AddSubMenuItem(layoutHF, std::string("Footer...") + SC(".fo", ""), none, false);

    mMenu.AddSubItem(layout, std::string("Page Numbering...") + SC("^O#", ""), none, false);
    mMenu.AddSubItem(layout, std::string("Line Numbering...") + SC(".l#", ""), none, false);
    mMenu.AddSubItem(layout, std::string("Alignment/Spacing") + SC("^OS", ""), none, false);

    int layoutSpecial = mMenu.AddSubMenu(layout, "Special &Effects");
    mMenu.AddSubMenuItem(layoutSpecial, std::string("Overprint Char") + SC("^PH", ""), none, false);
    mMenu.AddSubMenuItem(layoutSpecial, std::string("Overprint Line") + SC("^P Enter", ""), none, false);
    mMenu.AddSubMenuItem(layoutSpecial, std::string("Option Hyphen") + SC("^OE", ""), none, false);
    mMenu.AddSubMenuItem(layoutSpecial, std::string("Vertically Center") + SC("^OV", ""), none, false);
    mMenu.AddSubMenuItem(layoutSpecial, std::string("Keep Word Together") + SC("^PO", ""), none, false);
    mMenu.AddSubMenuItem(layoutSpecial, std::string("Keep Lines/Page") + SC(".cp", ""), none, false);
    mMenu.AddSubMenuItem(layoutSpecial, std::string("Keep Lines/Column") + SC(".cc", ""), none, false);

    // ----------------------------- Utilities ----------------------------
    int util = mMenu.AddMenu("Utilities", 'U');
    mMenu.AddSubItem(util, std::string("&Spell Check Global") + SC("^QL", ""), [this](void) { mEditor->SpellCheckDocument(); });

    int utilSpell = mMenu.AddSubMenu(util, "Spell Check Ot&her");
    mMenu.AddSubMenuItem(utilSpell, std::string("Rest of Document") + SC("^QL", ""), [this](void) { mEditor->SpellCheckDocument(); });
    mMenu.AddSubMenuItem(utilSpell, std::string("Word") + SC("^QN", ""), [this](void) { mEditor->SpellCheckWord(); });
    mMenu.AddSubMenuItem(utilSpell, std::string("Type Word...") + SC("^QO", ""), none, false);
    mMenu.AddSubMenuItem(utilSpell, std::string("Rest of Notes") + SC("^ONL", ""), none, false);

    mMenu.AddSubItem(util, std::string("&Thesaurus") + SC("^QJ", ""), none, false);
    mMenu.AddSubItem(util, std::string("&Language Change...") + SC(".la", ""), none, false);
    mMenu.AddSeparator(util);
    mMenu.AddSubItem(util, std::string("&Inset") + SC("^P&", ""), none, false);
    mMenu.AddSubItem(util, std::string("&Calculator") + SC("^QM", ""), none, false);
    mMenu.AddSubItem(util, std::string("Block &Math") + SC("^KM", ""), none, false);

    int utilSort = mMenu.AddSubMenu(util, "Sort &Block");
    mMenu.AddSubMenuItem(utilSort, std::string("Ascending") + SC("^KZA", ""), none, false);
    mMenu.AddSubMenuItem(utilSort, std::string("Descending") + SC("^KZD", ""), none, false);

    mMenu.AddSubItem(util, std::string("&Word Count") + SC("^K?", "Alt+K, ?"), [this](void) { InjectChord(CTRL_K, '?'); });
    mMenu.AddSeparator(util);

    int utilMacros = mMenu.AddSubMenu(util, "Macr&os");
    mMenu.AddSubMenuItem(utilMacros, std::string("Play...") + SC("^MP", ""), none, false);
    mMenu.AddSubMenuItem(utilMacros, std::string("Record...") + SC("^MR", ""), none, false);
    mMenu.AddSubMenuItem(utilMacros, std::string("Edit/Create...") + SC("^MD", ""), none, false);
    mMenu.AddSubMenuItem(utilMacros, std::string("Single Step...") + SC("^MS", ""), none, false);
    mMenu.AddSubMenuItem(utilMacros, std::string("Copy...") + SC("^MO", ""), none, false);
    mMenu.AddSubMenuItem(utilMacros, std::string("Delete...") + SC("^MY", ""), none, false);
    mMenu.AddSubMenuItem(utilMacros, std::string("Rename...") + SC("^ME", ""), none, false);

    int utilMerge = mMenu.AddSubMenu(util, "Merge &Print Commands");
    mMenu.AddSubMenuItem(utilMerge, std::string("Data File...") + SC(".df", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Name Variables") + SC(".rv", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Set Variable") + SC(".sv", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Set Var to Math") + SC(".ma", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Ask for Variable") + SC(".av", ""), none, false);
    mMenu.AddSubMenuSeparator(utilMerge);
    mMenu.AddSubMenuItem(utilMerge, std::string("If...") + SC(".if", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Else") + SC(".el", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("End If") + SC(".ei", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Go to Top") + SC(".go t", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Go to Bottom") + SC(".go b", ""), none, false);
    mMenu.AddSubMenuSeparator(utilMerge);
    mMenu.AddSubMenuItem(utilMerge, std::string("Clear Screen...") + SC(".cs", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Display Message") + SC(".dm", ""), none, false);
    mMenu.AddSubMenuItem(utilMerge, std::string("Print n Times...") + SC(".rp", ""), none, false);

    int utilReformat = mMenu.AddSubMenu(util, "&Reformat");
    mMenu.AddSubMenuItem(utilReformat, std::string("Rest of Document") + SC("^QU", "Alt+Q, U"), [this](void) { InjectChord(CTRL_Q, 'u'); });
    mMenu.AddSubMenuItem(utilReformat, std::string("Paragraph") + SC("^B", ""), none, false);
    mMenu.AddSubMenuItem(utilReformat, std::string("Rest of Notes") + SC("^ONU", ""), none, false);

    mMenu.AddSubItem(util, std::string("Repeat Keystroke") + SC("^QQ", ""), none, false);
    mMenu.AddSeparator(util);
    mMenu.AddSubItem(util, std::string("System Preferences"), [this](void) { OpenSystemPreferences(); });

    // ------------------------------- Help -------------------------------
    int help = mMenu.AddMenu("Help", 'H');
    mMenu.AddSubItem(help, "&About WordTsar", [this](void) { ShowAboutWordTsar(); });
}

// =========================================================================
// Layout
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int rows [in] new terminal rows
/// @param  int cols [in] new terminal cols
///
/// @return nothing
///
/// @brief
/// Recompute the chrome layout for a new terminal size.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::OnResize(int rows, int cols)
{
    mRows = rows;
    mCols = cols;
    LayoutChrome();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Position the menu bar for the current terminal size.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::LayoutChrome(void)
{
    sRect menuRect;
    menuRect.row = 1;              // row 0 is the title bar
    menuRect.col = 0;
    menuRect.rows = 1;
    menuRect.cols = mCols;
    mMenu.SetBounds(menuRect);

    mEditor->SetTerminalSize(mRows, mCols);
}

// =========================================================================
// Help panel
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @return true when the help panel should be shown for the current mode
///
/// @brief
/// The main edit menu (HELP_MAIN) shows immediately; the context menus
/// (^K/^Q/^O/^P/^M) appear ~1 second after the chord,
/// frontend. HELP_NONE/HELP_UNKNOWN never show.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSWordTsar::IsHelpVisible(void)
{
    eHelpDisplay mode = mEditor->GetHelpDisplay();

    if (mode != mLastHelpMode)
    {
        mLastHelpMode = mode;
        mHelpModeChangedAt = std::chrono::steady_clock::now();
    }

    if ((mode == HELP_NONE) || (mode == HELP_UNKNOWN))
    {
        return false;
    }

    if (mode == HELP_MAIN)
    {
        return true;
    }

    // mode is one of the HELP_CTRLx submenus here. Per the WS7 manual's
    // "Change Help Level", submenus only appear at levels 2-3 -- level 4
    // relies on the pull-down bar instead.
    if (mEditor->mHelpLevel < 2 || mEditor->mHelpLevel > 3)
    {
        return false;
    }

    std::chrono::steady_clock::duration elapsed = std::chrono::steady_clock::now() - mHelpModeChangedAt;
    return elapsed >= std::chrono::seconds(1);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return the number of rows the help panel currently needs (0 if hidden)
///
/// @brief
/// Compute the help-panel height for the editor's current help mode, capped so
/// the editor keeps at least a few rows.
///
/////////////////////////////////////////////////////////////////////////////
int cWSWordTsar::HelpPanelRows(void)
{
    if (IsHelpVisible() == false)
    {
        return 0;
    }

    eHelpDisplay mode = mEditor->GetHelpDisplay();
    std::string text = GetHelpText(mode);
    if (text.empty() == true)
    {
        return 0;
    }

    int lines = 1;
    for (char ch : text)
    {
        if (ch == '\n')
        {
            lines++;
        }
    }

    // Leave room for the fixed chrome (title + menu + top status + ruler +
    // bottom status = 5 rows) and at least two editor rows. The help
    // menus are up to 16 lines, so the editor shrinks to show the full panel.
    int maxRows = mRows - 5 - 2;
    if (maxRows < 0)
    {
        maxRows = 0;
    }
    if (lines > maxRows)
    {
        lines = maxRows;
    }

    return lines;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  int startRow [in] first row of the panel
/// @param  int rows [in] panel height
///
/// @return nothing
///
/// @brief
/// Draw the WordStar help/command panel for the current help mode.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawHelpPanel(cScreen& screen, int startRow, int rows)
{
    if (rows <= 0)
    {
        return;
    }

    // Help panel colors: keystroke labels (wrapped
    // in \x01 markers in the help text) are bright and bold; the rest is plain.
    sStyle normal;
    normal.fg = mHelpFg;
    normal.bg = mHelpBg;
    normal.attrs = wordstartui::CELL_ATTR_NONE;

    sStyle key;
    key.fg = mHelpKeyFg;
    key.bg = mHelpKeyBg;
    key.attrs = wordstartui::CELL_ATTR_BOLD;

    sRect area;
    area.row = startRow;
    area.col = 0;
    area.rows = rows;
    area.cols = mCols;
    screen.FillRect(area, " ", normal);

    std::string text = GetHelpText(mEditor->GetHelpDisplay());
    int row = startRow;
    size_t pos = 0;

    while ((pos <= text.size()) && (row < (startRow + rows)))
    {
        size_t nl = text.find('\n', pos);
        std::string line;
        if (nl == std::string::npos)
        {
            line = text.substr(pos);
            pos = text.size() + 1;
        }
        else
        {
            line = text.substr(pos, nl - pos);
            pos = nl + 1;
        }

        bool highlight = false;
        int col = 0;
        for (size_t index = 0; (index < line.size()) && (col < mCols); ++index)
        {
            char ch = line[index];
            if (ch == '\001')
            {
                highlight = (highlight == false);
                continue;
            }

            sStyle style = normal;
            if (highlight == true)
            {
                style = key;
            }
            screen.PutCell(row, col, std::string(1, ch), style);
            col++;
        }

        row++;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eHelpDisplay help [in] the help mode
///
/// @return the help-panel text for that mode
///
/// @brief
/// The WordStar command-menu text shown for each control-key prefix.
///
/////////////////////////////////////////////////////////////////////////////
std::string cWSWordTsar::GetHelpText(eHelpDisplay help) const
{
    switch (help)
    {
        case HELP_MAIN:
        {
            return "                        ----- E D I T   M E N U -----\n"
                   "  CURSOR      SCROLL        DELETE      OTHER                 MENUS\n"
                   " \001^E\001 up       \001^W\001 up         \001^G\001 char    \001F1\001 help                \001^O\001 onscreen format\n"
                   " \001^X\001 down     \001^Z\001 down       \001^T\001 word    \001^I\001 tab                 \001^K\001 block & save\n"
                   " \001^S\001 left     \001^R\001 up screen  \001^Y\001 line    \001^V\001 turn insert off     \001^M\001 macros\n"
                   " \001^D\001 right    \001^C\001 down      \001Del\001 char    \001^L\001 find/replace again  \001^P\001 print controls\n"
                   " \001^A\001 word left   screen     \001^U\001 unerase                        \001^Q\001 quick functions\n"
                   " \001^F\001 word right\n"
                   "                                     \001F10\001 Menu";
        }

        case HELP_CTRLM:
        {
            return "                  ----- MACRO MENU -----\n"
                   "     MACRO FUNCTIONS                   INSERT\n"
                   " \001P\001 play          \001E\001 rename        \001@\001 today's date         \001*\001 current filename\n"
                   " \001R\001 record        \001O\001 copy          \001!\001 current time         \001:\001 current drive\n"
                   " \001D\001 edit/create   \001Y\001 delete        \001=\001 last math result     \001.\001 current directory\n"
                   " \001S\001 single step                   \001#\001 last math expression \001\\\001 current path\n"
                   "                                 \001$\001 last math as dollar";
        }

        case HELP_CTRLK:
        {
            return "                  ----- B L O C K   &   S A V E   M E N U -----\n"
                   "    SAVE                     BLOCK                       WINDOW\n"
                   "  \001D\001 save                   \001B\001 begin block               \001A\001 copy between\n"
                   "  \001T\001 save as                \001K\001 end block                 \001G\001 move between\n"
                   "  \001S\001 save and resume        \001C\001 copy                      \n"
                   "  \001X\001 save and exit          \001V\001 move                        CASE\n"
                   "  \001Q\001 abandon changes        \001Y\001 delete                    \001\"\001 upper\n"
                   "    FILE                   \001W\001 write to disk             \001'\001 lower\n"
                   "  \001O\001 copy                   \001M\001 math                      \001.\001 sentence\n"
                   "  \001E\001 rename                 \001Z\001 sort                      \n"
                   "  \001J\001 delete                 \001?\001 word count                  CURSOR\n"
                   "  \001P\001 print                  \001H\001 turn disp on/off        \0010-9\001 set marker\n"
                   "  \001\\\001 fax                    \001U\001 mark previous block       \n"
                   "  \001L\001 change drive/dir       \001<\001 unmark block                SYSTEM CLIPBOARD\n"
                   "  \001R\001 insert a file          \001N\001 turn column mode on       \001[\001 copy from clipboard\n"
                   "  \001F\001 run command            \001I\001 turn column replace on    \001]\001 copy to clipboard";
        }

        case HELP_CTRLP:
        {
            return "              ----- P R I N T   C O N T R O L S   M E N U -----\n"
                   "            BEGIN & END                                OTHER \n"
                   "    \001B\001 bold         \001X\001 strike out         \001H\001 overprint char   \001O\001 binding space\n"
                   "    \001S\001 underline    \001D\001 double strike    \001RET\001 overprint line   \001C\001 print pause\n"
                   "    \001V\001 subscript    \001Y\001 italics            \001F\001 phantom space    \001I\001 8-column tab\n"
                   "    \001T\001 superscript  \001K\001 indexing           \001G\001 phantom rubout   \001.\001 dot leader\n"
                   "                                        \001*\001 graphics tag     \0010\001 extended characters\n"
                   "               STYLE                    \001&\001 start Inset\n"
                   "    \001=/+\001 select font \001N\001 Normal Font\n"
                   "    \001-\001 select color  \001A\001 alternate font    \001Q W E R !\001 custom    \001?\001 select printer";
        }

        case HELP_CTRLQ:
        {
            return "                      ----- Q U I C K   M E N U -----\n"
                   "            CURSOR              FIND            OTHER             SPELL\n"
                   " \001E\001 upper left   \001P\001 previous   \001F\001 find text     \001U\001 align rest doc  \001L\001 check document\n"
                   " \001X\001 lower right  \001V\001 prev find  \001A\001 find/replace  \001M\001 math  \001Q\001 repeat  \001N\001 check word\n"
                   " \001S\001 begin line   \001B\001 beg block  \001G\001 char forward  \001J\001 thesaurus       \001O\001 enter word\n"
                   " \001D\001 end line     \001K\001 end block  \001H\001 char back                         DELETE\n"
                   " \001R\001 beg doc    \0010-9\001 marker     \001I\001 page/line       SCROLL        \001Del\001 line to left\n"
                   " \001C\001 end doc                   \001=\001 next font     \001W\001 up, repeat      \001Y\001 line to right\n"
                   "                             \001<\001 next style    \001Z\001 dn, repeat      \001T\001 to character";
        }

        case HELP_CTRLO:
        {
            return "           ----- O N S C R E E N   F O R M A T   M E N U -----\n"
                   "   MARGINS & TABS            TYPING                         DISPLAY\n"
                   " \001L\001 left  \001R\001 right     \001C\001 center line                   \001P\001 page preview\n"
                   " \001G\001 temporary indent   \001]\001 right flush line              \001D\001 turn command tags off\n"
                   " \001X\001 release margin    \001V\001 vertically center             \001B\001 change screen settings\n"
                   " \001I\001 set/clear tabs    \001E\001 enter soft hyphen             \001K\001 open or switch window\n"
                   " \001O\001 ruler to text     \001H\001 turn auto-hyphenation off     \001M\001 size current window\n"
                   " \001U\001 column layout     \001J\001 turn justification on         \001?\001 status\n"
                   " \001Y\001 page layout       \001A\001 turn auto-align off           \001Z\001 paragraph number\n"
                   " \001F\001 paragraph styles  \001W\001 turn word wrap off            \001#\001 page numbering\n"
                   "                     --------\n"
                   "                     \001<\001 left align paragraph\n"
                   "                     \001=\001 center paragraph\n"
                   "                     \001>\001 right align paragraph\n"
                   "                     \001+\001 justify paragraph\n"
                   "                                                     \001T\001 center view\n"
                   " \001S\001 set line spacing \001RET\001 turn Enter closes dialog off \001N\001 notes";
        }

        default:
        {
            return "";
        }
    }
}

// =========================================================================
// Drawing
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the current mode: splash, opening menu, or the editor.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::Draw(cScreen& screen, const cTheme& theme)
{
    if (mMode == APP_SPLASH)
    {
        DrawSplash(screen);

        mSplashFrames++;
        if (mSplashFrames >= SPLASH_TICKS)
        {
            if (mHaveFileArg == true)
            {
                mMode = APP_EDITOR;
            }
            else
            {
                mMode = APP_OPENING;
            }
        }
        return;
    }

    if (mMode == APP_OPENING)
    {
        DrawOpeningMenu(screen);
        return;
    }

    DrawEditor(screen, theme);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
///
/// @return nothing
///
/// @brief
/// Draw the startup splash banner.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawSplash(cScreen& screen)
{
    sStyle back;
    back.fg = wordstartui::MakeRgb(204, 170, 0);
    back.bg = wordstartui::MakeRgb(0, 0, 0);
    back.attrs = wordstartui::CELL_ATTR_NONE;

    sRect full;
    full.row = 0;
    full.col = 0;
    full.rows = mRows;
    full.cols = mCols;
    screen.FillRect(full, " ", back);

    sStyle bold = back;
    bold.attrs = wordstartui::CELL_ATTR_BOLD;

    // Echoes the GUI splash's own cover-page layout (letter-spaced wordmark,
    // thin rule, "FOR MACOS" subtitle) rather than the old boxed "MM" logo --
    // a terminal has no way to show that cover's actual gear photo (no
    // inline-image protocol in a plain terminal), but the same spaced-caps
    // title treatment already used for the Opening Menu banner gets close.
    std::string wordmark = "W O R D T S A R";
    std::string subtitle = "F O R   M A C O S";

    std::string rule;
    for (size_t i = 0; i < wordmark.size(); ++i)
    {
        rule += "\xe2\x94\x80"; // U+2500 BOX DRAWINGS LIGHT HORIZONTAL
    }

    int titleRow = (mRows / 2) - 3;
    if (titleRow < 0)
    {
        titleRow = 0;
    }

    screen.PutText(titleRow, (mCols - static_cast<int>(wordmark.size())) / 2, wordmark, bold);
    screen.PutText(titleRow + 1, (mCols - static_cast<int>(wordmark.size())) / 2, rule, back);
    screen.PutText(titleRow + 3, (mCols - static_cast<int>(subtitle.size())) / 2, subtitle, back);

    std::string version = std::string(FULLVERSION_STRING) + " " + std::string(STATUS);
    screen.PutText(titleRow + 5, (mCols - static_cast<int>(version.size())) / 2, version, back);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
///
/// @return nothing
///
/// @brief
/// Draw the WordStar opening menu (two columns of choices).
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawOpeningMenu(cScreen& screen)
{
    // The opening screen uses the default terminal colors.
    sStyle base;
    base.fg = wordstartui::MakeDefaultColor();
    base.bg = wordstartui::MakeDefaultColor();
    base.attrs = wordstartui::CELL_ATTR_NONE;

    sStyle sel = base;
    sel.attrs = wordstartui::CELL_ATTR_INVERSE;

    // Gold, not blue -- pure blue reads poorly against a dark terminal
    // background (this screen paints no background of its own, so it
    // inherits whatever the user's terminal theme is). Reuses the same
    // accent as the splash screen's logo, just above, for consistency
    // between the two "welcome" screens.
    sStyle keyStyle = base;
    keyStyle.fg = wordstartui::MakeRgb(204, 170, 0);
    keyStyle.attrs = wordstartui::CELL_ATTR_BOLD;

    sStyle disabled = base;
    disabled.attrs = wordstartui::CELL_ATTR_DIM;

    sRect full;
    full.row = 0;
    full.col = 0;
    full.rows = mRows;
    full.cols = mCols;
    screen.FillRect(full, " ", base);

    auto separator = [&](int r)
    {
        sRect line;
        line.row = r;
        line.col = 0;
        line.rows = 1;
        line.cols = mCols;
        screen.FillRect(line, "\xe2\x94\x80", base);
    };

    // Title (row 0) + separator.
    std::string header = "O P E N I N G   M E N U";
    screen.PutText(0, (mCols - static_cast<int>(header.size())) / 2, header, base);
    separator(1);

    // Two-column menu: left column at column 2, right column near mid.
    // Item order in kOpeningItems is left-column-first (kOpeningLeftCount
    // rows), then right column -- rows are just the index within that run.
    int leftCol = 2;
    int rightCol = mCols / 2;
    if (rightCol > 40)
    {
        rightCol = 40;
    }
    int rightCount = kOpeningCount - kOpeningLeftCount;

    for (int index = 0; index < kOpeningCount; ++index)
    {
        const sOpeningItem& item = kOpeningItems[index];
        bool onLeft = (index < kOpeningLeftCount);
        int col = onLeft ? leftCol : rightCol;
        int row = 2 + (onLeft ? index : (index - kOpeningLeftCount));

        char keyChar = (item.key != 0) ? item.key : ' ';
        std::string label = std::string(1, keyChar) + " " + item.label;

        bool selected = (index == mOpeningSel) && (mOpeningBrowserFocus == false);

        if (selected == true)
        {
            std::string padded = "  " + label;
            while (static_cast<int>(padded.size()) < 36)
            {
                padded.push_back(' ');
            }
            screen.PutText(row, col - 2, padded, sel);
        }
        else
        {
            sStyle labelStyle = base;
            if (item.enabled == false)
            {
                labelStyle = disabled;
            }
            screen.PutText(row, col, label, labelStyle);
            if (item.key != 0)
            {
                screen.PutText(row, col, std::string(1, item.key), keyStyle);
            }
        }
    }

    // F1 (help) -- real WS7's own 20th opening-menu item, drawn below the
    // left column since it isn't part of kOpeningItems (it arrives as a
    // function-key event, not a typed character -- see HandleOpeningKey).
    {
        int row = 2 + kOpeningLeftCount;
        screen.PutText(row, leftCol, "F1 help", base);
        screen.PutText(row, leftCol, std::string("F1"), keyStyle);
    }

    // Separator, then the Filenames/Path header, then another separator.
    // Positioned below whichever column (including the F1 row) is taller.
    int menuRows = std::max(kOpeningLeftCount + 1, rightCount);
    int separatorRow = 2 + menuRows;
    separator(separatorRow);
    screen.PutText(separatorRow + 2, 0, "Filenames:       Path: " + mBrowser.GetCurrentDirectory(), base);
    separator(separatorRow + 3);

    // File browser fills the rest, starting at column 0.
    int browserTop = separatorRow + 4;
    sRect browserRect;
    browserRect.row = browserTop;
    browserRect.col = 0;
    browserRect.rows = mRows - browserTop;
    if (browserRect.rows < 1)
    {
        browserRect.rows = 1;
    }
    browserRect.cols = mCols;
    mBrowser.SetBounds(browserRect);
    mBrowser.Draw(screen, GetTheme(), mOpeningBrowserFocus);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int& offset      [out] column offset to center the pane, 0 if off
/// @param  int& contentCols [out] pane width in columns, mCols if off
///
/// @return nothing
///
/// @brief
/// See declaration in wordtsar.h.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::GetEditorHorizontalExtent(int& offset, int& contentCols) const
{
    offset = 0;
    contentCols = mCols;

    if (mEditor->IsCenterViewEnabled() == false)
    {
        return;
    }

    cLayoutBase* layout = mEditor->GetLayout();
    COORD_T twipsPerCol = mEditor->GetTwipsPerColumn();
    if ((layout == nullptr) || (twipsPerCol <= 0))
    {
        return;
    }

    COORD_T leftMargin = layout->GetLeftMargin();
    COORD_T rightMargin = layout->GetRightMargin();
    int cols = static_cast<int>((rightMargin - leftMargin) / twipsPerCol);
    if (cols < 10)
    {
        cols = 10;
    }
    if (cols > mCols)
    {
        cols = mCols;
    }

    contentCols = cols;
    offset = (mCols - cols) / 2;
    if (offset < 0)
    {
        offset = 0;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  const cTheme& theme [in] color theme
///
/// @return nothing
///
/// @brief
/// Draw the editor: menu bar, top status, help panel, editor area, and bottom
/// status. The editor area shrinks to make room for an active help panel.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawEditor(cScreen& screen, const cTheme& theme)
{
    if (mEditor->HasPendingLayout() == true)
    {
        mEditor->IdleLayout();
    }

    sStyle editorStyle;
    editorStyle.fg = mEditor->mTextColour;
    editorStyle.bg = mEditor->mBGroundColour;
    editorStyle.attrs = wordstartui::CELL_ATTR_NONE;
    screen.Clear(editorStyle);

    // Assign chrome rows top-down, honouring the visibility toggles.
    int row = 0;
    int titleRow = -1;
    int menuRow = -1;
    int topStatusRow = -1;
    int rulerRow = -1;

    if (mShowTitle == true)
    {
        titleRow = row++;
    }
    if (mShowMenu == true)
    {
        menuRow = row++;
    }
    if (mShowTopStatus == true)
    {
        topStatusRow = row++;
    }
    mTopStatusRow = topStatusRow;   // remembered for click hit-testing
    mBottomStatusRow = -1;          // set by DrawBottomStatus when it is shown

    int helpTop = row;
    int helpRows = HelpPanelRows();
    row += helpRows;

    if (mShowRuler == true)
    {
        rulerRow = row++;
    }

    int editorTop = row;
    int editorBottom = (mShowBottomStatus == true) ? (mRows - 2) : (mRows - 1);
    int editorRows = editorBottom - editorTop + 1;
    if (editorRows < 1)
    {
        editorRows = 1;
    }

    int centerOffset = 0;
    int centerCols = mCols;
    GetEditorHorizontalExtent(centerOffset, centerCols);

    sRect editorRect;
    editorRect.row = editorTop;
    editorRect.col = centerOffset;
    editorRect.rows = editorRows;
    editorRect.cols = centerCols;

    mEditor->SetChromeRows(mRows - editorRows);
    mView->SetBounds(editorRect);
    mView->Draw(screen);

    if (helpRows > 0)
    {
        DrawHelpPanel(screen, helpTop, helpRows);
    }

    if (rulerRow >= 0)
    {
        DrawRuler(screen, rulerRow);
    }
    if (titleRow >= 0)
    {
        DrawTitleBar(screen, titleRow);
    }
    if (topStatusRow >= 0)
    {
        DrawTopStatus(screen, topStatusRow);
    }
    if (mShowBottomStatus == true)
    {
        DrawBottomStatus(screen);
    }
    if (menuRow >= 0)
    {
        sRect menuRect;
        menuRect.row = menuRow;
        menuRect.col = 0;
        menuRect.rows = 1;
        menuRect.cols = mCols;
        mMenu.SetBounds(menuRect);
        mMenu.Draw(screen, theme);

        // Right-aligned "F10=Menu" hint.
        std::string hint = "F10=Menu";
        int hintCol = mCols - static_cast<int>(hint.size()) - 1;
        if (hintCol > 0)
        {
            screen.PutText(menuRow, hintCol, hint, theme.GetStyle(wordstartui::THEME_ROLE_MENU));
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
///
/// @return nothing
///
/// @brief
/// Draw the title bar: "filename - WordTsar <version> <status>", centred.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawTitleBar(cScreen& screen, int row)
{
    sStyle style;
    style.fg = mTitleFg;
    style.bg = mTitleBg;
    style.attrs = wordstartui::CELL_ATTR_BOLD;

    sRect bar;
    bar.row = row;
    bar.col = 0;
    bar.rows = 1;
    bar.cols = mCols;
    screen.FillRect(bar, " ", style);

    std::string name = mFilename;
    size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos)
    {
        name = name.substr(slash + 1);
    }
    if (name.empty() == true)
    {
        name = "untitled";
    }

    std::string title = name + " - WordTsar " + std::string(FULLVERSION_STRING) + " " + std::string(STATUS);
    int col = (mCols - static_cast<int>(title.size())) / 2;
    if (col < 0)
    {
        col = 0;
    }
    screen.PutText(row, col, title, style);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
///
/// @return nothing
///
/// @brief
/// Draw the top style bar: paragraph style, font, bold/italic/underline
/// indicators, change flag, and paragraph alignment.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawTopStatus(cScreen& screen, int row)
{
    sStatus status;
    mEditor->GetStatus(status);

    sStyle base;
    base.fg = mStatusFg;
    base.bg = mStatusBg;
    base.attrs = wordstartui::CELL_ATTR_NONE;

    sStyle bold = base;
    bold.attrs = wordstartui::CELL_ATTR_BOLD;

    sStyle dim = base;
    dim.attrs = wordstartui::CELL_ATTR_DIM;

    sRect bar;
    bar.row = row;
    bar.col = 0;
    bar.rows = 1;
    bar.cols = mCols;
    screen.FillRect(bar, " ", base);

    auto styleFor = [&](bool active) -> sStyle
    {
        if (active == true)
        {
            return bold;
        }
        return dim;
    };

    const std::string vbar = "\xe2\x94\x82";   // box vertical

    // Right-anchored indicator block: "│ B  I  U │ * │ L  C  R  J".
    int blockStart = mCols - 27;
    if (blockStart < 20)
    {
        blockStart = 20;
    }

    // "*" lights when command tags (formatting marks) are shown, matching the
    // GUI's Command Tags indicator.
    std::string flag = " ";
    if (status.showcontrol == true)
    {
        flag = "*";
    }

    screen.PutText(row, blockStart + 0, vbar, base);
    screen.PutText(row, blockStart + 2, "B", styleFor(status.bold));
    screen.PutText(row, blockStart + 5, "I", styleFor(status.italic));
    screen.PutText(row, blockStart + 8, "U", styleFor(status.underline));
    screen.PutText(row, blockStart + 10, vbar, base);
    screen.PutText(row, blockStart + 12, flag, bold);
    screen.PutText(row, blockStart + 14, vbar, base);
    screen.PutText(row, blockStart + 16, "L", styleFor(status.just == JUST_LEFT));
    screen.PutText(row, blockStart + 19, "C", styleFor(status.just == JUST_CENTER));
    screen.PutText(row, blockStart + 22, "R", styleFor(status.just == JUST_RIGHT));
    screen.PutText(row, blockStart + 25, "J", styleFor(status.just == JUST_JUST));

    // Left divider and centred font/style name.
    screen.PutText(row, 2, vbar, base);

    std::string label = status.font;
    if (status.style.empty() == false)
    {
        label = status.style + "  " + status.font;
    }
    int fieldStart = 4;
    int fieldWidth = blockStart - fieldStart - 1;
    if (fieldWidth < 1)
    {
        fieldWidth = 1;
    }
    int labelCol = fieldStart + ((fieldWidth - static_cast<int>(label.size())) / 2);
    if (labelCol < fieldStart)
    {
        labelCol = fieldStart;
    }
    screen.PutText(row, labelCol, label, base);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int col [in] clicked column on the top status row
///
/// @return true if a clickable region was hit
///
/// @brief
/// Dispatch a left click on the top status bar. The column layout mirrors
/// DrawTopStatus: the centred font/style label opens the font selector, the
/// B/I/U glyphs toggle bold/italic/underline, "*" toggles command tags, and
/// L/C/R/J set paragraph alignment. All actions run through the shared command
/// pipeline (InjectChord) so they behave exactly like the menu items.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSWordTsar::HandleTopStatusClick(int col)
{
    int blockStart = mCols - 27;
    if (blockStart < 20)
    {
        blockStart = 20;
    }

    // Single-glyph indicators: accept the glyph column and the space after it.
    auto hit = [col](int c) -> bool
    {
        return (col >= c) && (col <= c + 1);
    };

    if (hit(blockStart + 2) == true)            // Bold
    {
        InjectChord(CTRL_P, 'b');
        return true;
    }
    if (hit(blockStart + 5) == true)            // Italic
    {
        InjectChord(CTRL_P, 'y');
        return true;
    }
    if (hit(blockStart + 8) == true)            // Underline
    {
        InjectChord(CTRL_P, 's');
        return true;
    }
    if (hit(blockStart + 12) == true)           // Command tags
    {
        InjectChord(CTRL_O, 'd');
        return true;
    }

    // Alignment: mirror the GUI status bar exactly. SetAlignment inserts the .oj
    // dot command at the start of the current paragraph (not at the caret, and
    // without the ^O bracket/toggle behaviour), then PerformPostCommandUpdate
    // re-lays out so the command takes effect -- the GUI eventFilter does the
    // same after each status-bar action.
    bool aligned = true;
    eJustification just = JUST_LEFT;
    if (hit(blockStart + 16) == true)           // Left
    {
        just = JUST_LEFT;
    }
    else if (hit(blockStart + 19) == true)      // Center
    {
        just = JUST_CENTER;
    }
    else if (hit(blockStart + 22) == true)      // Right
    {
        just = JUST_RIGHT;
    }
    else if (hit(blockStart + 25) == true)      // Justify
    {
        just = JUST_JUST;
    }
    else
    {
        aligned = false;
    }
    if (aligned == true)
    {
        mEditor->SetAlignment(just);
        mEditor->PerformPostCommandUpdate();
        return true;
    }

    // Centred font/style field: the style name sits on the left, the font name
    // on the right (matching the GUI's Style + Font status widgets). The font
    // name opens the font dialog; the style region opens the character-style
    // picker.
    sStatus status;
    mEditor->GetStatus(status);
    std::string label = status.font;
    if (status.style.empty() == false)
    {
        label = status.style + "  " + status.font;
    }
    int fieldStart = 4;
    int fieldWidth = blockStart - fieldStart - 1;
    if (fieldWidth < 1)
    {
        fieldWidth = 1;
    }
    int labelCol = fieldStart + ((fieldWidth - static_cast<int>(label.size())) / 2);
    if (labelCol < fieldStart)
    {
        labelCol = fieldStart;
    }

    int fontStart = labelCol;
    if (status.style.empty() == false)
    {
        fontStart = labelCol + static_cast<int>(status.style.size()) + 2;
    }
    int fontEnd = fontStart + static_cast<int>(status.font.size());

    if ((col >= fontStart) && (col < fontEnd))
    {
        InjectChord(CTRL_P, '=');   // font selector (^P=)
        return true;
    }
    if ((col >= fieldStart) && (col < fontStart))
    {
        SelectStyle();              // character-style selector
        return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int col [in] clicked column on the bottom status row
///
/// @return true if a clickable field was hit
///
/// @brief
/// Dispatch a left click on the bottom status bar, mirroring the GUI: clicking
/// the Insert/Overwrite mode field toggles the mode; clicking the page field
/// opens Go To Page. The field ranges are recorded by DrawBottomStatus.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSWordTsar::HandleBottomStatusClick(int col)
{
    if ((col >= mModeClickStart) && (col < mModeClickEnd))
    {
        mEditor->ToggleInsertOverwrite();
        mEditor->PerformPostCommandUpdate();
        return true;
    }
    if ((col >= mPageClickStart) && (col < mPageClickEnd))
    {
        InjectChord(CTRL_Q, 'I');   // Go To Page (^QI)
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Present the character-style selector and apply the chosen style through the
/// shared command pipeline, so it behaves exactly like the Style menu items.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::SelectStyle(void)
{
    int index = -1;
    if (wsdialogs::SelectStyleDialog(this, index) == false)
    {
        return;
    }

    switch (index)
    {
        case 0:   // Bold
        {
            InjectChord(CTRL_P, 'b');
            break;
        }
        case 1:   // Italic
        {
            InjectChord(CTRL_P, 'y');
            break;
        }
        case 2:   // Underline
        {
            InjectChord(CTRL_P, 's');
            break;
        }
        case 3:   // Strikeout
        {
            InjectChord(CTRL_P, 'x');
            break;
        }
        case 4:   // Superscript
        {
            InjectChord(CTRL_P, 't');
            break;
        }
        case 5:   // Subscript
        {
            InjectChord(CTRL_P, 'v');
            break;
        }
        case 6:   // Font...
        {
            InjectChord(CTRL_P, '=');
            break;
        }
        case 7:   // Color...
        {
            InjectChord(CTRL_P, '-');
            break;
        }
        default:
        {
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
/// @param  int row [in] the row to draw the ruler on
///
/// @return nothing
///
/// @brief
/// Draw the ruler: margins, tab stops, paragraph indent, and caret position.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawRuler(cScreen& screen, int row)
{
    sStyle style;
    style.fg = mRulerFg;
    style.bg = mRulerBg;
    style.attrs = wordstartui::CELL_ATTR_NONE;

    int offset = 0;
    int rulerCols = mCols;
    GetEditorHorizontalExtent(offset, rulerCols);

    sRect bar;
    bar.row = row;
    bar.col = offset;
    bar.rows = 1;
    bar.cols = rulerCols;
    screen.FillRect(bar, "\xe2\x94\x80", style);

    cLayoutBase* layout = mEditor->GetLayout();
    COORD_T twipsPerCol = mEditor->GetTwipsPerColumn();
    if ((layout == nullptr) || (twipsPerCol <= 0))
    {
        return;
    }

    COORD_T leftMargin = layout->GetLeftMargin();

    // Tab stops.
    const std::vector<sTabStop>& tabs = layout->GetTabs();
    for (const sTabStop& tab : tabs)
    {
        int tabCol = static_cast<int>((tab.position - leftMargin) / twipsPerCol);
        if ((tabCol > 0) && (tabCol < rulerCols - 1))
        {
            std::string glyph = "\xe2\x96\xba";     // right arrow
            if (tab.type == TAB_DECIMAL)
            {
                glyph = "\xe2\x80\xa2";             // bullet
            }
            screen.PutCell(row, offset + tabCol, glyph, style);
        }
    }

    // Paragraph indent.
    COORD_T paraIndent = layout->GetParagraphMargin();
    if (paraIndent != 0)
    {
        int pmCol = static_cast<int>(paraIndent / twipsPerCol);
        if ((pmCol > 0) && (pmCol < rulerCols - 1))
        {
            screen.PutCell(row, offset + pmCol, "\xc2\xb6", style);
        }
    }

    // Left and right margin markers.
    screen.PutCell(row, offset, "\xe2\x94\x9c", style);
    if (rulerCols > 1)
    {
        screen.PutCell(row, offset + rulerCols - 1, "\xe2\x94\xa4", style);
    }

    // Caret indicator (inverted), hidden on dot/comment lines.
    cDocument* doc = mEditor->GetDocument();
    if (doc != nullptr)
    {
        POSITION_T pos = doc->GetPosition();
        LINE_T caretLine = layout->GetLineFromPosition(pos);
        if (caretLine >= 0)
        {
            PARAGRAPH_T caretPara = layout->GetParagraphFromLine(caretLine);
            bool hide = layout->ParagraphIsCommand(caretPara) || layout->ParagraphIsComment(caretPara);
            if (hide == false)
            {
                COORD_T pageOffset = layout->GetPageOffsetOdd();
                COORD_T absoluteX = layout->FindCoordInLine(pos, caretLine);
                COORD_T relativeX = absoluteX - pageOffset - leftMargin;
                int caretCol = static_cast<int>(relativeX / twipsPerCol);
                if ((caretCol >= 0) && (caretCol < rulerCols))
                {
                    sStyle inv = style;
                    inv.attrs = inv.attrs | wordstartui::CELL_ATTR_INVERSE;
                    screen.PutCell(row, offset + caretCol, "\xe2\x94\x80", inv);
                }
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cScreen& screen [in,out] target screen
///
/// @return nothing
///
/// @brief
/// Draw the bottom status line: transient message, markers, insert/overwrite
/// mode, block state, page/line/column position, and word/char counts.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::DrawBottomStatus(cScreen& screen)
{
    sStatus status;
    mEditor->GetStatus(status);

    sStyle style;
    style.fg = mStatusFg;
    style.bg = mStatusBg;
    style.attrs = wordstartui::CELL_ATTR_NONE;

    int row = mRows - 1;
    sRect bar;
    bar.row = row;
    bar.col = 0;
    bar.rows = 1;
    bar.cols = mCols;
    screen.FillRect(bar, " ", style);

    // Left: transient status message, or an animated busy indicator while a
    // background layout is in progress.
    std::string message = mEditor->GetStatusMessage();
    mEditor->TickStatusMessage();
    if (message.empty() == false)
    {
        screen.PutText(row, 1, " " + message + " ", style);
        mBusyFrame = 0;
    }
    else if (status.backgroundBusy == true)
    {
        // Rising/falling block glyphs.
        static const char* busyFrames[15] =
        {
            "\xe2\x96\x81", "\xe2\x96\x82", "\xe2\x96\x83", "\xe2\x96\x84",
            "\xe2\x96\x85", "\xe2\x96\x86", "\xe2\x96\x87", "\xe2\x96\x88",
            "\xe2\x96\x87", "\xe2\x96\x86", "\xe2\x96\x85", "\xe2\x96\x84",
            "\xe2\x96\x83", "\xe2\x96\x82", "\xe2\x96\x81"
        };
        std::string spin = busyFrames[mBusyFrame % 15];
        mBusyFrame++;
        screen.PutText(row, 1, " " + spin + " working... ", style);
    }
    else
    {
        mBusyFrame = 0;
    }

    // Right: markers, mode, block, position, counts.
    std::string mode = (status.mode == true) ? "Insert" : "Overwrite";
    std::string block = (status.blockSet == true) ? "<B><K>" : "      ";
    std::string suffix = status.measureSuffix;

    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "%s %s %s  P%ld/%ld  L%ld V%.2f%s  C%ld H%.2f%s  W:%ld Ch:%ld ",
                  status.markers.c_str(), mode.c_str(), block.c_str(),
                  status.page, status.pagecount,
                  status.line, status.vPosition, suffix.c_str(),
                  status.column, status.hPosition, suffix.c_str(),
                  status.wordcount, status.charcount);

    std::string right = buffer;
    int col = mCols - static_cast<int>(right.size());
    if (col < 0)
    {
        col = 0;
    }
    screen.PutText(row, col, right, style);

    // Record clickable field ranges (absolute columns) matching the format
    // string above: "<markers> <mode> <block>  P<page>/<count> ...". The mode
    // field toggles Insert/Overwrite; the page field opens Go To Page.
    char pageBuffer[64];
    int pageLen = std::snprintf(pageBuffer, sizeof(pageBuffer), "P%ld/%ld",
                                status.page, status.pagecount);
    int modeOffset = static_cast<int>(status.markers.size()) + 1;
    mModeClickStart = col + modeOffset;
    mModeClickEnd = mModeClickStart + static_cast<int>(mode.size());
    int pageOffset = modeOffset + static_cast<int>(mode.size()) + 1
                     + static_cast<int>(block.size()) + 2;
    mPageClickStart = col + pageOffset;
    mPageClickEnd = mPageClickStart + pageLen;
    mBottomStatusRow = row;
}

// =========================================================================
// Input
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true if handled
///
/// @brief
/// Route input for the current mode.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSWordTsar::HandleEvent(const sInputEvent& event)
{
    if (mMode == APP_SPLASH)
    {
        if (mHaveFileArg == true)
        {
            mMode = APP_EDITOR;
        }
        else
        {
            mMode = APP_OPENING;
        }
        return true;
    }

    if (mMode == APP_OPENING)
    {
        return HandleOpeningKey(event);
    }

    // F1 is WordStar 7's real Help key now (see cWordStarInput::HandleSpecialKey);
    // it used to open System Preferences here, ahead of the normal input
    // pipeline -- now it falls through so mView/mInput can handle it.

    if (mMenu.HandleEvent(event) == true)
    {
        CheckCommandSignals();
        return true;
    }

    // A left click on the top status bar acts on its regions (font/style label,
    // B/I/U, command-tags "*", L/C/R/J alignment) instead of the editor.
    if ((event.type == wordstartui::INPUT_TYPE_MOUSE) &&
        (event.mouseAction == wordstartui::MOUSE_ACTION_PRESS) &&
        (event.mouseButton == wordstartui::MOUSE_BUTTON_LEFT) &&
        (mTopStatusRow >= 0) && (event.mouseRow == mTopStatusRow))
    {
        if (HandleTopStatusClick(event.mouseCol) == true)
        {
            // Direct edits (e.g. SetAlignment) mutate the document without going
            // through the input pipeline, so refresh the view here too.
            mView->EnsureCaretVisible();
            CheckCommandSignals();
        }
        return true;
    }

    // A left click on the bottom status bar acts on its mode/page fields.
    if ((event.type == wordstartui::INPUT_TYPE_MOUSE) &&
        (event.mouseAction == wordstartui::MOUSE_ACTION_PRESS) &&
        (event.mouseButton == wordstartui::MOUSE_BUTTON_LEFT) &&
        (mBottomStatusRow >= 0) && (event.mouseRow == mBottomStatusRow))
    {
        if (HandleBottomStatusClick(event.mouseCol) == true)
        {
            mView->EnsureCaretVisible();
            CheckCommandSignals();
        }
        return true;
    }

    bool handled = mView->HandleEvent(event);

    if (handled == true)
    {
        // A scrollbar drag sets the viewport directly; don't re-center on caret.
        if (mView->DidScrollByBar() == false)
        {
            mView->EnsureCaretVisible();
        }
        CheckCommandSignals();
    }

    return handled;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sInputEvent& event [in] input event
///
/// @return true if handled
///
/// @brief
/// Drive the opening menu: arrows move the selection, a letter or Enter picks
/// an item.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSWordTsar::HandleOpeningKey(const sInputEvent& event)
{
    // Tab toggles focus between the menu and the file picker.
    if ((event.type == wordstartui::INPUT_TYPE_SPECIAL) &&
        (event.special == wordstartui::SPECIAL_KEY_TAB))
    {
        if (mOpeningBrowserFocus == true)
        {
            mOpeningBrowserFocus = false;
        }
        else
        {
            mOpeningBrowserFocus = true;
        }
        return true;
    }

    // F1 (help) works regardless of focus, matching real WS7 -- it's not a
    // menu item you select, it's always-available help.
    if ((event.type == wordstartui::INPUT_TYPE_FUNCTION) && (event.functionKey == 1))
    {
        ShowOpeningHelp();
        return true;
    }

    // ----- File picker focused -----
    if (mOpeningBrowserFocus == true)
    {
        if ((event.type == wordstartui::INPUT_TYPE_SPECIAL) &&
            (event.special == wordstartui::SPECIAL_KEY_ESCAPE))
        {
            mOpeningBrowserFocus = false;
            mOpeningWantPrint = false;
            mOpeningWantTOC = false;
            mOpeningWantIndex = false;
            return true;
        }

        wsui::cFileBrowser::eAction action = mBrowser.HandleEvent(event);
        if (action == wsui::cFileBrowser::ACTION_FILE)
        {
            mFilename = mBrowser.GetSelectedFile();
            mEditor->LoadFile(mFilename);
            mEditor->RelayoutAndRedraw();
            if (mOpeningWantPrint == true)
            {
                mOpeningWantPrint = false;
                mEditor->Print();
            }
            if (mOpeningWantTOC == true)
            {
                mOpeningWantTOC = false;
                GenerateTOCFromFile();
            }
            if (mOpeningWantIndex == true)
            {
                mOpeningWantIndex = false;
                GenerateIndexFromFile();
            }
            mMode = APP_EDITOR;
            return true;
        }
        if (action == wsui::cFileBrowser::ACTION_EXIT_TOP)
        {
            mOpeningBrowserFocus = false;
            mOpeningWantPrint = false;
            mOpeningWantTOC = false;
            mOpeningWantIndex = false;
        }
        return true;
    }

    // ----- Menu focused -----
    eOpeningAction action{};
    bool haveAction = false;
    int rightCount = kOpeningCount - kOpeningLeftCount;

    if (event.type == wordstartui::INPUT_TYPE_SPECIAL)
    {
        // Two-column grid navigation: Down/Up move within a column (cursor
        // may land on disabled items), Down past the bottom enters the file
        // browser, and Left/Right switch columns, clamping to the target
        // column's (shorter or longer) row count.
        bool onLeft = (mOpeningSel < kOpeningLeftCount);
        int col = onLeft ? 0 : 1;
        int row = onLeft ? mOpeningSel : (mOpeningSel - kOpeningLeftCount);
        int rowsInCol = onLeft ? kOpeningLeftCount : rightCount;

        if (event.special == wordstartui::SPECIAL_KEY_ARROW_DOWN)
        {
            if (row < (rowsInCol - 1))
            {
                mOpeningSel = onLeft ? (row + 1) : (kOpeningLeftCount + row + 1);
            }
            else
            {
                mOpeningBrowserFocus = true;
            }
            return true;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_UP)
        {
            if (row > 0)
            {
                mOpeningSel = onLeft ? (row - 1) : (kOpeningLeftCount + row - 1);
            }
            return true;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_RIGHT)
        {
            if (col == 0)
            {
                mOpeningSel = kOpeningLeftCount + std::min(row, rightCount - 1);
            }
            return true;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ARROW_LEFT)
        {
            if (col == 1)
            {
                mOpeningSel = std::min(row, kOpeningLeftCount - 1);
            }
            return true;
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ENTER)
        {
            if (kOpeningItems[mOpeningSel].enabled == true)
            {
                action = kOpeningItems[mOpeningSel].action;
                haveAction = true;
            }
        }
        else if (event.special == wordstartui::SPECIAL_KEY_ESCAPE)
        {
            action = eOpeningAction::EXIT;
            haveAction = true;
        }
    }
    else if ((event.type == wordstartui::INPUT_TYPE_TEXT) && (event.textUtf8.empty() == false))
    {
        char typed = static_cast<char>(std::toupper(static_cast<unsigned char>(event.textUtf8[0])));

        for (int index = 0; index < kOpeningCount; ++index)
        {
            const sOpeningItem& item = kOpeningItems[index];
            if ((item.key != 0) && (item.enabled == true) &&
                (std::toupper(static_cast<unsigned char>(item.key)) == typed))
            {
                mOpeningSel = index;
                action = item.action;
                haveAction = true;
                break;
            }
        }
    }

    if (haveAction == false)
    {
        return true;
    }

    switch (action)
    {
        case eOpeningAction::EXIT:
            Quit();
            break;

        case eOpeningAction::OPEN_DOCUMENT:
        case eOpeningAction::CHANGE_DIRECTORY:
            mOpeningWantPrint = false;
            mOpeningWantTOC = false;
            mOpeningWantIndex = false;
            mOpeningBrowserFocus = true;
            break;

        case eOpeningAction::PRINT_FILE:
            mOpeningWantPrint = true;
            mOpeningBrowserFocus = true;
            break;

        case eOpeningAction::TABLE_OF_CONTENTS:
            mOpeningWantTOC = true;
            mOpeningBrowserFocus = true;
            break;

        case eOpeningAction::INDEX_DOCUMENT:
            mOpeningWantIndex = true;
            mOpeningBrowserFocus = true;
            break;

        case eOpeningAction::SPEED_WRITE:
            mFilename.clear();
            mMode = APP_EDITOR;
            break;

        case eOpeningAction::ABOUT:
            ShowAboutWordTsar();
            break;

        case eOpeningAction::PREFERENCES:
            OpenSystemPreferences();
            break;

        case eOpeningAction::RECENT_FILES:
            OpenRecentFiles();
            break;

        case eOpeningAction::DISPLAY_STATUS:
            ShowOpeningStatus();
            break;

        default:
            // Fax, print-from-keyboard, index/TOC, protect, rename/copy/
            // delete, turn-directory-off, macros, and run-a-DOS-command are
            // all disabled in kOpeningItems (nobody needs a DOS shell from
            // here today, same as fax), so this is unreachable via normal
            // dispatch -- kept as a safe no-op rather than a fallthrough
            // NotImplemented, since none of these can currently be selected.
            break;
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show a small status screen (version, current directory, free disk space)
/// -- real WS7's own Opening Menu "?" (display status).
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::ShowOpeningStatus(void)
{
    std::string directory = mBrowser.GetCurrentDirectory();

    std::string text = std::string("WordTsar ") + FULLVERSION_STRING + " " + STATUS + "\n";
    text += "Current directory: " + directory + "\n";

    std::error_code ec;
    std::filesystem::space_info space = std::filesystem::space(directory, ec);
    if (!ec)
    {
        double freeGb = static_cast<double>(space.available) / (1024.0 * 1024.0 * 1024.0);
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%.1f GB free", freeGb);
        text += std::string("Disk space: ") + buffer;
    }

    wsdialogs::MessageBox(this, "Status", text);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show WordTsar's real legal notices: copyright, no-warranty disclaimer,
/// redistribution rights, and how to view the full licence text -- AGPL
/// §5(d)/§0's "Appropriate Legal Notices." Reached via Help -> About
/// WordTsar. Deliberately separate from About(), which stays the
/// memory/status screen reachable via ^O? and the Opening Menu's "?".
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::ShowAboutWordTsar(void)
{
    std::string text = std::string("WordTsar ") + FULLVERSION_STRING + " " + STATUS + "\n\n";

    text += "Wordstar for the 21st century. This is a macOS-focused fork of "
            "Gerald Brandt's WordTsar -- the Wordstar-clone editing engine, "
            "document formats, and original design are his work; this fork "
            "adds macOS-specific packaging and features on top. Not endorsed "
            "by or affiliated with the upstream WordTsar project.\n\n";

    text += "Copyright (C) 2018 Gerald Brandt\n";
    text += "Portions Copyright (C) 2026 Egbert H. Schroeer\n\n";

    text += "This program comes with ABSOLUTELY NO WARRANTY.\n";
    text += "This is free software, and you are welcome to redistribute it "
            "under the terms of the GNU Affero General Public License v3.0. "
            "See LICENSE.md in the source distribution, or "
            "https://www.gnu.org/licenses/agpl-3.0.html for the full licence "
            "text.\n\n";

    text += "https://github.com/Egbert-Azure/WordTsar";

    wsdialogs::MessageBox(this, "About WordTsar", text);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show brief instructions for the Opening Menu -- real WS7's own Opening
/// Menu "F1" (help).
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::ShowOpeningHelp(void)
{
    wsdialogs::MessageBox(this, "Help",
        "Press the highlighted letter to choose a command, or use the arrow "
        "keys and Enter. Tab moves focus into the file list below.");
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Real WS7's own Opening Menu "T" (table of contents). mFilename (set by
/// the caller from the file browser selection) is loaded and laid out,
/// every .tc/.tc1-.tc9 entry in it is collected and resolved to a real
/// page number, and each non-empty table is written to its own file. The
/// first file written is then opened for editing, matching the manual's
/// own "you can edit the table of contents file to make any formatting
/// changes or corrections."
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::GenerateTOCFromFile(void)
{
    std::vector<std::string> outputFiles;
    bool ok = cTOCIndexGenerator::GenerateTOC(mEditor, mFilename, outputFiles);

    if (ok == false)
    {
        // GenerateTOC() never touches mEditor's document unless it actually
        // has entries to write -- it's already sitting there exactly as the
        // browser-selection flow loaded it, so there's nothing to reload.
        wsdialogs::MessageBox(this, "Table of Contents",
            "No .tc entries were found in\n" + mFilename);
        return;
    }

    std::string text = "Wrote:\n";
    for (const std::string& path : outputFiles)
    {
        text += path + "\n";
    }
    wsdialogs::MessageBox(this, "Table of Contents", text);

    mFilename = outputFiles.front();
    mEditor->LoadFile(mFilename);
    mEditor->RelayoutAndRedraw();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Real WS7's own Opening Menu "I" (index a document). mFilename is loaded
/// and laid out, every .ix entry is collected, resolved, alphabetized and
/// merged, and the result is written to a real .IDX file and opened for
/// editing.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::GenerateIndexFromFile(void)
{
    std::string outputFile;
    bool ok = cTOCIndexGenerator::GenerateIndex(mEditor, mFilename, outputFile);

    if (ok == false)
    {
        // GenerateIndex() never touches mEditor's document unless it actually
        // has entries to write -- it's already sitting there exactly as the
        // browser-selection flow loaded it, so there's nothing to reload.
        wsdialogs::MessageBox(this, "Index",
            "No .ix entries were found in\n" + mFilename);
        return;
    }

    wsdialogs::MessageBox(this, "Index", "Wrote:\n" + outputFile);

    mFilename = outputFile;
    mEditor->LoadFile(mFilename);
    mEditor->RelayoutAndRedraw();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Real WS7's own Insert -> Index/TOC Entry -> TOC Entry command (classic
/// .tc). Prompts for the entire line of text to appear in the table of
/// contents -- per the manual, extra leading spaces indent it, and a bare
/// # marks where the real page number goes at generation time -- then
/// inserts ".tc <text>" as its own line right before the current one,
/// matching the manual's own "WordStar inserts the .tc dot command
/// followed by the text."
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::InsertTOCEntry(void)
{
    std::string text;
    if (wsdialogs::InputBox(this, "Table of Contents Entry",
            "Text to appear in the table of contents:", text) == false)
    {
        return;
    }
    if (text.empty())
    {
        return;
    }

    mEditor->InsertDotCommandEntry(".tc", text);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Real WS7's own Insert -> Index/TOC Entry -> Index Entry command
/// (classic ^ONI). Prompts for the word or phrase to appear in the index
/// -- a leading + bolds its page number, a leading - marks a
/// cross-reference, a comma adds a subreference -- then inserts
/// ".ix <text>" as its own line, the same way TOC Entry does.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::InsertIndexEntry(void)
{
    std::string text;
    if (wsdialogs::InputBox(this, "Index Entry",
            "Word or phrase to appear in the index:", text) == false)
    {
        return;
    }
    if (text.empty())
    {
        return;
    }

    mEditor->InsertDotCommandEntry(".ix", text);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cTerminalDriver& driver [in,out] terminal driver
///
/// @return nothing
///
/// @brief
/// Place the hardware cursor at the caret in editor mode; hide it otherwise.
///
/////////////////////////////////////////////////////////////////////////////
void cWSWordTsar::OnAfterDraw(cTerminalDriver& driver)
{
    // Cache the terminal's capabilities so dialogs (e.g. the color picker) can
    // query the color depth via HostCapabilities().
    mCapabilities = driver.GetCapabilities();

    // Pass the terminal's attribute capabilities to the editor once the driver
    // is up, so unsupported attributes fall back to substitute colors.
    if (mCapsApplied == false)
    {
        mEditor->SetTermCapabilities(mCapabilities.bold, mCapabilities.italic, mCapabilities.underline, mCapabilities.strikethrough);
        mCapsApplied = true;
    }

    // A termination signal (SIGTERM/SIGHUP/SIGINT) only set a flag. Break the
    // event loop; the emergency save runs after the loop exits (in Run()), in
    // clean context with the terminal already restored -- doing a full SaveFile
    // mid-frame would re-enter the renderer.
    if (gShutdownPending != 0)
    {
        Quit();
        return;
    }

    // Periodic auto-save of the -bak backup, driven off the frame loop instead
    // of a background thread. An interval of 0 disables it.
    if ((mMode == APP_EDITOR) && (mEditor->mAutoSaveIntervalSec > 0))
    {
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        if ((now - mLastAutoSave) >= std::chrono::seconds(mEditor->mAutoSaveIntervalSec))
        {
            mEditor->AutoSaveBackup();
            mLastAutoSave = now;
        }
    }

    if ((mMode == APP_EDITOR) && (mView->CaretVisible() == true))
    {
        // Insert mode draws a blinking block; overwrite mode draws a steady
        // block that covers the character.
        wordstartui::eCursorShape shape = (mEditor->mInsertMode == true)
                                              ? wordstartui::CURSOR_SHAPE_BLOCK
                                              : wordstartui::CURSOR_SHAPE_BLOCK_STEADY;
        driver.SetCursorShape(shape);
        driver.SetCursor(mView->GetCaretRow(), mView->GetCaretCol(), true);
    }
    else
    {
        driver.SetCursor(0, 0, false);
    }
}
