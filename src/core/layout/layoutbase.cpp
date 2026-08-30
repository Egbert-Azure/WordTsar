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
 * @class cLayoutBase
 *
 * @brief Core layout engine that converts a document model into positioned lines and pages.
 *
 * Implements the cLayoutBase class, the main layout algorithm that iterates
 * over document paragraphs, performs word wrapping, builds segment and line
 * structures, manages page boxes with margins, handles dot command processing,
 * and coordinates header/footer insertion. Provides the public API for
 * position-to-coordinate and coordinate-to-position mapping, line/paragraph
 * queries, block selection marking, text justification, and incremental
 * re-layout with formatting checkpoints.
 *
 * @section layoutbase_algorithm Layout Algorithm
 * LayoutDocument() iterates through all document paragraphs, calling
 * LayoutParagraph() for each. Each paragraph is broken into segments
 * (sSegmentLayout) representing individual graphemes with their measured
 * widths. Segments are then grouped into lines (sLineLayout) via word
 * wrapping, and lines are assigned to page boxes (sBoxes) with page
 * breaks triggered by overflow or explicit dot commands.
 *
 * @section layoutbase_coordinates Coordinate Systems
 * - Document positions: grapheme-based POSITION_T values from cDocument
 * - Page coordinates: X/Y positions in twips relative to page origin
 * - Screen coordinates: continuous Y positions for viewport scrolling
 * - The public API (FindCoordInLine, FindPositionInLine) works with
 *   absolute page coordinates; segments store base-0 relative positions
 *
 * @section layoutbase_delegates Delegate Components
 * - cLayoutState: all formatting state (margins, fonts, styles, page geometry)
 * - cPageManager: page box creation, page numbering, margin recalculation
 * - cDotCommandParser: dot command recognition and state modification
 * - cHeaderFooterManager: header/footer text storage and insertion
 * - cTextMeasurement: platform-specific text width and height mMeasurement
 *
 * @section layoutbase_incremental Incremental Re-layout
 * Formatting checkpoints (sFormattingCheckpoint) are saved at paragraph
 * boundaries, allowing re-layout to start from a known state near the
 * modified paragraph rather than from the beginning of the document.
 *
 * @section layoutbase_display Display Features
 * - Block selection marking: MarkSegmentIfInRange() highlights selected text
 * - Text justification: distributes extra space between words for justified text
 * - Control code display: dot command lines, formatting markers, tab symbols
 * - Page view: stacked pages with gaps, or continuous scrolling mode
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cPageManager Page box and page break management
 * @see cDotCommandParser Dot command parsing
 * @see cHeaderFooterManager Header/footer management
 * @see cTextMeasurement Text width/height mMeasurement interface
 * @see sSegmentLayout Per-grapheme layout data
 * @see sLineLayout Per-line layout data
 * @see sParagraphLayout Per-paragraph layout data
 * @see sBoxes Page box geometry
 * @see sFormattingCheckpoint Incremental re-layout checkpoint
 */

#include "layoutbase.h"
#include "textmeasurement.h"
#include "dotcommandparser.h"
#include "pagemanager.h"
#include "headerfootermanager.h"
#include "src/core/document/document.h"
#include <sstream>
#include <iomanip>
#include <limits>
#include <chrono>
#include <ctime>
#include <algorithm>
#include <cstdlib>
#include <cstring>

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Constructor - initializes all member variables to default values
///
/////////////////////////////////////////////////////////////////////////////
cLayoutBase::cLayoutBase(void)
{
    // Create layout state instance
    // mLayoutState handles all settings/state initialization
    mLayoutState = new cLayoutState();

    // Create page manager (depends on mLayoutState)
    mPageManager = new cPageManager(mLayoutState);

    // Text mMeasurement interface (set by derived class)
    mTextMeasurement = nullptr;

    // Current layout state (NOT in mLayoutState - these are layout runtime variables)
    // Box/page tracking handled by mPageManager
    mCurrentBoxIndex = NOT_SET;
    // NOTE: mInFullLayout removed - now computed from mInLayoutDocumentLoop
    mFullLayout = false;  // Track LayoutDocument loop state
    mLastCheckpointMatched = false;
    mInitialPaperWidth = 0;
    mInitialPaperHeight = 0;
    mActiveParagraph = -1;  // No active paragraph until editor sets it

    // Current box coordinates (cached from mPageManager for wrapping performance)
    mBoxLeft = 0;
    mBoxRight = 0;
    mBoxTop = 0;
    mBoxBottom = 0;

    // Document access
    mDocument = nullptr;

    // Line tracking
    mCurrentContentLineNumber = 0;
    mCurrentRawLineNumber = 0;
    mCurrentPageLineNumber = 0;

    // Page tracking (cached from mPageManager for wrapping performance and dot command parser)
    mCurrentPage = 1;
    mLogicalPageNumber = 1;  // Initially same as physical page

    // Height tracking
    mCurrentCumulativeHeight = 0;

    // Create dot command parser (created after page variables initialized)
    // Note: mDocument is nullptr here, will be set later via SetDocument()
    // Pass 'this' so parser can call GetLineHeight() for font-based calculation
    // dotCommandParser uses references to mCurrentPage/mLogicalPageNumber
    mDotCommandParser = new cDotCommandParser(mLayoutState, mDocument, mCurrentPage, mLogicalPageNumber, this);

    // Create header/footer manager (depends on mLayoutState and 'this' for text mMeasurement)
    mHeaderFooterManager = new cHeaderFooterManager(mLayoutState, this);

    // Paragraph tracking
    mCurrentParagraph = 0;

    // Continuous Y tracking
    mScreenY = 0;

}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor - cleanup handled by member destructors
///
/////////////////////////////////////////////////////////////////////////////
cLayoutBase::~cLayoutBase(void)
{
    // Delete header/footer manager
    delete mHeaderFooterManager;

    // Delete page manager
    delete mPageManager;

    // Delete dot command parser
    delete mDotCommandParser;

    // Delete layout state
    delete mLayoutState;

    // Cleanup handled by destructors
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  textMeasurement [in] text mMeasurement implementation (NOT owned)
///
/// @return nothing
///
/// @brief
/// Sets the text mMeasurement interface. Called by derived class to provide
/// platform-specific text mMeasurement (Qt, DirectWrite, etc.).
///
/// @note
/// The text mMeasurement object is NOT owned by cLayoutBase.
/// Derived class is responsible for creation and cleanup.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetTextMeasurement(cTextMeasurement* textMeasurement)
{
    mTextMeasurement = textMeasurement;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] text to measure
///
/// @return width in twips
///
/// @brief
/// Delegates to mTextMeasurement->GetTextWidth().
/// Measures text width using current font.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetTextWidth(const std::string& text)
{
    if (mTextMeasurement)
    {
#ifdef DETAIL_LAYOUT_TIMER
        cTimer measureTimer;
        measureTimer.start();
#endif

        COORD_T result = mTextMeasurement->GetTextWidth(text);

#ifdef DETAIL_LAYOUT_TIMER
        mMeasureTextCallCount++;
        mMeasureTextAccumulatedTimeNs += measureTimer.time_elapsed_nanoseconds();
#endif
        return result;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] text to measure
/// @param  font [in] font specification string
///
/// @return width in twips
///
/// @brief
/// Delegates to mTextMeasurement->GetTextWidth().
/// Measures text width using specified font.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetTextWidth(const std::string& text, const std::string& font)
{
    if (mTextMeasurement)
    {
#ifdef DETAIL_LAYOUT_TIMER
        cTimer measureTimer;
        measureTimer.start();
#endif

        COORD_T result = mTextMeasurement->GetTextWidth(text, font);

#ifdef DETAIL_LAYOUT_TIMER
        mMeasureTextCallCount++;
        mMeasureTextAccumulatedTimeNs += measureTimer.time_elapsed_nanoseconds();
#endif
        return result;
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return height in twips
///
/// @brief
/// Delegates to mTextMeasurement->GetFontHeight().
/// Returns the height of the current font.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetFontHeight(void)
{
    if (mTextMeasurement)
    {
        return mTextMeasurement->GetFontHeight();
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return line spacing in twips
///
/// @brief
/// Delegates to mTextMeasurement->GetFontLineSpacing().
/// Returns the font's recommended line spacing (height + leading).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetFontLineSpacing(void) const
{
    if (mTextMeasurement)
    {
        return mTextMeasurement->GetFontLineSpacing();
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] - font descriptor string
///
/// @return line spacing in twips for specified font
///
/// @brief
/// Returns the line spacing for a specific font without changing current font.
/// Delegates to mTextMeasurement->GetFontLineSpacing(font).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetFontLineSpacing(const std::string& font) const
{
    if (mTextMeasurement)
    {
        return mTextMeasurement->GetFontLineSpacing(font);
    }
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  doc [in] document to layout
///
/// @return nothing
///
/// @brief
/// Main layout entry point. Coordinates the entire layout process by:
/// - Initializing layout state
/// - Creating the first page box
/// - Iterating through all paragraphs
/// - Dispatching to ParseDotCommand() or WordWrapParagraph()
///
/// Performs full document layout.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::LayoutDocument(cDocument* doc, std::function<void(int)> progressCallback)
{
#ifdef LAYOUT_TIMER
    cTimer overallTimer;
    cTimer phaseTimer;
    overallTimer.start();
#endif

    if (doc == nullptr)
    {
        return;
    }

    // Clear all layout data
    mPageManager->Reset();
    mParagraphLayout.clear();
    mFormattingCheckpoints.clear();

    // Clear header/footer storage
    mHeaderFooterManager->Reset();
    mHeaderFooterManager->SetFooterValue(0);

    // Reset state
    mCurrentBoxIndex = NOT_SET;

    // Store document pointer
    SetDocument(doc);

    // Save initial paper dimensions before any dot commands modify them (.PR can swap)
    // Used by ResetFormattingState() to restore document's original paper size
    mInitialPaperWidth = mLayoutState->GetPaperWidth();
    mInitialPaperHeight = mLayoutState->GetPaperHeight();

    // Sync local variables with page manager after reset
    mCurrentBoxIndex = mPageManager->GetCurrentBoxIndex();
    mCurrentPage = mPageManager->GetCurrentPage();
    mLogicalPageNumber = mPageManager->GetLogicalPageNumber();
    mBoxLeft = mBoxRight = mBoxTop = mBoxBottom = 0;

    // Initialize all line tracking variables
    mCurrentContentLineNumber = 0;
    mCurrentRawLineNumber = 0;
    mCurrentPageLineNumber = 0;
    mCurrentCumulativeHeight = 0;
    mScreenY = 0;

    // Reset formatting state to defaults
    // This ensures every full layout starts clean, preventing state from
    // previous layouts (e.g. narrow margins from .rr commands) from persisting
    ResetFormattingState();

    // Clear per-page page number overrides (rebuilt during layout by .pn commands)
    mLayoutState->ClearPageNumOverrides();

#ifdef LAYOUT_TIMER
    // Initialize timing accumulators
    mPreliminaryTimeNs = 0;
    mDotCommandTimeNs = 0;
    mTextLayoutTimeNs = 0;
    mMeasureTextTimeNs = 0;
    mPostLayoutTimeNs = 0;

    // Comprehensive per-section timers
    mGetTextTimeNs = 0;
    mOldLayoutCopyTimeNs = 0;
    mSetupParagraphTimeNs = 0;
    mFontScanTimeNs = 0;
    mDotCommandLineTimeNs = 0;
    mPageBoxTimeNs = 0;
    mEndStateSaveTimeNs = 0;
    mLayoutCompareTimeNs = 0;
    mCheckpointTimeNs = 0;
    mProgressCallbackTimeNs = 0;
    mResizeTimeNs = 0;
    mLayoutParagraphTotalTimeNs = 0;

    // Counters
    mProgressCallbackCount = 0;
    mDotCommandParaCount = 0;
    mTextParaCount = 0;
    mPageBoxCreateCount = 0;
#endif

#ifdef DETAIL_LAYOUT_TIMER
    mApplyPreviousDotCommandsTimeNs = 0;
    mLayoutLineTimeNs = 0;
    mMeasureTextCallCount = 0;
    mMeasureTextAccumulatedTimeNs = 0;
#endif

    // Get paragraph count
    PARAGRAPH_T paraCount = mDocument->GetNumberofParagraphs();

    // Set flag to indicate we're inside the sequential loop
    // LayoutParagraph() uses this to skip state restoration during sequential processing
    mFullLayout = true;

    // Loop through all paragraphs
    // Call LayoutParagraph() for each paragraph
    // This eliminates code duplication and ensures LayoutDocument() and LayoutParagraph()
    // produce identical results (critical for equality-based background layout optimization)
    int lastTenPercent = -1;
    for (PARAGRAPH_T para = 0; para < paraCount; para++)
    {
        // Layout this paragraph using the stateless LayoutParagraph() method
        // Return value (bool) indicates if layout equals old layout - not used in full layout mode
        LayoutParagraph(para);

        // Save formatting checkpoint every CHECKPOINT_INTERVAL paragraphs
        // Used by ApplyPreviousDotCommands() for fast random-access layout
        if ((para + 1) % CHECKPOINT_INTERVAL == 0)
        {
#ifdef LAYOUT_TIMER
            cTimer cpTimer;
            cpTimer.start();
#endif

            SaveFormattingCheckpoint(para);

#ifdef LAYOUT_TIMER
            mCheckpointTimeNs += cpTimer.time_elapsed_nanoseconds();
#endif
        }

        // Report progress at 10% increments via callback (0-10 scale)
        if (progressCallback)
        {
            int tenPercent = static_cast<int>(((para + 1) * 10) / paraCount);
            if (tenPercent != lastTenPercent)
            {
                lastTenPercent = tenPercent;
#ifdef LAYOUT_TIMER
                cTimer cbTimer;
                cbTimer.start();
#endif

                progressCallback(tenPercent);

#ifdef LAYOUT_TIMER
                mProgressCallbackTimeNs += cbTimer.time_elapsed_nanoseconds();
                mProgressCallbackCount++;
#endif
            }
        }
    }

    // Clear loop flag - subsequent LayoutParagraph() calls are partial layout
    mFullLayout = false;

#ifdef LAYOUT_TIMER
    phaseTimer.start();
#endif

    // After all paragraphs are laid out, insert headers/footers for all pages
    // This must be done after parsing all dot commands to ensure templates are populated
    // Page box is created by LayoutParagraph() on the first paragraph (dot or text)
    for (PAGE_T page = 1; page <= mCurrentPage; page++)
    {
        InsertHeadersFooters(page);
    }

    // Calculate screenYTop and screenYBottom for all boxes
    // This must be done after all lines are laid out so that screeny values are final
    const std::vector<sBoxes>& boxList = mPageManager->GetGlobalBoxList();
    for (size_t i = 0; i < boxList.size(); i++)
    {
        sBoxes* box = mPageManager->GetBoxByIndexMutable(i);
        if (box)
        {
            box->CalculateScreenYRange(this);
        }
    }

#ifdef LAYOUT_TIMER
    mPostLayoutTimeNs += phaseTimer.time_elapsed_nanoseconds();

    // Calculate total time
    long long totalTimeNs = overallTimer.time_elapsed_nanoseconds();

    // Skip timing report if running under test mode (QT_TESTING environment variable set)
    // or if this is a help panel (informational display only)
    const char* qtTesting = std::getenv("QT_TESTING");
    bool isTesting = (qtTesting != nullptr && std::strcmp(qtTesting, "1") == 0);

    if (!isTesting && !mLayoutState->IsHelp())
    {
        // Print timing report
        printf("\n=== LAYOUT TIMING REPORT ===\n");
        printf("Total layout time:          %10.2f ms\n", totalTimeNs / 1000000.0);
        printf("Paragraphs: %lld total (%lld dot commands, %lld text)\n",
               mDotCommandParaCount + mTextParaCount, mDotCommandParaCount, mTextParaCount);
        printf("Pages: %ld, Page boxes created: %lld\n", mCurrentPage, mPageBoxCreateCount);

        printf("\n--- LayoutParagraph() Breakdown ---\n");
        printf("  LayoutParagraph total:    %10.2f ms (%5.1f%%)\n",
               mLayoutParagraphTotalTimeNs / 1000000.0,
               (mLayoutParagraphTotalTimeNs * 100.0) / totalTimeNs);
        printf("    Vector resize:          %10.2f ms (%5.1f%%)\n",
               mResizeTimeNs / 1000000.0,
               (mResizeTimeNs * 100.0) / totalTimeNs);
        printf("    GetParagraphText:       %10.2f ms (%5.1f%%)\n",
               mGetTextTimeNs / 1000000.0,
               (mGetTextTimeNs * 100.0) / totalTimeNs);
        printf("    Old layout copy:        %10.2f ms (%5.1f%%)\n",
               mOldLayoutCopyTimeNs / 1000000.0,
               (mOldLayoutCopyTimeNs * 100.0) / totalTimeNs);
        printf("    SetupParagraph:         %10.2f ms (%5.1f%%)\n",
               mSetupParagraphTimeNs / 1000000.0,
               (mSetupParagraphTimeNs * 100.0) / totalTimeNs);
        printf("    Font backward scan:     %10.2f ms (%5.1f%%)\n",
               mFontScanTimeNs / 1000000.0,
               (mFontScanTimeNs * 100.0) / totalTimeNs);
        printf("    Page/margin checks:     %10.2f ms (%5.1f%%)  [%lld box creates]\n",
               mPageBoxTimeNs / 1000000.0,
               (mPageBoxTimeNs * 100.0) / totalTimeNs,
               mPageBoxCreateCount);
        printf("    DotCommandText lines:   %10.2f ms (%5.1f%%)\n",
               mDotCommandLineTimeNs / 1000000.0,
               (mDotCommandLineTimeNs * 100.0) / totalTimeNs);
        printf("    ParseDotCommand:        %10.2f ms (%5.1f%%)\n",
               mDotCommandTimeNs / 1000000.0,
               (mDotCommandTimeNs * 100.0) / totalTimeNs);
        printf("    WordWrapParagraph:      %10.2f ms (%5.1f%%)\n",
               mTextLayoutTimeNs / 1000000.0,
               (mTextLayoutTimeNs * 100.0) / totalTimeNs);
        printf("    End state save:         %10.2f ms (%5.1f%%)\n",
               mEndStateSaveTimeNs / 1000000.0,
               (mEndStateSaveTimeNs * 100.0) / totalTimeNs);
        printf("    Layout comparison:      %10.2f ms (%5.1f%%)\n",
               mLayoutCompareTimeNs / 1000000.0,
               (mLayoutCompareTimeNs * 100.0) / totalTimeNs);

        // What's inside LayoutParagraph but not sub-timed
        long long lpAccountedNs = mResizeTimeNs + mGetTextTimeNs + mOldLayoutCopyTimeNs +
                                   mSetupParagraphTimeNs + mFontScanTimeNs + mPageBoxTimeNs +
                                   mDotCommandLineTimeNs + mDotCommandTimeNs + mTextLayoutTimeNs +
                                   mEndStateSaveTimeNs + mLayoutCompareTimeNs;
        long long lpUnaccountedNs = mLayoutParagraphTotalTimeNs - lpAccountedNs;
        printf("    LP unaccounted:         %10.2f ms (%5.1f%%)\n",
               lpUnaccountedNs / 1000000.0,
               (lpUnaccountedNs * 100.0) / totalTimeNs);

        printf("\n--- Loop Overhead (outside LayoutParagraph) ---\n");
        printf("  Checkpoint saves:         %10.2f ms (%5.1f%%)\n",
               mCheckpointTimeNs / 1000000.0,
               (mCheckpointTimeNs * 100.0) / totalTimeNs);
        printf("  Progress callback:        %10.2f ms (%5.1f%%)  [%lld calls, avg %.1f ms/call]\n",
               mProgressCallbackTimeNs / 1000000.0,
               (mProgressCallbackTimeNs * 100.0) / totalTimeNs,
               mProgressCallbackCount,
               mProgressCallbackCount > 0 ? (mProgressCallbackTimeNs / 1000000.0) / mProgressCallbackCount : 0.0);
        printf("  Post-layout work:         %10.2f ms (%5.1f%%)\n",
               mPostLayoutTimeNs / 1000000.0,
               (mPostLayoutTimeNs * 100.0) / totalTimeNs);

        // Overall accounting
        long long totalAccountedNs = mLayoutParagraphTotalTimeNs + mCheckpointTimeNs +
                                      mProgressCallbackTimeNs + mPostLayoutTimeNs;
        long long totalUnaccountedNs = totalTimeNs - totalAccountedNs;
        printf("\n  TOTAL ACCOUNTED:          %10.2f ms (%5.1f%%)\n",
               totalAccountedNs / 1000000.0,
               (totalAccountedNs * 100.0) / totalTimeNs);
        printf("  TOTAL UNACCOUNTED:        %10.2f ms (%5.1f%%)\n",
               totalUnaccountedNs / 1000000.0,
               (totalUnaccountedNs * 100.0) / totalTimeNs);

#ifdef DETAIL_LAYOUT_TIMER
        printf("\n--- Font Measurement Detail ---\n");
        printf("  Previous dot commands:    %10.2f ms\n",
               mApplyPreviousDotCommandsTimeNs / 1000000.0);
        printf("  Font measurements:        %8lld calls, %8.2f ms",
               mMeasureTextCallCount,
               mMeasureTextAccumulatedTimeNs / 1000000.0);
        if (mMeasureTextCallCount > 0)
        {
            printf(" (avg %6.3f ms/call)\n",
                   (mMeasureTextAccumulatedTimeNs / 1000000.0) / mMeasureTextCallCount);
        }
        else
        {
            printf("\n");
        }
        printf("  Line creation:            %10.2f ms\n",
               mLayoutLineTimeNs / 1000000.0);
#endif

        printf("============================\n\n");
    }
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to layout
///
/// @return bool - true if new layout equals old layout, false if changed
///
/// @brief
/// Layout a single paragraph.
///
/// Full single-paragraph layout with equality checking.
///
/// This method layouts a single paragraph and is designed for both full layout
/// (called from LayoutDocument) and incremental background layout.
///
/// The method:
/// 1. Validates paragraph number
/// 2. Saves old layout for comparison
/// 3. Gets paragraph text from document
/// 4. Handles empty paragraphs (creates empty entry)
/// 5. Handles dot commands (creates visual line, parses command)
/// 6. Handles text paragraphs (restores formatting state, calls WordWrapParagraph)
/// 7. Saves formatting state for next paragraph
/// 8. Compares new layout with old using operator==
/// 9. Returns true if equal (optimization for background layout), false if changed
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::LayoutParagraph(PARAGRAPH_T para)
{
#ifdef LAYOUT_TIMER
    cTimer lpTotalTimer;
    lpTotalTimer.start();
#endif

    // Validate document and paragraph number
    if (mDocument == nullptr)
    {
        return false;
    }

    PARAGRAPH_T paraCount = mDocument->GetNumberofParagraphs();
    if (para < 0 || para >= paraCount)
    {
        return false;
    }

    // Track current paragraph for error reporting
    mCurrentParagraph = para;

    // Ensure mParagraphLayout has space for this paragraph
#ifdef LAYOUT_TIMER
    cTimer resizeTimer;
    resizeTimer.start();
#endif

    if (para >= static_cast<PARAGRAPH_T>(mParagraphLayout.size()))
    {
        mParagraphLayout.resize(para + 1);
    }

#ifdef LAYOUT_TIMER
    mResizeTimeNs += resizeTimer.time_elapsed_nanoseconds();
#endif

    // Get paragraph text
#ifdef LAYOUT_TIMER
    cTimer getTextTimer;
    getTextTimer.start();
#endif

    std::string text = mDocument->GetParagraphText(para);

#ifdef LAYOUT_TIMER
    mGetTextTimeNs += getTextTimer.time_elapsed_nanoseconds();
#endif

    // Empty paragraphs should not exist -- skip silently
    if (text.empty())
    {

#ifdef LAYOUT_TIMER
        mLayoutParagraphTotalTimeNs += lpTotalTimer.time_elapsed_nanoseconds();
#endif

        return true;
    }

    // Save old layout for equality-based stopping optimization
#ifdef LAYOUT_TIMER
    cTimer oldCopyTimer;
    oldCopyTimer.start();
#endif

    sParagraphLayout oldLayout = mParagraphLayout[para];

#ifdef LAYOUT_TIMER
    mOldLayoutCopyTimeNs += oldCopyTimer.time_elapsed_nanoseconds();
#endif

    bool hadPreviousLayout = (oldLayout.number == para || !oldLayout.lines.empty() ||
                               oldLayout.isCommand || oldLayout.isComment);
    bool wasDirty = oldLayout.dirty;

    // Apply all previous dot commands to get correct state for this paragraph
    // During sequential LayoutDocument (mFullLayout), formatting state carries
    // forward naturally from ParseDotCommand() calls -- skip the O(N) rescan.
    // Only rescan for random-access layout (IdleLayout) or the first paragraph.
    if (!mFullLayout || para == 0)
    {
        ApplyPreviousDotCommands(para);
    }

    // Restore state from previous paragraph's stored end state
    // This ensures full and partial layout follow identical code paths
#ifdef LAYOUT_TIMER
    cTimer setupTimer;
    setupTimer.start();
#endif

    sParagraphSetupState setup = SetupParagraph(para);

    // Initialize member variables from previous paragraph's state
    mCurrentContentLineNumber = setup.startContentLineNum;
    mCurrentRawLineNumber = setup.startRawLineNum;
    mCurrentPageLineNumber = setup.startPageLineNum;
    mCurrentPage = setup.startPage;
    mCurrentBoxIndex = setup.startBox;
    mCurrentCumulativeHeight = setup.startCumulativeHeight;
    mScreenY = setup.startScreenY;

    // Restore box fill position and edges
    sBoxes* box = mPageManager->GetBoxByIndexMutable(setup.startBox);
    if (box != nullptr)
    {
        box->currentY = setup.startBoxY;
        mBoxTop = box->top + setup.startBoxY;

        // Restore box left/right edges for rendering
        // Partial layout needs these synced with the actual box so dot
        // commands get consistent boxRight values across layout passes
        mBoxLeft = box->left;
        mBoxRight = box->right;
    }
    else
    {
        // No box exists yet (first paragraph on first layout)
        // Reset to 0 so CreatePageBox below sets them properly
        mBoxLeft = 0;
        mBoxRight = 0;
    }

    // Sync PageManager's current box index and margin tracking
    mPageManager->SetCurrentBoxIndex(setup.startBox);
    mPageManager->SyncLastBoxMargins(setup.startBox);

    // Create or update the page box to ensure document defaults are applied.
    // For paragraph 0: always create/update so the box reflects default margins
    // (ApplyPreviousDotCommands resets formatting state to defaults above).
    // For other paragraphs: only create if no box exists yet.
    // CreatePageBox is idempotent: existing boxes get coordinates updated
    // from current layout state, new boxes are created with current state.
    if (para == 0 || mCurrentBoxIndex == NOT_SET ||
        (mCurrentBoxIndex >= 0 && static_cast<size_t>(mCurrentBoxIndex) >= mPageManager->GetGlobalBoxList().size()))
    {
        CreatePageBox(mCurrentPage);
    }

    // Restore paragraph margin flag
    mLayoutState->SetIsFirstLineOfParagraph(setup.isFirstLineOfParagraph);

#ifdef LAYOUT_TIMER
    mSetupParagraphTimeNs += setupTimer.time_elapsed_nanoseconds();
#endif

    // Dispatch to dot command or text paragraph handler
    bool result;
    if (text[0] == '.')
    {
        result = LayoutDotCommandParagraph(para, text, oldLayout, hadPreviousLayout, wasDirty);
    }
    else
    {
        result = LayoutTextParagraph(para, oldLayout, hadPreviousLayout, wasDirty);
    }

#ifdef LAYOUT_TIMER
    mLayoutParagraphTotalTimeNs += lpTotalTimer.time_elapsed_nanoseconds();
#endif

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number (used to find previous paragraph)
///
/// @return nothing
///
/// @brief
/// Restore font and formatting state from the previous paragraph's endState.
/// Every paragraph (text or dot command) saves font state to endState, so the
/// immediately previous paragraph always has the correct font. Falls back to
/// defaults for paragraph 0.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::RestoreFontStateFromPreviousParagraph(PARAGRAPH_T para)
{
#ifdef LAYOUT_TIMER
    cTimer fontScanTimer;
    fontScanTimer.start();
#endif

    const sParagraphLayout* prevPara = nullptr;
    if (para > 0)
    {
        prevPara = GetParagraphLayout(para - 1);
    }

    if (prevPara != nullptr)
    {
        // Inherit font/formatting state from previous paragraph
        mLayoutState->SetCurrentFont(prevPara->endState.font);
        mLayoutState->SetCurrentColor(prevPara->endState.textcolor);
        mLayoutState->SetBoldActive(prevPara->endState.bold);
        mLayoutState->SetItalicActive(prevPara->endState.italics);
        mLayoutState->SetUnderlineActive(prevPara->endState.underline);
        mLayoutState->SetStrikethroughActive(prevPara->endState.strikethrough);
        mLayoutState->SetSuperscriptActive(prevPara->endState.superscript);
        mLayoutState->SetSubscriptActive(prevPara->endState.subscript);
    }
    else
    {
        // No previous paragraph (para 0) - use defaults
        mLayoutState->SetCurrentFont(mLayoutState->GetDefaultFont());
        mLayoutState->SetCurrentColor(mLayoutState->GetDefaultTextColor());
        mLayoutState->SetBoldActive(false);
        mLayoutState->SetItalicActive(false);
        mLayoutState->SetUnderlineActive(false);
        mLayoutState->SetStrikethroughActive(false);
        mLayoutState->SetSuperscriptActive(false);
        mLayoutState->SetSubscriptActive(false);
    }

#ifdef LAYOUT_TIMER
    mFontScanTimeNs += fontScanTimer.time_elapsed_nanoseconds();
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to save end state for
///
/// @return nothing
///
/// @brief
/// Save position, formatting, alignment, and margin end state to
/// mParagraphLayout[para]. This state is used by the next paragraph's
/// SetupParagraph() call to restore correct starting state.
///
/// @note
/// Does not set boxRight (handled differently by dot command and text cases).
/// Caller must set boxRight before calling this method.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SaveParagraphEndState(PARAGRAPH_T para)
{
#ifdef LAYOUT_TIMER
    cTimer endStateTimer;
    endStateTimer.start();
#endif

    if (para >= static_cast<PARAGRAPH_T>(mParagraphLayout.size()))
    {
#ifdef LAYOUT_TIMER
        mEndStateSaveTimeNs += endStateTimer.time_elapsed_nanoseconds();
#endif

        return;
    }

    // Position end state
    mParagraphLayout[para].endContentLineNum = mCurrentContentLineNumber;
    mParagraphLayout[para].endRawLineNum = mCurrentRawLineNumber;
    mParagraphLayout[para].endPageLineNum = mCurrentPageLineNumber;
    mParagraphLayout[para].endPage = mCurrentPage;
    mParagraphLayout[para].endBox = mCurrentBoxIndex;
    mParagraphLayout[para].endCumulativeHeight = mCurrentCumulativeHeight;
    mParagraphLayout[para].endScreenY = mScreenY;
    if (mCurrentBoxIndex >= 0 && mCurrentBoxIndex < static_cast<int>(mPageManager->GetGlobalBoxList().size()))
    {
        mParagraphLayout[para].endBoxY = mBoxTop - mPageManager->GetGlobalBoxList()[mCurrentBoxIndex].top;
    }
    else
    {
        mParagraphLayout[para].endBoxY = 0;
    }

    // Font and formatting end state
    mParagraphLayout[para].endState.font = mLayoutState->GetCurrentFont();
    mParagraphLayout[para].endState.textcolor = mLayoutState->GetCurrentColor();
    mParagraphLayout[para].endState.bold = mLayoutState->IsBoldActive();
    mParagraphLayout[para].endState.italics = mLayoutState->IsItalicActive();
    mParagraphLayout[para].endState.underline = mLayoutState->IsUnderlineActive();
    mParagraphLayout[para].endState.strikethrough = mLayoutState->IsStrikethroughActive();
    mParagraphLayout[para].endState.superscript = mLayoutState->IsSuperscriptActive();
    mParagraphLayout[para].endState.subscript = mLayoutState->IsSubscriptActive();

    // Alignment end state
    mParagraphLayout[para].endState.left = mLayoutState->GetModifiers().left;
    mParagraphLayout[para].endState.right = mLayoutState->GetModifiers().right;
    mParagraphLayout[para].endState.center = mLayoutState->GetModifiers().center;
    mParagraphLayout[para].endState.justify = mLayoutState->GetModifiers().justify;
    mParagraphLayout[para].endState.linespace = mLayoutState->GetModifiers().linespace;

    // Margin end state
    mParagraphLayout[para].endState.leftMargin = mLayoutState->GetLeftMargin();
    mParagraphLayout[para].endState.rightMargin = mLayoutState->GetRightMargin();
    mParagraphLayout[para].endState.paragraphMargin = mLayoutState->GetParagraphMargin();
    mParagraphLayout[para].endState.validParagraphMargin = mLayoutState->IsValidParagraphMargin();

#ifdef LAYOUT_TIMER
    mEndStateSaveTimeNs += endStateTimer.time_elapsed_nanoseconds();
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number that was just laid out
/// @param  const sParagraphLayout& oldLayout [in] - Previous layout for comparison
/// @param  bool hadPreviousLayout [in] - Whether paragraph was previously laid out
/// @param  bool wasDirty [in] - Whether paragraph was marked dirty before layout
///
/// @return bool - true if layout is unchanged (safe to stop), false if changed
///
/// @brief
/// Compare newly laid out paragraph with its previous layout. Used by background
/// layout for equality-based stopping optimization. If layouts are equal,
/// background layout can stop (all following paragraphs are also unchanged).
///
/// Dirty paragraphs always return false (changed) to prevent early stopping.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::CompareWithOldLayout(PARAGRAPH_T para, const sParagraphLayout& oldLayout, bool hadPreviousLayout, bool wasDirty)
{
    // During idle layout, update checkpoint at boundary if needed
    if (!mFullLayout)
    {
        UpdateCheckpointIfNeeded(para);
    }

    // If paragraph was dirty, force "changed" to prevent early stopping
#ifdef LAYOUT_TIMER
    cTimer compareTimer;
    compareTimer.start();
#endif

    bool result;
    if (wasDirty)
    {
        result = false;
    }
    else
    {
        // Only compare if paragraph was previously laid out; first-time layout returns false (changed)
        result = hadPreviousLayout && mParagraphLayout[para].IsEqualTo(oldLayout);
    }

#ifdef LAYOUT_TIMER
    mLayoutCompareTimeNs += compareTimer.time_elapsed_nanoseconds();
#endif

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number
/// @param  const std::string& text [in] - Paragraph text (starts with '.')
/// @param  const sParagraphLayout& oldLayout [in] - Previous layout for comparison
/// @param  bool hadPreviousLayout [in] - Whether paragraph was previously laid out
/// @param  bool wasDirty [in] - Whether paragraph was marked dirty before layout
///
/// @return bool - true if layout is unchanged, false if changed
///
/// @brief
/// Handle layout of a dot command paragraph. Dot commands start with '.'
/// and may be actual commands (.lm, .rm, .pa, etc.) or comments (..).
/// Creates visible display lines if ShowControl mode permits, parses the
/// command to update layout state, and stores end state for next paragraph.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::LayoutDotCommandParagraph(PARAGRAPH_T para, const std::string& text, const sParagraphLayout& oldLayout, bool hadPreviousLayout, bool wasDirty)
{
#ifdef LAYOUT_TIMER
    mDotCommandParaCount++;
#endif

    // Restore font state from previous paragraph
    RestoreFontStateFromPreviousParagraph(para);

    // Create paragraph entry for dot command
    sParagraphLayout dotPara;
    dotPara.number = para;

    // Set visual display flags for dot commands and comments
    if ((text.length() >= 2 && text[1] == '.') ||
        (text.length() >= 3 && (text[1] == 'i' || text[1] == 'I') && (text[2] == 'g' || text[2] == 'G')))
    {
        // Comment (..) or ignore (.ig)
        dotPara.isComment = true;
        dotPara.dotStatus = DOT_GOOD;
    }
    else
    {
        // Dot command
        dotPara.isCommand = true;
    }

    // Create visible line BEFORE parsing (so it uses current page)
    // This ensures page-affecting commands like .pa are displayed on the
    // page they're breaking FROM, not the page they're breaking TO
    //
    // ShowControl behavior:
    // - SHOW_ALL: Create visible line (will show dot commands with backgrounds)
    // - SHOW_DOT: Create visible line (will show dot commands with backgrounds)
    // - SHOW_NONE: Skip visible line (hide dot commands - used in page mode/print)
    // Show dot command line if display mode allows it, OR if this is
    // the active paragraph (caret is here) -- so the user can see what
    // they're typing when entering a new dot command in SHOW_NONE mode
    if (mLayoutState->GetShowControl() == SHOW_ALL ||
        mLayoutState->GetShowControl() == SHOW_DOT ||
        para == mActiveParagraph)
    {
#ifdef LAYOUT_TIMER
        cTimer dotLineTimer;
        dotLineTimer.start();
#endif

        std::vector<sLineLayout> lines = LayoutDotCommandText(para, dotPara.isComment);
        for (auto& line : lines)
        {
            RegisterDotLineInBox(line);
            dotPara.lines.push_back(std::move(line));
        }

#ifdef LAYOUT_TIMER
        mDotCommandLineTimeNs += dotLineTimer.time_elapsed_nanoseconds();
#endif

    }

    // Store box right edge BEFORE ParseDotCommand (which may change margins via .rm)
    // This ensures the dot command line renders with the margin in effect at its position
    if (mBoxRight > 0)
    {
        dotPara.boxRight = mBoxRight;
    }
    else
    {
        // No page box created yet (dot commands before first text paragraph)
        // Compute right edge from layout state (same formula as CreatePageBox)
        COORD_T pageOffset = (mCurrentPage % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();
        dotPara.boxRight = pageOffset + mLayoutState->GetRightMargin();
    }

    // Parse dot command AFTER creating visible line
    // (allows page-affecting commands like .pa to take effect after display)
#ifdef LAYOUT_TIMER
    cTimer dotTimer;
    dotTimer.start();
#endif

    eDotCommandStatus parseResult = ParseDotCommand(text);

#ifdef LAYOUT_TIMER
    mDotCommandTimeNs += dotTimer.time_elapsed_nanoseconds();
#endif

    // Update dotStatus based on parse result (for commands only, not comments)
    if (dotPara.isCommand)
    {
        dotPara.dotStatus = parseResult;
    }

    // Detect page-affecting dot commands (.MT, .MB, .PL)
    // Mark all paragraphs from current page onward as dirty so idle layout
    // re-lays them with correct page box coordinates.
    if (parseResult == DOT_GOOD && text.length() >= 3)
    {
        char c1 = toupper(text[1]);
        char c2 = toupper(text[2]);
        bool isPageAffecting = (c1 == 'M' && (c2 == 'T' || c2 == 'B')) ||
                               (c1 == 'P' && c2 == 'L');
        if (isPageAffecting)
        {
            // Mark all paragraphs from current page onward as dirty
            PARAGRAPH_T totalParas = static_cast<PARAGRAPH_T>(mParagraphLayout.size());
            for (PARAGRAPH_T p = para + 1; p < totalParas; p++)
            {
                mParagraphLayout[p].dirty = true;
            }
        }
    }

    // Preserve pageBreak flag that might have been set by ParseDotCommand (.PA)
    // ParseDotCommand sets mParagraphLayout[para].pageBreak directly, so we must copy it
    dotPara.pageBreak = mParagraphLayout[para].pageBreak;

    // Replace existing entry with dot command layout
    mParagraphLayout[para] = dotPara;

    // Save end state for next paragraph
    SaveParagraphEndState(para);

    return CompareWithOldLayout(para, oldLayout, hadPreviousLayout, wasDirty);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number
/// @param  const sParagraphLayout& oldLayout [in] - Previous layout for comparison
/// @param  bool hadPreviousLayout [in] - Whether paragraph was previously laid out
/// @param  bool wasDirty [in] - Whether paragraph was marked dirty before layout
///
/// @return bool - true if layout is unchanged, false if changed
///
/// @brief
/// Handle layout of a regular text paragraph. Checks for margin and page
/// changes, restores font state, performs word wrapping, and stores end
/// state for next paragraph. Page box creation is handled by LayoutParagraph.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::LayoutTextParagraph(PARAGRAPH_T para, const sParagraphLayout& oldLayout, bool hadPreviousLayout, bool wasDirty)
{
#ifdef LAYOUT_TIMER
    mTextParaCount++;
#endif

    // Page box is created by LayoutParagraph() before dispatch.
    // Check if content-flow margins changed (left/right) - stack new box
#ifdef LAYOUT_TIMER
    cTimer pageBoxTimer;
    pageBoxTimer.start();
#endif
    if (CheckMarginChange())
    {
        CreateMarginBox(mCurrentPage);
    }

    // Check if page-level margins changed (top/bottom/pageLength).
    // Per WordStar 7: MT/MB/PL must be at the top of the page before any text.
    // If no text has been laid out on this page (currentY == 0), apply to current page.
    // If text already exists, skip -- new values will apply to next page via CreatePageBox.
    if (CheckPageChange())
    {
        const sBoxes* currentBox = mPageManager->GetCurrentBox();
        if (currentBox != nullptr && CoordsEqual(currentBox->currentY, 0))
        {
            // No text on page yet -- apply to current page
            UpdatePageBox();
        }
        // else: new margins apply to next page via CreatePageBox
    }
#ifdef LAYOUT_TIMER
    mPageBoxTimeNs += pageBoxTimer.time_elapsed_nanoseconds();
#endif

    // Restore font state from previous paragraph
    RestoreFontStateFromPreviousParagraph(para);

    // Clear existing lines before re-layout
    // WordWrapParagraph() adds to existing lines, so we must clear first
    mParagraphLayout[para].lines.clear();

    // CRITICAL: Set paragraph number for equality checks
    // This must be set BEFORE WordWrap, because WordWrap modifies the paragraph structure
    mParagraphLayout[para].number = para;

    // Word wrap regular text paragraph
#ifdef LAYOUT_TIMER
    cTimer textTimer;
    textTimer.start();
#endif

    WordWrapParagraph(para);

#ifdef LAYOUT_TIMER
    mTextLayoutTimeNs += textTimer.time_elapsed_nanoseconds();
#endif

    // Store box right edge for rendering
    mParagraphLayout[para].boxRight = mBoxRight;

    // Save end state for next paragraph
    SaveParagraphEndState(para);

    return CompareWithOldLayout(para, oldLayout, hadPreviousLayout, wasDirty);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to check
///
/// @return bool - true if paragraph has valid layout, false otherwise
///
/// @brief
/// Check if a paragraph has been laid out. Used by background layout system
/// to determine which paragraphs need layout.
///
/// A paragraph is considered laid out if:
/// 1. It's within valid range
/// 2. It has an entry in mParagraphLayout
/// 3. It has at least one line OR is a dot command/empty paragraph
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::IsParagraphLaidOut(PARAGRAPH_T para) const
{
    // Check valid range
    if (para < 0 || para >= static_cast<PARAGRAPH_T>(mParagraphLayout.size()))
    {
        return false;
    }

    // Get paragraph layout
    const sParagraphLayout& paraLayout = mParagraphLayout[para];

    // Paragraph is laid out if:
    // 1. It has lines (text paragraph that has been word-wrapped)
    // 2. OR it's a dot command (isCommand flag set)
    // 3. OR it's a comment (isComment flag set)
    // 4. OR it's an empty paragraph (no text, valid entry exists)

    if (!paraLayout.lines.empty())
    {
        return true;  // Has lines - definitely laid out
    }

    if (paraLayout.isCommand || paraLayout.isComment)
    {
        return true;  // Dot command or comment - may have no lines but is laid out
    }

    // Empty paragraph - check if document paragraph is actually empty
    if (mDocument != nullptr)
    {
        std::string text = mDocument->GetParagraphText(para);
        if (text.empty())
        {
            return true;  // Empty paragraph - valid layout
        }
    }

    // Paragraph has no lines and is not a special case - not laid out
    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return reference to global box list
///
/// @brief
/// Returns the global box list containing all boxes for the document
///
/////////////////////////////////////////////////////////////////////////////
const std::vector<sBoxes>& cLayoutBase::GetGlobalBoxList(void) const
{
    return mPageManager->GetGlobalBoxList();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current box index, or NOT_SET if no current box
///
/// @brief
/// Returns the index into the global box list for the current box
///
/////////////////////////////////////////////////////////////////////////////
int cLayoutBase::GetCurrentBoxIndex(void) const
{
    return mCurrentBoxIndex;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number to create box for (1-based)
///
/// @return true if box created successfully, false otherwise
///
/// @brief
/// Creates a new text box for the start of a page.
///
/// Box coordinates are calculated as absolute page positions:
///   left   = pageOffset + mLayoutState->GetLeftMargin()
///   right  = pageOffset + mLayoutState->GetRightMargin()
///   top    = mLayoutState->GetTopMargin() + mLayoutState->GetHeaderMargin()
///   bottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin() - mLayoutState->GetFooterMargin()
///
/// If page length (.PL) is set, bottom is adjusted to not exceed it.
///
/// If we've already created a box for this page (during incremental
/// layout or redraw), we don't create a new one - we reuse the existing.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::CreatePageBox(PAGE_T page)
{
    bool result = mPageManager->CreatePageBox(page);

    // Sync local box coordinates with page manager
    mBoxLeft = mPageManager->GetBoxLeft();
    mBoxRight = mPageManager->GetBoxRight();
    mBoxTop = mPageManager->GetBoxTop();
    mBoxBottom = mPageManager->GetBoxBottom();
    mCurrentBoxIndex = mPageManager->GetCurrentBoxIndex();

    // Set the continuous Y where this page starts (for viewport/rendering)
    // Formula: pageStartY = mScreenY - box.top
    // This converts page-relative coordinates to continuous coordinates
    // Note: Can be negative for page 1 (e.g., -1440 if top margin is 1440)
    sBoxes* box = mPageManager->GetBoxByIndexMutable(mCurrentBoxIndex);
    if (box != nullptr)
    {
        box->pageStartY = mScreenY - mBoxTop;
    }

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Helper method for wordwrapper - increments current page and creates
/// a new page box. Updates both page manager and local page tracking.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::IncrementPageAndCreateBox(void)
{
    // Increment page numbers
    mCurrentPage = mCurrentPage + 1;
    mLogicalPageNumber = mCurrentPage + mLayoutState->GetPageNumOffsetForPage(mCurrentPage);

    // New page: restart the per-page line counter (status-line "L" resets each page)
    mCurrentPageLineNumber = 0;

    // Update page manager
    mPageManager->SetCurrentPage(mCurrentPage);
    mPageManager->SetLogicalPageNumber(mLogicalPageNumber);

    // Add gap for continuous mode page break separation (before CreatePageBox
    // because it uses mScreenY to compute box.pageStartY)
    mScreenY += CONTINUOUS_PAGE_GAP;

    // Create new page box (full page size - boxes are layout containers, not content wrappers)
    CreatePageBox(mCurrentPage);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if margins have changed since last box creation
///
/// @brief
/// Checks if the current margin settings differ from those used to
/// create the last box. Used to determine if a new margin box is needed.
///
/// Compares left margin, right margin, and page offset.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::CheckMarginChange(void)
{
    return mPageManager->CheckMarginChange();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if page-level margins (top/bottom) have changed
///
/// @brief
/// Checks if the current top/bottom margin settings differ from those
/// used when the current page box was created.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::CheckPageChange(void)
{
    return mPageManager->CheckPageChange();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true on success
///
/// @brief
/// Updates the current page box in-place when top/bottom margins change.
/// Modifies all boxes on the current page to use new top/bottom boundaries.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::UpdatePageBox(void)
{
    bool result = mPageManager->UpdatePageBox();

    // Sync local box coordinates with page manager
    // (same pattern as CreatePageBox and CreateMarginBox)
    mBoxLeft = mPageManager->GetBoxLeft();
    mBoxRight = mPageManager->GetBoxRight();
    mBoxTop = mPageManager->GetBoxTop();
    mBoxBottom = mPageManager->GetBoxBottom();
    mCurrentBoxIndex = mPageManager->GetCurrentBoxIndex();

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraphNum [in] paragraph number to check
///
/// @return true if previous paragraph has pageBreak flag set
///
/// @brief
/// Checks if the previous paragraph had a .PA command, which would
/// require starting the current paragraph on a new page.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::CheckPageBreak(PARAGRAPH_T paragraphNum)
{
    // No previous paragraph to check
    if (paragraphNum == 0)
    {
        return false;
    }

    // Find previous paragraph
    const sParagraphLayout* prevPara = GetParagraphLayout(paragraphNum - 1);

    if (prevPara != nullptr)
    {
        return prevPara->pageBreak;
    }

    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  lineHeight [in] height of line to be added (in twips)
///
/// @return true if adding this line would overflow the current box
///
/// @brief
/// Checks if there's enough space in the current box for a line of
/// the specified height. Used for automatic page breaks when box fills.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::NeedNewPage(COORD_T lineHeight)
{
    return mPageManager->NeedNewPage(lineHeight);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number for the new box (1-based)
///
/// @return true if box created successfully, false otherwise
///
/// @brief
/// Creates a new text box due to margin change mid-page.
///
/// Unlike CreatePageBox, this stacks vertically from the previous box.
/// The top of the new box starts at the previous box's currentY position.
///
/// Box coordinates:
///   left   = pageOffset + mLayoutState->GetLeftMargin()
///   right  = pageOffset + mLayoutState->GetRightMargin()
///   top    = previousBox.top + previousBox.currentY
///   bottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin() - mLayoutState->GetFooterMargin()
///
/// If page length (.PL) is set, bottom is adjusted to not exceed it.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::CreateMarginBox(PAGE_T page)
{
    bool result = mPageManager->CreateMarginBox(page);

    // Sync local box coordinates with page manager
    mBoxLeft = mPageManager->GetBoxLeft();
    mBoxRight = mPageManager->GetBoxRight();
    mBoxTop = mPageManager->GetBoxTop();
    mBoxBottom = mPageManager->GetBoxBottom();
    mCurrentBoxIndex = mPageManager->GetCurrentBoxIndex();

    // Set the continuous Y where this page starts (for viewport/rendering)
    // Same formula as CreatePageBox: pageStartY = mScreenY - box.top
    // For margin boxes, box.top is the accumulated Y from previous box, so this still gives correct page start
    sBoxes* box = mPageManager->GetBoxByIndexMutable(mCurrentBoxIndex);
    if (box != nullptr)
    {
        box->pageStartY = mScreenY - mBoxTop;
    }

    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  margin [in] left margin in twips
///
/// @return nothing
///
/// @brief
/// Sets the left margin for future box creation.
/// This is the column position from the left edge after page offset.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetLeftMargin(COORD_T margin)
{
    mLayoutState->SetLeftMargin(margin);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  margin [in] right margin in twips
///
/// @return nothing
///
/// @brief
/// Sets the right margin for future box creation.
/// This is the column position from the left edge after page offset.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetRightMargin(COORD_T margin)
{
    mLayoutState->SetRightMargin(margin);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  margin [in] paragraph margin in twips
///
/// @return nothing
///
/// @brief
/// Sets the paragraph margin (first line indent).
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetParagraphMargin(COORD_T margin)
{
    mLayoutState->SetParagraphMargin(margin);
    mLayoutState->SetValidParagraphMargin(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  offset [in] page offset in twips
///
/// @return nothing
///
/// @brief
/// Sets the odd page offset (binding margin).
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetPageOffsetOdd(COORD_T offset)
{
    mLayoutState->SetPageOffsetOdd(offset);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  offset [in] page offset in twips
///
/// @return nothing
///
/// @brief
/// Sets the even page offset (binding margin).
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetPageOffsetEven(COORD_T offset)
{
    mLayoutState->SetPageOffsetEven(offset);
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string command [in] - Dot command text to parse
///
/// @return eDotCommandStatus - Command recognition status
///
/// @brief
/// Parse WordStar dot command and apply settings.
/// All dot command dispatch is centralized in cDotCommandParser.
/// Commands needing layout internals (.PA, .CP, .HE, .FO) call back
/// into this class via the mLayout pointer held by the parser.
///
/// Returns:
///   DOT_GOOD - Command recognized and successfully parsed
///   DOT_ERROR - Command recognized but has syntax/parameter errors
///   DOT_NOTIMPLEMENTED - Command is valid WordStar but not yet coded
///   DOT_UNKNOWN - Command not recognized (not in WordStar spec)
///
/// @note
///   This should be cleaned up/removed and every place it's called should 
///   call the parser directly, but for now it's used a lot, changes tests, 
///   etc, we'll leave as is for now, we're too close to release.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cLayoutBase::ParseDotCommand(const std::string& command)
{
    return mDotCommandParser->ParseDotCommand(command);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number to format
/// @param  format [in] format style (arabic, roman, etc.)
///
/// @return formatted page number string
///
/// @brief
/// Formats page number according to specified format.
/// Delegates to cDotCommandParser which has the implementation.
///
/////////////////////////////////////////////////////////////////////////////
std::string cLayoutBase::FormatPageNumber(PAGE_T page, ePageNumberFormat format) const
{
    return mDotCommandParser->FormatPageNumber(page, format);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .PA (page break) command.
/// Increments page counter so subsequent content appears on the next page.
/// The new page box is created when the next text paragraph is encountered,
/// allowing subsequent dot commands to affect the new page's formatting.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cLayoutBase::ParsePageBreak(const std::string& command)
{
    UNUSED_ARGUMENT(command);  // .PA command has no parameters
    // .PA command has no parameters - it forces immediate page break

    // Check if we need to create a box for content before the page break
    // We need a box if:
    // 1. We've already created a box (mCurrentBoxIndex != NOT_SET)
    // 2. OR there are visible lines in any paragraph that need a box
    bool needBox = (mCurrentBoxIndex != NOT_SET);

    if (!needBox)
    {
        // Check if any paragraph has visible lines
        for (const auto& para : mParagraphLayout)
        {
            if (!para.lines.empty())
            {
                needBox = true;
                break;
            }
        }
    }

    // Create box for content before page break if needed
    // This ensures we don't create an empty page 1 when .pa is at document start
    if (needBox && mCurrentBoxIndex == NOT_SET)
    {
        CreatePageBox(mCurrentPage);
    }

    // NOTE: Do NOT truncate box.bottom - boxes are layout containers that define
    // where text should flow. They should remain full page size.

    // Increment page counter for next content
    // Do NOT create the box yet - it will be created when next text
    // paragraph is encountered, allowing subsequent dot commands (like .mt)
    // to affect the new page's box
    mCurrentPage = mCurrentPage + 1;
    mPageManager->SetCurrentPage(mCurrentPage);
    mLogicalPageNumber = mCurrentPage + mLayoutState->GetPageNumOffsetForPage(mCurrentPage);
    mPageManager->SetLogicalPageNumber(mLogicalPageNumber);

    // New page: restart the per-page line counter (status-line "L" resets each page)
    mCurrentPageLineNumber = 0;

    // Add gap for continuous mode page break separation
    mScreenY += CONTINUOUS_PAGE_GAP;

    // Reset box index so next text paragraph creates a new box
    mCurrentBoxIndex = NOT_SET;
    mPageManager->SetCurrentBoxIndex(NOT_SET);

    // Mark this paragraph as having a page break after it
    // This is metadata that documents which paragraph triggered the page break,
    // useful for debugging and future features
    if (!mParagraphLayout.empty())
    {
        mParagraphLayout.back().pageBreak = true;
    }

    return DOT_GOOD;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] full dot command string (.cp 5)
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses .CP (conditional page break) command. If remaining space in current
/// box is less than specified number of lines, forces a page break.
///
/// This is useful for keeping blocks of text together (like headings with
/// following paragraphs).
///
/// Example: .cp 5 breaks page if less than 5 lines remain
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cLayoutBase::ParseConditionalPageBreak(const std::string& command)
{
    std::string param = command.substr(3);

    // Trim whitespace
    param.erase(0, param.find_first_not_of(" \t"));
    if (!param.empty())
    {
        param.erase(param.find_last_not_of(" \t") + 1);
    }

    // .CP requires a parameter
    if (param.empty())
    {
        return DOT_ERROR;
    }

    bool incdec;
    double requiredLines = mDocument->GetValue(param, incdec);

    // Must be positive number of lines
    if (requiredLines <= 0.0)
    {
        return DOT_ERROR;
    }

    // Get current box
    if (mPageManager->GetGlobalBoxList().empty() || mCurrentBoxIndex < 0)
    {
        return DOT_ERROR;
    }

    const sBoxes* currentBox = mPageManager->GetCurrentBox();
    if (!currentBox)
    {
        return DOT_ERROR;
    }

    // Calculate required space (lines * current line height)
    COORD_T requiredSpace = static_cast<COORD_T>(requiredLines * GetLineHeight());

    // Calculate remaining space in box
    COORD_T remainingSpace = currentBox->bottom - (currentBox->top + currentBox->currentY);

    // If not enough space, trigger page break
    if (remainingSpace < requiredSpace)
    {
        mLayoutState->SetDoNewPage(true);
    }

    return DOT_GOOD;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return pointer to current box, or nullptr if no current box
///
/// @brief
/// Returns a pointer to the current box being filled.
/// Used during wrapping to access box properties.
///
/////////////////////////////////////////////////////////////////////////////
const sBoxes* cLayoutBase::GetCurrentBox(void) const
{
    return mPageManager->GetCurrentBox();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return left coordinate of current box in twips
///
/// @brief
/// Returns the left edge of the current box.
/// Used during wrapping to determine where text can start.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetBoxLeft(void) const
{
    return mBoxLeft;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return right coordinate of current box in twips
///
/// @brief
/// Returns the right edge of the current box.
/// Used during wrapping to determine where text must wrap.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetBoxRight(void) const
{
    return mBoxRight;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return top coordinate of current box in twips
///
/// @brief
/// Returns the top edge of the current box.
/// Used during wrapping to calculate absolute Y positions.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetBoxTop(void) const
{
    return mBoxTop;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return bottom coordinate of current box in twips
///
/// @brief
/// Returns the bottom edge of the current box.
/// Used during wrapping to detect when box is full and page break needed.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetBoxBottom(void) const
{
    return mBoxBottom;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current line height in twips
///
/// @brief
/// Returns the current line height. If NOT_SET, calculates font-based line
/// height using font metrics. Otherwise returns configured value from .LH command.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetLineHeight(void) const
{
    if (mLayoutState->GetLineHeight() == NOT_SET)
    {
        return CalculateFontBasedLineHeight();
    }
    return mLayoutState->GetLineHeight();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return font-based line height in twips
///
/// @brief
/// Calculates line height from current font's lineSpacing() metric.
/// This provides typographically correct spacing as designed by font creator.
/// Used when mLayoutState->GetLineHeight() is NOT_SET (no explicit .LH command).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::CalculateFontBasedLineHeight(void) const
{
    // Get font's recommended line spacing (ascent + descent + leading)
    // This is the natural baseline-to-baseline distance for the font
    return GetFontLineSpacing();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if auto-leading mode enabled, false otherwise
///
/// @brief
/// Returns whether auto-leading mode is enabled (.LH A).
/// In auto-leading mode, line height is determined by tallest font in line.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::GetAutoLeading(void) const
{
    return mLayoutState->IsAutoLeading();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current subscript/superscript vertical offset in twips
///
/// @brief
/// Returns the current subscript/superscript roll value set by .SR command.
/// Default is 90 twips (3/48 inch), matching WordStar 7.0 default.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetSubSuperRoll(void) const
{
    return mLayoutState->GetSubSuperRoll();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return paper width in twips
///
/// @brief
/// Returns the current paper width.
/// Default is 12240 twips (8.5 inches for US Letter).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetPaperWidth(void) const
{
    return mLayoutState->GetPaperWidth();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return paper height in twips
///
/// @brief
/// Returns the current paper height.
/// Default is 15840 twips (11 inches for US Letter).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetPaperHeight(void) const
{
    return mLayoutState->GetPaperHeight();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if mLandscape mode, false if portrait mode
///
/// @brief
/// Returns the current printer orientation setting from .PR command.
/// Default is false (portrait mode).
/// Used by print engine to set QPrinter orientation.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::GetLandscapeMode(void) const
{
    return mLayoutState->IsLandscapeMode();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if automatic page numbering enabled, false if disabled
///
/// @brief
/// Returns the current automatic page numbering setting from .OP/.PG commands.
/// Default is true (page numbering enabled).
/// When true and no footers defined, page numbers are automatically added
/// at bottom center of each page.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::GetPrintPageNumbers(void) const
{
    return mLayoutState->ShouldPrintPageNumbers();
}

PAGE_T cLayoutBase::GetPageNumberOffset(void) const
{
    return mLayoutState->GetPageNumberOffset();
}

ePageNumberFormat cLayoutBase::GetPageNumFormat(void) const
{
    return mLayoutState->GetPageNumFormat();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return Current logical page number
///
/// @brief
/// Returns the logical page number (physical page + offset).
/// This is the page number that should be displayed to the user.
/// Physical page is tracked in mCurrentPage for navigation.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cLayoutBase::GetLogicalPageNumber(void) const
{
    return mLogicalPageNumber;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return const reference to current text modifiers
///
/// @brief
/// Returns current modifier state for testing and queries.
///
/////////////////////////////////////////////////////////////////////////////
const sModifiers& cLayoutBase::GetModifiers(void) const
{
    return mLayoutState->GetModifiers();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return const reference to tab stops vector
///
/// @brief
/// Returns the current list of tab stops parsed from .RR or .TB commands.
/// Each tab stop contains position (in twips) and type (normal, decimal, center, right).
///
/////////////////////////////////////////////////////////////////////////////
const std::vector<sTabStop>& cLayoutBase::GetTabs(void) const
{
    return mLayoutState->GetTabs();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Total document height in twips
///
/// @brief
/// Calculates total height of document by finding the maximum
/// screenYBottom value across all boxes. Used for scrollbar range.
/// Works for both continuous and page display modes because screeny
/// values already account for display mode (page gaps included).
///
/// This is critical for Y-based scrollbar integration.
/// The scrollbar needs to know document height to set its range correctly.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetTotalDocumentHeight(void) const
{
    if (mPageManager->GetGlobalBoxList().empty())
    {
        return 0;
    }

    // Find maximum screenYBottom across all boxes
    // Simple O(n) scan - fast enough even for large documents
    COORD_T maxHeight = 0;
    const std::vector<sBoxes>& boxList = mPageManager->GetGlobalBoxList();
    for (const auto& box : boxList)
    {
        if (box.screenYBottom > maxHeight)
        {
            maxHeight = box.screenYBottom;
        }
    }

    return maxHeight;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return COORD_T - Average line height in twips
///
/// @brief
/// Returns the average line height for scrolling calculations.
/// Used for scrollbar single step (arrow key scrolling).
/// Uses mLayoutState->GetLineHeight() if set, otherwise first line's height.
///
/// This provides a reasonable scroll increment for single-line
/// scrolling operations (arrow keys on scrollbar).
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetAverageLineHeight(void) const
{
    // If line height is explicitly set (.LH command), use it
    if (mLayoutState->GetLineHeight() > 0)
    {
        return mLayoutState->GetLineHeight();
    }

    // Otherwise use first line's height as approximation
    if (!mParagraphLayout.empty() && !mParagraphLayout[0].lines.empty())
    {
        return mParagraphLayout[0].lines[0].lineheight;
    }

    // Fallback: reasonable default (12pt at single spacing)
    return 240;  // 240 twips ~ 12pt
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return sLayoutMemoryUsage with byte counts for each subsystem
///
/// @brief
/// Calculate approximate memory usage of the layout's data structures.
/// Walks paragraph layouts, lines, segments, checkpoints, and boxes.
///
/////////////////////////////////////////////////////////////////////////////
sLayoutMemoryUsage cLayoutBase::GetMemoryUsage(void) const
{
    sLayoutMemoryUsage usage = {} ;

    usage.paragraphCount = mParagraphLayout.size() ;
    usage.checkpointCount = mFormattingCheckpoints.size() ;

    // Paragraph layout storage
    for (const auto& para : mParagraphLayout)
    {
        // allocated
        usage.paragraphBytes += sizeof(sParagraphLayout) ;
        // in use
        usage.paragraphUsedBytes += sizeof(sParagraphLayout) ;

        for (const auto& line : para.lines)
        {
            usage.lineCount++ ;
            // allocated
            usage.lineBytes += sizeof(sLineLayout) ;
            // in use
            usage.lineUsedBytes += sizeof(sLineLayout) ;

            for (const auto& seg : line.segments)
            {
                usage.segmentCount++ ;

                // sub-breakdown
                size_t structCost = sizeof(sSegmentLayout) ;
                size_t posAllocCost = seg.position.capacity() * sizeof(float) ;
                size_t posUsedCost = seg.position.size() * sizeof(float) ;
                size_t fontCost = seg.font.capacity() ;
                size_t ctrlCost = seg.controlCodeIndices.capacity() * sizeof(size_t) ;

                usage.segStructBytes += structCost ;
                usage.segPositionBytes += posAllocCost ;
                usage.segFontBytes += fontCost ;
                usage.segControlBytes += ctrlCost ;

                // allocated total
                usage.segmentBytes += structCost + posAllocCost + fontCost + ctrlCost ;
                // in use total
                usage.segmentUsedBytes += structCost + posUsedCost
                                        + seg.font.size() + seg.controlCodeIndices.size() * sizeof(size_t) ;
            }
        }
    }

    // Formatting checkpoints
    for (const auto& cp : mFormattingCheckpoints)
    {
        usage.checkpointBytes += sizeof(sFormattingCheckpoint) ;
        usage.checkpointBytes += cp.tabStops.capacity() * sizeof(sTabStop) ;
    }

    // Page boxes
    if (mPageManager)
    {
        const auto& boxes = mPageManager->GetGlobalBoxList() ;
        for (const auto& box : boxes)
        {
            usage.boxBytes += sizeof(sBoxes) ;
            usage.boxBytes += box.containedLines.capacity() * sizeof(LINE_T) ;
        }
    }

    return usage ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Compact all internal containers to release excess allocated memory.
/// Walks the full layout hierarchy: paragraphs, lines, segments,
/// formatting checkpoints, and delegates to owned sub-managers
/// (cLayoutState, cPageManager, cHeaderFooterManager).
///
/// @note Call when no further layout additions are expected (e.g., after
///       backup save) to reclaim wasted vector capacity.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::ShrinkToFit(void)
{
    // Top-level layout containers
    mParagraphLayout.shrink_to_fit();
    mFormattingCheckpoints.shrink_to_fit();

    // Shrink checkpoint tab-stop vectors
    for (auto& cp : mFormattingCheckpoints)
    {
        cp.tabStops.shrink_to_fit();
    }

    // Walk each paragraph -> line -> segment and shrink nested containers
    for (auto& para : mParagraphLayout)
    {
        para.lines.shrink_to_fit();

        for (auto& line : para.lines)
        {
            line.segments.shrink_to_fit();

            for (auto& segment : line.segments)
            {
                segment.position.shrink_to_fit();
                segment.controlCodeIndices.shrink_to_fit();
            }
        }
    }

    // Delegate to owned sub-managers
    if (mLayoutState)
    {
        mLayoutState->ShrinkToFit();
    }
    if (mPageManager)
    {
        mPageManager->ShrinkToFit();
    }
    if (mHeaderFooterManager)
    {
        mHeaderFooterManager->ShrinkToFit();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in] dot command / comment line to register
///
/// @return nothing
///
/// @brief
/// Registers a dot command or comment line's bounds into the current box.
///
/// Dot command and comment lines are laid out by LayoutDotCommandText (which
/// advances mScreenY itself) and bypass SaveLine. This registers the line in
/// the current box so the box's screenYTop/screenYBottom and containedLines
/// include it -- making GetTotalDocumentHeight (and thus the scroll range)
/// account for trailing comment/dot lines at the end of a document.
///
/// Unlike SaveLine, this does NOT advance mScreenY/mBoxTop (already done) and
/// does NOT touch currentY (box fill / pagination is left unchanged). No-ops
/// when no box exists yet (dot commands before the first text paragraph).
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::RegisterDotLineInBox(const sLineLayout& line)
{
    sBoxes* box = mPageManager->GetCurrentBoxMutable();
    if (box == nullptr)
    {
        return;
    }

    if (box->containedLines.empty())
    {
        box->screenYTop = line.screeny;
    }

    box->containedLines.push_back(line.rawLineNumber);

    COORD_T bottom = line.screeny + line.lineheight;
    if (bottom > box->screenYBottom)
    {
        box->screenYBottom = bottom;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraphNum [in] paragraph number this line belongs to
/// @param  line [in/out] line to save (boxIndex will be set)
///
/// @return true if line saved successfully, false otherwise
///
/// @brief
/// Saves a wrapped line into the current box and paragraph layout.
///
/// This method:
/// - Sets the line's boxIndex to track which box contains it
/// - Stores the line in mParagraphLayout for the specified paragraph
/// - Adds the line number to the current box's containedLines
/// - Updates the current box's currentY to track fill height
///
/// The line must have its properties already set (position, height, etc.)
/// before calling this method.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::SaveLine(PARAGRAPH_T paragraphNum, sLineLayout& line)
{
    // Must have a current box to save into
    if (mCurrentBoxIndex < 0 || mCurrentBoxIndex >= static_cast<int>(mPageManager->GetGlobalBoxList().size()))
    {
        return false;
    }

    // Calculate absolute document position for this line
    // This is CRITICAL for caret navigation - eliminates complex paragraph-relative calculations
    if (mDocument)
    {
        POSITION_T paraStart = 0;
        POSITION_T paraEnd = 0;
        mDocument->GetParagraphStartandEnd(paragraphNum, paraStart, paraEnd);
        line.documentPosition = paraStart + line.linestart;
    }
    else
    {
        line.documentPosition = 0;  // Fallback if no document
    }

    // Set the line's box index to current box
    line.boxIndex = mCurrentBoxIndex;

    // Find or create the paragraph in mParagraphLayout using direct indexing
    if (paragraphNum >= static_cast<PARAGRAPH_T>(mParagraphLayout.size()))
    {
        mParagraphLayout.resize(paragraphNum + 1);
    }
    sParagraphLayout* para = &mParagraphLayout[paragraphNum];
    para->number = paragraphNum;

    // Add line to paragraph
    para->lines.push_back(line);

    // First line of paragraph gets paragraph margin (mLayoutState->GetParagraphMargin() applied to pagex).
    // Clear flag after saving first line so subsequent wrapped lines don't get margin.
    // This ensures paragraph margin (first-line indent) only applies once per paragraph.
    if (mLayoutState->IsFirstLineOfParagraph())
    {
        mLayoutState->SetIsFirstLineOfParagraph(false);
    }

    // Add line number to current box's containedLines
    sBoxes* currentBox = mPageManager->GetCurrentBoxMutable();
    if (currentBox)
    {
        // If this is the first line in the box, set screenYTop
        // This ensures incremental layout has correct screen Y range for scrolling
        if (currentBox->containedLines.empty())
        {
            currentBox->screenYTop = line.screeny;
        }

        currentBox->containedLines.push_back(line.rawLineNumber);

        // Update box's current Y position (tracks how full the box is)
        currentBox->currentY += line.lineheight;

        // Update screenYBottom to include this line
        // Critical for incremental layout: GetTotalDocumentHeight() uses screenYBottom
        // Without this, scrolling stops prematurely during background layout
        COORD_T newBottom = line.screeny + line.lineheight;
        if (newBottom > currentBox->screenYBottom)
        {
            currentBox->screenYBottom = newBottom;
        }
    }

    // Update continuous Y coordinate for next line (never resets at page breaks)
    mScreenY += line.lineheight;

    // Update page Y coordinate for next line (resets at page breaks via CreatePageBox)
    mBoxTop += line.lineheight;

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  doc [in] pointer to document
///
/// @return nothing
///
/// @brief
/// Sets the document pointer for accessing paragraph text during layout.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetDocument(cDocument* doc)
{
    mDocument = doc;

    // Update dot command parser's document reference
    if (mDotCommandParser != nullptr)
    {
        mDotCommandParser->SetDocument(doc);
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return document pointer
///
/// @brief
/// Gets the document pointer for accessing paragraph text.
///
/////////////////////////////////////////////////////////////////////////////
cDocument* cLayoutBase::GetDocument(void) const
{
    return mDocument;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current page number being laid out
///
/// @brief
/// Returns the current physical page number during layout.
/// Used by header/footer manager to store definition page.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cLayoutBase::GetCurrentPage(void) const
{
    return mCurrentPage;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return current paragraph number being laid out
///
/// @brief
/// Returns the current paragraph number during layout.
/// Used by header/footer manager for segment tracking.
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cLayoutBase::GetCurrentParagraph(void) const
{
    return mCurrentParagraph;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  documentPos [in] absolute document position of the grapheme
/// @param  grapheme [in] original grapheme from document
///
/// @return display character (converted for MARKER_CHAR/REPLACE_CHAR, or original)
///
/// @brief
/// Centralizes character replacement logic for display/mMeasurement.
/// Converts MARKER_CHAR control codes to ASCII (controlType + '@').
/// Converts REPLACE_CHAR block markers to '<' when at block start.
/// Returns original grapheme for all other cases.
///
/// @note
/// This method is called by both BuildParagraphSegments (for mMeasurement)
/// and DrawSegment (for rendering) to ensure consistent character display.
///
/////////////////////////////////////////////////////////////////////////////
std::string cLayoutBase::GetDisplayCharacter(POSITION_T documentPos,
                                                const std::string& grapheme,
                                                PAGE_T pageNumber) const
{
    // Handle variable control codes -- cTextMeasurement has no page context
    // needed for variable expansion (e.g., page number from sLineLayout)
    if (mDocument && !grapheme.empty() && grapheme[0] == MARKER_CHAR)
    {
        eModifiers controlType = mDocument->GetControlChar(documentPos);
        if (controlType == STYLE_VARIABLE)
        {
            eVariableType varType = mDocument->GetVariable(documentPos);
            if (varType == VAR_PAGE_NUMBER)
            {
                // Use the page number from the line where this variable appears
                return FormatPageNumber(pageNumber, mLayoutState->GetPageNumFormatForPage(pageNumber));
            }
            // Non-page variables (date, time, filename) expand normally
            return GetVariableExpansion(varType);
        }
    }

    // Delegate all other cases to text mMeasurement
    if (mTextMeasurement)
    {
        return mTextMeasurement->GetDisplayCharacter(documentPos, grapheme, mDocument, mLayoutState->GetShowControl());
    }

    // Fallback if mTextMeasurement not set (should not happen in normal operation)
    return grapheme;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  type [in] - variable type to expand
///
/// @return expanded text string for the variable
///
/// @brief
/// Returns the expanded text for a document variable. Called during layout
/// to replace MARKER_CHAR + variable metadata with actual display text.
///
/////////////////////////////////////////////////////////////////////////////
std::string cLayoutBase::GetVariableExpansion(eVariableType type) const
{
    switch (type)
    {
        case VAR_DATE:
        {
            // Current date formatted as WordStar default
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm tm_now;
            localtime_r(&time_t_now, &tm_now);
            char buf[64];
            strftime(buf, sizeof(buf), "%B %d, %Y", &tm_now);
            return std::string(buf);
        }
        case VAR_TIME:
        {
            // Current time
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            struct tm tm_now;
            localtime_r(&time_t_now, &tm_now);
            char buf[64];
            strftime(buf, sizeof(buf), "%H:%M:%S", &tm_now);
            return std::string(buf);
        }
        case VAR_PAGE_NUMBER:
        {
            // Current page number using layout's page formatting
            PAGE_T page = GetCurrentPage();
            return FormatPageNumber(page, mLayoutState->GetPageNumFormatForPage(page));
        }
        case VAR_LINE_NUMBER:
        {
            // Line number not readily available during segment building
            // Return placeholder for now
            return "0";
        }
        case VAR_FILENAME:
        {
            if(!mFilename.empty())
            {
                return mFilename;
            }
            return "untitled";
        }
        case VAR_DRIVE:
        {
            // Extract drive/root from file directory
            if(!mFileDir.empty())
            {
                if(mFileDir[0] == '/')
                {
                    return "/";
                }
                // Windows drive letter (e.g., "C:")
                if(mFileDir.length() >= 2 && mFileDir[1] == ':')
                {
                    return mFileDir.substr(0, 2);
                }
            }
            return "/";
        }
        case VAR_DIRECTORY:
        {
            if(!mFileDir.empty())
            {
                return mFileDir;
            }
            return ".";
        }
        case VAR_FULLPATH:
        {
            if(!mFileDir.empty() && !mFilename.empty())
            {
                return mFileDir + "/" + mFilename;
            }
            if(!mFilename.empty())
            {
                return mFilename;
            }
            return "./untitled";
        }
        case VAR_WORD_COUNT:
        {
            cDocument* doc = GetDocument();
            if (doc)
            {
                return std::to_string(doc->GetWordCount());
            }
            return "0";
        }
    }

    return "?";
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraphNum [in] paragraph this line belongs to
///
/// @return initialized line structure
///
/// @brief
/// Creates a line structure with default values and current box position.
///
/////////////////////////////////////////////////////////////////////////////
sLineLayout cLayoutBase::CreateLine(PARAGRAPH_T paragraphNum)
{
    UNUSED_ARGUMENT(paragraphNum);  // Parameter reserved for future use
#ifdef DETAIL_LAYOUT_TIMER
    cTimer lineTimer;
    lineTimer.start();
#endif

    sLineLayout line;

    // Set line numbers from member state
    line.contentLineNumber = mCurrentContentLineNumber;
    line.rawLineNumber = mCurrentRawLineNumber;
    line.pageLineNumber = mCurrentPageLineNumber;
    line.cumalativeheight = mCurrentCumulativeHeight;

    // Increment line counters for next line
    mCurrentContentLineNumber++;
    mCurrentRawLineNumber++;
    mCurrentPageLineNumber++;

    // Apply line spacing multiplier from .LS command to configured line height
    line.lineheight = GetLineHeight() * mLayoutState->GetModifiers().linespace;

    // Increment cumulative height for next line
    mCurrentCumulativeHeight += line.lineheight;

    line.pagex = mBoxLeft;
    line.pagey = mBoxTop;
    line.screeny = mScreenY;  // Continuous Y coordinate (never resets at page breaks)

    // Copy alignment from current modifiers
    line.left = mLayoutState->GetModifiers().left;
    line.center = mLayoutState->GetModifiers().center;
    line.right = mLayoutState->GetModifiers().right;
    line.justify = mLayoutState->GetModifiers().justify;

    line.pagenumber = (mCurrentBoxIndex >= 0) ? mPageManager->GetGlobalBoxList()[mCurrentBoxIndex].pageNumber : 1;
    line.boxIndex = mCurrentBoxIndex;
    line.linestart = 0;

#ifdef DETAIL_LAYOUT_TIMER
    mLayoutLineTimeNs += lineTimer.time_elapsed_nanoseconds();
#endif

    return line;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  paragraphNum [in] paragraph number to wrap
///
/// @return true if wrapping succeeded, false otherwise
///
/// @brief
/// Performs word wrapping on a paragraph using Unicode word boundaries.
/// Breaks lines when they exceed box width.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::WordWrapParagraph(PARAGRAPH_T paragraphNum)
{
    if (mDocument == nullptr || mCurrentBoxIndex < 0)
    {
        return false;
    }

    // Partial layout no longer needs ApplyPreviousDotCommands because all state
    // (margins, font, alignment, etc.) is now stored in each paragraph's endState
    // and restored by LayoutParagraph from the previous paragraph.
    // This ensures full and partial layout follow identical code paths.
    if (!mFullLayout)
    {
        // Set box coordinates for wrapping (reuse from previous layout)
        // Box index is already set by LayoutParagraph from SetupParagraph()
        const sBoxes& box = mPageManager->GetGlobalBoxList()[mCurrentBoxIndex];
        mBoxLeft = box.left;
        mBoxRight = box.right;
        // mBoxTop is already set by LayoutParagraph from SetupParagraph() stored state

        // CRITICAL: Do NOT use box.bottom directly - it may have been truncated
        mBoxBottom = mLayoutState->GetPaperHeight() - mLayoutState->GetBottomMargin() - mLayoutState->GetFooterMargin();

        // Sync page manager with current state
        mPageManager->SetCurrentPage(mCurrentPage);
        mPageManager->SetLogicalPageNumber(mCurrentPage + mLayoutState->GetPageNumberOffset());
    }

    // Check for conditional page break (.CP) flag
    // If flag is set, start this paragraph on a new page and clear flag
    // Use IncrementPageAndCreateBox to properly truncate the previous box
    if (mLayoutState->ShouldDoNewPage())
    {
        IncrementPageAndCreateBox();
        mLayoutState->SetDoNewPage(false);
    }

    // Get pre-extracted graphemes and offsets from cDocument API
    // This gives us everything we need in one call
    std::vector<std::string> graphemes;
    std::vector<POSITION_T> offsets;
    mDocument->GetParagraphGraphemes(paragraphNum, graphemes, offsets);

    // Get paragraph layout structure for status tracking
    // CRITICAL: This MUST happen BEFORE the empty check to ensure para.number is ALWAYS set
    // Otherwise empty paragraphs will have number=0 from default constructor, breaking equality checks
    if (paragraphNum >= static_cast<PARAGRAPH_T>(mParagraphLayout.size()))
    {
        mParagraphLayout.resize(paragraphNum + 1);
    }
    sParagraphLayout& para = mParagraphLayout[paragraphNum];
    para.number = paragraphNum;

    // Check for empty paragraph
    if (graphemes.empty())
    {
        return true;
    }

    // Check for dot command or comment
    // This sets isCommand, isComment, and dotStatus for visual feedback
    std::string paraText = mDocument->GetParagraphText(paragraphNum);
    if (!paraText.empty() && paraText[0] == '.')
    {
        para.isCommand = false;
        para.isComment = false;
        para.dotStatus = DOT_GOOD;

        // Check for comment (..) or ignore (.ig)
        if ((paraText.length() >= 2 && paraText[1] == '.') ||
            (paraText.length() >= 3 && (paraText[1] == 'i' || paraText[1] == 'I') && (paraText[2] == 'g' || paraText[2] == 'G')))
        {
            para.isComment = true;
            para.dotStatus = DOT_GOOD;
        }
        else
        {
            // It's a dot command - try to parse it
            para.isCommand = true;
            eDotCommandStatus parseResult = ParseDotCommand(paraText);
            para.dotStatus = parseResult;
        }
    }
    else
    {
        // Regular paragraph (not a dot command or comment)
        para.isCommand = false;
        para.isComment = false;
        para.dotStatus = DOT_GOOD;
    }

    // Apply paragraph spacing before (.psb - WordTsar extension)
    // Adds vertical space before this paragraph starts
    if (mLayoutState->GetParagraphSpacingBefore() > 0)
    {
        mBoxTop += mLayoutState->GetParagraphSpacingBefore();
        mScreenY += mLayoutState->GetParagraphSpacingBefore();

        // Also update the current box's Y position
        if (mCurrentBoxIndex >= 0 && mCurrentBoxIndex < static_cast<int>(mPageManager->GetGlobalBoxList().size()))
        {
            mPageManager->UpdateCurrentBoxY(mLayoutState->GetParagraphSpacingBefore());
        }
    }

    // If word wrap is disabled or this is a help display, treat entire paragraph as single line
    // Help displays never wrap text
    if (!mLayoutState->IsWordWrapEnabled() || mLayoutState->IsHelp())
    {
        // Use new two-phase approach (BuildParagraphSegments + place all on one line)
        std::vector<sSegmentLayout> segments = BuildParagraphSegments(paragraphNum);

        // Create single line and add all segments (no wrapping)
        sLineLayout line = CreateLine(paragraphNum);
        line.linestart = 0;  // Entire paragraph is one line starting at position 0

        // Add all segments to the single line
        for (auto& segment : segments)
        {
            line.segments.push_back(segment);
        }

        // Finalize line to convert segment-relative positions to line-relative
        // maxLineWidth = 0 for help displays (no alignment needed, just position accumulation)
        FinalizeLine(line, 0);
        SaveLine(paragraphNum, line);
        return true;
    }

    // Two-phase word wrap approach
    // Build segments with measurements (one pass through graphemes)
    std::vector<sSegmentLayout> segments = BuildParagraphSegments(paragraphNum);

    // Arrange segments into lines (uses pre-measured widths)
    WordWrapSegmentsIntoLines(segments, paragraphNum);

    // Apply paragraph spacing after (.psa - WordTsar extension)
    // Adds vertical space after this paragraph ends
    if (mLayoutState->GetParagraphSpacingAfter() > 0)
    {
        mBoxTop += mLayoutState->GetParagraphSpacingAfter();
        mScreenY += mLayoutState->GetParagraphSpacingAfter();

        // Also update the current box's Y position
        if (mCurrentBoxIndex >= 0 && mCurrentBoxIndex < static_cast<int>(mPageManager->GetGlobalBoxList().size()))
        {
            mPageManager->UpdateCurrentBoxY(mLayoutState->GetParagraphSpacingAfter());
        }
    }

    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  boxIndex [in] index into mGlobalBoxList
///
/// @return pointer to box if valid index, nullptr otherwise
///
/// @brief
/// Returns pointer to a specific box by index with bounds checking.
/// Provides safe access to boxes in the global box list.
///
/////////////////////////////////////////////////////////////////////////////
const sBoxes* cLayoutBase::GetBoxByIndex(int boxIndex) const
{
    return mPageManager->GetBoxByIndex(boxIndex);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  contentLineNumber [in] global content line number (excludes dot-command lines)
///
/// @return pointer to box containing the line, nullptr if line not found
///
/// @brief
/// Finds which box contains a specific line by searching through all
/// paragraphs and checking each line's boxIndex field.
///
/////////////////////////////////////////////////////////////////////////////
const sBoxes* cLayoutBase::GetBoxForLine(LINE_T contentLineNumber) const
{
    // This method needs access to mParagraphLayout, so keep it in cLayoutBase
    // Search all paragraphs for this line
    for (const auto& para : mParagraphLayout)
    {
        for (const auto& line : para.lines)
        {
            if (line.contentLineNumber == contentLineNumber)
            {
                // Found the line - return its box via page manager
                return mPageManager->GetBoxByIndex(line.boxIndex);
            }
        }
    }

    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number (1-based)
///
/// @return vector of box indices for the specified page
///
/// @brief
/// Returns all box indices that belong to a specific page.
/// The vector will be empty if no boxes exist on the page.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<int> cLayoutBase::GetBoxesOnPage(PAGE_T page) const
{
    return mPageManager->GetBoxesOnPage(page);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return total number of boxes in the layout
///
/// @brief
/// Returns the count of boxes in the global box list.
///
/////////////////////////////////////////////////////////////////////////////
int cLayoutBase::GetBoxCount(void) const
{
    return mPageManager->GetBoxCount();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rawLineNumber [in] global raw line number (every laid-out row, includes dot commands) to find
///
/// @return pair of paragraph number and line pointer, or {NOT_SET, nullptr}
///
/// @brief
/// Binary search on mParagraphLayout to find the paragraph containing
/// the given raw line number, then linear scan within that paragraph.
/// Handles empty paragraphs by scanning forward to the nearest non-empty
/// paragraph during the search.
///
/////////////////////////////////////////////////////////////////////////////
std::pair<PARAGRAPH_T, const sLineLayout*> cLayoutBase::FindLineByRawLineNumber(LINE_T rawLineNumber) const
{
    if (mParagraphLayout.empty())
    {
        return {NOT_SET, nullptr};
    }

    // Binary search for the paragraph containing this line number
    ssize_t low = 0;
    ssize_t high = static_cast<ssize_t>(mParagraphLayout.size()) - 1;

    while (low <= high)
    {
        ssize_t mid = low + (high - low) / 2;

        // Skip empty paragraphs by scanning forward from mid
        ssize_t check = mid;
        while (check <= high && mParagraphLayout[check].lines.empty())
        {
            check++;
        }

        // If all paragraphs from mid to high are empty, search lower half
        if (check > high)
        {
            high = mid - 1;
            continue;
        }

        const auto& para = mParagraphLayout[check];
        LINE_T firstLine = para.lines.front().rawLineNumber;
        LINE_T lastLine = para.lines.back().rawLineNumber;

        if (rawLineNumber < firstLine)
        {
            // Target is before this paragraph
            high = mid - 1;
        }
        else if (rawLineNumber > lastLine)
        {
            // Target is after this paragraph
            low = check + 1;
        }
        else
        {
            // Target is within this paragraph, linear scan its lines
            for (const auto& line : para.lines)
            {
                if (line.rawLineNumber == rawLineNumber)
                {
                    return {para.number, &line};
                }
            }

            // Line number in range but not found
            return {NOT_SET, nullptr};
        }
    }

    return {NOT_SET, nullptr};
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rawLineNumber [in] global raw line number (every laid-out row, includes dot commands)
///
/// @return pointer to line if found, nullptr otherwise
///
/// @brief
/// Finds a line by its raw line number using binary search across
/// paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
const sLineLayout* cLayoutBase::GetLineByRawLineNumber(LINE_T rawLineNumber) const
{
    auto [paraNum, line] = FindLineByRawLineNumber(rawLineNumber);
    return line;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rawLineNumber [in] global raw line number (every laid-out row, includes dot commands)
///
/// @return paragraph number containing the line, or NOT_SET if not found
///
/// @brief
/// Finds which paragraph contains a specific line by its raw line number
/// using binary search across paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cLayoutBase::GetParagraphFromLine(LINE_T rawLineNumber) const
{
    auto [paraNum, line] = FindLineByRawLineNumber(rawLineNumber);
    return paraNum;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return total number of lines across all paragraphs
///
/// @brief
/// Counts the total number of lines in all laid-out paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cLayoutBase::GetNumberOfLines(void) const
{
    LINE_T totalLines = 0;

    for (const auto& para : mParagraphLayout)
    {
        totalLines += static_cast<LINE_T>(para.lines.size());
    }

    return totalLines;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos [in] document position (grapheme index) to find line for
///
/// @return raw line number (every laid-out row, includes dot commands) containing the position, or 0 if not found
///
/// @brief
/// Finds which line contains a given document position.
/// Maps a character position within the document to the corresponding
/// line number in the formatted layout.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cLayoutBase::GetLineFromPosition(POSITION_T pos) const
{
    if (!mDocument)
    {
        return 0;
    }

    // Clamp position to valid range
    if (pos >= mDocument->GetTextSize())
    {
        pos = mDocument->GetTextSize() - 1;
    }

    if (pos < 0)
    {
        pos = 0;
    }

    // No layout yet
    if (mParagraphLayout.empty())
    {
        return 0;
    }

    // Get paragraph containing this position
    PARAGRAPH_T para = mDocument->GetParagraphFromPosition(pos);

    // Get paragraph boundaries
    POSITION_T start = 0;
    POSITION_T end = 0;
    mDocument->GetParagraphStartandEnd(para, start, end);

    // Adjust if paragraph not formatted yet
    if (para >= static_cast<PARAGRAPH_T>(mParagraphLayout.size()))
    {
        para = static_cast<PARAGRAPH_T>(mParagraphLayout.size()) - 1;
        mDocument->GetParagraphStartandEnd(para, start, end);
        pos = end - 1;
    }

    // No lines in this paragraph
    if (mParagraphLayout[para].lines.empty())
    {
        // Return first line of paragraph if it exists elsewhere
        // For now, just return 0
        return 0;
    }

    // Convert document-absolute position to paragraph-relative position
    POSITION_T paraRelativePos = pos - start;

    // Search backward through lines in this paragraph
    // (lines are stored in order, so reverse search finds the right one)
    for (int i = static_cast<int>(mParagraphLayout[para].lines.size()) - 1; i >= 0; --i)
    {
        const sLineLayout& line = mParagraphLayout[para].lines[i];

        if (paraRelativePos >= line.linestart)
        {
            return line.rawLineNumber;
        }
    }

    // Fallback: return first line of paragraph
    return mParagraphLayout[para].lines[0].rawLineNumber;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  pos [in] - Document position to find
/// @param  lineNumber [in] - Line number containing the position
///
/// @return COORD_T - X coordinate of position within line (in twips)
///
/// @brief
/// Find the X coordinate of a document position within a specific line.
/// Used for caret positioning and vertical movement.
///
/// @note
/// If position is beyond end of line, returns coordinate of last position.
/// This is a BLACK BOX API method - editor should use this instead of
/// accessing line internals directly.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::FindCoordInLine(POSITION_T pos, LINE_T lineNumber)
{
    // Get the line structure (internal API)
    const sLineLayout* line = GetLineByRawLineNumber(lineNumber);
    if (line == nullptr)
    {
        return 0;
    }

    // Handle empty line
    if (line->segments.empty())
    {
        return line->pagex;
    }

    // Convert document position to paragraph-relative position
    PARAGRAPH_T para = GetParagraphFromLine(lineNumber);
    if (para == NOT_SET)
    {
        return line->pagex;
    }

    // Get paragraph boundaries to convert absolute pos to paragraph-relative
    POSITION_T paraStart = 0;
    POSITION_T paraEnd = 0;
    if (mDocument)
    {
        mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);
    }

    POSITION_T paraRelativePos = pos - paraStart;

    // Iterate through segments to find position
    POSITION_T currentPos = line->linestart;

    for (const auto &segment : line->segments)
    {
        // Search through this segment's positions
        for (size_t i = 0; i < segment.position.size(); ++i)
        {
            // Check if current position matches what we're looking for
            if (currentPos == paraRelativePos)
            {
                // Return ABSOLUTE coordinate (base-0 segment position + line's pagex)
                return line->pagex + segment.position[i];
            }

            // Move to next position
            currentPos++;
        }
    }

    // Position not found or beyond end of line - return last coordinate in line
    if (!line->segments.empty())
    {
        const auto& lastSegment = line->segments.back();
        if (!lastSegment.position.empty())
        {
            // Return ABSOLUTE coordinate (base-0 segment position + line's pagex)
            return line->pagex + lastSegment.position.back();
        }
    }

    return line->pagex;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  targetX [in] - X coordinate to find closest position to
/// @param  lineNumber [in] - Line number to search
///
/// @return POSITION_T - Document position closest to targetX
///
/// @brief
/// Find the position in a line that is closest to a given X coordinate.
/// Uses minimum distance algorithm to find best match.
///
/// @note
/// Skips control codes when mLayoutState->GetShowControl() != SHOW_ALL.
/// This is a BLACK BOX API method - editor should use this for mouse
/// clicks and vertical cursor movement instead of accessing line internals.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cLayoutBase::FindPositionInLine(COORD_T targetX, LINE_T lineNumber)
{
    // Get the line structure (internal API)
    const sLineLayout* line = GetLineByRawLineNumber(lineNumber);
    if (line == nullptr)
    {
        return 0;
    }

    // Handle edge case: if target line has no segments, return line start as document position
    if (line->segments.empty())
    {
        return line->documentPosition;
    }

    // Get paragraph to convert result to document-absolute position
    PARAGRAPH_T para = GetParagraphFromLine(lineNumber);
    if (para == NOT_SET)
    {
        return line->documentPosition;
    }

    // Get paragraph boundaries
    POSITION_T paraStart = 0;
    POSITION_T paraEnd = 0;
    if (mDocument)
    {
        mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);
    }

    // Find the position in the line closest to targetX
    // targetX is ABSOLUTE page coordinate, convert to line-relative
    COORD_T targetXRelative = targetX - line->pagex;

    POSITION_T bestPosition = line->linestart;  // Paragraph-relative
    COORD_T minDifference = std::numeric_limits<COORD_T>::max();

    POSITION_T currentPosition = line->linestart;

    for (size_t segmentNum = 0; segmentNum < line->segments.size(); ++segmentNum)
    {
        const auto &segment = line->segments[segmentNum];

        // Find closest position to target X coordinate
        // segment.position[i] is base-0 (relative to line), compare with targetXRelative
        for (size_t i = 0; i < segment.position.size(); ++i)
        {
            COORD_T diff = std::abs(segment.position[i] - targetXRelative);
            if (diff < minDifference)
            {
                minDifference = diff;
                bestPosition = currentPosition;
            }

            // Move to next position
            currentPosition++;
        }
    }

    // Convert paragraph-relative position to document-absolute position
    return paraStart + bestPosition;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Raw line number (every laid-out row, includes dot commands)
///
/// @return POSITION_T - Paragraph-relative position where line starts
///
/// @brief
/// Get the starting position of a line within its paragraph.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cLayoutBase::GetLineStartPosition(LINE_T line)
{
    const sLineLayout* lineLayout = GetLineByRawLineNumber(line);
    if (lineLayout == nullptr)
    {
        return 0;
    }

    return lineLayout->linestart;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Raw line number (every laid-out row, includes dot commands)
///
/// @return POSITION_T - Absolute document position where line starts
///
/// @brief
/// Get the starting position of a line in absolute document coordinates.
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cLayoutBase::GetLineStartDocumentPosition(LINE_T line)
{
    const sLineLayout* lineLayout = GetLineByRawLineNumber(line);
    if (lineLayout == nullptr)
    {
        return 0;
    }

    return lineLayout->documentPosition;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Raw line number (every laid-out row, includes dot commands)
///
/// @return POSITION_T - Paragraph-relative position of last character on line
///
/// @brief
/// Get the ending position of a line (position OF last char, not one past).
///
/// @note
/// Accounts for control code visibility (mLayoutState->GetShowControl() setting)
///
/////////////////////////////////////////////////////////////////////////////
POSITION_T cLayoutBase::GetLineEndPosition(LINE_T line)
{
    const sLineLayout* lineLayout = GetLineByRawLineNumber(line);
    if (lineLayout == nullptr)
    {
        return 0;
    }

    // Calculate end position by adding up segment lengths
    POSITION_T endPos = lineLayout->linestart;

    for (const auto& segment : lineLayout->segments)
    {
        endPos += segment.length;
    }

    // Return position OF last char, not one past end
    if (endPos > lineLayout->linestart)
    {
        return endPos - 1;
    }

    return lineLayout->linestart;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number
///
/// @return LINE_T - Number of lines in paragraph
///
/// @brief
/// Get the count of lines in a paragraph.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cLayoutBase::GetNumberofLinesinParagraph(PARAGRAPH_T para)
{
    const sParagraphLayout* paraLayout = GetParagraphLayout(para);
    if (paraLayout == nullptr)
    {
        return 0;
    }

    return static_cast<LINE_T>(paraLayout->lines.size());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PAGE_T page [in] - Page number to query
///
/// @return PARAGRAPH_T - Index of first paragraph on page
///
/// @brief
/// Find the first paragraph that appears on a given page.
/// Uses linear search through paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cLayoutBase::GetFirstParagraphOnPage(PAGE_T page)
{
    for (size_t paraIdx = 0; paraIdx < mParagraphLayout.size(); ++paraIdx)
    {
        const auto& para = mParagraphLayout[paraIdx];

        // Skip empty paragraphs
        if (para.lines.empty())
        {
            continue;
        }

        // Check if any line in this paragraph is on the requested page
        for (const auto& line : para.lines)
        {
            if (line.pagenumber == page)
            {
                return static_cast<PARAGRAPH_T>(paraIdx);
            }
            if (line.pagenumber > page)
            {
                break;
            }
        }
    }

    // Page not found - return out of bounds indicator
    return static_cast<PARAGRAPH_T>(mParagraphLayout.size());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  POSITION_T pos [in] - Position within paragraph
/// @param  PARAGRAPH_T para [in] - Paragraph number (NOT_SET for special cases)
/// @param  bool up [in] - Search direction (true=backward, false=forward)
///
/// @return LINE_T - Paragraph-relative line number containing position
///
/// @brief
/// Find which line within a paragraph contains a given position.
/// Search direction affects which line is returned for ambiguous cases.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cLayoutBase::GetParagraphLineFromPosition(POSITION_T pos, PARAGRAPH_T para, bool up)
{
    // Special case: para == NOT_SET
    if (para == NOT_SET)
    {
        if (up)
        {
            return 0;
        }
        else
        {
            LINE_T numLines = GetNumberOfLines();
            return (numLines > 0) ? numLines - 1 : 0;
        }
    }

    const sParagraphLayout* paraLayout = GetParagraphLayout(para);
    if (paraLayout == nullptr || paraLayout->lines.empty())
    {
        return 0;
    }

    if (up)
    {
        // Search backward from end
        for (int line = static_cast<int>(paraLayout->lines.size()) - 1; line >= 0; line--)
        {
            if (paraLayout->lines[line].linestart <= pos)
            {
                return static_cast<LINE_T>(line);
            }
        }
        return 0;
    }
    else
    {
        // Search forward from beginning
        for (LINE_T line = 0; line < static_cast<LINE_T>(paraLayout->lines.size()); line++)
        {
            if (paraLayout->lines[line].linestart >= pos)
            {
                return line;
            }
        }
        // Position is beyond last line
        return static_cast<LINE_T>(paraLayout->lines.size()) - 1;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PAGE_T page [in] - Page number to query
///
/// @return sPageInfo - Page information structure
///
/// @brief
/// Get page-specific metadata including margins and paper size.
/// Returns settings that were active when page was created.
///
/////////////////////////////////////////////////////////////////////////////
sPageInfo cLayoutBase::GetPageInfo(PAGE_T page)
{
    // Find first box on this page
    std::vector<int> boxes = GetBoxesOnPage(page);
    if (!boxes.empty())
    {
        const sBoxes* box = GetBoxByIndex(boxes[0]);
        if (box != nullptr)
        {
            return box->pageInfo;
        }
    }

    // Fallback: return current global settings
    sPageInfo info;
    info.paperwidth = mLayoutState->GetPaperWidth();
    info.paperheight = mLayoutState->GetPaperHeight();
    info.papertype = PaperLetter;
    info.set = false;
    info.topmargin = mLayoutState->GetTopMargin();
    info.bottommargin = mLayoutState->GetBottomMargin();
    info.leftmargin = mLayoutState->GetLeftMargin();
    info.rightmargin = mLayoutState->GetRightMargin();
    info.headermargin = mLayoutState->GetHeaderMargin();
    info.footermargin = mLayoutState->GetFooterMargin();

    return info;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Line number to query
///
/// @return sPageInfo - Page information structure
///
/// @brief
/// Get page information for the page containing a given line.
///
/////////////////////////////////////////////////////////////////////////////
sPageInfo cLayoutBase::GetPageInfoFromLine(LINE_T line)
{
    PAGE_T page = GetPageFromLine(line);
    return GetPageInfo(page);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Raw line number (every laid-out row, includes dot commands)
///
/// @return COORD_T - Base X coordinate (line->pagex)
///
/// @brief
/// Get the base X coordinate of a line (left edge of line content).
/// This is the line's starting X position on the page.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetLineBaseX(LINE_T line)
{
    const sLineLayout* lineLayout = GetLineByRawLineNumber(line);
    if (lineLayout == nullptr)
    {
        return 0;
    }

    return lineLayout->pagex;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Raw line number (every laid-out row, includes dot commands)
///
/// @return COORD_T - Screen Y coordinate (line->screeny)
///
/// @brief
/// Get the Y coordinate of a line on screen. This is the line's
/// vertical position from the top of the page.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetLineScreenY(LINE_T line)
{
    const sLineLayout* lineLayout = GetLineByRawLineNumber(line);
    if (lineLayout == nullptr)
    {
        return 0;
    }

    return lineLayout->screeny;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Raw line number (every laid-out row, includes dot commands)
///
/// @return COORD_T - Line height (line->lineheight)
///
/// @brief
/// Get the height of a line in twips. Accounts for font size and
/// line spacing.
///
/////////////////////////////////////////////////////////////////////////////
COORD_T cLayoutBase::GetLineHeight(LINE_T line)
{
    const sLineLayout* lineLayout = GetLineByRawLineNumber(line);
    if (lineLayout == nullptr)
    {
        return 0;
    }

    return lineLayout->lineheight;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  LINE_T line [in] - Raw line number (every laid-out row, includes dot commands)
///
/// @return PAGE_T - Page number containing the line
///
/// @brief
/// Get the page number that a line appears on.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cLayoutBase::GetLinePageNumber(LINE_T line)
{
    const sLineLayout* lineLayout = GetLineByRawLineNumber(line);
    if (lineLayout == nullptr)
    {
        return 0;
    }

    return lineLayout->pagenumber;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to query
///
/// @return bool - true if paragraph is a dot command, false otherwise
///
/// @brief
/// Check if a paragraph is a dot command (.MT, .PA, etc.)
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::ParagraphIsCommand(PARAGRAPH_T para) const
{
    const sParagraphLayout* p = GetParagraphLayout(para);
    return (p != nullptr) ? p->isCommand : false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to query
///
/// @return bool - true if paragraph is a comment, false otherwise
///
/// @brief
/// Check if a paragraph is a comment (..)
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::ParagraphIsComment(PARAGRAPH_T para) const
{
    const sParagraphLayout* p = GetParagraphLayout(para);
    return (p != nullptr) ? p->isComment : false;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to query
///
/// @return eDotCommandStatus - Dot command status
///
/// @brief
/// Get the validation status of a dot command paragraph.
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cLayoutBase::GetParagraphDotStatus(PARAGRAPH_T para) const
{
    const sParagraphLayout* p = GetParagraphLayout(para);
    return (p != nullptr) ? p->dotStatus : DOT_GOOD;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to query
///
/// @return LINE_T - Content line number of first line (NOT_SET if no lines)
///
/// @brief
/// Get the content line number of the first line in a paragraph.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cLayoutBase::GetFirstLineOfParagraph(PARAGRAPH_T para) const
{
    const sParagraphLayout* p = GetParagraphLayout(para);
    if (p == nullptr || p->lines.empty())
    {
        return NOT_SET;
    }
    return p->lines[0].contentLineNumber;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number to query
///
/// @return LINE_T - Content line number of last line (NOT_SET if no lines)
///
/// @brief
/// Get the content line number of the last line in a paragraph.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cLayoutBase::GetLastLineOfParagraph(PARAGRAPH_T para) const
{
    const sParagraphLayout* p = GetParagraphLayout(para);
    if (p == nullptr || p->lines.empty())
    {
        return NOT_SET;
    }
    return p->lines.back().contentLineNumber;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  POSITION_T pos [in] - Paragraph-relative position
/// @param  PARAGRAPH_T para [in] - Paragraph number
///
/// @return LINE_T - Paragraph-relative line number (NOT_SET if not found)
///
/// @brief
/// Find which line within a paragraph contains a given position.
///
/////////////////////////////////////////////////////////////////////////////
LINE_T cLayoutBase::GetLineContainingPosition(POSITION_T pos, PARAGRAPH_T para) const
{
    const sParagraphLayout* paraLayout = GetParagraphLayout(para);
    if (paraLayout == nullptr || paraLayout->lines.empty())
    {
        return NOT_SET;
    }

    // Iterate through lines to find which contains position
    for (size_t lineIdx = 0; lineIdx < paraLayout->lines.size(); ++lineIdx)
    {
        const auto& line = paraLayout->lines[lineIdx];

        // Calculate line's grapheme range
        POSITION_T lineStart = line.linestart;
        POSITION_T lineEnd = lineStart;

        for (const auto& segment : line.segments)
        {
            lineEnd += segment.GetGraphemeCount();
        }

        // Check if position falls within this line
        if (pos >= lineStart && pos < lineEnd)
        {
            return static_cast<LINE_T>(lineIdx);
        }
    }

    // Position not found - return last line
    return static_cast<LINE_T>(paraLayout->lines.size()) - 1;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  number [in] paragraph number to find
///
/// @return const pointer to paragraph layout if found, nullptr otherwise
///
/// @brief
/// Searches mParagraphLayout for a paragraph with the specified number.
/// Returns const pointer for read-only access.
///
/////////////////////////////////////////////////////////////////////////////
const sParagraphLayout* cLayoutBase::GetParagraphLayout(PARAGRAPH_T number) const
{
    if (number < 0 || number >= static_cast<PARAGRAPH_T>(mParagraphLayout.size()))
    {
        return nullptr;
    }

    return &mParagraphLayout[number];
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return number of paragraphs in mParagraphLayout
///
/// @brief
/// Returns the count of paragraphs that have been laid out.
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cLayoutBase::GetNumberOfParagraphs(void) const
{
    return static_cast<PARAGRAPH_T>(mParagraphLayout.size());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rawLineNumber [in] global raw line number (every laid-out row, includes dot commands)
///
/// @return page number containing the line, or NOT_SET if line not found
///
/// @brief
/// Finds which page contains a specific line by searching for the line
/// and returning its pagenumber field.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cLayoutBase::GetPageFromLine(LINE_T rawLineNumber) const
{
    const sLineLayout* line = GetLineByRawLineNumber(rawLineNumber);

    if (line != nullptr)
    {
        return line->pagenumber;
    }

    return NOT_SET;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return highest page number in any box, or 0 if no boxes exist
///
/// @brief
/// Calculates the total number of pages by finding the highest page
/// number among all boxes in the global box list.
///
/////////////////////////////////////////////////////////////////////////////
PAGE_T cLayoutBase::GetNumberOfPages(void) const
{
    return mPageManager->GetNumberOfPages();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  segment [in/out] - segment to mark
/// @param  paragraphStart [in] - document-relative position where paragraph starts
///
/// @return nothing
///
/// @brief
/// Marks segment as being within block range.
/// Sets segment.isBlock flag.
///
/// Converts segment's paragraph-relative position to document-relative,
/// then tests for overlap with block range (from cDocument) and search range
/// (from mLayoutState->GetSearchStart()/mLayoutState->GetSearchEnd()).
///
/// Overlap test: Two ranges [A,B) and [C,D) overlap if A < D AND B > C
///
/// Called during segment creation in PopulateLineSegments.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::MarkSegmentIfInRange(sSegmentLayout& segment,
                                         POSITION_T paragraphStart)
{
    // Convert segment's paragraph-relative position to document-relative
    POSITION_T segmentDocStart = paragraphStart + segment.startPosition;
    POSITION_T segmentDocEnd = segmentDocStart + segment.length;

    // Check if segment overlaps with block range
    if (mDocument)
    {
        POSITION_T blockStart, blockEnd;
        if (mDocument->GetBlock(blockStart, blockEnd))
        {
            // Overlap test: segment intersects block if:
            // segmentStart < blockEnd AND segmentEnd > blockStart
            if (segmentDocStart < blockEnd && segmentDocEnd > blockStart)
            {
                segment.isBlock = true;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  segment [in/out] - segment to analyze for control codes
/// @param  graphemes [in] - all graphemes from the paragraph
///
/// @return nothing
///
/// @brief
/// Marks which glyphs in a segment are control codes for visual display.
/// Sets hasControlCodes flag and populates controlCodeIndices vector.
///
/// Control codes are identified by MARKER_CHAR (127) at the start of a grapheme.
/// The indices stored are relative to the segment's position array, not the
/// full paragraph grapheme array.
///
/// Called during segment creation in PopulateLineSegments.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::MarkControlCodesInSegment(sSegmentLayout& segment,
                                              const std::vector<std::string>& graphemes)
{
    // Initialize
    segment.hasControlCodes = false;
    segment.controlCodeIndices.clear();

    // Check if segment has valid range
    if (segment.startPosition < 0 || segment.length <= 0)
    {
        return;
    }

    // Calculate segment range in paragraph graphemes
    POSITION_T segmentStart = segment.startPosition;
    POSITION_T segmentEnd = segment.startPosition + segment.length;

    // Bounds checking
    if (segmentStart >= static_cast<POSITION_T>(graphemes.size()))
    {
        return;
    }

    if (segmentEnd > static_cast<POSITION_T>(graphemes.size()))
    {
        segmentEnd = static_cast<POSITION_T>(graphemes.size());
    }

    // Scan segment's grapheme range for control codes
    size_t segmentIndex = 0;
    for (POSITION_T i = segmentStart; i < segmentEnd; ++i)
    {
        const std::string& glyph = graphemes[i];

        // Check if this is a control code (starts with MARKER_CHAR, REPLACE_CHAR or SAVE_CHAR)
        if (!glyph.empty() && (glyph[0] == MARKER_CHAR || glyph[0] == REPLACE_CHAR || glyph[0] == SAVE_CHAR))
        {
            segment.hasControlCodes = true;
            segment.controlCodeIndices.push_back(segmentIndex);
        }

        segmentIndex++;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in/out] - line to justify
/// @param  isFinalLineOfParagraph [in] - true if this is the last line of paragraph
///
/// @return nothing
///
/// @brief
/// Applies justification settings to a line by adjusting glyph positions.
///
/// For center alignment: shifts all positions by (boxWidth - lineWidth) / 2
/// For right alignment: shifts all positions by (boxWidth - lineWidth)
/// For full justification: distributes extra space across word separators
///
/// Modifies the position deque in each segment to achieve the alignment.
///
/// Note: Final lines of paragraphs are never justified (left-aligned instead).
/// Trailing spaces are excluded from line width calculations.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::JustifyLine(sLineLayout& line, bool isFinalLineOfParagraph, COORD_T maxLineWidth)
{
    // Skip if no segments or no alignment needed
    if (line.segments.empty())
    {
        return;
    }

    // Left-aligned lines don't need adjustment
    if (line.left)
    {
        return;
    }

    // Use the actual available line width (accounts for paragraph margin)
    COORD_T boxWidth = maxLineWidth;

    // Get line width excluding trailing spaces
    COORD_T lineWidth = 0;

    // Find the last non-space character
    size_t lastNonSpaceSegIndex = 0;
    size_t lastNonSpaceGlyphIndex = 0;
    bool foundNonSpace = false;

    for (size_t segIdx = line.segments.size(); segIdx > 0; --segIdx)
    {
        size_t segIndex = segIdx - 1;
        const auto& segment = line.segments[segIndex];

        // TODO: This needs document access to check for spaces
        // For now, assume last grapheme is non-space
        if (segment.GetGraphemeCount() > 0)
        {
            lastNonSpaceSegIndex = segIndex;
            lastNonSpaceGlyphIndex = segment.GetGraphemeCount() - 1;
            foundNonSpace = true;
            break;
        }

        if (foundNonSpace)
        {
            break;
        }
    }

    // If we found a non-space character, get its position
    if (foundNonSpace && lastNonSpaceGlyphIndex < line.segments[lastNonSpaceSegIndex].position.size())
    {
        lineWidth = line.segments[lastNonSpaceSegIndex].position[lastNonSpaceGlyphIndex];
        // Add width of the last non-space character
        // Fetch grapheme on-demand from document
        std::vector<std::string> graphemes;
        line.segments[lastNonSpaceSegIndex].GetGraphemes(mDocument, graphemes);
        if (lastNonSpaceGlyphIndex < graphemes.size())
        {
            lineWidth += GetTextWidth(graphemes[lastNonSpaceGlyphIndex]);
        }
    }
    else
    {
        return;  // Line is all spaces or empty
    }

    // Handle full justification
    if (line.justify)
    {
        // Don't justify the final line of a paragraph (leave it left-aligned)
        if (isFinalLineOfParagraph)
        {
            return;
        }

        // Calculate space to distribute
        COORD_T extraSpace = boxWidth - lineWidth;

        // Don't justify if line is too short or too close to box width
        if (extraSpace <= 0)
        {
            return;
        }

        // Count word separators (spaces) in the line
        // Fetch graphemes on-demand from document
        int spaceCount = 0;
        for (const auto& segment : line.segments)
        {
            std::vector<std::string> graphemes;
            segment.GetGraphemes(mDocument, graphemes);
            for (const auto& glyph : graphemes)
            {
                if (glyph == " ")
                {
                    spaceCount++;
                }
            }
        }

        // Can't justify without spaces
        if (spaceCount == 0)
        {
            return;
        }

        // Distribute space evenly across all word separators
        COORD_T spacePerSeparator = extraSpace / spaceCount;
        COORD_T accumulatedOffset = 0;

        // Walk through all segments and adjust positions after each space
        // Fetch graphemes on-demand from document
        for (auto& segment : line.segments)
        {
            std::vector<std::string> graphemes;
            segment.GetGraphemes(mDocument, graphemes);

            for (size_t i = 0; i < segment.position.size(); ++i)
            {
                // Apply accumulated offset to this position
                segment.position[i] += accumulatedOffset;

                // If this is a space, add to accumulated offset
                if (i < graphemes.size() && graphemes[i] == " ")
                {
                    accumulatedOffset += spacePerSeparator;
                }
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  currentPosition [in] current X position in twips
///
/// @return position of next tab stop in twips
///
/// @brief
/// Finds the next tab stop position after the given current position.
/// Searches through mLayoutState->GetTabs() vector for first tab stop > currentPosition.
/// Returns the right margin (last tab) if no tab stop found.
///
/////////////////////////////////////////////////////////////////////////////
sTabStop cLayoutBase::FindNextTabStop(COORD_T currentPosition) const
{
    // Find first tab stop > currentPosition
    for (const sTabStop& tab : mLayoutState->GetTabs())
    {
        if (tab.position > currentPosition)
        {
            return tab;
        }
    }

    // No tab found - return right margin (last tab in list)
    // mLayoutState->GetTabs() always includes left margin first and right margin last
    if (!mLayoutState->GetTabs().empty())
    {
        return mLayoutState->GetTabs().back();
    }

    // Fallback: return current position (shouldn't happen if tabs are set up correctly)
    return sTabStop(currentPosition, TAB_TAB);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  currentPosition [in] current position in twips
///
/// @return next tab stop at-or-past currentPosition
///
/// @brief
/// Public wrapper around FindNextTabStop for the TUI renderer's tab-handling
/// path. Renderers need to advance the column to the next tab stop when a
/// tab grapheme is encountered.
///
/////////////////////////////////////////////////////////////////////////////
sTabStop cLayoutBase::GetNextTabStop(COORD_T currentPosition) const
{
    return FindNextTabStop(currentPosition);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  segment [in] - Segment to split
/// @param  splitPosition [in] - Paragraph-relative position where split occurs
///
/// @return std::pair<sSegmentLayout, sSegmentLayout> - First and second segments
///
/// @brief
/// Splits a segment at a grapheme boundary, preserving all measurements and formatting.
/// Pure data manipulation - no mMeasurement happens here.
///
/// @note
/// This is a helper for word wrapping.
/// Positions in segment.position[] are base-0 (relative to segment start).
/// Tab marker stays with first segment; second segment loses tab.
///
/// @see
/// WordWrapSegmentsIntoLines() - caller of this method
///
/////////////////////////////////////////////////////////////////////////////
std::pair<sSegmentLayout, sSegmentLayout> cLayoutBase::SplitSegmentAtPosition(const sSegmentLayout& segment, POSITION_T splitPosition)
{
    // Calculate split index within segment
    // splitPosition is paragraph-relative, segment.startPosition is paragraph-relative
    POSITION_T splitIndex = splitPosition - segment.startPosition;

    // Create first segment (before split point)
    sSegmentLayout segment1;
    segment1.paragraph = segment.paragraph;
    segment1.startPosition = segment.startPosition;
    segment1.length = splitIndex;
    segment1.font = segment.font;
    segment1.textcolor = segment.textcolor;
    segment1.backcolor = segment.backcolor;
    segment1.isBlock = segment.isBlock;
    segment1.isSubscript = segment.isSubscript;
    segment1.isSuperscript = segment.isSuperscript;
    segment1.segmentheight = segment.segmentheight;
    segment1.hasControlCodes = segment.hasControlCodes;

    // Copy position array for first part (0 to splitIndex-1)
    for (POSITION_T i = 0; i < splitIndex; i++)
    {
        if (i < static_cast<POSITION_T>(segment.position.size()))
        {
            segment1.position.push_back(segment.position[i]);
        }
    }

    // Calculate totalWidth from position array
    // totalWidth must be position AFTER last grapheme (where segment2 begins)
    if (splitIndex < static_cast<POSITION_T>(segment.position.size()))
    {
        // Use original segment's position at splitIndex (where segment2 starts)
        // This gives us the position AFTER segment1's last grapheme
        segment1.totalWidth = segment.position[splitIndex];
    }
    else if (segment1.position.size() > 0)
    {
        // Edge case: split at end, use last position
        segment1.totalWidth = segment1.position.back();
    }
    else
    {
        segment1.totalWidth = 0;
    }

    // Tab marker stays with first segment
    segment1.isTab = segment.isTab;
    segment1.tabDocPosition = segment.tabDocPosition;
    segment1.tabWidth = segment.tabWidth;
    segment1.tabType = segment.tabType;
    segment1.tabStopType = segment.tabStopType;

    // Create second segment (after split point)
    sSegmentLayout segment2;
    segment2.paragraph = segment.paragraph;
    segment2.startPosition = splitPosition;
    segment2.length = segment.length - splitIndex;
    segment2.font = segment.font;
    segment2.textcolor = segment.textcolor;
    segment2.backcolor = segment.backcolor;
    segment2.isBlock = segment.isBlock;
    segment2.isSubscript = segment.isSubscript;
    segment2.isSuperscript = segment.isSuperscript;
    segment2.segmentheight = segment.segmentheight;
    segment2.hasControlCodes = segment.hasControlCodes;

    // Filter and rebase control-code indices across the split point.
    // Indices in [0, splitIndex) stay in segment1; indices >= splitIndex
    // move to segment2 with their position rebased to segment2's start.
    for (size_t idx : segment.controlCodeIndices)
    {
        if (idx < static_cast<size_t>(splitIndex))
        {
            segment1.controlCodeIndices.push_back(idx);
        }
        else
        {
            segment2.controlCodeIndices.push_back(idx - static_cast<size_t>(splitIndex));
        }
    }

    // Copy position array for second part (splitIndex to length-1)
    // CRITICAL: Adjust positions to be relative to NEW segment start (base-0)
    COORD_T offsetToSubtract = 0;
    if (splitIndex < static_cast<POSITION_T>(segment.position.size()))
    {
        offsetToSubtract = segment.position[splitIndex];
    }

    for (POSITION_T i = splitIndex; i < segment.length; i++)
    {
        if (i < static_cast<POSITION_T>(segment.position.size()))
        {
            COORD_T adjustedPosition = segment.position[i] - offsetToSubtract;
            segment2.position.push_back(adjustedPosition);
        }
    }

    // Calculate totalWidth from original segment's totalWidth
    // CRITICAL: totalWidth must include the width of the last grapheme
    // position.back() is the LEFT edge of the last grapheme, which is WRONG
    // Instead, calculate from original totalWidth minus the offset we subtracted
    if (segment2.position.size() > 0)
    {
        segment2.totalWidth = segment.totalWidth - offsetToSubtract;
    }
    else
    {
        segment2.totalWidth = 0;
    }

    // Second segment is never a tab (tab stays with first segment)
    segment2.isTab = false;
    segment2.tabDocPosition = 0;
    segment2.tabWidth = 0;
    segment2.tabType = TAB_TAB;
    segment2.tabStopType = TAB_TAB;

    return std::make_pair(segment1, segment2);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  segments [in] - Pre-built segments with measurements from BuildParagraphSegments
/// @param  paragraphNum [in] - Paragraph number being laid out
///
/// @return nothing
///
/// @brief
/// Arranges pre-built, pre-measured segments into lines. All grapheme widths
/// are already measured in segments - only tab widths are calculated during
/// word wrap (position-dependent). Uses pre-measured segment.totalWidth and
/// segment.position[] arrays for all fitting calculations.
///
/// @note
/// NO mMeasurement happens here except for tabs (position-dependent).
/// Word wrap is pure arithmetic on pre-measured widths.
///
/// Algorithm:
/// 1. Get word break positions from document
/// 2. Create first line and track current width
/// 3. For each segment:
///    a. Transfer line-level tab flags (centerLine, rightLine)
///    b. Calculate tab width if segment starts with tab
///    c. Check if segment fits on current line
///    d. If not, split at word boundary or wrap entire segment
///    e. Finalize complete lines
/// 4. Finalize last line
///
/// @see
/// BuildParagraphSegments() - produces the input segments
/// SplitSegmentAtPosition() - called when segment must be split
/// FinalizeLine() - called to complete each line
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::WordWrapSegmentsIntoLines(const std::vector<sSegmentLayout>& segments, PARAGRAPH_T paragraphNum)
{
    // Validate inputs
    if (mDocument == nullptr || segments.empty())
    {
        return;
    }

    // Get word break positions from document
    std::vector<POSITION_T> wordStarts;
    mDocument->GetWordPositions(paragraphNum, wordStarts);

    // Convert from document-relative to paragraph-relative positions
    POSITION_T paragraphStart, paragraphEnd;
    mDocument->GetParagraphStartandEnd(paragraphNum, paragraphStart, paragraphEnd);
    for (size_t i = 0; i < wordStarts.size(); ++i)
    {
        wordStarts[i] -= paragraphStart;
    }

    // Remove hard return from word break list (can't break AT the hard return)
    std::vector<std::string> graphemes;
    std::vector<POSITION_T> offsets;
    mDocument->GetParagraphGraphemes(paragraphNum, graphemes, offsets);
    if (!graphemes.empty() && !graphemes.back().empty() && graphemes.back()[0] == HARD_RETURN)
    {
        if (!wordStarts.empty())
        {
            wordStarts.pop_back();
        }
    }

    // Initialize first line
    sLineLayout currentLine = CreateLine(paragraphNum);
    COORD_T currentLineWidth = 0;

    // Cross-segment word break tracking -- tracks the last valid word break
    // point across all committed segments on the current line, so we can
    // backtrack to it if a future segment overflows with no intra-segment
    // split available. This matches the old layout code's behavior of
    // tracking wrapseg/wrappos across segment boundaries (old layoutbase.cpp
    // lines 2882-2886).
    POSITION_T lastBreakWordPos = -1;  // paragraph-relative word start position
    int lastBreakSegIdx = -1;          // index into currentLine.segments

    // Calculate available widths
    COORD_T baseBoxWidth = mBoxRight - mBoxLeft;

    // Safety clamp: prevent zero or negative wrap width from invalid margins
    // Minimum 720 twips (0.5 inch) ensures word wrap always makes progress
    if (baseBoxWidth < 720)
    {
        baseBoxWidth = 720;
    }

    COORD_T firstLineWidth = baseBoxWidth;
    if (mLayoutState->IsValidParagraphMargin())
    {
        // .pm is absolute from page offset, not relative to .lm
        // pagex currently = po + lm, we need po + pm
        COORD_T pm = mLayoutState->GetParagraphMargin();
        COORD_T lm = mLayoutState->GetLeftMargin();
        currentLine.pagex += (pm - lm);
        firstLineWidth = mBoxRight - currentLine.pagex;
    }

    COORD_T maxLineWidth = firstLineWidth;  // First line uses first line width
    mLayoutState->SetIsFirstLineOfParagraph(true);  // Track first line to apply paragraph margin (auto-cleared by SaveLine)

    // Process segments using deque for easy reprocessing after splits
    std::deque<sSegmentLayout> remainingSegments(segments.begin(), segments.end());

    while (!remainingSegments.empty())
    {
        sSegmentLayout segment = remainingSegments.front();
        remainingSegments.pop_front();

        // Step 3a+3b: Handle tab segments
        if (segment.isTab)
        {
            // TAB_CENTER and TAB_RIGHT are line-alignment markers from the document.
            // They center/right-align the ENTIRE LINE, not individual tab stops.
            if (segment.tabType == TAB_CENTER)
            {
                currentLine.centerLine = true;
            }
            else if (segment.tabType == TAB_RIGHT || segment.tabType == TAB_RIGHT1)
            {
                currentLine.rightLine = true;
            }
            else
            {
                // TAB_TAB / TAB_DECIMAL -- expand to next tab stop
                COORD_T currentLineX = currentLine.pagex + currentLineWidth;

                // Tab stops are margin-relative, currentLineX is absolute page coords.
                // Convert to margin-relative for lookup, then back to absolute.
                COORD_T pageOffset = currentLine.pagex - mLayoutState->GetLeftMargin();
                sTabStop tabStop = FindNextTabStop(currentLineX - pageOffset);
                COORD_T nextTabStop = tabStop.position + pageOffset;

                // Store the tab stop type for column positioning in FinalizeLine
                segment.tabStopType = tabStop.type;

                // Check if tab forces line break (tab stop beyond right margin,
                // or zero/negative width meaning we're at or past all tab stops)
                COORD_T tabWidth = nextTabStop - currentLineX;
                if (nextTabStop > mBoxRight || tabWidth <= 0)
                {
                    // Finalize current line and start new line
                    if (currentLine.segments.size() > 0)
                    {
                        FinalizeLine(currentLine, maxLineWidth);
                        SaveLine(paragraphNum, currentLine);
                    }

                    // Check for page break
                    if (NeedNewPage(currentLine.lineheight))
                    {
                        IncrementPageAndCreateBox();
                    }

                    // Start new line
                    currentLine = CreateLine(paragraphNum);
                    currentLineWidth = 0;
                    maxLineWidth = baseBoxWidth;
                    lastBreakWordPos = -1;
                    lastBreakSegIdx = -1;

                    // Apply paragraph margin if this is still the first line
                    if (mLayoutState->IsFirstLineOfParagraph() && mLayoutState->IsValidParagraphMargin())
                    {
                        // .pm is absolute from page offset, not relative to .lm
                        COORD_T pm = mLayoutState->GetParagraphMargin();
                        COORD_T lm = mLayoutState->GetLeftMargin();
                        currentLine.pagex += (pm - lm);
                        maxLineWidth = mBoxRight - currentLine.pagex;
                    }

                    // Recalculate tab stop from new line position
                    currentLineX = currentLine.pagex;
                    pageOffset = currentLine.pagex - mLayoutState->GetLeftMargin();
                    tabStop = FindNextTabStop(currentLineX - pageOffset);
                    nextTabStop = tabStop.position + pageOffset;
                    segment.tabStopType = tabStop.type;
                    tabWidth = nextTabStop - currentLineX;
                }

                // Store tab width
                segment.tabWidth = tabWidth;
                currentLineWidth += tabWidth;
            }
        }

        // Step 3c: Get segment content width (already measured)
        COORD_T segmentWidth = segment.totalWidth;
        COORD_T availableSpace = maxLineWidth - currentLineWidth;

        // Step 3d: Check if segment fits on current line
        if (segmentWidth <= availableSpace)
        {
            // Segment fits - add to current line
            if (currentLine.segments.empty())
            {
                currentLine.linestart = segment.startPosition;  // Set line start from first segment
            }
            currentLine.segments.push_back(segment);
            currentLineWidth += segmentWidth;

            // Update cross-segment word break tracking for this segment.
            // Record word positions within this segment as potential future
            // break points. If a later segment overflows, we can backtrack
            // to the last recorded break instead of wrapping at a marker
            // boundary.
            {
                int segIdx = static_cast<int>(currentLine.segments.size()) - 1;

                // Find word starts within this segment using binary search
                auto wsIt = std::lower_bound(wordStarts.begin(), wordStarts.end(),
                                             segment.startPosition);
                for (; wsIt != wordStarts.end() &&
                       *wsIt < segment.startPosition + segment.length; ++wsIt)
                {
                    POSITION_T wordPos = *wsIt;
                    POSITION_T splitIdxInSeg = wordPos - segment.startPosition;

                    // Record this word position as a potential break point
                    // (at segment start or within the segment's position range)
                    if (splitIdxInSeg == 0 ||
                        splitIdxInSeg < static_cast<POSITION_T>(segment.position.size()))
                    {
                        lastBreakWordPos = wordPos;
                        lastBreakSegIdx = segIdx;
                    }
                }
            }
        }
        else
        {
            // Segment doesn't fit - try to split at word boundary

            // Find word boundaries within this segment
            std::vector<POSITION_T> wordsInSegment;
            for (POSITION_T wordPos : wordStarts)
            {
                if (wordPos > segment.startPosition && wordPos < segment.startPosition + segment.length)
                {
                    wordsInSegment.push_back(wordPos);
                }
            }

            // Try to find best split position that fits
            POSITION_T bestSplit = -1;  // -1 means no split found

            for (POSITION_T wordPos : wordsInSegment)
            {
                // Calculate width from segment start to this word boundary
                POSITION_T splitIndexInSegment = wordPos - segment.startPosition;
                COORD_T widthToSplit;

                if (splitIndexInSegment == 0)
                {
                    widthToSplit = 0;
                }
                else if (splitIndexInSegment >= segment.length)
                {
                    widthToSplit = segment.totalWidth;
                }
                else
                {
                    // Use position array: position[N] is the cumulative width up to grapheme N
                    widthToSplit = segment.position[splitIndexInSegment];
                }

                if (widthToSplit <= availableSpace)
                {
                    bestSplit = wordPos;  // This split fits, keep looking for better
                }
                else
                {
                    break;  // This split doesn't fit, use previous best
                }
            }

            if (bestSplit > segment.startPosition)
            {
                // Found a split position - split segment
                auto [seg1, seg2] = SplitSegmentAtPosition(segment, bestSplit);

                // Add first part to current line
                if (currentLine.segments.empty())
                {
                    currentLine.linestart = seg1.startPosition;  // Set line start from first segment
                }
                currentLine.segments.push_back(seg1);
                currentLineWidth += seg1.totalWidth;

                // Finalize current line
                FinalizeLine(currentLine, maxLineWidth);
                SaveLine(paragraphNum, currentLine);

                // Check for page break
                // Check for page break
                if (NeedNewPage(currentLine.lineheight))
                {
                    IncrementPageAndCreateBox();
                }

                // Start new line
                currentLine = CreateLine(paragraphNum);
                currentLineWidth = 0;
                maxLineWidth = baseBoxWidth;  // No paragraph margin on wrapped lines
                lastBreakWordPos = -1;
                lastBreakSegIdx = -1;

                // Apply paragraph margin if this is still the first line
                if (mLayoutState->IsFirstLineOfParagraph() && mLayoutState->IsValidParagraphMargin())
                {
                    // .pm is absolute from page offset, not relative to .lm
                    COORD_T pm = mLayoutState->GetParagraphMargin();
                    COORD_T lm = mLayoutState->GetLeftMargin();
                    currentLine.pagex += (pm - lm);
                    maxLineWidth = mBoxRight - currentLine.pagex;
                }

                // Push second part back to front of queue for reprocessing
                remainingSegments.push_front(seg2);

            }
            else
            {
                // No word boundary to split at within this segment.
                // Try cross-segment backtracking: if we have a valid break
                // point in a previously committed segment, backtrack to it
                // instead of wrapping at the segment boundary (which may be
                // a marker position, not a word boundary).
                bool backtracked = false;

                if (lastBreakWordPos >= 0 && lastBreakSegIdx >= 0 &&
                    !currentLine.segments.empty())
                {
                    // Check if wrapping at the segment boundary is already a
                    // clean word break. Walk backward from the overflowing
                    // segment past any markers to find the last text character.
                    // If it's a space, the break is clean -- no backtracking
                    // needed. Only backtrack when the marker is mid-word
                    // (e.g., "Hel^PBlo" where wrapping at ^PB splits "Hello").
                    bool isCleanBreak = false;
                    {
                        // Clamp the starting index so graphemes[checkPos] is
                        // always in bounds even if a caller hands us a segment
                        // whose startPosition exceeds the paragraph's grapheme
                        // count. The post-decrement form below walks
                        // start-1, start-2, ..., 0 inclusive while keeping the
                        // step-guard explicit (and not entangled with the
                        // entry-validation that the clamp now owns).
                        POSITION_T checkPos = std::min<POSITION_T>(
                            segment.startPosition,
                            static_cast<POSITION_T>(graphemes.size()));
                        while (checkPos-- > 0)
                        {
                            const std::string& g = graphemes[checkPos];
                            if (!g.empty() &&
                                g[0] != MARKER_CHAR && g[0] != REPLACE_CHAR && g[0] != SAVE_CHAR)
                            {
                                isCleanBreak = (g == " ");
                                break;
                            }
                        }
                    }

                    // Verify this break wouldn't leave the line empty
                    bool wouldBeEmpty = (lastBreakSegIdx == 0 &&
                                        lastBreakWordPos ==
                                            currentLine.segments[0].startPosition);

                    if (!wouldBeEmpty && !isCleanBreak)
                    {
                        backtracked = true;

                        // Collect segments to push back to the queue
                        std::deque<sSegmentLayout> pushBack;
                        pushBack.push_back(segment);  // The overflowing segment

                        if (lastBreakWordPos ==
                            currentLine.segments[lastBreakSegIdx].startPosition)
                        {
                            // Break at the start of a committed segment --
                            // remove it and all segments after it from the line
                            for (int i = static_cast<int>(currentLine.segments.size()) - 1;
                                 i >= lastBreakSegIdx; --i)
                            {
                                pushBack.push_front(currentLine.segments[i]);
                            }
                            currentLine.segments.erase(
                                currentLine.segments.begin() + lastBreakSegIdx,
                                currentLine.segments.end());
                        }
                        else
                        {
                            // Break within a committed segment -- split it
                            auto [seg1, seg2] = SplitSegmentAtPosition(
                                currentLine.segments[lastBreakSegIdx],
                                lastBreakWordPos);

                            // Remove segments after the split point
                            for (int i = static_cast<int>(currentLine.segments.size()) - 1;
                                 i > lastBreakSegIdx; --i)
                            {
                                pushBack.push_front(currentLine.segments[i]);
                            }

                            // Remove the original segment at lastBreakSegIdx
                            // and replace with the first half of the split
                            currentLine.segments.erase(
                                currentLine.segments.begin() + lastBreakSegIdx,
                                currentLine.segments.end());
                            currentLine.segments.push_back(seg1);

                            // Add second half to front of push-back queue
                            pushBack.push_front(seg2);
                        }

                        // Finalize current line at the break point
                        FinalizeLine(currentLine, maxLineWidth);
                        SaveLine(paragraphNum, currentLine);

                        // Check for page break
                        if (NeedNewPage(currentLine.lineheight))
                        {
                            IncrementPageAndCreateBox();
                        }

                        // Start new line
                        currentLine = CreateLine(paragraphNum);
                        currentLineWidth = 0;
                        maxLineWidth = baseBoxWidth;
                        lastBreakWordPos = -1;
                        lastBreakSegIdx = -1;

                        // Apply paragraph margin if needed
                        if (mLayoutState->IsFirstLineOfParagraph() &&
                            mLayoutState->IsValidParagraphMargin())
                        {
                            COORD_T pm = mLayoutState->GetParagraphMargin();
                            COORD_T lm = mLayoutState->GetLeftMargin();
                            currentLine.pagex += (pm - lm);
                            maxLineWidth = mBoxRight - currentLine.pagex;
                        }

                        // Push segments back to front of queue for reprocessing
                        remainingSegments.insert(remainingSegments.begin(),
                                                 pushBack.begin(), pushBack.end());
                    }
                }

                if (!backtracked)
                {
                    if (currentLine.segments.empty())
                    {
                        // Try character-boundary split (Word/LibreOffice
                        // style) before resorting to forced overflow.
                        COORD_T charAvailable = maxLineWidth - currentLineWidth;
                        POSITION_T charSplit = 0;
                        for (POSITION_T i = 1; i < segment.length; i++)
                        {
                            if (i < static_cast<POSITION_T>(segment.position.size()) &&
                                segment.position[i] <= charAvailable)
                            {
                                charSplit = i;
                            }
                            else
                            {
                                break;
                            }
                        }

                        if (charSplit > 0)
                        {
                            auto [seg1, seg2] = SplitSegmentAtPosition(
                                segment, segment.startPosition + charSplit);
                            currentLine.linestart = seg1.startPosition;
                            currentLine.segments.push_back(seg1);
                            currentLineWidth += seg1.totalWidth;
                            remainingSegments.push_front(seg2);
                        }
                        else
                        {
                            // Can't fit even one grapheme - forced overflow
                            currentLine.linestart = segment.startPosition;
                            currentLine.segments.push_back(segment);
                            currentLineWidth += segmentWidth;
                        }

                        // Finalize line immediately
                        FinalizeLine(currentLine, maxLineWidth);
                        SaveLine(paragraphNum, currentLine);

                        // Check for page break
                        if (NeedNewPage(currentLine.lineheight))
                        {
                            IncrementPageAndCreateBox();
                        }

                        // Start new line
                        currentLine = CreateLine(paragraphNum);
                        currentLineWidth = 0;
                        maxLineWidth = baseBoxWidth;
                        lastBreakWordPos = -1;
                        lastBreakSegIdx = -1;

                        // Apply paragraph margin if this is still the first line
                        if (mLayoutState->IsFirstLineOfParagraph() &&
                            mLayoutState->IsValidParagraphMargin())
                        {
                            COORD_T pm = mLayoutState->GetParagraphMargin();
                            COORD_T lm = mLayoutState->GetLeftMargin();
                            currentLine.pagex += (pm - lm);
                            maxLineWidth = mBoxRight - currentLine.pagex;
                        }
                    }
                    else
                    {
                        // Line has content - finalize WITHOUT this segment,
                        // then start a new line and re-process the segment
                        FinalizeLine(currentLine, maxLineWidth);
                        SaveLine(paragraphNum, currentLine);

                        // Check for page break
                        if (NeedNewPage(currentLine.lineheight))
                        {
                            IncrementPageAndCreateBox();
                        }

                        // Start new line
                        currentLine = CreateLine(paragraphNum);
                        currentLineWidth = 0;
                        maxLineWidth = baseBoxWidth;
                        lastBreakWordPos = -1;
                        lastBreakSegIdx = -1;

                        // Apply paragraph margin if this is still the first line
                        if (mLayoutState->IsFirstLineOfParagraph() &&
                            mLayoutState->IsValidParagraphMargin())
                        {
                            COORD_T pm = mLayoutState->GetParagraphMargin();
                            COORD_T lm = mLayoutState->GetLeftMargin();
                            currentLine.pagex += (pm - lm);
                            maxLineWidth = mBoxRight - currentLine.pagex;
                        }

                        // Push overflowing segment back to queue for
                        // re-processing with full line width available.
                        // This ensures intra-segment word wrap logic runs
                        // on the segment instead of adding it directly at
                        // full width (which would bypass word wrap).
                        remainingSegments.push_front(segment);
                    }
                }
            }
        }
    }

    // Finalize last line if it has content (this is the final line of the paragraph)
    if (currentLine.segments.size() > 0)
    {
        FinalizeLine(currentLine, maxLineWidth, true);
        SaveLine(paragraphNum, currentLine);

        // Check for page break after final line (same pattern as mid-paragraph breaks)
        // Without this, single-line paragraphs (e.g. blank lines) never trigger page breaks
        if (NeedNewPage(currentLine.lineheight))
        {
            IncrementPageAndCreateBox();
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line [in/out] - Line to finalize with segments already added
/// @param  maxLineWidth [in] - Maximum line width for alignment calculations
///
/// @return nothing
///
/// @brief
/// Completes line setup after segments are added. Applies justification/alignment
/// and positions segments, converting segment-relative coordinates to line-relative.
///
/// @note
/// This is a helper for word wrapping.
/// Segments must already be added to line before calling.
/// Converts segment.position[] from segment-relative to line-relative coordinates.
///
/// @see
/// WordWrapSegmentsIntoLines() - caller of this method
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::FinalizeLine(sLineLayout& line, COORD_T maxLineWidth, bool isFinalLine)
{
    // Calculate total content width
    COORD_T totalWidth = 0;
    for (const auto& segment : line.segments)
    {
        if (segment.isTab)
        {
            totalWidth += segment.tabWidth;
        }
        totalWidth += segment.totalWidth;
    }

    // Calculate available width and remaining space
    COORD_T availableWidth = maxLineWidth;
    COORD_T remainingSpace = availableWidth - totalWidth;
    if (remainingSpace < 0)
    {
        remainingSpace = 0;  // Line is overfull
    }

    // Determine alignment offset
    // Line-level tab flags OVERRIDE paragraph alignment
    COORD_T alignmentOffset = 0;

    if (line.centerLine && line.rightLine)
    {
        // Both flags set - right takes precedence
        alignmentOffset = remainingSpace;
    }
    else if (line.centerLine)
    {
        // Center this line only
        alignmentOffset = remainingSpace / 2;
    }
    else if (line.rightLine)
    {
        // Right-align this line only
        alignmentOffset = remainingSpace;
    }
    else
    {
        // Use standard paragraph alignment
        if (mLayoutState->GetModifiers().center)
        {
            alignmentOffset = remainingSpace / 2;
        }
        else if (mLayoutState->GetModifiers().right)
        {
            alignmentOffset = remainingSpace;
        }
        else if (mLayoutState->GetModifiers().justify)
        {
            // Justification handled separately (distribute space between segments)
            alignmentOffset = 0;
        }
        else
        {
            // Left align (default)
            alignmentOffset = 0;
        }
    }

    // Position segments within line
    // Convert segment-relative positions to line-relative positions
    COORD_T currentX = alignmentOffset;

    for (size_t si = 0; si < line.segments.size(); ++si)
    {
        auto& segment = line.segments[si];

        if (segment.isTab)
        {
            // Position the display glyph (if SHOW_ALL)
            for (size_t i = 0; i < segment.position.size(); ++i)
            {
                segment.position[i] += currentX;
            }

            // TAB_CENTER/TAB_RIGHT are line-alignment markers -- no expanding width.
            // Their effect is handled by alignmentOffset above via centerLine/rightLine.
            // Only TAB_TAB and TAB_DECIMAL advance currentX by tabWidth.
            if (segment.tabType == TAB_TAB || segment.tabType == TAB_DECIMAL)
            {
                // Advance to the tab stop position
                currentX += segment.tabWidth;

                // Apply tab stop column positioning based on tabStopType
                // tabStopType is from .tb/.tab definitions, NOT the document tab character
                if (segment.tabStopType == TAB_CENTER)
                {
                    // Center tab stop: center following text around the tab stop position
                    COORD_T followingWidth = 0;
                    for (size_t fi = si + 1; fi < line.segments.size() && !line.segments[fi].isTab; ++fi)
                    {
                        followingWidth += line.segments[fi].totalWidth;
                    }
                    currentX -= followingWidth / 2;
                }
                else if (segment.tabStopType == TAB_RIGHT)
                {
                    // Right tab stop: right-align following text to the tab stop position
                    COORD_T followingWidth = 0;
                    for (size_t fi = si + 1; fi < line.segments.size() && !line.segments[fi].isTab; ++fi)
                    {
                        followingWidth += line.segments[fi].totalWidth;
                    }
                    currentX -= followingWidth;
                }
                else if (segment.tabStopType == TAB_DECIMAL || segment.tabType == TAB_DECIMAL)
                {
                    // Decimal tab stop: align following text on the decimal point (".")
                    // Text before "." is right-aligned to tab stop, "." sits at stop,
                    // text after "." flows right from stop.
                    // If no "." found, right-align all text (same as right tab).
                    COORD_T widthBeforeDecimal = 0;
                    bool foundDecimal = false;

                    for (size_t fi = si + 1; fi < line.segments.size() && !line.segments[fi].isTab; ++fi)
                    {
                        // Get graphemes to search for "."
                        std::vector<std::string> graphemes;
                        line.segments[fi].GetGraphemes(mDocument, graphemes);

                        for (size_t gi = 0; gi < graphemes.size(); ++gi)
                        {
                            if (graphemes[gi] == ".")
                            {
                                // position[gi] is base-0 offset of "." within this segment
                                if (gi < line.segments[fi].position.size())
                                {
                                    widthBeforeDecimal += line.segments[fi].position[gi];
                                }
                                foundDecimal = true;
                                break;
                            }
                        }

                        if (foundDecimal)
                        {
                            break;
                        }

                        // No decimal in this segment -- accumulate full segment width
                        widthBeforeDecimal += line.segments[fi].totalWidth;
                    }

                    if (!foundDecimal)
                    {
                        // No decimal point -- right-align all following text
                        widthBeforeDecimal = 0;
                        for (size_t fi = si + 1; fi < line.segments.size() && !line.segments[fi].isTab; ++fi)
                        {
                            widthBeforeDecimal += line.segments[fi].totalWidth;
                        }
                    }

                    currentX -= widthBeforeDecimal;
                }
                // else TAB_TAB: left-aligned, no adjustment needed
            }
            else
            {
                // TAB_CENTER / TAB_RIGHT / TAB_RIGHT1 -- line-alignment markers
                // Just advance by display char width (the marker glyph itself)
                currentX += segment.totalWidth;
            }

            continue;
        }

        // Convert segment-relative positions to line-relative positions
        for (size_t i = 0; i < segment.position.size(); ++i)
        {
            segment.position[i] += currentX;
        }

        // Move to next segment position
        currentX += segment.totalWidth;
    }

    // Apply justification spacing if needed
    if (mLayoutState->GetModifiers().justify && !line.centerLine && !line.rightLine)
    {
        JustifyLine(line, isFinalLine, maxLineWidth);
    }

    // Line is now finalized and ready for rendering
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Resets all formatting state to defaults.
/// Used before replaying dot commands in partial layout.
///
/// Resets margins, tabs, modifiers, line spacing, page settings, etc.
/// Does NOT reset box tracking variables (mLastBoxLeftMargin, etc.) as
/// those are managed separately by box creation logic.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::ResetFormattingState(void)
{
    // Reset margins to defaults
    mLayoutState->SetLeftMargin(0);
    mLayoutState->SetRightMargin(9360);  // 6.5 inches
    mLayoutState->SetPageOffsetOdd(1440);   // 1 inch
    mLayoutState->SetPageOffsetEven(1440);  // 1 inch
    mLayoutState->SetTopMargin(1440);       // 1 inch (matches layoutstate.cpp constructor)
    mLayoutState->SetBottomMargin(1440);    // 1 inch
    mLayoutState->SetHeaderMargin(720);     // 0.5 inch
    mLayoutState->SetFooterMargin(720);     // 0.5 inch

    // Reset paragraph margin
    mLayoutState->SetParagraphMargin(0);
    mLayoutState->SetValidParagraphMargin(false);

    // Reset tabs to WordStar defaults (0.5" intervals)
    std::vector<sTabStop> tabs;
    tabs.push_back(sTabStop(0, TAB_TAB));  // Left margin
    COORD_T tabsize = TWIPSPERINCH / 2;  // 720 twips = 0.5"
    for (short loop = 0; loop < 12; loop++)
    {
        tabs.push_back(sTabStop(tabsize, TAB_TAB));
        tabsize += TWIPSPERINCH / 2;
    }
    tabs.push_back(sTabStop(mLayoutState->GetRightMargin(), TAB_TAB));  // Right margin
    mLayoutState->SetTabs(tabs);

    // Reset text modifiers
    sModifiers modifiers;
    modifiers.justify = false;
    modifiers.left = true;
    modifiers.right = false;
    modifiers.center = false;
    modifiers.linespace = 1.0;
    mLayoutState->SetModifiers(modifiers);

    // Reset font to default
    mLayoutState->SetCurrentFont(mLayoutState->GetDefaultFont());

    // Reset text formatting state
    mLayoutState->SetBoldActive(false);
    mLayoutState->SetItalicActive(false);
    mLayoutState->SetUnderlineActive(false);
    mLayoutState->SetStrikethroughActive(false);
    mLayoutState->SetSuperscriptActive(false);
    mLayoutState->SetSubscriptActive(false);
    mLayoutState->SetCurrentColor(mLayoutState->GetDefaultTextColor());

    // Reset line settings
    mLayoutState->SetLineHeight(NOT_SET);   // Use font-based lineSpacing() by default
    mLayoutState->SetAutoLeading(false);

    // Reset other formatting state
    mLayoutState->SetWordWrapEnabled(true);
    mLayoutState->SetPageNumberOffset(0);
    mLayoutState->SetPageNumFormat(PAGE_NUM_ARABIC);
    mLayoutState->SetPrintPageNumbers(true);
    mLayoutState->SetLandscapeMode(false);
    mLayoutState->SetPageLength(0);

    // Reset paragraph spacing
    mLayoutState->SetParagraphSpacingBefore(0);
    mLayoutState->SetParagraphSpacingAfter(0);

    // Reset conditional page break
    mLayoutState->SetDoNewPage(false);

    // Reset header/footer tracking
    mHeaderFooterManager->SetHeaderValue(0);
    mHeaderFooterManager->SetFooterValue(0);

    // Reset subscript/superscript roll to default (3/48 inch = 90 twips)
    mLayoutState->SetSubSuperRoll(90);

    // Reset paper size to document's initial dimensions (before any .PR swaps)
    mLayoutState->SetPaperWidth(mInitialPaperWidth);
    mLayoutState->SetPaperHeight(mInitialPaperHeight);

    // NOTE: Do NOT reset mLastBoxLeftMargin, mLastBoxRightMargin, mLastBoxPageOffset
    // Those are box tracking state, not formatting state
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - paragraph number this checkpoint is taken after
///
/// @return nothing
///
/// @brief
/// Snapshot current formatting state into a checkpoint for fast restoration.
/// Called during LayoutDocument() every CHECKPOINT_INTERVAL paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SaveFormattingCheckpoint(PARAGRAPH_T para)
{
    sFormattingCheckpoint cp;
    cp.paragraph = para;

    // Margins
    cp.leftMargin = mLayoutState->GetLeftMargin();
    cp.rightMargin = mLayoutState->GetRightMargin();
    cp.topMargin = mLayoutState->GetTopMargin();
    cp.bottomMargin = mLayoutState->GetBottomMargin();
    cp.pageOffsetOdd = mLayoutState->GetPageOffsetOdd();
    cp.pageOffsetEven = mLayoutState->GetPageOffsetEven();
    cp.headerMargin = mLayoutState->GetHeaderMargin();
    cp.footerMargin = mLayoutState->GetFooterMargin();
    cp.paragraphMargin = mLayoutState->GetParagraphMargin();
    cp.validParagraphMargin = mLayoutState->IsValidParagraphMargin();

    // Tab stops
    cp.tabStops = mLayoutState->GetTabs();

    // Text modifiers
    cp.modifiers = mLayoutState->GetModifiers();

    // Line settings
    cp.lineHeight = mLayoutState->GetLineHeight();
    cp.autoLeading = mLayoutState->IsAutoLeading();

    // Other formatting state
    cp.wordWrapEnabled = mLayoutState->IsWordWrapEnabled();
    cp.pageNumberOffset = mLayoutState->GetPageNumberOffset();
    cp.pageNumFormat = mLayoutState->GetPageNumFormat();
    cp.printPageNumbers = mLayoutState->ShouldPrintPageNumbers();
    cp.landscapeMode = mLayoutState->IsLandscapeMode();
    cp.pageLength = mLayoutState->GetPageLength();
    cp.paragraphSpacingBefore = mLayoutState->GetParagraphSpacingBefore();
    cp.paragraphSpacingAfter = mLayoutState->GetParagraphSpacingAfter();
    cp.doNewPage = mLayoutState->ShouldDoNewPage();

    // Subscript/superscript roll distance (.SR command)
    cp.subSuperRoll = mLayoutState->GetSubSuperRoll();

    // Paper dimensions (can be swapped by .PR mLandscape/portrait)
    cp.paperWidth = mLayoutState->GetPaperWidth();
    cp.paperHeight = mLayoutState->GetPaperHeight();

    // Header/footer tracking
    cp.headerValue = mHeaderFooterManager->GetHeaderValue();
    cp.footerValue = mHeaderFooterManager->GetFooterValue();

    mFormattingCheckpoints.push_back(cp);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sFormattingCheckpoint& checkpoint [in] - checkpoint to restore
///
/// @return nothing
///
/// @brief
/// Restore formatting state from a checkpoint snapshot.
/// Used by ApplyPreviousDotCommands() to start replay from a checkpoint
/// instead of paragraph 0.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::RestoreFormattingCheckpoint(const sFormattingCheckpoint& cp)
{
    // Margins
    mLayoutState->SetLeftMargin(cp.leftMargin);
    mLayoutState->SetRightMargin(cp.rightMargin);
    mLayoutState->SetTopMargin(cp.topMargin);
    mLayoutState->SetBottomMargin(cp.bottomMargin);
    mLayoutState->SetPageOffsetOdd(cp.pageOffsetOdd);
    mLayoutState->SetPageOffsetEven(cp.pageOffsetEven);
    mLayoutState->SetHeaderMargin(cp.headerMargin);
    mLayoutState->SetFooterMargin(cp.footerMargin);
    mLayoutState->SetParagraphMargin(cp.paragraphMargin);
    mLayoutState->SetValidParagraphMargin(cp.validParagraphMargin);

    // Tab stops
    mLayoutState->SetTabs(cp.tabStops);

    // Text modifiers
    mLayoutState->SetModifiers(cp.modifiers);

    // Line settings
    mLayoutState->SetLineHeight(cp.lineHeight);
    mLayoutState->SetAutoLeading(cp.autoLeading);

    // Other formatting state
    mLayoutState->SetWordWrapEnabled(cp.wordWrapEnabled);
    mLayoutState->SetPageNumberOffset(cp.pageNumberOffset);
    mLayoutState->SetPageNumFormat(cp.pageNumFormat);
    mLayoutState->SetPrintPageNumbers(cp.printPageNumbers);
    mLayoutState->SetLandscapeMode(cp.landscapeMode);
    mLayoutState->SetPageLength(cp.pageLength);
    mLayoutState->SetParagraphSpacingBefore(cp.paragraphSpacingBefore);
    mLayoutState->SetParagraphSpacingAfter(cp.paragraphSpacingAfter);
    mLayoutState->SetDoNewPage(cp.doNewPage);

    // Subscript/superscript roll distance
    mLayoutState->SetSubSuperRoll(cp.subSuperRoll);

    // Paper dimensions
    mLayoutState->SetPaperWidth(cp.paperWidth);
    mLayoutState->SetPaperHeight(cp.paperHeight);

    // Header/footer tracking
    mHeaderFooterManager->SetHeaderValue(cp.headerValue);
    mHeaderFooterManager->SetFooterValue(cp.footerValue);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number just laid out
///
/// @return bool - true if checkpoint at this boundary matches old (state unchanged)
///
/// @brief
/// Called after each paragraph is laid out during idle layout.
/// If para is at a checkpoint boundary (every CHECKPOINT_INTERVAL paragraphs),
/// builds a new checkpoint from current formatting state and compares with
/// the existing checkpoint. If they match, formatting state is unchanged
/// and idle layout can stop early. If not, replaces the old checkpoint.
///
/// Sets mLastCheckpointMatched for IdleLayout to query via LastCheckpointMatched().
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::UpdateCheckpointIfNeeded(PARAGRAPH_T para)
{
    mLastCheckpointMatched = false;

    // Only act at checkpoint boundaries
    if ((para + 1) % CHECKPOINT_INTERVAL != 0)
    {
        return false;
    }

    // Build new checkpoint from current formatting state
    sFormattingCheckpoint newCp;
    newCp.paragraph = para;
    newCp.leftMargin = mLayoutState->GetLeftMargin();
    newCp.rightMargin = mLayoutState->GetRightMargin();
    newCp.topMargin = mLayoutState->GetTopMargin();
    newCp.bottomMargin = mLayoutState->GetBottomMargin();
    newCp.pageOffsetOdd = mLayoutState->GetPageOffsetOdd();
    newCp.pageOffsetEven = mLayoutState->GetPageOffsetEven();
    newCp.headerMargin = mLayoutState->GetHeaderMargin();
    newCp.footerMargin = mLayoutState->GetFooterMargin();
    newCp.paragraphMargin = mLayoutState->GetParagraphMargin();
    newCp.validParagraphMargin = mLayoutState->IsValidParagraphMargin();
    newCp.tabStops = mLayoutState->GetTabs();
    newCp.modifiers = mLayoutState->GetModifiers();
    newCp.lineHeight = mLayoutState->GetLineHeight();
    newCp.autoLeading = mLayoutState->IsAutoLeading();
    newCp.wordWrapEnabled = mLayoutState->IsWordWrapEnabled();
    newCp.pageNumberOffset = mLayoutState->GetPageNumberOffset();
    newCp.pageNumFormat = mLayoutState->GetPageNumFormat();
    newCp.printPageNumbers = mLayoutState->ShouldPrintPageNumbers();
    newCp.landscapeMode = mLayoutState->IsLandscapeMode();
    newCp.pageLength = mLayoutState->GetPageLength();
    newCp.paragraphSpacingBefore = mLayoutState->GetParagraphSpacingBefore();
    newCp.paragraphSpacingAfter = mLayoutState->GetParagraphSpacingAfter();
    newCp.doNewPage = mLayoutState->ShouldDoNewPage();
    newCp.subSuperRoll = mLayoutState->GetSubSuperRoll();
    newCp.paperWidth = mLayoutState->GetPaperWidth();
    newCp.paperHeight = mLayoutState->GetPaperHeight();
    newCp.headerValue = mHeaderFooterManager->GetHeaderValue();
    newCp.footerValue = mHeaderFooterManager->GetFooterValue();

    // Find existing checkpoint at this boundary
    for (size_t i = 0; i < mFormattingCheckpoints.size(); ++i)
    {
        if (mFormattingCheckpoints[i].paragraph == para)
        {
            // Compare with existing checkpoint
            if (newCp == mFormattingCheckpoints[i])
            {
                // Formatting state unchanged at this boundary
                mLastCheckpointMatched = true;
                return true;
            }

            // State changed -- replace old checkpoint
            mFormattingCheckpoints[i] = newCp;
            return false;
        }
    }

    // No existing checkpoint at this boundary -- insert new one
    // Keep checkpoints sorted by paragraph number
    auto it = mFormattingCheckpoints.begin();
    while (it != mFormattingCheckpoints.end() && it->paragraph < para)
    {
        ++it;
    }
    mFormattingCheckpoints.insert(it, newCp);
    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  PARAGRAPH_T para [in] - Paragraph number from which to invalidate
///
/// @return nothing
///
/// @brief
/// Removes all checkpoints at or after the given paragraph.
/// Called when a dot command paragraph is edited, since formatting state
/// after that point may have changed. Text-only edits do NOT call this.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::InvalidateCheckpointsFrom(PARAGRAPH_T para)
{
    // Remove all checkpoints where checkpoint.paragraph >= para
    mFormattingCheckpoints.erase(
        std::remove_if(mFormattingCheckpoints.begin(), mFormattingCheckpoints.end(),
            [para](const sFormattingCheckpoint& cp)
            {
                return cp.paragraph >= para;
            }),
        mFormattingCheckpoints.end());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  targetPara [in] - Target paragraph number
///
/// @return nothing
///
/// @brief
/// Applies all dot commands from paragraph 0 to targetPara-1.
/// Establishes correct formatting state for laying out targetPara.
///
/// Used in partial layout to replay previous commands and get correct
/// margins, tabs, justification, line spacing, etc.
///
/// Does NOT create boxes - only updates formatting state.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::ApplyPreviousDotCommands(PARAGRAPH_T targetPara)
{
#ifdef DETAIL_LAYOUT_TIMER
    cTimer replayTimer;
    replayTimer.start();
#endif

    // Find nearest checkpoint before targetPara to avoid replaying from paragraph 0
    // Checkpoints are built during LayoutDocument() at CHECKPOINT_INTERVAL intervals
    PARAGRAPH_T startPara = 0;
    if (!mFormattingCheckpoints.empty())
    {
        // Binary search: find the last checkpoint at or before targetPara-1
        for (auto it = mFormattingCheckpoints.rbegin(); it != mFormattingCheckpoints.rend(); ++it)
        {
            if (it->paragraph < targetPara)
            {
                // Restore this checkpoint and start replaying from the next paragraph
                RestoreFormattingCheckpoint(*it);
                startPara = it->paragraph + 1;
                break;
            }
        }
    }

    // If no checkpoint found, reset to defaults and start from paragraph 0
    if (startPara == 0)
    {
        ResetFormattingState();
    }

    // Replay dot commands from startPara to targetPara-1
    // Only apply state-setting commands, skip commands with layout side effects
    //
    // OPTIMIZATION: Use cached isCommand flag from mParagraphLayout to quickly skip
    // non-command paragraphs. This avoids O(n) document reads per paragraph.
    // Only read document text for actual dot commands.
    for (PARAGRAPH_T p = startPara; p < targetPara; p++)
    {
        // Use cached isCommand flag to quickly skip non-commands
        // This is the key optimization - avoid reading text for every paragraph
        if (p < static_cast<PARAGRAPH_T>(mParagraphLayout.size()) && !mParagraphLayout[p].isCommand)
        {
            continue;  // Not a dot command, skip quickly
        }

        // Either this is a dot command, or we haven't laid out this paragraph yet
        // (during first layout). Read the text to check.
        std::string text = mDocument->GetParagraphText(p);

        // Guard: must be a dot command with at least 3 characters (e.g. ".lm")
        if (text.length() < 3 || text[0] != '.')
        {
            continue;
        }

        // Extract command code (uppercase)
        char c1 = toupper(text[1]);
        char c2 = toupper(text[2]);

        // Skip commands that have layout side effects (create pages, insert headers/footers)
        // - .PA (page break) - creates new page
        // - .CP (conditional page break) - may create new page
        // - .HE, .H1-.H5 (headers) - inserts header text
        // - .FO, .F1-.F5 (footers) - inserts footer text
        // These are handled during normal paragraph layout
        if (c1 == 'P' && c2 == 'A')
        {
            continue;  // Skip .PA
        }
        if (c1 == 'P' && c2 == 'N')
        {
            continue;  // Skip .PN -- per-page overrides are authoritative; replay would
                       // add duplicate overrides and calculate wrong offset (mCurrentPage
                       // doesn't match the page where .pn originally appeared)
        }
        if (c1 == 'C' && c2 == 'P')
        {
            continue;  // Skip .CP
        }
        if (c1 == 'H' && (c2 == 'E' || (c2 >= '1' && c2 <= '5')))
        {
            continue;  // Skip .HE, .H1-.H5
        }
        if (c1 == 'F' && (c2 == 'O' || (c2 >= '1' && c2 <= '5')))
        {
            continue;  // Skip .FO, .F1-.F5
        }

        // Apply state-setting commands (margins, tabs, alignment, linespace, etc.)
        ParseDotCommand(text);
    }

    // NOTE: Do NOT sync mLastBoxLeftMargin here
    // We're establishing formatting state, not creating boxes
    // Box selection happens separately in FindBoxForParagraph()

#ifdef DETAIL_LAYOUT_TIMER
    mApplyPreviousDotCommandsTimeNs += replayTimer.time_elapsed_nanoseconds();
#endif
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para [in] - Paragraph number to find box for
///
/// @return box index in mGlobalBoxList, or 0 if not found
///
/// @brief
/// Finds which existing box a paragraph should use for partial layout.
///
/// Strategy:
/// 1. If paragraph has existing lines, use their box
/// 2. Otherwise, find previous text paragraph and check if margins match
/// 3. If margins match, use previous paragraph's box
/// 4. If margins changed, search forward for box with matching margins
/// 5. Fallback: use previous paragraph's box
///
/// Used in partial layout to reuse existing box structure.
///
/////////////////////////////////////////////////////////////////////////////
int cLayoutBase::FindBoxForParagraph(PARAGRAPH_T para)
{
    // Strategy 1: If paragraph has existing lines, use their box
    const sParagraphLayout* paraLayout = GetParagraphLayout(para);
    if (paraLayout && !paraLayout->lines.empty())
    {
        return paraLayout->lines[0].boxIndex;
    }

    // Strategy 2: Find previous text paragraph, use its box
    for (PARAGRAPH_T p = para - 1; p >= 0; p--)
    {
        const sParagraphLayout* prevPara = GetParagraphLayout(p);
        if (prevPara && !prevPara->lines.empty())
        {
            int prevBoxIndex = prevPara->lines.back().boxIndex;
            const sBoxes& prevBox = mPageManager->GetGlobalBoxList()[prevBoxIndex];

            // Check if current margins match previous box
            COORD_T pageOffset = (prevBox.pageNumber % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();
            COORD_T expectedLeft = pageOffset + mLayoutState->GetLeftMargin();
            COORD_T expectedRight = pageOffset + mLayoutState->GetRightMargin();

            if (CoordsEqual(prevBox.left, expectedLeft) && CoordsEqual(prevBox.right, expectedRight))
            {
                // Margins match - use same box
                return prevBoxIndex;
            }

            // Margins changed - search forward for box with matching margins
            for (size_t b = prevBoxIndex + 1; b < mPageManager->GetGlobalBoxList().size(); b++)
            {
                const sBoxes& box = mPageManager->GetGlobalBoxList()[b];
                pageOffset = (box.pageNumber % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();
                expectedLeft = pageOffset + mLayoutState->GetLeftMargin();
                expectedRight = pageOffset + mLayoutState->GetRightMargin();

                if (CoordsEqual(box.left, expectedLeft) && CoordsEqual(box.right, expectedRight))
                {
                    // Found matching box
                    return b;
                }
            }

            // No matching box - use previous box as fallback
            // (Should only happen in edge cases)
            return prevBoxIndex;
        }
    }

    // No previous paragraph - use box 0 (first box)
    if (!mPageManager->GetGlobalBoxList().empty())
    {
        return 0;
    }

    // No boxes at all - return 0 (caller should handle this)
    return 0;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para [in] - Paragraph number to set up
///
/// @return sParagraphSetupState - Initial state for laying out this paragraph
///
/// @brief
/// Retrieves starting state from previous paragraph's stored end state.
/// Allows LayoutParagraph to be called on any paragraph without sequential
/// processing dependency.
///
/// For paragraph 0: Returns defaults (line 0, page 1, height 0)
/// For paragraph N: Reads directly from paragraph N-1's stored position
/// end state (endContentLineNum, endScreenY, etc.)
///
/// This approach ensures partial layout produces identical results to full
/// layout by reading only from immutable stored data, never from live
/// mutable state like box->currentY.
///
/// Works correctly even when paragraphs have no lines (e.g., dot commands
/// in SHOW_NONE mode) because the end state is always stored.
///
/////////////////////////////////////////////////////////////////////////////
sParagraphSetupState cLayoutBase::SetupParagraph(PARAGRAPH_T para)
{
    sParagraphSetupState state;

    // Paragraph 0: Use defaults from constructor, but if boxes exist (partial relayout),
    // use box 0 with initial fill position
    if (para == 0)
    {
        // For partial relayout (boxes already exist), use box 0
        if (!mPageManager->GetGlobalBoxList().empty())
        {
            state.startBox = 0;
            state.startBoxY = 0;  // Start of box
        }
        return state;
    }

    // Paragraph N > 0: Read from previous paragraph's stored end state
    const sParagraphLayout* prevPara = GetParagraphLayout(para - 1);
    if (!prevPara)
    {
        // Previous paragraph not yet formatted - use defaults as fallback
        return state;
    }

    // Read directly from stored end state
    // No dependency on lines existing (works for SHOW_NONE mode)
    // No reading from live box->currentY (prevents partial/full layout discrepancies)
    state.startContentLineNum = prevPara->endContentLineNum;
    state.startRawLineNum = prevPara->endRawLineNum;
    state.startPageLineNum = prevPara->endPageLineNum;
    state.startPage = prevPara->endPage;
    state.startBox = prevPara->endBox;
    state.startCumulativeHeight = prevPara->endCumulativeHeight;
    state.startScreenY = prevPara->endScreenY;
    state.startBoxY = prevPara->endBoxY;

    // New paragraph always starts with first line flag true
    // Each paragraph's first line gets paragraph margin treatment
    state.isFirstLineOfParagraph = true;

    return state;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] - Header command text (e.g., ".HE text" or ".HM 2")
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses header-related dot commands (.HE, .H1-.H5, .HM).
///
/// Commands:
///   .HE or .H1 - Header 1 text
///   .H2 - Header 2 text
///   .H3 - Header 3 text
///   .H4 - Header 4 text
///   .H5 - Header 5 text
///   .HM - Header margin setting
///
/// Headers can have even/odd specifiers:
///   .HEE - Even page header
///   .HEO - Odd page header
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cLayoutBase::ParseHeader(const std::string& command)
{
    // Extract command portion (first 2-3 characters after dot)
    if (command.length() < 3)
    {
        return DOT_ERROR;
    }

    // Get command (skip the dot) and uppercase it for comparison
    std::string cmd = command.substr(1, 2);  // Get 2 chars after dot
    for (size_t i = 0; i < cmd.length(); i++)
    {
        cmd[i] = toupper(cmd[i]);
    }

    // Check for H1-H5
    if (cmd == "HE" || cmd == "H1")
    {
        mHeaderFooterManager->SetHeaderValue(1);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "H2")
    {
        mHeaderFooterManager->SetHeaderValue(2);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "H3")
    {
        mHeaderFooterManager->SetHeaderValue(3);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "H4")
    {
        mHeaderFooterManager->SetHeaderValue(4);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "H5")
    {
        mHeaderFooterManager->SetHeaderValue(5);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }

    return DOT_ERROR;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  command [in] - Footer command text (e.g., ".FO text" or ".FM 2")
///
/// @return true if parsed successfully, false otherwise
///
/// @brief
/// Parses footer-related dot commands (.FO, .F1-.F5, .FM).
///
/// Commands:
///   .FO or .F1 - Footer 1 text
///   .F2 - Footer 2 text
///   .F3 - Footer 3 text
///   .F4 - Footer 4 text
///   .F5 - Footer 5 text
///   .FM - Footer margin setting
///
/// Footers can have even/odd specifiers:
///   .FOE - Even page footer
///   .FOO - Odd page footer
///
/////////////////////////////////////////////////////////////////////////////
eDotCommandStatus cLayoutBase::ParseFooter(const std::string& command)
{
    // Extract command portion (first 2-3 characters after dot)
    if (command.length() < 3)
    {
        return DOT_ERROR;
    }

    // Get command (skip the dot) and uppercase it for comparison
    std::string cmd = command.substr(1, 2);  // Get 2 chars after dot
    for (size_t i = 0; i < cmd.length(); i++)
    {
        cmd[i] = toupper(cmd[i]);
    }

    // Check for F1-F5
    if (cmd == "FO" || cmd == "F1")
    {
        mHeaderFooterManager->SetFooterValue(1);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "F2")
    {
        mHeaderFooterManager->SetFooterValue(2);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "F3")
    {
        mHeaderFooterManager->SetFooterValue(3);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "F4")
    {
        mHeaderFooterManager->SetFooterValue(4);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }
    else if (cmd == "F5")
    {
        mHeaderFooterManager->SetFooterValue(5);
        HandleHeaderFooterText(command, cmd);
        return DOT_GOOD;
    }

    return DOT_ERROR;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] - Full command text including header/footer content
/// @param  command [in] - Command identifier (HE, H1, FO, F1, etc.)
///
/// @return nothing
///
/// @brief
/// Extracts and stores header or footer text from command.
///
/// Format: .HE[E|O] text   or   .FO[E|O] text
///
/// The third character (after the command) determines page type:
///   E or e = Even pages only
///   O or o = Odd pages only
///   (space or other) = Both pages
///
/// Stores the text in the appropriate array (regular, even, or odd).
///
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] raw dot command text (e.g., ".HE Header Text")
/// @param  command [in] uppercase command (e.g., "HE", "H1", "F2")
///
/// @return nothing
///
/// @brief
/// Processes header/footer dot command text and stores it for later rendering.
/// Delegates to cHeaderFooterManager.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::HandleHeaderFooterText(const std::string& text, const std::string& command)
{
    mHeaderFooterManager->HandleHeaderFooterText(text, command);
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] - Dot command text to layout
/// @param  para [in] - Paragraph number
///
/// @return sLineLayout - Laid out line with dot command visible
///
/// @brief
/// Layouts dot command text for display when mLayoutState->GetShowControl() allows it.
/// Similar to LayoutHeaderFooterText() but for dot commands in the body.
///
/// Dot commands are ALWAYS positioned at page offset (not left margin),
/// so they appear in the left margin area.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<sLineLayout> cLayoutBase::LayoutDotCommandText(PARAGRAPH_T para, bool isComment)
{
    std::vector<sLineLayout> result;

    // Get line height using the default font (ensures consistent height across redraws)
    std::string mDefaultFont = mLayoutState->GetDefaultFont();
    COORD_T lineHeight = GetFontLineSpacing(mDefaultFont);

    // Calculate page offset for current page (odd/even)
    COORD_T pageOffset = (mCurrentPage % 2 == 0) ? mLayoutState->GetPageOffsetEven() : mLayoutState->GetPageOffsetOdd();

    // Active paragraph exception (SHOW_NONE): use page-relative Y for page mode rendering
    // SHOW_ALL/SHOW_DOT: use continuous Y (existing behavior, screeny used for rendering)
    bool isActiveParagraphException = (mLayoutState->GetShowControl() != SHOW_ALL &&
                                       mLayoutState->GetShowControl() != SHOW_DOT);

    // Calculate absolute document position for this paragraph
    POSITION_T paraStart = 0;
    POSITION_T paraEnd = 0;
    if (mDocument)
    {
        mDocument->GetParagraphStartandEnd(para, paraStart, paraEnd);
    }

    // Measure all glyphs into a single segment (full paragraph width)
    sSegmentLayout fullSegment;
    fullSegment.font = mDefaultFont;
    fullSegment.paragraph = para;
    fullSegment.segmentheight = lineHeight;
    fullSegment.startPosition = 0;
    fullSegment.length = 0;

    // Iterate by grapheme (NOT raw bytes) so multi-byte UTF-8 stays intact and
    // docPos stays aligned with cDocument grapheme positions (black-box rule).
    std::vector<std::string> graphemes;
    std::vector<POSITION_T> offsets;
    if (mDocument)
    {
        mDocument->GetParagraphGraphemes(para, graphemes, offsets);
    }
    COORD_T currentX = 0;
    for (size_t i = 0; i < graphemes.size(); i++)
    {
        const std::string &glyph = graphemes[i];
        fullSegment.length++;
        fullSegment.position.push_back(currentX);

        // Calculate width using DISPLAY character (not raw byte)
        POSITION_T docPos = paraStart + static_cast<POSITION_T>(i);
        std::string displayGlyph = GetDisplayCharacter(docPos, glyph);
        COORD_T glyphWidth = GetTextWidth(displayGlyph, fullSegment.font);

        currentX += glyphWidth;
    }

    // Mark control codes for highlighting
    MarkControlCodesInSegment(fullSegment, graphemes);

    COORD_T totalWidth = currentX;

    // Determine wrap width for comments
    COORD_T wrapWidth = 0;
    if (isComment)
    {
        if (mBoxRight > 0)
        {
            wrapWidth = mBoxRight - pageOffset;
        }
        else
        {
            wrapWidth = mLayoutState->GetRightMargin();
        }
    }

    // Decide whether to wrap: only comments that exceed wrap width
    bool needsWrap = isComment && wrapWidth > 0 && totalWidth > wrapWidth && fullSegment.length > 0;

    if (!needsWrap)
    {
        // Single line (existing behavior for dot commands and short comments)
        sLineLayout line;
        line.isPrintable = false;
        line.pagex = pageOffset;
        if (isActiveParagraphException)
        {
            line.pagey = mBoxTop;
        }
        else
        {
            line.pagey = mScreenY;
        }
        line.screeny = mScreenY;
        line.pagenumber = mCurrentPage;
        line.lineheight = lineHeight;
        line.left = true;
        line.contentLineNumber = mCurrentContentLineNumber;
        line.rawLineNumber = mCurrentRawLineNumber;
        line.pageLineNumber = mCurrentPageLineNumber;
        line.cumalativeheight = mCurrentCumulativeHeight;
        line.boxIndex = mCurrentBoxIndex;
        line.linestart = 0;
        line.documentPosition = paraStart;

        if (fullSegment.GetGraphemeCount() > 0)
        {
            line.segments.push_back(fullSegment);
        }
        result.push_back(line);

        // Advance counters (dot commands don't increment page line number -- they don't print)
        mCurrentRawLineNumber++;
        mScreenY += lineHeight;
        if (isActiveParagraphException)
        {
            mBoxTop += lineHeight;
        }
    }
    else
    {
        // Word-wrap the comment into multiple lines

        // Find word break positions (spaces) for wrapping
        std::vector<POSITION_T> spacePositions;
        for (POSITION_T i = 0; i < fullSegment.length; i++)
        {
            if (graphemes[i] == " ")
            {
                spacePositions.push_back(i);
            }
        }

        POSITION_T lineStart = 0;

        while (lineStart < fullSegment.length)
        {
            // Create a line
            sLineLayout line;
            line.isPrintable = false;
            line.pagex = pageOffset;
            if (isActiveParagraphException)
            {
                line.pagey = mBoxTop;
            }
            else
            {
                line.pagey = mScreenY;
            }
            line.screeny = mScreenY;
            line.pagenumber = mCurrentPage;
            line.lineheight = lineHeight;
            line.left = true;
            line.contentLineNumber = mCurrentContentLineNumber;
            line.rawLineNumber = mCurrentRawLineNumber;
            line.pageLineNumber = mCurrentPageLineNumber;
            line.cumalativeheight = mCurrentCumulativeHeight;
            line.boxIndex = mCurrentBoxIndex;
            line.linestart = lineStart;
            line.documentPosition = paraStart + lineStart;

            // Find how many characters fit on this line
            COORD_T startX = fullSegment.position[lineStart];
            POSITION_T lineEnd = fullSegment.length;  // Assume rest fits

            // Check if remaining text exceeds wrap width
            COORD_T remainingWidth = totalWidth - startX;
            if (remainingWidth > wrapWidth)
            {
                // Find last space that fits within wrap width
                POSITION_T lastFittingSpace = lineStart;
                bool foundSpace = false;
                for (POSITION_T sp : spacePositions)
                {
                    if (sp <= lineStart)
                    {
                        continue;
                    }
                    COORD_T widthToHere = fullSegment.position[sp] - startX;
                    if (widthToHere > wrapWidth)
                    {
                        break;
                    }
                    lastFittingSpace = sp;
                    foundSpace = true;
                }

                if (foundSpace && lastFittingSpace > lineStart)
                {
                    // Break after the space
                    lineEnd = lastFittingSpace + 1;
                }
                else
                {
                    // No space fits -- break at character boundary
                    for (POSITION_T c = lineStart + 1; c < fullSegment.length; c++)
                    {
                        if (fullSegment.position[c] - startX > wrapWidth)
                        {
                            lineEnd = c;
                            break;
                        }
                    }
                    // Ensure at least one character per line to avoid infinite loop
                    if (lineEnd == lineStart)
                    {
                        lineEnd = lineStart + 1;
                    }
                }
            }

            // Extract sub-segment for this line with rebased positions
            sSegmentLayout lineSegment;
            lineSegment.font = fullSegment.font;
            lineSegment.paragraph = fullSegment.paragraph;
            lineSegment.segmentheight = fullSegment.segmentheight;
            lineSegment.startPosition = lineStart;
            lineSegment.length = lineEnd - lineStart;

            // Copy positions rebased to start at 0
            COORD_T baseX = fullSegment.position[lineStart];
            for (POSITION_T i = lineStart; i < lineEnd; i++)
            {
                lineSegment.position.push_back(fullSegment.position[i] - baseX);
            }
            lineSegment.totalWidth = (lineEnd < fullSegment.length)
                ? fullSegment.position[lineEnd] - baseX
                : totalWidth - baseX;

            // Copy control code flags for this range
            if (!fullSegment.controlCodeIndices.empty())
            {
                for (POSITION_T i = lineStart; i < lineEnd; i++)
                {
                    if (i < static_cast<POSITION_T>(fullSegment.controlCodeIndices.size()))
                    {
                        lineSegment.controlCodeIndices.push_back(fullSegment.controlCodeIndices[i]);
                    }
                }
            }

            if (lineSegment.GetGraphemeCount() > 0)
            {
                line.segments.push_back(lineSegment);
            }
            result.push_back(line);

            // Advance counters for this line (comments don't increment page line number -- they don't print)
            mCurrentRawLineNumber++;
            mScreenY += lineHeight;
            if (isActiveParagraphException)
            {
                mBoxTop += lineHeight;
            }

            lineStart = lineEnd;
        }
    }

    return result;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  page [in] page number to insert headers/footers for
///
/// @return nothing
///
/// @brief
/// Inserts headers and footers for specified page.
/// Delegates to cHeaderFooterManager.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::InsertHeadersFooters(PAGE_T page)
{
    mHeaderFooterManager->InsertHeadersFooters(page);
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return reference to page headers map
///
/// @brief
/// Returns map of page numbers to their header lines.
/// Delegates to cHeaderFooterManager.
///
/////////////////////////////////////////////////////////////////////////////
const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& cLayoutBase::GetPageHeaders(void) const
{
    return mHeaderFooterManager->GetPageHeaders();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return reference to page footers map
///
/// @brief
/// Returns map of page numbers to their footer lines.
/// Delegates to cHeaderFooterManager.
///
/////////////////////////////////////////////////////////////////////////////
const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& cLayoutBase::GetPageFooters(void) const
{
    return mHeaderFooterManager->GetPageFooters();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  mode [in] - Control code visibility mode
///
/// @return nothing
///
/// @brief
/// Sets the control code visibility mode (SHOW_ALL, SHOW_DOT, SHOW_NONE).
/// This affects whether dot commands and control codes are displayed.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetShowControl(eShowControl mode)
{
    mLayoutState->SetShowControl(mode);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return Current control code visibility mode
///
/// @brief
/// Gets the current control code visibility mode.
///
/////////////////////////////////////////////////////////////////////////////
eShowControl cLayoutBase::GetShowControl(void) const
{
    return mLayoutState->GetShowControl();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  para [in] paragraph number of the caret (active editing position)
///
/// @return nothing
///
/// @brief
/// Set the active paragraph. Dot commands on this paragraph remain visible
/// even when SHOW_NONE is set, so the user can see what they are typing.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetActiveParagraph(PARAGRAPH_T para)
{
    mActiveParagraph = para;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the active paragraph number, or -1 if not set
///
/// @brief
/// Get the active paragraph (caret position).
///
/////////////////////////////////////////////////////////////////////////////
PARAGRAPH_T cLayoutBase::GetActiveParagraph(void) const
{
    return mActiveParagraph;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  font [in] font specification string
///
/// @return nothing
///
/// @brief
/// Set the default font for layout. This font is used for all segments
/// unless overridden by font commands in the document. Also used for
/// dot commands and comments which should always display in default font.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetDefaultFont(const std::string& font)
{
    mLayoutState->SetDefaultFont(font);
    mLayoutState->SetCurrentFont(font);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  color [in] default text color
///
/// @return nothing
///
/// @brief
/// Set the default text color for layout. This color is used for all segments
/// unless overridden by color commands in the document. Also used when
/// resetting formatting state.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetDefaultTextColor(const sSeqRGBColor& color)
{
    mLayoutState->SetDefaultTextColor(color);
    mLayoutState->SetCurrentColor(color);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  isHelp [in] true if this is a help display
///
/// @return nothing
///
/// @brief
/// Sets the help mode flag.
/// When true, disables wordwrap, margins, and font scaling.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetIsHelp(bool isHelp)
{
    mLayoutState->SetIsHelp(isHelp);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @return Current help mode flag
///
/// @brief
/// Gets the current help mode flag.
///
/////////////////////////////////////////////////////////////////////////////
bool cLayoutBase::GetIsHelp(void) const
{
    return mLayoutState->IsHelp();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] - filename (without path) for variable expansion
///
/// @return nothing
///
/// @brief
/// Sets the filename used by &*& variable expansion.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetFilename(const std::string& filename)
{
    mFilename = filename;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  dir [in] - directory path for variable expansion
///
/// @return nothing
///
/// @brief
/// Sets the file directory used by &.& and &\& variable expansion.
///
/////////////////////////////////////////////////////////////////////////////
void cLayoutBase::SetFileDir(const std::string& dir)
{
    mFileDir = dir;
}


/////////////////////////////////////////////////////////////////////////////
//
// sDisplayBox methods - Display list filtering
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  viewport [in] - Current viewport
/// @param  layout [in] - Layout engine
///
/// @return nothing
///
/// @brief
/// Calculates which lines in this box are visible in the viewport.
/// Uses Y-coordinate intersection to filter lines within the box.
///
/// Algorithm:
/// 1. Clear previous visible lines
/// 2. Guard: If no box pointer, nothing to calculate
/// 3. For each line number in box.containedLines:
///    a. Get line from layout
///    b. Calculate line Y range (screeny to screeny + lineheight)
///    c. Test if line Y range intersects viewport Y range
///    d. If intersects, add line pointer to visibleLines
///
/// This is critical for partially visible boxes. A box might intersect
/// the viewport, but only some of its lines are actually visible. This
/// method performs the second level of filtering (after box intersection)
/// to determine exactly which lines need to be rendered.
///
/// Intersection test: lineBottom >= viewport.topY && lineTop <= viewport.bottomY
/// This is the same algorithm used in sViewport::Intersects() for boxes.
///
/////////////////////////////////////////////////////////////////////////////
void sDisplayBox::CalculateVisibleLines(const sViewport& viewport, const cLayoutBase* layout)
{
    // Clear previous visible lines
    visibleLines.clear();

    // Guard: if no box pointer, nothing to calculate
    if (!box)
    {
        return;
    }

    // Iterate through all lines in this box
    for (LINE_T lineNum : box->containedLines)
    {
        // Get line data from layout
        const sLineLayout* line = layout->GetLineByRawLineNumber(lineNum);
        if (!line)
        {
            // Line lookup failed - skip this line
            continue;
        }

        // Calculate line Y range in screen coordinates
        // Line top is screeny, bottom is screeny + lineheight
        COORD_T lineTop = line->screeny;
        COORD_T lineBottom = line->screeny + line->lineheight;

        // Test if line intersects viewport Y range
        // Same algorithm as sViewport::Intersects() for boxes
        // Line must have bottom at or below viewport top
        // AND top at or above viewport bottom
        // In other words: Y ranges must overlap
        if (lineBottom >= viewport.topY && lineTop <= viewport.bottomY)
        {
            // Line is visible - add to list
            // const_cast is safe: we're not modifying the line,
            // just storing a pointer for rendering
            visibleLines.push_back(const_cast<sLineLayout*>(line));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// sDisplayList methods - Display list building
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @param  viewport [in] - Current viewport
/// @param  layout [in] - Layout engine
///
/// @return nothing
///
/// @brief
/// Builds display list from visible boxes in viewport.
///
/// Algorithm:
/// 1. Clear previous display list
/// 2. Cache scroll offset from viewport for rendering
/// 3. For each visible box in viewport:
///    a. Create new sDisplayBox entry
///    b. Set box pointer
///    c. Cache scroll offset for this box
///    d. Cache needsRedraw flag from box
///    e. Calculate visible lines for this box
///    f. Add to display list
///
/// After Build() completes, the display list contains only the boxes
/// and lines that need to be rendered for the current frame. This is
/// the final stage of the rendering pipeline before actual drawing.
///
/// The display list is transient and rebuilt every frame. This keeps
/// the design simple and avoids complex dirty tracking and invalidation.
///
/// Two-level filtering ensures efficiency:
/// - Level 1 (viewport): Which boxes intersect viewport? (already done)
/// - Level 2 (display list): Which lines within those boxes are visible?
///
/////////////////////////////////////////////////////////////////////////////
void sDisplayList::Build(const sViewport& viewport, const cLayoutBase* layout)
{
    // Clear previous display list
    boxes.clear();

    // Cache scroll offset from viewport
    scrollOffset = viewport.scrollOffset;

    // Build display box entry for each visible box
    for (sBoxes* box : viewport.visibleBoxes)
    {
        // Create new display box entry
        sDisplayBox displayBox;

        // Set box pointer (points to layout data, not owned by display list)
        displayBox.box = box;

        // Cache scroll offset for rendering
        displayBox.screenYOffset = viewport.scrollOffset;

        // Cache needsRedraw flag from box
        displayBox.needsRedraw = box->needsRedraw;

        // Calculate which lines in this box are visible
        // This performs line-level intersection within the box
        displayBox.CalculateVisibleLines(viewport, layout);

        // Add to display list
        // Note: We add even if visibleLines is empty, in case we want to
        // render box borders or backgrounds. Renderer can check visibleLines.size()
        boxes.push_back(displayBox);
    }
}
