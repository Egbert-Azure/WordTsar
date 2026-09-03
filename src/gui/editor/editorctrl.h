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

#ifndef EDITORCTRL_H
#define EDITORCTRL_H

#include <QWidget>
#include <QPaintEvent>
#include <QWheelEvent>
#include <QScrollBar>
#include <QTimer>
#include <QPainter>
#include <QRectF>
#include <QPageSize>
#include <QProgressDialog>
#include <QInputMethodEvent>

#include "src/core/editor/editorbase.h"
#include "src/gui/layout/layout.h"
#include "src/input/inputhandler.h"

// Forward declarations
class cFile;
class cWordTsar;
class cRulerCtrl;


class cEditorCtrl : public QWidget, public cEditorBase
{
    Q_OBJECT

    // =================================================================
    // METHODS
    // =================================================================

public:
    // --- Construction / Destruction ---
    explicit cEditorCtrl(QWidget* parent = nullptr);
    explicit cEditorCtrl(cDocument* sharedDocument, QWidget* parent = nullptr);
    virtual ~cEditorCtrl(void);

    // --- Qt layout integration ---
    // Required for QVBoxLayout to size help panels correctly
    QSize sizeHint(void) const override;
    QSize minimumSizeHint(void) const override;

    // --- cEditorBase pure virtual overrides ---
    void Repaint(void) override;
    COORD_T GetViewportHeight(void) const override;
    void StartCaretTimer(long delay) override;
    void StopCaretTimer(void) override;

    // --- Document listener overrides ---
    // Drive visual updates from document mutations
    void OnDocumentChanged(PARAGRAPH_T fromParagraph) override ;
    void OnDocumentCleared(void) override ;

    // --- Display mode hooks ---
    // Hook: auto-hide reveal codes when leaving page mode
    void OnBeforeDisplayModeChange(eDisplayMode newMode) override ;
    // Hook: update menu labels after mode change (Command Tags <-> Reveal Codes)
    void OnAfterDisplayModeChange(eDisplayMode newMode) override ;

    // --- Background layout hook ---
    // Hook: trigger background layout via QTimer (for sibling editor updates)
    void TriggerIdleLayout(void) override ;

    // --- Visual update ---
    // Single visual update method -- incremental layout + caret + scroll + paint
    void PerformVisualUpdate(void) override;

    // --- Scroll management ---
    // Override scroll management to sync scrollbar
    void SetScrollOffset(COORD_T offset) override;
    // Qt-specific methods (scrollbar integration)
    void SetScrollbar(QScrollBar* scrollbar);
    void UpdateScrollbar(void);

    // --- Page scale management ---
    // Shared by paintEvent and keyPressEvent
    void RecalculatePageScale(void);

    // --- Ruler integration ---
    void UpdateRuler(void);

    // --- Qt-specific accessors ---
    const QRectF& GetCaretPosQt(void) const;

    // --- Color setters and getters ---
    void SetBGroundColour(const QColor& color);
    QColor GetBGroundColour(void) const;
    void SetTextColour(const QColor& color);
    QColor GetTextColour(void) const;
    void SetHighlightColour(const QColor& color);
    void SetHighlightFgColour(const QColor& color);
    QColor GetHighlightColour(void) const;
    void SetDotColour(const QColor& color);
    void SetDotFgColour(const QColor& color);
    QColor GetDotColour(void) const;
    void SetBlockColour(const QColor& color);
    QColor GetBlockColour(void) const;
    void SetCommentColour(const QColor& color);
    void SetCommentFgColour(const QColor& color);
    QColor GetCommentColour(void) const;
    void SetErrorColour(const QColor& color);
    void SetErrorFgColour(const QColor& color);
    QColor GetErrorColour(void) const;
    void SetUnknownColour(const QColor& color);
    void SetUnknownFgColour(const QColor& color);
    QColor GetUnknownColour(void) const;
    void SetNotImplementedColour(const QColor& color);
    void SetNotImplementedFgColour(const QColor& color);
    QColor GetNotImplementedColour(void) const;
    // Search highlight color (internal use)
    void SetSearchColour(const QColor& color);

    // --- Highlight management (public for testing) ---
    void ClearHighlights(void);
    void GenerateHighlightRects(void);

    // --- Background layout methods (P4, public for testing) ---
    bool IdleLayout(void);
    PARAGRAPH_T GetLastVisibleParagraph(void); 

    // --- Listener state accessors (public for testing) ---
    bool GetListenerHandledUpdate() const { return mListenerHandledUpdate; }
    void ResetListenerHandledUpdate() { mListenerHandledUpdate = false; }

    // --- Background layout state accessors (P4, public for testing) ---
    PARAGRAPH_T GetLayoutParagraph() const { return mLayoutParagraph; }
    void SetLayoutParagraph(PARAGRAPH_T para) { mLayoutParagraph = para; }
    bool GetLayoutInt() const { return mLayoutInt; }
    void SetLayoutInt(bool value) { mLayoutInt = value; }
    bool GetLayoutRest() const { return mLayoutRest; }
    void SetLayoutRest(bool value) { mLayoutRest = value; }
    const std::vector<PARAGRAPH_T>& GetInterruptStack() const { return mInterruptStack; }
    std::vector<PARAGRAPH_T>& GetInterruptStack() { return mInterruptStack; }

    // --- Platform-specific overrides of cEditorBase pure virtual methods ---
    void NotImplemented(const std::string& command) override;
    void InvalidCommand(const std::string& command) override;
    void SetTitle(const std::string& title) override;
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
    void ChangeHelpLevel(void) override;
    void SelectFont(void) override;
    void SelectColor(void) override;
    void About(void) override;
    void PageLayout(void) override;
    void PrintPreview(void) override;
    void Print(void);
    void SpellCheckDocument(void) override;
    void SpellCheckWord(void) override;
    void SpellCheckEnterWord(void) override;
    void WordCountBlock(void) override;
    void Undo(void) override;
    void Redo(void) override;
    void ToggleShowControl(void) override;
    void LayoutDocument(bool force) override;
    bool IsBusy(void) const;
    bool LoadFile(const std::string& filename) override;
    bool SaveFile(const std::string& filename) override;
    std::string PromptForLoadFile(void) override;
    std::string PromptForSaveFile(void) override;

    // --- High-level file operations for menu actions. Bypass the input
    // handler so they work correctly in all input modes (WordStar / Modern).
    bool Open(void);
    bool Save(void);
    bool SaveAs(void);
    bool SaveAndClose(void);
    void ExitApplication(void);
    bool CloseEvent(void) override;
    void ShowError(const std::string& title, const std::string& message) override;
    void ShowMessage(const std::string& title, const std::string& message) override;
    bool AskYesNo(const std::string& title, const std::string& question) override;
    void Quit(void) override;
    void SetEnabled(bool enabled) override;

    // --- GUI-specific wrappers (call base class + update Qt UI) ---
    void SetPreviousBlock(void);
    void CopyBlock(void);
    void MoveBlock(void);
    void DeleteBlock(void);
    void WordLeft(void);
    void WordRight(void);
    void PageUp(void);
    void PageDown(void);

    // --- Main window integration ---
    void Preferences(void) override;
    void ToggleFullscreen(void) override;
    void SystemPreferences(void);
    void SetFrame(cWordTsar* frame);
    void SetRuler(cRulerCtrl* ruler);

    // --- File I/O (GUI-specific helpers) ---
    void EmergencySaveFile(char *text) override;
    void FileIOProgress(int percent) override;

    // --- Progress indicator helpers (call cWordTsar::SetStatus) ---
    void StartProgress(const std::string& message);
    void UpdateProgress(int percent);
    void StopProgress(void);

    // --- Mouse handling (P1.4, public for testing) ---
    POSITION_T PositionFromPoint(const QPoint& point);
    void mousePressEvent(QMouseEvent* event) override;

    // --- Input mode management ---
    void SetInputMode(eInputMode mode);
    eInputMode GetInputMode(void) const;

signals:

public slots:

protected:
    // --- Qt event handlers ---
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    // --- Initialization helper ---
    // Common initialization shared by both constructors
    void Init(void) ;

    // --- Caret sync helper ---
    // Helper method to sync base class coordinates to Qt rectangle
    void SyncCaretToQt(void);

    // --- Dialog helpers ---
    // Helper method to build file filter string for dialogs
    QString BuildFileFilterString(bool forSave);

    // --- Qt-specific rendering methods ---
    void DrawContinuousMode(QPainter& painter, COORD_T viewportHeight);
    void DrawPageMode(QPainter& painter, COORD_T viewportHeight);
    void DrawLine(QPainter& painter, const sLineLayout& line, const QColor& fgOverride = QColor());
    void DrawSegment(QPainter& painter, const sSegmentLayout& segment, COORD_T lineX, COORD_T lineY, COORD_T maxAscent, PAGE_T pageNumber, const QColor& fgOverride = QColor());
    void DrawControlCodeBackgrounds(QPainter& painter, const sSegmentLayout& segment, const std::vector<std::string>& graphemes, COORD_T lineX, COORD_T lineY);
    void DrawPageBackground(QPainter& painter, PAGE_T pageNumber, COORD_T pageYOffset, COORD_T paperWidth, COORD_T paperHeight);
    void DrawPageShadow(QPainter& painter, COORD_T pageYOffset, COORD_T paperWidth, COORD_T paperHeight);
    void DrawHeadersFooters(QPainter& painter, PAGE_T page, COORD_T pageScreenY);
    void DrawHeaderFooterLine(QPainter& painter, const sHeaderFooterLine& hfLine, COORD_T screenY);
    void DrawDotCommandBackground(QPainter& painter, const sParagraphLayout& paragraph);

    // --- Caret rendering (Qt-specific) ---
    void DrawCaret(QPainter& painter);

    // --- Highlight rendering (internal methods) ---
    void DrawHighlights(QPainter& painter);
    QPainterPath BuildSelectionContour(const std::vector<QRectF>& rects);
    QPainterPath BuildBlockPolygon(void);   // LibreOffice-style 8-point polygon
    QPainterPath BuildSearchPolygon(void);  // Search highlight polygon (paint-time)

private slots:
    void OnScrollbarChanged(int value);
    void OnCaretTimer(void);
    void OnAutoSaveTimer(void);
    void OnIdle(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

public:
    // Input handler (public for testing, uses Strategy Pattern)
    IInputHandler* mInput;                // Polymorphic keyboard command handler

    std::vector<cFile*> mFileTypes;       // File type handlers (for load/save dialogs)
    QProgressDialog* mProgress;           // Progress dialog for file I/O operations

    // P3: UI Dialog state (for PageLayout, PrintPreview)
    QPageSize::PageSizeId mPaperId;       // Current paper type
    cLayout* mPrintLayout;               // Layout for printing (temporary, created for print preview)
    bool mInPrintPreview;                 // Flag: currently in print preview mode

    // P1 Cutover: Main window integration
    cWordTsar* mWordTsar;                 // Reference to main window (for updates, events)
    cRulerCtrl* mRuler;                   // Reference to ruler control (for sync)
    bool mFullScreen;                     // Flag: window is in fullscreen mode

    // P4 Cutover: Preferences dialog state
    // mShortName, mLongName, mAlwaysDot now live in cEditorBase (shared with the
    // TUI); they are inherited here.
    bool mAlwaysFlag;                     // Always show flag column
    bool mDispScrollBar;                  // Display scrollbar flag
    bool mDispStyleBar;                   // Display style bar flag
    bool mDispStatusBar;                  // Display status bar flag
    bool mDispStatusBarBeforeHelpLevel0;  // mDispStatusBar as it was before ChangeHelpLevel forced it off at level 0
    bool mDispRuler;                      // Display ruler flag
    bool mDispMenu;                       // Display menu bar flag

protected:

private:
    // Qt-specific members (platform-specific rendering and UI)
    QScrollBar* mScrollbar;               // Scrollbar widget (not owned)
    QTimer* mCaretTimer;                  // Blink timer (caret blinking)
    QTimer* mAutoSaveTimer;               // Auto-save backup timer
    // Note: Idle layout uses QTimer::singleShot() on-demand, no persistent timer needed
    QRectF mCaretPosQt;                   // Qt rectangle for rendering (synced from base class coords)
    QRectF mLastPaintedCaretPosQt;         // Caret rect as of the last FULL repaint (not a blink-only tick)
    COORD_T mLastPaintedScrollOffset;     // Scroll offset as of the last FULL repaint
    double mPageScale;                    // Dynamic page zoom scale

    // Visual display colors (Qt-specific)
    QColor mBGroundColour;                // Background color
    QColor mHighlightColour;              // Control code backgrounds
    QColor mHighlightFgColour;           // Control code foreground
    QColor mDotColour;                    // DOT_GOOD commands background
    QColor mDotFgColour;                 // DOT_GOOD commands foreground
    QColor mCommentColour;                // Comment backgrounds
    QColor mCommentFgColour;             // Comment foreground
    QColor mUnknownColour;                // DOT_UNKNOWN command background
    QColor mUnknownFgColour;             // DOT_UNKNOWN command foreground
    QColor mErrorColour;                  // DOT_ERROR command background
    QColor mErrorFgColour;               // DOT_ERROR command foreground
    QColor mNotImplementedColour;         // DOT_NOTIMPLEMENTED background
    QColor mNotImplementedFgColour;      // DOT_NOTIMPLEMENTED foreground
    QColor mTextColour;                   // Default text color

    // Selection highlighting (Qt-specific)
    std::vector<QRectF> mBlockCoords;     // Rectangles for block highlight
    QColor mBlockColour;                  // Block highlight color (semi-transparent)
    QColor mSearchColour;                 // Search highlight color (semi-transparent)

    // Block highlight colors are stored here; block range comes from cDocument directly

    // Input mode state
    eInputMode mInputMode;                // Current input mode

public:
    // Focus dimming (for split-pane reveal codes)
    // Public so cWordTsar can set initial state in ToggleRevealCodes
    bool mHasFocusDim;                    // True when this editor has keyboard focus
};

#endif // EDITORCTRL_H
