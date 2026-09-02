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

/**
 * @class cWSEditorCtrl
 * @brief wordstartui editor control implementation.
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * Parallel of cTUIEditorCtrl for the wordstartui rendering toolkit. Provides
 * concrete overrides of all cEditorBase pure virtuals: viewport management,
 * scroll/caret handling, background incremental layout, and file I/O with
 * format detection (WordStar / RTF / DOCX / plain text). Colors are stored as
 * wordstartui::sColor. Dialog and message virtuals present real modal dialogs
 * through the iWSDialogHost that the application installs.
 */

#include "editorctrl.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

// TUI layout
#include "src/tui/layout/layout.h"
#include "src/core/layout/layoutstructs.h"

// wordstartui dialogs and dialog host
#include "src/tui/dialogs/dialogs.h"
#include "src/tui/dialogs/dialoghost.h"

// Version info (About dialog)
#include "src/core/include/version.h"
#include "src/core/include/config.h"
#include "src/core/utils/config.h"

// Spell checking
#include "src/core/spellcheck/spellcheck.h"

// Print preview (PDF via Quartz/Core Text, backend-neutral)
#include "src/tui/print/tuiprintout.h"

// Input handlers (Strategy Pattern)
#include "src/input/wordtsarinput.h"
#include "src/input/moderninput.h"

// File handlers
#include "src/files/wordstar/wordstarfile.h"
#include "src/files/rtffile.h"
#include "src/files/docxfile.h"
#include "src/files/textfile.h"

using wordstartui::MakeRgb;


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Creates the document and layout, initializes the font
/// system, wires the listener, caches font metrics, and installs the
/// default WordStar input handler. Colors are initialized to the same RGB
/// values used by the editor control.
///
/////////////////////////////////////////////////////////////////////////////
cWSEditorCtrl::cWSEditorCtrl(void)
    : cEditorBase()
    , mInput(nullptr)
    , mBGroundColour(MakeRgb(245, 245, 245))
    , mTextColour(MakeRgb(0, 0, 0))
    , mScrollbarBg(MakeRgb(122, 122, 122))
    , mScrollbarFg(MakeRgb(245, 245, 245))
    , mFlagBg(MakeRgb(0, 0, 0))
    , mFlagFg(MakeRgb(245, 245, 245))
    , mHighlightColour(MakeRgb(123, 198, 223))
    , mHighlightFgColour(MakeRgb(0, 0, 0))
    , mDotColour(MakeRgb(137, 212, 212))
    , mDotFgColour(MakeRgb(0, 0, 0))
    , mBlockColour(MakeRgb(188, 202, 232))
    , mBlockFgColour(MakeRgb(0, 0, 0))
    , mSearchColour(MakeRgb(50, 100, 200))
    , mSearchFgColour(MakeRgb(0, 0, 0))
    , mCommentColour(MakeRgb(252, 195, 138))
    , mCommentFgColour(MakeRgb(0, 0, 0))
    , mErrorColour(MakeRgb(207, 115, 111))
    , mErrorFgColour(MakeRgb(255, 255, 255))
    , mUnknownColour(MakeRgb(207, 137, 62))
    , mUnknownFgColour(MakeRgb(255, 255, 255))
    , mNotImplementedColour(MakeRgb(215, 206, 63))
    , mNotImplementedFgColour(MakeRgb(0, 0, 0))
    , mBoldColour(MakeRgb(0, 0, 180))
    , mBoldBgColour(MakeRgb(245, 245, 245))
    , mItalicColour(MakeRgb(0, 128, 128))
    , mItalicBgColour(MakeRgb(245, 245, 245))
    , mStrikethroughColour(MakeRgb(180, 0, 0))
    , mStrikethroughBgColour(MakeRgb(245, 245, 245))
    , mUnderlineColour(MakeRgb(128, 0, 128))
    , mUnderlineBgColour(MakeRgb(245, 245, 245))
    , mSuperscriptColour(MakeRgb(100, 100, 0))
    , mSuperscriptBgColour(MakeRgb(245, 245, 245))
    , mSubscriptColour(MakeRgb(0, 100, 100))
    , mSubscriptBgColour(MakeRgb(245, 245, 245))
    , mAlwaysFlag(true)
    , mTitle()
    , mLastMessage()
    , mTerminalRows(24)
    , mTerminalCols(80)
    , mChromeRows(3)
    , mViewFirstLine(0)
    , mViewRows(24)
    , mColumnWidth(0)
    , mLineHeightTwips(240)
    , mQuit(false)
    , mReturnToOpening(false)
    , mTermSupportsBold(true)
    , mTermSupportsItalic(true)
    , mTermSupportsUnderline(true)
    , mTermSupportsStrikethrough(true)
    , mCaretTimerRunning(false)
    , mInputMode(INPUT_WORDSTAR)
    , mRedrawHook(nullptr)
    , mHost(nullptr)
    , mClipboardTool(0)
    , mSearchText()
    , mReplaceText()
    , mWholeWord(false)
    , mCaseCmp(false)
    , mSearchBackwards(false)
    , mWildCard(false)
    , mWholeFile(false)
    , mReplaceAsk(true)
    , mReplaceScope(0)
    , mReplaceSize(0)
    , mLastFindandReplace(0)
{
    // wordstartui does not support page view mode (no pixel rendering)
    mPageModeSupported = false;

    // Create and own document and layout
    mDocument = new cDocument();
    mLayout = new cLayout();

    // Initialize TUI font system
    cLayout* tuiLayout = static_cast<cLayout*>(mLayout);
    tuiLayout->InitializeFontSystem();

    // Concrete monospace font so text mMeasurement is deterministic
    mLayout->SetDefaultFont("Courier New|12.0|0|0|0|0|0");

    // Connect layout to document
    mLayout->SetDocument(mDocument);

    // Register as listener for document change notifications
    mDocument->AddListener(this);

    // Cache the monospace column width
    mColumnWidth = mLayout->GetTextWidth("x");

    // Sync document ShowControl to display settings default
    mDocument->SetShowControl(mDisplaySettings.showControl);

    // Initialize background layout state
    mLayoutParagraph = 0;
    mLayoutInt = false;
    mLayoutRest = false;

    // Create input handler using Strategy Pattern
    SetInputMode(INPUT_WORDSTAR);

    // Initialize caret position
    CalculateCaretPosition();

    // Start word count timer (base class background thread)
    StartWordCountTimer();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor. Stops timers and releases owned resources (input handler,
/// layout, document).
///
/////////////////////////////////////////////////////////////////////////////
cWSEditorCtrl::~cWSEditorCtrl(void)
{
    StopWordCountTimer();

    delete mInput;
    mInput = nullptr;

    if (mDocument)
    {
        mDocument->RemoveListener(this);
    }

    if (mLayout)
    {
        cLayout* tuiLayout = static_cast<cLayout*>(mLayout);
        tuiLayout->ShutdownFontSystem();
    }

    delete mLayout;
    mLayout = nullptr;

    delete mDocument;
    mDocument = nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  host [in] dialog host used to run modal dialogs (not owned)
///
/// @return nothing
///
/// @brief
/// Install the dialog host. All dialog methods route their modal UI through
/// this host; when it is null they do nothing.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetDialogHost(iWSDialogHost* host)
{
    mHost = host;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Trigger a redraw by invoking the redraw hook if one is installed.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Repaint(void)
{
    if (mRedrawHook)
    {
        mRedrawHook();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T viewport height in twips
///
/// @brief
/// Convert editor rows to twips for viewport height. One terminal row maps
/// to one mLineHeightTwips unit of document height.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cWSEditorCtrl::GetViewportHeight(void) const
{
    int editorRows = GetEditorRows();
    return static_cast<COORD_T>(editorRows) * mLineHeightTwips;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  delay [in] delay in milliseconds (unused)
///
/// @return nothing
///
/// @brief
/// No-op caret timer start. The wordstartui build runs a single-threaded
/// loop with no caret blink.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::StartCaretTimer(long /*delay*/)
{
    mCaretTimerRunning = true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// No-op caret timer stop.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::StopCaretTimer(void)
{
    mCaretTimerRunning = false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Perform a coordinated visual update: caret position, scroll, repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::PerformVisualUpdate(void)
{
    if (!mLayout || !mDocument)
    {
        return;
    }

    CalculateCaretPosition();
    ScrollIntoView();
    CalculateCaretPosition();   // Recalculate after scroll

    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return PARAGRAPH_T - last visible paragraph number
///
/// @brief
/// Find the last visible paragraph based on the current scroll position and
/// viewport height. Used for incremental layout range calculation.
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cWSEditorCtrl::GetLastVisibleParagraph(void)
{
    if (!mLayout || !mDocument)
    {
        return 0;
    }

    COORD_T viewportTop = mScrollOffset;
    COORD_T viewportHeight = GetViewportHeight();
    COORD_T viewportBottom = viewportTop + viewportHeight;

    PARAGRAPH_T lastVisible = 0;

    // Clamp to minimum of layout and document counts. After cross-paragraph
    // deletes, layout may have stale entries beyond the document count.
    PARAGRAPH_T totalParagraphs = std::min(
        mLayout->GetNumberOfParagraphs(),
        mDocument->GetNumberofParagraphs());
    bool foundFirst = false;

    for (PARAGRAPH_T para = 0; para < totalParagraphs; ++para)
    {
        const sParagraphLayout* paragraph = mLayout->GetParagraphLayout(para);
        if (!paragraph || paragraph->lines.empty())
        {
            continue;
        }

        const sLineLayout& firstLine = paragraph->lines.front();
        const sLineLayout& lastLine = paragraph->lines.back();

        if (lastLine.screeny >= viewportTop && firstLine.screeny <= viewportBottom)
        {
            if (!foundFirst)
            {
                mVisibleStart = para;
                foundFirst = true;
            }
            lastVisible = para;
        }
        else if (firstLine.screeny > viewportBottom)
        {
            break;
        }
    }

    return lastVisible;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  offset [in] new scroll offset in twips
///
/// @return nothing
///
/// @brief
/// Snap the scroll offset to the nearest line boundary at or below the
/// requested offset so the top of the viewport aligns to a document line,
/// then repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetScrollOffset(COORD_T offset)
{
    if (mLayout && offset > 0)
    {
        LINE_T totalLines = mLayout->GetNumberOfLines();
        for (LINE_T i = 0; i < totalLines; ++i)
        {
            const sLineLayout* line = mLayout->GetLineByRawLineNumber(i);
            if (!line)
            {
                continue;
            }
            if (line->screeny == offset)
            {
                break;
            }
            if (line->screeny > offset)
            {
                if (i > 0)
                {
                    const sLineLayout* prev = mLayout->GetLineByRawLineNumber(i - 1);
                    if (prev)
                    {
                        offset = prev->screeny;
                    }
                }
                else
                {
                    offset = 0;
                }
                break;
            }
        }
    }

    cEditorBase::SetScrollOffset(offset);
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Keep the caret visible by adjusting the scroll offset. Scrolls up so the
/// caret line sits at the top when it is above the viewport, or advances the
/// top line downward until the caret line fits when it is below.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::ScrollIntoView(void)
{
    if (!mLayout)
    {
        return;
    }

    // mCaretLine is a contentLineNumber; convert the caret's document position
    // to a rawLineNumber for the line lookup.
    LINE_T caretRawLine = mLayout->GetLineFromPosition(mCaretDocumentPosition);
    const sLineLayout* caretLine = mLayout->GetLineByRawLineNumber(caretRawLine);
    if (!caretLine)
    {
        return;
    }

    COORD_T viewportHeight = GetViewportHeight();

    // Caret line above viewport: scroll up so caret line sits at the top.
    if (caretLine->screeny < mScrollOffset)
    {
        cEditorBase::SetScrollOffset(caretLine->screeny);
        return;
    }

    // Caret line below viewport: scroll down by one line height until the
    // caret line's bottom fits within the viewport.
    COORD_T caretBottom = caretLine->screeny + mLineHeightTwips;
    if (caretBottom > mScrollOffset + viewportHeight)
    {
        COORD_T newOffset = caretBottom - viewportHeight;
        if (newOffset < 0)
        {
            newOffset = 0;
        }
        cEditorBase::SetScrollOffset(newOffset);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return cLayoutBase* - the layout engine
///
/// @brief
/// Expose the protected layout pointer for the view/app.
///
/////////////////////////////////////////////////////////////////////////////
cLayoutBase* cWSEditorCtrl::GetLayout(void) const
{
    return mLayout;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return cDocument* - the document model
///
/// @brief
/// Expose the document pointer for the view/app.
///
/////////////////////////////////////////////////////////////////////////////
cDocument* cWSEditorCtrl::GetDocument(void) const
{
    return mDocument;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return IInputHandler* - the active input handler
///
/// @brief
/// Expose the input handler for the view/app to route keystrokes.
///
/////////////////////////////////////////////////////////////////////////////
IInputHandler* cWSEditorCtrl::GetInput(void) const
{
    return mInput;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  mode [in] input mode to switch to
///
/// @return nothing
///
/// @brief
/// Set the keyboard input mode (Strategy Pattern). Builds the new handler
/// before releasing the old one so mInput is never null.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetInputMode(eInputMode mode)
{
    IInputHandler* newInput = nullptr;
    switch (mode)
    {
        case INPUT_WORDSTAR:
        {
            newInput = new cWordStarInput(this);
            break;
        }
        case INPUT_MODERN:
        {
            newInput = new cModernInput(this);
            break;
        }
        default:
        {
            newInput = new cWordStarInput(this);
            mode = INPUT_WORDSTAR;
            break;
        }
    }

    IInputHandler* oldInput = mInput;
    mInput = newInput;
    delete oldInput;

    mInputMode = mode;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return eInputMode - current input mode
///
/// @brief
/// Get the current keyboard input mode.
///
/////////////////////////////////////////////////////////////////////////////
eInputMode cWSEditorCtrl::GetInputMode(void) const
{
    return mInputMode;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool bold [in] terminal renders bold
/// @param  bool italic [in] terminal renders italic
/// @param  bool underline [in] terminal renders underline
/// @param  bool strikethrough [in] terminal renders strikethrough
///
/// @return nothing
///
/// @brief
/// Record which text attributes the terminal can render natively. Attributes
/// the terminal cannot render fall back to substitute colors.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetTermCapabilities(bool bold, bool italic, bool underline, bool strikethrough)
{
    mTermSupportsBold = bold;
    mTermSupportsItalic = italic;
    mTermSupportsUnderline = underline;
    mTermSupportsStrikethrough = strikethrough;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  firstLine [in] first visible line number
/// @param  rows [in] number of visible rows
///
/// @return nothing
///
/// @brief
/// Store the visible line range reported by the view.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetViewport(LINE_T firstLine, int rows)
{
    mViewFirstLine = firstLine;
    mViewRows = rows;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rows [in] terminal height in rows
/// @param  cols [in] terminal width in columns
///
/// @return nothing
///
/// @brief
/// Update terminal dimensions and recalculate viewport.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetTerminalSize(int rows, int cols)
{
    mTerminalRows = rows;
    mTerminalCols = cols;

    CalculateViewport();
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rows [in] number of rows used by UI chrome
///
/// @return nothing
///
/// @brief
/// Set the number of terminal rows used by UI chrome and recalculate the
/// viewport if the value changed.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetChromeRows(int rows)
{
    if (rows != mChromeRows)
    {
        mChromeRows = rows;
        CalculateViewport();
        CalculateCaretPosition();
        ScrollIntoView();
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return int - number of editor text rows
///
/// @brief
/// Editor rows are the terminal height minus the chrome rows, at least 1.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorCtrl::GetEditorRows(void) const
{
    return std::max(1, mTerminalRows - mChromeRows);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return int - terminal height in rows
///
/// @brief
/// Get the current terminal height.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorCtrl::GetTerminalRows(void) const
{
    return mTerminalRows;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return int - terminal width in columns
///
/// @brief
/// Get the current terminal width.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorCtrl::GetTerminalCols(void) const
{
    return mTerminalCols;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return int - text area width in columns
///
/// @brief
/// Terminal width minus the indicator column(s). When the indicator column
/// is hidden it is reclaimed for the text area.
///
/////////////////////////////////////////////////////////////////////////////
int cWSEditorCtrl::GetTextAreaCols(void) const
{
    bool showInd = mAlwaysFlag || (GetShowControls() == SHOW_ALL);
    int offset = 1;
    if (showInd)
    {
        offset = 2;
    }
    return std::max(1, mTerminalCols - offset);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - cached column width in twips
///
/// @brief
/// Return the cached monospace column width, falling back to 120 twips if
/// it has not been measured.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cWSEditorCtrl::GetColumnWidth(void) const
{
    if (mColumnWidth > 0)
    {
        return mColumnWidth;
    }
    return 120;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - twips per terminal column
///
/// @brief
/// One terminal column equals one monospace character, so twips-per-column
/// is the cached column width.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cWSEditorCtrl::GetTwipsPerColumn(void) const
{
    return GetColumnWidth();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - line height in twips
///
/// @brief
/// Get the fixed line height used by the viewport calculation.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cWSEditorCtrl::GetLineHeightTwips(void) const
{
    return mLineHeightTwips;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  hook [in] callback invoked by Repaint
///
/// @return nothing
///
/// @brief
/// Install a redraw hook that Repaint invokes to refresh the screen.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetRedrawHook(std::function<void()> hook)
{
    mRedrawHook = hook;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Force a full relayout, clear the dirty flag, and repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::RelayoutAndRedraw(void)
{
    LayoutDocument(true);
    ResetDocumentDirty();
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if more paragraphs remain to lay out
///
/// @brief
/// Lay out a budget of paragraphs during idle time, advancing a running
/// counter. Rewinds to the earliest dirty paragraph when the document has
/// changed. Clears the dirty flag when the pass completes.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorCtrl::IdleLayout(void)
{
    if (!mLayout || !mDocument)
    {
        return false;
    }

    // Rewind if document changed before the current position
    if (mDocumentDirty)
    {
        if (mDirtyFromParagraph < mLayoutParagraph)
        {
            mLayoutParagraph = mDirtyFromParagraph;
        }
        mDocumentDirty = false;
    }

    PARAGRAPH_T totalParagraphs = mDocument->GetNumberofParagraphs();

    if (mLayoutParagraph >= totalParagraphs)
    {
        ResetDocumentDirty();
        mLayoutRest = false;
        return false;
    }

    // Lay out a fixed budget of paragraphs this pass
    constexpr PARAGRAPH_T LAYOUT_BUDGET = 20;
    PARAGRAPH_T done = 0;
    while (mLayoutParagraph < totalParagraphs && done < LAYOUT_BUDGET)
    {
        mLayout->LayoutParagraph(mLayoutParagraph);
        mLayoutParagraph++;
        done++;
    }

    if (mLayoutParagraph >= totalParagraphs)
    {
        ResetDocumentDirty();
        mLayoutRest = false;
        return false;
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if background layout has more work
///
/// @brief
/// Report whether background layout has not yet reached the end of the
/// document.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorCtrl::HasPendingLayout(void) const
{
    if (!mDocument)
    {
        return false;
    }

    // A pending edit (mDocumentDirty) needs re-layout even when the background
    // layout counter has already reached the end.
    if (mDocumentDirty)
    {
        return true;
    }

    return mLayoutParagraph < mDocument->GetNumberofParagraphs();
}


// =========================================================================
// Undo / Redo
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Undo the last document change.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Undo(void)
{
    if (mDocument)
    {
        mDocument->Undo();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Redo the last undone document change.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Redo(void)
{
    if (mDocument)
    {
        mDocument->Redo();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Cycle the control-code display mode SHOW_ALL to SHOW_DOT to SHOW_NONE and
/// relayout, since showing/hiding control codes changes the layout.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::ToggleShowControl(void)
{
    if (!mLayout)
    {
        return;
    }

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

    SetShowControls(nextMode);
    mLayout->LayoutDocument(mDocument);
    Repaint();
}


// =========================================================================
// File operations
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  force [in] force complete relayout
///
/// @return nothing
///
/// @brief
/// Lay out the document. When forced, lays out the whole document; otherwise
/// lays out just the first visible range.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::LayoutDocument(bool force)
{
    if (!mLayout || !mDocument)
    {
        return;
    }

    if (force)
    {
        mLayout->LayoutDocument(mDocument);
        mLayoutParagraph = mDocument->GetNumberofParagraphs();
    }
    else
    {
        PARAGRAPH_T totalParas = mDocument->GetNumberofParagraphs();
        for (PARAGRAPH_T para = 0; para < totalParas && para < 10; ++para)
        {
            mLayout->LayoutParagraph(para);
        }
        mLayoutParagraph = std::min(totalParas, static_cast<PARAGRAPH_T>(10));
    }

    // Refresh cached column width after layout (font may have changed)
    mColumnWidth = mLayout->GetTextWidth("x");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] path to file to load
///
/// @return bool - true if loaded successfully
///
/// @brief
/// Load a file. Detects the file type from content/extension and delegates
/// to the appropriate handler. Loads synchronously (no progress thread).
/// Resets editor state after a successful load.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorCtrl::LoadFile(const std::string& filename)
{
    if (filename.empty())
    {
        return false;
    }

    std::filesystem::path filepath = std::filesystem::absolute(filename);

    // Detect file type using each handler's CheckType()
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
        fileReader = new cTextFile(this);
    }

    // Save dirty flag before loading (Insert() sets mChanged per character)
    bool wasDirty = mDocument->mChanged;

    // Block idle layout during file loading
    mDocument->SetLoading(true);

    // Show the load-progress overlay. Only the WordStar reader reports
    // incremental progress; for other formats the box appears at 0% and
    // clears when the (blocking) load returns.
    mLoadingFileName = filepath.filename().string();
    if (mHost != nullptr)
    {
        mHost->HostShowLoadProgress(mLoadingFileName, 0);
    }

    // Load synchronously
    bool success = fileReader->LoadFile(filename);

    if (mHost != nullptr)
    {
        mHost->HostHideLoadProgress();
    }

    // Re-enable idle layout
    mDocument->SetLoading(false);

    // Restore dirty flag (loading shouldn't change dirty state)
    mDocument->mChanged = wasDirty;

    delete fileReader;

    if (success)
    {
        SetScrollOffset(0);

        if (mDocument)
        {
            mDocument->UnsetBlock();
            mDocument->SetPosition(0);
            mDocument->ClearUndoHistory();
        }

        LoadFileState(filename);

        LayoutDocument(true);

        CalculateCaretPosition();
        ScrollIntoView();
        Repaint();

        mFileName = filepath.filename().string();
        mFileDir = filepath.parent_path().string();
        mFileSet = true;

        mLayout->SetFilename(mFileName);
        mLayout->SetFileDir(mFileDir);

        SetTitle(mFileName);

        std::filesystem::path backupPath = filepath.parent_path() /
            (filepath.stem().string() + "-bak.ws");
        mBackupFileName = backupPath.string();

        // Record the opened file in the recent-files list (mirrors the GUI).
        cConfig recentConfig;
        recentConfig.Load();
        recentConfig.AddRecentFile(filepath.string());
        recentConfig.Save();
    }

    return success;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  percent [in] load progress 0..100
///
/// @return nothing
///
/// @brief
/// File-load progress callback (invoked by the file readers via
/// cFile::UpdateProgress). Updates the host's load-progress overlay.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::FileIOProgress(int percent)
{
    if (mHost != nullptr)
    {
        mHost->HostShowLoadProgress(mLoadingFileName, percent);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] path to file to save to
///
/// @return bool - true if saved successfully
///
/// @brief
/// Save the document. Detects the format from the extension and delegates to
/// the appropriate handler. Also writes a WordStar backup (except DOCX).
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorCtrl::SaveFile(const std::string& filename)
{
    if (filename.empty() || !mDocument)
    {
        return false;
    }

    std::filesystem::path filepath = std::filesystem::absolute(filename);
    std::string ext = filepath.extension().string();

    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return std::tolower(c); });

    cFile* fileWriter = nullptr;

    if (ext == ".ws" || ext == ".ws7" || ext == ".ws4")
    {
        fileWriter = new cWordstarFile(this);
    }
    else if (ext == ".rtf")
    {
        fileWriter = new cRTFFile(this);
    }
    else if (ext == ".txt")
    {
        fileWriter = new cTextFile(this);
    }
    else
    {
        fileWriter = new cWordstarFile(this);
    }

    POSITION_T docSize = mDocument->GetTextSize();
    bool success = fileWriter->SaveFile(filename, docSize);

    delete fileWriter;

    if (success)
    {
        mFileName = filepath.filename().string();
        mFileDir = filepath.parent_path().string();
        mLayout->SetFilename(mFileName);
        mLayout->SetFileDir(mFileDir);

        SetTitle(mFileName);

        std::filesystem::path backupPath = filepath.parent_path() /
            (filepath.stem().string() + "-bak.ws");
        mBackupFileName = backupPath.string();

        if (ext != ".docx")
        {
            cFile* backupWriter = new cWordstarFile(this);
            POSITION_T backupSize = mDocument->GetTextSize();
            backupWriter->SaveFile(mBackupFileName, backupSize);
            delete backupWriter;
        }

        SaveFileState(filename);
    }
    else
    {
        ShowError("Error", "File save failed.");
    }

    return success;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] optional crash message to log to stderr
///
/// @return nothing
///
/// @brief
/// Emergency save on a fatal error. Saves to filename.emergency when the
/// document has unsaved changes.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::EmergencySaveFile(char *text)
{
    if (!mDocument || !mDocument->mChanged)
    {
        return;
    }

    std::string filename = mFileName;
    if (filename.empty())
    {
        filename = "untitled";
    }

    std::string emergency = filename + ".emergency";
    SaveFile(emergency);

    if (text)
    {
        fprintf(stderr, "Emergency save: %s\n", text);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Periodic auto-save. Writes the document to its -bak backup file in
/// WordStar format when there are unsaved changes. Called from the host's
/// frame loop on the configured interval. Skips documents with no backup
/// name (nothing loaded/saved yet) or no changes.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::AutoSaveBackup(void)
{
    if (mBackupFileName.empty() == true)
    {
        return;
    }

    if (!mDocument || mDocument->mChanged == false)
    {
        return;
    }

    SetStatusMessage("Saving backup...", true, 30);

    cFile* backupWriter = new cWordstarFile(this);
    POSITION_T backupSize = mDocument->GetTextSize();
    backupWriter->SaveFile(mBackupFileName, backupSize);
    delete backupWriter;

    mDocument->ShrinkToFit();
    if (mLayout != nullptr)
    {
        mLayout->ShrinkToFit();
    }

    SetStatusMessage("Backup saved", false, 30);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - empty (no file dialog yet)
///
/// @brief
/// Prompt for a file to open via the file browser dialog; returns the chosen
/// path or empty on cancel.
///
/////////////////////////////////////////////////////////////////////////////
std::string cWSEditorCtrl::PromptForLoadFile(void)
{
    if (mHost == nullptr)
    {
        return "";
    }

    std::string startDir = mFileDir;
    if (startDir.empty() == true)
    {
        startDir = ".";
    }

    std::string result;
    if (wsdialogs::FileBrowser(mHost, "Open File", false, startDir, result) == true)
    {
        return result;
    }

    return "";
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - empty (no file dialog yet)
///
/// @brief
/// Prompt for a file to save to via the file browser dialog; returns the chosen
/// path or empty on cancel.
///
/////////////////////////////////////////////////////////////////////////////
std::string cWSEditorCtrl::PromptForSaveFile(void)
{
    if (mHost == nullptr)
    {
        return "";
    }

    std::string startDir = mFileDir;
    if (startDir.empty() == true)
    {
        startDir = ".";
    }

    std::string result;
    if (wsdialogs::FileBrowser(mHost, "Save File", true, startDir, result) == true)
    {
        return result;
    }

    return "";
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - always true
///
/// @brief
/// Handle close. Allows closing unconditionally for now.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorCtrl::CloseEvent(void)
{
    if ((mDocument == nullptr) || (mDocument->mChanged == false))
    {
        return true;
    }

    if (mHost == nullptr)
    {
        return true;
    }

    int choice = wsdialogs::ThreeChoice(mHost, "Close",
                                        "The document has unsaved changes.",
                                        "Save", "Don't Save", "Cancel");

    if (choice == 0)
    {
        std::string filename = mFileName;

        if (filename.empty() == true)
        {
            filename = PromptForSaveFile();
            if (filename.empty() == true)
            {
                return false;
            }
        }

        return SaveFile(filename);
    }
    else if (choice == 1)
    {
        return true;
    }

    return false;
}


// =========================================================================
// Dialog / message operations
// =========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] name of the unimplemented command
///
/// @return nothing
///
/// @brief
/// Report that a command is not yet implemented.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::NotImplemented(const std::string& command)
{
    ShowMessage("Not Implemented", command + " is not yet implemented");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] name of the invalid command
///
/// @return nothing
///
/// @brief
/// Report that an invalid command was entered.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::InvalidCommand(const std::string& command)
{
    ShowMessage("Invalid Command", command);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] window/document title
///
/// @return nothing
///
/// @brief
/// Store the window title for the app to display.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetTitle(const std::string& title)
{
    mTitle = title;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] error dialog title
/// @param  message [in] error message
///
/// @return nothing
///
/// @brief
/// Display an error message as a modal dialog.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::ShowError(const std::string& title, const std::string& message)
{
    if (!mHost)
    {
        return;
    }
    wsdialogs::MessageBox(mHost, "ERROR: " + title, message);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] message title
/// @param  message [in] message text
///
/// @return nothing
///
/// @brief
/// Display an informational message as a modal dialog.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::ShowMessage(const std::string& title, const std::string& message)
{
    if (!mHost)
    {
        return;
    }
    wsdialogs::MessageBox(mHost, title, message);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  title [in] dialog title
/// @param  question [in] yes/no question
///
/// @return bool - true for yes, false for no
///
/// @brief
/// Ask the user a yes/no question via a modal dialog. Defaults to no when
/// no dialog host is available.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorCtrl::AskYesNo(const std::string& title, const std::string& question)
{
    if (!mHost)
    {
        return false;
    }
    return wsdialogs::YesNo(mHost, title, question);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Display the status dialog with version and memory usage statistics.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::About(void)
{
    if (!mHost)
    {
        return;
    }

    auto formatBytes = [](size_t bytes) -> std::string
    {
        if (bytes < 1024)
        {
            return std::to_string(bytes) + " B";
        }
        else if (bytes < 1024 * 1024)
        {
            return std::to_string(bytes / 1024) + "." +
                   std::to_string((bytes % 1024) * 10 / 1024) + " KB";
        }
        else
        {
            return std::to_string(bytes / (1024 * 1024)) + "." +
                   std::to_string((bytes % (1024 * 1024)) * 10 / (1024 * 1024)) + " MB";
        }
    };

    auto formatUsedAlloc = [&](size_t used, size_t alloc) -> std::string
    {
        return formatBytes(used) + " / " + formatBytes(alloc);
    };

    std::string info = std::string("WordTsar ") + FULLVERSION_STRING;

    if (mDocument && mLayout)
    {
        auto docMem = mDocument->GetMemoryUsage();
        auto layMem = mLayout->GetMemoryUsage();

        size_t docTotal = docMem.textBytes + docMem.attributeBytes +
                          docMem.undoBytes + docMem.redoBytes + docMem.copyBufferBytes;
        size_t layTotal = layMem.paragraphBytes + layMem.lineBytes + layMem.segmentBytes +
                          layMem.checkpointBytes + layMem.boxBytes;

        size_t docUsedTotal = docMem.textUsedBytes + docMem.attributeUsedBytes +
                              docMem.undoBytes + docMem.redoBytes + docMem.copyBufferBytes;
        size_t layUsedTotal = layMem.paragraphUsedBytes + layMem.lineUsedBytes +
                              layMem.segmentUsedBytes + layMem.checkpointBytes + layMem.boxBytes;

        info += "\n\nMemory: used / allocated";
        info += "\n\nDocument: " + std::to_string(docMem.paragraphCount) + " paragraphs";
        info += "\n  Text:        " + formatUsedAlloc(docMem.textUsedBytes, docMem.textBytes);
        info += "\n  Attributes:  " + formatUsedAlloc(docMem.attributeUsedBytes, docMem.attributeBytes);
        info += "\n  Undo:        " + formatBytes(docMem.undoBytes) +
                " (" + std::to_string(docMem.undoGroupCount) + " groups)";
        info += "\n  Redo:        " + formatBytes(docMem.redoBytes) +
                " (" + std::to_string(docMem.redoGroupCount) + " groups)";
        info += "\n  Copy buffer: " + formatBytes(docMem.copyBufferBytes);
        info += "\n  Subtotal:    " + formatUsedAlloc(docUsedTotal, docTotal);

        info += "\n\nLayout: " + std::to_string(layMem.paragraphCount) + " para, " +
                std::to_string(layMem.lineCount) + " lines, " +
                std::to_string(layMem.segmentCount) + " segs";
        info += "\n  Paragraphs:  " + formatUsedAlloc(layMem.paragraphUsedBytes, layMem.paragraphBytes);
        info += "\n  Lines:       " + formatUsedAlloc(layMem.lineUsedBytes, layMem.lineBytes);
        info += "\n  Segments:    " + formatUsedAlloc(layMem.segmentUsedBytes, layMem.segmentBytes);
        info += "\n  Checkpoints: " + formatBytes(layMem.checkpointBytes) +
                " (" + std::to_string(layMem.checkpointCount) + ")";
        info += "\n  Pages/Boxes: " + formatBytes(layMem.boxBytes);
        info += "\n  Subtotal:    " + formatUsedAlloc(layUsedTotal, layTotal);

        info += "\n\nTotal:         " + formatUsedAlloc(docUsedTotal + layUsedTotal, docTotal + layTotal);
    }

    wsdialogs::MessageBox(mHost, "WordTsar Status", info);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Request application quit.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Quit(void)
{
    // ^KX: save and exit -- prompt to save unsaved changes first
    // (only quit when CloseEvent is not cancelled).
    if (CloseEvent() == true)
    {
        mQuit = true;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Abandon the current file (^KQ): the base confirms and clears the document,
/// then, if not cancelled, signal the application to return to the opening
/// menu.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::AbandonFile(void)
{
    cEditorBase::AbandonFile();

    if (mDocument && mDocument->mChanged)
    {
        return;
    }

    mReturnToOpening = true;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::function<void()> hook [in] the preferences dialog callback
///
/// @return nothing
///
/// @brief
/// Install the application callback that shows the screen-settings dialog.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetPreferencesHook(std::function<void()> hook)
{
    mPreferencesHook = hook;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the screen-settings preferences dialog (^OB and related ^O keys),
/// delegating to the application which owns the chrome.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Preferences(void)
{
    if (mPreferencesHook)
    {
        mPreferencesHook();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  enabled [in] enable/disable flag (unused)
///
/// @return nothing
///
/// @brief
/// Enable/disable editor input. The wordstartui loop always routes input, so
/// this is a no-op retained for the cEditorBase contract.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SetEnabled(bool /*enabled*/)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return int - detected clipboard tool code
///
/// @brief
/// Detect an available system clipboard tool once and cache the result in
/// mClipboardTool. Codes: 0 undetected, 1 none, 2 wl-copy/wl-paste,
/// 3 xclip, 4 xsel, 5 pbcopy/pbpaste. Called lazily by copy/paste.
///
/////////////////////////////////////////////////////////////////////////////
static int DetectClipboardTool(void)
{
#ifdef __APPLE__
    return 5;
#else
    if (system("which wl-copy > /dev/null 2>&1") == 0)
    {
        return 2;
    }
    if (system("which xclip > /dev/null 2>&1") == 0)
    {
        return 3;
    }
    if (system("which xsel > /dev/null 2>&1") == 0)
    {
        return 4;
    }
    return 1;
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Copy the selected block to the system clipboard using an external tool
/// (wl-copy, xclip, xsel, or pbcopy).
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::ClipboardCopy(void)
{
    if (!mDocument)
    {
        return;
    }

    if (mClipboardTool == 0)
    {
        mClipboardTool = DetectClipboardTool();
    }

    POSITION_T start = 0;
    POSITION_T end = 0;

    if (mDocument->GetBlock(start, end))
    {
        std::string str = mDocument->GetBlockText(start, end);

        const char* cmd = nullptr;
        switch (mClipboardTool)
        {
            case 2: { cmd = "wl-copy 2>/dev/null"; break; }
            case 3: { cmd = "xclip -selection clipboard 2>/dev/null"; break; }
            case 4: { cmd = "xsel --clipboard --input 2>/dev/null"; break; }
            case 5: { cmd = "pbcopy 2>/dev/null"; break; }
            default: { return; }
        }

        FILE* pipe = popen(cmd, "w");
        if (pipe)
        {
            fwrite(str.c_str(), 1, str.size(), pipe);
            pclose(pipe);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Paste text from the system clipboard at the current cursor position
/// using an external tool (wl-paste, xclip, xsel, or pbpaste).
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::ClipboardPaste(void)
{
    if (!mDocument)
    {
        return;
    }

    if (mClipboardTool == 0)
    {
        mClipboardTool = DetectClipboardTool();
    }

    const char* cmd = nullptr;
    switch (mClipboardTool)
    {
        case 2: { cmd = "wl-paste 2>/dev/null"; break; }
        case 3: { cmd = "xclip -selection clipboard -o 2>/dev/null"; break; }
        case 4: { cmd = "xsel --clipboard --output 2>/dev/null"; break; }
        case 5: { cmd = "pbpaste 2>/dev/null"; break; }
        default:
        {
            ShowError("Error", "No clipboard tool available.");
            return;
        }
    }

    FILE* pipe = popen(cmd, "r");
    if (pipe)
    {
        std::string text;
        char buf[4096];
        while (fgets(buf, sizeof(buf), pipe))
        {
            text += buf;
        }
        pclose(pipe);

        if (!text.empty())
        {
            InsertText(text);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the Find dialog, store the collected search text and options, then
/// repeat the search via FindAgain().
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Find(void)
{
    if (!mHost || !mDocument)
    {
        return;
    }

    // Save position before starting search (for GotoLastFindandReplace)
    mLastFindandReplace = mDocument->GetPosition();

    wsdialogs::sFindOptions initial;
    initial.text = mSearchText;
    initial.wholeWord = mWholeWord;
    initial.ignoreCase = mCaseCmp;
    initial.backward = mSearchBackwards;
    initial.wildcard = mWildCard;
    initial.scope = mWholeFile ? 1 : 0;

    wsdialogs::sFindOptions result = wsdialogs::FindDialog(mHost, initial);
    if (!result.ok)
    {
        return;
    }

    mSearchText = result.text;
    mWholeWord = result.wholeWord;
    mCaseCmp = result.ignoreCase;
    mSearchBackwards = result.backward;
    mWildCard = result.wildcard;
    mWholeFile = (result.scope == 1);

    FindAgain();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Repeat the last search using the saved search parameters. Searches
/// forward or backward from the current position and updates the search
/// block highlight when a match is found.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::FindAgain(void)
{
    if (!mDocument)
    {
        return;
    }

    POSITION_T fpos = NOT_SET;

    if (mSearchBackwards == false)
    {
        fpos = mDocument->FindNext(mSearchText, mDocument->GetPosition() + 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == mDocument->GetTextSize())
        {
            ShowMessage("Not found", "Search word: " + mSearchText + "  not found.");
        }
        else
        {
            fpos = SkipOverHiddenContent(fpos, +1);
            mDocument->SetPosition(fpos);
            mStartSearchBlock = fpos;

            std::vector<POSITION_T> matchOffsets;
            POSITION_T searchGraphemes = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mEndSearchBlock = fpos + searchGraphemes;
            mSearchBlockSet = true;

            CalculateCaretPosition();
            ScrollIntoView();
            Repaint();
        }
    }
    else
    {
        fpos = mDocument->FindPrev(mSearchText, mDocument->GetPosition() - 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == NOT_SET)
        {
            ShowMessage("Not found", "Search word: " + mSearchText + "  not found.");
        }
        else
        {
            fpos = SkipOverHiddenContent(fpos, -1);
            mDocument->SetPosition(fpos);
            mStartSearchBlock = fpos;

            std::vector<POSITION_T> matchOffsets;
            POSITION_T searchGraphemes = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mEndSearchBlock = fpos + searchGraphemes;
            mSearchBlockSet = true;

            CalculateCaretPosition();
            ScrollIntoView();
            Repaint();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  noquery [in] if true, skip the confirmation prompt (batch mode)
///
/// @return bool - true if no more matches (end of document), false otherwise
///
/// @brief
/// Perform a single find-and-replace from the current position. Searches for
/// mSearchText, asks for confirmation when mReplaceAsk is set and not in
/// batch mode, then deletes the match and inserts mReplaceText.
///
/////////////////////////////////////////////////////////////////////////////
bool cWSEditorCtrl::ReplaceAgain(bool noquery)
{
    if (!mDocument)
    {
        return true;
    }

    bool retval = false;
    POSITION_T fpos = NOT_SET;

    if (mSearchBackwards == false)
    {
        if (mWholeFile)
        {
            mDocument->SetPosition(0);
        }

        fpos = mDocument->FindNext(mSearchText, mDocument->GetPosition() + 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == mDocument->GetTextSize())
        {
            if (mReplaceAsk == true)
            {
                ShowMessage("Not Found", "At end of document.");
            }
            retval = true;
        }
        else
        {
            mDocument->SetPosition(fpos);

            std::vector<POSITION_T> matchOffsets;
            POSITION_T graphemeCount = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mReplaceSize = graphemeCount;

            mStartSearchBlock = fpos;
            mEndSearchBlock = fpos + graphemeCount;
            mSearchBlockSet = true;

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
                Repaint();
            }

            bool rep = true;
            if (mReplaceAsk == true && !noquery)
            {
                rep = AskYesNo("Replace", "Replace?");
            }

            if (rep == true)
            {
                BeginBatchUpdate();
                Delete(mDocument->GetPosition(), mReplaceSize);
                mDocument->Insert(mReplaceText.c_str());
                EndBatchUpdate();
            }
        }
    }
    else
    {
        if (mWholeFile)
        {
            mDocument->SetPosition(mDocument->GetTextSize() - 2);
        }

        fpos = mDocument->FindPrev(mSearchText, mDocument->GetPosition() - 1, mWildCard, mCaseCmp, mWholeWord);

        if (fpos == NOT_SET)
        {
            if (mReplaceAsk == true)
            {
                ShowMessage("Not Found", "At start of document.");
            }
            retval = true;
        }
        else
        {
            mDocument->SetPosition(fpos);

            std::vector<POSITION_T> matchOffsets;
            POSITION_T graphemeCount = static_cast<POSITION_T>(mDocument->GraphemeCount(mSearchText, matchOffsets));
            mReplaceSize = graphemeCount;

            mStartSearchBlock = fpos;
            mEndSearchBlock = fpos + graphemeCount;
            mSearchBlockSet = true;

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
                Repaint();
            }

            bool rep = true;
            if (mReplaceAsk == true && !noquery)
            {
                rep = AskYesNo("Replace", "Replace?");
            }

            if (rep == true)
            {
                BeginBatchUpdate();
                Delete(mDocument->GetPosition(), mReplaceSize);
                mDocument->Insert(mReplaceText.c_str());
                EndBatchUpdate();
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
/// Show the Find & Replace dialog, store the options, then perform a single
/// replacement or a counted batch replacement depending on the scope.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Replace(void)
{
    if (!mHost || !mDocument)
    {
        return;
    }

    mLastFindandReplace = mDocument->GetPosition();

    wsdialogs::sReplaceOptions initial;
    initial.find = mSearchText;
    initial.replace = mReplaceText;
    initial.scope = 0;
    initial.dontAsk = !mReplaceAsk;
    initial.wholeWord = mWholeWord;
    initial.ignoreCase = mCaseCmp;
    initial.backward = mSearchBackwards;
    initial.wildcard = mWildCard;

    wsdialogs::sReplaceOptions result = wsdialogs::ReplaceDialog(mHost, initial);
    if (!result.ok)
    {
        return;
    }

    mSearchText = result.find;
    mReplaceText = result.replace;
    mWholeWord = result.wholeWord;
    mCaseCmp = result.ignoreCase;
    mSearchBackwards = result.backward;
    mWildCard = result.wildcard;
    mReplaceAsk = !result.dontAsk;
    mWholeFile = (result.scope == 1);
    mReplaceScope = result.scope;

    mReplaceSize = static_cast<POSITION_T>(mSearchText.length());

    // mReplaceScope: 0=next, 1=entire file, 2=rest of file
    if (mReplaceScope == 0)
    {
        ReplaceAgain();
    }
    else
    {
        if (mReplaceScope == 1)
        {
            if (mSearchBackwards)
            {
                mDocument->SetPosition(mDocument->GetTextSize() - 1);
            }
            else
            {
                mDocument->SetPosition(0);
            }
        }

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

        ShowMessage("Replace", "Replaced " + std::to_string(count) + " items");
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to the next occurrence of a user-specified character (forward).
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::GotoCharacter(void)
{
    if (!mHost || !mDocument)
    {
        return;
    }

    std::string character;
    if (!wsdialogs::InputBox(mHost, "Go To Character", "Character", character))
    {
        return;
    }

    if (!character.empty())
    {
        mWholeFile = false;
        mCaseCmp = false;
        mSearchBackwards = false;
        mWildCard = false;
        mWholeWord = false;

        mSearchText = character;

        POSITION_T oldPos = mDocument->GetPosition();
        FindAgain();

        if (mDocument->GetPosition() == oldPos)
        {
            POSITION_T safePos = SkipOverHiddenContent(mDocument->GetTextSize(), -1);
            mDocument->SetPosition(safePos);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to the previous occurrence of a user-specified character (backward).
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::GotoCharacterBackward(void)
{
    if (!mHost || !mDocument)
    {
        return;
    }

    std::string character;
    if (!wsdialogs::InputBox(mHost, "Go To Character Backward", "Character", character))
    {
        return;
    }

    if (!character.empty())
    {
        mWholeFile = false;
        mCaseCmp = false;
        mSearchBackwards = true;
        mWildCard = false;
        mWholeWord = false;

        mSearchText = character;

        POSITION_T oldPos = mDocument->GetPosition();
        FindAgain();

        if (mDocument->GetPosition() == oldPos)
        {
            POSITION_T safePos = SkipOverHiddenContent(0, +1);
            mDocument->SetPosition(safePos);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to the first line of a user-specified page number.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::GotoPage(void)
{
    if (!mHost || !mDocument || !mLayout)
    {
        return;
    }

    std::string pageStr;
    if (!wsdialogs::InputBox(mHost, "Go To Page", "Page number", pageStr))
    {
        return;
    }

    if (pageStr.empty())
    {
        return;
    }

    PAGE_T page = 0;
    try
    {
        page = std::stol(pageStr);
    }
    catch (...)
    {
        return;
    }

    if (page > 0 && page <= mLayout->GetNumberOfPages())
    {
        PARAGRAPH_T para = mLayout->GetFirstParagraphOnPage(page);
        if (para < mLayout->GetNumberOfParagraphs())
        {
            const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para);
            if (paraLayout)
            {
                for (const auto& line : paraLayout->lines)
                {
                    if (line.pagenumber == page)
                    {
                        mDocument->SetPosition(line.documentPosition);
                        ScrollIntoView();
                        CalculateCaretPosition();
                        Repaint();
                        break;
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
///   4 - Pull-down menu bar shown; all prompts displayed. Looks the same
///       as 0/1 here since the TUI's menu bar is always shown regardless
///       of level -- no classic Edit Menu or submenus either way.
///   3 - Classic Edit Menu and submenus (^K/^Q/^O/^P/^M) both displayed.
///   2 - Classic Edit Menu hidden; submenus still displayed after a pause.
///   1 - No classic menus displayed.
///   0 - Same as 1 here (unlike the GUI, the TUI doesn't also hide the
///       status line at level 0).
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::ChangeHelpLevel(void)
{
    if (!mHost)
    {
        return;
    }

    std::string levelStr;
    if (!wsdialogs::InputBox(mHost, "Help Level", "What help level do you want? (0-4)", levelStr))
    {
        return;
    }

    int level = 0;
    try
    {
        level = std::stoi(levelStr);
    }
    catch (...)
    {
        return;
    }

    if (level < 0 || level > 4)
    {
        return;
    }

    mHelpLevel = level;
    mHelpDisplay = (level == 3) ? HELP_MAIN : HELP_NONE;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete text from the current position to the next occurrence of a
/// user-specified character.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::DeleteToChar(void)
{
    if (!mHost || !mDocument)
    {
        return;
    }

    std::string character;
    if (!wsdialogs::InputBox(mHost, "Delete To Character", "Character", character))
    {
        return;
    }

    if (!character.empty())
    {
        POSITION_T spos = mDocument->GetPosition();

        mWholeFile = false;
        mCaseCmp = false;
        mSearchBackwards = false;
        mWildCard = false;
        mWholeWord = false;

        mSearchText = character;
        FindAgain();

        POSITION_T epos = mDocument->GetPosition();

        if (spos != epos)
        {
            Delete(spos, epos - spos);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the font selection dialog. Lets the user pick a font family and point
/// size, then inserts the font into the document.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SelectFont(void)
{
    if (!mHost || !mLayout || !mDocument)
    {
        return;
    }

    std::string currentFont = GetFont();
    std::string fontFamily;
    std::string fontSize = "12.0";

    if (!currentFont.empty())
    {
        size_t pipe1 = currentFont.find('|');
        if (pipe1 != std::string::npos)
        {
            fontFamily = currentFont.substr(0, pipe1);
            size_t pipe2 = currentFont.find('|', pipe1 + 1);
            if (pipe2 != std::string::npos)
            {
                fontSize = currentFont.substr(pipe1 + 1, pipe2 - pipe1 - 1);
            }
        }
    }

    cLayout* tui = static_cast<cLayout*>(mLayout);
    if (!tui->IsFontEnumerationDone())
    {
        tui->WaitForFontEnumeration();
    }

    std::vector<std::string> families = tui->GetFontFamilies();

    std::string family = fontFamily;
    std::string size = fontSize;
    if (!wsdialogs::SelectFontDialog(mHost, families, fontFamily, fontSize, family, size))
    {
        return;
    }

    fontFamily = family;
    fontSize = size;

    double pointSize = 12.0;
    try
    {
        pointSize = std::stod(fontSize);
    }
    catch (...)
    {
        pointSize = 12.0;
    }
    if (pointSize < 1.0)
    {
        pointSize = 1.0;
    }
    if (pointSize > 144.0)
    {
        pointSize = 144.0;
    }

    sInternalFonts intfont;
    intfont.fontname = fontFamily;
    intfont.name = fontFamily;
    intfont.size = pointSize;
    intfont.haveWSFont = false;

    intfont.wsfont.width = 0;
    intfont.wsfont.height = static_cast<uint16_t>(pointSize) * 20;
    intfont.wsfont.style = 0x9FFF;
    intfont.wsfont.prevwidth = 0;
    intfont.wsfont.prevheight = 0;
    intfont.wsfont.prevstyle = 0;

    GetDocument()->InsertFont(intfont);
    LayoutDocument(false);
    PerformVisualUpdate();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the color selection dialog. Inserts the chosen RGB color (or the
/// default-color sentinel) into the document.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SelectColor(void)
{
    if (!mHost || !mDocument)
    {
        return;
    }

    wsdialogs::sColorResult result = wsdialogs::SelectColorDialog(mHost, 0, 0, 0);
    if (!result.ok)
    {
        return;
    }

    sSeqRGBColor color;

    if (result.useDefault)
    {
        color.red = -1;
        color.green = -1;
        color.blue = -1;
        color.alpha = -1;
    }
    else
    {
        int r = result.red;
        int g = result.green;
        int b = result.blue;
        if (r < 0) { r = 0; }
        if (r > 255) { r = 255; }
        if (g < 0) { g = 0; }
        if (g > 255) { g = 255; }
        if (b < 0) { b = 0; }
        if (b > 255) { b = 255; }

        color.red = static_cast<short>(r);
        color.green = static_cast<short>(g);
        color.blue = static_cast<short>(b);
        color.alpha = 255;
    }

    GetDocument()->InsertColor(color);
    LayoutDocument(false);
    PerformVisualUpdate();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Show the page-layout dialog and insert the resulting margin dot commands.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::PageLayout(void)
{
    if (mHost == nullptr)
    {
        return;
    }

    double divisor = TWIPSPERINCH;
    std::string unitSuffix = "\"";

    if (mMeasure == MSR_MILLIMETERS)
    {
        divisor = TWIPSPERMM;
        unitSuffix = "mm";
    }
    else if (mMeasure == MSR_CENTIMETERS)
    {
        divisor = TWIPSPERCM;
        unitSuffix = "cm";
    }

    auto fmt = [&](COORD_T twips) -> std::string
    {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", static_cast<double>(twips) / divisor);
        return std::string(buffer);
    };

    wsdialogs::sPageLayoutValues values;
    values.top = fmt(mLayout->GetTopMargin());
    values.bottom = fmt(mLayout->GetBottomMargin());
    values.left = fmt(mLayout->GetLeftMargin());
    values.right = fmt(mLayout->GetRightMargin());
    values.oddOffset = fmt(mLayout->GetPageOffsetOdd());
    values.evenOffset = fmt(mLayout->GetPageOffsetEven());
    values.headerMargin = fmt(mLayout->GetHeaderMargin());
    values.footerMargin = fmt(mLayout->GetFooterMargin());

    wsdialogs::sPageLayoutValues result = wsdialogs::PageLayoutDialog(mHost, values, unitSuffix);
    if (result.ok == false)
    {
        return;
    }

    // Insert the dot commands at the start of the current paragraph.
    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(GetCaretDocumentPosition());
    POSITION_T paraStart = 0;
    POSITION_T paraEnd = 0;
    mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);
    mDocument->SetPosition(paraStart);

    BeginBatchUpdate();

    auto insertDot = [&](const std::string& command, const std::string& value)
    {
        std::string line = command + " " + value + unitSuffix + "\r";
        mDocument->Insert(line);
    };

    insertDot(".mt", result.top);
    insertDot(".mb", result.bottom);
    insertDot(".lm", result.left);
    insertDot(".rm", result.right);
    insertDot(".poo", result.oddOffset);
    insertDot(".poe", result.evenOffset);
    insertDot(".hm", result.headerMargin);
    insertDot(".fm", result.footerMargin);

    EndBatchUpdate();

    LayoutDocument(true);
    PerformVisualUpdate();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Paginate the laid-out document into text and show it in a scrollable viewer.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::PrintPreview(void)
{
    // Generate a PDF via Quartz/Core Text and open it in the system viewer.
    // The printout is backend-neutral (cEditorBase + system frameworks).
    cTUIPrintout printout(this);
    printout.PrintPreview();
}


#ifndef _WIN32
/////////////////////////////////////////////////////////////////////////////
///
/// @return CUPS default destination name, or empty if none is configured
///
/// @brief
/// Runs `lpstat -d` and parses its output for a configured default
/// destination. CUPS reports "no system default destination" when none
/// has been set, which is common on a fresh macOS install even when
/// printers are available.
///
/////////////////////////////////////////////////////////////////////////////
static std::string GetCupsDefaultPrinter(void)
{
    FILE* pipe = popen("lpstat -d 2>/dev/null", "r");
    if (!pipe)
    {
        return std::string();
    }

    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe))
    {
        output += buf;
    }
    pclose(pipe);

    const std::string marker = "system default destination:";
    size_t pos = output.find(marker);
    if (pos == std::string::npos)
    {
        return std::string();
    }

    std::string name = output.substr(pos + marker.size());
    size_t start = name.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        return std::string();
    }
    size_t end = name.find_last_not_of(" \t\r\n");
    return name.substr(start, end - start + 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return names of installed CUPS destinations (possibly empty)
///
/// @brief
/// Runs `lpstat -p` and parses each "printer NAME is ..." line for the
/// destination name.
///
/////////////////////////////////////////////////////////////////////////////
static std::vector<std::string> GetCupsPrinters(void)
{
    std::vector<std::string> printers;

    FILE* pipe = popen("lpstat -p 2>/dev/null", "r");
    if (!pipe)
    {
        return printers;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), pipe))
    {
        std::string line(buf);
        if (line.rfind("printer ", 0) == 0)
        {
            size_t nameStart = 8;
            size_t nameEnd = line.find(' ', nameStart);
            if (nameEnd != std::string::npos)
            {
                printers.push_back(line.substr(nameStart, nameEnd - nameStart));
            }
        }
    }
    pclose(pipe);

    return printers;
}
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Print the document by rendering it to a PDF (Quartz/Core Text) and handing the file
/// to the system print spooler. On non-Windows platforms this targets a CUPS
/// destination: the configured default if there is one, the sole installed
/// printer if there's exactly one, or a user pick from a list otherwise
/// (mirroring the "Name of printer?" prompt in real WordStar's print flow).
/// On failure the PDF path is reported so the user can print it manually.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::Print(void)
{
    cTUIPrintout printout(this);

    std::string stem = mFileName.empty() ? std::string("untitled") : mFileName;
    std::filesystem::path path = std::filesystem::temp_directory_path() /
                                 ("WordTsar-" + stem + "-print.pdf");

    if (printout.GeneratePDF(path.string()) == false)
    {
        ShowError("Print", "Could not generate the document for printing.");
        return;
    }

#ifdef _WIN32
    HINSTANCE result = ShellExecuteA(nullptr, "print", path.string().c_str(),
                                      nullptr, nullptr, SW_HIDE);
    if (reinterpret_cast<INT_PTR>(result) > 32)
    {
        SetStatusMessage("Sent to printer", false, 90);
    }
    else
    {
        ShowMessage("Print", "Saved to:\n" + path.string() +
                             "\n\nOpen it in your PDF viewer to print.");
    }
#else
    std::string printerName = GetCupsDefaultPrinter();

    if (printerName.empty())
    {
        std::vector<std::string> printers = GetCupsPrinters();
        if (printers.size() == 1)
        {
            printerName = printers.front();
        }
        else if (printers.size() > 1)
        {
            if (!wsdialogs::SelectPrinterDialog(mHost, printers, printerName))
            {
                ShowMessage("Print", "Saved to:\n" + path.string() +
                                     "\n\nOpen it in your PDF viewer to print.");
                return;
            }
        }
    }

    if (printerName.empty())
    {
        ShowError("Print", "No printer is configured.\n"
                           "The PDF was written to:\n" + path.string());
        return;
    }

    std::string command = "lp -d \"" + printerName + "\" \"" + path.string() + "\" >/dev/null 2>&1";
    int result = std::system(command.c_str());
    if (result == 0)
    {
        SetStatusMessage("Sent to " + printerName, false, 90);
    }
    else
    {
        ShowError("Print", "Could not send the document to \"" + printerName + "\".\n"
                           "The PDF was written to:\n" + path.string());
    }
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Spell check the whole document, prompting to correct each misspelled word.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SpellCheckDocument(void)
{
    if (mHost == nullptr)
    {
        return;
    }

    cSpellChecker checker(mSpellCheckLanguage);
    std::set<std::string> ignored;
    PARAGRAPH_T total = mDocument->GetNumberofParagraphs();
    int misspelled = 0;

    for (PARAGRAPH_T para = 0; para < total; ++para)
    {
        bool rescan = true;

        while (rescan == true)
        {
            rescan = false;

            std::vector<std::string> graphemes;
            std::vector<POSITION_T> offsets;
            mDocument->GetParagraphGraphemes(para, graphemes, offsets);

            POSITION_T paraStart = 0;
            POSITION_T paraEnd = 0;
            mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);

            std::string word;
            size_t wordStart = 0;

            for (size_t index = 0; index <= graphemes.size(); ++index)
            {
                bool isWordChar = false;

                if (index < graphemes.size())
                {
                    const std::string& grapheme = graphemes[index];
                    if (grapheme.size() == 1)
                    {
                        unsigned char code = static_cast<unsigned char>(grapheme[0]);
                        if ((std::isalpha(code) != 0) || (code == '\''))
                        {
                            isWordChar = true;
                        }
                    }
                }

                if (isWordChar == true)
                {
                    if (word.empty() == true)
                    {
                        wordStart = index;
                    }
                    word += graphemes[index];
                    continue;
                }

                if ((word.size() > 1) && (ignored.count(word) == 0) && (checker.CheckWord(word) == false))
                {
                    misspelled++;
                    std::vector<std::string> suggestions = checker.suggestions(word);
                    int selected = 0;
                    int action = wsdialogs::SpellCheckDialog(mHost, word, suggestions, selected);

                    if (action == -1)
                    {
                        return;
                    }
                    else if (action == 0)
                    {
                        ignored.insert(word);
                    }
                    else if (action == 1)
                    {
                        if ((selected >= 0) && (selected < static_cast<int>(suggestions.size())))
                        {
                            POSITION_T pos = paraStart + static_cast<POSITION_T>(wordStart);
                            mDocument->SetPosition(pos);
                            Delete(pos, static_cast<POSITION_T>(word.size()));
                            InsertText(suggestions[static_cast<size_t>(selected)]);
                            LayoutDocument(false);
                            rescan = true;
                            break;
                        }
                    }
                    else if (action == 2)
                    {
                        checker.AddWord(word);
                        ignored.insert(word);
                    }
                }

                word.clear();
            }
        }
    }

    ShowMessage("Spell Check", "Spell check complete. " + std::to_string(misspelled) + " misspelled word(s) found.");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Spell check the word at the cursor and report the result with suggestions.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SpellCheckWord(void)
{
    if (mHost == nullptr)
    {
        return;
    }

    cSpellChecker checker(mSpellCheckLanguage);
    POSITION_T pos = GetCaretDocumentPosition();
    POSITION_T wordStart = mDocument->GetPrevWordPosition(pos + 1);
    POSITION_T wordEnd = mDocument->GetNextWordPosition(pos);

    std::string word;
    if (wordEnd > wordStart)
    {
        word = mDocument->GetBlockText(wordStart, wordEnd);
    }

    // Trim trailing non-word characters that GetBlockText may include.
    while (word.empty() == false)
    {
        unsigned char code = static_cast<unsigned char>(word.back());
        if ((std::isalpha(code) == 0) && (code != '\''))
        {
            word.pop_back();
        }
        else
        {
            break;
        }
    }

    if (word.empty() == true)
    {
        ShowMessage("Spell Check", "No word at the cursor.");
        return;
    }

    if (checker.CheckWord(word) == true)
    {
        ShowMessage("Spell Check", "\"" + word + "\" is spelled correctly.");
        return;
    }

    std::vector<std::string> suggestions = checker.suggestions(word);
    std::string message = "\"" + word + "\" is misspelled.";

    if (suggestions.empty() == false)
    {
        message += "\nSuggestions:";
        for (size_t index = 0; (index < suggestions.size()) && (index < 10); ++index)
        {
            message += "\n  " + suggestions[index];
        }
    }

    ShowMessage("Spell Check", message);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Prompt for a word and report whether it is spelled correctly.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::SpellCheckEnterWord(void)
{
    if (mHost == nullptr)
    {
        return;
    }

    std::string word;
    if (wsdialogs::InputBox(mHost, "Spell Check", "Enter a word to check:", word) == false)
    {
        return;
    }

    if (word.empty() == true)
    {
        return;
    }

    cSpellChecker checker(mSpellCheckLanguage);

    if (checker.CheckWord(word) == true)
    {
        ShowMessage("Spell Check", "\"" + word + "\" is spelled correctly.");
        return;
    }

    std::vector<std::string> suggestions = checker.suggestions(word);
    std::string message = "\"" + word + "\" is misspelled.";

    if (suggestions.empty() == false)
    {
        message += "\nSuggestions:";
        for (size_t index = 0; (index < suggestions.size()) && (index < 10); ++index)
        {
            message += "\n  " + suggestions[index];
        }
    }

    ShowMessage("Spell Check", message);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Count words in the marked block (or the whole document) and show the total.
///
/////////////////////////////////////////////////////////////////////////////
void cWSEditorCtrl::WordCountBlock(void)
{
    if (mHost == nullptr)
    {
        return;
    }

    long words = 0;
    POSITION_T start = 0;
    POSITION_T end = 0;

    if (mDocument->GetBlock(start, end) == true)
    {
        words = WordCount(start, end);
    }
    else
    {
        words = WordCount(0, 0);
    }

    mLastWordCount = words;

    ShowMessage("Word Count", "Word Count: " + std::to_string(words));
}
