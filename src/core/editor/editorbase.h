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

#ifndef EDITORBASE_H
#define EDITORBASE_H

#include "src/core/layout/layoutstructs.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/document/document.h"
#include "src/core/document/documentlistener.h"
#include <atomic>
#include <thread>
#include <vector>

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sStatus
///
/// @brief
/// Editor state snapshot for the status bar display.
/// Contains cursor position, page/word/char counts, formatting state,
/// and current font/file information. Updated periodically by the editor.
///
/////////////////////////////////////////////////////////////////////////////
struct sStatus
{
    long column;
    long line;
    long page;
    long pagecount;
    long wordcount;
    long charcount;
    bool showcontrol;
    bool saving;
    bool mode;
    bool attrib;
    bool bold;
    bool italic;
    bool underline;
    eJustification just;
    eHelpDisplay help;
    std::string style;
    std::string font;
    std::string filename;
    double vPosition;               // Vertical position in user measurement units
    double hPosition;               // Horizontal position in user measurement units
    std::string measureSuffix;      // Measurement suffix: "\"", " cm", " mm"
    bool blockSet;                  // Whether a block selection is active
    bool backgroundBusy;            // Whether background layout is in progress
    std::string markers;            // Active place markers (e.g. "150" if markers 1, 5, 0 are set)
};

/////////////////////////////////////////////////////////////////////////////
///
/// @class cEditorBase
///
/// @brief
/// Non-Qt base class for editor functionality.
/// Contains all business logic for document navigation, caret positioning,
/// scrolling, and viewport management without any UI dependencies.
///
/// This class uses the Template Method pattern: derived classes must
/// implement platform-specific operations (Repaint, GetViewportHeight, etc.)
/// while the base class provides all the business logic.
///
/// Coordinates are stored in TWIPS (platform-independent). Derived classes
/// convert to their own coordinate systems during rendering.
///
/// Design for subclass reuse:
/// - All state is platform-independent
/// - Pure virtual methods for platform-specific operations
/// - No Qt dependencies (no QWidget, QPainter, QRect, etc.)
/// - Caret position is simple COORD_T values, not rectangle objects
///
/////////////////////////////////////////////////////////////////////////////
class cEditorBase : public cDocumentListener
{
    // =================================================================
    //  METHODS
    // =================================================================

public:
    // Construction / Destruction
    cEditorBase(void);
    virtual ~cEditorBase(void);

    // Default starting directory for file dialogs (user's Documents folder,
    // falling back to "./" if it can't be determined) when no explicit
    // "Default Directory" preference has been set.
    static std::string DefaultFileDir(void);

    // Document Listener Implementation
    void OnDocumentChanged(PARAGRAPH_T fromParagraph) override ;
    void OnDocumentCleared(void) override ;

    // Pure Virtual - Platform Rendering
    virtual void Repaint(void) = 0;                           // Trigger platform-specific repaint
    virtual COORD_T GetViewportHeight(void) const = 0;        // Get viewport height in twips
    virtual void StartCaretTimer(long delay) = 0;             // Start platform-specific blink timer
    virtual void StopCaretTimer(void) = 0;                    // Stop platform-specific blink timer
    virtual void PerformVisualUpdate(void) = 0;               // Caret + scroll + repaint (platform-specific)
    virtual PARAGRAPH_T GetLastVisibleParagraph(void) = 0;    // Find last visible paragraph from layout data

    // Pure Virtual - Platform UI Dialogs
    virtual void NotImplemented(const std::string& command) = 0;  // Show "not implemented" message
    virtual void InvalidCommand(const std::string& command) = 0;  // Show "invalid command" message
    virtual void SetTitle(const std::string& title) = 0;          // Set window title
    virtual void ShowError(const std::string& title, const std::string& message) = 0;    // Show error dialog
    virtual void ShowMessage(const std::string& title, const std::string& message) = 0;  // Show info message dialog
    virtual bool AskYesNo(const std::string& title, const std::string& question) = 0;    // Show yes/no dialog, return true if yes
    virtual void About(void) = 0;                             // Show about dialog
    virtual void Quit(void) = 0;                              // Quit the application
    virtual void SetEnabled(bool enabled) = 0;                // Enable/disable editor input

    // Pure Virtual - Clipboard and Search
    virtual void ClipboardCopy(void) = 0;                     // Copy block to system clipboard
    virtual void ClipboardPaste(void) = 0;                    // Paste from system clipboard
    virtual void Find(void) = 0;                              // Show find dialog
    virtual void FindAgain(void) = 0;                         // Repeat last find
    virtual bool ReplaceAgain(bool noquery = false) = 0;      // Repeat last replace
    virtual void Replace(void) = 0;                           // Show find & replace dialog
    virtual void GotoCharacter(void) = 0;                     // Show goto character dialog
    virtual void GotoCharacterBackward(void) = 0;             // Show goto character backward dialog
    virtual void GotoPage(void) = 0;                          // Show goto page dialog
    virtual void DeleteToChar(void) = 0;                      // Show delete to character dialog
    virtual void ChangeHelpLevel(void) = 0;                   // Prompt for and apply a new WS4 help level (0-3)

    // Pure Virtual - Formatting and Layout Dialogs
    virtual void SelectFont(void) = 0;                        // Show font selection dialog
    virtual void SelectColor(void) = 0;                       // Show color selection dialog
    virtual void PageLayout(void) = 0;                        // Show page layout dialog
    virtual void PrintPreview(void) = 0;                      // Show print preview
    virtual void SpellCheckDocument(void) = 0;                // Spell check entire document
    virtual void SpellCheckWord(void) = 0;                    // Spell check word at cursor
    virtual void SpellCheckEnterWord(void) = 0;               // Spell check user-entered word
    virtual void WordCountBlock(void) = 0;                    // Count words in block, display result
    virtual void ToggleShowControl(void) = 0;                 // Toggle control code display mode

    // Pure Virtual - Undo/Redo
    virtual void Undo(void) = 0;                              // Undo last action
    virtual void Redo(void) = 0;                              // Redo last undone action

    // Pure Virtual - File Operations
    virtual void LayoutDocument(bool force) = 0;              // Layout document (platform-specific refresh)
    virtual bool LoadFile(const std::string& filename) = 0;   // Load file with platform progress display, replacing the current document
    virtual bool InsertFileAtCursor(const std::string& filename) = 0; // Insert a file's parsed content at the cursor, real WS7 ^KR/Insert>File
    virtual bool SaveFile(const std::string& filename) = 0;   // Save file
    virtual void EmergencySaveFile(char *text) = 0;           // Emergency save on fatal error (e.g., bad_alloc)
    virtual std::string PromptForLoadFile(void) = 0;          // Show file open dialog
    virtual std::string PromptForSaveFile(void) = 0;          // Show file save dialog
    virtual bool CloseEvent(void) = 0;                        // Handle close (confirm save)
    virtual void FileIOProgress(int /*percent*/) {}           // Report file I/O progress (default no-op)
    virtual void Preferences(void) {}                         // Show preferences dialog (default no-op)
    virtual void SystemPreferences(void) {}                   // Show system preferences dialog (default no-op)
    virtual void ToggleFullscreen(void) {}                    // Toggle fullscreen mode (default no-op)
    virtual void AbandonFile(void);                           // Abandon current file (^KQ)

    // Post-Command Update
    void PerformPostCommandUpdate(void);

    // Dirty State
    void ResetDocumentDirty(void) ;
    bool IsDocumentDirty(void) const ;

    // Display Mode
    void SetDisplayMode(eDisplayMode mode);
    eDisplayMode GetDisplayMode(void) const;
    void ToggleDisplayMode(void);
    bool IsPageModeSupported(void) const;

    // TUI-only: center the editing pane horizontally in the terminal instead
    // of pinning it to the left edge (the character-grid equivalent of the
    // GUI's pixel-based page centering). No-op where not overridden (GUI uses
    // ToggleDisplayMode/page mode for its own centering instead).
    virtual void ToggleCenterView(void) {}

    // Document/Layout Accessors
    cDocument* GetDocument(void) const;
    cLayoutBase* GetLayout(void) const;

    // Sibling Editor (Reveal Codes)
    void SetSiblingEditor(cEditorBase* sibling) ;

    // Scroll Management
    virtual void SetScrollOffset(COORD_T offset);
    COORD_T GetScrollOffset(void) const;
    COORD_T CalculateTotalDocumentHeight(void) const;

    // Viewport Management
    void CalculateViewport(void);
    const sViewport& GetViewport(void) const;

    // Caret Movement
    void CalculateCaretPosition(void);
    virtual void ScrollIntoView(void);
    void MoveCaretLeft(void);
    void MoveCaretRight(void);
    void MoveCaretWordLeft(void);
    void MoveCaretWordRight(void);
    void MoveCaretLine(int delta);
    void MoveCaretPage(int delta);
    void MoveCaretToDocStart(void);
    void MoveCaretToDocEnd(void);

    // Navigation (high-level, includes visual update)
    void ScrollUp(void);
    void ScrollDown(void);
    void MoveCursorTopLeft(void);
    void MoveCursorBottomRight(void);
    void MoveCursorTopofFile(void);
    void MoveCursorEndofFile(void);
    void MoveCursorStartBlock(void);
    void MoveCursorEndBlock(void);
    void MoveCursorStartLine(void);
    void MoveCursorEndLine(void);
    void InsertDotCommandEntry(const std::string &dotPrefix, const std::string &text);
    void GotoPreviousPosition(void);
    void GotoLastFindandReplace(void);
    void GotoFontTag(void);

    // Text Editing
    void InsertText(const std::string& text);
    void Delete(POSITION_T pos, POSITION_T length);
    void DeleteChar(void);
    void ToggleInsertOverwrite(void);
    void DeleteWordRight(void);
    void DeleteWordLeft(void);
    void LineBreak(void);
    void Backspace(void);
    void DeleteKey(void);
    void Tab(void);
    void InsertWordStarString(const std::string& text);
    void InsertCenterTab(void);
    void InsertRightTab(void);

    // Delete Line Operations
    void DeleteLine(void);
    void DeleteLineLeft(void);
    void DeleteLineRight(void);

    // Undo Grouping
    void CloseTypingGroup(void) ;

    // Block Operations
    void SetPreviousBlock(void);
    void CopyBlock(void);
    void MoveBlock(void);
    void DeleteBlock(void);
    void SetBeginBlock(void);
    void SetEndBlock(void);
    void UnSetBlock(void);
    void ToggleHideBlock(void);
    void UpperCaseBlock(void);
    void LowerCaseBlock(void);
    void SentenceCaseBlock(void);

    // Batch Update
    void BeginBatchUpdate(void);
    void EndBatchUpdate(void);

    // Position Save/Restore
    void SavePosition(int offset);
    void GotoSavePosition(int offset);

    // Formatting
    void ToggleJustification(void);
    void ToggleWordWrap(void);
    void SetAlignment(eJustification align) ;
    void SetParagraphAlignment(eJustification align) ;

    // Font and Display Settings
    void SetFont(const std::string& font);
    std::string GetFont(void) const;
    void SetShowControls(eShowControl show);
    eShowControl GetShowControls(void) const;
    void SetShowDot(bool show);
    bool GetShowDot(void) const;
    void SetMeasurement(const std::string& measure);
    std::string GetMeasurement(void) const;

    // Codepage
    void SetCodePage(eCodePage page);
    eCodePage GetCodePage(void);

    // Spell check dot commands
    bool GetSpellCheckDotCommands(void) const;
    void SetSpellCheckDotCommands(bool val);

    // Line Lookup
    LINE_T LineFromY(COORD_T yTwips);

    // Caret Position Accessors
    COORD_T GetCaretX(void) const;
    COORD_T GetCaretY(void) const;
    COORD_T GetCaretWidth(void) const;
    COORD_T GetCaretHeight(void) const;

    // Caret Visibility
    bool GetDrawnCaret(void) const;
    bool GetDoDrawCaret(void) const;
    void SetDrawnCaret(bool drawn);
    void SetDoDrawCaret(bool doDraw);

    // Document Position Accessors
    POSITION_T GetCaretDocumentPosition(void) const;
    LINE_T GetCaretLine(void) const;
    PARAGRAPH_T GetCaretParagraph(void) const;
    COORD_T GetCaretStickyX(void) const;
    PAGE_T GetCaretPageNumber(void) const;
    COORD_T GetCaretPageY(void) const;
    POSITION_T GetCaretLineDocPosition(void) const;

    // Display Settings Accessor
    const sDisplaySettings& GetDisplaySettings(void) const;

    // Debug Overlays
    void SetShowViewportDebug(bool show);
    void SetShowBoxStats(bool show);
    void SetShowBoxOutlines(bool show);
    bool GetShowViewportDebug(void) const;
    bool GetShowBoxStats(void) const;
    bool GetShowBoxOutlines(void) const;

    // Word Count
    long WordCount(POSITION_T start, POSITION_T end);
    void StartWordCountTimer(void);
    void StopWordCountTimer(void);

    // Status Bar
    void GetStatus(sStatus& status);
    void SetStatusMessage(const std::string& msg, bool busy = false, int durationFrames = 60);
    std::string GetStatusMessage(void) const;
    bool IsStatusBusy(void) const;
    void TickStatusMessage(void);

protected:
    // Caret Coordinate Helpers
    void AdjustCaretYForPageMode(const sLineLayout* line);
    COORD_T ScreenYToViewportY(const sLineLayout* line) const;

    // Sibling Caret Sync
    void SyncSiblingCaret(void) ;

    // Caret Segment Hook
    virtual void OnCaretSegmentResolved(const sLineLayout& line,
                                        size_t segmentIndex,
                                        const sSegmentLayout& segment,
                                        size_t graphemeIndex,
                                        bool atEndOfLine);

    // Hidden Content Skipping
    POSITION_T SkipOverHiddenContent(POSITION_T pos, int direction);

    // File State Persistence
    void LoadFileState(const std::string& filepath);
    void SaveFileState(const std::string& filepath);

private:
    /////////////////////////////////////////////////////////////////////////
    ///
    /// @struct sCaretCalcState
    ///
    /// @brief
    /// Carries shared state between the CalculateCaretPosition helper
    /// methods. Populated incrementally as each phase resolves its part
    /// of the document-position-to-screen-coordinate mapping.
    ///
    /////////////////////////////////////////////////////////////////////////
    struct sCaretCalcState
    {
        POSITION_T caretPos ;                    // document grapheme position
        PARAGRAPH_T para ;                       // paragraph containing caret
        const sParagraphLayout* paraLayout ;     // layout for that paragraph
        const sLineLayout* caretLine ;           // line containing caret
        POSITION_T paraStart ;                   // paragraph start in document
        POSITION_T offsetInPara ;                // caret offset within paragraph
        COORD_T caretX ;                         // resolved X coordinate
        COORD_T caretSegmentHeight ;             // text height (not line height)
        COORD_T caretGlyphWidth ;                // glyph width (for overwrite mode)
    } ;

    // Caret Calc Helpers
    bool LookupCaretParagraph(sCaretCalcState& state) ;
    bool HandleHiddenParagraph(sCaretCalcState& state) ;
    bool FindCaretLine(sCaretCalcState& state) ;
    void ResolveCaretGlyph(sCaretCalcState& state) ;
    void CalculatePrintX(const sCaretCalcState& state) ;

    // Helper Methods
    POSITION_T FindPositionAtX(const sLineLayout* line, COORD_T targetX);
    POSITION_T FindWordEnd(POSITION_T startPos);
    void ParseFontDescriptor(const std::string& fontDescriptor, const sLineLayout& line);

    // Word Count Timer
    void WordCountTimerThreadFunc(void);

    // Text Helpers
    void CheckAndReplaceVariable(void);
    bool IsCJKPunctuation(const std::string& text);

    // Alignment Helpers (for SetParagraphAlignment)
    bool IsOjDotCommand(PARAGRAPH_T para);
    eJustification GetAlignmentFromEndState(PARAGRAPH_T para);
    eJustification FindNextTextParagraphAlignment(PARAGRAPH_T currentPara);
    std::string AlignmentToDotCommand(eJustification align);

    // Hidden Content Skipping
    POSITION_T SkipHiddenParagraphs(POSITION_T pos, int direction);
    POSITION_T SkipHiddenControlCodes(POSITION_T pos, int direction);

    // Display Mode Hooks
    virtual void OnBeforeDisplayModeChange(eDisplayMode newMode) ;
    virtual void OnAfterDisplayModeChange(eDisplayMode newMode) ;

    // Idle Layout Hook
    virtual void TriggerIdleLayout(void) ;

    // =================================================================
    //  MEMBER VARIABLES
    // =================================================================

public:
    // Editor State
    std::string mFileName;              // Current filename
    std::string mBackupFileName;        // Backup filename (derived from mFileName)
    std::string mBackupUuid;            // Unique ID for untitled document temp backups
    bool mInsertMode;                   // Insert vs overwrite mode
    long mLastWordCount;                // Cached word count (updated by timer)
    std::string mShortName;             // User's short name (config [user])
    std::string mLongName;              // User's long name (config [user])
    bool mAlwaysDot;                    // Preference: always show dot commands
    eHelpDisplay mHelpDisplay;          // Current help mode display
    int mHelpLevel;                     // WS4 help level 0-3; gates Edit Menu/submenu visibility (see ChangeHelpLevel)
    bool mRevealCodesVisible;           // true if the reveal codes pane should be open for this document

    // Search/Replace State
    std::string mSearchText;            // Last search text
    std::string mReplaceText;           // Last replacement text
    bool mCaseCmp;                      // Case-sensitive search
    bool mSearchBackwards;              // Search direction (forward/backward)
    bool mWildCard;                     // Use wildcard matching
    bool mWholeWord;                    // Match whole words only
    bool mWholeFile;                    // Search entire file
    POSITION_T mLastFindandReplace;     // Position before search started
    POSITION_T mStartSearchBlock;       // Start of search highlight
    POSITION_T mEndSearchBlock;         // End of search highlight
    bool mSearchBlockSet;               // Search highlight active?
    POSITION_T mReplaceSize;            // Size of text to replace
    bool mReplaceAsk;                   // Ask before each replacement?
    int mReplaceScope;                  // Replace scope: 0=next, 1=entire, 2=rest

    // Navigation/File State
    bool mLastKeyUpOrDown;              // Track if last key was up/down for sticky X
    std::string mFileDir;               // Current file directory
    bool mFileSet;                      // Has filename been set?

    // Configurable Settings
    int mCaretBlinkRate;                // Cursor blink speed in milliseconds
    int mAutoSaveIntervalSec;           // Seconds between auto-saves (0 = disabled)
    std::string mDefaultFormat;         // Default file format ("ws" or "rtf")
    std::string mSpellCheckLanguage;    // Spell check dictionary language
    bool mSpellCheckDotCommands;        // Spell check text content of dot command lines

protected:
    // Layout and Document
    cLayoutBase* mLayout;
    cDocument* mDocument;

    // Listener State
    bool mOwnsDocument;                   // true if this editor created the document
    bool mDocumentDirty;                  // set by listener, cleared after relayout
    PARAGRAPH_T mDirtyFromParagraph;      // earliest paragraph affected by change
    int mBatchUpdateCount;                // > 0 suppresses listener visual updates
    bool mListenerHandledUpdate;          // set by OnDocumentChanged, checked by keyPressEvent
    cEditorBase* mSiblingEditor;          // sibling editor for reveal codes caret sync (nullptr when no reveal codes)
    bool mSyncingCaret;                   // recursion guard for bidirectional caret sync

    // Display Settings
    sDisplaySettings mDisplaySettings;
    eShowControl mSavedShowControl;

    // Scroll State
    COORD_T mScrollOffset;
    COORD_T mCaretLinePageX;             // pagex of the caret's line (set by CalculateCaretPosition)

    // Viewport State
    sViewport mViewport;
    sDisplayList mDisplayList;

    // Debug Overlays
    bool mShowViewportDebug;
    bool mShowBoxStats;
    bool mShowBoxOutlines;

    // Caret Position (in TWIPS - platform independent)
    COORD_T mCaretX;
    COORD_T mCaretPrintX;               // Caret X excluding non-printing control code widths (for status bar H)
    COORD_T mCaretY;
    COORD_T mCaretWidth;
    COORD_T mCaretHeight;

    // Document Position Tracking
    POSITION_T mCaretDocumentPosition;
    LINE_T mCaretLine;
    PARAGRAPH_T mCaretParagraph;
    COORD_T mCaretStickyX;
    PAGE_T mCaretPageNumber;              // Page number (cached by CalculateCaretPosition)
    COORD_T mCaretPageY;                  // Page-relative Y in twips (cached by CalculateCaretPosition)
    POSITION_T mCaretLineDocPosition;     // Document position where caret's line starts (for column calc)
    bool mCaretOnPrintableLine;           // false when caret is on a dot command or comment
    COORD_T mLastPrintablePageY;          // last V position from a printable line (persists across dot commands)
    LINE_T mCaretPageLineNumber;          // Line number on current page (resets each page, excludes dot commands)

    // Caret Visibility
    bool mDrawnCaret;
    bool mDoDrawCaret;

    // Undo Grouping
    bool mTypingGroupActive;              // tracks whether we're accumulating typed characters

    // Background Layout State
    bool mLayoutInt;                          // Layout interrupted by user input?
    bool mLayoutRest;                         // Force complete layout (to end of document)?
    PARAGRAPH_T mLayoutParagraph;             // Current background layout position
    std::vector<PARAGRAPH_T> mInterruptStack; // Stack of saved background layout positions
    PARAGRAPH_T mVisibleStart;                // First visible paragraph
    PARAGRAPH_T mVisibleEnd;                  // Last visible paragraph

    // Visible Line Tracking (populated during paint)
    LINE_T mPageFirstVisibleLine;             // First visible line number on this page
    LINE_T mPageLastVisibleLine;              // Last visible line number on this page
    COORD_T mPageFirstVisibleLineScrollY;     // Scroll-space Y of first visible line
    COORD_T mPageLastVisibleLineScrollY;      // Scroll-space Y of last visible line

    // Font and Display Settings
    std::string mBaseFont;              // Base font name/specification
    bool mShowDot;                      // Show dot commands (hidden/visible)
    bool mPageModeSupported;            // Whether page view mode is available
    std::string mMeasurement;           // Measurement string (for ruler)
    eMeasurement mMeasure;              // Measurement system (inches/cm/mm)

    // Codepage
    eCodePage mCodePage;

    // Status Bar Cache
    std::string mStatusFont;                // Formatted font string (e.g., "Times New Roman 8.5")
    bool mStatusBold;                       // Bold state at caret
    bool mStatusItalic;                     // Italic state at caret
    bool mStatusUnderline;                  // Underline state at caret
    eJustification mStatusJust;             // Justification at caret

    // Status Message
    std::string mStatusMessage;             // Current status message text
    int mStatusMessageTimer;                // Countdown frames to clear message
    bool mStatusBusy;                       // True when busy indicator should be shown

private:
    // Word Count Timer
    std::atomic<bool> mWordCountTimerRunning;
    std::thread mWordCountTimerThread;
};

#endif // EDITORBASE_H
