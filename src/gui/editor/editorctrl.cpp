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
 * @class cEditorCtrl
 *
 * @brief Qt GUI editor control implementing the WordTsar editing surface.
 *
 * Implements the cEditorCtrl class, the QWidget-based editor that handles all
 * Qt-specific rendering and user interaction. Inherits from both QWidget
 * (for Qt integration) and cEditorBase (for shared editing logic).
 *
 * @section guieditor_rendering Rendering (paintEvent)
 * - Draws the document content using layout data (segments, lines, paragraphs)
 * - Handles font formatting: bold, italic, underline, strikethrough,
 *   superscript, subscript, color via QPainter and QFont
 * - Draws page boundaries, margin guides, and selection highlighting
 * - Renders control code characters when display mode is active
 * - Caret rendering with configurable blink rate via QTimer
 *
 * @section guieditor_input Input Handling
 * - Keyboard: keyPressEvent processes all keystrokes, delegates to
 *   cWordStarInput for WordStar command interpretation
 * - Mouse: click for caret positioning, drag for selection, double-click
 *   for word selection, scroll wheel for viewport scrolling
 * - IME: inputMethodEvent for international text composition
 * - Clipboard: Qt clipboard integration for copy/cut/paste (text and RTF)
 *
 * @section guieditor_dialogs Dialog-Driven Features
 * - Find/Replace: modal dialogs with search options
 * - Goto: page number or character position navigation
 * - Page Layout: paper size, margins, orientation configuration
 * - System Preferences: multi-tab configuration dialog
 * - Spell Check: interactive spell checking with suggestions
 * - Print Preview: QPrintPreviewDialog integration via cPrintout
 * - Font/Color Selection: QFontDialog and custom cColorDialog
 *
 * @section guieditor_fileio File I/O
 * Manages loading and saving through cWordstarFile, cRTFFile, cDOCXFile,
 * and cTextFile handlers. Includes backup file management and auto-save
 * via configurable timer intervals.
 *
 * @section guieditor_scrolling Scrollbar and Viewport
 * - Vertical scrollbar tracks document position in both continuous and
 *   page display modes
 * - ScrollIntoView() ensures the caret remains visible after navigation
 * - Page mode: stacked pages with gaps, horizontal centering
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cEditorBase Base editor class with shared logic
 * @see cLayout Qt-specific layout engine
 * @see cWordTsar Main application window
 * @see cWordStarInput WordStar command interpreter
 * @see cPrintout Print/preview engine
 * @see cRulerCtrl Ruler widget
 */

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QFont>
#include <QDebug>
#include <bitset>
#include <new>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QClipboard>
#include <QMimeData>

#include <QApplication>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QColorDialog>
#include <QFontDialog>
#include <QDialog>
#include <QFontInfo>
#include <algorithm>  // for std::sort, std::min
#include <cmath>      // for std::abs
#include <ctime>      // for time(), strftime()
#include <cstdlib>    // for std::getenv()
#include <cstring>    // for std::strcmp()
#include <map>        // for std::map
#include <limits>     // for std::numeric_limits

// UI dialogs
#include "ui_preferences.h"
#include "ui_systempreferences.h"
#include "src/gui/ruler/rulerctrl.h"
#include "src/gui/wordtsar.h"
#include "src/gui/dialogs/colordialog.h"

// File I/O handlers
#include "src/files/wordstar/wordstarfile.h"
#include "src/files/textfile.h"
#include "src/files/rtffile.h"
#include "src/files/docxfile.h"

// Dialog UI headers
#include "ui_gotochar.h"
#include "ui_gotopage.h"
#include "ui_deletetochar.h"
#include "ui_pagelayout.h"
#include "ui_find.h"
#include "ui_findreplace.h"
#include <QDialog>
#include <QInputDialog>

#include "editorctrl.h"
#include "src/gui/utils/fontutils.h"
#include "src/input/wordtsarinput.h"
#include "src/input/moderninput.h"
#include "src/core/include/version.h"
#include "src/core/include/utils.h"
#include "src/core/utils/config.h"

// Spell check
#include "src/gui/spellcheck/cspellcheck.h"

// Print support
#include "src/gui/print/printout.h"

// Forward declarations - utility functions
bool FuzzyCompare(double a, double b);  // Defined in document.cpp

// Caret blink rate is now configurable via mCaretBlinkRate (from cConfig).
// macOS needs a halved rate because its compositing is different.

constexpr double INDICATOR_SPACE_PIXELS = 60.0;  // Space for right margin line + indicators

/////////////////////////////////////////////////////////////////////////////
///
/// @param  parent [in] parent widget (optional)
///
/// @return nothing
///
/// @brief
/// Constructor. Initializes Qt-specific editor control.
/// Base class cEditorBase handles all business logic initialization.
///
/////////////////////////////////////////////////////////////////////////////
cEditorCtrl::cEditorCtrl(QWidget* parent)
    : QWidget(parent)
    , cEditorBase()
    , mInput(nullptr)
    , mScrollbar(nullptr)
    , mInputMode(INPUT_WORDSTAR)
{
    // cEditorBase constructor already initialized:
    // - mLayout = nullptr
    // - mDocument = nullptr
    // - mScrollOffset = 0
    // - mDisplaySettings with defaults
    // - mCaretX, mCaretY, mCaretWidth, mCaretHeight
    // - mCaretDocumentPosition, mCaretLine, mCaretParagraph, mCaretStickyX
    // - mDrawnCaret, mDoDrawCaret
    // - mShowViewportDebug, mShowBoxStats, mShowBoxOutlines

    // Create and own document and layout
    // Document is auto-initialized with ^Z (EOF marker) in its constructor
    mDocument = new cDocument();
    mLayout = new cLayout();

    // Common initialization (colors, timers, widget setup, etc.)
    Init() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  sharedDocument [in] existing document to share (not owned)
/// @param  parent [in] parent widget (optional)
///
/// @return nothing
///
/// @brief
/// Shared-document constructor for reveal codes pane. Uses an existing
/// document (not owned) with its own layout. The codes editor registers
/// as a listener so it receives change notifications from the shared doc.
///
/////////////////////////////////////////////////////////////////////////////
cEditorCtrl::cEditorCtrl(cDocument* sharedDocument, QWidget* parent)
    : QWidget(parent)
    , cEditorBase()
    , mInput(nullptr)
    , mScrollbar(nullptr)
    , mInputMode(INPUT_WORDSTAR)
{
    // Share existing document (not owned by this editor)
    mDocument = sharedDocument;
    mOwnsDocument = false;
    mLayout = new cLayout();

    // Common initialization (colors, timers, widget setup, etc.)
    Init() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Common initialization shared by both constructors. Called after
/// mDocument and mLayout are set up. Initializes colors, widget
/// attributes, timers, input handler, and triggers first idle layout.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Init(void)
{
    // Create input handler using Strategy Pattern
    SetInputMode(INPUT_WORDSTAR);

    // Qt-specific caret position (synced from base class)
    mCaretPosQt = QRectF(0, 0, 30, 300);

    // Enable Input Method (IME) support for CJK text input
    // This allows Japanese, Chinese, Korean, etc. input via system IME
    setAttribute(Qt::WA_InputMethodEnabled);

    // Initialize page scale to default (fixed DPI scale)
    mPageScale = 1.0 / FONTSCALE;

    // Default colors for visual display
    mBGroundColour = QColor(245, 245, 245);         // Light gray background
    mHasFocusDim = true;                              // Assume focused until told otherwise
    mTextColour = QColor(0, 0, 0);                  // Black text
    mHighlightColour = QColor(0, 150, 200, 75);    // Cyan with alpha
    mHighlightFgColour = QColor(0, 0, 0);           // Black text for control codes
    mDotColour = QColor(100, 200, 200, 190);        // Light cyan with alpha
    mDotFgColour = QColor(0, 0, 0);                 // Black text for dot commands
    mBlockColour = QColor(50, 100, 200, 75);       // Blue with alpha
    mCommentColour = QColor(255, 178, 102, 190);    // Orange with alpha
    mCommentFgColour = QColor(0, 0, 0);             // Black text for comments
    mUnknownColour = QColor(194, 70, 65, 190);      // Red with alpha
    mUnknownFgColour = QColor(255, 255, 255);       // White text for unknown
    mErrorColour = QColor(255, 200, 200);           // Light red
    mErrorFgColour = QColor(255, 255, 255);         // White text for errors
    mNotImplementedColour = QColor(255, 220, 200);  // Light orange
    mNotImplementedFgColour = QColor(0, 0, 0);      // Black text for not implemented

    // Initialize highlight tracking
    mBlockCoords.clear();
    mSearchColour = QColor(50, 100, 200, 75);      // Darker blue, more opaque (75%)

    // Initialize UI dialog state
    mPaperId = QPageSize::Letter;                   // Default to Letter size
    mPrintLayout = nullptr;                         // Created on-demand for print preview
    mInPrintPreview = false;                        // Not in print preview mode

    // Initialize main window integration
    mWordTsar = nullptr;                            // Set by SetFrame()
    mRuler = nullptr;                               // Set by SetRuler()
    mFullScreen = false;                            // Start in windowed mode

    // Initialize preferences dialog state
    mShortName = "";                                // Empty by default
    mLongName = "";                                 // Empty by default
    mAlwaysDot = true;                              // Show dots by default
    mAlwaysFlag = true;                             // Show flag column by default
    mDispScrollBar = true;                          // Display scrollbar by default
    mDispStyleBar = true;                           // Display style bar by default
    mDispStatusBar = true;                          // Display status bar by default
    mDispStatusBarBeforeHelpLevel0 = true;          // Restored when leaving help level 0
    mDispRuler = true;                              // Display ruler by default
    mDispMenu = true;                               // Display menu bar by default

    // Register as listener for document change notifications
    mDocument->AddListener(this) ;

    // Sync document's ShowControl to match display settings default (SHOW_ALL)
    // The old editor did this in its constructor via SetShowControls(SHOW_ALL)
    mDocument->SetShowControl(mDisplaySettings.showControl);
    // Prevent Qt from auto-filling background on update()
    // CRITICAL for XOR caret drawing
    setAttribute(Qt::WA_OpaquePaintEvent);

    // Allow widget to receive focus
    setFocusPolicy(Qt::StrongFocus);

    // Let idle layout handle initial layout instead of calling LayoutDocument()
    // This preserves empty mParagraphLayout for first-time incremental layout
    // IdleLayout() will be triggered below and will layout from paragraph 0
    if (mLayout && mDocument)
    {
        // Initialize incremental layout state for first layout
        mLayoutParagraph = 0;
        mLayoutInt = false;
        mLayoutRest = false;
    }

    // Initialize caret position (will be recalculated after first layout)
    CalculateCaretPosition();

    // Create caret timer
    mCaretTimer = new QTimer(this);
    mCaretTimer->setSingleShot(true);
    connect(mCaretTimer, &QTimer::timeout, this, &cEditorCtrl::OnCaretTimer);
    mCaretTimer->start(mCaretBlinkRate);

    // Start word count timer (base class background thread)
    StartWordCountTimer();

    // Create auto-save timer
    mAutoSaveTimer = new QTimer(this);
    mAutoSaveTimer->setSingleShot(false);  // Repeating timer
    connect(mAutoSaveTimer, &QTimer::timeout, this, &cEditorCtrl::OnAutoSaveTimer);
    mAutoSaveTimer->start(mAutoSaveIntervalSec * 1000);

    // Trigger initial idle layout after 1ms
    // Uses QTimer::singleShot() for on-demand scheduling pattern (not persistent timer)
    // Timer will reschedule itself in OnIdle() while work remains, stops when complete
    QTimer::singleShot(1, this, SLOT(OnIdle()));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor. Qt timer is auto-deleted by parent relationship.
/// Deletes WordStar input handler.
///
/////////////////////////////////////////////////////////////////////////////
cEditorCtrl::~cEditorCtrl(void)
{
    // Delete file type handlers
    for (auto* fileType : mFileTypes)
    {
        delete fileType;
    }
    mFileTypes.clear();

    // Delete input handler (polymorphic deletion)
    delete mInput;

    // Deregister from document before deleting it
    if (mDocument != nullptr)
    {
        mDocument->RemoveListener(this) ;
    }

    // Delete owned document and layout
    if (mOwnsDocument)
    {
        delete mDocument;
    }
    delete mLayout;

    // Qt timer is auto-deleted (parent is this widget)
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return QSize  minimum size hint for layout
///
/// @brief
/// Returns minimum size for this widget. Required by QVBoxLayout to
/// allocate space when hidden help panels are shown via show().
///
/////////////////////////////////////////////////////////////////////////////
QSize cEditorCtrl::minimumSizeHint(void) const
{
    return QSize(100, 100);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return QSize  preferred size hint for layout
///
/// @brief
/// Returns preferred size for this widget. Required by QVBoxLayout to
/// allocate space when hidden help panels are shown via show().
///
/////////////////////////////////////////////////////////////////////////////
QSize cEditorCtrl::sizeHint(void) const
{
    return QSize(800, 600);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T fromParagraph [in] earliest affected paragraph
///
/// @return nothing
///
/// @brief
/// Called by the document when it is mutated (Insert, Delete, etc.).
/// Sets dirty flags for idle layout rewind. Does NOT call PerformVisualUpdate()
/// because layout hasn't run yet -- keyPressEvent handles visual updates after
/// incremental layout with current data.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnDocumentChanged(PARAGRAPH_T fromParagraph)
{
    // Set dirty flags in base class (for idle layout rewind)
    cEditorBase::OnDocumentChanged(fromParagraph) ;

    // Mark that a document mutation occurred (for keyPressEvent flow tracking).
    // Suppressed during batch -- EndBatchUpdate sets the flag when count drops to 0.
    if (mBatchUpdateCount == 0)
    {
        if (mSiblingEditor == nullptr || hasFocus())
        {
            // Active editor (or sole editor): defer visual update to keyPressEvent
            mListenerHandledUpdate = true ;
        }
        else
        {
            // Sibling editor (no keyPressEvent running): self-update immediately
            PerformPostCommandUpdate() ;
            TriggerIdleLayout() ;       // re-layout paragraphs beyond visible range
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Called by the document after Clear(). Triggers full relayout
/// unless the document is loading (file I/O) or batch is active.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnDocumentCleared(void)
{
    // Set dirty flags in base class
    cEditorBase::OnDocumentCleared() ;

    if (!mDocument->GetLoading() && mBatchUpdateCount == 0)
    {
        LayoutDocument(true) ;
        CalculateCaretPosition() ;
        ScrollIntoView() ;
        UpdateScrollbar() ;
        update() ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  eDisplayMode newMode [in] the display mode about to be set
///
/// @return nothing
///
/// @brief
/// Hook called before display mode changes. Auto-hides the reveal codes
/// pane when leaving page mode, since reveal codes is a page-mode feature.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnBeforeDisplayModeChange(eDisplayMode newMode)
{
    if (mWordTsar != nullptr && mWordTsar->IsRevealCodesVisible() &&
        mDisplaySettings.mode == DISPLAY_PAGE && newMode != DISPLAY_PAGE)
    {
        mWordTsar->ToggleRevealCodes() ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  eDisplayMode newMode [in] the mode that was just set
///
/// @return nothing
///
/// @brief
/// Update the View menu label after a display mode change.
/// In page mode the menu says "Show Formatting", in continuous mode it says
/// "Command Tags".
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnAfterDisplayModeChange(eDisplayMode newMode)
{
    if (mWordTsar != nullptr)
    {
        mWordTsar->UpdateCommandTagsLabel(newMode) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Trigger background/idle layout via QTimer. Used by sibling editors
/// (reveal codes) to ensure paragraphs beyond the visible range get
/// re-laid-out after a document mutation in the other editor.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::TriggerIdleLayout(void)
{
    QTimer::singleShot(1, this, SLOT(OnIdle())) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Single visual update method. Recalculates caret position, scrolls
/// the viewport to keep the caret visible, and triggers a repaint.
///
/// This replaces scattered CalculateCaretPosition + ScrollIntoView +
/// Repaint calls throughout the editor. All document mutations drive
/// visual updates through OnDocumentChanged calls PerformVisualUpdate.
///
/// Note: incremental layout is NOT done here -- keyPressEvent handles
/// that in its centralized update block (always, for both mutation and
/// navigation keys). This matches the current behavior where scattered
/// methods do caret+scroll+paint without layout.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::PerformVisualUpdate(void)
{
    if (!mLayout || !mDocument)
    {
        return ;
    }

    // Caret, scroll, paint
    CalculateCaretPosition() ;
    ScrollIntoView() ;
    CalculateCaretPosition() ;      // recalculate after scroll adjusts viewport
    UpdateScrollbar() ;
    update() ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  eInputMode mode [in] Input mode to switch to
///
/// @return nothing
///
/// @brief
/// Set the keyboard input mode 
/// Deletes the current input handler and creates a new one for the
/// specified mode. Currently only WordStar mode is supported.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetInputMode(eInputMode mode)
{
    // Build the new handler before touching mInput so it is never null
    IInputHandler *newInput = nullptr;
    switch (mode)
    {
        case INPUT_WORDSTAR:
            newInput = new cWordStarInput(this);
            break;

        case INPUT_MODERN:
            newInput = new cModernInput(this);
            break;

        default:
            // Fallback to WordStar
            newInput = new cWordStarInput(this);
            mode = INPUT_WORDSTAR;
            break;
    }

    // Swap in the new handler, then delete the old one
    IInputHandler *oldInput = mInput;
    mInput = newInput;
    delete oldInput;

    mInputMode = mode;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return eInputMode Current input mode
///
/// @brief
/// Get the current keyboard input mode.
///
/////////////////////////////////////////////////////////////////////////////
eInputMode cEditorCtrl::GetInputMode(void) const
{
    return mInputMode;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Implements pure virtual from cEditorBase.
/// Triggers Qt repaint via update().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Repaint(void)
{
    update();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return viewport height in twips
///
/// @brief
/// Implements pure virtual from cEditorBase.
/// Returns Qt widget height converted to twips.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorCtrl::GetViewportHeight(void) const
{
    // Convert pixel height to twips using actual rendering scale
    // mPageScale converts twips to pixels, so pixels to twips is 1/mPageScale
    if (mPageScale > 0)
    {
        return static_cast<COORD_T>(height() / mPageScale);
    }
    // Fallback before mPageScale is initialized
    return static_cast<COORD_T>(height()) * FONTSCALE;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  delay [in] timer delay in milliseconds
///
/// @return nothing
///
/// @brief
/// Implements pure virtual from cEditorBase.
/// Starts Qt caret blink timer.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::StartCaretTimer(long delay)
{
    mCaretTimer->start(delay);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Implements pure virtual from cEditorBase.
/// Stops Qt caret blink timer.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::StopCaretTimer(void)
{
    mCaretTimer->stop();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  offset [in] new scroll offset in twips
///
/// @return nothing
///
/// @brief
/// Overrides cEditorBase::SetScrollOffset to sync scrollbar value.
/// Calls base class implementation for clamping and state update,
/// then syncs the Qt scrollbar widget.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetScrollOffset(COORD_T offset)
{
    // Call base class implementation (handles clamping and mScrollOffset update)
    cEditorBase::SetScrollOffset(offset);

    // Sync scrollbar value to match new offset
    UpdateScrollbar();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  scrollbar [in] scrollbar widget to use (or nullptr)
///
/// @return nothing
///
/// @brief
/// Sets the scrollbar for this editor. Scrollbar is optional.
/// If provided, connects valueChanged signal to update viewport.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetScrollbar(QScrollBar* scrollbar)
{
    // Disconnect old scrollbar if it exists
    if (mScrollbar)
    {
        disconnect(mScrollbar, &QScrollBar::valueChanged,
                   this, &cEditorCtrl::OnScrollbarChanged);
    }

    // Store new scrollbar
    mScrollbar = scrollbar;

    // Connect new scrollbar if provided
    if (mScrollbar)
    {
        connect(mScrollbar, &QScrollBar::valueChanged,
                this, &cEditorCtrl::OnScrollbarChanged);

        // Initial update to sync scrollbar with current state
        UpdateScrollbar();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Updates scrollbar range and position based on document height.
/// Uses Y-coordinates (twips), NOT line numbers.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::UpdateScrollbar(void)
{
    // Guard: scrollbar is optional
    if (!mScrollbar || !mLayout)
    {
        return;
    }

    // Get total document height in twips
    COORD_T totalHeight = CalculateTotalDocumentHeight();

    // Get viewport height in twips
    COORD_T viewportHeight = GetViewportHeight();

    // Calculate maximum scroll position
    COORD_T maxScroll = totalHeight - viewportHeight;
    if (maxScroll < 0)
    {
        maxScroll = 0;
    }

    // Block signals to prevent OnScrollbarChanged loop
    QSignalBlocker blocker(mScrollbar);

    // Set scrollbar range
    mScrollbar->setRange(0, static_cast<int>(maxScroll));

    // Set scrollbar current position
    mScrollbar->setValue(static_cast<int>(mScrollOffset));

    // Set page step
    mScrollbar->setPageStep(static_cast<int>(viewportHeight));

    // Set single step
    COORD_T avgLineHeight = mLayout->GetAverageLineHeight();
    mScrollbar->setSingleStep(static_cast<int>(avgLineHeight));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Recalculate mPageScale from current display mode and window geometry.
/// Shared by paintEvent (for rendering) and keyPressEvent (for viewport
/// calculations before GetLastVisibleParagraph).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::RecalculatePageScale(void)
{
    if (!mLayout)
    {
        return;
    }

    if (mDisplaySettings.mode == DISPLAY_CONTINUOUS)
    {
        COORD_T mPaperWidth = mLayout->GetPaperWidth();
        if (mPaperWidth > 0)
        {
            mPageScale = (static_cast<double>(width()) - INDICATOR_SPACE_PIXELS) / mPaperWidth;
        }
    }
    else
    {
        // Page mode
        COORD_T mPaperWidth = mLayout->GetPaperWidth();
        COORD_T pageBorder = mDisplaySettings.pageBorder;
        if (mPaperWidth > 0)
        {
            mPageScale = static_cast<double>(width()) / (mPaperWidth + 2 * pageBorder);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return const QRectF& - Qt caret rectangle
///
/// @brief
/// Returns the Qt-specific caret rectangle (synced from base class).
///
/////////////////////////////////////////////////////////////////////////////
const QRectF& cEditorCtrl::GetCaretPosQt(void) const
{
    return mCaretPosQt;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the highlight color for control code backgrounds.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetHighlightColour(const QColor& color)
{
    mHighlightColour = QColor(color.red(), color.green(), color.blue(), 190);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the foreground color for control code text.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetHighlightFgColour(const QColor& color)
{
    mHighlightFgColour = color;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the color for DOT_GOOD commands (green).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetDotColour(const QColor& color)
{
    mDotColour = QColor(color.red(), color.green(), color.blue(), 190);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the foreground color for DOT_GOOD command text.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetDotFgColour(const QColor& color)
{
    mDotFgColour = color;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the color for comment backgrounds (blue).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetCommentColour(const QColor& color)
{
    mCommentColour = QColor(color.red(), color.green(), color.blue(), 190);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the foreground color for comment text.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetCommentFgColour(const QColor& color)
{
    mCommentFgColour = color;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the color for DOT_UNKNOWN commands (yellow).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetUnknownColour(const QColor& color)
{
    mUnknownColour = QColor(color.red(), color.green(), color.blue(), 190);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the foreground color for DOT_UNKNOWN command text.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetUnknownFgColour(const QColor& color)
{
    mUnknownFgColour = color;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the color for DOT_ERROR commands (red).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetErrorColour(const QColor& color)
{
    mErrorColour = QColor(color.red(), color.green(), color.blue(), 190);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the foreground color for DOT_ERROR command text.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetErrorFgColour(const QColor& color)
{
    mErrorFgColour = color;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the color for DOT_NOTIMPLEMENTED commands (orange).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetNotImplementedColour(const QColor& color)
{
    mNotImplementedColour = QColor(color.red(), color.green(), color.blue(), 190);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the foreground color for DOT_NOTIMPLEMENTED command text.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetNotImplementedFgColour(const QColor& color)
{
    mNotImplementedFgColour = color;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the default text color.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetTextColour(const QColor& color)
{
    mTextColour = color;

    // NOTE: No layout propagation needed -- rendering checks IsDefault()
    // on the sentinel and uses mTextColour at draw time.
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] block highlight color
///
/// @return nothing
///
/// @brief
/// Sets block highlight color with alpha transparency.
/// Forces alpha to 127 (50% opacity) for semi-transparent overlay.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetBlockColour(const QColor& color)
{
    mBlockColour = QColor(color.red(), color.green(), color.blue(), 127);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] search highlight color
///
/// @return nothing
///
/// @brief
/// Sets search highlight color with alpha transparency.
/// Forces alpha to 190 (75% opacity) for slightly more opaque overlay.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetSearchColour(const QColor& color)
{
    mSearchColour = QColor(color.red(), color.green(), color.blue(), 75);
}


/////////////////////////////////////////////////////////////////////////////
// COLOR GETTERS AND SETTERS
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] color to set
///
/// @return nothing
///
/// @brief
/// Sets the background color for the editor.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetBGroundColour(const QColor& color)
{
    mBGroundColour = color;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current background color
///
/// @brief
/// Returns the background color for the editor.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetBGroundColour(void) const
{
    return mBGroundColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current text color
///
/// @brief
/// Returns the default text color.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetTextColour(void) const
{
    return mTextColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current highlight color
///
/// @brief
/// Returns the highlight color for control code backgrounds.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetHighlightColour(void) const
{
    return mHighlightColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current dot color
///
/// @brief
/// Returns the color for DOT_GOOD commands.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetDotColour(void) const
{
    return mDotColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current block color
///
/// @brief
/// Returns the block highlight color.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetBlockColour(void) const
{
    return mBlockColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current comment color
///
/// @brief
/// Returns the color for comment backgrounds.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetCommentColour(void) const
{
    return mCommentColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current error color
///
/// @brief
/// Returns the color for DOT_ERROR commands.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetErrorColour(void) const
{
    return mErrorColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current unknown color
///
/// @brief
/// Returns the color for DOT_UNKNOWN commands.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetUnknownColour(void) const
{
    return mUnknownColour;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QColor - current not implemented color
///
/// @brief
/// Returns the color for DOT_NOTIMPLEMENTED commands.
///
/////////////////////////////////////////////////////////////////////////////
QColor cEditorCtrl::GetNotImplementedColour(void) const
{
    return mNotImplementedColour;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears all highlight rectangles.
/// Called before regenerating highlights during repaint or viewport change.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::ClearHighlights(void)
{
    mBlockCoords.clear();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Syncs base class caret coordinates to Qt rectangle.
/// Called before rendering to ensure Qt rect matches base class state.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SyncCaretToQt(void)
{
    mCaretPosQt.setX(mCaretX);
    mCaretPosQt.setY(mCaretY);
    mCaretPosQt.setWidth(mCaretWidth);
    mCaretPosQt.setHeight(mCaretHeight);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  value [in] new scrollbar value (in twips)
///
/// @return nothing
///
/// @brief
/// Handles scrollbar value changes from user interaction.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnScrollbarChanged(int value)
{
    // Update scroll offset from scrollbar
    COORD_T newOffset = static_cast<COORD_T>(value);

    // Clamp to valid range
    if (newOffset < 0)
    {
        newOffset = 0;
    }

    if (mLayout)
    {
        COORD_T totalHeight = CalculateTotalDocumentHeight();
        COORD_T viewportHeight = GetViewportHeight();
        COORD_T maxScroll = totalHeight - viewportHeight;

        if (maxScroll < 0)
        {
            maxScroll = 0;
        }

        if (newOffset > maxScroll)
        {
            newOffset = maxScroll;
        }
    }

    // Update scroll offset
    mScrollOffset = newOffset;
    mViewport.scrollOffset = newOffset;

    // Recalculate viewport
    CalculateViewport();

    // Rebuild display list
    if (mLayout)
    {
        mDisplayList.Build(mViewport, mLayout);
    }

    // Recalculate caret position with new scroll offset
    CalculateCaretPosition();

    // Trigger repaint
    update();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QResizeEvent *event [in] - the resize event
///
/// @return nothing
///
/// @brief
/// Handles widget resize. Recalculates caret position and scrolls to
/// keep the caret visible after viewport dimensions change.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event) ;

    // Recalculate page scale for new window geometry, then do full visual update
    RecalculatePageScale() ;
    PerformVisualUpdate() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  event [in] wheel event
///
/// @return nothing
///
/// @brief
/// Handles mouse wheel scrolling.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::wheelEvent(QWheelEvent* event)
{
    // Guard: need layout
    if (!mLayout)
    {
        event->ignore();
        return;
    }

    // Get wheel delta
    QPoint angleDelta = event->angleDelta();

    if (angleDelta.y() == 0)
    {
        event->ignore();
        return;
    }

    // Calculate scroll amount
    int numDegrees = angleDelta.y() / 8;
    int numSteps = numDegrees / 15;

    COORD_T lineHeight = mLayout->GetAverageLineHeight();
    COORD_T scrollAmount = -numSteps * lineHeight;

    // Apply scroll (base class method handles everything)
    SetScrollOffset(mScrollOffset + scrollAmount);

    // Recalculate caret position with new scroll offset
    CalculateCaretPosition();

    // Sync scrollbar if present
    if (mScrollbar)
    {
        QSignalBlocker blocker(mScrollbar);
        mScrollbar->setValue(static_cast<int>(mScrollOffset));
    }

    event->accept();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  event [in] paint event
///
/// @return nothing
///
/// @brief
/// Main paint handler. Dispatches to appropriate display mode renderer.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::paintEvent(QPaintEvent* event)
{
    UNUSED_ARGUMENT(event);  // Qt passes event, but we repaint entire widget
    QPainter painter(this);

    // Check if this is a timer-triggered caret blink
    if (mDoDrawCaret)
    {
        painter.save();
        painter.scale(mPageScale, mPageScale);
        if (mDisplaySettings.mode == DISPLAY_PAGE)
        {
            painter.translate(mDisplaySettings.pageBorder, mDisplaySettings.pageBorder);
        }
        DrawCaret(painter);
        painter.restore();
        return;
    }

    // FULL DOCUMENT REPAINT
    mDrawnCaret = false;

    if (!mLayout)
    {
        return;
    }

    // During full layout (LayoutDocument), layout data structures are being
    // rebuilt and are not in a consistent state for rendering. Paint events
    // arrive here because the progress callback calls processEvents() to
    // update the status bar. Skip the full repaint -- the status bar update
    // is handled separately by mWordTsar->SetStatus().
    if (mLayout->InFullLayout())
    {
        return;
    }

    // Fill entire widget background first (pixel coordinates, no scaling)
    if (mDisplaySettings.mode == DISPLAY_PAGE)
    {
        // Page mode: desk area derives from paper color (darker neutral)
        QColor deskColor = QColor(
            static_cast<int>(mBGroundColour.red() * 0.5),
            static_cast<int>(mBGroundColour.green() * 0.5),
            static_cast<int>(mBGroundColour.blue() * 0.5));

        // Dim when this pane is inactive (reveal codes split)
        if (mSiblingEditor != nullptr && !mHasFocusDim)
        {
            deskColor = QColor(
                static_cast<int>(deskColor.red() * 0.65),
                static_cast<int>(deskColor.green() * 0.65),
                static_cast<int>(deskColor.blue() * 0.65));
        }
        painter.fillRect(rect(), deskColor);
    }
    else
    {
        // Continuous mode: use configured background color directly
        QColor bgColor = mBGroundColour;
        if (mSiblingEditor != nullptr && !mHasFocusDim)
        {
            bgColor = QColor(
                static_cast<int>(bgColor.red() * 0.65),
                static_cast<int>(bgColor.green() * 0.65),
                static_cast<int>(bgColor.blue() * 0.65));
        }
        painter.fillRect(rect(), bgColor);
    }


    // Initialize caret position if not yet set
    if (mCaretX == 0 && mCaretY == 0)
    {
        if (mLayout->GetNumberOfParagraphs() > 0)
        {
            // Use black box API instead of accessing paragraph->lines
            LINE_T firstLine = mLayout->GetFirstLineOfParagraph(0);
            if (firstLine >= 0)
            {
                mCaretX = mLayout->GetLineBaseX(firstLine);
                mCaretY = mLayout->GetLineScreenY(firstLine) - mScrollOffset;
                mCaretWidth = DEFAULT_CARET_WIDTH;
                mCaretHeight = mLayout->GetLineHeight(firstLine);
                mCaretLine = firstLine;
                AdjustCaretYForPageMode(mLayout->GetLineByRawLineNumber(firstLine));
            }
        }
    }

    // Sync base class coordinates to Qt rectangle
    SyncCaretToQt();

    // Convert viewport height to twips
    COORD_T viewportHeightTwips = GetViewportHeight();

    // Draw debug overlays FIRST (uses pixel coordinates, before scaling)
    if (mShowViewportDebug || mShowBoxStats)
    {
        painter.setRenderHint(QPainter::Antialiasing);

        QColor overlayBg(0, 0, 0, 200);
        QColor textColor(255, 255, 255);
        QColor highlightColor(0, 255, 0);

        // QFont font("Courier", 10);
        // painter.setFont(font);

        int startY = 10;
        int lineHeight = 20;
        int currentY = startY;

        const_cast<cEditorCtrl*>(this)->CalculateViewport();

        if (mShowViewportDebug)
        {
            painter.fillRect(10, startY - 5, 500, 200, overlayBg);

            painter.setPen(highlightColor);
            painter.drawText(15, currentY, "=== VIEWPORT DEBUG (Press V to hide) ===");
            currentY += lineHeight;

            painter.setPen(textColor);

            QString bounds = QString("Viewport Y Range: %1 to %2 twips")
                            .arg(mViewport.topY)
                            .arg(mViewport.bottomY);
            painter.drawText(15, currentY, bounds);
            currentY += lineHeight;

            QString height = QString("Viewport Height: %1 twips (%2 pixels)")
                            .arg(mViewport.viewportHeight)
                            .arg(this->height());
            painter.drawText(15, currentY, height);
            currentY += lineHeight;

            QString scroll = QString("Scroll Offset: %1 twips")
                            .arg(mViewport.scrollOffset);
            painter.drawText(15, currentY, scroll);
            currentY += lineHeight;

            QString boxCount = QString("Visible Boxes: %1 of %2 total")
                              .arg(mViewport.visibleBoxes.size())
                              .arg(mLayout->GetBoxCount());
            painter.drawText(15, currentY, boxCount);
            currentY += lineHeight;

            painter.setPen(highlightColor);
            painter.drawText(15, currentY, "First 5 visible boxes:");
            currentY += lineHeight;

            painter.setPen(textColor);
            int boxesToShow = std::min(5, (int)mViewport.visibleBoxes.size());
            for (int i = 0; i < boxesToShow; ++i)
            {
                const sBoxes* box = mViewport.visibleBoxes[i];
                QString boxInfo = QString("  Box %1: Y=[%2..%3] X=[%4..%5] Page %6")
                                 .arg(box->boxNumber)
                                 .arg(box->screenYTop)
                                 .arg(box->screenYBottom)
                                 .arg(box->left)
                                 .arg(box->right)
                                 .arg(box->pageNumber);
                painter.drawText(15, currentY, boxInfo);
                currentY += lineHeight;
            }

            currentY += lineHeight;
        }

        if (mShowBoxStats)
        {
            painter.fillRect(10, currentY - 5, 500, 180, overlayBg);

            painter.setPen(highlightColor);
            painter.drawText(15, currentY, "=== BOX STATISTICS (Press B to hide) ===");
            currentY += lineHeight;

            painter.setPen(textColor);

            int totalBoxes = mLayout->GetBoxCount();
            QString total = QString("Total Boxes: %1").arg(totalBoxes);
            painter.drawText(15, currentY, total);
            currentY += lineHeight;

            int pageCount = mLayout->GetNumberOfPages();
            double avgBoxesPerPage = (pageCount > 0) ? (double)totalBoxes / pageCount : 0;
            QString avg = QString("Average Boxes per Page: %1")
                         .arg(avgBoxesPerPage, 0, 'f', 2);
            painter.drawText(15, currentY, avg);
            currentY += lineHeight;

            std::map<PAGE_T, int> boxesPerPage;
            const std::vector<sBoxes>& allBoxes = mLayout->GetGlobalBoxList();
            for (const auto& box : allBoxes)
            {
                boxesPerPage[box.pageNumber]++;
            }

            painter.setPen(highlightColor);
            painter.drawText(15, currentY, "Boxes per page (first 5 pages):");
            currentY += lineHeight;

            painter.setPen(textColor);
            int pagesToShow = std::min(5, (int)boxesPerPage.size());
            auto it = boxesPerPage.begin();
            for (int i = 0; i < pagesToShow && it != boxesPerPage.end(); ++i, ++it)
            {
                QString pageInfo = QString("  Page %1: %2 boxes")
                                  .arg(it->first)
                                  .arg(it->second);
                painter.drawText(15, currentY, pageInfo);
                currentY += lineHeight;
            }

            currentY += lineHeight;
            painter.setPen(highlightColor);
            painter.drawText(15, currentY, "Document Statistics:");
            currentY += lineHeight;

            painter.setPen(textColor);
            QString lines = QString("  Total Lines: %1").arg(mLayout->GetNumberOfLines());
            painter.drawText(15, currentY, lines);
            currentY += lineHeight;

            QString paras = QString("  Total Paragraphs: %1").arg(mLayout->GetNumberOfParagraphs());
            painter.drawText(15, currentY, paras);
            currentY += lineHeight;
        }
    }

    // Apply scaling based on display mode
    painter.save();

    // Generate highlight rectangles for current viewport (before scaling)
    // This must be done once before rendering either mode
    GenerateHighlightRects();

    // Recalculate page scale for current window geometry and display mode
    RecalculatePageScale();

    if (mDisplaySettings.mode == DISPLAY_CONTINUOUS)
    {
        painter.scale(mPageScale, mPageScale);
        DrawContinuousMode(painter, viewportHeightTwips);
    }
    else
    {
        // Page mode: scale and translate for page borders
        COORD_T pageBorder = mDisplaySettings.pageBorder;
        painter.scale(mPageScale, mPageScale);
        painter.translate(pageBorder, pageBorder);

        DrawPageMode(painter, viewportHeightTwips);
    }

    // Draw box outlines if enabled (now using TWIPS coordinates with scaled painter)
    if (mShowBoxOutlines)
    {
        QPen pen(QColor(255, 0, 0), 10);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        if (mDisplaySettings.mode == DISPLAY_CONTINUOUS)
        {
            // Linear scan through all boxes (debug-only, no need for binary search)
            const std::vector<sBoxes>& allBoxes = mLayout->GetGlobalBoxList();
            COORD_T viewportTop = mScrollOffset;
            COORD_T viewportBottom = mScrollOffset + viewportHeightTwips;

            for (const auto& box : allBoxes)
            {
                // Convert page-relative box boundaries to continuous Y coordinates
                COORD_T boxContinuousTop = box.pageStartY + box.top;
                COORD_T boxContinuousBottom = box.pageStartY + box.bottom;

                // Skip boxes outside viewport
                if (boxContinuousBottom < viewportTop || boxContinuousTop > viewportBottom)
                {
                    continue;
                }

                // Convert to screen coordinates
                COORD_T boxLeft = box.left;
                COORD_T boxRight = box.right;
                COORD_T boxTop = boxContinuousTop - mScrollOffset;
                COORD_T boxBottom = boxContinuousBottom - mScrollOffset;

                painter.drawRect(QRectF(boxLeft, boxTop,
                                boxRight - boxLeft,
                                boxBottom - boxTop));

                // Draw box number label at top-left of the box
                painter.drawText(QPointF(boxLeft - 800, boxTop + 200),
                                QString("BOX %1").arg(box.boxNumber));

                // Draw box number label at bottom-left of the box
                painter.drawText(QPointF(boxLeft - 800, boxBottom - 50),
                                QString("EBOX %1").arg(box.boxNumber));
            }
        }
        else
        {
            const std::vector<sBoxes>& allBoxes = mLayout->GetGlobalBoxList();
            PAGE_T pageCount = mLayout->GetNumberOfPages();

            if (pageCount > 0)
            {
                COORD_T mPaperHeight = mLayout->GetPaperHeight();
                COORD_T viewportTop = mScrollOffset;
                COORD_T viewportBottom = mScrollOffset + viewportHeightTwips;
                COORD_T currentPageYOffset = 0;

                for (PAGE_T page = 1; page <= pageCount; ++page)
                {
                    COORD_T pageTop = currentPageYOffset;
                    COORD_T pageBottom = currentPageYOffset + mPaperHeight;

                    if (pageBottom >= viewportTop && pageTop <= viewportBottom)
                    {
                        // Draw boxes on this page
                        for (const auto& box : allBoxes)
                        {
                            if (box.pageNumber == page)
                            {
                                // Use actual box boundaries (box.top/bottom), not content boundaries
                                // box.top/bottom are page-relative coordinates
                                COORD_T boxDocTop = box.top + currentPageYOffset;
                                COORD_T boxDocBottom = box.bottom + currentPageYOffset;

                                if (boxDocBottom >= viewportTop && boxDocTop <= viewportBottom)
                                {
                                    COORD_T boxLeft = box.left;
                                    COORD_T boxRight = box.right;
                                    COORD_T boxTop = boxDocTop - mScrollOffset;
                                    COORD_T boxBottom = boxDocBottom - mScrollOffset;

                                    painter.drawRect(QRectF(boxLeft, boxTop,
                                                    boxRight - boxLeft,
                                                    boxBottom - boxTop));

                                    // Draw box number label at top-left of the box
                                    painter.drawText(QPointF(boxLeft - 100, boxTop + 20),
                                                    QString("BOX %1").arg(box.boxNumber));

                                    // Draw box number label at bottom-left of the box
                                    painter.drawText(QPointF(boxLeft - 100, boxBottom - 5),
                                                    QString("EBOX %1").arg(box.boxNumber));
                                }
                            }
                        }
                    }

                    currentPageYOffset += mPaperHeight + mDisplaySettings.pageGap;
                }
            }
        }
    }

    // Restore painter state (end of TWIPS drawing)
    painter.restore();

    // Draw caret if positioned (uses mPageScale to match document content)
    if (mCaretPosQt.width() > 0 && mCaretPosQt.height() > 0)
    {
        painter.save();
        painter.scale(mPageScale, mPageScale);
        if (mDisplaySettings.mode == DISPLAY_PAGE)
        {
            painter.translate(mDisplaySettings.pageBorder, mDisplaySettings.pageBorder);
        }

        QPainter::CompositionMode oldMode = painter.compositionMode();
        painter.setCompositionMode(QPainter::RasterOp_NotDestination);

        QColor caretColor(Qt::black);
        QBrush brush(caretColor);
        painter.fillRect(mCaretPosQt, brush);

        painter.setCompositionMode(oldMode);
        painter.restore();

        mDrawnCaret = true;
    }

    // Update ruler to reflect current caret position and layout settings
    UpdateRuler();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Update ruler display based on current caret position and layout settings.
/// Called from paintEvent() to sync ruler with current document state.
/// Uses BLACK BOX API to get correct position. Mirrors old editorctrl.cpp:5014-5031.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::UpdateRuler(void)
{
    // Guard: ruler, layout, or document not set
    if (!mRuler || !mLayout || !mDocument)
    {
        return;
    }

    // Get current document position from the source of truth
    POSITION_T currentPos = mDocument->GetPosition();

    // Find which line contains the caret
    LINE_T caretLineNum = mLayout->GetLineFromPosition(currentPos);
    if (caretLineNum < 0)
    {
        return;
    }

    // Hide ruler caret when on a dot command or comment line
    PARAGRAPH_T caretPara = mLayout->GetParagraphFromLine(caretLineNum);
    bool hideCaret = mLayout->ParagraphIsCommand(caretPara) || mLayout->ParagraphIsComment(caretPara);
    mRuler->ShowCaret(!hideCaret);

    // Get ABSOLUTE X coordinate using BLACK BOX API
    COORD_T absoluteX = mLayout->FindCoordInLine(currentPos, caretLineNum);

    // Convert tab stops from vector<sTabStop> to vector<COORD_T>
    const std::vector<sTabStop>& tabs = mLayout->GetTabs();
    std::vector<COORD_T> tabPositions;
    for (const auto& tab : tabs)
    {
        tabPositions.push_back(tab.position);
    }

    // Update ruler paper width based on current layout (same as old editorctrl.cpp:4433-4446)
    COORD_T mPaperWidth = mLayout->GetPaperWidth();
    switch(mMeasure)
    {
        case MSR_INCHES:
            mRuler->SetRuler(mPaperWidth / TWIPSPERINCH, MSR_INCHES);
            break;

        case MSR_MILLIMETERS:
            mRuler->SetRuler(mPaperWidth / TWIPSPERMM, MSR_MILLIMETERS);
            break;

        default:
            mRuler->SetRuler(mPaperWidth / TWIPSPERCM, MSR_CENTIMETERS);
            break;
    }

    // Update ruler with current layout settings (same as old editorctrl.cpp:5014-5017)
    mRuler->SetTabStops(tabPositions);
    mRuler->SetParagraph(mLayout->GetParagraphMargin());  // Global state - correct
    mRuler->SetRightMargin(mLayout->GetRightMargin());
    mRuler->SetLeftMargin(mLayout->GetLeftMargin());

    // Get page info
    PAGE_T pageNum = mLayout->GetLinePageNumber(caretLineNum);
    bool evenPage = (pageNum % 2 == 0);
    COORD_T pageOffset = evenPage ? mLayout->GetPageOffsetEven() : mLayout->GetPageOffsetOdd();
    COORD_T mRightMargin = mLayout->GetRightMargin();

    // Set page margins
    mRuler->SetPageMargins(pageOffset, pageOffset + mRightMargin);

    // Calculate RELATIVE position for ruler (same as old editorctrl.cpp:5022,5028)
    // absoluteX is page coordinate, subtract page offset and left margin to get text area position
    COORD_T relativeX = absoluteX - pageOffset - mLayout->GetLeftMargin();
    mRuler->SetPosition(relativeX);

    // Pass editor scale to ruler for page mode alignment
    // In page mode, the ruler needs to match the editor's scaling and page offset
    if (mDisplaySettings.mode == DISPLAY_PAGE)
    {
        mRuler->SetEditorScale(mPageScale, mDisplaySettings.pageBorder);
    }
    else
    {
        mRuler->SetEditorScale(0, 0);
    }

    // Trigger ruler repaint (same as old editorctrl.cpp:5031)
    mRuler->update();
}


/////////////////////////////////////////////////////////////////////////////
//
// RENDERING METHODS
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  viewportHeight [in] viewport height in twips
///
/// @return nothing
///
/// @brief
/// Draws all visible lines in continuous scrolling mode.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawContinuousMode(QPainter& painter, COORD_T viewportHeight)
{
    if (!mLayout)
    {
        return;
    }

    COORD_T viewportTop = mScrollOffset;
    COORD_T viewportBottom = mScrollOffset + viewportHeight;

    COORD_T mPaperWidth = mLayout->GetPaperWidth();
    COORD_T indicatorX = mPaperWidth + 50;

    // Create indicator font from base font (consistent regardless of document formatting)
    QFont indicatorFont = FontUtils::FontFromDescriptor(GetFont());
    indicatorFont.setPointSizeF(indicatorFont.pointSizeF() * FONTSCALE);

    PAGE_T lastPage = 0;

    // STEP 1: Draw dot command/comment backgrounds
    // BLACK BOX API: Use GetParagraphLayout() instead of direct mParagraphLayout access
    for (PARAGRAPH_T paraNum = 0; paraNum < mLayout->GetNumberOfParagraphs(); paraNum++)
    {
        // Ensure paragraph is laid out before drawing.
        // Skip during full layout: LayoutDocument sets mFullLayout=true and calls
        // processEvents via progress callback, which can dispatch paint events.
        // Calling LayoutParagraph here would corrupt layout state mid-iteration.
        if (!mLayout->InFullLayout() && !mLayout->IsParagraphLaidOut(paraNum))
        {
            mLayout->LayoutParagraph(paraNum);
        }

        const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(paraNum);
        if (!paragraph)
        {
            continue;
        }

        if (!paragraph->lines.empty())
        {
            const sLineLayout& firstLine = paragraph->lines.front();
            const sLineLayout& lastLine = paragraph->lines.back();

            if (lastLine.screeny >= viewportTop && firstLine.screeny <= viewportBottom)
            {
                if (paragraph->isCommand || paragraph->isComment)
                {
                    DrawDotCommandBackground(painter, *paragraph);
                }
            }
        }
    }

    // STEP 2: Draw text and indicators
    // Reset visible line tracking for page up/down
    mPageFirstVisibleLine = 0 ;
    mPageLastVisibleLine = 0 ;
    mPageFirstVisibleLineScrollY = 0 ;
    mPageLastVisibleLineScrollY = 0 ;
    bool firstLineFound = false ;

    // BLACK BOX API: Use GetParagraphLayout() instead of direct mParagraphLayout access
    for (PARAGRAPH_T paraNum = 0; paraNum < mLayout->GetNumberOfParagraphs(); paraNum++)
    {
        const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(paraNum);
        if (!paragraph)
        {
            continue;
        }

        // Draw lines and their inline indicators
        size_t lineCount = paragraph->lines.size();
        for (size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
        {
            const sLineLayout& line = paragraph->lines[lineIndex];

            // Check for page separator
            if (line.pagenumber != lastPage && lastPage != 0)
            {
                // Center separator in the gap (line.screeny includes full gap)
                COORD_T sepY = line.screeny - CONTINUOUS_PAGE_GAP / 2;

                if (sepY >= viewportTop && sepY <= viewportBottom)
                {
                    COORD_T lineY = sepY - mScrollOffset;

                    // Draw page separator line
                    QPen pen(mTextColour, 4);
                    painter.setPen(pen);
                    painter.drawLine(QPointF(0, lineY), QPointF(mPaperWidth, lineY));

                    // Draw "P" indicator centered on separator line
                    painter.setFont(indicatorFont);
                    QFontMetricsF fm(indicatorFont);
                    painter.drawText(QPointF(indicatorX, lineY + fm.ascent() / 2), "P");
                }
            }

            lastPage = line.pagenumber;

            if (line.screeny >= viewportTop && line.screeny <= viewportBottom)
            {
                // Track first/last visible line for page up/down
                if (!firstLineFound)
                {
                    mPageFirstVisibleLine = line.rawLineNumber ;
                    mPageFirstVisibleLineScrollY = line.screeny ;
                    firstLineFound = true ;
                }
                mPageLastVisibleLine = line.rawLineNumber ;
                mPageLastVisibleLineScrollY = line.screeny ;

                // Draw the line's text
                // Determine foreground override for dot command/comment paragraphs
                QColor lineFgOverride;
                if (paragraph->isComment)
                {
                    lineFgOverride = mCommentFgColour;
                }
                else if (paragraph->isCommand)
                {
                    switch (paragraph->dotStatus)
                    {
                        case DOT_ERROR:
                        {
                            lineFgOverride = mErrorFgColour;
                            break;
                        }
                        case DOT_UNKNOWN:
                        {
                            lineFgOverride = mUnknownFgColour;
                            break;
                        }
                        case DOT_NOTIMPLEMENTED:
                        {
                            lineFgOverride = mNotImplementedFgColour;
                            break;
                        }
                        default:
                        {
                            lineFgOverride = mDotFgColour;
                            break;
                        }
                    }
                }
                DrawLine(painter, line, lineFgOverride);

#ifdef DEBUG
                // Draw paragraph and line numbers in left margin
                {
                    COORD_T lineY = line.screeny - mScrollOffset;
                    painter.setPen(Qt::lightGray);
                    painter.setFont(indicatorFont);

                    // Paragraph number on first line only
                    if (lineIndex == 0)
                    {
                        QString pnum = QString("%1:").arg(paraNum);
                        painter.drawText(QPointF(10, lineY + line.lineheight), pnum);
                    }
                    // Line number on every line
                    QString lnum = QString("%1").arg(line.rawLineNumber);
                    painter.drawText(QPointF(550, lineY + line.lineheight), lnum);
                }
#endif

                // Draw inline indicators at this line's Y position
                // Honor mAlwaysFlag: "Always On" shows always, "Display with Command Tags"
                // shows only when control codes are visible (SHOW_ALL mode)
                bool showIndicators = mAlwaysFlag || (GetShowControls() == SHOW_ALL);
                if (showIndicators)
                {
                    COORD_T lineY = line.screeny - mScrollOffset;

                    if (paragraph->isCommand || paragraph->isComment)
                    {
                        // Draw "." or "?" for dot commands/comments (only on first line)
                        if (lineIndex == 0)
                        {
                            QString indicator;
                            if (paragraph->isComment || paragraph->dotStatus == DOT_GOOD)
                            {
                                indicator = ".";
                            }
                            else
                            {
                                indicator = "?";
                            }

                            painter.setFont(indicatorFont);
                            painter.setPen(mTextColour);
                            painter.drawText(QPointF(indicatorX, lineY + line.lineheight), indicator);
                        }
                    }
                    else if (lineIndex == lineCount - 1)
                    {
                        // Draw "CR-symbol" for end of regular paragraphs (last line only)
                        painter.setFont(indicatorFont);
                        painter.setPen(mTextColour);
                        painter.drawText(QPointF(indicatorX, lineY + line.lineheight), "↵");
                    }
                }
            }
        }
    }

    // STEP 3: Draw selection highlights LAST (on top of text and all backgrounds)
    DrawHighlights(painter);

    // Draw vertical line at right edge of page to separate content from indicators
    // Skip when indicator column is hidden
    {
        bool showIndicators = mAlwaysFlag || (GetShowControls() == SHOW_ALL);
        if (showIndicators)
        {
            QPen pen(mTextColour, 4);
            painter.setPen(pen);
            painter.drawLine(QPointF(mPaperWidth, 0), QPointF(mPaperWidth, viewportHeight));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  viewportHeight [in] viewport height in twips
///
/// @return nothing
///
/// @brief
/// Draws all visible pages in page view mode with gaps.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawPageMode(QPainter& painter, COORD_T viewportHeight)
{
    if (!mLayout)
    {
        return;
    }

    PAGE_T pageCount = mLayout->GetNumberOfPages();
    if (pageCount == 0)
    {
        return;
    }

    COORD_T viewportTop = mScrollOffset;
    COORD_T viewportBottom = mScrollOffset + viewportHeight;

    COORD_T mPaperWidth = mLayout->GetPaperWidth();
    COORD_T mPaperHeight = mLayout->GetPaperHeight();

    // Reset visible line tracking for page up/down
    mPageFirstVisibleLine = 0 ;
    mPageLastVisibleLine = 0 ;
    mPageFirstVisibleLineScrollY = 0 ;
    mPageLastVisibleLineScrollY = 0 ;
    bool firstLineFound = false ;

    COORD_T currentPageYOffset = 0;

    for (PAGE_T page = 1; page <= pageCount; ++page)
    {
        COORD_T pageTop = currentPageYOffset;
        COORD_T pageBottom = currentPageYOffset + mPaperHeight;

        if (pageBottom >= viewportTop && pageTop <= viewportBottom)
        {
            // Draw shadow first, then page on top to mask interior
            COORD_T viewportPageY = currentPageYOffset - mScrollOffset;
            DrawPageShadow(painter, viewportPageY,
                          mPaperWidth, mPaperHeight);
            DrawPageBackground(painter, page, viewportPageY,
                             mPaperWidth, mPaperHeight);

            DrawHeadersFooters(painter, page, viewportPageY);

            // Draw dot command/comment backgrounds
            // BLACK BOX API: Use GetParagraphLayout() instead of direct mParagraphLayout access
            for (PARAGRAPH_T paraNum = 0; paraNum < mLayout->GetNumberOfParagraphs(); paraNum++)
            {
                const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(paraNum);
                if (!paragraph)
                {
                    continue;
                }

                if (!paragraph->lines.empty() && paragraph->lines.front().pagenumber == page)
                {
                    const sLineLayout& firstLine = paragraph->lines.front();
                    const sLineLayout& lastLine = paragraph->lines.back();

                    COORD_T firstScreenY = firstLine.pagey + currentPageYOffset;
                    COORD_T lastScreenY = lastLine.pagey + currentPageYOffset;

                    if (lastScreenY >= viewportTop && firstScreenY <= viewportBottom)
                    {
                        if (paragraph->isCommand || paragraph->isComment)
                        {
                            sParagraphLayout translatedPara = *paragraph;
                            for (auto& line : translatedPara.lines)
                            {
                                line.screeny = line.pagey + currentPageYOffset;
                            }

                            DrawDotCommandBackground(painter, translatedPara);
                        }
                    }
                }
            }

            // Draw text
            // BLACK BOX API: Use GetParagraphLayout() instead of direct mParagraphLayout access
            for (PARAGRAPH_T paraNum = 0; paraNum < mLayout->GetNumberOfParagraphs(); paraNum++)
            {
                const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(paraNum);
                if (!paragraph)
                {
                    continue;
                }

                for (const auto& line : paragraph->lines)
                {
                    if (line.pagenumber == page)
                    {
                        COORD_T screenY = line.pagey + currentPageYOffset;

                        if (screenY >= viewportTop && screenY <= viewportBottom)
                        {
                            // Track first/last visible line for page up/down
                            if (!firstLineFound)
                            {
                                mPageFirstVisibleLine = line.rawLineNumber ;
                                mPageFirstVisibleLineScrollY = screenY ;
                                firstLineFound = true ;
                            }
                            mPageLastVisibleLine = line.rawLineNumber ;
                            mPageLastVisibleLineScrollY = screenY ;

                            sLineLayout translatedLine = line;
                            translatedLine.screeny = screenY;

                            // Determine foreground override for dot command/comment paragraphs
                            QColor lineFgOverride;
                            if (paragraph->isComment)
                            {
                                lineFgOverride = mCommentFgColour;
                            }
                            else if (paragraph->isCommand)
                            {
                                switch (paragraph->dotStatus)
                                {
                                    case DOT_ERROR:
                                    {
                                        lineFgOverride = mErrorFgColour;
                                        break;
                                    }
                                    case DOT_UNKNOWN:
                                    {
                                        lineFgOverride = mUnknownFgColour;
                                        break;
                                    }
                                    case DOT_NOTIMPLEMENTED:
                                    {
                                        lineFgOverride = mNotImplementedFgColour;
                                        break;
                                    }
                                    default:
                                    {
                                        lineFgOverride = mDotFgColour;
                                        break;
                                    }
                                }
                            }
                            DrawLine(painter, translatedLine, lineFgOverride);
                        }
                    }
                }
            }

            // Draw selection highlights LAST (on top of text and all backgrounds)
            // Clip highlights to text content area of this page so they don't
            // extend into margins or page gaps between pages
            const std::vector<sBoxes>& boxList = mLayout->GetGlobalBoxList();
            COORD_T contentTop = viewportPageY;
            COORD_T contentBottom = viewportPageY + mPaperHeight;

            for (const auto& box : boxList)
            {
                if (box.pageNumber == page)
                {
                    // box.top/bottom are page-relative Y of text content area
                    contentTop = viewportPageY + box.top;
                    contentBottom = viewportPageY + box.bottom;
                    break;
                }
            }

            painter.save();
            painter.setClipRect(QRectF(0, contentTop, mPaperWidth, contentBottom - contentTop));
            DrawHighlights(painter);
            painter.restore();
        }

        currentPageYOffset += mPaperHeight + mDisplaySettings.pageGap;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  line [in] line to draw
///
/// @return nothing
///
/// @brief
/// Draws a single line at its screeny position.
/// Painter is already scaled to TWIPS by paintEvent().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawLine(QPainter& painter, const sLineLayout& line, const QColor& fgOverride)
{
    COORD_T lineY = line.screeny - mScrollOffset;

    // Compute max ascent across all segments for baseline alignment
    // All segments on a line share the same baseline = lineY + maxAscent
    COORD_T maxAscent = 0;
    for (const auto& seg : line.segments)
    {
        if (seg.position.empty() || seg.GetGraphemeCount() == 0)
        {
            continue;
        }
        QFont f = FontUtils::FontFromDescriptor(seg.font);
        f.setPointSizeF(f.pointSizeF() * FONTSCALE);
        QFontMetricsF fm(f);
        if (fm.ascent() > maxAscent)
        {
            maxAscent = fm.ascent();
        }
    }

    for (size_t i = 0; i < line.segments.size(); ++i)
    {
        const auto& segment = line.segments[i];

        // All segments use line.pagex as base coordinate
        // Their position[] arrays already contain correct continuous offsets
        DrawSegment(painter, segment, line.pagex, lineY, maxAscent, line.pagenumber, fgOverride);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  segment [in] segment to check for control codes
/// @param  graphemes [in] graphemes for the segment
/// @param  lineX [in] line X position in TWIPS
/// @param  lineY [in] line Y position in TWIPS (screen coordinates)
///
/// @return nothing
///
/// @brief
/// Draws rounded rectangles behind control codes in a segment.
/// Called BEFORE drawing the segment text.
///
/// This is data-driven: the layout phase sets hasControlCodes flag
/// only when SHOW_ALL is active. When SHOW_DOT or SHOW_NONE, control
/// codes are skipped during layout and this flag is never set.
///
/// Control codes are marked with MARKER_CHAR (127) and are identified
/// by segment.hasControlCodes and segment.controlCodeIndices.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawControlCodeBackgrounds(QPainter& painter, const sSegmentLayout& segment, const std::vector<std::string>& graphemes, COORD_T lineX, COORD_T lineY)
{
    if (!segment.hasControlCodes || segment.controlCodeIndices.empty())
    {
        return;
    }

    if (graphemes.empty())
    {
        return;
    }

    QBrush brush(mHighlightColour);
    painter.setBrush(brush);

    // Add outline in background color
    QPen pen(mBGroundColour, 2);  // 2 pixel width outline
    painter.setPen(pen);

    for (size_t idx : segment.controlCodeIndices)
    {
        if (idx >= segment.position.size() || idx >= graphemes.size())
        {
            continue;
        }

        COORD_T glyphX = lineX + segment.position[idx];
        COORD_T glyphWidth;

        if (idx + 1 < segment.position.size())
        {
            glyphWidth = segment.position[idx + 1] - segment.position[idx];
        }
        else
        {
            // Last glyph in segment
            if (segment.isTab && segment.tabWidth > 0)
            {
                // Expanding tab (TAB_TAB/TAB_DECIMAL): use tabWidth for background
                glyphWidth = segment.tabWidth;
            }
            else if (idx == 0)
            {
                glyphWidth = segment.totalWidth;
            }
            else if (idx > 0 && idx < segment.position.size())
            {
                glyphWidth = segment.totalWidth - (segment.position[idx] - segment.position[0]);
            }
            else
            {
                glyphWidth = 200;
            }
        }

        COORD_T glyphHeight = segment.segmentheight;

        QRectF rect(glyphX, lineY, glyphWidth, glyphHeight);
        painter.drawRoundedRect(rect, 30.0, 30.0);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  segment [in] segment to draw
/// @param  lineX [in] line X position
/// @param  lineY [in] line Y position
///
/// @return nothing
///
/// @brief
/// Draws a segment with its glyphs.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawSegment(QPainter& painter, const sSegmentLayout& segment, COORD_T lineX, COORD_T lineY, COORD_T maxAscent, PAGE_T pageNumber, const QColor& fgOverride)
{
    if (!mDocument || segment.position.empty() || segment.GetGraphemeCount() == 0)
    {
        return;
    }

    // Fetch graphemes from document (MARKER_CHAR bytes converted at render time)
    std::vector<std::string> graphemes;
    segment.GetGraphemes(mDocument, graphemes);

    if (graphemes.empty())
    {
        return;
    }

    // Draw control code backgrounds FIRST (before text)
    DrawControlCodeBackgrounds(painter, segment, graphemes, lineX, lineY);

    // Get paragraph start position (for MARKER_CHAR document position lookup)
    POSITION_T paragraphStart = 0;
    POSITION_T paragraphEnd = 0;
    mDocument->GetParagraphStartandEnd(segment.paragraph, paragraphStart, paragraphEnd);

    // Load font from segment (already has correct size from layout phase)
    QFont font;
    if (!segment.font.empty())
    {
        font = FontUtils::FontFromDescriptor(segment.font);
    }

    // Apply screen scaling only
    font.setPointSizeF(font.pointSizeF() * FONTSCALE);
    painter.setFont(font);

    // Set text color: fgOverride (dot command/comment overlay) takes priority,
    // then check if segment has the default sentinel (-1,-1,-1) meaning
    // "use editor's configured text color", otherwise use explicit document color.
    QColor textColor;
    if (fgOverride.isValid())
    {
        textColor = fgOverride;
    }
    else if (segment.textcolor.IsDefault())
    {
        // No explicit color -- use the editor's configured text color
        textColor = mTextColour;
    }
    else
    {
        // Document has explicit color for this segment -- use it
        textColor = QColor(
            segment.textcolor.red,
            segment.textcolor.green,
            segment.textcolor.blue,
            segment.textcolor.alpha
        );
    }
    painter.setPen(textColor);

    for (size_t i = 0; i < graphemes.size(); ++i)
    {
        COORD_T x = lineX + segment.position[i];

        // Calculate absolute document position for this grapheme
        POSITION_T docPos = paragraphStart + segment.startPosition + i;

        // Y positioning: Qt drawText(QPointF) uses Y as BASELINE coordinate
        // All segments share the same baseline (lineY + maxAscent) so mixed fonts align
        COORD_T y = lineY + maxAscent;

        // Get sub/super roll from document (default 90 twips = 3/48ths inch)
        COORD_T rollAmount = mLayout->GetSubSuperRoll();

        if (segment.isSubscript)
        {
            y += rollAmount;  // Move down by .SR value
        }
        else if (segment.isSuperscript)
        {
            y -= rollAmount;  // Move up by .SR value
        }

        // Convert control codes and block markers to displayable characters
        std::string displayGrapheme = graphemes[i];
        bool isControlCode = false;

        if (!graphemes[i].empty() && (graphemes[i][0] == MARKER_CHAR || graphemes[i][0] == REPLACE_CHAR || graphemes[i][0] == SAVE_CHAR))
        {
            isControlCode = true;

            // Draw background for variables (half-intensity highlight)
            if (graphemes[i][0] == MARKER_CHAR &&
                mDocument->GetControlChar(docPos) == STYLE_VARIABLE)
            {
                // Calculate width of this glyph position
                COORD_T glyphWidth;
                if (i + 1 < segment.position.size())
                {
                    glyphWidth = segment.position[i + 1] - segment.position[i];
                }
                else
                {
                    glyphWidth = segment.totalWidth - segment.position[i];
                }

                // Draw background at half intensity of control code highlight
                QColor varColor = mHighlightColour;
                varColor.setAlpha(mHighlightColour.alpha() / 2);

                painter.save();
                painter.setPen(Qt::NoPen);
                painter.setBrush(varColor);
                QRectF rect(x, lineY, glyphWidth, segment.segmentheight);
                painter.drawRoundedRect(rect, 30.0, 30.0);
                painter.restore();
            }

            // Get display character (centralized logic handles conversion)
            displayGrapheme = mLayout->GetDisplayCharacter(docPos, graphemes[i], pageNumber);

            // Use control code foreground color for this character
            painter.setPen(mHighlightFgColour);
        }

        QString glyph = QString::fromStdString(displayGrapheme);
        painter.drawText(QPointF(x, y), glyph);

        // Restore text color after control code character
        if (isControlCode)
        {
            painter.setPen(textColor);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  pageNumber [in] page number
/// @param  pageYOffset [in] page Y offset in TWIPS
/// @param  mPaperWidth [in] paper width in TWIPS
/// @param  mPaperHeight [in] paper height in TWIPS
///
/// @return nothing
///
/// @brief
/// Draws page background. Painter is already scaled to TWIPS by paintEvent().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawPageBackground(
    QPainter& painter,
    PAGE_T pageNumber,
    COORD_T pageYOffset,
    COORD_T mPaperWidth,
    COORD_T mPaperHeight)
{
    UNUSED_ARGUMENT(pageNumber);

    // Round the paper corners
    COORD_T cornerRadius = 30;

    // Page paper uses the configured background color (the user's "normal text" bg)
    QColor pageColor = mBGroundColour;

    // Dim the page paper when this pane is inactive (reveal codes split)
    if (mSiblingEditor != nullptr && !mHasFocusDim)
    {
        pageColor = QColor(
            static_cast<int>(pageColor.red() * 0.65),
            static_cast<int>(pageColor.green() * 0.65),
            static_cast<int>(pageColor.blue() * 0.65));
    }

    QPainterPath pagePath;
    pagePath.addRoundedRect(QRectF(0, pageYOffset, mPaperWidth, mPaperHeight),
                            cornerRadius, cornerRadius);
    painter.fillPath(pagePath, pageColor);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  pageYOffset [in] page Y offset in TWIPS
/// @param  mPaperWidth [in] paper width in TWIPS
/// @param  mPaperHeight [in] paper height in TWIPS
///
/// @return nothing
///
/// @brief
/// Draws page shadow. Painter is already scaled to TWIPS by paintEvent().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawPageShadow(
    QPainter& painter,
    COORD_T pageYOffset,
    COORD_T mPaperWidth,
    COORD_T mPaperHeight)
{
    // Shadow is drawn as a gradient outline around a box that is smaller
    // than the page and offset so its right/bottom edges align with the
    // paper edges. The white page drawn on top covers the interior,
    // leaving a full shadow on right/bottom and a subtle shadow on top/left.

    // Shadow widths in screen pixels (zoom-independent)
    double subtlePixels = 4.0;
    double strongPixels = 11.0;
    COORD_T subtleWidth = static_cast<COORD_T>(subtlePixels / mPageScale);
    COORD_T strongWidth = static_cast<COORD_T>(strongPixels / mPageScale);
    int strongAlpha = 110;          // gradient opacity

    COORD_T offset = strongWidth - subtleWidth;

    // Shadow box position and size (smaller than page, offset to bottom-right)
    COORD_T shadowX = offset;
    COORD_T shadowY = pageYOffset + offset;
    COORD_T shadowW = mPaperWidth - offset;
    COORD_T shadowH = mPaperHeight - offset;

    // Full outer bounds of the shadow
    COORD_T outerLeft = shadowX - strongWidth;
    COORD_T outerTop = shadowY - strongWidth;
    COORD_T outerRight = shadowX + shadowW + strongWidth;

    QColor shadowStart(0, 0, 0, strongAlpha);
    QColor shadowEnd(0, 0, 0, 0);

    // Top strip: shadow box width only (corners handled separately)
    QLinearGradient topGrad(0, shadowY, 0, outerTop);
    topGrad.setColorAt(0.0, shadowStart);
    topGrad.setColorAt(1.0, shadowEnd);
    painter.fillRect(QRectF(shadowX, outerTop,
                            shadowW, strongWidth), topGrad);

    // Bottom strip: shadow box width only
    QLinearGradient bottomGrad(0, shadowY + shadowH,
                               0, shadowY + shadowH + strongWidth);
    bottomGrad.setColorAt(0.0, shadowStart);
    bottomGrad.setColorAt(1.0, shadowEnd);
    painter.fillRect(QRectF(shadowX, shadowY + shadowH,
                            shadowW, strongWidth), bottomGrad);

    // Left strip: shadow box height only
    QLinearGradient leftGrad(shadowX, 0, outerLeft, 0);
    leftGrad.setColorAt(0.0, shadowStart);
    leftGrad.setColorAt(1.0, shadowEnd);
    painter.fillRect(QRectF(outerLeft, shadowY,
                            strongWidth, shadowH), leftGrad);

    // Right strip: shadow box height only
    QLinearGradient rightGrad(shadowX + shadowW, 0,
                              outerRight, 0);
    rightGrad.setColorAt(0.0, shadowStart);
    rightGrad.setColorAt(1.0, shadowEnd);
    painter.fillRect(QRectF(shadowX + shadowW, shadowY,
                            strongWidth, shadowH), rightGrad);

    // Radial gradient corners with adjustable squareness
    // Corner squareness: 1.0 = fully round, higher = squarer
    double cornerSquareness = 1.33;
    COORD_T cornerRadius = static_cast<COORD_T>(strongWidth * cornerSquareness);
    double fadeStop = static_cast<double>(strongWidth) / cornerRadius;

    // Top-left corner
    QRadialGradient tlCorner(shadowX, shadowY, cornerRadius);
    tlCorner.setColorAt(0.0, shadowStart);
    tlCorner.setColorAt(fadeStop, shadowEnd);
    painter.fillRect(QRectF(outerLeft, outerTop,
                            strongWidth, strongWidth), tlCorner);

    // Top-right corner
    QRadialGradient trCorner(shadowX + shadowW, shadowY, cornerRadius);
    trCorner.setColorAt(0.0, shadowStart);
    trCorner.setColorAt(fadeStop, shadowEnd);
    painter.fillRect(QRectF(shadowX + shadowW, outerTop,
                            strongWidth, strongWidth), trCorner);

    // Bottom-left corner
    QRadialGradient blCorner(shadowX, shadowY + shadowH, cornerRadius);
    blCorner.setColorAt(0.0, shadowStart);
    blCorner.setColorAt(fadeStop, shadowEnd);
    painter.fillRect(QRectF(outerLeft, shadowY + shadowH,
                            strongWidth, strongWidth), blCorner);

    // Bottom-right corner
    QRadialGradient brCorner(shadowX + shadowW, shadowY + shadowH,
                             cornerRadius);
    brCorner.setColorAt(0.0, shadowStart);
    brCorner.setColorAt(fadeStop, shadowEnd);
    painter.fillRect(QRectF(shadowX + shadowW, shadowY + shadowH,
                            strongWidth, strongWidth), brCorner);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  page [in] page number
/// @param  pageScreenY [in] page screen Y offset
///
/// @return nothing
///
/// @brief
/// Draws headers and footers for a page.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawHeadersFooters(
    QPainter& painter,
    PAGE_T page,
    COORD_T pageScreenY)
{
    if (!mLayout)
    {
        return;
    }

    const auto& allHeaders = mLayout->GetPageHeaders();
    const auto& allFooters = mLayout->GetPageFooters();

    auto headerIt = allHeaders.find(page);
    if (headerIt != allHeaders.end())
    {
        for (const auto& hfLine : headerIt->second)
        {
            COORD_T screenY = hfLine.line.pagey + pageScreenY;
            DrawHeaderFooterLine(painter, hfLine, screenY);
        }
    }

    auto footerIt = allFooters.find(page);
    if (footerIt != allFooters.end())
    {
        for (const auto& hfLine : footerIt->second)
        {
            COORD_T screenY = hfLine.line.pagey + pageScreenY;
            DrawHeaderFooterLine(painter, hfLine, screenY);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  hfLine [in] header/footer line with pre-rendered graphemes
/// @param  screenY [in] screen Y position in TWIPS
///
/// @return nothing
///
/// @brief
/// Draws a header or footer line using its stored graphemes.
/// Unlike body text, header/footer text is not in the document model,
/// so graphemes are stored separately rather than read via GetGraphemes().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawHeaderFooterLine(QPainter& painter, const sHeaderFooterLine& hfLine, COORD_T screenY)
{
    const sLineLayout& line = hfLine.line;

    if (line.segments.empty() || hfLine.graphemes.empty())
    {
        return;
    }

    // Header/footer lines have a single segment
    const sSegmentLayout& segment = line.segments[0];

    // Load font
    QFont font;
    if (!segment.font.empty())
    {
        font = FontUtils::FontFromDescriptor(segment.font);
    }
    font.setPointSizeF(font.pointSizeF() * FONTSCALE);
    painter.setFont(font);

    QFontMetricsF metrics(font);
    COORD_T ascent = metrics.ascent();

    // Use editor's configured text color for default sentinel,
    // otherwise use explicit document color
    QColor textColor;
    if (segment.textcolor.IsDefault())
    {
        textColor = mTextColour;
    }
    else
    {
        textColor = QColor(
            segment.textcolor.red,
            segment.textcolor.green,
            segment.textcolor.blue,
            segment.textcolor.alpha
        );
    }
    painter.setPen(textColor);

    // Draw each grapheme at its pre-calculated position
    for (size_t i = 0; i < hfLine.graphemes.size() && i < segment.position.size(); ++i)
    {
        COORD_T x = line.pagex + segment.position[i];
        COORD_T y = screenY + ascent;

        QString glyph = QString::fromStdString(hfLine.graphemes[i]);
        painter.drawText(QPointF(x, y), glyph);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
/// @param  paragraph [in] paragraph layout
///
/// @return nothing
///
/// @brief
/// Draws colored background for dot commands and comments.
/// Called BEFORE drawing paragraph text.
///
/// This is data-driven: the layout phase sets isCommand/isComment flags
/// regardless of ShowControl mode, and only creates visible lines when
/// SHOW_ALL or SHOW_DOT is active. Therefore, this function only draws
/// backgrounds when the paragraph has visible lines.
///
/// Color depends on dotStatus:
/// - DOT_GOOD: light green
/// - DOT_ERROR: light red (parse failed)
/// - DOT_UNKNOWN: yellow (not in WordStar spec)
/// - DOT_NOTIMPLEMENTED: orange (known but not implemented)
/// - Comment: light blue
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawDotCommandBackground(QPainter& painter, const sParagraphLayout& paragraph)
{
    if (!paragraph.isCommand && !paragraph.isComment)
    {
        return;
    }

    if (paragraph.lines.empty())
    {
        return;
    }

    const sLineLayout& firstLine = paragraph.lines.front();
    const sLineLayout& lastLine = paragraph.lines.back();

    COORD_T x = firstLine.pagex;
    COORD_T y = firstLine.screeny - mScrollOffset;
    COORD_T width = paragraph.boxRight - x;  // Use right margin stored at layout time, not current global
    COORD_T height = (lastLine.screeny + lastLine.lineheight) - firstLine.screeny;

    QBrush brush;

    if (paragraph.isComment)
    {
        brush.setColor(mCommentColour);
    }
    else if (paragraph.dotStatus == DOT_ERROR)
    {
        brush.setColor(mErrorColour);
    }
    else if (paragraph.dotStatus == DOT_UNKNOWN)
    {
        brush.setColor(mUnknownColour);
    }
    else if (paragraph.dotStatus == DOT_NOTIMPLEMENTED)
    {
        brush.setColor(mNotImplementedColour);
    }
    else
    {
        brush.setColor(mDotColour);
    }

    brush.setStyle(Qt::SolidPattern);  // Set brush style to solid fill

    painter.setBrush(brush);

    // Add outline in background color
    QPen pen(mBGroundColour, 2);  // 2 pixel width outline
    painter.setPen(pen);

    QRectF rect(x, y, width, height);
    painter.drawRoundedRect(rect, 30.0, 30.0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in] QPainter for drawing
///
/// @return nothing
///
/// @brief
/// Draws or erases the caret using XOR composition mode.
/// Caller must apply TWIPS scaling before calling.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawCaret(QPainter& painter)
{
    // Sync base class coordinates to Qt rectangle
    SyncCaretToQt();

    QPainter::CompositionMode oldMode = painter.compositionMode();
    painter.setCompositionMode(QPainter::RasterOp_NotDestination);

    QColor caretColor(Qt::black);
    QBrush brush(caretColor);
    QPen pen(caretColor);
    painter.setBrush(brush);
    painter.setPen(pen);

    if (mDrawnCaret)
    {
        if (hasFocus())
        {
            painter.fillRect(mCaretPosQt, brush);
            mDrawnCaret = false;
        }
    }
    else
    {
        painter.fillRect(mCaretPosQt, brush);
        mDrawnCaret = true;
    }

    painter.setCompositionMode(oldMode);

    mDoDrawCaret = false;

    mCaretTimer->start(mCaretBlinkRate);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Timer callback to toggle caret visibility.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnCaretTimer(void)
{
    mDoDrawCaret = true;
    update();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Timer callback to auto-save backup file.
/// Called every AUTO_SAVE_INTERVAL_MS (60 seconds) to save backup.
/// Same logic as old editorctrl.cpp OnStatusTimer() line 1529-1566.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnAutoSaveTimer(void)
{
    // Skip while document/layout is being mutated to avoid racing reads.
    if (IsBusy())
    {
        return;
    }

    // Only save if we have a filename and backup filename
    if (mFileName.empty() || mBackupFileName.empty())
    {
        return;
    }

    // Show backup status on status bar
    if (mWordTsar)
    {
        mWordTsar->SetStatus("Saving backup...", true, 0);
    }

    // Always save backups in WordStar format -- avoids hangs from RTF/DOCX backup loading
    cFile* fileWriter = new cWordstarFile(this);

    POSITION_T docSize = mDocument->GetTextSize();
    fileWriter->SaveFile(mBackupFileName, docSize);

    delete fileWriter;

    // Show completion briefly
    if (mWordTsar)
    {
        mWordTsar->SetStatus("Backup saved", false, 0);
    }

    // Compact memory after backup (matching old OnStatusTimer behavior)
    mDocument->ShrinkToFit();
    if (mLayout)
    {
        mLayout->ShrinkToFit();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Background layout timer callback. Checks if idle layout should run and
/// processes one paragraph at a time. This is the main entry point for the
/// background layout system.
///
/// Safety checks prevent layout during:
/// - Document loading
/// - Full layout operation (would conflict)
/// - Empty documents
/// - Help panels
///
/// Uses on-demand scheduling: reschedules itself with QTimer::singleShot(0)
/// if more work remains, otherwise stops naturally (matching old behavior).
///
/// Ported from old editorctrl.cpp OnIdle() lines 1463-1495.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::OnIdle(void)
{
    // SAFETY CHECKS (matches old editorctrl.cpp lines 1465-1486)

    // Don't layout while loading file
    if (mDocument->GetLoading())
    {
        return;
    }

    // Don't interfere with full layout
    if (mLayout->InFullLayout())
    {
        return;
    }

    // Empty document
    if (mDocument->GetTextSize() == 0)
    {
        return;
    }

    // PROCESS ONE PARAGRAPH
    // IdleLayout() returns true if more work remains (matches old line 1488)
    bool ret = IdleLayout();

    // RESCHEDULE IF MORE WORK (matches old editorctrl.cpp lines 1490-1493)
    // Uses 0ms delay for aggressive continuous processing
    // If ret is false, timer stops naturally (no reschedule)
    if (ret)
    {
        QTimer::singleShot(0, this, SLOT(OnIdle()));
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return PARAGRAPH_T - last visible paragraph number based on existing layout
///
/// @brief
/// Find the last paragraph that is visible in the current viewport.
/// Uses EXISTING layout data to determine visibility - does not layout
/// any paragraphs. Used to determine the range of paragraphs to layout
/// during keypress handling.
///
/// Page mode uses pagey + page offset (stacked-page coordinates) to match
/// DrawPageMode(), AdjustCaretYForPageMode(), and ScreenYToViewportY().
/// Continuous mode uses screeny directly (compact coordinates match
/// mScrollOffset in that mode).
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cEditorCtrl::GetLastVisibleParagraph(void)
{
    if (!mLayout || !mDocument)
    {
        return 0;
    }

    // Clamp to minimum of layout and document counts.
    // After cross-paragraph deletes, layout may have stale entries
    // beyond the current document paragraph count.
    PARAGRAPH_T totalParagraphs = std::min(
        mLayout->GetNumberOfParagraphs(),
        mDocument->GetNumberofParagraphs()) ;
    PARAGRAPH_T lastVisible = 0;
    bool foundFirst = false;

    // Ensure mPageScale is current for viewport height calculation
    RecalculatePageScale();

    COORD_T viewportTop = mScrollOffset;
    COORD_T viewportHeight = static_cast<COORD_T>(height() / mPageScale);
    COORD_T viewportBottom = viewportTop + viewportHeight;

    if (mDisplaySettings.mode == DISPLAY_PAGE)
    {
        // Page mode: use pagey + page offset (stacked-page coordinates).
        // screeny is in a different (compact) coordinate space and cannot
        // be compared against mScrollOffset which is in stacked-page space.
        PAGE_T pageCount = mLayout->GetNumberOfPages();
        COORD_T mPaperHeight = mLayout->GetPaperHeight();
        COORD_T pageGap = mDisplaySettings.pageGap;
        COORD_T currentPageYOffset = 0;

        for (PAGE_T page = 1; page <= pageCount; ++page)
        {
            COORD_T pageTop = currentPageYOffset;
            COORD_T pageBottom = currentPageYOffset + mPaperHeight;

            if (pageBottom >= viewportTop && pageTop <= viewportBottom)
            {
                // This page is visible -- find paragraphs on it
                for (PARAGRAPH_T para = 0; para < totalParagraphs; ++para)
                {
                    const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(para);
                    if (!paragraph || paragraph->lines.empty())
                    {
                        continue;
                    }

                    if (paragraph->lines.front().pagenumber == page)
                    {
                        COORD_T firstScreenY = paragraph->lines.front().pagey + currentPageYOffset;
                        COORD_T lastScreenY = paragraph->lines.back().pagey + currentPageYOffset;

                        if (lastScreenY >= viewportTop && firstScreenY <= viewportBottom)
                        {
                            if (!foundFirst)
                            {
                                mVisibleStart = para;
                                foundFirst = true;
                            }
                            lastVisible = para;
                        }
                    }
                }
            }
            else if (pageTop > viewportBottom)
            {
                // Past viewport, stop searching
                break;
            }

            currentPageYOffset += mPaperHeight + pageGap;
        }
    }
    else
    {
        // Continuous mode: screeny matches mScrollOffset coordinate space
        for (PARAGRAPH_T para = 0; para < totalParagraphs; ++para)
        {
            const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(para);
            if (!paragraph || paragraph->lines.empty())
            {
                continue;
            }

            const sLineLayout& firstLine = paragraph->lines.front();
            const sLineLayout& lastLine = paragraph->lines.back();

            // Check if paragraph overlaps viewport
            if (lastLine.screeny >= viewportTop && firstLine.screeny <= viewportBottom)
            {
                // Track first visible paragraph
                if (!foundFirst)
                {
                    mVisibleStart = para;
                    foundFirst = true;
                }
                lastVisible = para;
            }
            else if (firstLine.screeny > viewportBottom)
            {
                // Past viewport, stop searching
                break;
            }
        }
    }

    return lastVisible;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if more paragraphs to layout, false if complete
///
/// @brief
/// Layout one paragraph during idle time. Implements background layout logic
/// with multi-layer resumption support. Returns true if more work remains,
/// false if background layout is complete.
///
/// Simple approach (no equality-based stopping): Always layouts paragraphs
/// sequentially, ensuring complete document formatting. Handles interrupt
/// stack for multi-layer resumption (Requirement #3).
///
/// Progress indicator updates every 10% (Requirement #4).
///
/// Ported from old editorctrl.cpp IdleLayout() lines 1576-1621.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::IdleLayout(void)
{
    // Check if document was changed (possibly by another editor via listener notification)
    // Only rewind if the change is before our current layout position
    if (mDocumentDirty)
    {
        if (mDirtyFromParagraph < mLayoutParagraph)
        {
            mLayoutParagraph = mDirtyFromParagraph ;
        }
        mDocumentDirty = false ;
    }

    PARAGRAPH_T totalParagraphs = mDocument->GetNumberofParagraphs();

    // Debug: Track when layout starts from a new position
    static PARAGRAPH_T lastStartParagraph = -1;
    if (mLayoutParagraph == 0 || lastStartParagraph != mLayoutParagraph)
    {
        // printf("[IDLE] START layout from paragraph %ld/%ld\n", mLayoutParagraph, totalParagraphs);
        lastStartParagraph = mLayoutParagraph;
    }

    // CHECK IF DONE (reached end of document)
    if (mLayoutParagraph >= totalParagraphs)
    {
        // Check for saved positions to resume from (Requirement #3: Multi-layer resumption)
        if (!mInterruptStack.empty())
        {
            // Pop and resume from saved position
            mLayoutParagraph = mInterruptStack.back();
            mInterruptStack.pop_back();
            return true;  // Continue from saved position
        }

        // Truly complete - clear status
        if (mWordTsar != nullptr)
        {
            mWordTsar->SetStatus(" ", false, 0);
        }
        mLayoutRest = false;

        return false;  // STOP - no more work
    }

    // LAYOUT ONE PARAGRAPH (Requirement #1: Equality-based stopping)
    // LayoutParagraph returns true if layout unchanged from previous, false if changed
    bool same = mLayout->LayoutParagraph(mLayoutParagraph);

    // CHECKPOINT-BASED EARLY STOPPING
    // If formatting state at a checkpoint boundary matches the old checkpoint,
    // all subsequent paragraphs will produce identical layout (stronger guarantee
    // than per-paragraph equality since it covers the next CHECKPOINT_INTERVAL paragraphs)
    if (mLayout->LastCheckpointMatched() && !mLayoutRest)
    {
        // Formatting state unchanged at checkpoint boundary -- stop idle layout
        if (!mInterruptStack.empty())
        {
            PARAGRAPH_T savedPosition = mInterruptStack.back();
            if (mLayoutParagraph < savedPosition)
            {
                mLayoutParagraph = savedPosition;
                mInterruptStack.pop_back();
                return true;  // Continue from saved position
            }
            else
            {
                mInterruptStack.pop_back();
            }
        }

        // No more saved positions -- truly complete
        if (mWordTsar != nullptr)
        {
            mWordTsar->SetStatus(" ", false, 0);
        }
        mLayoutRest = false;

        return false;  // STOP -- checkpoint matched, formatting state unchanged
    }

    // EQUALITY-BASED EARLY STOPPING (50-500x speedup)
    // If layout unchanged AND not forcing complete layout, stop here
    if (same && !mLayoutRest)
    {
        // Layout unchanged - we can stop early
        // But first check if we have saved positions to resume from
        if (!mInterruptStack.empty())
        {
            // We stopped before a saved position - resume from it
            PARAGRAPH_T savedPosition = mInterruptStack.back();

            // Only resume if saved position is after current position
            if (mLayoutParagraph < savedPosition)
            {
                mLayoutParagraph = savedPosition;
                mInterruptStack.pop_back();
                return true;  // Continue from saved position
            }
            else
            {
                // We reached or passed the saved position - discard it
                mInterruptStack.pop_back();
            }
        }

        // No more saved positions - truly complete
        if (mWordTsar != nullptr)
        {
            mWordTsar->SetStatus(" ", false, 0);
        }
        mLayoutRest = false;

// printf("[IDLE] early stop at paragraph %ld/%ld\n", mLayoutParagraph, totalParagraphs);
        return false;  // STOP - layout unchanged, no more saved positions
    }

    // Layout changed or forcing complete - continue to next paragraph
    mLayoutParagraph++;
    mLayoutInt = false;

    // Activate busy indicator at start of background layout (no percentage text)
    if (mWordTsar != nullptr && mLayoutParagraph == 1)
    {
        mWordTsar->SetStatus(" ", true, 0);
    }

    return true;  // CONTINUE to next paragraph
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  event [in] key event
///
/// @return nothing
///
/// @brief
/// Handles keyboard input for navigation and editing.
/// Implements arrow keys, word navigation, page scrolling, and document jumps.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::keyPressEvent(QKeyEvent* event)
{
    // Guard: need document and layout for most operations
    if (!mDocument || !mLayout)
    {
        QWidget::keyPressEvent(event);
        return;
    }

    try
    {

    // Reset listener flag -- will be set by OnDocumentChanged if a mutation occurs
    mListenerHandledUpdate = false ;

    // If background layout is active, interrupt it and save current position
    PARAGRAPH_T totalParagraphs = mDocument->GetNumberofParagraphs();
    if (mLayoutParagraph < totalParagraphs)
    {
        // Background layout was in progress - interrupt it
        mLayoutInt = true;

        // REQUIREMENT #3: SAVE POSITION FOR MULTI-LAYER RESUMPTION
        // Save current background layout position to interrupt stack
        mInterruptStack.push_back(mLayoutParagraph);

        // Restart background layout from edited paragraph
        // (will be set after keystroke is processed)
    }

    bool handled = false;

    // Clear search highlighting on any keypress (same as mouse click behavior)
    // Find/FindAgain will re-set these inside HandleKey if needed
    mSearchBlockSet = false;
    mStartSearchBlock = 0;
    mEndSearchBlock = 0;

    // Get modifier keys
    Qt::KeyboardModifiers modifiers = event->modifiers();
    int key = event->key();

    // If WordStar handler is in a control sequence mode (e.g., Ctrl+Q waiting
    // for next key), translate special keys to their character equivalents and
    // pass through to the handler instead of processing them here
    if (mInput && mInput->CheckControlMode())
    {
        char translated = 0;
        switch (key)
        {
            case Qt::Key_Delete:
            {
                translated = 0x7F;  // DEL character (CP/M Delete key)
                break;
            }
            case Qt::Key_Backspace:
            {
                translated = 0x08;  // BS character (Ctrl+H)
                break;
            }
        }
        if (translated != 0)
        {
            bool shift = (modifiers & Qt::ShiftModifier) != 0;
            mInput->HandleKey(translated, shift);
            handled = true;
        }
    }

    // Route all keys through the input handler (thin translator pattern).
    // Special keys go through HandleSpecialKey(), character keys through HandleKey().
    if (!handled)
    {
        bool shift = (modifiers & Qt::ShiftModifier) != 0 ;
        bool ctrl = (modifiers & Qt::ControlModifier) != 0 ;
        bool alt = (modifiers & Qt::AltModifier) != 0 ;
        bool meta = (modifiers & Qt::MetaModifier) != 0 ;   // physical Cmd key (AA_MacDontSwapCtrlAndMeta)

        // macOS-idiomatic shortcuts for commands this app also puts on bare
        // F-keys (F1/F3/F11). macOS reserves those for brightness, Mission
        // Control, and Show Desktop at the OS level, so they never reliably
        // reach any app; these Cmd-chords are the primary path here. F1 is
        // WordStar 7's own real Help key (see cWordStarInput::HandleSpecialKey),
        // so Preferences moved fully to Cmd+, rather than sharing F1 with it.
        if (meta && !ctrl && !alt && key == Qt::Key_Comma)
        {
            SystemPreferences() ;
            handled = true ;
        }
        else if (meta && !ctrl && !alt && key == Qt::Key_Slash)
        {
            // Cmd+/ mirrors F1: "press it, then press the command you want
            // help with" -- route through the same shared-handler state
            // machine rather than duplicating it here.
            handled = mInput->HandleSpecialKey(SPECIAL_F1, shift, ctrl, alt) ;
        }
        else if (meta && ctrl && !alt && key == Qt::Key_F)
        {
            ToggleFullscreen() ;
            handled = true ;
        }
        else if (meta && !ctrl && !alt && key == Qt::Key_G)
        {
            FindAgain() ;
            handled = true ;
        }
        else
        switch (key)
        {
            // --- Special keys: route through HandleSpecialKey ---
            case Qt::Key_Up:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_UP, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Down:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_DOWN, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Left:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_LEFT, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Right:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_RIGHT, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Home:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_HOME, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_End:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_END, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_PageUp:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_PAGE_UP, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_PageDown:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_PAGE_DOWN, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Delete:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_DELETE, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Backspace:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_BACKSPACE, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Tab:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_TAB, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Return:
            case Qt::Key_Enter:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_ENTER, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_Escape:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_ESCAPE, shift, ctrl, alt) ;
                break ;
            }

            // --- Function keys ---
            case Qt::Key_F1:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F1, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F2:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F2, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F3:
            {
#ifdef DEBUG
                // Debug overlay toggle (temporary -- will be removed when debug keys removed)
                SetShowViewportDebug(!GetShowViewportDebug()) ;
                update() ;
                handled = true ;
#else
                // Release builds: real command (Modern mode = Find Next; best-effort,
                // since macOS reserves bare F3 for Mission Control system-wide).
                handled = mInput->HandleSpecialKey(SPECIAL_F3, shift, ctrl, alt) ;
#endif
                break ;
            }

            case Qt::Key_F4:
            {
#ifdef DEBUG
                // Debug overlay toggle (temporary)
                SetShowBoxStats(!GetShowBoxStats()) ;
                update() ;
                handled = true ;
#else
                handled = mInput->HandleSpecialKey(SPECIAL_F4, shift, ctrl, alt) ;
#endif
                break ;
            }

            case Qt::Key_F5:
            {
#ifdef DEBUG
                // Debug overlay toggle (temporary)
                SetShowBoxOutlines(!GetShowBoxOutlines()) ;
                update() ;
                handled = true ;
#else
                handled = mInput->HandleSpecialKey(SPECIAL_F5, shift, ctrl, alt) ;
#endif
                break ;
            }

            case Qt::Key_F6:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F6, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F7:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F7, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F8:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F8, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F9:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F9, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F10:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F10, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F11:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F11, shift, ctrl, alt) ;
                break ;
            }

            case Qt::Key_F12:
            {
                handled = mInput->HandleSpecialKey(SPECIAL_F12, shift, ctrl, alt) ;
                break ;
            }

            default:
            {
                // --- Alt+letter: route to HandleKey with alt=true ---
                if (alt && !ctrl && key >= Qt::Key_A && key <= Qt::Key_Z)
                {
                    char letter = static_cast<char>(tolower(key - Qt::Key_A + 'a')) ;
                    handled = mInput->HandleKey(letter, shift, true) ;
                    break ;
                }

                // --- Ctrl+Alt+letter: route to HandleKey with alt=true ---
                // Derived directly from the key code (same approach as the Alt+letter
                // branch above), not from event->text(): Qt6 on macOS returns an empty
                // text() whenever Control is held, which silently dropped every
                // Ctrl+letter combination -- the core of WordStar's own command set.
                if (alt && ctrl && key >= Qt::Key_A && key <= Qt::Key_Z)
                {
                    char ch = static_cast<char>(key - Qt::Key_A + 1) ;   // Ctrl+A=1 .. Ctrl+Z=26
                    handled = mInput->HandleKey(ch, shift, true) ;
                    break ;
                }

                // --- Ctrl+letter: route to HandleKey with alt=false ---
                // Same reasoning as above: derive the control character from the key
                // code, not event->text(), which Qt6 leaves empty on macOS whenever
                // Control is held.
                if (ctrl && !alt && key >= Qt::Key_A && key <= Qt::Key_Z)
                {
                    char ch = static_cast<char>(key - Qt::Key_A + 1) ;   // Ctrl+A=1 .. Ctrl+Z=26
                    handled = mInput->HandleKey(ch, shift, false) ;
                    break ;
                }

                // --- printable characters ---
                QString text = event->text() ;

                // Skip if no text (modifier-only keys)
                if (text.isEmpty())
                {
                    break ;
                }

                // Get character code
                CHAR_T ch = text.at(0).unicode() ;

                // Delegate to input handler (handles control sequences and CUA bindings)
                bool handledByInput = mInput->HandleKey(static_cast<char>(ch), shift, false) ;

                if (!handledByInput)
                {
                    // Input handler didn't handle it -- insert as regular text
                    // Only insert if no control modifiers (except Shift and AltGr)
                    if (!ctrl && !alt)
                    {
                        std::string utf8Text = text.toStdString() ;
                        InsertText(utf8Text) ;
                        handled = true ;
                    }
                }
                else
                {
                    handled = true ;
                }
                break ;
            }
        }
    }

    // SINGLE UPDATE POINT - handles visual updates for ALL keys
    if (handled)
    {
        // Skip update for F2 (already handled) and, in debug builds only, F3/F4/F5
        // (the debug overlay toggles above call update() themselves). In release
        // builds F3/F4/F5 are real commands routed through HandleSpecialKey like
        // F6-F10, so they need the normal post-command update too.
#ifdef DEBUG
        if (key != Qt::Key_F2 && key != Qt::Key_F3 && key != Qt::Key_F4 && key != Qt::Key_F5)
#else
        if (key != Qt::Key_F2)
#endif
        {
            // Incremental layout of visible range + caret + scroll + paint
            PerformPostCommandUpdate() ;
            mDoDrawCaret = false ;  // Force full repaint (not caret-only blink shortcut)
        }

        // If we interrupted background layout, restart from edited paragraph
        if (mLayoutInt)
        {
            POSITION_T editPosition = mDocument->GetPosition();
            PARAGRAPH_T editedPara = mDocument->GetParagraphFromPosition(editPosition);

            // Restart from edited paragraph (or earlier if interrupt stack has earlier position)
            // Note: We already saved the old position to mInterruptStack above
            mLayoutParagraph = editedPara;
        }

        // Trigger idle layout after keypress (matches old editorctrl.cpp line 477)
        // Uses 1ms delay to allow screen redraw to complete first
        // OnIdle() will reschedule itself with 0ms while work remains
        QTimer::singleShot(1, this, SLOT(OnIdle()));

        event->accept();
    }
    else
    {
        // Pass unhandled keys to base class
        QWidget::keyPressEvent(event);
    }

    }
    catch (const std::bad_alloc&)
    {
        // out of memory -- emergency save and shut down
        EmergencySaveFile(const_cast<char*>("Out of memory in keyPressEvent"));
        ShowError("Fatal Error", "Out of memory. Your file has been emergency-saved.");
        Quit();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QInputMethodEvent *event [in] - the input method event
///
/// @return nothing
///
/// @brief
/// Handle Input Method Editor (IME) events for CJK text input.
/// Per Qt documentation: preeditString() should NOT affect undo/redo stack,
/// only commitString() should. This ensures proper undo granularity for
/// Japanese, Chinese, Korean, and other IME-based input.
///
/// When text is committed (IME finishes conversion), we close any open
/// typing group so each IME commit becomes a separate undo step.
///
/// @see https://doc.qt.io/qt-6/qinputmethodevent.html
/// @see https://firefox-source-docs.mozilla.org/editor/IMEHandlingGuide.html
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::inputMethodEvent(QInputMethodEvent* event)
{
    // Guard: need document and layout
    if (!mDocument || !mLayout)
    {
        QWidget::inputMethodEvent(event);
        return;
    }

    try
    {
        QString commitString = event->commitString();

        if (!commitString.isEmpty())
        {
            // IME is committing text - close any open typing group
            // so this commit becomes a separate undo step
            CloseTypingGroup();

            // Insert the committed text
            InsertText(commitString.toStdString());

            // Accept the event
            event->accept();

            // Incremental layout + caret + scroll + paint (shared with keyPressEvent)
            PerformPostCommandUpdate() ;
            mDoDrawCaret = false ;  // Force full repaint (not caret-only blink shortcut)

            // Restart background layout
            QTimer::singleShot(1, this, SLOT(OnIdle()));
        }
        else
        {
            // Preedit only (composition in progress)
            // Per Qt docs: preedit should NOT affect undo/redo stack
            // Let base class handle display of composition underline
            QWidget::inputMethodEvent(event);
        }
    }
    catch (const std::bad_alloc&)
    {
        // out of memory -- emergency save and shut down
        EmergencySaveFile(const_cast<char*>("Out of memory in inputMethodEvent"));
        ShowError("Fatal Error", "Out of memory. Your file has been emergency-saved.");
        Quit();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QFocusEvent* event [in] the focus event
///
/// @return nothing
///
/// @brief
/// Called when this editor gains keyboard focus. Updates the focus
/// dimming flag and triggers a repaint so the background brightens.
/// Used to visually distinguish the active pane when reveal codes
/// is showing two editors in a split view.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::focusInEvent(QFocusEvent* event)
{
    mHasFocusDim = true;
    update();
    QWidget::focusInEvent(event);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QFocusEvent* event [in] the focus event
///
/// @return nothing
///
/// @brief
/// Called when this editor loses keyboard focus. Updates the focus
/// dimming flag and triggers a repaint so the background dims.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::focusOutEvent(QFocusEvent* event)
{
    mHasFocusDim = false;
    update();
    QWidget::focusOutEvent(event);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Generates highlight rectangles for visible block selections.
/// Called during paintEvent() to update highlights for current viewport.
///
/// Collects all rectangles from segments with isBlock flag
/// and stores them in mBlockCoords vector.
///
/// These rectangles are viewport-relative (adjusted for mScrollOffset)
/// and ready for rendering by DrawHighlights().
///
/// IMPORTANT: This iterates through visible paragraphs only, checking
/// each segment for isBlock flag set during layout phase.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::GenerateHighlightRects(void)
{
    if (!mLayout || !mDocument)
    {
        return;
    }

    // Clear old rectangles
    ClearHighlights();

    COORD_T viewportTop = mScrollOffset;
    COORD_T viewportBottom = mScrollOffset + GetViewportHeight();

    // Only process visible paragraphs, not the entire document
    PARAGRAPH_T lastVisible = GetLastVisibleParagraph();
    PARAGRAPH_T startPara = (mVisibleStart > 0) ? mVisibleStart - 1 : 0;
    PARAGRAPH_T endPara = lastVisible + 1;
    PARAGRAPH_T totalParas = mLayout->GetNumberOfParagraphs();
    if (endPara > totalParas)
    {
        endPara = totalParas;
    }

    // Process visible paragraphs to find marked segments
    // BLACK BOX API: Use GetParagraphLayout() instead of direct mParagraphLayout access
    for (PARAGRAPH_T paraNum = startPara; paraNum < endPara; paraNum++)
    {
        const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(paraNum);
        if (!paragraph)
        {
            continue;
        }

        if (paragraph->lines.empty())
        {
            continue;
        }

        // Check if any line in paragraph is visible
        const sLineLayout& firstLine = paragraph->lines.front();
        const sLineLayout& lastLine = paragraph->lines.back();

        if (lastLine.screeny < viewportTop || firstLine.screeny > viewportBottom)
        {
            continue;
        }

        // Process each visible line
        size_t lineCount = paragraph->lines.size();
        for (size_t lineIndex = 0; lineIndex < lineCount; ++lineIndex)
        {
            const sLineLayout& line = paragraph->lines[lineIndex];

            // Check line visibility
            if (line.screeny < viewportTop || line.screeny > viewportBottom)
            {
                continue;
            }

            // Check if entire line is selected (all segments are marked)
            bool allSegmentsSelected = true;
            for (const auto& seg : line.segments)
            {
                if (!seg.isBlock)
                {
                    allSegmentsSelected = false;
                    break;
                }
            }

            // Check if current line has any selected segments
            bool hasSelectedSegments = false;
            for (const auto& seg : line.segments)
            {
                if (seg.isBlock)
                {
                    hasSelectedSegments = true;
                    break;
                }
            }

            // Check if next line has any selected segments (indicating wrap)
            bool nextLineHasSelection = false;
            if (lineIndex + 1 < lineCount)
            {
                const sLineLayout& nextLine = paragraph->lines[lineIndex + 1];
                for (const auto& seg : nextLine.segments)
                {
                    if (seg.isBlock)
                    {
                        nextLineHasSelection = true;
                        break;
                    }
                }
            }

            // Collect selected segment rectangles for this line
            std::vector<QRectF> lineBlockRects;
            COORD_T maxRight = 0;  // Track rightmost edge

            // Process each segment in visible line
            for (const auto& segment : line.segments)
            {
                // Skip unmarked segments
                if (!segment.isBlock)
                {
                    continue;
                }

                // Skip empty segments
                if (segment.position.empty() || segment.GetGraphemeCount() == 0)
                {
                    continue;
                }

                // Calculate segment bounding rectangle
                COORD_T segmentX = line.pagex + segment.position[0];
                COORD_T segmentY = ScreenYToViewportY(&line);
                COORD_T segmentWidth = 0;
                COORD_T segmentHeight = segment.segmentheight;

                // Calculate width from first to last glyph position + last glyph width
                // Width = span from first to last position + width of last character
                // This ensures the rectangle covers the full width of all selected text
                if (segment.position.size() > 1)
                {
                    // Width spans from first to last position
                    segmentWidth = segment.position.back() - segment.position[0];

                    // Add width of last glyph
                    if (!segment.font.empty())
                    {
                        // Get graphemes to measure last one
                        std::vector<std::string> graphemes;
                        segment.GetGraphemes(mDocument, graphemes);

                        if (!graphemes.empty())
                        {
                            QFont font = FontUtils::FontFromDescriptor(segment.font);
                            font.setPointSizeF(font.pointSizeF() * FONTSCALE);
                            QFontMetricsF metrics(font);

                            QString lastGlyph = QString::fromStdString(graphemes.back());
                            // Font is already scaled by FONTSCALE, so metrics are in twips - don't scale again!
                            COORD_T lastGlyphWidth = metrics.horizontalAdvance(lastGlyph);
                            segmentWidth += lastGlyphWidth;
                        }
                    }
                }
                else
                {
                    // Single glyph - measure it
                    if (!segment.font.empty())
                    {
                        std::vector<std::string> graphemes;
                        segment.GetGraphemes(mDocument, graphemes);

                        if (!graphemes.empty())
                        {
                            QFont font = FontUtils::FontFromDescriptor(segment.font);
                            font.setPointSizeF(font.pointSizeF() * FONTSCALE);
                            QFontMetricsF metrics(font);

                            QString glyph = QString::fromStdString(graphemes[0]);
                            // Font is already scaled by FONTSCALE, so metrics are in twips - don't scale again!
                            segmentWidth = metrics.horizontalAdvance(glyph);
                        }
                    }
                }

                // Create rectangle
                QRectF rect(segmentX, segmentY, segmentWidth, segmentHeight);

                // Track rightmost edge
                COORD_T rightEdge = segmentX + segmentWidth;
                if (rightEdge > maxRight)
                {
                    maxRight = rightEdge;
                }

                // Store in temporary line list
                lineBlockRects.push_back(rect);
            }

            // Extend rectangles to right margin if:
            // 1. Entire line is selected (full-line selection), OR
            // 2. Line has selection AND next line has selection (wrapped selection)
            bool shouldExtendToMargin = (allSegmentsSelected || (hasSelectedSegments && nextLineHasSelection));

            if (shouldExtendToMargin && !lineBlockRects.empty())
            {
                // Get the right margin from the current box
                COORD_T mRightMargin = mLayout->GetBoxRight();

                for (QRectF& rect : lineBlockRects)
                {
                    COORD_T newWidth = mRightMargin - rect.left();
                    rect.setWidth(newWidth);
                }
            }

            // Add line rectangles to global list
            mBlockCoords.insert(mBlockCoords.end(), lineBlockRects.begin(), lineBlockRects.end());
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rects [in] vector of rectangles to build contour from
///
/// @return QPainterPath with contour path
///
/// @brief
/// Builds a selection contour path with rounded corners only on outer edges.
///
/// Creates a flowing highlight shape where consecutive lines connect
/// with sharp internal corners, but the outermost edges have rounded corners.
///
/// Implementation: Merges rectangles into plain rect shapes, then applies
/// a rounded rect mask based on the overall bounding box to create smooth
/// outer edges while preserving sharp internal corners.
///
/////////////////////////////////////////////////////////////////////////////
QPainterPath cEditorCtrl::BuildSelectionContour(const std::vector<QRectF>& rects)
{
    QPainterPath path;

    if (rects.empty())
    {
        return path;
    }

    // Corner radius in twips - provides subtle rounding without expanding shape
    constexpr double RADIUS = 60.0;

    // Add each rectangle with rounded corners
    // This approach doesn't expand the shape like the stroking technique did
    for (const QRectF& rect : rects)
    {
        path.addRoundedRect(rect, RADIUS, RADIUS);
    }

    // Merge overlapping/adjacent rectangles into unified shape
    return path.simplified();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return QPainterPath containing the block selection polygon
///
/// @brief
/// Builds a simple 8-point (or fewer) polygon for block selection,
/// LibreOffice-style: full lines extend from left margin to right margin,
/// first/last partial lines show exact selection boundaries.
///
/// Polygon structure for multi-line selection:
///
///         startX          mRightMargin
///           1-------------2
///           |             |
///           8             3
/// mLeftMargin|             |
///   7-------+-------------4 endX
///   |                     |
///   6---------------------5
///
/////////////////////////////////////////////////////////////////////////////
QPainterPath cEditorCtrl::BuildBlockPolygon(void)
{
    QPainterPath path;

    // Query block range from document (the source of truth)
    POSITION_T blockStart = 0, blockEnd = 0;
    if (!mLayout || !mDocument || !mDocument->GetBlock(blockStart, blockEnd))
    {
        return path;
    }

    if (blockStart >= blockEnd)
    {
        return path;
    }

    // Get margins (left margin is the box left edge, right margin is box right)
    COORD_T mLeftMargin = mLayout->GetBoxLeft();
    COORD_T mRightMargin = mLayout->GetBoxRight();

    // Find the line containing the start of the selection
    LINE_T firstLineNum = mLayout->GetLineFromPosition(blockStart);
    const sLineLayout* firstLine = mLayout->GetLineByRawLineNumber(firstLineNum);
    if (!firstLine)
    {
        return path;
    }

    // Find the line containing the end of the selection (end-1 because half-open interval)
    LINE_T lastLineNum = mLayout->GetLineFromPosition(blockEnd - 1);
    const sLineLayout* lastLine = mLayout->GetLineByRawLineNumber(lastLineNum);
    if (!lastLine)
    {
        return path;
    }

    // Get the X coordinate where selection starts on the first line
    COORD_T startX = mLayout->FindCoordInLine(blockStart, firstLineNum);

    // Get the X coordinate where selection ends on the last line
    // FindCoordInLine returns the LEFT edge of the character, we need to add its width
    COORD_T endX = mLayout->FindCoordInLine(blockEnd - 1, lastLineNum);

    // Add width of the last character to get right edge of selection
    // Use layout's FindCoordInLine for position blockEnd to get the next character's position
    COORD_T endXNext = mLayout->FindCoordInLine(blockEnd, lastLineNum);
    if (endXNext > endX)
    {
        endX = endXNext;
    }
    else
    {
        // If blockEnd is past the line end, extend to right margin
        // Or estimate character width (use a reasonable default)
        endX += 200;  // Default character width in twips
    }

    // Calculate Y coordinates (viewport-relative, mode-aware)
    COORD_T firstLineTop = ScreenYToViewportY(firstLine);
    COORD_T firstLineBottom = firstLineTop + firstLine->lineheight;
    COORD_T lastLineTop = ScreenYToViewportY(lastLine);
    COORD_T lastLineBottom = lastLineTop + lastLine->lineheight;

    // Build the polygon based on whether it's single-line or multi-line
    if (firstLineNum == lastLineNum)
    {
        // Single line selection: simple 4-point rectangle
        path.moveTo(startX, firstLineTop);
        path.lineTo(endX, firstLineTop);
        path.lineTo(endX, firstLineBottom);
        path.lineTo(startX, firstLineBottom);
        path.closeSubpath();
    }
    else
    {
        // Multi-line selection: 8-point polygon (or 6 if starts/ends at margins)
        // Build clockwise from top-left of selection

        // Point 1: Start of selection, top of first line
        path.moveTo(startX, firstLineTop);

        // Point 2: Right margin, top of first line
        path.lineTo(mRightMargin, firstLineTop);

        // Point 3: Right margin, top of last line
        path.lineTo(mRightMargin, lastLineTop);

        // Point 4: End of selection, top of last line
        path.lineTo(endX, lastLineTop);

        // Point 5: End of selection, bottom of last line
        path.lineTo(endX, lastLineBottom);

        // Point 6: Left margin, bottom of last line
        path.lineTo(mLeftMargin, lastLineBottom);

        // Point 7: Left margin, bottom of first line
        path.lineTo(mLeftMargin, firstLineBottom);

        // Point 8: Start of selection, bottom of first line
        path.lineTo(startX, firstLineBottom);

        path.closeSubpath();
    }

    return path;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return QPainterPath - polygon covering the current search result
///
/// @brief
/// Builds a highlight polygon for the current search result at paint time.
/// Converts mStartSearchBlock / mEndSearchBlock document positions to screen
/// coordinates using the layout's public API, following the same pattern as
/// BuildBlockPolygon().
///
/////////////////////////////////////////////////////////////////////////////
QPainterPath cEditorCtrl::BuildSearchPolygon(void)
{
    QPainterPath path;

    // No search result
    if (!mSearchBlockSet || mStartSearchBlock >= mEndSearchBlock || !mLayout || !mDocument)
    {
        return path;
    }

    // Find the line containing the start of the search result
    LINE_T firstLineNum = mLayout->GetLineFromPosition(mStartSearchBlock);
    const sLineLayout* firstLine = mLayout->GetLineByRawLineNumber(firstLineNum);
    if (!firstLine)
    {
        return path;
    }

    // Find the line containing the end of the search result (half-open interval)
    LINE_T lastLineNum = mLayout->GetLineFromPosition(mEndSearchBlock - 1);
    const sLineLayout* lastLine = mLayout->GetLineByRawLineNumber(lastLineNum);
    if (!lastLine)
    {
        return path;
    }

    // Get the X coordinate where search result starts
    COORD_T startX = mLayout->FindCoordInLine(mStartSearchBlock, firstLineNum);

    // Get the X coordinate where search result ends
    // FindCoordInLine returns the LEFT edge, so get position of mEndSearchBlock
    // to find the right edge of the last character
    COORD_T endX = mLayout->FindCoordInLine(mEndSearchBlock - 1, lastLineNum);
    COORD_T endXNext = mLayout->FindCoordInLine(mEndSearchBlock, lastLineNum);
    if (endXNext > endX)
    {
        endX = endXNext;
    }
    else
    {
        // Past end of line, estimate character width
        endX += 200;
    }

    // Calculate Y coordinates (viewport-relative, mode-aware)
    COORD_T firstLineTop = ScreenYToViewportY(firstLine);
    COORD_T firstLineBottom = firstLineTop + firstLine->lineheight;
    COORD_T lastLineTop = ScreenYToViewportY(lastLine);
    COORD_T lastLineBottom = lastLineTop + lastLine->lineheight;

    // Build the polygon
    if (firstLineNum == lastLineNum)
    {
        // Single line: simple rectangle
        path.moveTo(startX, firstLineTop);
        path.lineTo(endX, firstLineTop);
        path.lineTo(endX, firstLineBottom);
        path.lineTo(startX, firstLineBottom);
        path.closeSubpath();
    }
    else
    {
        // Multi-line search result: 8-point polygon
        COORD_T mLeftMargin = mLayout->GetBoxLeft();
        COORD_T mRightMargin = mLayout->GetBoxRight();

        path.moveTo(startX, firstLineTop);
        path.lineTo(mRightMargin, firstLineTop);
        path.lineTo(mRightMargin, lastLineTop);
        path.lineTo(endX, lastLineTop);
        path.lineTo(endX, lastLineBottom);
        path.lineTo(mLeftMargin, lastLineBottom);
        path.lineTo(mLeftMargin, firstLineBottom);
        path.lineTo(startX, firstLineBottom);
        path.closeSubpath();
    }

    return path;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  painter [in/out] QPainter to draw with
///
/// @return nothing
///
/// @brief
/// Draws block selection and search highlights.
///
/// Block selection is drawn as a simple polygon (LibreOffice-style) that
/// extends from left margin to right margin for full lines.
///
/// Search highlights use the existing per-grapheme rectangle approach.
///
/// Painter is already scaled to TWIPS coordinates by caller.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DrawHighlights(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing, true);

    // Draw block selection (queries document directly for block range)
    {
        QPainterPath blockPath = BuildBlockPolygon();
        if (!blockPath.isEmpty())
        {
            painter.setPen(Qt::NoPen);
            painter.setBrush(mBlockColour);
            painter.drawPath(blockPath);
        }
    }

    // Draw search highlight on top of block selection (computed at paint time)
    if (mSearchBlockSet)
    {
        QPainterPath searchPath = BuildSearchPolygon();

        painter.setPen(Qt::NoPen);
        painter.setBrush(mSearchColour);
        painter.drawPath(searchPath);
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
}


/////////////////////////////////////////////////////////////////////////////
// WORDSTAR INPUT INTEGRATION
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] command string to display
///
/// @return nothing
///
/// @brief
/// Shows "Not Implemented" message for commands that exist in WordStar
/// but are not yet implemented in WordTsar.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::NotImplemented(const std::string& command)
{
    const char* qtTesting = std::getenv("QT_TESTING");
    if (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0)
    {
        return;
    }

    QString message = string_sprintf("Command %s not implemented (yet)", command.c_str()).c_str();
    QMessageBox msgBox(QMessageBox::Information, "Information", message, QMessageBox::Ok, this);
    msgBox.exec();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] invalid command string
///
/// @return nothing
///
/// @brief
/// Shows "Invalid Command" message for unrecognized WordStar commands.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::InvalidCommand(const std::string& command)
{
    const char* qtTesting = std::getenv("QT_TESTING");
    if (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0)
    {
        return;
    }

    QString message = string_sprintf("Invalid Command %s ", command.c_str()).c_str();
    QMessageBox msgBox(QMessageBox::Information, "Information", message, QMessageBox::Ok, this);
    msgBox.exec();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] dialog title
/// @param  message [in] error message to display
///
/// @return nothing
///
/// @brief
/// Shows an error dialog to the user.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::ShowError(const std::string& title, const std::string& message)
{
    QMessageBox msgBox(QMessageBox::Critical, QString::fromStdString(title), QString::fromStdString(message), QMessageBox::Ok, this);
    msgBox.exec();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] dialog title
/// @param  message [in] informational message to display
///
/// @return nothing
///
/// @brief
/// Shows an informational message dialog to the user.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::ShowMessage(const std::string& title, const std::string& message)
{
    const char* qtTesting = std::getenv("QT_TESTING");
    if (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0)
    {
        return;
    }

    QMessageBox msgBox(QMessageBox::Information, QString::fromStdString(title), QString::fromStdString(message), QMessageBox::Ok, this);
    msgBox.exec();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] dialog title
/// @param  question [in] question text to display
///
/// @return true if user clicked Yes, false if No
///
/// @brief
/// Shows a Yes/No question dialog and returns the user's choice.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::AskYesNo(const std::string& title, const std::string& question)
{
    return QMessageBox::question(this, QString::fromStdString(title), QString::fromStdString(question),
                                 QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Quits the application.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Quit(void)
{
    QCoreApplication::quit();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  enabled [in] true to enable, false to disable
///
/// @return nothing
///
/// @brief
/// Enables or disables the editor widget for input.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetEnabled(bool enabled)
{
    setEnabled(enabled);
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] new window title
///
/// @return nothing
///
/// @brief
/// Sets the window title (typically shows current filename).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetTitle(const std::string& title)
{
    // *** REFACTORED FROM OLD SYSTEM - editorctrl.cpp::SetTitle() (lines 813-818) ***
    // API CHANGE: mWordTsar->setWindowTitle() becomes window()->setWindowTitle()
    //   Old system had pointer to main window (mWordTsar)
    //   New system uses Qt's window() to get top-level window

    // Format title with version information
    std::string formattedTitle = string_sprintf("%s - WordTsar %ld.%ld build %ld %s",
                                                 title.c_str(), MAJOR, MINOR, BUILD, STATUS);

    // Set window title on top-level window
    QWidget* topWindow = window();
    if (topWindow)
    {
        topWindow->setWindowTitle(QString::fromStdString(formattedTitle));
    }
}


/////////////////////////////////////////////////////////////////////////////
// BLOCK OPERATIONS
/////////////////////////////////////////////////////////////////////////////

void cEditorCtrl::SetPreviousBlock(void)
{
    // Call base class method
    cEditorBase::SetPreviousBlock();

    // Update visual state
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

void cEditorCtrl::CopyBlock(void)
{
    // Call base class method
    // Visual update driven by listener (Paste calls NotifyChanged calls OnDocumentChanged)
    cEditorBase::CopyBlock();
}

void cEditorCtrl::MoveBlock(void)
{
    // Call base class method
    // Batch wraps Paste + Cut (2 notifications) into one visual update
    BeginBatchUpdate() ;
    cEditorBase::MoveBlock();
    EndBatchUpdate() ;
}

void cEditorCtrl::DeleteBlock(void)
{
    // Call base class method
    // Visual update driven by listener (Cut calls NotifyChanged calls OnDocumentChanged)
    cEditorBase::DeleteBlock();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Count words in block (if set) or entire document (if no block).
/// Displays result in a message box.
/// [REFACTORED from editorctrl.cpp::WordCountBlock()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::WordCountBlock(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::WordCountBlock() ***

    if (!mDocument)
    {
        return;
    }

    long words = 0;

    if (mDocument->mBlockSet == true)
    {
        words = WordCount(mDocument->mStartBlock, mDocument->mEndBlock);
    }
    else
    {
        words = WordCount(0, 0);  // Count entire document
    }

    // Store result for testing access
    mLastWordCount = words;

    // Skip dialog if running in test mode (QT_TESTING environment variable set)
    const char* qtTesting = std::getenv("QT_TESTING");
    if (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0)
    {
        // Running in test mode - skip dialog
        return;
    }

    QString tmp = QString::asprintf("       \n\n       Word Count: %ld       \n\n\n", words);
    QMessageBox msgBox(QMessageBox::Information, "Word Count", tmp, QMessageBox::Ok, this);
    msgBox.exec();
}


/////////////////////////////////////////////////////////////////////////////
// CLIPBOARD OPERATIONS
/////////////////////////////////////////////////////////////////////////////

void cEditorCtrl::ClipboardCopy(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::ClipboardCopy() ***
    // API CHANGE: Old used mDocument.mBlockSet (member)
    // New uses mDocument->GetBlock() (pointer + method)

    if (!mDocument)
    {
        return;
    }

    POSITION_T start = 0;
    POSITION_T end = 0;

    if (mDocument->GetBlock(start, end))
    {
        // Get block text from document
        std::string str = mDocument->GetBlockText(start, end);

        // Copy to system clipboard
        QClipboard* clipboard = QApplication::clipboard();
        clipboard->setText(QString::fromUtf8(str.c_str()), QClipboard::Clipboard);

        // Show feedback on status bar
        if (mWordTsar)
        {
            mWordTsar->SetStatus("Copied to clipboard", false, 0);
        }
    }
}

void cEditorCtrl::ClipboardPaste(void)
{
    if (!mDocument)
    {
        return;
    }

    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();

    // Check for RTF formatted content first (preserves formatting from other apps)
    QStringList formats = mimeData->formats();
    if (formats.contains("text/rtf"))
    {
        QByteArray rtfData = mimeData->data("text/rtf");
        std::string rtfString(rtfData.constData(), rtfData.size());

        // Suppress per-insert notifications (RTF parser makes many Insert calls).
        // Use SetSuppressNotify (not SetLoading) to preserve undo recording.
        mDocument->SetSuppressNotify(true);
        mDocument->BeginUndoGroup();

        cRTFFile rtf(this);
        rtf.LoadRTFString(rtfString);

        mDocument->EndUndoGroup();
        mDocument->SetSuppressNotify(false);

        // Single full relayout after all inserts
        LayoutDocument(true);
        CalculateCaretPosition();
        ScrollIntoView();
        Repaint();

        // Notify sibling -- notifications suppressed during RTF paste
        if (mSiblingEditor != nullptr)
        {
            mSiblingEditor->LayoutDocument(true) ;
            mSiblingEditor->PerformPostCommandUpdate() ;
        }

        // Show feedback on status bar
        if (mWordTsar)
        {
            mWordTsar->SetStatus("Pasted", false, 0);
        }
        return;
    }

    // Fall back to plain text paste
    if (mimeData->hasText())
    {
        // Get text from clipboard
        QString str = clipboard->text();
        std::string utf8 = str.toUtf8().toStdString();

        // Insert at current position
        // Visual update driven by listener (InsertText calls NotifyChanged calls OnDocumentChanged)
        InsertText(utf8);

        // Show feedback on status bar
        if (mWordTsar)
        {
            mWordTsar->SetStatus("Pasted", false, 0);
        }
    }
    else
    {
        const char* qtTesting = std::getenv("QT_TESTING");
        if (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0)
        {
            ShowError("Error", "Unknown data format - cannot paste.");
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// SEARCH/REPLACE
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Initiates a new search operation. Saves the current position and
/// sets up search parameters, then calls FindAgain() to perform the search.
///
/// *** COPIED FROM OLD SYSTEM - editorctrl.cpp::Find() ***
/// API CHANGE: mDocument member becomes mDocument pointer
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Find(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::Find() (lines 3058-3089) ***
    // API CHANGE: mDocument.GetPosition() becomes mDocument->GetPosition()

    if (!mDocument)
    {
        return;
    }

    // Save position before starting search (for GotoLastFindandReplace)
    mLastFindandReplace = mDocument->GetPosition();

    // Skip dialog if running in test mode (QT_TESTING environment variable set)
    const char* qtTesting = std::getenv("QT_TESTING");
    if (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0)
    {
        // Test mode - use existing mSearchText without dialog
        if (!mSearchText.empty())
        {
            FindAgain();
        }
        return;
    }

    // Show Find dialog
    QDialog dialog(this);
    Ui::Find find;
    find.setupUi(&dialog);
    find.mFind->setFocus();
    find.mNextOccurance->setChecked(true);

    // Pre-populate with previous search text if available
    if (!mSearchText.empty())
    {
        find.mFind->setText(QString::fromStdString(mSearchText));
        find.mFind->selectAll();
    }

    int ecode = dialog.exec();

    if (ecode != 0)
    {
        QString text = find.mFind->text();
        if (text.length() != 0)
        {
            mWholeFile = find.mGlobal->isChecked();
            mCaseCmp = find.mIgnoreCase->isChecked();
            mSearchBackwards = find.mBackward->isChecked();
            mWildCard = find.mUseWildCard->isChecked();
            mWholeWord = find.mWholeWord->isChecked();
            mSearchText = text.toStdString();
            FindAgain();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Repeats the last search operation using saved search parameters.
/// Searches for mSearchText from current position + 1 (forward) or
/// current position - 1 (backward), using saved wildcard/case/wholeword flags.
///
/// Updates mStartSearchBlock, mEndSearchBlock, and mSearchBlockSet when found.
/// Scrolls found text into view. Search highlighting is computed at paint time
/// by BuildSearchPolygon() using these stored positions.
///
/// *** COPIED FROM OLD SYSTEM - editorctrl.cpp::FindAgain() ***
/// API CHANGE: mDocument member becomes mDocument pointer
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::FindAgain(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::FindAgain() (lines 3093-3134) ***
    // API CHANGE: mDocument.FindNext() becomes mDocument->FindNext()
    // API CHANGE: mDocument.GetPosition() becomes mDocument->GetPosition()
    // API CHANGE: mDocument.GetTextSize() becomes mDocument->GetTextSize()

    if (!mDocument)
    {
        return;
    }

    POSITION_T fpos = NOT_SET;

    if (mSearchBackwards == false)
    {
        // Search forward from current position + 1
        fpos = mDocument->FindNext(mSearchText, mDocument->GetPosition() + 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == mDocument->GetTextSize())
        {
            // Not found - reached end of document
            const char* qtTesting = std::getenv("QT_TESTING");
            if (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0)
            {
                QString temp = string_sprintf("Search word: %s  not found.", mSearchText.c_str()).c_str();
                QMessageBox msgBox(QMessageBox::Information, "Not found", temp, QMessageBox::Ok, this);
                msgBox.exec();
            }
        }
        else
        {
            // Found - update position and highlight
            // Skip over hidden content (dot commands/comments in SHOW_NONE mode)
            fpos = SkipOverHiddenContent(fpos, +1);
            mDocument->SetPosition(fpos);
            mStartSearchBlock = fpos;

            // Count graphemes in the search text via cDocument (black box).
            std::vector<POSITION_T> matchOffsets;
            POSITION_T searchGraphemes = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mEndSearchBlock = fpos + searchGraphemes;
            mSearchBlockSet = true;

            // Update visual state
            CalculateCaretPosition();
            ScrollIntoView();
            Repaint();
        }
    }
    else
    {
        // Search backward from current position - 1
        fpos = mDocument->FindPrev(mSearchText, mDocument->GetPosition() - 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == NOT_SET)
        {
            // Not found - reached start of document
            const char* qtTesting = std::getenv("QT_TESTING");
            if (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0)
            {
                QString temp = string_sprintf("Search word: %s  not found.", mSearchText.c_str()).c_str();
                QMessageBox msgBox(QMessageBox::Information, "Not found", temp, QMessageBox::Ok, this);
                msgBox.exec();
            }
        }
        else
        {
            // Found - update position and highlight
            // Skip over hidden content (dot commands/comments in SHOW_NONE mode)
            fpos = SkipOverHiddenContent(fpos, -1);
            mDocument->SetPosition(fpos);
            mStartSearchBlock = fpos;

            // Count graphemes in the search text via cDocument (black box).
            std::vector<POSITION_T> matchOffsets;
            POSITION_T searchGraphemes = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mEndSearchBlock = fpos + searchGraphemes;
            mSearchBlockSet = true;

            // Update visual state
            CalculateCaretPosition();
            ScrollIntoView();
            Repaint();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  noquery [in] if true, skip confirmation dialog (unused in P1)
///
/// @return true if reached end of document (no more matches), false otherwise
///
/// @brief
/// Performs a single find-and-replace operation from current position.
/// Searches for mSearchText, optionally asks for confirmation (if mReplaceAsk),
/// then deletes the found text and inserts mReplaceText.
///
/// Returns true when search reaches end/start of document (no more matches).
///
/// *** COPIED FROM OLD SYSTEM - editorctrl.cpp::ReplaceAgain() ***
/// API CHANGE: mDocument member becomes mDocument pointer
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::ReplaceAgain(bool noquery)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::ReplaceAgain() (lines 2957-3055) ***
    // API CHANGE: mDocument.FindNext() becomes mDocument->FindNext()
    // API CHANGE: mDocument.FindPrev() becomes mDocument->FindPrev()
    // API CHANGE: mDocument.GetPosition() becomes mDocument->GetPosition()
    // API CHANGE: mDocument.SetPosition() becomes mDocument->SetPosition()
    // API CHANGE: mDocument.GetTextSize() becomes mDocument->GetTextSize()
    // API CHANGE: mDocument.Insert() becomes mDocument->Insert()
    // API CHANGE: Delete() is inherited from cEditorBase

    if (!mDocument)
    {
        return true;
    }

    bool retval = false;
    POSITION_T fpos = NOT_SET;

    if (mSearchBackwards == false)
    {
        // Forward search
        if (mWholeFile)
        {
            mDocument->SetPosition(0);
        }

        fpos = mDocument->FindNext(mSearchText, mDocument->GetPosition() + 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == mDocument->GetTextSize())
        {
            // At end of document - no more matches
            const char* qtTesting = std::getenv("QT_TESTING");
            if (mReplaceAsk == true && (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0))
            {
                QString temp = "At end of document.";
                QMessageBox msgBox(QMessageBox::Information, "Not Found", temp, QMessageBox::Ok, this);
                msgBox.exec();
            }
            retval = true;
        }
        else
        {
            // Found match - position at start of match
            mDocument->SetPosition(fpos);

            // Grapheme count of the matched text via cDocument (black box).
            // mSearchText.length() is a BYTE count, but Delete() needs a GRAPHEME
            // count; matching is length-preserving so the match spans the same
            // number of graphemes as the (normalization-invariant) search text.
            std::vector<POSITION_T> matchOffsets;
            POSITION_T graphemeCount = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mReplaceSize = graphemeCount;

            // Highlight found text so user can see what will be replaced
            mStartSearchBlock = fpos;
            mEndSearchBlock = fpos + graphemeCount;
            mSearchBlockSet = true;

            // In interactive mode, layout and repaint so user can see the
            // highlighted match before the "Replace?" dialog.  In batch mode
            // (noquery == true) skip all visual work -- one repaint happens
            // after the entire batch loop finishes.
            if (!noquery)
            {
                PARAGRAPH_T foundPara = mDocument->GetParagraphFromPosition(fpos);
                mLayout->LayoutParagraph(foundPara);
                CalculateCaretPosition();
                ScrollIntoView();

                PARAGRAPH_T startPara = (mVisibleStart > 0) ? mVisibleStart - 1 : 0;
                PARAGRAPH_T endPara = GetLastVisibleParagraph() + 1;
                PARAGRAPH_T totalParagraphs = mDocument->GetNumberofParagraphs();
                if (endPara >= totalParagraphs)
                {
                    endPara = totalParagraphs - 1;
                }
                for (PARAGRAPH_T para = startPara; para <= endPara; ++para)
                {
                    mLayout->LayoutParagraph(para);
                }
                CalculateCaretPosition();
                repaint();
            }

            // Ask for confirmation if enabled (only in interactive mode)
            bool rep = true;
            if (mReplaceAsk == true && !noquery)
            {
                const char* qtTesting = std::getenv("QT_TESTING");
                if (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0)
                {
                    QMessageBox msgBox(QMessageBox::Question, "Replace", "Replace?",
                                       QMessageBox::Yes | QMessageBox::No, this);
                    msgBox.setDefaultButton(QMessageBox::Yes);
                    int answer = msgBox.exec();
                    if (answer == QMessageBox::No)
                    {
                        rep = false;
                    }
                }
            }

            if (rep == true)
            {
                // Perform replacement:
                // 1. Delete the found text (mReplaceSize graphemes)
                // 2. Insert the replacement text
                // Batch wraps Delete + Insert (2 notifications) into one visual update
                BeginBatchUpdate() ;
                Delete(mDocument->GetPosition(), mReplaceSize);
                mDocument->Insert(mReplaceText.c_str());
                EndBatchUpdate() ;
            }
        }
    }
    else
    {
        // Backward search
        if (mWholeFile)
        {
            mDocument->SetPosition(mDocument->GetTextSize() - 2);
        }

        fpos = mDocument->FindPrev(mSearchText, mDocument->GetPosition() - 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == NOT_SET)
        {
            // At start of document - no more matches
            const char* qtTesting = std::getenv("QT_TESTING");
            if (mReplaceAsk == true && (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0))
            {
                QString temp = "At start of document.";
                QMessageBox msgBox(QMessageBox::Information, "Not Found", temp, QMessageBox::Ok, this);
                msgBox.exec();
            }
            retval = true;
        }
        else
        {
            // Found match - position at start of match
            mDocument->SetPosition(fpos);

            // Grapheme count of the matched text via cDocument (black box) -- same
            // as the forward path.
            std::vector<POSITION_T> matchOffsets;
            POSITION_T graphemeCount = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mReplaceSize = graphemeCount;

            // Highlight found text so user can see what will be replaced
            mStartSearchBlock = fpos;
            mEndSearchBlock = fpos + graphemeCount;
            mSearchBlockSet = true;

            // In interactive mode, layout and repaint so user can see the
            // highlighted match.  In batch mode skip all visual work.
            if (!noquery)
            {
                PARAGRAPH_T foundPara = mDocument->GetParagraphFromPosition(fpos);
                mLayout->LayoutParagraph(foundPara);
                CalculateCaretPosition();
                ScrollIntoView();

                PARAGRAPH_T startPara = (mVisibleStart > 0) ? mVisibleStart - 1 : 0;
                PARAGRAPH_T endPara = GetLastVisibleParagraph() + 1;
                PARAGRAPH_T totalParagraphs = mDocument->GetNumberofParagraphs();
                if (endPara >= totalParagraphs)
                {
                    endPara = totalParagraphs - 1;
                }
                for (PARAGRAPH_T para = startPara; para <= endPara; ++para)
                {
                    mLayout->LayoutParagraph(para);
                }
                CalculateCaretPosition();
                repaint();
            }

            // Ask for confirmation if enabled (only in interactive mode)
            bool rep = true;
            if (mReplaceAsk == true && !noquery)
            {
                const char* qtTesting = std::getenv("QT_TESTING");
                if (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0)
                {
                    QMessageBox msgBox(QMessageBox::Question, "Replace", "Replace?",
                                       QMessageBox::Yes | QMessageBox::No, this);
                    msgBox.setDefaultButton(QMessageBox::Yes);
                    int answer = msgBox.exec();
                    if (answer == QMessageBox::No)
                    {
                        rep = false;
                    }
                }
            }

            if (rep == true)
            {
                // Perform replacement
                // Batch wraps Delete + Insert (2 notifications) into one visual update
                BeginBatchUpdate() ;
                Delete(mDocument->GetPosition(), mReplaceSize);
                mDocument->Insert(mReplaceText.c_str());
                EndBatchUpdate() ;
            }
        }
    }

    return retval;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Initiates a find-and-replace operation. Shows dialog to get
/// search/replace text and options, then performs replacement.
///
/// Supports three modes:
/// - Next occurrence: Find and replace once (with optional confirmation)
/// - Entire file: Replace all from start/end of document
/// - Rest of file: Replace all from current position
///
/// *** COPIED FROM OLD SYSTEM - editorctrl.cpp::Replace() ***
/// API CHANGE: mDocument member becomes mDocument pointer
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Replace(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::Replace() (lines 2867-2954) ***
    // API CHANGE: mDocument.GetPosition() becomes mDocument->GetPosition()
    // API CHANGE: mDocument.SetPosition() becomes mDocument->SetPosition()
    // API CHANGE: mDocument.GetTextSize() becomes mDocument->GetTextSize()

    if (!mDocument)
    {
        return;
    }

    // Save position before starting search (for GotoLastFindandReplace)
    mLastFindandReplace = mDocument->GetPosition();

    // Skip dialog if running in test mode (QT_TESTING environment variable set)
    const char* qtTesting = std::getenv("QT_TESTING");
    if (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0)
    {
        // Test mode - use existing mSearchText/mReplaceText without dialog
        if (!mSearchText.empty())
        {
            mReplaceSize = static_cast<POSITION_T>(mSearchText.length());
            ReplaceAgain();
        }
        return;
    }

    // Show Find/Replace dialog
    QDialog dialog(this);
    Ui::FindReplace findandreplace;
    findandreplace.setupUi(&dialog);
    findandreplace.mFind->setFocus();
    findandreplace.mNextOccurance->setChecked(true);

    // Pre-populate with previous search/replace text if available
    if (!mSearchText.empty())
    {
        findandreplace.mFind->setText(QString::fromStdString(mSearchText));
    }
    if (!mReplaceText.empty())
    {
        findandreplace.mReplace->setText(QString::fromStdString(mReplaceText));
    }

    int ecode = dialog.exec();

    if (ecode != 0)
    {
        QString findText = findandreplace.mFind->text();
        if (findText.length() == 0)
        {
            return;
        }

        mSearchText = findText.toStdString();
        mReplaceText = findandreplace.mReplace->text().toStdString();
        mCaseCmp = findandreplace.mIgnoreCase->isChecked();
        mSearchBackwards = findandreplace.mBackward->isChecked();
        mWildCard = findandreplace.mUseWildCard->isChecked();
        mWholeWord = findandreplace.mWholeWords->isChecked();
        bool dontask = findandreplace.mDontAsk->isChecked();
        mReplaceAsk = !dontask;
        mReplaceSize = static_cast<POSITION_T>(mSearchText.length());

        bool next = findandreplace.mNextOccurance->isChecked();
        bool entire = findandreplace.mGlobal->isChecked();
        bool rest = findandreplace.mRestofFile->isChecked();

        if (next)
        {
            // Single replacement
            ReplaceAgain();
        }
        else if (entire || rest)
        {
            // Batch replacement
            if (entire)
            {
                // Start from beginning (forward) or end (backward)
                if (mSearchBackwards)
                {
                    mDocument->SetPosition(mDocument->GetTextSize() - 1);
                }
                else
                {
                    mDocument->SetPosition(0);
                }
            }

            // Replace all occurrences with counting
            // Undo group wraps the entire batch so one Ctrl+Z undoes all
            CloseTypingGroup();
            mDocument->BeginUndoGroup();
            BeginBatchUpdate();
            bool retval = false;
            long count = -1;
            while (retval == false)
            {
                count++;
                retval = ReplaceAgain(true);
            }
            EndBatchUpdate();
            mDocument->EndUndoGroup();

            // Show replacement count dialog (matches old WordStar behavior)
            QString t = string_sprintf("Replaced %ld items", count).c_str();
            QMessageBox msgBox(QMessageBox::Information, "Replace", t, QMessageBox::Ok, this);
            msgBox.exec();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// ADVANCED NAVIGATION
/////////////////////////////////////////////////////////////////////////////

void cEditorCtrl::WordLeft(void)
{
    // Wrapper for base class method
    MoveCaretWordLeft();
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

void cEditorCtrl::WordRight(void)
{
    // Wrapper for base class method
    MoveCaretWordRight();
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

void cEditorCtrl::PageUp(void)
{
    // Wrapper for base class method
    MoveCaretPage(-1);
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

void cEditorCtrl::PageDown(void)
{
    // Wrapper for base class method
    MoveCaretPage(1);
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
// DELETE OPERATIONS
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete from caret to next occurrence of user-specified character.
/// Prompts user for character via dialog.
/// [REFACTORED from editorctrl.cpp::DeleteToChar()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::DeleteToChar(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::DeleteToChar() ***

    QDialog dialog(this);

    Ui::DelToChar gotochar;
    gotochar.setupUi(&dialog);

    gotochar.character->setFocus();

    dialog.setWindowTitle("Delete to Character");
    int ecode = dialog.exec();

    if (ecode != 0)
    {
        POSITION_T spos = mDocument->GetPosition();

        if (gotochar.character->text().length() != 0)
        {
            // Set search parameters for forward character search
            mWholeFile = false;
            mCaseCmp = false;
            mSearchBackwards = false;
            mWildCard = false;
            mWholeWord = false;

            mSearchText = gotochar.character->text().toStdString();
            FindAgain();
        }

        POSITION_T epos = mDocument->GetPosition();

        // Delete from start to found position
        if (spos != epos)
        {
            Delete(spos, epos - spos);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// GOTO OPERATIONS
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to next occurrence of user-specified character (forward search).
/// Prompts user for character via dialog.
/// [REFACTORED from editorctrl.cpp::GotoCharacter()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::GotoCharacter(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::GotoCharacter() ***

    QDialog dialog(this);

    Ui::GotoChar gotochar;
    gotochar.setupUi(&dialog);

    gotochar.character->setFocus();

    dialog.setWindowTitle("Goto to Character");
    int ecode = dialog.exec();

    if (ecode != 0)
    {
        if (gotochar.character->text().length() != 0)
        {
            // Set search parameters for forward character search
            mWholeFile = false;
            mCaseCmp = false;
            mSearchBackwards = false;
            mWildCard = false;
            mWholeWord = false;

            mSearchText = gotochar.character->text().toStdString();

            // Save current position to detect if FindAgain succeeded
            POSITION_T oldPos = mDocument->GetPosition();
            FindAgain();

            // If position didn't change, character was not found - move to end
            // Skip hidden content at document end
            if (mDocument->GetPosition() == oldPos)
            {
                POSITION_T safePos = SkipOverHiddenContent(mDocument->GetTextSize(), -1);
                mDocument->SetPosition(safePos);
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to previous occurrence of user-specified character (backward search).
/// Prompts user for character via dialog.
/// [REFACTORED from editorctrl.cpp::GotoCharacterBackward()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::GotoCharacterBackward(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::GotoCharacterBackward() ***

    QDialog dialog(this);

    Ui::GotoChar gotochar;
    gotochar.setupUi(&dialog);

    gotochar.character->setFocus();

    dialog.setWindowTitle("Go to to Character");
    int ecode = dialog.exec();

    if (ecode != 0)
    {
        if (gotochar.character->text().length() != 0)
        {
            // Set search parameters for backward character search
            mWholeFile = false;
            mCaseCmp = false;
            mSearchBackwards = true;  // API NOTE: This is the only difference from GotoCharacter()
            mWildCard = false;
            mWholeWord = false;

            mSearchText = gotochar.character->text().toStdString();

            // Save current position to detect if FindAgain succeeded
            POSITION_T oldPos = mDocument->GetPosition();
            FindAgain();

            // If position didn't change, character was not found - move to start
            // Skip hidden content at document start
            if (mDocument->GetPosition() == oldPos)
            {
                POSITION_T safePos = SkipOverHiddenContent(0, +1);
                mDocument->SetPosition(safePos);
            }
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to first line of specified page number.
/// Prompts user for page number via dialog.
/// [REFACTORED from editorctrl.cpp::GotoPage()]
///
/// @note API CHANGE: Old code directly accessed mParagraphLayout.
///       New code uses black box layout APIs (GetNumberOfParagraphs,
///       GetFirstLineOfParagraph, GetNumberofLinesinParagraph, etc.)
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::GotoPage(void)
{
    // *** REFACTORED FROM OLD SYSTEM - editorctrl.cpp::GotoPage() ***

    QDialog dialog(this);

    Ui::GotoPage gotopage;
    gotopage.setupUi(&dialog);

    gotopage.pagenumber->setFocus();

    dialog.setWindowTitle("Go to Page Number");
    int ecode = dialog.exec();

    if (ecode != 0)
    {
        PAGE_T page = gotopage.pagenumber->text().toLong();
        if (page > 0 && page <= mLayout->GetNumberOfPages())
        {
            // Find the first paragraph that has lines on the target page
            PARAGRAPH_T para = mLayout->GetFirstParagraphOnPage(page) ;
            if (para < mLayout->GetNumberOfParagraphs())
            {
                const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para) ;
                if (paraLayout)
                {
                    // Find the first LINE on the target page (paragraph may start on previous page)
                    for (const auto& line : paraLayout->lines)
                    {
                        if (line.pagenumber == page)
                        {
                            mDocument->SetPosition(line.documentPosition) ;
                            ScrollIntoView() ;
                            CalculateCaretPosition() ;
                            Repaint() ;
                            break ;
                        }
                    }
                }
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Prompt for and apply a new WordStar 7 help level (F1 F1). Per the real
/// manual's "Change Help Level" reference:
///   4 - Pull-down menu bar shown (Alt+letter opens a menu); all prompts
///       displayed. WordTsar's menu bar is always shown regardless of
///       level, so this looks the same as 0/1 here -- no classic Edit
///       Menu or submenus, since the pull-down bar is the access point.
///   3 - Classic Edit Menu and submenus (^K/^Q/^O/^P/^M) both displayed.
///   2 - Classic Edit Menu hidden; submenus still displayed after a pause.
///   1 - No classic menus displayed.
///   0 - Same as 1, plus the status line is hidden too -- WordTsar's own
///       choice for "as minimal as it gets," not from the manual, which
///       instead distinguishes 0 by letting you operate on hidden blocks
///       without confirmation (not modeled here).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::ChangeHelpLevel(void)
{
    const char* qtTesting = std::getenv("QT_TESTING");
    if (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0)
    {
        return;
    }

    bool ok = false ;
    int level = QInputDialog::getInt(this, "Help Level", "What help level do you want? (0-4)",
                                      mHelpLevel, 0, 4, 1, &ok) ;
    if(ok == false)
    {
        return ;
    }

    bool wasLevel0 = (mHelpLevel == 0) ;
    mHelpLevel = level ;
    mHelpDisplay = (level == 3) ? HELP_MAIN : HELP_NONE ;

    if(level == 0)
    {
        // Only capture the pre-level-0 state on the transition into it --
        // selecting 0 again while already at 0 must not overwrite the saved
        // flag with the current (already-false) mDispStatusBar, or the next
        // transition back out would have nothing true to restore.
        if(wasLevel0 == false)
        {
            mDispStatusBarBeforeHelpLevel0 = mDispStatusBar ;
        }
        mDispStatusBar = false ;
    }
    else if(mDispStatusBarBeforeHelpLevel0 == true)
    {
        mDispStatusBar = true ;
    }
}


/////////////////////////////////////////////////////////////////////////////
// SPELL CHECKING
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Spell check the entire document starting from current position.
/// [REFACTORED from editorctrl.cpp::SpellCheckDocument()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SpellCheckDocument(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::SpellCheckDocument() ***

    cSpellCheck dialog(this, SPELLCHECKDOCUMENT) ;

    dialog.CheckDocument() ;

    ScrollIntoView() ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Spell check the word at the current cursor position.
/// Uses Unicode word boundary detection to find the word, then
/// shows the spell check dialog with suggestions if misspelled.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SpellCheckWord(void)
{
    cSpellCheck dialog(this, SPELLCHECKWORD) ;
    dialog.CheckWord() ;
    ScrollIntoView() ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Prompt the user to enter a word, then check its spelling.
/// Shows the spell check dialog with suggestions if misspelled.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SpellCheckEnterWord(void)
{
    cSpellCheck dialog(this, SPELLENTERWORD) ;
    dialog.CheckEnteredWord() ;
    ScrollIntoView() ;
}


/////////////////////////////////////////////////////////////////////////////
// UNDO/REDO
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Undo the last editing operation. Closes any active typing group first
/// so that word-level grouping is finalized, then calls cDocument::Undo()
/// and updates layout and display.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Undo(void)
{
    if (!mDocument)
    {
        return ;
    }

    CloseTypingGroup() ;

    if (mDocument->Undo())
    {
        // update caret from document position
        mCaretDocumentPosition = mDocument->GetPosition() ;

        LayoutDocument(true) ;
        CalculateCaretPosition() ;
        ScrollIntoView() ;
        update() ;

        // Notify sibling -- notifications suppressed during undo/redo
        if (mSiblingEditor != nullptr)
        {
            mSiblingEditor->LayoutDocument(true) ;
            mSiblingEditor->PerformPostCommandUpdate() ;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Redo the last undone operation. Closes any active typing group first,
/// then calls cDocument::Redo() and updates layout and display.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Redo(void)
{
    if (!mDocument)
    {
        return ;
    }

    CloseTypingGroup() ;

    if (mDocument->Redo())
    {
        // update caret from document position
        mCaretDocumentPosition = mDocument->GetPosition() ;

        LayoutDocument(true) ;
        CalculateCaretPosition() ;
        ScrollIntoView() ;
        update() ;

        // Notify sibling -- notifications suppressed during undo/redo
        if (mSiblingEditor != nullptr)
        {
            mSiblingEditor->LayoutDocument(true) ;
            mSiblingEditor->PerformPostCommandUpdate() ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
// UI DIALOGS
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display page layout dialog to set page size, margins, and orientation.
/// [REFACTORED from editorctrl.cpp::PageLayout()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::PageLayout(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::PageLayout() ***

    QDialog dialog(this);

    Ui::PageLayout pagelayout;
    pagelayout.setupUi(&dialog);

    double value;
    QString text;

    // Populate page size combo list
    for (int loop = 1; loop < 200; loop++)
    {
        QPageSize psize(static_cast<QPageSize::PageSizeId>(loop));
        if (psize.isValid() == false)
        {
            continue;
        }

        QString name = psize.name();

        QString full;
        QSizeF size;

        if (mMeasure == MSR_CENTIMETERS || mMeasure == MSR_MILLIMETERS)
        {
            size = psize.size(QPageSize::Millimeter);
            full = string_sprintf("%s, %d x %d mm", name.toUtf8().constData(), static_cast<int>(size.width()), static_cast<int>(size.height())).c_str();
        }
        else
        {
            size = psize.size(QPageSize::Inch);
            full = string_sprintf("%s, %.2f x %.2f in", name.toUtf8().constData(), size.width(), size.height()).c_str();
        }

        pagelayout.mPaperSize->addItem(full, loop);
    }

    pagelayout.mPaperSize->setCurrentIndex(mPaperId - 1);

    // @TODO page orientation set
    pagelayout.mOrientation->setEnabled(false);

    // API UPDATE: mLayout becomes GetLayout()
    cLayoutBase* layout = GetLayout();

    // BLACK BOX API: Use getters instead of direct member access
    value = layout->GetPageOffsetOdd();
    switch (mMeasure)
    {
        case MSR_INCHES:
            text = string_sprintf("%0.2f\"", value / TWIPSPERINCH).c_str();
            break;

        case MSR_MILLIMETERS:
            text = string_sprintf("%0.2f\"", value / TWIPSPERMM).c_str();
            break;

        default:
            text = string_sprintf("%0.2f\"", value / TWIPSPERCM).c_str();
            break;
    }
    pagelayout.mOddPageOffset->setText(text);

    value = layout->GetPageOffsetEven();
    switch (mMeasure)
    {
        case MSR_INCHES:
            text = string_sprintf("%0.2f\"", value / TWIPSPERINCH).c_str();
            break;

        case MSR_MILLIMETERS:
            text = string_sprintf("%0.2f\"", value / TWIPSPERMM).c_str();
            break;

        default:
            text = string_sprintf("%0.2f\"", value / TWIPSPERCM).c_str();
            break;
    }
    pagelayout.mEvenPageOffset->setText(text);

    value = layout->GetTopMargin();
    switch (mMeasure)
    {
        case MSR_INCHES:
            text = string_sprintf("%0.2f\"", value / TWIPSPERINCH).c_str();
            break;

        case MSR_MILLIMETERS:
            text = string_sprintf("%0.2f\"", value / TWIPSPERMM).c_str();
            break;

        default:
            text = string_sprintf("%0.2f\"", value / TWIPSPERCM).c_str();
            break;
    }
    pagelayout.mTopMargin->setText(text);

    value = layout->GetBottomMargin();
    switch (mMeasure)
    {
        case MSR_INCHES:
            text = string_sprintf("%0.2f\"", value / TWIPSPERINCH).c_str();
            break;

        case MSR_MILLIMETERS:
            text = string_sprintf("%0.2f\"", value / TWIPSPERMM).c_str();
            break;

        default:
            text = string_sprintf("%0.2f\"", value / TWIPSPERCM).c_str();
            break;
    }
    pagelayout.mBottomMargin->setText(text);

    value = layout->GetRightMargin();
    switch (mMeasure)
    {
        case MSR_INCHES:
            text = string_sprintf("%0.2f\"", value / TWIPSPERINCH).c_str();
            break;

        case MSR_MILLIMETERS:
            text = string_sprintf("%0.2f\"", value / TWIPSPERMM).c_str();
            break;

        default:
            text = string_sprintf("%0.2f\"", value / TWIPSPERCM).c_str();
            break;
    }
    pagelayout.mRightMargin->setText(text);

    value = layout->GetHeaderMargin();
    switch (mMeasure)
    {
        case MSR_INCHES:
            text = string_sprintf("%0.2f\"", value / TWIPSPERINCH).c_str();
            break;

        case MSR_MILLIMETERS:
            text = string_sprintf("%0.2f\"", value / TWIPSPERMM).c_str();
            break;

        default:
            text = string_sprintf("%0.2f\"", value / TWIPSPERCM).c_str();
            break;
    }
    pagelayout.mHeader->setText(text);

    value = layout->GetFooterMargin();
    switch (mMeasure)
    {
        case MSR_INCHES:
            text = string_sprintf("%0.2f\"", value / TWIPSPERINCH).c_str();
            break;

        case MSR_MILLIMETERS:
            text = string_sprintf("%0.2f\"", value / TWIPSPERMM).c_str();
            break;

        default:
            text = string_sprintf("%0.2f\"", value / TWIPSPERCM).c_str();
            break;
    }
    pagelayout.mFooter->setText(text);


    int ecode = dialog.exec();


    if (ecode != 0)
    {
        // API UPDATE: mDocument becomes GetDocument()
        cDocument* doc = GetDocument();

        // Insert anything we need at the beginning of our current paragraph
        PARAGRAPH_T para = doc->GetParagraphFromPosition(doc->GetPosition());
        POSITION_T start, end;
        doc->GetParagraphStartandEnd(para, start, end);
        doc->SetPosition(start);

        int newtype = pagelayout.mPaperSize->currentIndex();
        int newpaper = pagelayout.mPaperSize->currentData().toInt();
        if (newtype != mPaperId)
        {
            mPaperId = static_cast<QPageSize::PageSizeId>(newpaper);

            QString t = string_sprintf(".pt %d (%s)", newpaper, pagelayout.mPaperSize->currentText().toUtf8().constData()).c_str();
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        bool incdec;
        QString str = pagelayout.mOddPageOffset->text();
        double num = doc->GetValue(str.toStdString(), incdec);
        char type = doc->GetType(str.toStdString());
        value = doc->ConvertToTwips(num, type);
        if (!CoordsEqual(value, layout->GetPageOffsetOdd()))
        {
            QString t;
            switch (mMeasure)
            {
                case MSR_INCHES:
                    t = string_sprintf(".poo %0.2f\"", value / TWIPSPERINCH).c_str();
                    break;

                case MSR_MILLIMETERS:
                    t = string_sprintf(".poo %0.2fmm", value / TWIPSPERMM).c_str();
                    break;

                default:
                    t = string_sprintf(".poo %0.2fcm", value / TWIPSPERCM).c_str();
                    break;
            }
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        str = pagelayout.mEvenPageOffset->text();
        num = doc->GetValue(str.toStdString(), incdec);
        type = doc->GetType(str.toStdString());
        value = doc->ConvertToTwips(num, type);
        if (!CoordsEqual(value, layout->GetPageOffsetEven()))
        {
            QString t;
            switch (mMeasure)
            {
                case MSR_INCHES:
                    t = string_sprintf(".poe %0.2f\"", value / TWIPSPERINCH).c_str();
                    break;

                case MSR_MILLIMETERS:
                    t = string_sprintf(".poe %0.2fmm", value / TWIPSPERMM).c_str();
                    break;

                default:
                    t = string_sprintf(".poe %0.2fcm", value / TWIPSPERCM).c_str();
                    break;
            }
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        str = pagelayout.mTopMargin->text();
        num = doc->GetValue(str.toStdString(), incdec);
        type = doc->GetType(str.toStdString());
        value = doc->ConvertToTwips(num, type);
        if (!CoordsEqual(value, layout->GetTopMargin()))
        {
            QString t;
            switch (mMeasure)
            {
                case MSR_INCHES:
                    t = string_sprintf(".mt %0.2f\"", value / TWIPSPERINCH).c_str();
                    break;

                case MSR_MILLIMETERS:
                    t = string_sprintf(".mt %0.2fmm", value / TWIPSPERMM).c_str();
                    break;

                default:
                    t = string_sprintf(".mt %0.2fcm", value / TWIPSPERCM).c_str();
                    break;
            }
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        str = pagelayout.mBottomMargin->text();
        num = doc->GetValue(str.toStdString(), incdec);
        type = doc->GetType(str.toStdString());
        value = doc->ConvertToTwips(num, type);
        if (!CoordsEqual(value, layout->GetBottomMargin()))
        {
            QString t;
            switch (mMeasure)
            {
                case MSR_INCHES:
                    t = string_sprintf(".mb %0.2f\"", value / TWIPSPERINCH).c_str();
                    break;

                case MSR_MILLIMETERS:
                    t = string_sprintf(".mb %0.2fmm", value / TWIPSPERMM).c_str();
                    break;

                default:
                    t = string_sprintf(".mb %0.2fcm", value / TWIPSPERCM).c_str();
                    break;
            }
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        str = pagelayout.mRightMargin->text();
        num = doc->GetValue(str.toStdString(), incdec);
        type = doc->GetType(str.toStdString());
        value = doc->ConvertToTwips(num, type);
        if (!CoordsEqual(value, layout->GetRightMargin()))
        {
            QString t;
            switch (mMeasure)
            {
                case MSR_INCHES:
                    t = string_sprintf(".rm %0.2f\"", value / TWIPSPERINCH).c_str();
                    break;

                case MSR_MILLIMETERS:
                    t = string_sprintf(".rm %0.2fmm", value / TWIPSPERMM).c_str();
                    break;

                default:
                    t = string_sprintf(".rm %0.2fcm", value / TWIPSPERCM).c_str();
                    break;
            }
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        str = pagelayout.mHeader->text();
        num = doc->GetValue(str.toStdString(), incdec);
        type = doc->GetType(str.toStdString());
        value = doc->ConvertToTwips(num, type);
        if (!CoordsEqual(value, layout->GetHeaderMargin()))
        {
            QString t;
            switch (mMeasure)
            {
                case MSR_INCHES:
                    t = string_sprintf(".hm %0.2f\"", value / TWIPSPERINCH).c_str();
                    break;

                case MSR_MILLIMETERS:
                    t = string_sprintf(".hm %0.2fmm", value / TWIPSPERMM).c_str();
                    break;

                default:
                    t = string_sprintf(".hm %0.2fcm", value / TWIPSPERCM).c_str();
                    break;
            }
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        str = pagelayout.mFooter->text();
        num = doc->GetValue(str.toStdString(), incdec);
        type = doc->GetType(str.toStdString());
        value = doc->ConvertToTwips(num, type);
        if (!CoordsEqual(value, layout->GetFooterMargin()))
        {
            QString t;
            switch (mMeasure)
            {
                case MSR_INCHES:
                    t = string_sprintf(".fm %0.2f\"", value / TWIPSPERINCH).c_str();
                    break;

                case MSR_MILLIMETERS:
                    t = string_sprintf(".fm %0.2fmm", value / TWIPSPERMM).c_str();
                    break;

                default:
                    t = string_sprintf(".fm %0.2fcm", value / TWIPSPERCM).c_str();
                    break;
            }
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }

        QString t;
        if (pagelayout.mLandscape->isChecked())
        {
            t = ".pr or=p";
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }
        else
        {
            t = ".pr or=l";
            doc->Insert(t.toUtf8().constData());
            doc->Insert(HARD_RETURN);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display print preview dialog.
/// [REFACTORED from editorctrl.cpp::PrintPreview()]
///
/// @note
/// Uses cPrintout which handles all printing/preview logic.
/// The new system's layout is always up-to-date, so no need for
/// special print layout or IdleLayout calls like the old system.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::PrintPreview(void)
{
    // *** SIMPLIFIED FROM OLD SYSTEM - editorctrl.cpp::PrintPreview() ***

    // The new system doesn't need mPrintLayout or special layout handling
    // because the main layout is already in the correct format for printing.
    // cPrintout can use the editor's existing layout directly.

    // API UPDATE: cPrintout becomes cPrintout
    cPrintout print(this);
    print.PrintPreview();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display the system print dialog and send the document straight to a
/// printer, bypassing preview. Uses the same cPrintout engine as
/// PrintPreview.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Print(void)
{
    cPrintout print(this);
    print.PrintDocument();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display font selection dialog and insert selected font into document.
/// [REFACTORED from editorctrl.cpp::SelectFont()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SelectFont(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::SelectFont() ***

    bool ok;
    QFont font = QFontDialog::getFont(&ok, this);

    if (ok)
    {
        sInternalFonts intfont;
        intfont.name = font.family().toStdString();
        intfont.fontname = font.family().toStdString();
        intfont.size = font.pointSizeF();
        intfont.haveWSFont = false;
        // Typestyle bits are set by the file saver (cWSFontClassifier)
        // at save time, not here. SelectFont only stores the font identity.

        // API UPDATE: mDocument becomes GetDocument()
        GetDocument()->InsertFont(intfont);

        // API UPDATE: use existing method (already refactored)
        LayoutDocument(false);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display color selection dialog and insert selected color into document.
/// [REFACTORED from editorctrl.cpp::SelectColor()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SelectColor(void)
{
    cColorDialog dlg(this) ;
    if (dlg.exec() == QDialog::Accepted)
    {
        sSeqRGBColor color = dlg.GetSelectedColor() ;
        GetDocument()->InsertColor(color) ;
        LayoutDocument(false) ;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display About dialog with version information.
/// [REFACTORED from editorctrl.cpp::About()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::About(void)
{
    // Helper to format bytes as KB or MB
    auto formatBytes = [](size_t bytes) -> QString
    {
        if (bytes >= 1024 * 1024)
        {
            return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB" ;
        }
        return QString::number(bytes / 1024.0, 'f', 1) + " KB" ;
    } ;

    // Collect memory usage from subsystems
    sDocumentMemoryUsage docMem = {} ;
    if (mDocument)
    {
        docMem = mDocument->GetMemoryUsage() ;
    }

    sLayoutMemoryUsage layMem = {} ;
    if (mLayout)
    {
        layMem = mLayout->GetMemoryUsage() ;
    }

    // Helper to format "used / allocated" pair
    auto formatUsedAlloc = [&formatBytes](size_t used, size_t alloc) -> QString
    {
        return formatBytes(used) + " / " + formatBytes(alloc) ;
    } ;

    // Calculate totals (allocated)
    size_t docTotal = docMem.textBytes + docMem.attributeBytes +
                      docMem.undoBytes + docMem.redoBytes + docMem.copyBufferBytes ;
    size_t layTotal = layMem.paragraphBytes + layMem.lineBytes + layMem.segmentBytes +
                      layMem.checkpointBytes + layMem.boxBytes ;
    size_t grandTotal = docTotal + layTotal ;

    // Calculate totals (used)
    size_t docUsedTotal = docMem.textUsedBytes + docMem.attributeUsedBytes +
                          docMem.undoBytes + docMem.redoBytes + docMem.copyBufferBytes ;
    size_t layUsedTotal = layMem.paragraphUsedBytes + layMem.lineUsedBytes + layMem.segmentUsedBytes +
                          layMem.checkpointBytes + layMem.boxBytes ;
    size_t grandUsedTotal = docUsedTotal + layUsedTotal ;

    // Format the display text (used / allocated)
    QString text ;
    text += QString("WordTsar %1\n\n").arg(FULLVERSION_STRING) ;
    text += QString("Memory: used / allocated\n\n") ;

    text += QString("Document: %1 paragraphs\n").arg(docMem.paragraphCount) ;
    text += QString("  Text:          %1\n").arg(formatUsedAlloc(docMem.textUsedBytes, docMem.textBytes)) ;
    text += QString("  Attributes:    %1\n").arg(formatUsedAlloc(docMem.attributeUsedBytes, docMem.attributeBytes)) ;
    text += QString("    pairs:       %1\n").arg(formatBytes(docMem.attrPairsBytes)) ;
    text += QString("    format:      %1\n").arg(formatBytes(docMem.attrFormatBytes)) ;
    text += QString("    font:        %1\n").arg(formatBytes(docMem.attrFontBytes)) ;
    text += QString("    tab:         %1\n").arg(formatBytes(docMem.attrTabBytes)) ;
    text += QString("    color:       %1\n").arg(formatBytes(docMem.attrColorBytes)) ;
    text += QString("    footnote:    %1\n").arg(formatBytes(docMem.attrFootnoteBytes)) ;
    text += QString("    endnote:     %1\n").arg(formatBytes(docMem.attrEndnoteBytes)) ;
    text += QString("    variable:    %1\n").arg(formatBytes(docMem.attrVariableBytes)) ;
    text += QString("    offsets:     %1\n").arg(formatBytes(docMem.attrOffsetsBytes)) ;
    text += QString("  Undo:          %1  (%2 groups)\n").arg(formatBytes(docMem.undoBytes)).arg(docMem.undoGroupCount) ;
    text += QString("  Redo:          %1  (%2 groups)\n").arg(formatBytes(docMem.redoBytes)).arg(docMem.redoGroupCount) ;
    text += QString("  Copy buffer:   %1\n").arg(formatBytes(docMem.copyBufferBytes)) ;
    text += QString("  Subtotal:      %1\n\n").arg(formatUsedAlloc(docUsedTotal, docTotal)) ;

    text += QString("Layout: %1 paragraphs, %2 lines, %3 segments\n")
                .arg(layMem.paragraphCount).arg(layMem.lineCount).arg(layMem.segmentCount) ;
    text += QString("  Paragraphs:    %1\n").arg(formatUsedAlloc(layMem.paragraphUsedBytes, layMem.paragraphBytes)) ;
    text += QString("  Lines:         %1\n").arg(formatUsedAlloc(layMem.lineUsedBytes, layMem.lineBytes)) ;
    text += QString("  Segments:      %1\n").arg(formatUsedAlloc(layMem.segmentUsedBytes, layMem.segmentBytes)) ;
    text += QString("    struct:      %1\n").arg(formatBytes(layMem.segStructBytes)) ;
    text += QString("    positions:   %1\n").arg(formatBytes(layMem.segPositionBytes)) ;
    text += QString("    fonts:       %1\n").arg(formatBytes(layMem.segFontBytes)) ;
    text += QString("    ctrl codes:  %1\n").arg(formatBytes(layMem.segControlBytes)) ;
    text += QString("  Checkpoints:   %1  (%2 checkpoints)\n").arg(formatBytes(layMem.checkpointBytes)).arg(layMem.checkpointCount) ;
    text += QString("  Pages/Boxes:   %1\n").arg(formatBytes(layMem.boxBytes)) ;
    text += QString("  Subtotal:      %1\n\n").arg(formatUsedAlloc(layUsedTotal, layTotal)) ;

    text += QString("Total:           %1\n").arg(formatUsedAlloc(grandUsedTotal, grandTotal)) ;

    QMessageBox::information(this, "WordTsar Status", text) ;
}


/////////////////////////////////////////////////////////////////////////////
// TAB OPERATIONS
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
// SETTINGS
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle show control codes display mode.
/// Cycles through: SHOW_ALL to SHOW_DOT to SHOW_NONE to SHOW_ALL
/// [REFACTORED from F2 key handler in editorctrl.cpp::keyPressEvent()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::ToggleShowControl(void)
{
    // Guard: need layout
    if (!mLayout)
    {
        return;
    }

    // If reveal codes pane is visible, ^OD always toggles it off
    if (mWordTsar != nullptr && mWordTsar->IsRevealCodesVisible())
    {
        mWordTsar->ToggleRevealCodes() ;
        return;
    }

    if (GetDisplayMode() == DISPLAY_PAGE)
    {
        // Page mode: toggle reveal codes pane on
        if (mWordTsar != nullptr)
        {
            mWordTsar->ToggleRevealCodes() ;
        }
    }
    else
    {
        // Continuous mode: cycle SHOW_ALL to SHOW_DOT to SHOW_NONE to SHOW_ALL
        eShowControl currentMode = mLayout->GetShowControl();
        eShowControl nextMode;

        if (currentMode == SHOW_ALL)
        {
            nextMode = SHOW_DOT;
        }
        else if (currentMode == SHOW_DOT)
        {
            nextMode = SHOW_NONE;
        }
        else
        {
            nextMode = SHOW_ALL;
        }

        // Set new mode (syncs to both layout and document)
        SetShowControls(nextMode);

        // Re-layout document (showing/hiding control codes affects layout)
        mLayout->LayoutDocument(mDocument);

        // Update display
        UpdateScrollbar();
        update();
    }
}



/////////////////////////////////////////////////////////////////////////////
// FILE I/O
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string filename [in] - File to load
///
/// @return bool - true if loaded successfully
///
/// @brief
/// Load a file. Detects file type and uses appropriate file reader.
/// Supports WordStar 4, WordStar 7, RTF, DOCX, Text formats.
///
/// [REFACTORED from editorctrl.cpp::LoadFile()]
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::LoadFile(const std::string& filename)
{
    if (filename.empty())
    {
        return false;
    }

    // Convert to QString for QFileInfo path operations later
    QString qfilename = QString::fromStdString(filename);

    // Detect file type using each handler's CheckType()
    // WordStar CheckType includes content-based detection for unknown extensions
    cFile* fileReader = nullptr;

    cWordstarFile wsCheck(this);
    cRTFFile rtfCheck(this);
    cDOCXFile docxCheck(this);

    if (wsCheck.CheckType(filename))
    {
        fileReader = new cWordstarFile(this);
    }
    else if (rtfCheck.CheckType(filename))
    {
        fileReader = new cRTFFile(this);
    }
    else if (docxCheck.CheckType(filename))
    {
        fileReader = new cDOCXFile(this);
    }
    else
    {
        // No known format detected: load as plain text
        fileReader = new cTextFile(this);
    }

    // Save dirty flag before loading (Insert() sets mChanged for every character)
    // Restore after so loading doesn't change the document's dirty state
    bool wasDirty = mDocument->mChanged;

    // Block idle layout during file loading
    // QProgressDialog processes events, which could trigger OnIdle before loading completes
    mDocument->SetLoading(true);

    // Start busy indicator in status bar
    StartProgress("Loading...");

    // Block UI re-entrancy: FileIOProgress() pumps the event loop via processEvents().
    QProgressDialog progress("Loading file...", "Cancel", 0, 100, this);
    progress.setWindowModality(Qt::ApplicationModal);
    mProgress = &progress;
    progress.show();

    // Load file using file handler
    bool success = fileReader->LoadFile(filename);

    // Clean up progress dialog
    mProgress = nullptr;

    // Stop busy indicator
    StopProgress();

    // Re-enable idle layout
    mDocument->SetLoading(false);

    // Restore dirty flag (loading shouldn't change dirty state)
    mDocument->mChanged = wasDirty;

    delete fileReader;

    if (success)
    {
        // Reset editor state for newly loaded document
        SetScrollOffset(0);

        // Unset any block selection and clear undo history
        if (mDocument)
        {
            mDocument->UnsetBlock();
            mDocument->SetPosition(0);  // Move to start of document
            mDocument->ClearUndoHistory();
        }

        // Restore per-file editor state (cursor, saved positions, block)
        LoadFileState(filename);

        // Trigger full layout
        LayoutDocument(true);  // Force layout

        // Recalculate caret position
        CalculateCaretPosition();

        // Scroll to show caret
        ScrollIntoView();

        // Recalculate after scroll adjusts viewport (mCaretY depends on mScrollOffset)
        CalculateCaretPosition();

        // Update display
        update();

        // Store filename, directory, and file-set flag
        QFileInfo fi(qfilename);
        mFileName = fi.fileName().toStdString();
        mFileDir = fi.path().toStdString();
        mFileSet = true;

        // Wire filename to layout for variable expansion (&*&, &.&, &\&)
        mLayout->SetFilename(mFileName);
        mLayout->SetFileDir(mFileDir);

        // Update window title with new filename
        SetTitle(mFileName);

        // Compute backup filename -- always use .ws extension for backups
        // regardless of original file format, to avoid issues with RTF/DOCX loading
        mBackupFileName = (fi.path() + "/" + fi.completeBaseName() + "-bak.ws").toStdString();

        // Emit signal for file loaded (signal not yet implemented)
        // emit FileLoaded(qfilename);
    }

    return success;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  QString filename [in] - File to save to
///
/// @return bool - true if saved successfully
///
/// @brief
/// Save the document. Detects file type from extension.
/// Supports WordStar 7, RTF, Text formats for saving.
///
/// [REFACTORED from editorctrl.cpp::SaveFile()]
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::SaveFile(const std::string& filename)
{
    QString qfilename = QString::fromStdString(filename);
    if (qfilename.isEmpty() || !mDocument)
    {
        return false;
    }

    // Detect file type from extension and create appropriate file handler
    cFile* fileWriter = nullptr;

    if (qfilename.endsWith(".ws", Qt::CaseInsensitive) ||
        qfilename.endsWith(".ws7", Qt::CaseInsensitive) ||
        qfilename.endsWith(".ws4", Qt::CaseInsensitive))
    {
        fileWriter = new cWordstarFile(this);
    }
    else if (qfilename.endsWith(".rtf", Qt::CaseInsensitive))
    {
        fileWriter = new cRTFFile(this);
    }
    else if (qfilename.endsWith(".txt", Qt::CaseInsensitive))
    {
        fileWriter = new cTextFile(this);
    }
    else if (qfilename.endsWith(".docx", Qt::CaseInsensitive))
    {
        fileWriter = new cDOCXFile(this);
    }
    else
    {
        // Default to WordStar 7 format
        fileWriter = new cWordstarFile(this);
    }

    // Get document size for save
    POSITION_T docSize = mDocument->GetTextSize();

    // Start busy indicator in status bar
    StartProgress("Saving...");

    // Save file using file handler
    bool success = fileWriter->SaveFile(filename, docSize);

    // Stop busy indicator
    StopProgress();

    delete fileWriter;

    if (success)
    {
        // Show "File saved" on status bar
        if (mWordTsar)
        {
            mWordTsar->SetStatus("File saved", false, 0);
        }

        // Store filename and wire to layout for variable expansion
        QFileInfo saveInfo(qfilename);
        mFileName = saveInfo.fileName().toStdString();
        mFileDir = saveInfo.path().toStdString();
        mLayout->SetFilename(mFileName);
        mLayout->SetFileDir(mFileDir);

        // Update window title with new filename
        SetTitle(mFileName);

        // Compute backup filename -- always .ws regardless of original format
        QFileInfo fi(qfilename);
        mBackupFileName = (fi.path() + "/" + fi.completeBaseName() + "-bak.ws").toStdString();

        // Create backup file in WordStar format (except for DOCX)
        if (!qfilename.endsWith(".docx", Qt::CaseInsensitive))
        {
            cFile* backupWriter = new cWordstarFile(this);
            POSITION_T backupSize = mDocument->GetTextSize();
            backupWriter->SaveFile(mBackupFileName, backupSize);
            delete backupWriter;
        }

        // Sync reveal codes visibility to base member before saving per-file state
        if (mWordTsar != nullptr)
        {
            mRevealCodesVisible = mWordTsar->IsRevealCodesVisible();
        }

        // Save per-file editor state (cursor, saved positions, block, display mode, reveal codes)
        SaveFileState(filename);
    }
    else
    {
        const char* qtTesting = std::getenv("QT_TESTING");
        if (qtTesting == nullptr || std::strcmp(qtTesting, "1") != 0)
        {
            ShowError("Error", "File save failed.");
        }
    }

    return success;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  char *text [in] - Crash message to include in emergency file
///
/// @return nothing
///
/// @brief
/// Emergency save when crashing. Saves to emergency-[timestamp].ws file.
/// Called by crash handler.
///
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::EmergencySaveFile(char *text)
{
    if (!mDocument)
    {
        fprintf(stderr, "Emergency save FAILED - no document!\n");
        if (text)
        {
            fprintf(stderr, "Crash message: %s\n", text);
        }
        return;
    }

    // Generate emergency filename with timestamp
    time_t now = time(nullptr);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", localtime(&now));

    std::string emergencyFilename = std::string("emergency-") + timestamp + ".ws";

    // Save document without progress UI (we're crashing)
    cWordstarFile fileWriter(this);
    POSITION_T docSize = mDocument->GetTextSize();
    bool success = fileWriter.SaveFile(emergencyFilename, docSize);

    if (success)
    {
        // Log to stderr (not UI - we're crashing)
        fprintf(stderr, "Emergency save successful: %s\n", emergencyFilename.c_str());
        if (text)
        {
            fprintf(stderr, "Crash message: %s\n", text);
        }
    }
    else
    {
        fprintf(stderr, "Emergency save FAILED: %s\n", emergencyFilename.c_str());
        if (text)
        {
            fprintf(stderr, "Crash message: %s\n", text);
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int percent [in] - Progress percentage (0-100)
///
/// @return nothing
///
/// @brief
/// Update progress display during file I/O operations.
/// Called by file handlers during load/save operations.
///
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::FileIOProgress(int percent)
{
    // Update progress dialog if active
    if (mProgress != nullptr)
    {
        mProgress->setValue(percent);
    }

    // Process events to keep UI responsive during I/O
    QApplication::processEvents();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string message [in] - Progress message to display
///
/// @return nothing
///
/// @brief
/// Start progress indicator in status bar with message.
/// Calls cWordTsar::SetStatus() to show animated busy indicator.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::StartProgress(const std::string& message)
{
    if (mWordTsar)
    {
        mWordTsar->SetStatus(QString::fromStdString(message), true, 0);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int percent [in] - Progress percentage (0-100)
///
/// @return nothing
///
/// @brief
/// Update progress indicator percentage.
/// Updates status bar message with percentage value.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::UpdateProgress(int percent)
{
    if (mWordTsar)
    {
        QString msg = QString("Progress: %1%").arg(percent);
        mWordTsar->SetStatus(msg, true, percent);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Stop progress indicator and clear status message.
/// Hides the animated busy indicator in the status bar.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::StopProgress(void)
{
    if (mWordTsar)
    {
        mWordTsar->SetStatus("", false, 0);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool forSave [in] - true for save dialog, false for load dialog
///
/// @return QString - Filter string for QFileDialog (e.g., "WordStar Files (*.ws);;RTF Files (*.rtf)")
///
/// @brief
/// Build file filter string by querying all file handler types.
/// Respects CanLoad()/CanSave() flags for each handler.
///
///
/////////////////////////////////////////////////////////////////////////////
QString cEditorCtrl::BuildFileFilterString(bool forSave)
{
    QStringList filters;

    // Create instances of each file handler to query capabilities
    cWordstarFile wsHandler(this);
    cRTFFile rtfHandler(this);
    cDOCXFile docxHandler(this);
    cTextFile textHandler(this);

    // Add WordStar if supported
    if ((forSave && wsHandler.CanSave()) || (!forSave && wsHandler.CanLoad()))
    {
        filters << QString::fromStdString(wsHandler.GetExtensions());
    }

    // Add RTF if supported
    if ((forSave && rtfHandler.CanSave()) || (!forSave && rtfHandler.CanLoad()))
    {
        filters << QString::fromStdString(rtfHandler.GetExtensions());
    }

    // Add DOCX if supported (load only)
    if ((forSave && docxHandler.CanSave()) || (!forSave && docxHandler.CanLoad()))
    {
        filters << QString::fromStdString(docxHandler.GetExtensions());
    }

    // Add Text/All Files if supported
    if ((forSave && textHandler.CanSave()) || (!forSave && textHandler.CanLoad()))
    {
        filters << QString::fromStdString(textHandler.GetExtensions());
    }

    // Join with ;; separator for Qt file dialog
    return filters.join(";;");
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - Selected filename, or empty string if cancelled
///
/// @brief
/// Show file open dialog with appropriate file type filters.
/// Returns filename if user selects a file, empty string if cancelled.
///
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorCtrl::PromptForLoadFile(void)
{
    QString filters = BuildFileFilterString(false);  // false = load dialog
    QString filename = QFileDialog::getOpenFileName(
        this,
        "Open File",
        QString::fromStdString(mFileDir),
        filters
    );

    return filename.toStdString();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return QString - Selected filename, or empty string if cancelled
///
/// @brief
/// Show file save dialog with appropriate file type filters.
/// Returns filename if user selects a file, empty string if cancelled.
///
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorCtrl::PromptForSaveFile(void)
{
    QString filters = BuildFileFilterString(true);  // true = save dialog
    QString filename = QFileDialog::getSaveFileName(
        this,
        "Save File",
        QString::fromStdString(mFileDir),
        filters
    );

    return filename.toStdString();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if a file was loaded, false if user cancelled or load failed
///
/// @brief
/// Prompt for a file to load, then load it. Used by menu File->Open.
/// Logic mirrors cWordStarInput::OnControlKChar case 'r' so WordStar-mode
/// keyboard input and menu invocation produce identical behavior.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::Open(void)
{
    std::string filename = PromptForLoadFile();
    if (filename.empty())
    {
        return false;
    }

    cDocument* doc = GetDocument();
    POSITION_T position = doc ? doc->GetPosition() : 0;
    SetEnabled(false);

    std::filesystem::path filepath(filename);
    mFileName = filepath.filename().string();
    mFileDir = filepath.parent_path().string() + '/';
    mFileSet = true;

    std::string loadfile = mFileDir + mFileName;
    bool ok = LoadFile(loadfile);

    SetEnabled(true);

    if (doc)
    {
        doc->SetPosition(position);
    }

    return ok;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true on successful save, false otherwise (including no-op for unchanged doc)
///
/// @brief
/// Save the current document. If no filename is set, fall back to SaveAs.
/// No-op if document is unchanged. Mirrors cWordStarInput::OnControlKChar
/// case 's'.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::Save(void)
{
    if (GetDocument() == nullptr || GetDocument()->mChanged == false)
    {
        return false;
    }

    if (mFileSet == false)
    {
        return SaveAs();
    }

    std::string fname = mFileDir + "/" + mFileName;
    bool ok = SaveFile(fname);
    if (ok == false)
    {
        ShowError("Error", "File Save failed");
        return false;
    }

    GetDocument()->mChanged = false;
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if file was saved, false if user cancelled or save failed
///
/// @brief
/// Prompt for a save filename, then save. Updates mFileName/mFileDir/mFileSet
/// and the window title. Mirrors cWordStarInput::OnControlKChar case 't'.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::SaveAs(void)
{
    std::string filename = PromptForSaveFile();
    if (filename.empty())
    {
        return false;
    }

    std::filesystem::path filepath(filename);
    mFileDir = filepath.parent_path().string();
    mFileName = filepath.filename().string();
    mFileSet = true;

    bool ok = SaveFile(filename);
    if (ok == false)
    {
        return false;
    }

    if (GetDocument())
    {
        GetDocument()->mChanged = false;
    }

    std::string msg = string_sprintf("File %s saved", filename.c_str());
    ShowMessage("Save OK", msg);

    std::string temp = mFileName;
    SetTitle(temp);
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if save succeeded (or doc was unchanged) and document was cleared
///
/// @brief
/// Save the current document then clear the buffer to a fresh "Unknown.ws".
/// Mirrors cWordStarInput::OnControlKChar case 'd'.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::SaveAndClose(void)
{
    bool ok = false;

    if (GetDocument() && GetDocument()->mChanged)
    {
        if (mFileSet != false)
        {
            std::string fname = mFileDir + "/" + mFileName;
            ok = SaveFile(fname);
            if (ok == false)
            {
                ShowError("Error", "File Save failed");
            }
            else
            {
                GetDocument()->mChanged = false;
            }
        }
        else
        {
            ok = SaveAs();
        }
    }

    if (ok == true && GetDocument())
    {
        GetDocument()->Clear();
        LayoutDocument(true);
        mFileDir = DefaultFileDir();
        mFileName = "Unknown.ws";
        mFileSet = false;
        SetTitle(mFileName);
        GetLayout()->SetFilename(mFileName);
        GetLayout()->SetFileDir(mFileDir);
    }

    return ok;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Request application exit. Mirrors cWordStarInput::OnControlKChar case 'x'.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::ExitApplication(void)
{
    Quit();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true while a full layout is in progress or a file load is
///                mutating the document, false otherwise
///
/// @brief
/// Timer-handler gate: returns true while cLayout or cDocument is being
/// mutated. Used by UpdateStatus() and OnAutoSaveTimer() to early-return and
/// avoid racing reads when QApplication::processEvents() pumps timer events
/// mid-layout or mid-load.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::IsBusy(void) const
{
    return (mLayout && mLayout->InFullLayout())
        || (mDocument && mDocument->GetLoading());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool force [in] - Force relayout even if not dirty
///
/// @return nothing
///
/// @brief
/// Trigger document layout. Uses layout black box API.
/// Called after edits, file loads, or when forced.
///
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::LayoutDocument(bool force)
{
    (void)force;  // No longer used - layout always runs when called

    if (!mLayout || !mDocument)
    {
        return;
    }

    // Start formatting progress indicator
    StartProgress("Formatting...");

    // Trigger full layout with progress callback (BLACK BOX API)
    // Callback receives 10-percent values (0-10 scale) for 10% display resolution
    mLayout->LayoutDocument(mDocument, [this](int tenPercent)
    {
        if (mWordTsar)
        {
            int displayPercent = tenPercent * 10 ;
            QString statusText = QString("Formatting %1%...").arg(displayPercent) ;
            mWordTsar->SetStatus(statusText, true, displayPercent) ;
        }
        // Process events so the animation and text actually render
        // ExcludeUserInputEvents prevents keyboard/mouse during format
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    });

    // Stop formatting progress indicator
    StopProgress();

    // Update scrollbar range after layout
    UpdateScrollbar();

    // Recalculate caret position after layout
    CalculateCaretPosition();

    // Update display
    update();
}


/////////////////////////////////////////////////////////////////////////////
// MOUSE HANDLING
/////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////
///
/// @param  const QPoint& point [in] - Mouse click position in widget pixels
///
/// @return POSITION_T - Document position at mouse click, or NOT_SET if invalid
///
/// @brief
/// Convert mouse click coordinates to document position.
/// Uses BLACK BOX API (FindPositionInLine, GetLineScreenY, GetLineBaseX).
///
/// Coordinate transformation:
/// 1. Mouse comes in as Qt widget pixels
/// 2. Convert to twips by multiplying by FONTSCALE (and applying page scale)
/// 3. Add scroll offset to get document Y coordinate
/// 4. Find line at that Y coordinate
/// 5. Use FindPositionInLine() to get position within line
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorCtrl::PositionFromPoint(const QPoint& point)
{
    if (!mLayout || !mDocument)
    {
        return NOT_SET;
    }

    // Convert Qt pixel coordinates to twips
    // Account for page scale (different in continuous vs page mode)
    // Both X and Y must use the same transformation since painter.scale() applies to both axes
    COORD_T xTwips = static_cast<COORD_T>(point.x()) / mPageScale;
    COORD_T yTwips = static_cast<COORD_T>(point.y()) / mPageScale;

    // In page mode, undo the painter.translate(border, border) applied in paintEvent
    if (mDisplaySettings.mode == DISPLAY_PAGE)
    {
        xTwips -= mDisplaySettings.pageBorder;
        yTwips -= mDisplaySettings.pageBorder;
    }

    // Mouse Y is viewport-relative, but line.screeny is document-absolute
    // So we need to convert viewport Y to document Y by adding scroll offset
    COORD_T docY = yTwips + mScrollOffset;

    // Find which line was clicked (passing absolute document Y)
    LINE_T clickedLine = LineFromY(docY);

    if (clickedLine == NOT_SET)
    {
        return NOT_SET;
    }

    // Use black box API to find position within line
    // FindPositionInLine expects ABSOLUTE X coordinate in twips
    POSITION_T pos = mLayout->FindPositionInLine(xTwips, clickedLine);

    return pos;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QMouseEvent* event [in] - Mouse press event
///
/// @return nothing
///
/// @brief
/// Handle mouse press event. Implements single-click
/// caret positioning.
///
/// Left click: Position caret at click location
/// Other buttons: Ignored for now
///
/// [REFACTORED from editorctrl.cpp::mousePressEvent()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::mousePressEvent(QMouseEvent* event)
{
    // Clear search highlighting on mouse click
    mSearchBlockSet = false;
    mStartSearchBlock = 0;
    mEndSearchBlock = 0;

    // Only handle left button
    if (event->button() == Qt::LeftButton)
    {
        // Convert mouse click to document position
        POSITION_T clickPos = PositionFromPoint(event->pos());

        if (clickPos != NOT_SET)
        {
            // Set caret to clicked position
            mCaretDocumentPosition = clickPos;
            mDocument->SetPosition(clickPos);

            // Recalculate caret visual position
            CalculateCaretPosition();

            // Update display
            update();
        }
    }

    // Pass to base class for default behavior
    QWidget::mousePressEvent(event);
}


/////////////////////////////////////////////////////////////////////////////
// MAIN WINDOW INTEGRATION
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  frame [in] pointer to main window
///
/// @return nothing
///
/// @brief
/// Store reference to main window and update window title.
/// Sets up connection to main window's scrollbar.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetFrame(cWordTsar* frame)
{
    mWordTsar = frame;

    // Cannot call mWordTsar methods here due to incomplete type
    // (forward declaration only, full definition in wordtsar.h)
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  ruler [in] pointer to ruler control
///
/// @return nothing
///
/// @brief
/// Store reference to ruler control and initialize ruler settings.
/// Sets ruler mMeasurement system, page width, margins, and position.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SetRuler(cRulerCtrl* ruler)
{
    mRuler = ruler;

    // Initialize ruler with default paper width based on mMeasurement system
    // mMeasure should already be set from INI file by this point
    // (same as old editorctrl.cpp:745-762)
    switch(mMeasure)
    {
        case MSR_INCHES:
            mRuler->SetRuler(8.5, MSR_INCHES);  // US Letter width
            break;

        case MSR_MILLIMETERS:
            mRuler->SetRuler(215.9, MSR_MILLIMETERS);  // A4 width
            break;

        default:
            mRuler->SetRuler(21.59, MSR_CENTIMETERS);  // A4 width
            break;
    }

    // Set default page margins (will be updated by UpdateRuler on first paint)
    mRuler->SetPageMargins(TWIPSPERINCH, TWIPSPERINCH * 7.5);
    mRuler->SetRightMargin((TWIPSPERINCH * 7.5) - TWIPSPERINCH);
    mRuler->SetPosition(0);

    // Set ruler background to match page background
    mRuler->SetBackgroundColour(mBGroundColour);

    mRuler->update();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if close should proceed, false to cancel
///
/// @brief
/// Handle window close event. Prompts to save if document is modified.
/// Returns true to allow close, false to cancel close operation.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorCtrl::CloseEvent(void)
{
    if (GetDocument() && GetDocument()->mChanged == true)
    {
        QMessageBox::StandardButton result = QMessageBox::question(
            this,
            "Save and Exit",
            "Save File Before Exiting?",
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (result == QMessageBox::Yes)
        {
            // Save the file directly (same logic as ^KS/^KT)
            if (mFileSet)
            {
                // We have a filename, save directly
                std::string fname = mFileDir + "/" + mFileName;
                if (!SaveFile(fname))
                {
                    return false;  // Save failed, cancel close
                }
                GetDocument()->mChanged = false;
            }
            else
            {
                // No filename set, prompt for Save As
                std::string saveFilename = PromptForSaveFile();
                if (saveFilename.empty())
                {
                    return false;  // User cancelled Save As dialog
                }
                QFileInfo info(QString::fromStdString(saveFilename));
                mFileDir = info.path().toStdString();
                mFileName = info.fileName().toStdString();
                mFileSet = true;
                mLayout->SetFilename(mFileName);
                mLayout->SetFileDir(mFileDir);
                if (!SaveFile(saveFilename))
                {
                    return false;  // Save failed, cancel close
                }
                GetDocument()->mChanged = false;
                SetTitle(mFileName);
            }
            return true;  // Saved successfully, allow close
        }
        else if (result == QMessageBox::No)
        {
            return true;  // User doesn't want to save, allow close
        }
        else
        {
            return false;  // Cancel clicked, don't close
        }
    }
    return true;  // No changes, allow close
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  tab [in] which preferences tab to display (0=default, 1=color, 2=user, 3=screen)
///
/// @return nothing
///
/// @brief
/// Display the preferences dialog with the specified tab selected.
/// Allows user to configure colors, display options, mMeasurement units, etc.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::Preferences(void)
{
    QDialog preferences(this);

    Ui::Preferences ui;
    ui.setupUi(&preferences);

    // Set dot command display mode
    if (mAlwaysDot == true)
    {
        ui.radioButton_6->setChecked(true);
    }
    else
    {
        ui.radioButton_7->setChecked(true);
    }

    // Set flag column display mode
    if (mAlwaysFlag == true)
    {
        ui.mFlagAwlaysOn->setChecked(true);
    }
    else
    {
        ui.mFlagWithTags->setChecked(true);
    }

    // Disable controls we don't implement yet
    ui.mSoftSpaceDots->setEnabled(false);

    // Set mMeasurement units
    switch (mMeasure)
    {
        case MSR_INCHES:
        {
            ui.mInches->setChecked(true);
            break;
        }

        case MSR_CENTIMETERS:
        {
            ui.mCM->setChecked(true);
            break;
        }

        default:
        {
            ui.mMM->setChecked(true);
            break;
        }
    }

    // Set UI element visibility checkboxes
    ui.mScrollBar->setChecked(mDispScrollBar);
    ui.mStyleBar->setChecked(mDispStyleBar);
    ui.mStatusLine->setChecked(mDispStatusBar);
    ui.mRulerLine->setChecked(mDispRuler);
    ui.mMenuBar->setChecked(mDispMenu);

    int ecode = preferences.exec();

    if (ecode != 0)
    {
        // Update mMeasurement units
        if (ui.mInches->isChecked())
        {
            mMeasure = MSR_INCHES;
            SetMeasurement("0i");
        }
        else if (ui.mCM->isChecked())
        {
            mMeasure = MSR_CENTIMETERS;
            SetMeasurement("0cm");
        }
        else
        {
            mMeasure = MSR_MILLIMETERS;
            SetMeasurement("0mm");
        }

        // Update dot command display
        if (ui.radioButton_6->isChecked())
        {
            mAlwaysDot = true;
            if (GetShowControls() == SHOW_ALL)
            {
                SetShowDot(true);
            }
        }
        else
        {
            mAlwaysDot = false;
            if (GetShowControls() == SHOW_DOT)
            {
                SetShowDot(false);
            }
        }

        // Update flag column display
        if (ui.mFlagAwlaysOn->isChecked())
        {
            mAlwaysFlag = true;
        }
        else
        {
            mAlwaysFlag = false;
        }

        // Update UI element visibility
        mDispScrollBar = ui.mScrollBar->isChecked();
        mDispStyleBar = ui.mStyleBar->isChecked();
        mDispStatusBar = ui.mStatusLine->isChecked();
        mDispRuler = ui.mRulerLine->isChecked();
        mDispMenu = ui.mMenuBar->isChecked();

        // Apply display toggle changes to main window widgets
        mWordTsar->ApplyDisplaySettings() ;

        // Trigger repaint to show changes
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle fullscreen mode on the main window. Called by HandleSpecialKey
/// when F11 is pressed.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::ToggleFullscreen(void)
{
    if (mWordTsar != nullptr)
    {
        if (mFullScreen == false)
        {
            mFullScreen = true;
            mWordTsar->setWindowState(Qt::WindowFullScreen);
            QCoreApplication::processEvents();
        }
        else
        {
            mFullScreen = false;
            mWordTsar->setWindowState(Qt::WindowNoState);
            QCoreApplication::processEvents();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the System Preferences dialog. Loads settings from cConfig,
/// displays a 6-tab dialog, and saves changes back on OK.
/// This is separate from the existing Preferences dialog.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorCtrl::SystemPreferences(void)
{
    // Skip dialog in automated testing mode
    const char* testing = std::getenv("QT_TESTING");
    if (testing && std::strcmp(testing, "1") == 0)
    {
        return;
    }

    // Load current config from disk
    cConfig config;
    config.Load();

    // Create and set up dialog
    QDialog dialog(this);
    Ui::SystemPreferences ui;
    ui.setupUi(&dialog);

    // --- Populate Tab 1: General ---
    ui.mDefaultFont->setCurrentFont(QFont(QString::fromStdString(config.mDefaultFont)));
    ui.mDefaultFontSize->setValue(config.mDefaultFontSize);

    // Measurement: read from in-memory state (set by LoadConfig and ^OB Preferences)
    std::string meas = GetMeasurement();
    if (meas.find('i') != std::string::npos || meas.find('"') != std::string::npos)
    {
        ui.mMeasurement->setCurrentIndex(0);  // Inches
    }
    else if (meas.find('c') != std::string::npos)
    {
        ui.mMeasurement->setCurrentIndex(1);  // Centimeters
    }
    else if (meas.find('m') != std::string::npos)
    {
        ui.mMeasurement->setCurrentIndex(2);  // Millimeters
    }
    else
    {
        ui.mMeasurement->setCurrentIndex(0);  // Default: inches
    }

    ui.mCodePage->setCurrentIndex(config.mCodePage);
    ui.mShowControls->setChecked(config.mShowControls);

    if (config.mDefaultFormat == "rtf")
    {
        ui.mDefaultFormat->setCurrentIndex(1);
    }
    else
    {
        ui.mDefaultFormat->setCurrentIndex(0);
    }

    ui.mDefaultDirectory->setText(QString::fromStdString(config.mDefaultDirectory));

    // Browse button for default directory
    QObject::connect(ui.mBrowseDir, &QPushButton::clicked, [&]()
    {
        QString dir = QFileDialog::getExistingDirectory(&dialog, "Select Default Directory",
            ui.mDefaultDirectory->text());
        if (!dir.isEmpty())
        {
            ui.mDefaultDirectory->setText(dir);
        }
    });

    // --- Populate Tab 2: Editor ---
    ui.mAutoSaveInterval->setValue(config.mAutoSaveInterval);
    ui.mCaretBlinkRate->setValue(config.mCaretBlinkRate);
    ui.mInputMode->setCurrentIndex(config.mInputMode);
    ui.mSpellCheckDotCommands->setChecked(config.mSpellCheckDotCommands);
    ui.mSpellCheckLanguage->setText(QString::fromStdString(config.mSpellCheckLanguage));

    // --- Populate Tab 3: Page Setup ---
    // Determine unit suffix based on mMeasurement selection
    char unitChar = 'i';
    int measIdx = ui.mMeasurement->currentIndex();
    if (measIdx == 1)
    {
        unitChar = 'c';
    }
    else if (measIdx == 2)
    {
        unitChar = 'm';
    }

    ui.mPaperWidth->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mPaperWidth, unitChar)));
    ui.mPaperHeight->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mPaperHeight, unitChar)));
    ui.mLeftMargin->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mLeftMargin, unitChar)));
    ui.mRightMargin->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mRightMargin, unitChar)));
    ui.mTopMargin->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mTopMargin, unitChar)));
    ui.mBottomMargin->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mBottomMargin, unitChar)));
    ui.mHeaderMargin->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mHeaderMargin, unitChar)));
    ui.mFooterMargin->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mFooterMargin, unitChar)));
    ui.mPageOffsetOdd->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mPageOffsetOdd, unitChar)));
    ui.mPageOffsetEven->setText(QString::fromStdString(cConfig::FormatMeasurement(config.mPageOffsetEven, unitChar)));

    // Refresh Page Setup fields when mMeasurement combo changes
    QObject::connect(ui.mMeasurement, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [&](int idx)
    {
        char uc = 'i';
        if (idx == 1)
        {
            uc = 'c';
        }
        else if (idx == 2)
        {
            uc = 'm';
        }

        // Parse current field text to twips, re-format in new unit
        auto convert = [uc](QLineEdit* field)
        {
            COORD_T twips = cConfig::ParseMeasurement(field->text().toStdString().c_str());
            field->setText(QString::fromStdString(cConfig::FormatMeasurement(twips, uc)));
        };

        convert(ui.mPaperWidth);
        convert(ui.mPaperHeight);
        convert(ui.mLeftMargin);
        convert(ui.mRightMargin);
        convert(ui.mTopMargin);
        convert(ui.mBottomMargin);
        convert(ui.mHeaderMargin);
        convert(ui.mFooterMargin);
        convert(ui.mPageOffsetOdd);
        convert(ui.mPageOffsetEven);
    });

    ui.mLandscape->setChecked(config.mLandscape);

    // --- Populate Tab 4: Colors ---
    // Helper to set button background to show current color
    auto setButtonColor = [](QPushButton* btn, const sRGB& c)
    {
        btn->setStyleSheet(QString("background-color: rgb(%1, %2, %3);").arg(c.r).arg(c.g).arg(c.b));
        btn->setProperty("colorR", c.r);
        btn->setProperty("colorG", c.g);
        btn->setProperty("colorB", c.b);
    };

    // Helper to update a preview label with fg+bg from two buttons
    auto updatePreview = [](QLabel* lbl, QPushButton* fgBtn, QPushButton* bgBtn)
    {
        int fgR = fgBtn->property("colorR").toInt();
        int fgG = fgBtn->property("colorG").toInt();
        int fgB = fgBtn->property("colorB").toInt();
        int bgR = bgBtn->property("colorR").toInt();
        int bgG = bgBtn->property("colorG").toInt();
        int bgB = bgBtn->property("colorB").toInt();
        lbl->setStyleSheet(QString("color: rgb(%1,%2,%3); background-color: rgb(%4,%5,%6); padding: 2px;")
            .arg(fgR).arg(fgG).arg(fgB).arg(bgR).arg(bgG).arg(bgB));
    };

    // Helper to read RGB from a color button as "r,g,b" string
    auto btnRGB = [](QPushButton* btn)
    {
        return QString("%1,%2,%3")
            .arg(btn->property("colorR").toInt())
            .arg(btn->property("colorG").toInt())
            .arg(btn->property("colorB").toInt());
    };

    // Helper to build a styled div line for the editor preview
    // fg and bg are "r,g,b" strings
    auto previewDiv = [](const QString& fg, const QString& bg, const QString& text)
    {
        return QString("<div style=\"font-family:monospace; padding:2px; color:rgb(%1); background-color:rgb(%2);\">%3</div>")
            .arg(fg, bg, text);
    };

    // Helper to build an inline span with fg+bg colors
    auto previewSpan = [](const QString& fg, const QString& bg, const QString& text)
    {
        return QString("<span style=\"color:rgb(%1); background-color:rgb(%2);\">%3</span>")
            .arg(fg, bg, text);
    };

    // Build and display the composite editor preview panel
    auto updateEditorPreview = [&]()
    {
        // Read current colors from buttons
        QString textFg   = btnRGB(ui.mColorText);
        QString textBg   = btnRGB(ui.mColorBackground);
        QString hlFg     = btnRGB(ui.mColorHighlightFg);
        QString hlBg     = btnRGB(ui.mColorHighlight);
        QString dotFg    = btnRGB(ui.mColorDotFg);
        QString dotBg    = btnRGB(ui.mColorDot);
        QString blockFg  = btnRGB(ui.mColorBlockFg);
        QString blockBg  = btnRGB(ui.mColorBlock);
        QString cmtFg    = btnRGB(ui.mColorCommentFg);
        QString cmtBg    = btnRGB(ui.mColorComment);
        QString errFg    = btnRGB(ui.mColorErrorFg);
        QString errBg    = btnRGB(ui.mColorError);
        QString unkFg    = btnRGB(ui.mColorUnknownFg);
        QString unkBg    = btnRGB(ui.mColorUnknown);
        QString niFg     = btnRGB(ui.mColorNotImplementedFg);
        QString niBg     = btnRGB(ui.mColorNotImplemented);
        QString srchFg   = btnRGB(ui.mColorSearchFg);
        QString srchBg   = btnRGB(ui.mColorSearch);
        QString statFg   = btnRGB(ui.mColorStatusBarFg);
        QString statBg   = btnRGB(ui.mColorStatusBarBg);
        QString helpFg   = btnRGB(ui.mColorHelpPanelFg);
        QString helpBg   = btnRGB(ui.mColorHelpPanelBg);
        QString keyFg    = btnRGB(ui.mColorHelpKeyFg);
        QString keyBg    = btnRGB(ui.mColorHelpKeyBg);
        QString rulerFg  = btnRGB(ui.mColorRulerFg);
        QString rulerBg  = btnRGB(ui.mColorRulerBg);

        QString html;

        // Status bar
        html += previewDiv(statFg, statBg,
            "Body Text | TNR 12 | B I U &nbsp; | L C R J &nbsp;&nbsp;&nbsp; Pg 1 Ln 1 Col 1");

        // Help panel with keystroke highlights
        html += previewDiv(helpFg, helpBg,
            previewSpan(keyFg, keyBg, "^J") + " help &nbsp; " +
            previewSpan(keyFg, keyBg, "^KD") + " done &nbsp; " +
            previewSpan(keyFg, keyBg, "^KS") + " save &nbsp; " +
            previewSpan(keyFg, keyBg, "^KQ") + " quit");

        // Ruler
        html += previewDiv(rulerFg, rulerBg,
            "&#9500;&#9472;&#9472;&#9472;&#9654;&#9472;&#9472;&#9472;&#9654;&#9472;&#9472;&#9472;"
            "&#9654;&#9472;&#9472;&#9472;&#9654;&#9472;&#9472;&#9472;&#9654;&#9472;&#9472;&#9472;"
            "&#9654;&#9472;&#9472;&#9472;&#9654;&#9472;&#9472;&#9472;&#9654;&#9472;&#9472;&#9472;"
            "&#9654;&#9472;&#9472;&#9472;&#9508;");

        // Normal text
        html += previewDiv(textFg, textBg,
            "The quick brown fox jumps over the lazy dog.");

        // Control codes inline
        html += previewDiv(textFg, textBg,
            "Use " + previewSpan(hlFg, hlBg, "B") + "bold" + previewSpan(hlFg, hlBg, "B") +
            " and " + previewSpan(hlFg, hlBg, "Y") + "italic" + previewSpan(hlFg, hlBg, "Y") +
            " text.");

        // Dot command
        html += previewDiv(dotFg, dotBg, ".LH 12");

        // Comment
        html += previewDiv(cmtFg, cmtBg, ".. This is a comment line");

        // Block selection
        html += previewDiv(blockFg, blockBg,
            "This line is selected as a block.");

        // Error
        html += previewDiv(errFg, errBg, ".XX error in dot command");

        // Unknown
        html += previewDiv(unkFg, unkBg, ".ZZ unknown dot command");

        // Not implemented
        html += previewDiv(niFg, niBg, ".TC not implemented dot command");

        // Search result
        html += previewDiv(textFg, textBg,
            "Found: " + previewSpan(srchFg, srchBg, "search result") + " in document.");

        ui.mColorPreview->setText(html);
    };

    // Helper to connect a color button to a color picker dialog with change callback
    auto connectColorButton = [&](QPushButton* btn, QWidget* parent, std::function<void()> onChange = nullptr)
    {
        QObject::connect(btn, &QPushButton::clicked, [btn, parent, onChange]()
        {
            QColor initial(btn->property("colorR").toInt(),
                           btn->property("colorG").toInt(),
                           btn->property("colorB").toInt());
            QColor chosen = QColorDialog::getColor(initial, parent, "Choose Color");
            if (chosen.isValid())
            {
                btn->setStyleSheet(QString("background-color: rgb(%1, %2, %3);")
                    .arg(chosen.red()).arg(chosen.green()).arg(chosen.blue()));
                btn->setProperty("colorR", chosen.red());
                btn->setProperty("colorG", chosen.green());
                btn->setProperty("colorB", chosen.blue());
                if (onChange)
                {
                    onChange();
                }
            }
        });
    };

    // Editor colors -- foreground + background
    setButtonColor(ui.mColorText, config.mGuiForeground);
    setButtonColor(ui.mColorBackground, config.mGuiBackground);
    setButtonColor(ui.mColorHighlightFg, config.mGuiHighlightForeground);
    setButtonColor(ui.mColorHighlight, config.mGuiHighlightBackground);
    setButtonColor(ui.mColorDotFg, config.mGuiDotForeground);
    setButtonColor(ui.mColorDot, config.mGuiDotBackground);
    setButtonColor(ui.mColorBlockFg, config.mGuiBlockForeground);
    setButtonColor(ui.mColorBlock, config.mGuiBlockBackground);
    setButtonColor(ui.mColorCommentFg, config.mGuiCommentForeground);
    setButtonColor(ui.mColorComment, config.mGuiCommentBackground);
    setButtonColor(ui.mColorErrorFg, config.mGuiErrorForeground);
    setButtonColor(ui.mColorError, config.mGuiErrorBackground);
    setButtonColor(ui.mColorUnknownFg, config.mGuiUnknownForeground);
    setButtonColor(ui.mColorUnknown, config.mGuiUnknownBackground);
    setButtonColor(ui.mColorNotImplementedFg, config.mGuiNotImplementedForeground);
    setButtonColor(ui.mColorNotImplemented, config.mGuiNotImplementedBackground);
    setButtonColor(ui.mColorSearchFg, config.mGuiSearchForeground);
    setButtonColor(ui.mColorSearch, config.mGuiSearchBackground);

    // Screen colors -- foreground + background
    setButtonColor(ui.mColorStatusBarFg, config.mGuiStatusBarForeground);
    setButtonColor(ui.mColorStatusBarBg, config.mGuiStatusBarBackground);
    setButtonColor(ui.mColorHelpPanelFg, config.mGuiHelpPanelForeground);
    setButtonColor(ui.mColorHelpPanelBg, config.mGuiHelpPanelBackground);
    setButtonColor(ui.mColorHelpKeyFg, config.mGuiHelpPanelKeystrokeForeground);
    setButtonColor(ui.mColorHelpKeyBg, config.mGuiHelpPanelKeystrokeBackground);
    setButtonColor(ui.mColorRulerFg, config.mGuiRulerForeground);
    setButtonColor(ui.mColorRulerBg, config.mGuiRulerBackground);

    // Create preview labels and connect color buttons with per-row onChange callbacks
    // Each row has fg button, bg button, preview label, and grid row number
    struct ColorRow
    {
        QPushButton* fgBtn;
        QPushButton* bgBtn;
        int gridRow;
    };

    ColorRow colorRows[] = {
        // Editor colors (rows 2-10)
        { ui.mColorText,             ui.mColorBackground,     2  },
        { ui.mColorHighlightFg,      ui.mColorHighlight,      3  },
        { ui.mColorDotFg,            ui.mColorDot,            4  },
        { ui.mColorBlockFg,          ui.mColorBlock,          5  },
        { ui.mColorCommentFg,        ui.mColorComment,        6  },
        { ui.mColorErrorFg,          ui.mColorError,          7  },
        { ui.mColorUnknownFg,        ui.mColorUnknown,        8  },
        { ui.mColorNotImplementedFg, ui.mColorNotImplemented, 9  },
        { ui.mColorSearchFg,         ui.mColorSearch,         10 },
        // Screen colors (rows 12-15)
        { ui.mColorStatusBarFg,      ui.mColorStatusBarBg,    12 },
        { ui.mColorHelpPanelFg,      ui.mColorHelpPanelBg,    13 },
        { ui.mColorHelpKeyFg,        ui.mColorHelpKeyBg,      14 },
        { ui.mColorRulerFg,          ui.mColorRulerBg,        15 },
    };

    // Array to hold preview labels for palette-apply refresh
    constexpr int NUM_COLOR_ROWS = 13;
    QLabel* previewLabels[NUM_COLOR_ROWS];

    for (int i = 0; i < NUM_COLOR_ROWS; ++i)
    {
        // Create preview label
        QLabel* preview = new QLabel("Sample");
        preview->setMinimumSize(60, 24);
        preview->setAlignment(Qt::AlignCenter);
        ui.colorsGrid->addWidget(preview, colorRows[i].gridRow, 3);
        previewLabels[i] = preview;

        // Wire fg+bg buttons with onChange that updates this preview
        QPushButton* fgBtn = colorRows[i].fgBtn;
        QPushButton* bgBtn = colorRows[i].bgBtn;
        auto onChangeCallback = [preview, fgBtn, bgBtn, &updatePreview, &updateEditorPreview]()
        {
            updatePreview(preview, fgBtn, bgBtn);
            updateEditorPreview();
        };
        connectColorButton(fgBtn, &dialog, onChangeCallback);
        connectColorButton(bgBtn, &dialog, onChangeCallback);

        // Set initial preview from populated button colors
        updatePreview(preview, fgBtn, bgBtn);
    }

    // Set initial editor preview from populated button colors
    updateEditorPreview();

    // Palette selector -- built-in + custom palettes
    auto builtInPalettes = cConfig::GetGUIPalettes();
    auto customPalettes = cConfig::LoadCustomGUIPalettes();

    // Combined list for indexing (built-in first, then custom)
    std::vector<sGUIPalette> allPalettes;
    allPalettes.insert(allPalettes.end(), builtInPalettes.begin(), builtInPalettes.end());
    allPalettes.insert(allPalettes.end(), customPalettes.begin(), customPalettes.end());

    // Populate combo: (Custom) then built-in then separator then custom
    ui.mPaletteCombo->addItem("(Custom)");
    for (const auto& pal : builtInPalettes)
    {
        ui.mPaletteCombo->addItem(QString::fromStdString(pal.name));
    }
    if (!customPalettes.empty())
    {
        ui.mPaletteCombo->insertSeparator(ui.mPaletteCombo->count());
        for (const auto& pal : customPalettes)
        {
            ui.mPaletteCombo->addItem(QString::fromStdString(pal.name));
        }
    }

    int builtInCount = static_cast<int>(builtInPalettes.size());

    // Auto-detect current palette: compare config colors against all palettes
    auto matchesRGB = [](sRGB a, sRGB b)
    {
        return a.r == b.r && a.g == b.g && a.b == b.b;
    };

    for (size_t pi = 0; pi < allPalettes.size(); ++pi)
    {
        const auto& pal = allPalettes[pi];
        bool match = true;

        // Editor colors (9 FG+BG pairs)
        match = match && matchesRGB(config.mGuiBackground, pal.background);
        match = match && matchesRGB(config.mGuiForeground, pal.foreground);
        match = match && matchesRGB(config.mGuiHighlightBackground, pal.highlightBackground);
        match = match && matchesRGB(config.mGuiHighlightForeground, pal.highlightForeground);
        match = match && matchesRGB(config.mGuiDotBackground, pal.dotBackground);
        match = match && matchesRGB(config.mGuiDotForeground, pal.dotForeground);
        match = match && matchesRGB(config.mGuiBlockBackground, pal.blockBackground);
        match = match && matchesRGB(config.mGuiBlockForeground, pal.blockForeground);
        match = match && matchesRGB(config.mGuiCommentBackground, pal.commentBackground);
        match = match && matchesRGB(config.mGuiCommentForeground, pal.commentForeground);
        match = match && matchesRGB(config.mGuiErrorBackground, pal.errorBackground);
        match = match && matchesRGB(config.mGuiErrorForeground, pal.errorForeground);
        match = match && matchesRGB(config.mGuiUnknownBackground, pal.unknownBackground);
        match = match && matchesRGB(config.mGuiUnknownForeground, pal.unknownForeground);
        match = match && matchesRGB(config.mGuiNotImplementedBackground, pal.notImplementedBackground);
        match = match && matchesRGB(config.mGuiNotImplementedForeground, pal.notImplementedForeground);
        match = match && matchesRGB(config.mGuiSearchBackground, pal.searchBackground);
        match = match && matchesRGB(config.mGuiSearchForeground, pal.searchForeground);

        // Screen colors (4 FG+BG pairs)
        match = match && matchesRGB(config.mGuiStatusBarForeground, pal.statusBarForeground);
        match = match && matchesRGB(config.mGuiStatusBarBackground, pal.statusBarBackground);
        match = match && matchesRGB(config.mGuiHelpPanelForeground, pal.helpPanelForeground);
        match = match && matchesRGB(config.mGuiHelpPanelBackground, pal.helpPanelBackground);
        match = match && matchesRGB(config.mGuiHelpPanelKeystrokeForeground, pal.helpPanelKeystrokeForeground);
        match = match && matchesRGB(config.mGuiHelpPanelKeystrokeBackground, pal.helpPanelKeystrokeBackground);
        match = match && matchesRGB(config.mGuiRulerForeground, pal.rulerForeground);
        match = match && matchesRGB(config.mGuiRulerBackground, pal.rulerBackground);

        if (match)
        {
            // Map allPalettes index to combo index
            // Combo layout: 0=(Custom), 1..builtInCount=built-in, separator, custom...
            int comboIdx;
            if (static_cast<int>(pi) < builtInCount)
            {
                comboIdx = static_cast<int>(pi) + 1;  // +1 for "(Custom)" at index 0
            }
            else
            {
                // Custom palettes: skip (Custom) + built-in + separator
                comboIdx = static_cast<int>(pi) + 2;  // +1 for "(Custom)" + 1 for separator
            }
            ui.mPaletteCombo->setCurrentIndex(comboIdx);
            break;
        }
    }

    // Shared palette-apply logic: load selected palette into all color buttons and previews
    auto applySelectedPalette = [&]()
    {
        int idx = ui.mPaletteCombo->currentIndex();
        if (idx <= 0)
        {
            return;  // "(Custom)" selected
        }

        // Map combo index to allPalettes index
        // Layout: 0=(Custom), 1..builtInCount=built-in, separator, custom...
        int paletteIdx;
        if (idx <= builtInCount)
        {
            // Built-in palette (indices 1..builtInCount maps to allPalettes 0..builtInCount-1)
            paletteIdx = idx - 1;
        }
        else
        {
            // Custom palette (skip separator item)
            paletteIdx = idx - 2;  // -1 for "(Custom)", -1 for separator
        }

        if (paletteIdx < 0 || paletteIdx >= static_cast<int>(allPalettes.size()))
        {
            return;
        }
        const sGUIPalette& pal = allPalettes[paletteIdx];

        // Apply editor colors
        setButtonColor(ui.mColorText, pal.foreground);
        setButtonColor(ui.mColorBackground, pal.background);
        setButtonColor(ui.mColorHighlightFg, pal.highlightForeground);
        setButtonColor(ui.mColorHighlight, pal.highlightBackground);
        setButtonColor(ui.mColorDotFg, pal.dotForeground);
        setButtonColor(ui.mColorDot, pal.dotBackground);
        setButtonColor(ui.mColorBlockFg, pal.blockForeground);
        setButtonColor(ui.mColorBlock, pal.blockBackground);
        setButtonColor(ui.mColorCommentFg, pal.commentForeground);
        setButtonColor(ui.mColorComment, pal.commentBackground);
        setButtonColor(ui.mColorErrorFg, pal.errorForeground);
        setButtonColor(ui.mColorError, pal.errorBackground);
        setButtonColor(ui.mColorUnknownFg, pal.unknownForeground);
        setButtonColor(ui.mColorUnknown, pal.unknownBackground);
        setButtonColor(ui.mColorNotImplementedFg, pal.notImplementedForeground);
        setButtonColor(ui.mColorNotImplemented, pal.notImplementedBackground);
        setButtonColor(ui.mColorSearchFg, pal.searchForeground);
        setButtonColor(ui.mColorSearch, pal.searchBackground);

        // Apply screen colors
        setButtonColor(ui.mColorStatusBarFg, pal.statusBarForeground);
        setButtonColor(ui.mColorStatusBarBg, pal.statusBarBackground);
        setButtonColor(ui.mColorHelpPanelFg, pal.helpPanelForeground);
        setButtonColor(ui.mColorHelpPanelBg, pal.helpPanelBackground);
        setButtonColor(ui.mColorHelpKeyFg, pal.helpPanelKeystrokeForeground);
        setButtonColor(ui.mColorHelpKeyBg, pal.helpPanelKeystrokeBackground);
        setButtonColor(ui.mColorRulerFg, pal.rulerForeground);
        setButtonColor(ui.mColorRulerBg, pal.rulerBackground);

        // Refresh all per-row preview labels and the editor preview
        for (int i = 0; i < NUM_COLOR_ROWS; ++i)
        {
            updatePreview(previewLabels[i], colorRows[i].fgBtn, colorRows[i].bgBtn);
        }
        updateEditorPreview();
    };

    // Apply palette button
    QObject::connect(ui.mApplyPalette, &QPushButton::clicked, applySelectedPalette);

    // Auto-apply palette when dropdown selection changes
    QObject::connect(ui.mPaletteCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        [&](int) { applySelectedPalette(); });

    // --- Populate Tab 5: Display ---
    // Help level 0-3 (WS4 manual's "Help levels"); combo index == level.
    ui.mShowHelp->setCurrentIndex(std::clamp(config.mGuiShowHelp, 0, 4));

    ui.mShowRuler->setChecked(config.mGuiShowRuler);
    ui.mShowScrollBar->setChecked(config.mGuiShowScrollBar);
    ui.mShowStatusBar->setChecked(config.mGuiShowStatusBar);
    ui.mShowStyleBar->setChecked(config.mGuiShowStyleBar);
    ui.mShowMenu->setChecked(config.mGuiShowMenu);
    // Set flag column display mode
    if (config.mGuiAlwaysFlagColumn)
    {
        ui.mFlagAlwaysOn->setChecked(true) ;
    }
    else
    {
        ui.mFlagWithTags->setChecked(true) ;
    }

    // Set dot command display mode
    if (config.mGuiAlwaysDotCommands)
    {
        ui.mDotAlwaysOn->setChecked(true) ;
    }
    else
    {
        ui.mDotWithTags->setChecked(true) ;
    }

    // Disable unimplemented controls
    ui.mSoftSpaceDots->setEnabled(false) ;

    // --- Populate Tab 6: User ---
    ui.mShortName->setText(QString::fromStdString(config.mShortName));
    ui.mLongName->setText(QString::fromStdString(config.mLongName));

    // Helper: read RGB from a color button's properties
    auto readButtonColor = [](QPushButton* btn) -> sRGB
    {
        return { static_cast<short>(btn->property("colorR").toInt()),
                 static_cast<short>(btn->property("colorG").toInt()),
                 static_cast<short>(btn->property("colorB").toInt()) };
    };

    // Save Theme button: build palette from current buttons and save to file
    QObject::connect(ui.mSaveTheme, &QPushButton::clicked, [&]()
    {
        // Ask user for theme name
        bool ok = false;
        QString themeName = QInputDialog::getText(&dialog, "Save Theme",
            "Theme name:", QLineEdit::Normal, "", &ok);

        if (!ok || themeName.trimmed().isEmpty())
        {
            return;
        }

        // Reject if name matches a built-in palette
        for (const auto& pal : builtInPalettes)
        {
            if (pal.name == themeName.toStdString())
            {
                QMessageBox::warning(&dialog, "Save Theme",
                    "Cannot overwrite built-in palette \"" + themeName + "\".");
                return;
            }
        }

        // Build palette from current button colors
        sGUIPalette newPal;
        newPal.name = themeName.toStdString();
        newPal.foreground                    = readButtonColor(ui.mColorText);
        newPal.background                    = readButtonColor(ui.mColorBackground);
        newPal.highlightForeground           = readButtonColor(ui.mColorHighlightFg);
        newPal.highlightBackground           = readButtonColor(ui.mColorHighlight);
        newPal.dotForeground                 = readButtonColor(ui.mColorDotFg);
        newPal.dotBackground                 = readButtonColor(ui.mColorDot);
        newPal.blockForeground               = readButtonColor(ui.mColorBlockFg);
        newPal.blockBackground               = readButtonColor(ui.mColorBlock);
        newPal.commentForeground             = readButtonColor(ui.mColorCommentFg);
        newPal.commentBackground             = readButtonColor(ui.mColorComment);
        newPal.errorForeground               = readButtonColor(ui.mColorErrorFg);
        newPal.errorBackground               = readButtonColor(ui.mColorError);
        newPal.unknownForeground             = readButtonColor(ui.mColorUnknownFg);
        newPal.unknownBackground             = readButtonColor(ui.mColorUnknown);
        newPal.notImplementedForeground      = readButtonColor(ui.mColorNotImplementedFg);
        newPal.notImplementedBackground      = readButtonColor(ui.mColorNotImplemented);
        newPal.searchForeground              = readButtonColor(ui.mColorSearchFg);
        newPal.searchBackground              = readButtonColor(ui.mColorSearch);
        newPal.statusBarForeground           = readButtonColor(ui.mColorStatusBarFg);
        newPal.statusBarBackground           = readButtonColor(ui.mColorStatusBarBg);
        newPal.helpPanelForeground           = readButtonColor(ui.mColorHelpPanelFg);
        newPal.helpPanelBackground           = readButtonColor(ui.mColorHelpPanelBg);
        newPal.helpPanelKeystrokeForeground  = readButtonColor(ui.mColorHelpKeyFg);
        newPal.helpPanelKeystrokeBackground  = readButtonColor(ui.mColorHelpKeyBg);
        newPal.rulerForeground               = readButtonColor(ui.mColorRulerFg);
        newPal.rulerBackground               = readButtonColor(ui.mColorRulerBg);

        // Save to file
        cConfig::SaveCustomGUIPalette(newPal);

        // Refresh the combo box
        customPalettes = cConfig::LoadCustomGUIPalettes();
        allPalettes.clear();
        allPalettes.insert(allPalettes.end(), builtInPalettes.begin(), builtInPalettes.end());
        allPalettes.insert(allPalettes.end(), customPalettes.begin(), customPalettes.end());

        // Block signals during combo rebuild to avoid spurious palette-apply calls
        ui.mPaletteCombo->blockSignals(true);
        ui.mPaletteCombo->clear();
        ui.mPaletteCombo->addItem("(Custom)");
        for (const auto& pal : builtInPalettes)
        {
            ui.mPaletteCombo->addItem(QString::fromStdString(pal.name));
        }
        if (!customPalettes.empty())
        {
            ui.mPaletteCombo->insertSeparator(ui.mPaletteCombo->count());
            for (const auto& pal : customPalettes)
            {
                ui.mPaletteCombo->addItem(QString::fromStdString(pal.name));
            }
        }

        // Select the newly saved palette
        int newIdx = ui.mPaletteCombo->findText(themeName);
        if (newIdx >= 0)
        {
            ui.mPaletteCombo->setCurrentIndex(newIdx);
        }
        ui.mPaletteCombo->blockSignals(false);
    });

    // --- Show dialog ---
    int ecode = dialog.exec();

    if (ecode != 0)
    {
        // --- Read back Tab 1: General ---
        config.mDefaultFont = ui.mDefaultFont->currentFont().family().toStdString();
        config.mDefaultFontSize = ui.mDefaultFontSize->value();

        int newMeasIdx = ui.mMeasurement->currentIndex();
        if (newMeasIdx == 0)
        {
            config.mMeasurement = "0i";
        }
        else if (newMeasIdx == 1)
        {
            config.mMeasurement = "0cm";
        }
        else
        {
            config.mMeasurement = "0mm";
        }

        config.mCodePage = ui.mCodePage->currentIndex();
        config.mShowControls = ui.mShowControls->isChecked();

        if (ui.mDefaultFormat->currentIndex() == 1)
        {
            config.mDefaultFormat = "rtf";
        }
        else
        {
            config.mDefaultFormat = "ws";
        }

        config.mDefaultDirectory = ui.mDefaultDirectory->text().toStdString();

        // --- Read back Tab 2: Editor ---
        config.mAutoSaveInterval = ui.mAutoSaveInterval->value();
        config.mCaretBlinkRate = ui.mCaretBlinkRate->value();
        config.mInputMode = ui.mInputMode->currentIndex();
        config.mSpellCheckDotCommands = ui.mSpellCheckDotCommands->isChecked();
        config.mSpellCheckLanguage = ui.mSpellCheckLanguage->text().toStdString();

        // --- Read back Tab 3: Page Setup ---
        config.mPaperWidth = cConfig::ParseMeasurement(ui.mPaperWidth->text().toStdString().c_str());
        config.mPaperHeight = cConfig::ParseMeasurement(ui.mPaperHeight->text().toStdString().c_str());
        config.mLeftMargin = cConfig::ParseMeasurement(ui.mLeftMargin->text().toStdString().c_str());
        config.mRightMargin = cConfig::ParseMeasurement(ui.mRightMargin->text().toStdString().c_str());
        config.mTopMargin = cConfig::ParseMeasurement(ui.mTopMargin->text().toStdString().c_str());
        config.mBottomMargin = cConfig::ParseMeasurement(ui.mBottomMargin->text().toStdString().c_str());
        config.mHeaderMargin = cConfig::ParseMeasurement(ui.mHeaderMargin->text().toStdString().c_str());
        config.mFooterMargin = cConfig::ParseMeasurement(ui.mFooterMargin->text().toStdString().c_str());
        config.mPageOffsetOdd = cConfig::ParseMeasurement(ui.mPageOffsetOdd->text().toStdString().c_str());
        config.mPageOffsetEven = cConfig::ParseMeasurement(ui.mPageOffsetEven->text().toStdString().c_str());
        config.mLandscape = ui.mLandscape->isChecked();

        // --- Read back Tab 4: Colors ---
        // (readButtonColor lambda is defined above, before dialog.exec())

        // Editor colors -- foreground + background
        config.mGuiForeground = readButtonColor(ui.mColorText);
        config.mGuiBackground = readButtonColor(ui.mColorBackground);
        config.mGuiHighlightForeground = readButtonColor(ui.mColorHighlightFg);
        config.mGuiHighlightBackground = readButtonColor(ui.mColorHighlight);
        config.mGuiDotForeground = readButtonColor(ui.mColorDotFg);
        config.mGuiDotBackground = readButtonColor(ui.mColorDot);
        config.mGuiBlockForeground = readButtonColor(ui.mColorBlockFg);
        config.mGuiBlockBackground = readButtonColor(ui.mColorBlock);
        config.mGuiCommentForeground = readButtonColor(ui.mColorCommentFg);
        config.mGuiCommentBackground = readButtonColor(ui.mColorComment);
        config.mGuiErrorForeground = readButtonColor(ui.mColorErrorFg);
        config.mGuiErrorBackground = readButtonColor(ui.mColorError);
        config.mGuiUnknownForeground = readButtonColor(ui.mColorUnknownFg);
        config.mGuiUnknownBackground = readButtonColor(ui.mColorUnknown);
        config.mGuiNotImplementedForeground = readButtonColor(ui.mColorNotImplementedFg);
        config.mGuiNotImplementedBackground = readButtonColor(ui.mColorNotImplemented);
        config.mGuiSearchForeground = readButtonColor(ui.mColorSearchFg);
        config.mGuiSearchBackground = readButtonColor(ui.mColorSearch);

        // Screen colors -- foreground + background
        config.mGuiStatusBarForeground = readButtonColor(ui.mColorStatusBarFg);
        config.mGuiStatusBarBackground = readButtonColor(ui.mColorStatusBarBg);
        config.mGuiHelpPanelForeground = readButtonColor(ui.mColorHelpPanelFg);
        config.mGuiHelpPanelBackground = readButtonColor(ui.mColorHelpPanelBg);
        config.mGuiHelpPanelKeystrokeForeground = readButtonColor(ui.mColorHelpKeyFg);
        config.mGuiHelpPanelKeystrokeBackground = readButtonColor(ui.mColorHelpKeyBg);
        config.mGuiRulerForeground = readButtonColor(ui.mColorRulerFg);
        config.mGuiRulerBackground = readButtonColor(ui.mColorRulerBg);

        // --- Read back Tab 5: Display ---
        config.mGuiShowHelp = ui.mShowHelp->currentIndex();

        config.mGuiShowRuler = ui.mShowRuler->isChecked();
        config.mGuiShowScrollBar = ui.mShowScrollBar->isChecked();
        config.mGuiShowStatusBar = ui.mShowStatusBar->isChecked();
        config.mGuiShowStyleBar = ui.mShowStyleBar->isChecked();
        config.mGuiShowMenu = ui.mShowMenu->isChecked();
        config.mGuiAlwaysDotCommands = ui.mDotAlwaysOn->isChecked() ;
        config.mGuiAlwaysFlagColumn = ui.mFlagAlwaysOn->isChecked() ;

        // --- Read back Tab 6: User ---
        config.mShortName = ui.mShortName->text().toStdString();
        config.mLongName = ui.mLongName->text().toStdString();

        // Save to disk
        config.Save();

        // Apply input mode change (swaps handler, menu provider, rebuilds menus)
        if (mWordTsar)
        {
            eInputMode newMode = static_cast<eInputMode>(config.mInputMode);
            mWordTsar->OnInputModeChanged(newMode);
        }

        // Apply to live editor state
        if (config.mShowControls)
        {
            SetShowControls(SHOW_ALL);
        }
        else
        {
            SetShowControls(SHOW_NONE);
        }

        mHelpLevel = std::clamp(config.mGuiShowHelp, 0, 4);
        mHelpDisplay = (mHelpLevel == 3) ? HELP_MAIN : HELP_NONE;
        SetMeasurement(config.mMeasurement);
        SetCodePage(static_cast<eCodePage>(config.mCodePage));
        mSpellCheckLanguage = config.mSpellCheckLanguage;
        mSpellCheckDotCommands = config.mSpellCheckDotCommands;
        mCaretBlinkRate = config.mCaretBlinkRate;
        mAutoSaveIntervalSec = config.mAutoSaveInterval;
        mDefaultFormat = config.mDefaultFormat;
        mShortName = config.mShortName;
        mLongName = config.mLongName;

        if (!config.mDefaultDirectory.empty())
        {
            mFileDir = config.mDefaultDirectory;
        }

        // Apply display flags
        mDispRuler = config.mGuiShowRuler;
        mDispScrollBar = config.mGuiShowScrollBar;
        mDispStatusBar = config.mGuiShowStatusBar;
        mDispStyleBar = config.mGuiShowStyleBar;
        mDispMenu = config.mGuiShowMenu;
        mAlwaysDot = config.mGuiAlwaysDotCommands;
        mAlwaysFlag = config.mGuiAlwaysFlagColumn;

        // Apply colors
        auto toQColor = [](sRGB c) { return QColor(c.r, c.g, c.b); };
        auto toQColorA = [](sRGB c, int a) { return QColor(c.r, c.g, c.b, a); };

        SetBGroundColour(toQColor(config.mGuiBackground));
        SetTextColour(toQColor(config.mGuiForeground));
        SetHighlightColour(toQColorA(config.mGuiHighlightBackground, 127));
        SetDotColour(toQColorA(config.mGuiDotBackground, 190));
        SetBlockColour(toQColorA(config.mGuiBlockBackground, 190));
        SetCommentColour(toQColorA(config.mGuiCommentBackground, 190));
        SetErrorColour(toQColorA(config.mGuiErrorBackground, 190));
        SetUnknownColour(toQColorA(config.mGuiUnknownBackground, 190));
        SetNotImplementedColour(toQColorA(config.mGuiNotImplementedBackground, 190));
        SetSearchColour(toQColorA(config.mGuiSearchBackground, 75));

        // Editor overlay foreground colors (opaque)
        SetHighlightFgColour(toQColor(config.mGuiHighlightForeground));
        SetDotFgColour(toQColor(config.mGuiDotForeground));
        SetCommentFgColour(toQColor(config.mGuiCommentForeground));
        SetErrorFgColour(toQColor(config.mGuiErrorForeground));
        SetUnknownFgColour(toQColor(config.mGuiUnknownForeground));
        SetNotImplementedFgColour(toQColor(config.mGuiNotImplementedForeground));

        // Tell the main window to update display settings and screen colors
        if (mWordTsar)
        {
            mWordTsar->ApplyDisplaySettings();
            mWordTsar->ApplyScreenColors();
        }

        Repaint();

        // Propagate color changes to sibling editor (reveal codes pane)
        if (mSiblingEditor != nullptr)
        {
            cEditorCtrl* sibling = static_cast<cEditorCtrl*>(mSiblingEditor);

            // Background and overlay colors
            sibling->SetBGroundColour(toQColor(config.mGuiBackground));
            sibling->SetTextColour(toQColor(config.mGuiForeground));
            sibling->SetHighlightColour(toQColorA(config.mGuiHighlightBackground, 127));
            sibling->SetDotColour(toQColorA(config.mGuiDotBackground, 190));
            sibling->SetBlockColour(toQColorA(config.mGuiBlockBackground, 190));
            sibling->SetCommentColour(toQColorA(config.mGuiCommentBackground, 190));
            sibling->SetErrorColour(toQColorA(config.mGuiErrorBackground, 190));
            sibling->SetUnknownColour(toQColorA(config.mGuiUnknownBackground, 190));
            sibling->SetNotImplementedColour(toQColorA(config.mGuiNotImplementedBackground, 190));
            sibling->SetSearchColour(toQColorA(config.mGuiSearchBackground, 75));

            // Foreground colors
            sibling->SetHighlightFgColour(toQColor(config.mGuiHighlightForeground));
            sibling->SetDotFgColour(toQColor(config.mGuiDotForeground));
            sibling->SetCommentFgColour(toQColor(config.mGuiCommentForeground));
            sibling->SetErrorFgColour(toQColor(config.mGuiErrorForeground));
            sibling->SetUnknownFgColour(toQColor(config.mGuiUnknownForeground));
            sibling->SetNotImplementedFgColour(toQColor(config.mGuiNotImplementedForeground));

            sibling->Repaint();
        }
    }
}
