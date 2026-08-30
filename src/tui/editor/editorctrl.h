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

#ifndef WSTUI_EDITORCTRL_H
#define WSTUI_EDITORCTRL_H

#include "src/core/editor/editorbase.h"
#include "src/input/inputhandler.h"

#include "src/tui/wordstartui/src/tuidefs.h"

#include <functional>
#include <string>

// Forward declarations
class IInputHandler;
class iWSDialogHost;

/////////////////////////////////////////////////////////////////////////////
///
/// @class cWSEditorCtrl
///
/// @brief
/// wordstartui-based editor control. Inherits from cEditorBase for all
/// business logic and implements the platform-specific operations without
/// an external UI framework. Colors use the wordstartui toolkit's sColor type.
///
/// It is built
/// for the wordstartui rendering toolkit. It owns a cDocument and a cLayout
/// (the TUI layout) and drives them via cEditorBase's business logic.
///
/////////////////////////////////////////////////////////////////////////////
class cWSEditorCtrl : public cEditorBase
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    explicit cWSEditorCtrl(void);
    virtual ~cWSEditorCtrl(void);

    // ---- Pure virtual: platform rendering ----
    void Repaint(void) override;
    COORD_T GetViewportHeight(void) const override;
    void StartCaretTimer(long delay) override;
    void StopCaretTimer(void) override;
    void PerformVisualUpdate(void) override;
    PARAGRAPH_T GetLastVisibleParagraph(void) override;

    // ---- Pure virtual: UI dialogs / messages ----
    void NotImplemented(const std::string& command) override;
    void InvalidCommand(const std::string& command) override;
    void SetTitle(const std::string& title) override;
    void ShowError(const std::string& title, const std::string& message) override;
    void ShowMessage(const std::string& title, const std::string& message) override;
    bool AskYesNo(const std::string& title, const std::string& question) override;
    void About(void) override;
    void Quit(void) override;
    void AbandonFile(void) override;
    void Preferences(void) override;
    void SetEnabled(bool enabled) override;

    // ---- Pure virtual: clipboard and search ----
    void ClipboardCopy(void) override;
    void ClipboardPaste(void) override;
    void Find(void) override;
    void FindAgain(void) override;
    bool ReplaceAgain(bool noquery = false) override;
    void Replace(void) override;
    void GotoCharacter(void) override;
    void GotoCharacterBackward(void) override;
    void GotoPage(void) override;
    void DeleteToChar(void) override;

    // ---- Pure virtual: formatting and layout dialogs ----
    void SelectFont(void) override;
    void SelectColor(void) override;
    void PageLayout(void) override;
    void PrintPreview(void) override;
    void Print(void);
    void SpellCheckDocument(void) override;
    void SpellCheckWord(void) override;
    void SpellCheckEnterWord(void) override;
    void WordCountBlock(void) override;
    void ToggleShowControl(void) override;

    // ---- Pure virtual: undo/redo ----
    void Undo(void) override;
    void Redo(void) override;

    // ---- Pure virtual: file operations ----
    void LayoutDocument(bool force) override;
    bool LoadFile(const std::string& filename) override;
    bool SaveFile(const std::string& filename) override;
    void EmergencySaveFile(char *text) override;
    void AutoSaveBackup(void);
    std::string PromptForLoadFile(void) override;
    std::string PromptForSaveFile(void) override;
    bool CloseEvent(void) override;
    void FileIOProgress(int percent) override;

    // ---- Scroll management overrides ----
    void SetScrollOffset(COORD_T offset) override;
    void ScrollIntoView(void) override;

    // ---- Accessors (expose protected base members) ----
    cLayoutBase* GetLayout(void) const;
    cDocument* GetDocument(void) const;
    IInputHandler* GetInput(void) const;

    // ---- Input mode (Strategy Pattern) ----
    void SetInputMode(eInputMode mode);
    eInputMode GetInputMode(void) const;

    // ---- Viewport / terminal geometry ----
    void SetViewport(LINE_T firstLine, int rows);
    void SetTerminalSize(int rows, int cols);
    void SetChromeRows(int rows);
    int GetEditorRows(void) const;
    int GetTerminalRows(void) const;
    int GetTerminalCols(void) const;
    int GetTextAreaCols(void) const;
    COORD_T GetColumnWidth(void) const;
    COORD_T GetTwipsPerColumn(void) const;
    COORD_T GetLineHeightTwips(void) const;

    // ---- Redraw hook (called by Repaint) ----
    void SetRedrawHook(std::function<void()> hook);

    // ---- Dialog host (provides modal dialogs; set by the app) ----
    void SetDialogHost(iWSDialogHost* host);

    // ---- Preferences hook (screen-settings dialog; provided by the app) ----
    void SetPreferencesHook(std::function<void()> hook);

    // ---- Relayout / background layout ----
    void RelayoutAndRedraw(void);
    bool IdleLayout(void);
    bool HasPendingLayout(void) const;

    // ---- Quit state ----
    bool QuitRequested(void) const { return mQuit; }

    // ---- WordStar help/command-mode display (set by the input handler) ----
    eHelpDisplay GetHelpDisplay(void) const { return mHelpDisplay; }
    void SetHelpDisplay(eHelpDisplay help) { mHelpDisplay = help; }

    // ---- Terminal attribute capabilities (set by the app from the driver) ----
    void SetTermCapabilities(bool bold, bool italic, bool underline, bool strikethrough);
    bool TermSupportsBold(void) const { return mTermSupportsBold; }
    bool TermSupportsItalic(void) const { return mTermSupportsItalic; }
    bool TermSupportsUnderline(void) const { return mTermSupportsUnderline; }
    bool TermSupportsStrikethrough(void) const { return mTermSupportsStrikethrough; }

    // ---- Abandon-file return-to-opening-menu signal (^KQ) ----
    bool ReturnToOpeningRequested(void) const { return mReturnToOpening; }
    void ClearReturnToOpening(void) { mReturnToOpening = false; }

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

public:
    // Input handler (Strategy Pattern)
    IInputHandler* mInput;

    // Display colors (wordstartui sColor). Public like cTUIEditorCtrl.
    wordstartui::sColor mBGroundColour;          // Editor background color
    wordstartui::sColor mTextColour;             // Default text color
    wordstartui::sColor mScrollbarBg;            // Scrollbar track background
    wordstartui::sColor mScrollbarFg;            // Scrollbar thumb foreground
    wordstartui::sColor mFlagBg;                 // Flag/indicator column background
    wordstartui::sColor mFlagFg;                 // Flag/indicator column foreground
    wordstartui::sColor mHighlightColour;        // Control code background
    wordstartui::sColor mHighlightFgColour;      // Control code foreground
    wordstartui::sColor mDotColour;              // DOT_GOOD command background
    wordstartui::sColor mDotFgColour;            // DOT_GOOD command foreground
    wordstartui::sColor mBlockColour;            // Block selection background
    wordstartui::sColor mBlockFgColour;          // Block selection foreground
    wordstartui::sColor mSearchColour;           // Search result highlight background
    wordstartui::sColor mSearchFgColour;         // Search result highlight foreground
    wordstartui::sColor mCommentColour;          // Comment background
    wordstartui::sColor mCommentFgColour;        // Comment foreground
    wordstartui::sColor mErrorColour;            // DOT_ERROR background
    wordstartui::sColor mErrorFgColour;          // DOT_ERROR foreground
    wordstartui::sColor mUnknownColour;          // DOT_UNKNOWN background
    wordstartui::sColor mUnknownFgColour;        // DOT_UNKNOWN foreground
    wordstartui::sColor mNotImplementedColour;   // DOT_NOTIMPLEMENTED background
    wordstartui::sColor mNotImplementedFgColour; // DOT_NOTIMPLEMENTED foreground

    // Style fallback colors (when terminal lacks native style support)
    wordstartui::sColor mBoldColour;             // Bold text fallback foreground
    wordstartui::sColor mBoldBgColour;           // Bold text fallback background
    wordstartui::sColor mItalicColour;           // Italic text fallback foreground
    wordstartui::sColor mItalicBgColour;         // Italic text fallback background
    wordstartui::sColor mStrikethroughColour;    // Strikethrough text fallback foreground
    wordstartui::sColor mStrikethroughBgColour;  // Strikethrough text fallback background
    wordstartui::sColor mUnderlineColour;        // Underline text fallback foreground
    wordstartui::sColor mUnderlineBgColour;      // Underline text fallback background
    wordstartui::sColor mSuperscriptColour;      // Superscript text fallback foreground
    wordstartui::sColor mSuperscriptBgColour;    // Superscript text fallback background
    wordstartui::sColor mSubscriptColour;        // Subscript text fallback foreground
    wordstartui::sColor mSubscriptBgColour;      // Subscript text fallback background

    // Indicator (flag) column visibility
    bool mAlwaysFlag;                            // Always show indicator column

private:
    // Title / message state
    std::string mTitle;                          // Current window/document title
    std::string mLastMessage;                    // Last message shown (for app display)

    // Terminal geometry
    int mTerminalRows;                           // Current terminal height in rows
    int mTerminalCols;                           // Current terminal width in columns
    int mChromeRows;                             // Rows used by chrome (status bars, ruler)

    // Viewport range
    LINE_T mViewFirstLine;                       // First visible line
    int mViewRows;                               // Number of visible rows

    // Font-derived metrics
    COORD_T mColumnWidth;                        // Cached monospace column width in twips
    COORD_T mLineHeightTwips;                    // Fixed line height in twips

    // Quit / caret state
    bool mQuit;                                  // True when Quit() requested
    bool mReturnToOpening;                       // True when ^KQ abandon returns to opening menu

    // Terminal attribute capabilities (native bold/italic/underline/strike).
    bool mTermSupportsBold;
    bool mTermSupportsItalic;
    bool mTermSupportsUnderline;
    bool mTermSupportsStrikethrough;
    bool mCaretTimerRunning;                     // Caret timer running flag (no blink loop)

    // Input mode
    eInputMode mInputMode;

    // Redraw hook (invoked by Repaint)
    std::function<void()> mRedrawHook;

    // Dialog host (provides modal dialogs; set by the application)
    iWSDialogHost* mHost;

    // Display name of the file currently loading (for the progress overlay).
    std::string mLoadingFileName;

    // Preferences hook (invoked by Preferences() -- shows the app's dialog)
    std::function<void()> mPreferencesHook;

    // Cached clipboard tool (0=none, detected on first use)
    int mClipboardTool;

    // ---- Search / replace state (mirrors cTUIEditorCtrl) ----
    std::string mSearchText;                     // Current search text
    std::string mReplaceText;                    // Current replacement text
    bool mWholeWord;                             // Match whole words only
    bool mCaseCmp;                               // Ignore case when true
    bool mSearchBackwards;                       // Search backward when true
    bool mWildCard;                              // Enable wildcard matching
    bool mWholeFile;                             // Search/replace over the whole file
    bool mReplaceAsk;                            // Prompt before each replacement
    int mReplaceScope;                           // 0=next, 1=entire file, 2=rest of file
    POSITION_T mReplaceSize;                     // Grapheme length of the last match
    POSITION_T mLastFindandReplace;              // Position before the last find/replace
    // mStartSearchBlock, mEndSearchBlock, mSearchBlockSet are inherited (public)
    // from cEditorBase and shared with the view for search-result highlighting.
};

#endif // WSTUI_EDITORCTRL_H
