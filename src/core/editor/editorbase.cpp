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
 * @class cEditorBase
 *
 * @brief Platform-independent base editor providing caret, navigation, and text manipulation.
 *
 * Implements the cEditorBase class, which is the shared foundation for both the
 * GUI (Qt) and TUI editor controls. Manages caret positioning and
 * movement (character, word, line, page, document start/end), scroll offset
 * tracking, viewport calculation for both continuous and page display modes,
 * text insertion with variable replacement and CJK punctuation handling,
 * deletion (character, word, line, block), block selection operations (copy,
 * move, delete, case transforms), find/replace state, undo grouping for typing
 * sequences, display mode toggling, sibling editor synchronization for reveal
 * codes, status bar updates, file state persistence, and INI-based configuration
 * loading. Communicates with cDocument for data and cLayoutBase for positioning.
 *
 * @section editorbase_navigation Caret and Navigation
 * - Character-level: left, right, with grapheme-aware skipping of hidden codes
 * - Word-level: forward/backward word boundaries with Unicode-aware word break
 * - Line-level: up, down, beginning/end of line, with column memory
 * - Page-level: page up/down with viewport-relative scrolling
 * - Document-level: beginning/end of document, goto position/page
 *
 * @section editorbase_editing Text Editing
 * - Insertion: single characters and strings with auto-wrap, variable replacement
 *   for date/time/page tokens, CJK punctuation width handling
 * - Deletion: character (forward/backspace), word, line, to end-of-line
 * - Block operations: copy, move, delete, uppercase, lowercase, word count
 * - Undo grouping: typing sequences coalesced into single undo groups,
 *   explicit group boundaries for compound operations
 *
 * @section editorbase_display Display and Viewport
 * - Display modes: continuous scrolling and page-view with stacked pages
 * - Scroll offset tracking with page-mode coordinate conversion
 * - Control code visibility: SHOW_NONE, SHOW_DOT, SHOW_ALL modes
 * - Sibling editor synchronization for reveal codes split view
 *
 * @section editorbase_state State Management
 * - Find/replace: search text, replacement text, direction, whole-word, wildcard
 * - File state: filename, dirty flag, auto-save, backup file management
 * - Configuration: INI-based loading of preferences (caret blink rate,
 *   auto-save interval, default format, spell check language)
 * - Status bar: line, column, page, insert/overwrite, formatting indicators
 *
 * @section editorbase_virtuals Pure Virtual Interface
 * Derived classes (GUI cEditorCtrl, TUI cEditorCtrl) must implement:
 * ShowError(), ShowMessage(), AskYesNo(), Quit(), SetEnabled(),
 * PerformVisualUpdate(), and other platform-specific operations.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cDocumentListener Document change notification interface
 * @see cDocument Document model for text storage
 * @see cLayoutBase Layout engine for position/coordinate mapping
 * @see sStatus Status bar data structure
 */

#include "editorbase.h"
#include "SimpleIni.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <random>

/////////////////////////////////////////////////////////////////////////////
///
/// @return the user's Documents folder if it can be determined, else "./"
///
/// @brief
/// Default starting directory for file dialogs when no explicit
/// "Default Directory" preference has been set. Without this, mFileDir
/// stays a relative "./" and silently resolves against whatever cwd the
/// OS happened to launch the process with -- on macOS that's the user's
/// home directory, not a place anyone expects to see file dialogs open to.
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorBase::DefaultFileDir(void)
{
    const char* home = std::getenv("HOME");
    if (home != nullptr && home[0] != '\0')
    {
        return std::string(home) + "/Documents";
    }

    return "./";
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor. Initializes editor base with defaults.
///
/////////////////////////////////////////////////////////////////////////////
cEditorBase::cEditorBase(void)
    : mFileName("Unknown.ws")
    , mInsertMode(true)
    , mLastWordCount(0)
    , mShortName("")
    , mLongName("")
    , mAlwaysDot(true)
    , mHelpDisplay(HELP_NONE)
    , mHelpLevel(3)                  // WS4 default: all menus and prompts displayed
    , mRevealCodesVisible(false)
    , mSearchText("")
    , mReplaceText("")
    , mCaseCmp(false)
    , mSearchBackwards(false)
    , mWildCard(false)
    , mWholeWord(false)
    , mWholeFile(false)
    , mLastFindandReplace(0)
    , mStartSearchBlock(0)
    , mEndSearchBlock(0)
    , mSearchBlockSet(false)
    , mReplaceSize(0)
    , mReplaceAsk(true)
    , mReplaceScope(0)
    , mLastKeyUpOrDown(false)
    , mFileDir(cEditorBase::DefaultFileDir())
    , mFileSet(false)
    , mCaretBlinkRate(500)
    , mAutoSaveIntervalSec(60)
    , mDefaultFormat("ws")
    , mSpellCheckLanguage("en_US")
    , mSpellCheckDotCommands(false)
    , mLayout(nullptr)
    , mDocument(nullptr)
    , mOwnsDocument(true)
    , mDocumentDirty(false)
    , mDirtyFromParagraph(0)
    , mBatchUpdateCount(0)
    , mListenerHandledUpdate(false)
    , mSiblingEditor(nullptr)
    , mSyncingCaret(false)
    , mSavedShowControl(SHOW_DOT)
    , mScrollOffset(0)
    , mCaretLinePageX(0)
    , mShowViewportDebug(false)
    , mShowBoxStats(false)
    , mShowBoxOutlines(false)
    , mCaretX(0)
    , mCaretPrintX(0)
    , mCaretY(0)
    , mCaretWidth(30)
    , mCaretHeight(300)
    , mCaretDocumentPosition(0)
    , mCaretLine(0)
    , mCaretParagraph(0)
    , mCaretStickyX(-1)
    , mCaretPageNumber(1)
    , mCaretPageY(0)
    , mCaretLineDocPosition(0)
    , mCaretOnPrintableLine(true)
    , mLastPrintablePageY(0)
    , mCaretPageLineNumber(0)
    , mDrawnCaret(false)
    , mDoDrawCaret(false)
    , mTypingGroupActive(false)
    , mLayoutInt(false)
    , mLayoutRest(false)
    , mLayoutParagraph(0)
    , mVisibleStart(0)
    , mVisibleEnd(0)
    , mPageFirstVisibleLine(0)
    , mPageLastVisibleLine(0)
    , mPageFirstVisibleLineScrollY(0)
    , mPageLastVisibleLineScrollY(0)
    , mBaseFont("")
    , mShowDot(false)
    , mPageModeSupported(true)
    , mMeasurement("0i")
    , mMeasure(MSR_INCHES)
    , mCodePage(CP437)
    , mStatusFont("")
    , mStatusBold(false)
    , mStatusItalic(false)
    , mStatusUnderline(false)
    , mStatusJust(JUST_LEFT)
    , mStatusMessage("")
    , mStatusMessageTimer(0)
    , mStatusBusy(false)
    , mWordCountTimerRunning(false)
{
    // sDisplaySettings constructor already sets defaults:
    // - mode = DISPLAY_CONTINUOUS
    // - showControl = SHOW_DOT
    // - pageGap = 360 twips
    // - showPageShadows = true
    // - backgroundColor = gray (128,128,128)

    // Generate unique ID for temp-dir backup of untitled documents.
    // When LoadFile/SaveFile runs, mBackupFileName is overwritten with
    // the real file's -bak.ws path, so this only affects new unsaved docs.
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dis(0, 0xFFFFFFFF);
    uint32_t r1 = dis(gen);
    uint32_t r2 = dis(gen);
    char hex[17];
    snprintf(hex, sizeof(hex), "%08x%08x", r1, r2);
    mBackupUuid = hex;

    std::filesystem::path tempPath = std::filesystem::temp_directory_path() /
        ("WordTsar-" + mBackupUuid + "-bak.ws");
    mBackupFileName = tempPath.string();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor. Stops word count timer thread. cEditorBase does not own
/// layout or document; subclasses may own additional resources and must
/// clean up in their own destructors.
///
/////////////////////////////////////////////////////////////////////////////
cEditorBase::~cEditorBase(void)
{
    // Stop word count timer thread if running
    StopWordCountTimer();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T fromParagraph [in] first paragraph affected
///
/// @return nothing
///
/// @brief
/// Called by cDocument when content changes. Sets dirty flag so the
/// idle layout timer picks up the change on the next tick.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::OnDocumentChanged(PARAGRAPH_T fromParagraph)
{
    if (mDocumentDirty)
    {
        // already dirty -- track earliest affected paragraph
        if (fromParagraph < mDirtyFromParagraph)
        {
            mDirtyFromParagraph = fromParagraph ;
        }
    }
    else
    {
        mDocumentDirty = true ;
        mDirtyFromParagraph = fromParagraph ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Called by cDocument when Clear() is called. Sets dirty flag so the
/// idle layout timer does a full relayout.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::OnDocumentCleared(void)
{
    mDocumentDirty = true ;
    mDirtyFromParagraph = 0 ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Abandon the current file (^KQ). Prompts to save if the document has
/// unsaved changes. Clears the document and resets the filename to
/// "Unknown.ws". Subclasses can override to change behavior (e.g. TUI
/// returns to the opening menu instead of staying in the editor).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::AbandonFile(void)
{
    if (mDocument && mDocument->mChanged)
    {
        if (!AskYesNo("Quit", "Quit without Saving?"))
        {
            return;
        }
    }

    if (mDocument)
    {
        mDocument->Clear();
        LayoutDocument(true);
        mDocument->mChanged = false;
    }

    mFileDir = DefaultFileDir();
    mFileName = "Unknown.ws";
    mFileSet = false;
    SetTitle(mFileName);

    if (GetLayout())
    {
        GetLayout()->SetFilename(mFileName);
        GetLayout()->SetFileDir(mFileDir);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Post-command update shared by keyboard handlers and menu handlers.
/// Performs incremental layout of the visible paragraph range, invalidates
/// checkpoints, resets idle layout state, then calls PerformVisualUpdate()
/// for platform-specific caret/scroll/paint.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::PerformPostCommandUpdate(void)
{
    if (!mLayout || !mDocument)
    {
        return ;
    }

    // Incremental layout of visible range
    PARAGRAPH_T totalParas = mDocument->GetNumberofParagraphs() ;
    PARAGRAPH_T startPara = (mVisibleStart > 0) ? mVisibleStart - 1 : 0 ;
    PARAGRAPH_T endPara = GetLastVisibleParagraph() + 1 ;
    if (endPara >= totalParas)
    {
        endPara = totalParas - 1 ;
    }

    for (PARAGRAPH_T para = startPara ; para <= endPara ; ++para)
    {
        mLayout->LayoutParagraph(para) ;
    }

    // Invalidate checkpoints from edit point forward
    mLayout->InvalidateCheckpointsFrom(startPara) ;

    // Advance dirty tracking past visible range we just processed.
    // Without this, idle layout rewinds to mDirtyFromParagraph (within the
    // visible range), finds the layout already matches (because we just
    // updated it), and stops early -- never processing paragraphs beyond
    // the visible range.
    if (mDocumentDirty && mDirtyFromParagraph <= endPara)
    {
        mDirtyFromParagraph = endPara + 1 ;
    }

    // Start idle layout from after visible range
    mLayoutParagraph = endPara + 1 ;
    mLayoutInt = false ;
    mLayoutRest = false ;

    // Caret, scroll, paint (platform-specific)
    PerformVisualUpdate() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears the document dirty flag. Used after explicit LayoutDocument()
/// calls and in tests that set up document state before testing IdleLayout.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ResetDocumentDirty(void)
{
    mDocumentDirty = false ;
    mDirtyFromParagraph = 0 ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if document dirty flag is set
///
/// @brief
/// Returns whether the document has been modified since last layout.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::IsDocumentDirty(void) const
{
    return mDocumentDirty ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  mode [in] display mode to set
///
/// @return nothing
///
/// @brief
/// Sets the display mode (CONTINUOUS or PAGE) and triggers repaint.
///
/// Page mode always uses SHOW_NONE (like printing). When entering page mode,
/// saves current show control state, sets to SHOW_NONE, and relayouts.
/// When leaving page mode, restores saved state and relayouts.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetDisplayMode(eDisplayMode mode)
{
    if (mDisplaySettings.mode != mode)
    {
        // Virtual hook: allows GUI to auto-hide reveal codes pane when leaving page mode
        OnBeforeDisplayModeChange(mode) ;

        mDisplaySettings.mode = mode;

        // Page mode always uses SHOW_NONE (like print)
        if (mode == DISPLAY_PAGE)
        {
            // Entering page mode - save state and hide dot commands
            if (mLayout)
            {
                mSavedShowControl = mLayout->GetShowControl();
                mLayout->SetShowControl(SHOW_NONE);
                mLayout->SetActiveParagraph(-1);  // Clear so all dot commands are hidden
                if (mDocument)
                {
                    mLayout->LayoutDocument(mDocument);

                    // Move caret off dot command if needed
                    POSITION_T currentPos = mDocument->GetPosition();
                    POSITION_T safePos = SkipOverHiddenContent(currentPos, +1);
                    if (safePos != currentPos)
                    {
                        mDocument->SetPosition(safePos);
                    }
                }
            }
        }
        else
        {
            // Leaving page mode - restore saved state
            if (mLayout)
            {
                mLayout->SetShowControl(mSavedShowControl);
                if (mDocument)
                {
                    mLayout->LayoutDocument(mDocument);

                    // Move caret off dot command if needed (mirrors entering-page-mode logic)
                    POSITION_T currentPos = mDocument->GetPosition();
                    POSITION_T safePos = SkipOverHiddenContent(currentPos, +1);
                    if (safePos != currentPos)
                    {
                        mDocument->SetPosition(safePos);
                    }
                }
            }
        }

        // Virtual hook: allows GUI to update menu labels after mode change
        OnAfterDisplayModeChange(mode) ;

        Repaint();  // Platform-specific repaint
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current display mode
///
/// @brief
/// Returns the current display mode.
///
/////////////////////////////////////////////////////////////////////////////
eDisplayMode cEditorBase::GetDisplayMode(void) const
{
    return mDisplaySettings.mode;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggles between CONTINUOUS and PAGE modes.
/// Uses SetDisplayMode to ensure proper show control handling.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ToggleDisplayMode(void)
{
    if (mDisplaySettings.mode == DISPLAY_CONTINUOUS)
    {
        SetDisplayMode(DISPLAY_PAGE);
    }
    else
    {
        SetDisplayMode(DISPLAY_CONTINUOUS);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if page mode is supported
///
/// @brief
/// Check whether this editor supports page view mode.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::IsPageModeSupported(void) const
{
    return mPageModeSupported;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return document pointer
///
/// @brief
/// Returns the document pointer (editor owns the document).
///
/////////////////////////////////////////////////////////////////////////////
cDocument* cEditorBase::GetDocument(void) const
{
    return mDocument;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return layout engine pointer
///
/// @brief
/// Returns the layout engine pointer (editor owns the layout).
///
/////////////////////////////////////////////////////////////////////////////
cLayoutBase* cEditorBase::GetLayout(void) const
{
    return mLayout;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  cEditorBase* sibling [in] sibling editor for reveal codes sync
///
/// @return nothing
///
/// @brief
/// Sets the sibling editor pointer for bidirectional caret sync.
/// Pass nullptr to disconnect the sibling.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetSiblingEditor(cEditorBase* sibling)
{
    mSiblingEditor = sibling ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  offset [in] scroll offset in twips
///
/// @return nothing
///
/// @brief
/// Sets the scroll offset with clamping to valid document range.
/// Triggers platform-specific repaint.
///
/// Clamps offset to [0, maxScroll] where maxScroll = docHeight - viewportHeight.
/// This ensures scrolling stays within document bounds.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetScrollOffset(COORD_T offset)
{
    // Clamp offset to valid range
    COORD_T clampedOffset = offset;

    // Minimum is always 0
    if (clampedOffset < 0)
    {
        clampedOffset = 0;
    }

    // Maximum depends on document height and viewport height
    // Only clamp if layout has been performed (has boxes)
    // This allows scroll offset to be set before layout, which is needed for tests
    // and for restoring scroll position when loading files
    if (mLayout && mLayout->GetBoxCount() > 0)
    {
        COORD_T totalHeight = CalculateTotalDocumentHeight();
        COORD_T viewportHeight = GetViewportHeight();

        // In page mode, pageBorder consumes viewport space (painter.translate
        // offsets content by pageBorder). Match what ScrollIntoView does so
        // the max scroll allows reaching the true document bottom.
        if (mDisplaySettings.mode == DISPLAY_PAGE)
        {
            viewportHeight -= mDisplaySettings.pageBorder;
        }

        COORD_T maxScroll = totalHeight - viewportHeight;

        if (maxScroll < 0)
        {
            maxScroll = 0;  // Document fits in viewport
        }

        if (clampedOffset > maxScroll)
        {
            clampedOffset = maxScroll;
        }
    }

    // Only update if value changed
    if (mScrollOffset != clampedOffset)
    {
        mScrollOffset = clampedOffset;
        Repaint();  // Platform-specific repaint
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return scroll offset in twips
///
/// @brief
/// Returns the current scroll offset.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::GetScrollOffset(void) const
{
    return mScrollOffset;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return total document height in twips
///
/// @brief
/// Calculates total document height including display mode adjustments.
///
/// In CONTINUOUS mode: Uses raw layout height from screeny coordinates.
/// No gaps exist between pages - continuous vertical flow.
///
/// In PAGE mode: Calculates height including visual gaps between pages.
/// Formula: (pageCount x paperHeight) + ((pageCount - 1) x pageGap)
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::CalculateTotalDocumentHeight(void) const
{
    // Guard: need layout to calculate height
    if (!mLayout)
    {
        return 0;
    }

    if (mDisplaySettings.mode == DISPLAY_CONTINUOUS)
    {
        // CONTINUOUS MODE: Use raw layout height (screeny coordinates)
        return mLayout->GetTotalDocumentHeight();
    }
    else  // DISPLAY_PAGE
    {
        // PAGE MODE: Calculate height including page gaps
        PAGE_T pageCount = mLayout->GetNumberOfPages();
        if (pageCount == 0)
        {
            return 0;
        }

        COORD_T paperHeight = mLayout->GetPaperHeight();

        // Total height = pageCount pages + (pageCount-1) gaps + top and bottom borders
        return (pageCount * paperHeight) + ((pageCount - 1) * mDisplaySettings.pageGap) + 2 * mDisplaySettings.pageBorder;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Calculates which boxes are visible in the current viewport.
/// This is the CORE of box-based viewport management.
///
/// Uses Y-coordinate intersection to support multi-column layouts.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::CalculateViewport(void)
{
    // Guard: If no layout engine, nothing to calculate
    if (!mLayout)
    {
        return;
    }

    // Clear previous visible boxes
    mViewport.visibleBoxes.clear();

    // Calculate viewport height in twips
    mViewport.viewportHeight = GetViewportHeight();

    // Calculate viewport Y range in document screeny coordinates
    mViewport.scrollOffset = mScrollOffset;
    mViewport.topY = mScrollOffset;
    mViewport.bottomY = mScrollOffset + mViewport.viewportHeight;

    // Get all boxes from layout engine
    const std::vector<sBoxes>& allBoxes = mLayout->GetGlobalBoxList();

    // Binary search for the first box whose bottom edge reaches the viewport top.
    // Boxes are Y-sorted (pages laid out sequentially, stacked boxes ascending).
    // Non-decreasing order is sufficient -- duplicate Y values (future columns) are handled
    // correctly because hi = mid converges to the first matching element.
    size_t lo = 0 ;
    size_t hi = allBoxes.size() ;
    while (lo < hi)
    {
        size_t mid = lo + (hi - lo) / 2 ;
        COORD_T midBottom = allBoxes[mid].pageStartY + allBoxes[mid].bottom ;
        if (midBottom < mViewport.topY)
        {
            lo = mid + 1 ;
        }
        else
        {
            hi = mid ;
        }
    }

    // Linear scan from first candidate, stop when box top passes viewport bottom
    for (size_t i = lo ; i < allBoxes.size() ; ++i)
    {
        COORD_T boxContinuousTop = allBoxes[i].pageStartY + allBoxes[i].top ;
        if (boxContinuousTop > mViewport.bottomY)
        {
            break ;
        }
        mViewport.visibleBoxes.push_back(const_cast<sBoxes*>(&allBoxes[i])) ;
    }

    // Sort visible boxes for correct rendering order
    // Use page number and box.top for reliable sorting (screenYTop may be 0 for empty boxes)
    std::sort(mViewport.visibleBoxes.begin(), mViewport.visibleBoxes.end(),
              [](const sBoxes* a, const sBoxes* b)
              {
                  // Sort by page first, then by Y position on page, then X for multi-column layouts
                  if (a->pageNumber != b->pageNumber)
                  {
                      return a->pageNumber < b->pageNumber;
                  }
                  if (std::abs(a->top - b->top) < 1.0)
                  {
                      return a->left < b->left;
                  }
                  return a->top < b->top;
              });

}


/////////////////////////////////////////////////////////////////////////////
///
/// @return const sViewport& - Current viewport
///
/// @brief
/// Returns the current viewport for reading by other components.
///
/////////////////////////////////////////////////////////////////////////////
const sViewport& cEditorBase::GetViewport(void) const
{
    return mViewport;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Maps the document's current grapheme position to screen coordinates for
/// the caret (blinking cursor). This is the central method that bridges the
/// document model (grapheme positions) and the visual display (pixel/twip
/// coordinates). Called after every cursor movement, edit, and scroll.
///
/// The method proceeds through these phases:
///
/// Phase 1 -- Guards and setup
///   Reads mDocument->GetPosition() into mCaretDocumentPosition.
///   Returns early with safe defaults if layout is empty.
///
/// Phase 2 -- Paragraph lookup and dot command tracking
///   Finds the paragraph containing the caret. When the caret moves to a
///   new paragraph, calls SetActiveParagraph() and re-layouts the old
///   paragraph if it was a dot command (so it collapses back to hidden).
///
/// Phase 3 -- Hidden paragraph fallback
///   If the caret's paragraph has no visible lines (hidden dot command or
///   comment), searches forward then backward for the nearest visible
///   paragraph and uses its first/last line for caret coordinates.
///
/// Phase 4 -- Line search within paragraph
///   Calculates the grapheme offset within the paragraph and walks the
///   paragraph's lines to find which line contains the caret. Falls back
///   to the last line if the offset exceeds all line ranges.
///
/// Phase 5 -- Glyph walk for X position
///   If the line has segments, walks through each segment's graphemes
///   comparing document positions (segment.startPosition + i) against the
///   caret's offset. When found, reads the glyph's X from
///   segment.position[i] (base-0, relative to line) and adds line.pagex
///   to get absolute X. Also calculates glyph width for overwrite mode,
///   parses the font descriptor for the status bar, and fires
///   OnCaretSegmentResolved(). If the caret is past all glyphs, positions
///   at the end of the last segment.
///
/// Phase 6 -- Set caret member variables
///   Populates mCaretX, mCaretY, mCaretWidth, mCaretHeight, mCaretLine,
///   mCaretPageNumber, mCaretPageY, mCaretLineDocPosition,
///   mCaretOnPrintableLine, mCaretPageLineNumber. mCaretY is computed as
///   line.screeny - mScrollOffset (continuous coordinates).
///
/// Phase 7 -- Print X calculation
///   Computes mCaretPrintX by subtracting widths of non-printing control
///   code glyphs (formatting markers like bold/italic toggles) that appear
///   before the caret. Tabs and variables are NOT subtracted (they print).
///   This gives the "real" X position for printing/export.
///
/// Phase 8 -- Page mode and sibling sync
///   AdjustCaretYForPageMode() converts mCaretY from continuous screeny to
///   page-relative stacked coordinates. SyncSiblingCaret() updates the
///   reveal codes editor if active.
///
/// @note Coordinate systems:
///   - Document positions: grapheme count from document start (POSITION_T)
///   - Segment positions: base-0, relative to line.pagex (COORD_T)
///   - Screen Y: line.screeny is continuous; mCaretY subtracts mScrollOffset
///   - Page mode: AdjustCaretYForPageMode converts to stacked-page coords
///
/// @note Member variables set by this method:
///   mCaretDocumentPosition, mCaretX, mCaretY, mCaretWidth, mCaretHeight,
///   mCaretLine, mCaretParagraph, mCaretPageNumber, mCaretPageY,
///   mCaretLineDocPosition, mCaretOnPrintableLine, mCaretPageLineNumber,
///   mCaretPrintX, mCaretLinePageX
///
/// @see AdjustCaretYForPageMode() page mode coordinate conversion
/// @see SyncSiblingCaret() reveal codes editor synchronization
/// @see OnCaretSegmentResolved() subclass hook for segment/glyph data
/// @see ParseFontDescriptor() extracts font info for status bar display
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::CalculateCaretPosition(void)
{
    // === Phase 1: Guards and setup ===
    if (!mDocument || !mLayout)
    {
        return ;
    }

    sCaretCalcState state {} ;
    state.caretPos = mDocument->GetPosition() ;
    mCaretDocumentPosition = state.caretPos ;

    if (mLayout->GetNumberOfParagraphs() == 0)
    {
        // No content -- place caret at origin
        mCaretX = 0 ;
        mCaretY = 0 ;
        mCaretWidth = DEFAULT_CARET_WIDTH ;
        mCaretHeight = DEFAULT_CARET_HEIGHT ;
        return ;
    }

    // === Phase 2: Paragraph lookup and dot command tracking ===
    if (LookupCaretParagraph(state))
    {
        return ;
    }

    // === Phase 3: Hidden paragraph fallback ===
    if (HandleHiddenParagraph(state))
    {
        return ;
    }

    // === Phase 4: Line search within paragraph ===
    if (FindCaretLine(state))
    {
        return ;
    }

    // === Phase 5: Glyph walk for X position ===
    ResolveCaretGlyph(state) ;

    // === Phase 6: Set caret member variables ===
    mCaretX = state.caretX ;
    mCaretLinePageX = state.caretLine->pagex ;
    mCaretY = state.caretLine->screeny - mScrollOffset ;
    // In overwrite mode, caret covers the character; in insert mode, thin caret
    if (mInsertMode)
    {
        mCaretWidth = DEFAULT_CARET_WIDTH ;
    }
    else
    {
        mCaretWidth = state.caretGlyphWidth ;
    }
    // Use segment height (text height) instead of line height (which includes line spacing)
    if (state.caretSegmentHeight > 0)
    {
        mCaretHeight = state.caretSegmentHeight ;
    }
    else
    {
        mCaretHeight = mLayout->GetLineHeight() ;
    }

    // Remember line number and page data for status bar
    mCaretLine = state.caretLine->contentLineNumber ;
    mCaretPageNumber = state.caretLine->pagenumber ;
    mCaretPageY = state.caretLine->pagey ;
    mCaretLineDocPosition = state.caretLine->documentPosition ;
    mCaretOnPrintableLine = state.caretLine->isPrintable ;
    mCaretPageLineNumber = state.caretLine->pageLineNumber ;

    // === Phase 7: Print X calculation ===
    CalculatePrintX(state) ;

    // === Phase 8: Page mode and sibling sync ===
    AdjustCaretYForPageMode(state.caretLine) ;
    SyncSiblingCaret() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scrolls viewport to ensure caret is visible on screen.
/// Auto-scrolls up or down when caret moves outside viewport bounds.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ScrollIntoView(void)
{
    if (!mLayout)
    {
        return;
    }

    // Convert caret SCREEN Y coordinates to DOCUMENT Y coordinates
    COORD_T caretDocY = mCaretY + mScrollOffset;
    COORD_T caretDocBottom = caretDocY + mCaretHeight;

    // Calculate viewport bounds in DOCUMENT coordinates
    COORD_T viewportHeight = GetViewportHeight();

    // In page mode, painter.translate(pageBorder, pageBorder) consumes
    // viewport space for the top border, reducing effective visible area
    if (mDisplaySettings.mode == DISPLAY_PAGE)
    {
        viewportHeight -= mDisplaySettings.pageBorder;
    }

    COORD_T viewportTop = mScrollOffset;
    COORD_T viewportBottom = mScrollOffset + viewportHeight;

    // Check if caret is above viewport, scroll up
    if (caretDocY < viewportTop)
    {
        SetScrollOffset(caretDocY);
    }
    // Check if caret is below viewport, scroll down
    else if (caretDocBottom > viewportBottom)
    {
        COORD_T newOffset = caretDocBottom - viewportHeight;
        SetScrollOffset(newOffset);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves caret left by one grapheme (horizontal movement).
/// Resets sticky X since this is horizontal navigation.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretLeft(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Get current position
    POSITION_T currentPos = mDocument->GetPosition();
    if (currentPos == 0)
    {
        return;  // Already at start
    }

    // Move one grapheme left, skip over hidden content
    POSITION_T newPos = SkipOverHiddenContent(currentPos - 1, -1);

    // Reset sticky X (horizontal movement)
    mCaretStickyX = -1;

    // Set new position
    mDocument->SetPosition(newPos);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves caret right by one grapheme (horizontal movement).
/// Resets sticky X since this is horizontal navigation.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretRight(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Get current position and document size
    POSITION_T currentPos = mDocument->GetPosition();
    if (currentPos >= mDocument->GetTextSize())
    {
        return;  // Already at end
    }

    // Move one grapheme right, skip over hidden content
    POSITION_T newPos = SkipOverHiddenContent(currentPos + 1, +1);

    // Reset sticky X (horizontal movement)
    mCaretStickyX = -1;

    // Set new position
    mDocument->SetPosition(newPos);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves caret to the start of the previous word.
/// Uses document's GetPrevWordPosition() to find word boundaries.
/// Resets sticky X since this is horizontal navigation.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretWordLeft(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Get current position
    POSITION_T currentPos = mDocument->GetPosition();
    if (currentPos == 0)
    {
        return;  // Already at start
    }

    // Get previous word position
    POSITION_T newPos = mDocument->GetPrevWordPosition(currentPos);

    // Skip over hidden control codes and hidden paragraphs (SHOW_DOT/SHOW_NONE)
    newPos = SkipOverHiddenContent(newPos, -1);

    // Reset sticky X (horizontal movement)
    mCaretStickyX = -1;

    // Set new position
    mDocument->SetPosition(newPos);

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves caret to the start of the next word.
/// Uses document's GetNextWordPosition() to find word boundaries.
/// Resets sticky X since this is horizontal navigation.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretWordRight(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Get current position and document size
    POSITION_T currentPos = mDocument->GetPosition();
    POSITION_T textSize = mDocument->GetTextSize();
    if (currentPos >= textSize)
    {
        return;  // Already at end
    }

    // Get next word position
    POSITION_T newPos = mDocument->GetNextWordPosition(currentPos);

    // Skip over hidden control codes and hidden paragraphs (SHOW_DOT/SHOW_NONE)
    newPos = SkipOverHiddenContent(newPos, +1);

    // Reset sticky X (horizontal movement)
    mCaretStickyX = -1;

    // Set new position
    mDocument->SetPosition(newPos);

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  delta [in] number of lines to move (+ = down, - = up)
///
/// @return nothing
///
/// @brief
/// Moves caret up or down by delta lines.
/// Maintains horizontal position (sticky X) across vertical movements.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretLine(int delta)
{
    // Guard: Need document and layout
    if (!mDocument || !mLayout)
    {
        return;
    }

    CloseTypingGroup() ;

    // Guard: No movement needed
    if (delta == 0)
    {
        return;
    }

    // Get current document position
    POSITION_T pos = mDocument->GetPosition();

    // Find which line contains the current position
    LINE_T currentLine = mLayout->GetLineFromPosition(pos);

    // Calculate target line number
    LINE_T targetLine = currentLine + delta;

    // Clamp to valid line range
    LINE_T maxLine = mLayout->GetNumberOfLines();
    if (maxLine == 0)
    {
        return;  // No lines in document
    }

    if (targetLine < 0)
    {
        targetLine = 0;
    }
    if (targetLine >= maxLine)
    {
        targetLine = maxLine - 1;
    }

    // No movement if already at target
    if (targetLine == currentLine)
    {
        return;
    }

    // Get target line structure
    const sLineLayout* line = mLayout->GetLineByRawLineNumber(targetLine);
    if (!line)
    {
        return;  // Invalid line
    }

    // Save sticky X on first vertical movement
    if (mCaretStickyX < 0)
    {
        mCaretStickyX = mCaretX;
    }

    // Find position in target line closest to sticky X
    POSITION_T targetPos = FindPositionAtX(line, mCaretStickyX);

    // Skip over hidden content (dot commands/comments in SHOW_NONE mode)
    targetPos = SkipOverHiddenContent(targetPos, (delta > 0) ? +1 : -1);

    // Set document position
    mDocument->SetPosition(targetPos);

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  delta [in] number of pages to move (+ = down, - = up)
///
/// @return nothing
///
/// @brief
/// Moves caret up or down by delta pages.
/// Page = viewport height / average line height (minus 2 for readability).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretPage(int delta)
{
    // Guard: need layout
    if (!mLayout)
    {
        return;
    }

    CloseTypingGroup() ;

    // Visible line tracking is populated during paint. If no paint has happened
    // yet (e.g., tests, or first keypress before first paint), fall back to
    // line-count estimate so we still move.
    bool hasTracking = (mPageFirstVisibleLine != 0 || mPageLastVisibleLine != 0
                        || mPageFirstVisibleLineScrollY != 0
                        || mPageLastVisibleLineScrollY != 0) ;

    if (!hasTracking)
    {
        // Fallback: estimate lines per page from viewport / average line height
        COORD_T viewportHeight = GetViewportHeight() ;
        COORD_T avgLineHeight = mLayout->GetAverageLineHeight() ;
        if (avgLineHeight <= 0)
        {
            return ;
        }
        int linesPerPage = static_cast<int>(viewportHeight / avgLineHeight) - 2 ;
        if (linesPerPage < 1)
        {
            linesPerPage = 1 ;
        }
        MoveCaretLine(delta * linesPerPage) ;
        return ;
    }

    // Calculate effective viewport height
    COORD_T viewportHeight = GetViewportHeight() ;
    if (mDisplaySettings.mode == DISPLAY_PAGE)
    {
        // pageBorder translation consumes viewport space
        viewportHeight -= mDisplaySettings.pageBorder ;
    }

    // Visible line count from last paint (for symmetric caret movement)
    LINE_T visibleLines = mPageLastVisibleLine - mPageFirstVisibleLine ;
    if (visibleLines < 1)
    {
        visibleLines = 1 ;
    }

    // Scroll anchor: caret's current scroll-space Y
    // Old caret line stays visible after the page movement
    COORD_T caretScrollY = mCaretY + mScrollOffset ;

    if (delta > 0)  // Page Down
    {
        // Old caret line becomes top of new view
        SetScrollOffset(caretScrollY) ;
    }
    else  // Page Up
    {
        // Old caret line becomes bottom of new view
        COORD_T newOffset = caretScrollY - viewportHeight ;
        if (newOffset < 0)
        {
            newOffset = 0 ;
        }
        SetScrollOffset(newOffset) ;
    }

    // Move caret by visible line count (symmetric in both directions)
    MoveCaretLine(delta * static_cast<int>(visibleLines)) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves caret to start of document (position 0).
/// Resets sticky X since this is an explicit jump.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretToDocStart(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Reset sticky X (explicit jump)
    mCaretStickyX = -1;

    // Move to position 0, skipping hidden paragraphs at document start
    POSITION_T newPos = SkipOverHiddenContent(0, +1);
    mDocument->SetPosition(newPos);

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves caret to end of document (last position).
/// Resets sticky X since this is an explicit jump.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCaretToDocEnd(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Reset sticky X (explicit jump)
    mCaretStickyX = -1;

    // Move to last position, skipping hidden paragraphs at document end
    POSITION_T lastPos = mDocument->GetTextSize();
    lastPos = SkipOverHiddenContent(lastPos, -1);
    mDocument->SetPosition(lastPos);

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scrolls the viewport up by one line height 
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ScrollUp(void)
{
    if (!mLayout)
    {
        return;
    }

    // Get current viewport
    const sViewport& viewport = GetViewport();

    // Already at top of document -- check scroll offset, not line number
    if (mScrollOffset <= 0)
    {
        return;
    }

    // Find the first visible line in viewport to get typical line height
    // Use viewport.topY to find line at top of screen
    LINE_T topLine = LineFromY(static_cast<COORD_T>(viewport.topY));

    // Get height of line to scroll by
    COORD_T scrollAmount = 0;
    if (topLine != NOT_SET && topLine >= 0 && topLine < mLayout->GetNumberOfLines())
    {
        scrollAmount = mLayout->GetLineHeight(topLine);
    }

    if (scrollAmount <= 0)
    {
        // Use default line height if calculation failed
        scrollAmount = 240;  // 12pt at 20 twips/point = 240 twips
    }

    // Decrease scroll offset by one line height
    COORD_T newOffset = mScrollOffset - scrollAmount;

    // SetScrollOffset clamps to valid range [0, maxScroll]
    SetScrollOffset(newOffset);

    // Recalculate viewport with new scroll position
    CalculateViewport();

    // Trigger repaint to show new viewport
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Scrolls the viewport down by one line height
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ScrollDown(void)
{
    if (!mLayout)
    {
        return;
    }

    // Get current viewport
    const sViewport& viewport = GetViewport();

    // Find the first visible line in viewport to get typical line height
    LINE_T topLine = LineFromY(static_cast<COORD_T>(viewport.topY));

    if (topLine == NOT_SET)
    {
        // Can't find visible line, use default height
        topLine = 0;
    }

    // Get height of line to scroll by
    COORD_T scrollAmount = 0;

    if (topLine >= 0 && topLine < mLayout->GetNumberOfLines())
    {
        scrollAmount = mLayout->GetLineHeight(topLine);
    }

    if (scrollAmount <= 0)
    {
        // Use default line height if calculation failed
        scrollAmount = 240;  // 12pt at 20 twips/point = 240 twips
    }

    // Increase scroll offset by one line height
    COORD_T newOffset = mScrollOffset + scrollAmount;

    // SetScrollOffset clamps to valid range [0, maxScroll]
    SetScrollOffset(newOffset);

    // Recalculate viewport with new scroll position
    CalculateViewport();

    // Trigger repaint to show new viewport
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move caret to start of first visible line in viewport (top-left of screen).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorTopLeft(void)
{
    if (!mLayout || !mDocument)
    {
        return;
    }

    // Get current viewport
    const sViewport& viewport = GetViewport();

    // Find the first visible line at top of viewport
    LINE_T topLine = LineFromY(static_cast<COORD_T>(viewport.topY));

    if (topLine == NOT_SET)
    {
        // Can't find line at viewport top, move to document start
        topLine = 0;
    }

    // Get absolute document position at start of this line
    POSITION_T position = mLayout->GetLineStartDocumentPosition(topLine);

    // Move caret to this position
    mCaretDocumentPosition = position;
    mDocument->SetPosition(position);

    // Update caret display
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move caret to end of last visible line in viewport (bottom-right of screen).
/// 
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorBottomRight(void)
{
    if (!mLayout || !mDocument)
    {
        return;
    }

    // Get current viewport
    const sViewport& viewport = GetViewport();

    // Find the last visible line at bottom of viewport
    // Subtract a small amount to ensure we're inside the viewport
    LINE_T bottomLine = LineFromY(static_cast<COORD_T>(viewport.bottomY - 1));

    if (bottomLine == NOT_SET)
    {
        // Can't find line at viewport bottom, use last line in document
        bottomLine = mLayout->GetNumberOfLines() - 1;

        if (bottomLine < 0)
        {
            bottomLine = 0;
        }
    }

    // Get absolute document position at end of this line
    POSITION_T position = 0;

    // Get line end position (relative to paragraph)
    POSITION_T lineEndOffset = mLayout->GetLineEndPosition(bottomLine);

    // Get paragraph containing this line
    PARAGRAPH_T para = mLayout->GetParagraphFromLine(bottomLine);

    if (para != NOT_SET)
    {
        // Get paragraph start
        POSITION_T paraStart = 0;
        POSITION_T paraEnd = 0;
        mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);

        // Calculate absolute position
        position = paraStart + lineEndOffset;
    }

    // Move caret to this position
    mCaretDocumentPosition = position;
    mDocument->SetPosition(position);

    // Update caret display
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves the caret to the beginning of the document, then updates
/// the caret position, scrolls into view, and repaints.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorTopofFile(void)
{
    // Wrapper for base class method
    MoveCaretToDocStart();
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Moves the caret to the end of the document, then updates
/// the caret position, scrolls into view, and repaints.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorEndofFile(void)
{
    // Wrapper for base class method
    MoveCaretToDocEnd();
    CalculateCaretPosition();
    ScrollIntoView();
    CalculateCaretPosition();  // Recalculate after scroll adjusts mScrollOffset
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move caret to the start of the current block (^QK in WordStar).
/// If no block is set, does nothing.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorStartBlock(void)
{
    if (!mDocument)
    {
        return;
    }

    // Check if block is set
    if (mDocument->mBlockSet)
    {
        // Get block boundaries (absolute document positions)
        POSITION_T blockStart = 0;
        POSITION_T blockEnd = 0;
        mDocument->GetBlock(blockStart, blockEnd);

        // Move caret to block start, skipping hidden content
        POSITION_T position = SkipOverHiddenContent(blockStart, +1);
        mCaretDocumentPosition = position;
        mDocument->SetPosition(position);

        // Reset sticky X (explicit jump)
        mCaretStickyX = -1;

        // Update display
        CalculateCaretPosition();
        ScrollIntoView();
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move caret to the end of the current block (^QX in WordStar).
/// If no block is set, does nothing.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorEndBlock(void)
{
    if (!mDocument)
    {
        return;
    }

    // Check if block is set
    if (mDocument->mBlockSet)
    {
        // Get block boundaries (absolute document positions)
        POSITION_T blockStart = 0;
        POSITION_T blockEnd = 0;
        mDocument->GetBlock(blockStart, blockEnd);

        // Move caret to block end, skipping hidden content
        POSITION_T position = SkipOverHiddenContent(blockEnd, -1);
        mCaretDocumentPosition = position;
        mDocument->SetPosition(position);

        // Reset sticky X (explicit jump)
        mCaretStickyX = -1;

        // Update display
        CalculateCaretPosition();
        ScrollIntoView();
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move caret to the start of the current line (Home key in WordStar).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorStartLine(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::MoveCursorStartLine() (lines 3460-3464) ***

    if (!mDocument || !mLayout)
    {
        return;
    }

    // Get current line from caret position (BLACK BOX API)
    LINE_T line = mLayout->GetLineFromPosition(mDocument->GetPosition());

    // Get absolute document position at start of line (BLACK BOX API)
    POSITION_T position = mLayout->GetLineStartDocumentPosition(line);

    // Move caret to line start
    mCaretDocumentPosition = position;
    mDocument->SetPosition(position);

    // Reset sticky X (explicit jump)
    mCaretStickyX = -1;

    // Update display
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  dotPrefix [in] the dot command itself, e.g. ".tc" or ".ix" --
///                    no trailing space or digit suffix
/// @param  text [in] the entry text collected from the caller's own dialog
///
/// @return nothing
///
/// @brief
/// Shared implementation behind the GUI's and TUI's Insert -> Index/TOC
/// Entry commands (TOC Entry / Index Entry): inserts "<dotPrefix> <text>"
/// as its own line right before the current one, matching the WS7 manual's
/// "WordStar inserts the .tc dot command followed by the text." A dot
/// command must be its own line to parse, so any embedded line break in
/// the typed text is stripped first rather than being allowed to split it
/// into two lines and corrupt the command. Text that's empty (or becomes
/// empty once line breaks are stripped) inserts nothing.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::InsertDotCommandEntry(const std::string &dotPrefix, const std::string &text)
{
    std::string clean = text;
    clean.erase(std::remove(clean.begin(), clean.end(), '\r'), clean.end());
    clean.erase(std::remove(clean.begin(), clean.end(), '\n'), clean.end());

    if (clean.empty())
    {
        return;
    }

    MoveCursorStartLine();
    GetDocument()->MaybeInsertHardReturn();
    GetDocument()->Insert(dotPrefix + " " + clean + "\n");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move caret to the end of the current line (End key in WordStar).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveCursorEndLine(void)
{
    if (!mDocument || !mLayout)
    {
        return;
    }

    // Get current line from caret position (BLACK BOX API)
    LINE_T line = mLayout->GetLineFromPosition(mDocument->GetPosition());

    // Get line end position (paragraph-relative) (BLACK BOX API)
    POSITION_T lineEndOffset = mLayout->GetLineEndPosition(line);

    // Get paragraph containing this line (BLACK BOX API)
    PARAGRAPH_T para = mLayout->GetParagraphFromLine(line);

    POSITION_T position = 0;

    if (para != NOT_SET)
    {
        // Get paragraph start position (absolute)
        POSITION_T paraStart = 0;
        POSITION_T paraEnd = 0;
        mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);

        // Calculate absolute document position
        // Old system: GetLineEndPosition returned absolute
        // New system: must convert paragraph-relative to absolute
        position = paraStart + lineEndOffset;
    }
    else
    {
        // Fallback: stay at current position
        position = mDocument->GetPosition();
    }

    // Move caret to line end
    mCaretDocumentPosition = position;
    mDocument->SetPosition(position);

    // Reset sticky X (explicit jump)
    mCaretStickyX = -1;

    // Update display
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to previous cursor position in position history stack.
/// [REFACTORED from editorctrl.cpp::GotoPreviousPosition()]
///
/// @note
/// cDocument maintains a stack of previous positions. This method pops
/// the most recent position from the stack and moves the caret there.
/// Useful for returning to where you were before a jump (e.g., after search).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::GotoPreviousPosition(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::GotoPreviousPosition() ***
    // API CHANGE: mDocument. becomes mDocument->
    // NO OTHER CHANGES: Exact copy of old implementation

    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Call document's position history management
    mDocument->GotoPreviousPosition();

    // Update caret to new position
    mCaretDocumentPosition = mDocument->GetPosition();

    // Update display
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Returns caret to the position saved before the last Find/Replace operation.
/// Uses mLastFindandReplace which is set by Find() and Replace().
///
/// *** PATTERN FROM OLD SYSTEM ***
/// Similar to other navigation methods - SetPosition, CalculateCaretPosition,
/// ScrollIntoView, Repaint
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::GotoLastFindandReplace(void)
{
    if (!mDocument)
    {
        return;
    }

    // Set document position to saved position
    // Skip hidden content (dot commands/comments in SHOW_NONE mode)
    POSITION_T safePos = SkipOverHiddenContent(mLastFindandReplace, +1);
    mDocument->SetPosition(safePos);

    // Update visual state
    CalculateCaretPosition();
    ScrollIntoView();
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Jump to next font tag (formatting change) in document.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::GotoFontTag(void)
{
    POSITION_T pos = mDocument->GetNextFontTagPosition();
    mDocument->SetPosition(pos);

    ScrollIntoView();
    CalculateCaretPosition();
    Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] text to insert at caret position
///
/// @return nothing
///
/// @brief
/// Inserts text at the current caret position.
/// Handles both plain text and text with WordStar control codes.
///
/// @note
/// In WordStar, inserting text does NOT delete or affect blocks.
/// Block operations only happen via explicit block commands.
/// cDocument automatically adjusts block positions/sizes via IncrementAttributes().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::InsertText(const std::string& text)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    // Guard: empty text
    if (text.empty())
    {
        return;
    }

    // open a typing group if not already active
    if (!mTypingGroupActive)
    {
        mDocument->BeginUndoGroup() ;
        mTypingGroupActive = true ;
    }

    // In overwrite mode, delete characters before inserting
    if (!mInsertMode)
    {
        // Count graphemes to overwrite via cDocument (BLACK BOX: never parse UTF-8
        // here). Counting code points over-deletes for combining marks (e + U+0301)
        // and ZWJ emoji families, which are multiple code points but one grapheme.
        std::vector<POSITION_T> insertOffsets;
        size_t graphemesToInsert = mDocument->GraphemeCount(text, insertOffsets);

        // Overwrite real graphemes, applying per-type rules to control markers:
        //   - format/tab/variable markers: delete (Delete() cleans up the tables)
        //   - font/color markers: stop and insert in front (preserve the marker)
        //   - footnote/endnote anchors and saved positions: jump over (preserve)
        //   - line end / document end: stop
        POSITION_T pos = mDocument->GetPosition();
        size_t remaining = graphemesToInsert;
        while (remaining > 0)
        {
            std::string ch = mDocument->GetCharNoAdvance(pos);
            if (ch.empty() || ch[0] == HARD_RETURN || ch[0] == STYLE_EOF)
            {
                break;  // line end or document end
            }
            if (ch[0] == SAVE_CHAR)
            {
                pos++;  // jump over a saved-position bookmark (non-deletable)
                continue;
            }
            if (ch[0] == MARKER_CHAR)
            {
                eModifiers code = mDocument->GetControlChar(pos);
                if (code == STYLE_FONT1 || code == STYLE_INTERNAL_COLOR)
                {
                    break;  // insert in front of a font/color change
                }
                if (code == STYLE_FOOTNOTE || code == STYLE_ENDNOTE)
                {
                    pos++;  // jump over a note anchor
                    continue;
                }
                // format/tab/variable: overwrite consumes the marker
                mDocument->Delete(pos, 1);
                remaining--;
                continue;
            }
            mDocument->Delete(pos, 1);  // normal visible grapheme
            remaining--;
        }

        // Insertion happens at pos: after any jumped-over markers, or in front of
        // a font/color marker we stopped on.
        mDocument->SetPosition(pos);
    }

    // Get position before insert
    POSITION_T posBeforeInsert = mDocument->GetPosition();
    UNUSED_ARGUMENT(posBeforeInsert);  // Reserved for future undo/redo tracking

    // Insert text at current caret position (BLACK BOX: document handles grapheme counting)
    mDocument->Insert(text);

    // Check if just-typed character completes a &X& variable pattern
    CheckAndReplaceVariable();

    // Close typing group on space (word boundary for Western languages)
    // AFTER insert so space is in the group
    if (text == " ")
    {
        CloseTypingGroup() ;
    }

    // Close typing group on CJK punctuation (sentence/clause boundary for CJK languages)
    // This handles direct keyboard input without IME (hiragana-only, Thai, etc.)
    // IME-based input (Japanese kanji, Chinese) is handled separately in inputMethodEvent()
    if (IsCJKPunctuation(text))
    {
        CloseTypingGroup() ;
    }

    // Get position after insert to calculate grapheme count
    POSITION_T posAfterInsert = mDocument->GetPosition();

    // Document auto-advances position, but we need to ensure we're at the right place
    // The Insert method should have moved position forward by the grapheme count
    // Just verify we're at the expected position
    (void)posAfterInsert;  // Position is already correct after insert

    // Note: Document's Insert() automatically advances position by grapheme count
    // So we don't need to manually calculate or set position

    // Reset sticky X (horizontal caret movement)
    mCaretStickyX = -1;

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos [in] position to start deletion
/// @param  length [in] number of graphemes to delete
///
/// @return nothing
///
/// @brief
/// Deletes a range of text from the document.
/// Adjusts caret position if it was in or after the deleted range.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::Delete(POSITION_T pos, POSITION_T length)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Guard: nothing to delete
    if (length == 0)
    {
        return;
    }

    // Guard: position is valid
    POSITION_T textSize = mDocument->GetTextSize();

    // Don't delete if position is at or past text size
    if (pos >= textSize || textSize == 0)
    {
        return;  // Nothing to delete
    }

    // Clamp length to available text
    POSITION_T maxDelete = textSize - pos;
    if (maxDelete <= 0)
    {
        return;  // Nothing to delete
    }

    if (length > maxDelete)
    {
        length = maxDelete;
    }

    // Safety: ensure length is positive
    if (length <= 0)
    {
        return;
    }

    // Save current position BEFORE delete (document's Delete may change it)
    POSITION_T currentPos = mDocument->GetPosition();

    // Delete from document (BLACK BOX: document handles deletion)
    mDocument->Delete(pos, length);

    // Adjust caret based on where it was BEFORE delete. Set the caret
    // explicitly in every case rather than relying on the position
    // cDocument::Delete leaves behind.
    if (currentPos <= pos)
    {
        // Caret was at or before deleted range - keep original position
        mDocument->SetPosition(currentPos);
    }
    else if (currentPos < pos + length)
    {
        // Caret was inside deleted range - move to start
        mDocument->SetPosition(pos);
    }
    else
    {
        // Caret was after deleted range - shift left by deleted length
        mDocument->SetPosition(currentPos - length);
    }

    // Reset sticky X (horizontal caret movement)
    mCaretStickyX = -1;

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Deletes character before caret position (Backspace behavior).
/// Does nothing if caret is at start of document.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteChar(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    // Get current position
    POSITION_T currentPos = mDocument->GetPosition();

    // Guard: at start of document
    if (currentPos == 0)
    {
        return;  // Nothing to delete
    }

    // Delete previous character (deletes 1 grapheme before caret)
    Delete(currentPos - 1, 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Flip between insert and overwrite caret modes and refresh the caret
/// position so the visible caret width reflects the new mode.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ToggleInsertOverwrite(void)
{
    mInsertMode = !mInsertMode;
    CalculateCaretPosition();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Deletes from caret to end of current word (Ctrl+T in WordStar).
/// Handles word boundaries using document's word navigation.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteWordRight(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    POSITION_T startPos = mDocument->GetPosition();

    // Find end of current word
    POSITION_T endPos = FindWordEnd(startPos);

    // Calculate length to delete
    POSITION_T length = endPos - startPos;

    // Delete from start to end
    if (length > 0)
    {
        Delete(startPos, length);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete from caret position backward to start of previous word
/// (Ctrl+Backspace behavior). Mirrors DeleteWordRight but deletes
/// backward using GetPrevWordPosition.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteWordLeft(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    POSITION_T currentPos = mDocument->GetPosition();
    if (currentPos == 0)
    {
        return;  // Already at start
    }

    // Find start of previous word
    POSITION_T wordStart = mDocument->GetPrevWordPosition(currentPos);

    // Calculate length to delete
    POSITION_T length = currentPos - wordStart;

    // Delete from word start to current position
    if (length > 0)
    {
        Delete(wordStart, length);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Inserts line break at caret position (Enter key behavior).
/// Creates new paragraph.
///
/// @note
/// In WordStar, line breaks do NOT delete or affect blocks.
/// cDocument automatically adjusts block positions/sizes via IncrementAttributes().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::LineBreak(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Insert paragraph break at current position (BLACK BOX: document handles paragraph creation)
    // HARD_RETURN (character 13) creates a new paragraph
    mDocument->Insert(HARD_RETURN);

    // Note: Insert() automatically advances position by 1 (the HARD_RETURN character)
    // So caret is now at start of new paragraph

    // Reset sticky X for vertical movement
    mCaretStickyX = -1;

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Handles Backspace key. Deletes character before caret.
///
/// @note
/// In WordStar, backspace does NOT delete blocks.
/// cDocument automatically adjusts block positions/sizes via DecrementAttributes().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::Backspace(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    // Delete character before caret
    DeleteChar();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Handles Delete key. Deletes character at caret position (forward delete).
///
/// @note
/// In WordStar, delete key does NOT delete blocks.
/// cDocument automatically adjusts block positions/sizes via DecrementAttributes().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteKey(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Delete character at caret (without moving caret)
    POSITION_T deletePos = mDocument->GetPosition();
    POSITION_T textSize = mDocument->GetTextSize();

    // Guard: at end of document
    if (deletePos >= textSize)
    {
        return;  // Nothing to delete
    }

    // Delete character at caret
    mDocument->Delete(deletePos, 1);

    // Visual updates removed - caller responsible for CalculateCaretPosition() + ScrollIntoView()
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Handles Tab key. Inserts a TAB_TAB marker into the document.
/// Uses InsertTab() to create proper metadata (pairs + tab table entries).
///
/// @note
/// Must use InsertTab() rather than InsertText("\t") because
/// SetControlChar() explicitly skips STYLE_TAB -- it expects tabs
/// to be inserted via InsertTab() which creates the required
/// TYPE_TAB pairs entry and sWSTab tab table entry.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::Tab(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // Create a standard left-aligned tab
    sWSTab tab;
    tab.type = TAB_TAB;
    tab.tabsize = 0;
    tab.abstabsize = 0;
    tab.size = 0;

    // InsertTab() creates MARKER_CHAR in buffer + pairs/tab table entries
    mDocument->InsertTab(tab);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] WordStar string to insert (may contain control codes)
///
/// @return nothing
///
/// @brief
/// Inserts a WordStar-formatted string at the caret position.
/// Used by ^J commands (date, time, filename, etc.)
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::InsertWordStarString(const std::string& text)
{
    // Use base class InsertText which handles all the heavy lifting
    // Visual update driven by listener (InsertText calls NotifyChanged calls OnDocumentChanged)
    InsertText(text);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Insert center tab at current caret position. Center tabs cause the line
/// to be center-aligned from the tab marker onward.
///
/////////////////////////////////////////////////////////////////////////////

void cEditorBase::InsertCenterTab(void)
{
    // Insert TAB_CENTER at the caret position (InsertTab advances position)
    // Visual update driven by listener (InsertTab calls NotifyChanged calls OnDocumentChanged)
    sWSTab tab;
    tab.type = TAB_CENTER;
    mDocument->InsertTab(tab);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Insert right tab at current caret position. Right tabs cause the line
/// to be right-aligned from the tab marker onward.
///
/////////////////////////////////////////////////////////////////////////////

void cEditorBase::InsertRightTab(void)
{
    // Insert TAB_RIGHT at the caret position (InsertTab advances position)
    // Visual update driven by listener (InsertTab calls NotifyChanged calls OnDocumentChanged)
    sWSTab tab;
    tab.type = TAB_RIGHT;
    mDocument->InsertTab(tab);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete the entire layout/screen line containing the caret.
/// Uses layout line APIs to determine line boundaries.
/// If not at EOF, the line break character is included in the deletion.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteLine(void)
{
    POSITION_T pos = mDocument->GetPosition();
    LINE_T line = mLayout->GetLineFromPosition(pos);

    // Get absolute document positions for line boundaries
    POSITION_T start = mLayout->GetLineStartDocumentPosition(line);
    POSITION_T relativeStart = mLayout->GetLineStartPosition(line);
    POSITION_T relativeEnd = mLayout->GetLineEndPosition(line);
    POSITION_T end = start + (relativeEnd - relativeStart);

    // Move cursor to start of line
    mDocument->SetPosition(start);

    POSITION_T size = mDocument->GetTextSize();

    // Include the line break in deletion, but don't delete ^Z
    if (end != size - 1)
    {
        end++;
    }

    // Delete the line
    // Visual update driven by listener (Delete calls NotifyChanged calls OnDocumentChanged)
    if (end > start)
    {
        Delete(start, end - start);

        // Update caret from document (Delete() adjusted it)
        mCaretDocumentPosition = mDocument->GetPosition();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete from start of layout/screen line to caret position.
/// Uses layout line APIs to determine line start.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteLineLeft(void)
{
    POSITION_T pos = mDocument->GetPosition();

    // Guard: at start of document
    if (pos == 0)
    {
        return;
    }

    LINE_T line = mLayout->GetLineFromPosition(pos);
    POSITION_T start = mLayout->GetLineStartDocumentPosition(line);

    // Delete from line start to current position
    // Visual update driven by listener (Delete calls NotifyChanged calls OnDocumentChanged)
    if (pos > start)
    {
        Delete(start, pos - start);

        // Update caret from document (Delete() adjusted it)
        mCaretDocumentPosition = mDocument->GetPosition();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete from caret position to end of layout/screen line.
/// Uses layout line APIs to determine line end.
/// If not at EOF, the line break character is included in the deletion.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteLineRight(void)
{
    POSITION_T pos = mDocument->GetPosition();
    POSITION_T size = mDocument->GetTextSize();

    // Guard: at or past end of document
    if (pos >= size)
    {
        return;
    }

    LINE_T line = mLayout->GetLineFromPosition(pos);

    // Compute absolute end position from layout line
    POSITION_T start = mLayout->GetLineStartDocumentPosition(line);
    POSITION_T relativeStart = mLayout->GetLineStartPosition(line);
    POSITION_T relativeEnd = mLayout->GetLineEndPosition(line);
    POSITION_T end = start + (relativeEnd - relativeStart);

    // Include the line break in deletion, but don't delete ^Z
    if (end != size - 1)
    {
        end++;
    }

    // Delete from current position to end of line
    // Visual update driven by listener (Delete calls NotifyChanged calls OnDocumentChanged)
    if (end > pos)
    {
        Delete(pos, end - pos);

        // Update caret from document (Delete() adjusted it)
        mCaretDocumentPosition = mDocument->GetPosition();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Close the current typing group. If a typing group is active, ends the
/// document-level undo group so the accumulated typed characters become
/// one undo step. Called on word boundaries (space), cursor movement,
/// delete operations, formatting changes, and block operations.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::CloseTypingGroup(void)
{
    if (mTypingGroupActive && mDocument)
    {
        mDocument->EndUndoGroup() ;
        mTypingGroupActive = false ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Restores the previous block selection. Swaps current and saved blocks.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetPreviousBlock(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    // *** COPIED FROM OLD SYSTEM - editorbase.cpp::SetPreviousBlock() ***
    // API CHANGE: Old used mDocument.SetPreviousBlock() (member)
    // New uses mDocument->SetPreviousBlock() (pointer)
    mDocument->SetPreviousBlock();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Copy block to caret position (Ctrl+KC in WordStar).
/// Block remains set after copy.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::CopyBlock(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // wrap in undo group so the copy undoes as one step
    mDocument->BeginUndoGroup() ;
    mDocument->CopyBlock() ;
    mDocument->EndUndoGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Move block to caret position (Ctrl+KV in WordStar).
/// Block is unset after move.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::MoveBlock(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // wrap in undo group so the move undoes as one step
    mDocument->BeginUndoGroup() ;
    mDocument->MoveBlock() ;
    mDocument->EndUndoGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Delete marked block (Ctrl+KY in WordStar).
/// Block is unset after deletion.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::DeleteBlock(void)
{
    // Guard: need document
    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    // wrap in undo group so the delete undoes as one step
    mDocument->BeginUndoGroup() ;
    mDocument->DeleteBlock() ;
    mDocument->EndUndoGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Marks the beginning of a block selection at the current document
/// position. Wraps the operation in a batch update.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetBeginBlock(void)
{
    if (mDocument)
    {
        BeginBatchUpdate() ;
        mDocument->SetBeginBlock() ;
        EndBatchUpdate() ;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Marks the end of a block selection at the current document position.
/// Wraps the operation in a batch update.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetEndBlock(void)
{
    if (mDocument)
    {
        BeginBatchUpdate() ;
        mDocument->SetEndBlock() ;
        EndBatchUpdate() ;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Clears the current block selection.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::UnSetBlock(void)
{
    if (mDocument)
    {
        mDocument->UnsetBlock();
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle block visibility (hide/show block without clearing it).
/// If block is visible (mBlockSet = true), hide it.
/// If block is hidden (mBlockSet = false) but positions are set, show it.
/// [REFACTORED from editorctrl.cpp::ToggleHideBlock()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ToggleHideBlock(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::ToggleHideBlock() ***

    if (!mDocument)
    {
        return;
    }

    if (mDocument->mBlockSet == true)
    {
        // Block is visible - hide it
        mDocument->mBlockSet = false;
    }
    else
    {
        // Block is hidden - show it if positions are set
        if (mDocument->mStartBlock != NOT_SET && mDocument->mEndBlock != NOT_SET)
        {
            mDocument->mBlockSet = true;
        }
    }

    // Repaint to update block highlighting
    Repaint();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Convert all characters in block to uppercase.
/// Uses Unicode-aware case conversion to handle international characters.
/// Block remains selected after conversion.
/// [REFACTORED from editorctrl.cpp::UpperCaseBlock()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::UpperCaseBlock(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::UpperCaseBlock() ***

    if (!mDocument || mDocument->mBlockSet == false)
    {
        return;
    }

    CloseTypingGroup() ;
    mDocument->BeginUndoGroup() ;

    // Get block boundaries
    POSITION_T start = 0;
    POSITION_T end = 0;
    mDocument->GetBlock(start, end);

    // Get block text (GetBlockText uses half-open interval, so pass end+1)
    std::string str = mDocument->GetBlockText(start, end + 1);

    // Convert to uppercase via cDocument (Unicode-aware)
    str = mDocument->UpperCase(str) ;

    // Batch wraps DeleteBlock + InsertWordStarString + block re-selection
    // (multiple notifications coalesced into one visual update)
    BeginBatchUpdate() ;

    // Delete old block
    mDocument->DeleteBlock();

    // Insert converted text at block start
    mDocument->SetPosition(start);
    InsertWordStarString(str);
    CloseTypingGroup() ;

    // Re-select the block using calculated length
    // Need to get actual inserted length from document
    POSITION_T newEnd = mDocument->GetPosition();

    mDocument->SetPosition(start);
    mDocument->SetBeginBlock();
    mDocument->SetPosition(newEnd);
    mDocument->SetEndBlock();

    EndBatchUpdate() ;

    mDocument->EndUndoGroup() ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Convert all characters in block to lowercase.
/// Uses Unicode-aware case conversion to handle international characters.
/// Block remains selected after conversion.
/// [REFACTORED from editorctrl.cpp::LowerCaseBlock()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::LowerCaseBlock(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::LowerCaseBlock() ***

    if (!mDocument || mDocument->mBlockSet == false)
    {
        return;
    }

    CloseTypingGroup() ;
    mDocument->BeginUndoGroup() ;

    // Get block boundaries
    POSITION_T start = 0;
    POSITION_T end = 0;
    mDocument->GetBlock(start, end);

    // Get block text (GetBlockText uses half-open interval, so pass end+1)
    std::string str = mDocument->GetBlockText(start, end + 1);

    // Convert to lowercase via cDocument (Unicode-aware)
    str = mDocument->LowerCase(str) ;

    // Batch wraps DeleteBlock + InsertWordStarString + block re-selection
    BeginBatchUpdate() ;

    // Delete old block
    mDocument->DeleteBlock();

    // Insert converted text at block start
    mDocument->SetPosition(start);
    InsertWordStarString(str);
    CloseTypingGroup() ;

    // Re-select the block using actual inserted length
    POSITION_T newEnd = mDocument->GetPosition();

    mDocument->SetPosition(start);
    mDocument->SetBeginBlock();
    mDocument->SetPosition(newEnd);
    mDocument->SetEndBlock();

    EndBatchUpdate() ;

    mDocument->EndUndoGroup() ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Convert all characters in block to titlecase.
/// Uses Unicode-aware case conversion to handle international characters.
/// Block remains selected after conversion.
/// [REFACTORED from editorctrl.cpp::TitleCaseBlock()]
///
/// @note
/// The old system had "// not working" comment, but the implementation
/// appears correct - uses unicode::to_titlecase().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::TitleCaseBlock(void)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::TitleCaseBlock() ***

    if (!mDocument || mDocument->mBlockSet == false)
    {
        return;
    }

    CloseTypingGroup() ;
    mDocument->BeginUndoGroup() ;

    // Get block boundaries
    POSITION_T start = 0;
    POSITION_T end = 0;
    mDocument->GetBlock(start, end);

    // Get block text (GetBlockText uses half-open interval, so pass end+1)
    std::string str = mDocument->GetBlockText(start, end + 1);

    // Convert to titlecase via cDocument (Unicode-aware)
    str = mDocument->TitleCase(str) ;

    // Batch wraps DeleteBlock + InsertWordStarString + block re-selection
    BeginBatchUpdate() ;

    // Delete old block
    mDocument->DeleteBlock();

    // Insert converted text at block start
    mDocument->SetPosition(start);
    InsertWordStarString(str);
    CloseTypingGroup() ;

    // Re-select the block using actual inserted length
    POSITION_T newEnd = mDocument->GetPosition();

    mDocument->SetPosition(start);
    mDocument->SetBeginBlock();
    mDocument->SetPosition(newEnd);
    mDocument->SetEndBlock();

    EndBatchUpdate() ;

    mDocument->EndUndoGroup() ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Begin a batch update. Suppresses visual updates from
/// OnDocumentChanged until EndBatchUpdate is called. Use for
/// multi-mutation operations (Replace, MoveBlock, case transforms).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::BeginBatchUpdate(void)
{
    mBatchUpdateCount++ ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// End a batch update. When the count drops to zero, marks that a
/// mutation occurred. Visual update is deferred to keyPressEvent's
/// post-layout block which has current layout data.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::EndBatchUpdate(void)
{
    mBatchUpdateCount-- ;
    if (mBatchUpdateCount == 0)
    {
        mListenerHandledUpdate = true ;

        // Update sibling editor (reveal codes) -- it has no keyPressEvent running
        if (mSiblingEditor != nullptr)
        {
            mSiblingEditor->PerformPostCommandUpdate() ;
            mSiblingEditor->TriggerIdleLayout() ;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  int offset [in] - Position slot (0-9) to save current caret position
///
/// @return nothing
///
/// @brief
/// Save current caret position to one of 10 position markers (0-9).
/// Position markers allow quick navigation back to saved locations.
/// If called twice on same slot, toggles the marker off.
/// [REFACTORED from editorctrl.cpp::SavePosition()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SavePosition(int offset)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::SavePosition() ***

    if (!mDocument)
    {
        return;
    }

    // SetSavePosition handles the toggle logic
    mDocument->SetSavePosition(offset);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int offset [in] - Position slot (0-9) to jump to
///
/// @return nothing
///
/// @brief
/// Jump to a previously saved position marker (0-9).
/// If position slot is not set (NOT_SET), does nothing.
/// Scrolls the caret into view after jumping.
/// [REFACTORED from editorctrl.cpp::GotoSavePosition()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::GotoSavePosition(int offset)
{
    // *** COPIED FROM OLD SYSTEM - editorctrl.cpp::GotoSavePosition() ***

    if (!mDocument)
    {
        return;
    }

    CloseTypingGroup() ;

    if (mDocument->mSavePosition[offset] != NOT_SET)
    {
        // Skip hidden content (dot commands/comments in SHOW_NONE mode)
        mCaretDocumentPosition = SkipOverHiddenContent(mDocument->mSavePosition[offset], +1);
        mDocument->SetPosition(mCaretDocumentPosition);

        CalculateCaretPosition();
        ScrollIntoView();
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Toggle full justification for the current paragraph by inserting a
/// .oj on or .oj off dot command on a new line before the paragraph.
/// Preserves caret position (stays at same place in text).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ToggleJustification(void)
{
    if (!mDocument || !mLayout)
    {
        return;
    }

    // Get current paragraph and caret position
    POSITION_T savedPos = mDocument->GetPosition();
    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(savedPos);

    // Determine current justification state from previous paragraph's endState
    // For paragraph 0: default is left-aligned (justify=false)
    bool isJustified = false;
    if (para > 0)
    {
        const sParagraphLayout* prevPara = mLayout->GetParagraphLayout(para - 1);
        if (prevPara)
        {
            isJustified = prevPara->endState.justify;
        }
    }

    // Build the dot command string
    std::string dotCmd = isJustified ? ".oj off\r" : ".oj on\r";

    // Move to start of current paragraph
    POSITION_T paraStart, paraEnd;
    mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);
    mDocument->SetPosition(paraStart);

    // Insert dot command (the \r creates a new paragraph)
    mDocument->Insert(dotCmd);

    // Restore caret position (shifted by inserted graphemes)
    // All ASCII, so byte count = grapheme count
    POSITION_T offset = static_cast<POSITION_T>(dotCmd.size());
    mDocument->SetPosition(savedPos + offset);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  align [in] requested alignment (JUST_LEFT, JUST_CENTER, JUST_RIGHT, JUST_JUST)
///
/// @return nothing
///
/// @brief
/// Set paragraph alignment by inserting a .oj dot command on a new line
/// above the current paragraph. Does nothing if already in the requested
/// alignment. Preserves caret position (stays at same place in text).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetAlignment(eJustification align)
{
    if (!mDocument || !mLayout)
    {
        return ;
    }

    // Get current paragraph and caret position
    POSITION_T savedPos = mDocument->GetPosition() ;
    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(savedPos) ;

    // Determine current alignment from previous paragraph's endState
    // For paragraph 0: default is left-aligned
    eJustification currentJust = JUST_LEFT ;
    if (para > 0)
    {
        const sParagraphLayout* prevPara = mLayout->GetParagraphLayout(para - 1) ;
        if (prevPara)
        {
            if (prevPara->endState.justify)
            {
                currentJust = JUST_JUST ;
            }
            else if (prevPara->endState.center)
            {
                currentJust = JUST_CENTER ;
            }
            else if (prevPara->endState.right)
            {
                currentJust = JUST_RIGHT ;
            }
            else
            {
                currentJust = JUST_LEFT ;
            }
        }
    }

    // Already in requested alignment, nothing to do
    if (currentJust == align)
    {
        return ;
    }

    // Build the dot command string
    std::string dotCmd ;
    switch (align)
    {
        case JUST_LEFT:
        {
            dotCmd = ".oj off\r" ;
            break ;
        }
        case JUST_CENTER:
        {
            dotCmd = ".oj c\r" ;
            break ;
        }
        case JUST_RIGHT:
        {
            dotCmd = ".oj r\r" ;
            break ;
        }
        case JUST_JUST:
        {
            dotCmd = ".oj on\r" ;
            break ;
        }
    }

    // Move to start of current paragraph
    POSITION_T paraStart, paraEnd ;
    mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd) ;
    mDocument->SetPosition(paraStart) ;

    // Insert dot command (the \r creates a new paragraph)
    mDocument->Insert(dotCmd) ;

    // Restore caret position (shifted by inserted graphemes)
    // All ASCII, so byte count = grapheme count
    POSITION_T offset = static_cast<POSITION_T>(dotCmd.size()) ;
    mDocument->SetPosition(savedPos + offset) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para [in] paragraph number to check
///
/// @return true if the paragraph is a .oj dot command
///
/// @brief
/// Check if a paragraph is a .oj (justify) dot command by examining
/// its raw text. Case-insensitive check for ".oj" prefix.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::IsOjDotCommand(PARAGRAPH_T para)
{
    if (para < 0 || para >= mDocument->GetNumberofParagraphs())
    {
        return false ;
    }

    std::string text = mDocument->GetParagraphText(para) ;
    if (text.length() < 3)
    {
        return false ;
    }

    // Case-insensitive check for ".oj" prefix
    if (text[0] == '.' &&
        (text[1] == 'o' || text[1] == 'O') &&
        (text[2] == 'j' || text[2] == 'J'))
    {
        return true ;
    }

    return false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para [in] paragraph number to read endState from
///
/// @return eJustification - alignment at end of the paragraph
///
/// @brief
/// Extract alignment from a paragraph's layout endState.
/// Returns JUST_LEFT for invalid or missing paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
eJustification cEditorBase::GetAlignmentFromEndState(PARAGRAPH_T para)
{
    if (para < 0)
    {
        return JUST_LEFT ;
    }

    const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para) ;
    if (!paraLayout)
    {
        return JUST_LEFT ;
    }

    if (paraLayout->endState.justify)
    {
        return JUST_JUST ;
    }
    if (paraLayout->endState.center)
    {
        return JUST_CENTER ;
    }
    if (paraLayout->endState.right)
    {
        return JUST_RIGHT ;
    }

    return JUST_LEFT ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  currentPara [in] paragraph to search forward from
///
/// @return eJustification - alignment of the next text paragraph
///
/// @brief
/// Scan forward from currentPara+1, skipping dot command and comment
/// paragraphs. When the next text paragraph M is found, return the
/// alignment in effect at its start (from endState of M-1).
/// Returns JUST_LEFT if no next text paragraph exists.
///
/////////////////////////////////////////////////////////////////////////////
eJustification cEditorBase::FindNextTextParagraphAlignment(PARAGRAPH_T currentPara)
{
    PARAGRAPH_T numParas = mDocument->GetNumberofParagraphs() ;
    PARAGRAPH_T layoutParas = mLayout->GetNumberOfParagraphs() ;

    for (PARAGRAPH_T m = currentPara + 1 ; m < numParas && m < layoutParas ; m++)
    {
        const sParagraphLayout* pl = mLayout->GetParagraphLayout(m) ;
        if (pl && !pl->isCommand && !pl->isComment)
        {
            // Found the next text paragraph at m
            // Its alignment comes from endState of m-1
            return GetAlignmentFromEndState(m - 1) ;
        }
    }

    // No next text paragraph found, default to left
    return JUST_LEFT ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  align [in] justification type
///
/// @return std::string - the .oj dot command string with trailing \r
///
/// @brief
/// Convert an eJustification value to the corresponding .oj dot
/// command string for document insertion.
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorBase::AlignmentToDotCommand(eJustification align)
{
    switch (align)
    {
        case JUST_LEFT:
        {
            return ".oj off\r" ;
        }
        case JUST_CENTER:
        {
            return ".oj c\r" ;
        }
        case JUST_RIGHT:
        {
            return ".oj r\r" ;
        }
        case JUST_JUST:
        {
            return ".oj on\r" ;
        }
    }

    return ".oj off\r" ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  align [in] requested alignment (JUST_LEFT, JUST_CENTER, JUST_RIGHT, JUST_JUST)
///
/// @return nothing
///
/// @brief
/// CUA paragraph alignment. Brackets the current paragraph with .oj dot
/// commands: a "before" command sets the desired alignment and an "after"
/// command restores the alignment of the next text paragraph.
///
/// If the paragraph already has the requested alignment and is bracketed,
/// both bracket commands are removed (toggle off). If the paragraph has
/// the requested alignment via inheritance (not bracketed), a .oj off
/// before and restoration after are inserted to override just this paragraph.
///
/// Preserves caret position. All mutations in one undo group.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetParagraphAlignment(eJustification align)
{
    if (!mDocument || !mLayout)
    {
        return ;
    }

    // ---- Phase 1: Gather state (read-only, before mutations) ----

    POSITION_T savedPos = mDocument->GetPosition() ;
    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(savedPos) ;

    // Current alignment from preceding paragraph's endState
    eJustification currentJust = GetAlignmentFromEndState(para - 1) ;

    // Check for existing bracket .oj commands around this paragraph
    bool hasPrecedingOj = (para > 0) && IsOjDotCommand(para - 1) ;
    bool hasFollowingOj = IsOjDotCommand(para + 1) ;
    bool isBracketed = hasPrecedingOj && hasFollowingOj ;

    // Alignment of the next text paragraph (for restoration command)
    eJustification nextTextAlign = FindNextTextParagraphAlignment(para) ;

    // Save positions and lengths of bracket paragraphs we may delete
    POSITION_T precedStart = 0, precedEnd = 0, precedLen = 0 ;
    if (hasPrecedingOj)
    {
        mDocument->GetParagraphStartandEnd(para - 1, precedStart, precedEnd) ;
        precedLen = precedEnd - precedStart + 1 ;
    }

    POSITION_T followStart = 0, followEnd = 0, followLen = 0 ;
    if (hasFollowingOj)
    {
        mDocument->GetParagraphStartandEnd(para + 1, followStart, followEnd) ;
        followLen = followEnd - followStart + 1 ;
    }

    // ---- Phase 2: Decide action ----

    // Action codes:
    // 0 = do nothing
    // 1 = remove bracket (toggle off)
    // 2 = insert/replace bracket
    int action = 0 ;

    if (currentJust == align)
    {
        // Same alignment requested
        if (isBracketed)
        {
            // Already bracketed with this alignment: toggle off
            action = 1 ;
        }
        else if (align != JUST_LEFT)
        {
            // Inherited non-left alignment, turn it off for this paragraph
            action = 2 ;
        }
        // else: already left (default), not bracketed: nothing to do
    }
    else
    {
        // Different alignment requested: insert or replace bracket
        action = 2 ;
    }

    if (action == 0)
    {
        return ;
    }

    // ---- Phase 3: Execute mutations ----

    CloseTypingGroup() ;
    mDocument->BeginUndoGroup() ;
    BeginBatchUpdate() ;

    // Delete existing bracket commands (only when fully bracketed)
    if (isBracketed)
    {
        // Delete following .oj FIRST (higher position, no effect on earlier positions)
        mDocument->Delete(followStart, followLen) ;

        // Delete preceding .oj
        mDocument->Delete(precedStart, precedLen) ;
        savedPos -= precedLen ;
    }

    if (action == 2)
    {
        // Determine the "before" command
        std::string beforeCmd ;
        if (currentJust == align)
        {
            // Turning off inherited alignment
            beforeCmd = AlignmentToDotCommand(JUST_LEFT) ;
        }
        else
        {
            beforeCmd = AlignmentToDotCommand(align) ;
        }

        // Insert before command at start of text paragraph
        PARAGRAPH_T newTextPara = mDocument->GetParagraphFromPosition(savedPos) ;
        POSITION_T newParaStart = 0, newParaEnd = 0 ;
        mDocument->GetParagraphStartandEnd(newTextPara, newParaStart, newParaEnd) ;

        mDocument->SetPosition(newParaStart) ;
        mDocument->Insert(beforeCmd) ;
        POSITION_T beforeLen = static_cast<POSITION_T>(beforeCmd.size()) ;
        savedPos += beforeLen ;

        // Insert restoration command after the text paragraph
        // Text paragraph shifted by 1 due to insertion of before command
        PARAGRAPH_T textParaAfterInsert = newTextPara + 1 ;
        POSITION_T textStart = 0, textEnd = 0 ;
        mDocument->GetParagraphStartandEnd(textParaAfterInsert, textStart, textEnd) ;

        std::string afterCmd = AlignmentToDotCommand(nextTextAlign) ;
        mDocument->SetPosition(textEnd + 1) ;
        mDocument->Insert(afterCmd) ;
        // No adjustment to savedPos: insertion is after caret
    }

    mDocument->SetPosition(savedPos) ;

    EndBatchUpdate() ;
    mDocument->EndUndoGroup() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] font name/specification string
///
/// @return nothing
///
/// @brief
/// Set the base font for the editor.
/// Stores the font specification string, propagates it to the layout,
/// and triggers a full relayout since font changes affect all measurements.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetFont(const std::string& font)
{
    mBaseFont = font;

    // Propagate font to layout and trigger full relayout
    if (mLayout && mDocument)
    {
        mLayout->SetDefaultFont(font);
        mLayout->LayoutDocument(mDocument);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - Base font specification
///
/// @brief
/// Get the base font specification string.
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorBase::GetFont(void) const
{
    return mBaseFont;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  show [in] control display mode (SHOW_ALL, SHOW_DOT, SHOW_NONE)
///
/// @return nothing
///
/// @brief
/// Set control character display mode. Updates both display settings
/// and layout if available. Changes trigger relayout.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetShowControls(eShowControl show)
{
    mDisplaySettings.showControl = show;

    // Sync to document (document's GetChar() uses mShowControl to determine
    // whether to return control codes or skip them)
    if (mDocument)
    {
        mDocument->SetShowControl(show);
    }

    if (mLayout)
    {
        mLayout->SetShowControl(show);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return eShowControl - Current control display mode
///
/// @brief
/// Get the current control character display mode.
///
/////////////////////////////////////////////////////////////////////////////
eShowControl cEditorBase::GetShowControls(void) const
{
    return mDisplaySettings.showControl;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  show [in] true to show dot commands, false to hide
///
/// @return nothing
///
/// @brief
/// Set whether dot commands are visible.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetShowDot(bool show)
{
    mShowDot = show;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if dot commands are visible
///
/// @brief
/// Get the dot command visibility flag.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetShowDot(void) const
{
    return mShowDot;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  measure [in] measurement string (e.g., "1\"" for inches)
///
/// @return nothing
///
/// @brief
/// Set measurement system based on string format.
/// Parses measurement string and sets both string and enum values.
/// Supports: " for inches, C for centimeters, M for millimeters.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetMeasurement(const std::string& measure)
{
    mMeasurement = measure;

    if (!mDocument)
    {
        mMeasure = MSR_INCHES;
        return;
    }

    char type = mDocument->GetType(measure);

    switch (type)
    {
        case '\"':
        {
            mMeasure = MSR_INCHES;
            break;
        }

        case 'C':
        {
            mMeasure = MSR_CENTIMETERS;
            break;
        }

        case 'M':
        {
            mMeasure = MSR_MILLIMETERS;
            break;
        }

        default:
        {
            mMeasure = MSR_INCHES;
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string - Measurement string
///
/// @brief
/// Get the measurement string.
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorBase::GetMeasurement(void) const
{
    return mMeasurement;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] - Code page to set
///
/// @return nothing
///
/// @brief
/// Set the code page of the file (WordStar file format only).
/// [REFACTORED from editorbase.cpp::SetCodePage()]
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetCodePage(eCodePage page)
{
    mCodePage = page;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return eCodePage - Current codepage
///
/// @brief
/// Get the codepage for the file (WordStar file format only).
/// [REFACTORED from editorbase.cpp::GetCodePage()]
///
/////////////////////////////////////////////////////////////////////////////
eCodePage cEditorBase::GetCodePage(void)
{
    return mCodePage;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if dot command text should be spell checked
///
/// @brief
/// Get whether spell check should include text content of dot command
/// and comment lines.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetSpellCheckDotCommands(void) const
{
    return mSpellCheckDotCommands;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool val [in] true to enable spell checking dot command text
///
/// @return nothing
///
/// @brief
/// Set whether spell check should include text content of dot command
/// and comment lines.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetSpellCheckDotCommands(bool val)
{
    mSpellCheckDotCommands = val;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  COORD_T yTwips [in] Y coordinate in document space (with scroll offset)
///
/// @return LINE_T - raw line number (every laid-out row, includes dot commands) at that Y, or NOT_SET if no layout
///
/// @brief
/// Find which line is at the given Y coordinate.
/// Y coordinate should be in document space (including scroll offset).
///
/// This method uses BLACK BOX API (GetParagraphLayout, paragraph line data).
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cEditorBase::LineFromY(COORD_T yTwips)
{
    if (!mLayout)
    {
        return NOT_SET;
    }

    // Linear search through all lines in layout
    // We iterate through actual line objects in paragraph layout, not by line number,
    // because GetLineByRawLineNumber() may return NULL for lines not yet laid out.
    LINE_T bestLine = NOT_SET;
    COORD_T bestDistance = std::numeric_limits<COORD_T>::max();
    LINE_T currentLineNumber = 0;

    // In page mode, docY is in pagey + currentPageYOffset space,
    // so we must compare against pagey + pageOffset instead of screeny
    bool pageMode = (mDisplaySettings.mode == DISPLAY_PAGE);
    COORD_T paperHeight = 0;
    COORD_T pageGap = 0;
    if (pageMode)
    {
        paperHeight = mLayout->GetPaperHeight();
        pageGap = mDisplaySettings.pageGap;
    }

    // Iterate through all paragraphs and their lines
    for (PARAGRAPH_T para = 0; para < mLayout->GetNumberOfParagraphs(); para++)
    {
        const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para);
        if (!paraLayout)
        {
            continue;
        }

        for (const auto& line : paraLayout->lines)
        {
            COORD_T lineTop;
            if (pageMode)
            {
                // Convert pagey to document-absolute Y in page mode coordinate space
                COORD_T pageOffset = (line.pagenumber - 1) * (paperHeight + pageGap);
                lineTop = line.pagey + pageOffset;
            }
            else
            {
                lineTop = line.screeny;
            }
            COORD_T lineBottom = lineTop + line.lineheight;

            // Check if Y is within this line
            if (yTwips >= lineTop && yTwips < lineBottom)
            {
                return currentLineNumber;
            }

            // Track closest line in case we don't find exact match
            COORD_T distance = std::min(std::abs(yTwips - lineTop), std::abs(yTwips - lineBottom));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestLine = currentLineNumber;
            }

            currentLineNumber++;
        }
    }

    // Return closest line if we didn't find exact match
    return bestLine;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Caret X coordinate in twips
///
/// @brief
/// Returns the caret X coordinate (platform-independent).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::GetCaretX(void) const
{
    return mCaretX;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Caret Y coordinate in twips
///
/// @brief
/// Returns the caret Y coordinate (platform-independent).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::GetCaretY(void) const
{
    return mCaretY;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Caret width in twips
///
/// @brief
/// Returns the caret width (platform-independent).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::GetCaretWidth(void) const
{
    return mCaretWidth;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Caret height in twips
///
/// @brief
/// Returns the caret height (platform-independent).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::GetCaretHeight(void) const
{
    return mCaretHeight;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if caret is currently drawn
///
/// @brief
/// Returns whether the caret is currently visible (drawn state).
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetDrawnCaret(void) const
{
    return mDrawnCaret;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if caret should be drawn
///
/// @brief
/// Returns whether the caret should be drawn (one-shot flag for timer).
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetDoDrawCaret(void) const
{
    return mDoDrawCaret;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  drawn [in] true if caret is currently drawn
///
/// @return nothing
///
/// @brief
/// Sets whether the caret is currently visible (drawn state).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetDrawnCaret(bool drawn)
{
    mDrawnCaret = drawn;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  doDraw [in] true if caret should be drawn
///
/// @return nothing
///
/// @brief
/// Sets whether the caret should be drawn (one-shot flag for timer).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetDoDrawCaret(bool doDraw)
{
    mDoDrawCaret = doDraw;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return POSITION_T - Current document position
///
/// @brief
/// Returns the caret's current document position (grapheme index).
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorBase::GetCaretDocumentPosition(void) const
{
    return mCaretDocumentPosition;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return LINE_T - Current line number
///
/// @brief
/// Returns the caret's current line number.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cEditorBase::GetCaretLine(void) const
{
    return mCaretLine;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return PARAGRAPH_T - Current paragraph number
///
/// @brief
/// Returns the caret's current paragraph number.
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cEditorBase::GetCaretParagraph(void) const
{
    return mCaretParagraph;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Sticky X coordinate
///
/// @brief
/// Returns the sticky X coordinate (for vertical movement).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::GetCaretStickyX(void) const
{
    return mCaretStickyX;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return PAGE_T - Page number the caret is on
///
/// @brief
/// Returns the page number cached by CalculateCaretPosition.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cEditorBase::GetCaretPageNumber(void) const
{
    return mCaretPageNumber;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Page-relative Y position in twips
///
/// @brief
/// Returns the caret line's pagey value cached by CalculateCaretPosition.
/// Measured from top of paper (includes top margin).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::GetCaretPageY(void) const
{
    return mCaretPageY;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return POSITION_T - Document position where the caret's line starts
///
/// @brief
/// Returns the line start position cached by CalculateCaretPosition.
/// Used to compute column number (caret position minus line start).
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorBase::GetCaretLineDocPosition(void) const
{
    return mCaretLineDocPosition;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return const sDisplaySettings& - Display settings
///
/// @brief
/// Returns the current display settings.
///
/////////////////////////////////////////////////////////////////////////////
const sDisplaySettings& cEditorBase::GetDisplaySettings(void) const
{
    return mDisplaySettings;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  show [in] true to show viewport debug overlay
///
/// @return nothing
///
/// @brief
/// Enables/disables viewport debug overlay and triggers repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetShowViewportDebug(bool show)
{
    if (mShowViewportDebug != show)
    {
        mShowViewportDebug = show;
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  show [in] true to show box statistics overlay
///
/// @return nothing
///
/// @brief
/// Enables/disables box statistics overlay and triggers repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetShowBoxStats(bool show)
{
    if (mShowBoxStats != show)
    {
        mShowBoxStats = show;
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  show [in] true to show box outlines overlay
///
/// @return nothing
///
/// @brief
/// Enables/disables box outlines overlay and triggers repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetShowBoxOutlines(bool show)
{
    if (mShowBoxOutlines != show)
    {
        mShowBoxOutlines = show;
        Repaint();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if viewport debug overlay is enabled
///
/// @brief
/// Returns whether viewport debug overlay is enabled.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetShowViewportDebug(void) const
{
    return mShowViewportDebug;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if box statistics overlay is enabled
///
/// @brief
/// Returns whether box statistics overlay is enabled.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetShowBoxStats(void) const
{
    return mShowBoxStats;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if box outlines overlay is enabled
///
/// @brief
/// Returns whether box outlines overlay is enabled.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::GetShowBoxOutlines(void) const
{
    return mShowBoxOutlines;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  start [in] start position for word count
/// @param  end [in] end position for word count (0 = entire document)
///
/// @return long - number of words counted
///
/// @brief
/// Count words in a range of text. If end == 0, counts entire document
/// (skipping dot command paragraphs). Treats punctuation as word
/// separators but preserves apostrophes (closer to MS Word behavior).
///
/////////////////////////////////////////////////////////////////////////////
long cEditorBase::WordCount(POSITION_T start, POSITION_T end)
{
    if (!mDocument)
    {
        return 0;
    }

    std::string text;

    // Punctuation characters treated as word separators
    // (do not break apostrophes -- closer to MS Word word count)
    std::string punctuation = "\\.()[]{},;:?+-*=/&~|\"<>!\r\n\t";
    punctuation += static_cast<char>(HARD_RETURN);

    if (end == 0)
    {
        // Count entire document
        ssize_t paras = mDocument->GetNumberofParagraphs();
        for (ssize_t loop = 0; loop < paras; loop++)
        {
            std::string paraText = mDocument->GetParagraphText(loop);
            if (!paraText.empty())
            {
                // Skip dot command paragraphs
                if (paraText[0] == '.')
                {
                    continue;
                }
                text += paraText;
            }
        }
    }
    else
    {
        // Count block
        text = mDocument->GetBlockText(start, end);
    }

    // Replace punctuation with spaces
    for (size_t i = 0; i < text.size(); i++)
    {
        unsigned char ch = static_cast<unsigned char>(text[i]);
        if (punctuation.find(static_cast<char>(ch)) != std::string::npos)
        {
            text[i] = ' ';
        }
    }

    // Count words by counting space-to-non-space transitions
    bool space = false;
    long wordcount = 1;

    for (char c : text)
    {
        if (c == ' ' && space == false)
        {
            space = true;
            ++wordcount;
        }
        else if (c != ' ')
        {
            space = false;
        }
    }

    return wordcount;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Start the background word count timer thread. The thread wakes every
/// second and fires a word count update every WORD_COUNT_INTERVAL_MS
/// (5 seconds). Safe to call multiple times (no-op if already running).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::StartWordCountTimer(void)
{
    if (mWordCountTimerRunning.load())
    {
        return;
    }

    mWordCountTimerRunning.store(true);
    mWordCountTimerThread = std::thread(&cEditorBase::WordCountTimerThreadFunc, this);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Stop the background word count timer thread and wait for it to finish.
/// Safe to call multiple times (no-op if not running).
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::StopWordCountTimer(void)
{
    mWordCountTimerRunning.store(false);
    if (mWordCountTimerThread.joinable())
    {
        mWordCountTimerThread.join();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  sStatus& status [out] - Status structure to populate
///
/// @return nothing
///
/// @brief
/// Populate status structure with current editor state.
/// Used by UI status bar to display editor information.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::GetStatus(sStatus& status)
{
    // File information
    status.filename = mFileName;

    // Column -- count visible graphemes only (control codes excluded, tabs counted)
    long visualCol = 1;
    if (mDocument)
    {
        for (POSITION_T pos = mCaretLineDocPosition; pos < mCaretDocumentPosition; ++pos)
        {
            eModifiers ctrl = mDocument->GetControlChar(pos);
            if (ctrl == STYLE_END_OF_STYLES || ctrl == STYLE_TAB)
            {
                visualCol++;
            }
        }
    }
    status.column = visualCol;
    status.line = mCaretPageLineNumber + 1;  // Per-page line number (resets at each page break)

    // Page information (cached by CalculateCaretPosition from line->pagenumber)
    status.page = mCaretPageNumber;
    if (mLayout)
    {
        status.pagecount = mLayout->GetNumberOfPages();
    }
    else
    {
        status.pagecount = 0;
    }

    // Editor mode
    status.mode = mInsertMode;
    // Command-tags/show-control indicator: on when formatting marks are fully
    // revealed (^OD -> SHOW_ALL). Drives the GUI and wstui top-status "*".
    status.showcontrol = (mLayout != nullptr) && (mLayout->GetShowControl() == SHOW_ALL);
    status.saving = false;

    // Document statistics
    if (mDocument)
    {
        status.charcount = mDocument->GetTextSize();
        status.wordcount = mLastWordCount;  // Updated by OnWordCountTimer() every 5 seconds
    }
    else
    {
        status.charcount = 0;
        status.wordcount = 0;
    }

    // Help display mode
    status.help = mHelpDisplay;

    // Text attributes - return cached values from ParseFontDescriptor()
    status.attrib = false;
    status.bold = mStatusBold;
    status.italic = mStatusItalic;
    status.underline = mStatusUnderline;

    // Justification - return cached value from ParseFontDescriptor()
    status.just = mStatusJust;

    // Style and font - return cached values from ParseFontDescriptor()
    status.style = "";  // TODO: Style support in future phase
    status.font = mStatusFont;

    // V position -- page-relative (from top of paper, includes top margin).
    // On printable lines, update the cached V; on dot command/comment lines,
    // retain the last known V from the nearest printable line.
    // H position -- horizontal caret offset from left edge (0 on non-printable lines).
    if (mCaretOnPrintableLine)
    {
        mLastPrintablePageY = mCaretPageY;
    }
    COORD_T vTwips = mLastPrintablePageY;
    COORD_T hTwips = mCaretOnPrintableLine ? mCaretPrintX : 0;

    switch (mMeasure)
    {
        case MSR_INCHES:
        {
            status.vPosition = static_cast<double>(vTwips) / TWIPSPERINCH;
            status.hPosition = static_cast<double>(hTwips) / TWIPSPERINCH;
            break;
        }
        case MSR_CENTIMETERS:
        {
            status.vPosition = static_cast<double>(vTwips) / TWIPSPERCM;
            status.hPosition = static_cast<double>(hTwips) / TWIPSPERCM;
            break;
        }
        case MSR_MILLIMETERS:
        {
            status.vPosition = static_cast<double>(vTwips) / TWIPSPERMM;
            status.hPosition = static_cast<double>(hTwips) / TWIPSPERMM;
            break;
        }
        default:
        {
            status.vPosition = static_cast<double>(vTwips) / TWIPSPERINCH;
            status.hPosition = static_cast<double>(hTwips) / TWIPSPERINCH;
            break;
        }
    }

    // Measurement suffix (always set regardless of printable state)
    switch (mMeasure)
    {
        case MSR_INCHES:
        {
            status.measureSuffix = "\"";
            break;
        }
        case MSR_CENTIMETERS:
        {
            status.measureSuffix = " cm";
            break;
        }
        case MSR_MILLIMETERS:
        {
            status.measureSuffix = " mm";
            break;
        }
        default:
        {
            status.measureSuffix = "\"";
            break;
        }
    }

    // Block selection state
    status.blockSet = false;
    if (mDocument)
    {
        POSITION_T blockStart = 0;
        POSITION_T blockEnd = 0;
        status.blockSet = mDocument->GetBlock(blockStart, blockEnd);
    }

    // Background layout busy state
    status.backgroundBusy = mLayoutInt || mLayoutRest;

    // Place markers -- show digits of active markers (1-9 then 0)
    status.markers.clear();
    if (mDocument)
    {
        for (int i = 1; i <= 9; ++i)
        {
            if (mDocument->mSavePosition[i] != NOT_SET)
            {
                status.markers += std::to_string(i);
            }
        }
        if (mDocument->mSavePosition[0] != NOT_SET)
        {
            status.markers += '0';
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  msg [in] - Message text to display
/// @param  durationFrames [in] - Number of TickStatusMessage() calls before auto-clear (default 60)
///
/// @return nothing
///
/// @brief
/// Set a temporary status message displayed in the status bar.
/// The message auto-clears after durationFrames ticks.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SetStatusMessage(const std::string& msg, bool busy, int durationFrames)
{
    mStatusMessage = msg;
    mStatusBusy = busy;
    mStatusMessageTimer = durationFrames;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return std::string the current status message (empty if none)
///
/// @brief
/// Get the current temporary status message text.
///
/////////////////////////////////////////////////////////////////////////////
std::string cEditorBase::GetStatusMessage(void) const
{
    return mStatusMessage;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bool - true if the status bar busy indicator should be shown
///
/// @brief
/// Check if the status bar busy indicator should be displayed.
/// Set by SetStatusMessage() with busy=true, cleared when the
/// status message timer expires or explicitly cleared.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::IsStatusBusy(void) const
{
    return mStatusBusy;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Decrement the status message timer. When it reaches zero,
/// clear the message text and busy flag. Called once per frame/tick by the UI.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::TickStatusMessage(void)
{
    if (mStatusMessageTimer > 0)
    {
        mStatusMessageTimer--;
        if (mStatusMessageTimer == 0)
        {
            mStatusMessage.clear();
            mStatusBusy = false;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in] line layout data for the caret's line
///
/// @return nothing
///
/// @brief
/// In page mode, converts caret Y from continuous (screeny) coordinates
/// to page-relative (pagey + page offset) coordinates. This is needed
/// because page mode renders text at pagey + currentPageYOffset, not at
/// screeny. No effect in continuous mode.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::AdjustCaretYForPageMode(const sLineLayout* line)
{
    if (mDisplaySettings.mode != DISPLAY_PAGE || !line || !mLayout)
    {
        return;
    }

    COORD_T paperHeight = mLayout->GetPaperHeight();
    COORD_T pageGap = mDisplaySettings.pageGap;
    COORD_T currentPageYOffset = (line->pagenumber - 1) * (paperHeight + pageGap);
    mCaretY = line->pagey + currentPageYOffset - mScrollOffset;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in] line layout data
///
/// @return viewport-relative Y coordinate for the line
///
/// @brief
/// Converts a line's Y position to viewport-relative coordinates.
/// In continuous mode, uses screeny. In page mode, uses pagey + page offset.
/// This is the single source of truth for line-to-viewport Y conversion.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cEditorBase::ScreenYToViewportY(const sLineLayout* line) const
{
    if (mDisplaySettings.mode == DISPLAY_PAGE && line && mLayout)
    {
        COORD_T paperHeight = mLayout->GetPaperHeight();
        COORD_T pageGap = mDisplaySettings.pageGap;
        COORD_T currentPageYOffset = (line->pagenumber - 1) * (paperHeight + pageGap);
        return line->pagey + currentPageYOffset - mScrollOffset;
    }

    if (line)
    {
        return line->screeny - mScrollOffset;
    }

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Sync sibling editor's caret position after CalculateCaretPosition().
/// Used for reveal codes feature -- keeps both editors' carets in sync.
/// Does NOT modify the shared document position.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SyncSiblingCaret(void)
{
    if (mSiblingEditor != nullptr && !mSyncingCaret)
    {
        mSiblingEditor->mSyncingCaret = true ;
        mSiblingEditor->mInsertMode = mInsertMode ;      // sync insert/overwrite mode
        mSiblingEditor->CalculateCaretPosition() ;  // find caret location (for ScrollIntoView)
        mSiblingEditor->ScrollIntoView() ;           // may change mScrollOffset
        mSiblingEditor->CalculateCaretPosition() ;  // recompute mCaretY with new scroll offset
        mSiblingEditor->Repaint() ;
        mSiblingEditor->StartCaretTimer(1) ;         // trigger caret draw promptly
        mSiblingEditor->mSyncingCaret = false ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  segment [in] the resolved segment containing the caret
/// @param  graphemeIndex [in] grapheme index within the segment (or count for end-of-line)
/// @param  atEndOfLine [in] true if caret is at/past end of line
///
/// @return nothing
///
/// @brief
/// Virtual hook called from CalculateCaretPosition when the caret's
/// segment position is resolved. Subclasses may override to act on
/// the resolved position (e.g. compute column). Base class does nothing.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::OnCaretSegmentResolved(const sLineLayout& /*line*/,
                                         size_t /*segmentIndex*/,
                                         const sSegmentLayout& /*segment*/,
                                         size_t /*graphemeIndex*/,
                                         bool /*atEndOfLine*/)
{
    // Default: nothing. Subclasses may override.
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos [in] document position to check
/// @param  direction [in] direction of movement (-1 = left, +1 = right)
///
/// @return adjusted position that skips over all hidden content
///
/// @brief
/// Adjusts caret position to skip over hidden content (SHOW_DOT/SHOW_NONE).
/// Handles both hidden paragraphs (dot commands/comments) and hidden
/// inline control codes (bold, italic, font, etc.).
///
/// In SHOW_ALL mode, nothing is hidden so this returns pos unchanged.
/// In SHOW_DOT mode, comments are hidden paragraphs and control codes are hidden.
/// In SHOW_NONE mode, all dot commands/comments are hidden and control codes are hidden.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorBase::SkipOverHiddenContent(POSITION_T pos, int direction)
{
    // Guard: need layout and document
    if (!mLayout || !mDocument)
    {
        return pos;
    }

    // When SHOW_ALL is active, all content is visible -- no skipping needed
    // Use the layout's show control (the active state), not the editor's cached
    // user preference -- page mode overrides show control to SHOW_NONE in the layout
    if (mLayout->GetShowControl() == SHOW_ALL)
    {
        return pos;
    }

    // Skip hidden paragraphs first, then hidden control codes at the result
    pos = SkipHiddenParagraphs(pos, direction);
    pos = SkipHiddenControlCodes(pos, direction);

    return pos;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string filepath [in] - full path to the document file
///
/// @return nothing
///
/// @brief
/// Load per-file editor state from a companion .ws-<filename> INI file.
/// Restores cursor position, 10 saved positions, block selection,
/// display mode (page vs continuous), and reveal codes pane visibility.
/// Silently does nothing if the companion file doesn't exist.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::LoadFileState(const std::string& filepath)
{
    if (filepath.empty() || !mDocument)
    {
        return;
    }

    // Build companion file path: dir/.ws-filename
    std::filesystem::path path(filepath);
    std::filesystem::path companion = path.parent_path() / (".ws-" + path.filename().string());

    CSimpleIniA ini;
    if (ini.LoadFile(companion.string().c_str()) < 0)
    {
        // Companion file doesn't exist or can't be read
        return;
    }

    // Restore cursor position
    POSITION_T pos = ini.GetLongValue("internal", "cursor", 0);
    mDocument->SetPosition(pos);

    // Restore 10 saved positions
    mDocument->mSavePosition[0] = ini.GetLongValue("internal", "save1", NOT_SET);
    mDocument->mSavePosition[1] = ini.GetLongValue("internal", "save2", NOT_SET);
    mDocument->mSavePosition[2] = ini.GetLongValue("internal", "save3", NOT_SET);
    mDocument->mSavePosition[3] = ini.GetLongValue("internal", "save4", NOT_SET);
    mDocument->mSavePosition[4] = ini.GetLongValue("internal", "save5", NOT_SET);
    mDocument->mSavePosition[5] = ini.GetLongValue("internal", "save6", NOT_SET);
    mDocument->mSavePosition[6] = ini.GetLongValue("internal", "save7", NOT_SET);
    mDocument->mSavePosition[7] = ini.GetLongValue("internal", "save8", NOT_SET);
    mDocument->mSavePosition[8] = ini.GetLongValue("internal", "save9", NOT_SET);
    mDocument->mSavePosition[9] = ini.GetLongValue("internal", "save10", NOT_SET);

    // Format migration: saved positions of 0 become NOT_SET (old format compatibility)
    for (int loop = 0; loop < 10; loop++)
    {
        if (mDocument->mSavePosition[loop] == 0)
        {
            mDocument->mSavePosition[loop] = NOT_SET;
        }
    }

    // Restore block selection state
    mDocument->mStartBlock = ini.GetLongValue("internal", "blockstart", NOT_SET);
    mDocument->mEndBlock = ini.GetLongValue("internal", "blockend", NOT_SET);
    mDocument->mBlockSet = ini.GetBoolValue("internal", "blockactive", false);

    // Format migration: block positions of 0 become NOT_SET when block is inactive
    if (mDocument->mBlockSet == false)
    {
        if (mDocument->mStartBlock == 0)
        {
            mDocument->mStartBlock = NOT_SET;
        }
        if (mDocument->mEndBlock == 0)
        {
            mDocument->mEndBlock = NOT_SET;
        }
    }

    // Restore display mode (0 = continuous, 1 = page)
    int displayMode = static_cast<int>(ini.GetLongValue("internal", "displayMode", static_cast<int>(DISPLAY_CONTINUOUS)));
    SetDisplayMode(static_cast<eDisplayMode>(displayMode));

    // Restore reveal codes pane visibility flag
    // (the caller is responsible for actually opening/closing the pane)
    mRevealCodesVisible = ini.GetBoolValue("internal", "revealCodes", false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string filepath [in] - full path to the document file
///
/// @return nothing
///
/// @brief
/// Save per-file editor state to a companion .ws-<filename> INI file.
/// Saves cursor position, 10 saved positions, block selection,
/// display mode (page vs continuous), and reveal codes pane visibility.
/// Silently does nothing if the companion file can't be written.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::SaveFileState(const std::string& filepath)
{
    if (filepath.empty() || !mDocument)
    {
        return;
    }

    // Build companion file path: dir/.ws-filename
    std::filesystem::path path(filepath);
    std::filesystem::path companion = path.parent_path() / (".ws-" + path.filename().string());

    CSimpleIniA ini;

    // Load existing companion file (preserves any other sections)
    ini.LoadFile(companion.string().c_str());

    // Save cursor position
    POSITION_T pos = mDocument->GetPosition();
    ini.SetLongValue("internal", "cursor", pos);

    // Save 10 saved positions
    ini.SetLongValue("internal", "save1", mDocument->mSavePosition[0]);
    ini.SetLongValue("internal", "save2", mDocument->mSavePosition[1]);
    ini.SetLongValue("internal", "save3", mDocument->mSavePosition[2]);
    ini.SetLongValue("internal", "save4", mDocument->mSavePosition[3]);
    ini.SetLongValue("internal", "save5", mDocument->mSavePosition[4]);
    ini.SetLongValue("internal", "save6", mDocument->mSavePosition[5]);
    ini.SetLongValue("internal", "save7", mDocument->mSavePosition[6]);
    ini.SetLongValue("internal", "save8", mDocument->mSavePosition[7]);
    ini.SetLongValue("internal", "save9", mDocument->mSavePosition[8]);
    ini.SetLongValue("internal", "save10", mDocument->mSavePosition[9]);

    // Save block selection state
    ini.SetLongValue("internal", "blockstart", mDocument->mStartBlock);
    ini.SetLongValue("internal", "blockend", mDocument->mEndBlock);
    ini.SetBoolValue("internal", "blockactive", mDocument->mBlockSet);

    // Save display mode (0 = continuous, 1 = page)
    ini.SetLongValue("internal", "displayMode", static_cast<int>(GetDisplayMode()));

    // Save reveal codes pane visibility
    ini.SetBoolValue("internal", "revealCodes", mRevealCodesVisible);

    // Write companion file
    ini.SaveFile(companion.string().c_str());

#ifdef _WIN32
    // On Windows, set companion file as hidden
    std::string compStr = companion.string();
    size_t csize = compStr.length() + 1;
    wchar_t* fileLPCWSTR = new wchar_t[csize];
    mbstowcs(fileLPCWSTR, compStr.c_str(), csize);

    int attr = GetFileAttributes(fileLPCWSTR);
    if ((attr & FILE_ATTRIBUTE_HIDDEN) == 0)
    {
        SetFileAttributes(fileLPCWSTR, attr | FILE_ATTRIBUTE_HIDDEN);
    }

    delete[] fileLPCWSTR;
#endif
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in/out] caret calculation state
///
/// @return true if caret position is fully resolved (caller should return)
///
/// @brief
/// Finds the paragraph containing the caret and tracks dot command
/// visibility. When the caret moves to a new paragraph, re-layouts the
/// old paragraph if it was a dot command (so it collapses back to hidden).
/// Returns true on invalid paragraph (sets safe defaults).
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::LookupCaretParagraph(sCaretCalcState& state)
{
    state.para = mDocument->GetParagraphFromPosition(state.caretPos) ;

    // Validate paragraph number
    if (state.para < 0 || state.para >= mLayout->GetNumberOfParagraphs())
    {
        // Invalid paragraph -- use safe default (first line)
        LINE_T firstLine = mLayout->GetFirstLineOfParagraph(0) ;
        if (firstLine >= 0)
        {
            mCaretX = mLayout->GetLineBaseX(firstLine) ;
            mCaretY = mLayout->GetLineScreenY(firstLine) - mScrollOffset ;
            mCaretWidth = DEFAULT_CARET_WIDTH ;
            mCaretHeight = mLayout->GetLineHeight(firstLine) ;
            AdjustCaretYForPageMode(mLayout->GetLineByRawLineNumber(firstLine)) ;
        }
        SyncSiblingCaret() ;
        return true ;
    }

    // Track active paragraph changes for dot command visibility
    // When caret leaves a dot command paragraph, re-layout it to hide it
    if (state.para != mCaretParagraph)
    {
        PARAGRAPH_T oldPara = mCaretParagraph ;

        // Update active paragraph in layout before re-layout
        mLayout->SetActiveParagraph(state.para) ;

        // If old paragraph was a dot command, re-layout to hide it
        // Guard: oldPara may be invalid during shutdown or after document changes
        const sParagraphLayout* oldLayout = nullptr ;
        if (oldPara >= 0 && oldPara < mLayout->GetNumberOfParagraphs() &&
            oldPara < mDocument->GetNumberofParagraphs())
        {
            oldLayout = mLayout->GetParagraphLayout(oldPara) ;
        }
        if (oldLayout && (oldLayout->isCommand || oldLayout->isComment))
        {
            mLayout->LayoutParagraph(oldPara) ;
            // Reset background layout position to re-process screeny cascade
            if (oldPara < mLayoutParagraph)
            {
                mLayoutParagraph = oldPara ;
            }
        }
    }
    else if (mLayout->GetActiveParagraph() < 0)
    {
        // First call -- initialize the active paragraph
        mLayout->SetActiveParagraph(state.para) ;
    }
    mCaretParagraph = state.para ;

    return false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in/out] caret calculation state
///
/// @return true if caret position is fully resolved (caller should return)
///
/// @brief
/// If the caret's paragraph has no visible lines (hidden dot command or
/// comment), searches forward then backward for the nearest visible
/// paragraph and uses its first/last line for caret coordinates.
/// Sets state.paraLayout on success.
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::HandleHiddenParagraph(sCaretCalcState& state)
{
    state.paraLayout = mLayout->GetParagraphLayout(state.para) ;

    if (state.paraLayout && !state.paraLayout->lines.empty())
    {
        // Paragraph has visible lines -- not hidden
        return false ;
    }

    // Paragraph has no visible lines (hidden dot command/comment)
    // Search forward for nearest visible paragraph
    for (PARAGRAPH_T s = state.para + 1 ; s < mLayout->GetNumberOfParagraphs() ; s++)
    {
        const sParagraphLayout* sp = mLayout->GetParagraphLayout(s) ;
        if (sp && !sp->lines.empty())
        {
            const sLineLayout& fallbackLine = sp->lines.front() ;
            mCaretX = fallbackLine.pagex ;
            mCaretPrintX = fallbackLine.pagex ;
            mCaretY = fallbackLine.screeny - mScrollOffset ;
            mCaretWidth = DEFAULT_CARET_WIDTH ;
            mCaretHeight = fallbackLine.lineheight ;
            mCaretLine = fallbackLine.contentLineNumber ;
            mCaretPageNumber = fallbackLine.pagenumber ;
            mCaretPageY = fallbackLine.pagey ;
            mCaretLineDocPosition = fallbackLine.documentPosition ;
            mCaretOnPrintableLine = fallbackLine.isPrintable ;
            mCaretPageLineNumber = fallbackLine.pageLineNumber ;
            AdjustCaretYForPageMode(&fallbackLine) ;
            SyncSiblingCaret() ;
            return true ;
        }
    }

    // Search backward for nearest visible paragraph
    for (PARAGRAPH_T s = state.para - 1 ; s >= 0 ; s--)
    {
        const sParagraphLayout* sp = mLayout->GetParagraphLayout(s) ;
        if (sp && !sp->lines.empty())
        {
            const sLineLayout& fallbackLine = sp->lines.back() ;
            mCaretX = fallbackLine.pagex ;
            mCaretPrintX = fallbackLine.pagex ;
            mCaretY = fallbackLine.screeny - mScrollOffset ;
            mCaretWidth = DEFAULT_CARET_WIDTH ;
            mCaretHeight = fallbackLine.lineheight ;
            mCaretLine = fallbackLine.contentLineNumber ;
            mCaretPageNumber = fallbackLine.pagenumber ;
            mCaretPageY = fallbackLine.pagey ;
            mCaretLineDocPosition = fallbackLine.documentPosition ;
            mCaretOnPrintableLine = fallbackLine.isPrintable ;
            mCaretPageLineNumber = fallbackLine.pageLineNumber ;
            AdjustCaretYForPageMode(&fallbackLine) ;
            SyncSiblingCaret() ;
            return true ;
        }
    }

    // No visible paragraphs at all
    mCaretX = 0 ;
    mCaretPrintX = 0 ;
    mCaretY = 0 ;
    mCaretWidth = DEFAULT_CARET_WIDTH ;
    mCaretHeight = DEFAULT_CARET_HEIGHT ;
    mCaretPageNumber = 1 ;
    mCaretPageY = 0 ;
    mCaretLineDocPosition = 0 ;
    mCaretOnPrintableLine = true ;
    mCaretPageLineNumber = 0 ;
    SyncSiblingCaret() ;
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in/out] caret calculation state
///
/// @return true if caret position is fully resolved (caller should return)
///
/// @brief
/// Calculates the grapheme offset within the paragraph and walks the
/// paragraph's lines to find which line contains the caret. Falls back
/// to the last line if the offset exceeds all line ranges. Returns true
/// if the line has no segments (empty line -- caret set to line start).
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::FindCaretLine(sCaretCalcState& state)
{
    // Get paragraph start position in document (graphemes)
    POSITION_T paraEnd = 0 ;
    mDocument->GetParagraphStartandEnd(state.para, state.paraStart, paraEnd) ;

    // Calculate offset within paragraph (graphemes)
    state.offsetInPara = state.caretPos - state.paraStart ;

    // Find which line contains this offset
    state.caretLine = nullptr ;
    for (const auto& line : state.paraLayout->lines)
    {
        // Calculate line's grapheme range
        POSITION_T lineGraphemeCount = 0 ;
        for (const auto& segment : line.segments)
        {
            lineGraphemeCount += segment.GetGraphemeCount() ;
        }

        POSITION_T lineStartInPara = line.linestart ;
        POSITION_T lineEndInPara = lineStartInPara + lineGraphemeCount ;

        // Check if caret falls within this line's range
        if (state.offsetInPara >= lineStartInPara && state.offsetInPara < lineEndInPara)
        {
            state.caretLine = &line ;
            break ;
        }
    }

    // If we didn't find the line, use last line in paragraph
    if (!state.caretLine)
    {
        state.caretLine = &state.paraLayout->lines.back() ;
    }

    // Handle empty line (no segments)
    if (state.caretLine->segments.empty())
    {
        // Empty line -- place caret at line start (no control codes to adjust)
        mCaretX = state.caretLine->pagex ;
        mCaretPrintX = state.caretLine->pagex ;
        mCaretLinePageX = state.caretLine->pagex ;
        mCaretY = state.caretLine->screeny - mScrollOffset ;
        mCaretWidth = DEFAULT_CARET_WIDTH ;
        mCaretHeight = mLayout->GetLineHeight() ;
        mCaretLine = state.caretLine->contentLineNumber ;
        mCaretPageNumber = state.caretLine->pagenumber ;
        mCaretPageY = state.caretLine->pagey ;
        mCaretLineDocPosition = state.caretLine->documentPosition ;
        mCaretOnPrintableLine = state.caretLine->isPrintable ;
        mCaretPageLineNumber = state.caretLine->pageLineNumber ;

        // Parse font descriptor for status bar (clears font info on empty line)
        // Skip on command/comment lines so the status bar retains the last text font
        static const sSegmentLayout emptySegment ;
        if (!state.paraLayout->isCommand && !state.paraLayout->isComment)
        {
            ParseFontDescriptor(emptySegment.font, *state.caretLine) ;
        }

        // Notify subclass hook about empty line caret (no segment data)
        OnCaretSegmentResolved(*state.caretLine, 0, emptySegment, 0, true) ;

        AdjustCaretYForPageMode(state.caretLine) ;
        SyncSiblingCaret() ;
        return true ;
    }

    return false ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in/out] caret calculation state
///
/// @return nothing
///
/// @brief
/// Walks through segments and glyphs on the caret's line to find the
/// X position of the caret. Uses segment.startPosition + i to compare
/// against the caret's paragraph offset (handles hidden control codes
/// correctly). Also calculates glyph width for overwrite mode, parses
/// the font descriptor for the status bar, and fires
/// OnCaretSegmentResolved().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ResolveCaretGlyph(sCaretCalcState& state)
{
    state.caretX = state.caretLine->pagex ;  // Default to line start
    state.caretSegmentHeight = 0 ;
    state.caretGlyphWidth = DEFAULT_CARET_WIDTH ;
    bool foundCaretPos = false ;

    for (size_t segIdx = 0; segIdx < state.caretLine->segments.size(); ++segIdx)
    {
        const auto& segment = state.caretLine->segments[segIdx];
        size_t graphemeCount = segment.GetGraphemeCount() ;
        for (size_t i = 0 ; i < graphemeCount ; ++i)
        {
            // Use segment's actual document position (paragraph-relative) instead
            // of a running counter -- correctly handles hidden control codes that
            // exist in the document but not in layout segments (SHOW_NONE mode)
            POSITION_T graphemeDocPos = segment.startPosition + static_cast<POSITION_T>(i) ;

            // Check if we've reached or passed the caret position
            if (graphemeDocPos >= state.offsetInPara)
            {
                // Found it! Use this glyph's X position and segment height
                state.caretX = state.caretLine->pagex + segment.position[i] ;
                state.caretSegmentHeight = segment.segmentheight ;

                // Calculate glyph width for overwrite mode
                if (i + 1 < graphemeCount)
                {
                    state.caretGlyphWidth = segment.position[i + 1] - segment.position[i] ;
                }
                else
                {
                    // Last glyph in segment -- estimate from total width
                    state.caretGlyphWidth = segment.totalWidth - segment.position[i] ;
                }
                if (state.caretGlyphWidth <= 0)
                {
                    state.caretGlyphWidth = DEFAULT_CARET_WIDTH ;  // Fallback
                }

                // Parse font descriptor for status bar
                // Skip on command/comment lines so the status bar retains the last text font
                if (!state.paraLayout->isCommand && !state.paraLayout->isComment)
                {
                    ParseFontDescriptor(segment.font, *state.caretLine) ;
                }

                // Notify subclass hook with resolved segment position
                OnCaretSegmentResolved(*state.caretLine, segIdx, segment, i, false) ;

                foundCaretPos = true ;
                break ;
            }
        }

        if (foundCaretPos)
        {
            break ;
        }
    }

    // If we walked past all glyphs, caret is at end of line
    if (!foundCaretPos)
    {
        size_t lastSegIdx = state.caretLine->segments.size() - 1;
        const auto& lastSegment = state.caretLine->segments[lastSegIdx] ;
        if (lastSegment.GetGraphemeCount() > 0)
        {
            size_t lastGlyphIdx = lastSegment.GetGraphemeCount() - 1 ;
            state.caretX = state.caretLine->pagex + lastSegment.position[lastGlyphIdx] ;
            // Calculate width of last glyph
            state.caretGlyphWidth = lastSegment.totalWidth - lastSegment.position[lastGlyphIdx] ;
            if (state.caretGlyphWidth <= 0)
            {
                state.caretGlyphWidth = DEFAULT_CARET_WIDTH ;
            }
        }
        state.caretSegmentHeight = lastSegment.segmentheight ;

        // Parse font descriptor for status bar
        // Skip on command/comment lines so the status bar retains the last text font
        if (!state.paraLayout->isCommand && !state.paraLayout->isComment)
        {
            ParseFontDescriptor(lastSegment.font, *state.caretLine) ;
        }

        // Notify subclass hook: caret is one past the last grapheme
        OnCaretSegmentResolved(*state.caretLine, lastSegIdx, lastSegment, lastSegment.GetGraphemeCount(), true) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  state [in] caret calculation state
///
/// @return nothing
///
/// @brief
/// Computes mCaretPrintX by subtracting widths of non-printing control
/// code glyphs (formatting markers like bold/italic toggles) that appear
/// before the caret position. Tabs and variables are NOT subtracted
/// because they occupy printed space.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::CalculatePrintX(const sCaretCalcState& state)
{
    COORD_T controlCodeWidthAdjust = 0 ;
    POSITION_T ccGraphemePos = state.caretLine->linestart ;

    for (const auto& seg : state.caretLine->segments)
    {
        // Tabs print at full expansion width -- no adjustment
        if (seg.isTab)
        {
            ccGraphemePos += seg.GetGraphemeCount() ;
            continue ;
        }

        size_t segGraphemeCount = seg.GetGraphemeCount() ;

        for (size_t ccIdx : seg.controlCodeIndices)
        {
            // Only count control codes before the caret position
            POSITION_T ccPosInPara = ccGraphemePos + static_cast<POSITION_T>(ccIdx) ;
            if (ccPosInPara >= state.offsetInPara)
            {
                break ;
            }

            // Variables print at their expanded width -- skip them
            POSITION_T docPos = state.paraStart + ccPosInPara ;
            std::string ch = mDocument->GetCharNoAdvance(docPos) ;
            if (!ch.empty() && ch[0] == MARKER_CHAR)
            {
                eModifiers controlType = mDocument->GetControlChar(docPos) ;
                if (controlType == STYLE_VARIABLE)
                {
                    continue ;
                }
            }

            // Calculate width of this non-printing control code glyph
            COORD_T ccWidth = 0 ;
            if (ccIdx + 1 < seg.position.size())
            {
                ccWidth = seg.position[ccIdx + 1] - seg.position[ccIdx] ;
            }
            else
            {
                // Last glyph in segment: width extends to segment end
                ccWidth = (seg.position[0] + seg.totalWidth) - seg.position[ccIdx] ;
            }
            controlCodeWidthAdjust += ccWidth ;
        }

        ccGraphemePos += segGraphemeCount ;
    }

    mCaretPrintX = mCaretX - controlCodeWidthAdjust ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in] line structure to search
/// @param  targetX [in] target X coordinate in twips
///
/// @return document position (grapheme index) closest to target X
///
/// @brief
/// Finds the document position in a line closest to target X coordinate.
/// Used for sticky X support when moving caret up/down.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorBase::FindPositionAtX(const sLineLayout* line, COORD_T targetX)
{
    // Guard: need document for position calculations
    if (!mDocument || !line)
    {
        return 0;
    }

    // Handle empty line (no segments)
    if (line->segments.empty())
    {
        return line->documentPosition;
    }

    // Walk through segments to find glyph closest to targetX
    POSITION_T posInLine = 0;
    COORD_T closestX = line->pagex;
    POSITION_T closestPos = 0;
    COORD_T closestDist = std::abs(targetX - closestX);

    for (const auto& segment : line->segments)
    {
        size_t graphemeCount = segment.GetGraphemeCount();
        for (size_t i = 0; i < graphemeCount; ++i)
        {
            COORD_T glyphX = line->pagex + segment.position[i];
            COORD_T dist = std::abs(glyphX - targetX);

            if (dist < closestDist)
            {
                closestX = glyphX;
                closestPos = posInLine;
                closestDist = dist;
            }

            posInLine++;
        }
    }

    // Convert line-relative position to document position
    return line->documentPosition + closestPos;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  startPos [in] starting position to search from
///
/// @return position at end of current word
///
/// @brief
/// Helper function to find the end position of the current word.
/// Used by DeleteWordRight().
///
/// @see cEditorBase::DeleteWordRight
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorBase::FindWordEnd(POSITION_T startPos)
{
    // Guard: need document
    if (!mDocument)
    {
        return startPos;
    }

    // Get text size
    POSITION_T textSize = mDocument->GetTextSize();
    if (startPos >= textSize)
    {
        return startPos;
    }

    // Use document's word end position to find end of current token
    // (word or whitespace run) without including trailing whitespace
    POSITION_T wordEnd = mDocument->GetWordEndPosition(startPos);

    return wordEnd;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  fontDescriptor [in] - Font descriptor string (e.g., "Times New Roman,8.5,-1,50,0,0,0,0,0")
/// @param  line [in] - Line containing the segment (for justification)
///
/// @return nothing
///
/// @brief
/// Parses font descriptor string and caches formatting information for status bar.
/// Called during rendering when a segment containing the caret is drawn.
///
/// Font descriptor format: "family,pointSize,styleHint,weight,italic,underline,..."
/// - Field [0] = family name
/// - Field [1] = point size (e.g., "8.5")
/// - Field [3] = weight (50=normal, 75=bold)
/// - Field [4] = italic (0 or 1)
/// - Field [5] = underline (0 or 1)
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::ParseFontDescriptor(const std::string& fontDescriptor, const sLineLayout& line)
{
    // Get justification from line
    if (line.center)
    {
        mStatusJust = JUST_CENTER;
    }
    else if (line.right)
    {
        mStatusJust = JUST_RIGHT;
    }
    else if (line.justify)
    {
        mStatusJust = JUST_JUST;
    }
    else
    {
        mStatusJust = JUST_LEFT;
    }

    // Parse font descriptor (pipe-delimited format)
    // Format: "FontName|Size|Bold|Italic|Underline|Superscript|Subscript"
    // Example: "Times New Roman|8.5|0|0|0|0|0"
    if (fontDescriptor.empty())
    {
        mStatusFont = "";
        mStatusBold = false;
        mStatusItalic = false;
        mStatusUnderline = false;
        return;
    }

    // Split descriptor on pipes
    std::vector<std::string> fields;
    std::string field;
    for (char ch : fontDescriptor)
    {
        if (ch == '|')
        {
            fields.push_back(field);
            field.clear();
        }
        else
        {
            field += ch;
        }
    }
    fields.push_back(field);  // Add last field

    // Need at least 5 fields to extract all information (name, size, bold, italic, underline)
    if (fields.size() >= 5)
    {
        // Field [0] = family name
        std::string family = fields[0];

        // Field [1] = point size (e.g., "8.5")
        std::string pointSize = fields[1];

        // Field [2] = bold (0 or 1)
        mStatusBold = (fields[2] == "1");

        // Field [3] = italic (0 or 1)
        mStatusItalic = (fields[3] == "1");

        // Field [4] = underline (0 or 1)
        mStatusUnderline = (fields[4] == "1");

        // Format as "Family PointSize" (e.g., "Times New Roman 8.5")
        mStatusFont = family + " " + pointSize;
    }
    else
    {
        // Not enough fields - use defaults
        mStatusFont = "";
        mStatusBold = false;
        mStatusItalic = false;
        mStatusUnderline = false;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Background thread function for periodic word count updates.
/// Sleeps in 100ms increments (checking mWordCountTimerRunning each
/// time for quick shutdown), and updates mLastWordCount every
/// WORD_COUNT_INTERVAL_MS milliseconds.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::WordCountTimerThreadFunc(void)
{
    constexpr int SLEEP_INTERVAL_MS = 100;
    int elapsedMs = 0;

    while (mWordCountTimerRunning.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_INTERVAL_MS));

        if (!mWordCountTimerRunning.load())
        {
            break;
        }

        elapsedMs += SLEEP_INTERVAL_MS;
        if (elapsedMs >= WORD_COUNT_INTERVAL_MS)
        {
            elapsedMs = 0;

            // Don't count during document loading
            if (mDocument && !mDocument->GetLoading())
            {
                mLastWordCount = WordCount(0, 0);

                // Store word count on document for variable expansion
                mDocument->SetWordCount(mLastWordCount);
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Checks if the last 3 characters typed form an &X& variable pattern
/// (e.g., &@& for date, &#& for page number). If so, deletes the 3
/// characters and inserts a variable MARKER_CHAR in their place.
///
/// Called after each character insertion in InsertText().
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::CheckAndReplaceVariable(void)
{
    if (!mDocument)
    {
        return;
    }

    // Need at least 3 characters behind cursor
    POSITION_T pos = mDocument->GetPosition();
    if (pos < 3)
    {
        return;
    }

    // Read the last 3 characters (pos is AFTER the just-inserted char)
    std::string ch3 = mDocument->GetCharNoAdvance(pos - 1);  // closing '&'
    std::string ch2 = mDocument->GetCharNoAdvance(pos - 2);  // variable char
    std::string ch1 = mDocument->GetCharNoAdvance(pos - 3);  // opening '&'

    // Check for &X& pattern
    if (ch1.size() != 1 || ch1[0] != '&')
    {
        return;
    }
    if (ch3.size() != 1 || ch3[0] != '&')
    {
        return;
    }
    if (ch2.size() != 1)
    {
        return;
    }

    // Map variable character to type
    eVariableType varType;
    switch (ch2[0])
    {
        case '@':
        {
            varType = VAR_DATE;
            break;
        }
        case '!':
        {
            varType = VAR_TIME;
            break;
        }
        case '#':
        {
            varType = VAR_PAGE_NUMBER;
            break;
        }
        case '_':
        {
            varType = VAR_LINE_NUMBER;
            break;
        }
        case '*':
        {
            varType = VAR_FILENAME;
            break;
        }
        case ':':
        {
            varType = VAR_DRIVE;
            break;
        }
        case '.':
        {
            varType = VAR_DIRECTORY;
            break;
        }
        case '\\':
        {
            varType = VAR_FULLPATH;
            break;
        }
        case '?':
        {
            varType = VAR_WORD_COUNT;
            break;
        }
        default:
        {
            return;  // Not a valid variable character
        }
    }

    // Variable expansion is its own undo unit.
    CloseTypingGroup();
    mDocument->BeginUndoGroup();
    mDocument->Delete(pos - 3, 3);
    mDocument->SetPosition(pos - 3);
    mDocument->InsertVariable(varType);
    mDocument->EndUndoGroup();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] - the text to check (UTF-8 string, typically 1 grapheme)
///
/// @return true if text is CJK sentence/clause punctuation
///
/// @brief
/// Checks if the given text is CJK punctuation that should close the
/// undo typing group. This provides natural undo boundaries for languages
/// without word-separating spaces (Japanese, Chinese, Korean, Thai).
///
/// Includes: periods, commas, exclamation/question marks, closing quotes,
/// closing brackets - both full-width and ideographic variants.
///
/// This is called from InsertText() to close typing groups on CJK punctuation,
/// complementing the space check for Western languages.
///
/// @see InsertText()
///
/////////////////////////////////////////////////////////////////////////////
bool cEditorBase::IsCJKPunctuation(const std::string& text)
{
    // CJK sentence-ending and clause-ending punctuation
    // These create natural undo boundaries in CJK text
    return text == "。" || text == "．" ||    // Period (ideographic / full-width)
           text == "、" || text == "，" ||    // Comma (ideographic / full-width)
           text == "！" || text == "？" ||    // Exclamation / Question (full-width)
           text == "；" || text == "：" ||    // Semicolon / Colon (full-width)
           text == "」" || text == "』" ||    // Closing quotation marks (CJK)
           text == "）" || text == "】" ||    // Closing brackets (full-width)
           text == "》" || text == "〉" ||    // Closing angle brackets
           text == "\xe2\x80\x99" ||          // Right single quote (U+2019)
           text == "\xe2\x80\x9d";            // Right double quote (U+201D)
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos [in] document position to check
/// @param  direction [in] direction of movement (-1 = left, +1 = right)
///
/// @return adjusted position in a visible paragraph
///
/// @brief
/// If the position is inside a hidden paragraph (dot command or comment
/// with no layout lines), scans in the given direction for the nearest
/// visible paragraph. Falls back to the opposite direction if needed.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorBase::SkipHiddenParagraphs(POSITION_T pos, int direction)
{
    // Save original position in case we need to return it unchanged
    POSITION_T originalPos = pos;

    // Use the minimum of layout and document paragraph counts as safe upper bound
    // During shutdown, layout may have more cached paragraphs than the document
    PARAGRAPH_T maxPara = std::min(mLayout->GetNumberOfParagraphs(),
                                   mDocument->GetNumberofParagraphs());

    // Check if position is inside a hidden paragraph (dot command/comment)
    // Hidden paragraphs have no lines in the layout (lines.empty())
    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(pos);
    if (para < 0 || para >= maxPara)
    {
        return pos;
    }

    const sParagraphLayout* paraLayout = mLayout->GetParagraphLayout(para);
    if (!paraLayout || !paraLayout->lines.empty())
    {
        return pos;  // Paragraph is visible -- no skipping needed
    }

    // Position is in a hidden paragraph -- skip to nearest visible one
    bool found = false;

    if (direction > 0)
    {
        // Scan forward for first visible paragraph
        for (PARAGRAPH_T s = para + 1; s < maxPara; s++)
        {
            const sParagraphLayout* sp = mLayout->GetParagraphLayout(s);
            if (sp && !sp->lines.empty())
            {
                POSITION_T start = 0;
                POSITION_T end = 0;
                mDocument->GetParagraphStartandEnd(s, start, end);
                pos = start;
                found = true;
                break;
            }
        }
    }
    else
    {
        // Scan backward for first visible paragraph
        for (PARAGRAPH_T s = para - 1; s >= 0; s--)
        {
            const sParagraphLayout* sp = mLayout->GetParagraphLayout(s);
            if (sp && !sp->lines.empty())
            {
                // Position at the last content position (before the \r)
                POSITION_T start = 0;
                POSITION_T end = 0;
                mDocument->GetParagraphStartandEnd(s, start, end);
                pos = (end > start) ? end - 1 : start;
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        // No visible paragraph in this direction -- try the other direction
        if (direction > 0)
        {
            for (PARAGRAPH_T s = para - 1; s >= 0; s--)
            {
                const sParagraphLayout* sp = mLayout->GetParagraphLayout(s);
                if (sp && !sp->lines.empty())
                {
                    POSITION_T start = 0;
                    POSITION_T end = 0;
                    mDocument->GetParagraphStartandEnd(s, start, end);
                    pos = (end > start) ? end - 1 : start;
                    found = true;
                    break;
                }
            }
        }
        else
        {
            for (PARAGRAPH_T s = para + 1; s < maxPara; s++)
            {
                const sParagraphLayout* sp = mLayout->GetParagraphLayout(s);
                if (sp && !sp->lines.empty())
                {
                    POSITION_T start = 0;
                    POSITION_T end = 0;
                    mDocument->GetParagraphStartandEnd(s, start, end);
                    pos = start;
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found)
    {
        // No visible paragraphs at all -- return original position
        return originalPos;
    }

    return pos;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos [in] document position to check
/// @param  direction [in] direction of movement (-1 = left, +1 = right)
///
/// @return adjusted position past any hidden control codes
///
/// @brief
/// If the position is on a hidden formatting control code (MARKER_CHAR
/// for bold, italic, font, color, etc.), walks in the given direction
/// until reaching a visible character. HARD_RETURN and BOF/EOF are
/// natural boundaries that stop the walk.
///
/// Functional control codes (TAB, VARIABLE, EOF) are not skipped -- the
/// caret can land on them because they have visible effects.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cEditorBase::SkipHiddenControlCodes(POSITION_T pos, int direction)
{
    POSITION_T textSize = mDocument->GetTextSize();

    // Walk in direction, skipping hidden MARKER_CHAR formatting codes
    while (pos >= 0 && pos < textSize)
    {
        std::string ch = mDocument->GetCharNoAdvance(pos);
        if (ch.empty() || ch[0] != MARKER_CHAR)
        {
            break;  // Normal character or HARD_RETURN -- stop here
        }

        // MARKER_CHAR -- check if this control code is functional
        // TABs and VARIABLEs have visible effects even in SHOW_NONE mode
        // EOF: caret lands ON the EOF marker (insert goes before it)
        eModifiers ctrl = mDocument->GetControlChar(pos);
        if (ctrl == STYLE_TAB || ctrl == STYLE_VARIABLE || ctrl == STYLE_EOF)
        {
            break;  // Functional control codes -- caret can land here
        }

        // Hidden formatting code (bold, italic, font, etc.) -- step past it
        pos += direction;
    }

    // Edge case: walked backward to BOF but position 0 is still a hidden code
    // Reverse direction and search forward for first valid position
    if (pos <= 0)
    {
        pos = 0;
        std::string ch = mDocument->GetCharNoAdvance(pos);
        if (!ch.empty() && ch[0] == MARKER_CHAR)
        {
            eModifiers ctrl = mDocument->GetControlChar(pos);
            if (ctrl != STYLE_TAB && ctrl != STYLE_VARIABLE && ctrl != STYLE_EOF)
            {
                // Position 0 is hidden -- walk forward
                while (pos < textSize)
                {
                    pos++;
                    ch = mDocument->GetCharNoAdvance(pos);
                    if (ch.empty() || ch[0] != MARKER_CHAR)
                    {
                        break;
                    }
                    ctrl = mDocument->GetControlChar(pos);
                    if (ctrl == STYLE_TAB || ctrl == STYLE_VARIABLE || ctrl == STYLE_EOF)
                    {
                        break;
                    }
                }
            }
        }
    }

    // Edge case: walked past end of document -- clamp to EOF position
    if (pos >= textSize)
    {
        pos = textSize - 1;
        if (pos < 0)
        {
            pos = 0;
        }
    }

    return pos;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  newMode [in] the display mode about to be set
///
/// @return nothing
///
/// @brief
/// Virtual hook called before display mode changes. Base class does nothing.
/// GUI override uses this to auto-hide the reveal codes pane when leaving
/// page mode.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::OnBeforeDisplayModeChange(eDisplayMode /*newMode*/)
{
    // Base class: no-op. GUI override handles reveal codes auto-hide.
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  eDisplayMode newMode [in] the mode that was just set
///
/// @return nothing
///
/// @brief
/// Virtual hook called after display mode changes. Base class does nothing.
/// GUI override uses this to update menu labels (e.g. "Command Tags" vs
/// "Reveal Codes").
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::OnAfterDisplayModeChange(eDisplayMode /*newMode*/)
{
    // Base class: no-op. GUI override updates menu labels.
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Trigger background/idle layout. Base class does nothing.
/// GUI override fires QTimer::singleShot to kick off OnIdle().
/// Called on sibling editors after PerformPostCommandUpdate to ensure
/// paragraphs beyond the visible range get re-laid-out.
///
/////////////////////////////////////////////////////////////////////////////
void cEditorBase::TriggerIdleLayout(void)
{
    // Base class: no-op. GUI override triggers background layout timer.
}


