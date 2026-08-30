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

#ifndef LAYOUTBASE_H
#define LAYOUTBASE_H

#include <map>
#include <functional>
#include <utility>

#include "layoutstructs.h"
#include "layoutstate.h"
#include "headerfootermanager.h"
#include "src/core/include/timer.h"

// Forward declarations
class cDocument;
class cDotCommandParser;
class cPageManager;
class cTextMeasurement;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sParagraphSetupState
///
/// @brief
/// State retrieved from previous paragraph for stateless paragraph layout.
/// Allows LayoutParagraph to be called on any paragraph without sequential
/// processing dependency.
///
/////////////////////////////////////////////////////////////////////////////
struct sParagraphSetupState
{
    LINE_T startContentLineNum;       // Starting content line number (excludes dot commands)
    LINE_T startRawLineNum;           // Starting raw line number (every laid-out row, includes dot commands)
    LINE_T startPageLineNum;          // Starting page line number (excludes dot commands)
    PAGE_T startPage;                 // Starting page number
    int startBox;                     // Starting box index
    COORD_T startCumulativeHeight;    // Starting cumulative height from document start
    COORD_T startScreenY;             // Starting continuous screen Y coordinate (never resets at page breaks)
    COORD_T startBoxY;                // Starting box fill position (box.currentY when paragraph starts)
    bool isFirstLineOfParagraph;      // Paragraph margin flag state

    // Constructor with defaults for paragraph 0
    // NOTE: startBox uses NOT_SET to match cLayoutBase constructor's initial mCurrentBoxIndex value
    sParagraphSetupState() : startContentLineNum(0), startRawLineNum(0), startPageLineNum(0),
                             startPage(1), startBox(NOT_SET), startCumulativeHeight(0),
                             startScreenY(0), startBoxY(0), isFirstLineOfParagraph(true) {}
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sFormattingCheckpoint
///
/// @brief
/// Snapshot of formatting state at a given paragraph boundary.
/// Used to avoid O(N) rescan of dot commands during random-access layout.
/// Built during LayoutDocument() at regular intervals (every CHECKPOINT_INTERVAL
/// paragraphs), then used by ApplyPreviousDotCommands() to start replay
/// from the nearest checkpoint instead of paragraph 0.
///
/////////////////////////////////////////////////////////////////////////////
struct sFormattingCheckpoint
{
    PARAGRAPH_T paragraph;            // Paragraph number this checkpoint was taken after

    // Margins
    COORD_T leftMargin;
    COORD_T rightMargin;
    COORD_T topMargin;
    COORD_T bottomMargin;
    COORD_T pageOffsetOdd;
    COORD_T pageOffsetEven;
    COORD_T headerMargin;
    COORD_T footerMargin;
    COORD_T paragraphMargin;
    bool validParagraphMargin;

    // Tab stops
    std::vector<sTabStop> tabStops;

    // Text modifiers
    sModifiers modifiers;

    // Line settings
    COORD_T lineHeight;
    bool autoLeading;

    // Other formatting state
    bool wordWrapEnabled;
    PAGE_T pageNumberOffset;
    ePageNumberFormat pageNumFormat;
    bool printPageNumbers;
    bool landscapeMode;
    COORD_T pageLength;
    COORD_T paragraphSpacingBefore;
    COORD_T paragraphSpacingAfter;
    bool doNewPage;

    // Subscript/superscript roll distance
    COORD_T subSuperRoll;

    // Paper dimensions (can be swapped by .PR mLandscape/portrait)
    COORD_T paperWidth;
    COORD_T paperHeight;

    // Header/footer tracking values
    int headerValue;
    int footerValue;

    // Equality comparison (ignores paragraph field -- compares formatting state only)
    bool operator==(const sFormattingCheckpoint& other) const
    {
        return leftMargin == other.leftMargin && rightMargin == other.rightMargin &&
               topMargin == other.topMargin && bottomMargin == other.bottomMargin &&
               pageOffsetOdd == other.pageOffsetOdd && pageOffsetEven == other.pageOffsetEven &&
               headerMargin == other.headerMargin && footerMargin == other.footerMargin &&
               paragraphMargin == other.paragraphMargin &&
               validParagraphMargin == other.validParagraphMargin &&
               tabStops == other.tabStops &&
               modifiers == other.modifiers &&
               lineHeight == other.lineHeight && autoLeading == other.autoLeading &&
               wordWrapEnabled == other.wordWrapEnabled &&
               pageNumberOffset == other.pageNumberOffset &&
               pageNumFormat == other.pageNumFormat &&
               printPageNumbers == other.printPageNumbers &&
               landscapeMode == other.landscapeMode && pageLength == other.pageLength &&
               paragraphSpacingBefore == other.paragraphSpacingBefore &&
               paragraphSpacingAfter == other.paragraphSpacingAfter &&
               doNewPage == other.doNewPage &&
               subSuperRoll == other.subSuperRoll &&
               paperWidth == other.paperWidth && paperHeight == other.paperHeight &&
               headerValue == other.headerValue && footerValue == other.footerValue;
    }
};

// Checkpoint interval: save formatting state every N paragraphs
static constexpr PARAGRAPH_T CHECKPOINT_INTERVAL = 100;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sLayoutMemoryUsage
///
/// @brief
/// Memory usage breakdown for the layout subsystem.
/// Tracks allocated vs in-use bytes for paragraphs, lines, segments,
/// and segment sub-breakdowns. Used by ^O? memory dialog.
///
/////////////////////////////////////////////////////////////////////////////
struct sLayoutMemoryUsage
{
    size_t paragraphBytes ;         ///< allocated (capacity)
    size_t paragraphUsedBytes ;     ///< in use (size)
    size_t lineBytes ;              ///< allocated (capacity)
    size_t lineUsedBytes ;          ///< in use (size)
    size_t segmentBytes ;           ///< allocated (capacity)
    size_t segmentUsedBytes ;       ///< in use (size)
    size_t segStructBytes ;         ///< sizeof(sSegmentLayout) * count
    size_t segPositionBytes ;       ///< position deque contents
    size_t segFontBytes ;           ///< font string contents
    size_t segControlBytes ;        ///< controlCodeIndices contents
    size_t checkpointBytes ;
    size_t boxBytes ;
    size_t paragraphCount ;
    size_t lineCount ;
    size_t segmentCount ;
    size_t checkpointCount ;
} ;


class cLayoutBase
{
    // Friend classes that need access to protected methods
    friend class cHeaderFooterManager;

    // =================================================================
    // METHODS
    // =================================================================

public:
    // ----- Construction/Destruction -----
    cLayoutBase(void);
    virtual ~cLayoutBase(void);

    // ----- Main Layout Operations -----
    void LayoutDocument(cDocument* doc, std::function<void(int)> progressCallback = nullptr);
    bool LayoutParagraph(PARAGRAPH_T para);
    bool IsParagraphLaidOut(PARAGRAPH_T para) const;
    bool InFullLayout() const { return mFullLayout; }
    bool WordWrapParagraph(PARAGRAPH_T paragraphNum);
    virtual std::vector<sSegmentLayout> BuildParagraphSegments(PARAGRAPH_T paragraphNum) = 0;
    virtual std::vector<sSegmentLayout> BuildHeaderFooterSegments(
        PARAGRAPH_T sourceParagraph,
        POSITION_T sourceStartPos,
        PAGE_T page,
        std::vector<std::string>& outGraphemes) = 0;
    void WordWrapSegmentsIntoLines(const std::vector<sSegmentLayout>& segments, PARAGRAPH_T paragraphNum);
    std::pair<sSegmentLayout, sSegmentLayout> SplitSegmentAtPosition(const sSegmentLayout& segment, POSITION_T splitPosition);
    void FinalizeLine(sLineLayout& line, COORD_T maxLineWidth, bool isFinalLine = false);
    bool SaveLine(PARAGRAPH_T paragraphNum, sLineLayout& line);
    sLineLayout CreateLine(PARAGRAPH_T paragraphNum);

    // ----- Formatting Checkpoint Management -----
    bool UpdateCheckpointIfNeeded(PARAGRAPH_T para);
    void InvalidateCheckpointsFrom(PARAGRAPH_T para);
    bool LastCheckpointMatched() const { return mLastCheckpointMatched; }

    // ----- Box Management -----
    bool CreatePageBox(PAGE_T page);
    bool CreateMarginBox(PAGE_T page);
    bool CheckMarginChange(void);
    bool CheckPageChange(void);
    bool UpdatePageBox(void);
    void IncrementPageAndCreateBox(void);

    // ----- Page Break Detection -----
    bool CheckPageBreak(PARAGRAPH_T paragraphNum);
    bool NeedNewPage(COORD_T lineHeight);

    // ----- Dot Command Parsing -----
    eDotCommandStatus ParseDotCommand(const std::string& command);
    eDotCommandStatus ParsePageBreak(const std::string& command);
    eDotCommandStatus ParseConditionalPageBreak(const std::string& command);
    eDotCommandStatus ParseHeader(const std::string& command);
    eDotCommandStatus ParseFooter(const std::string& command);
    std::string FormatPageNumber(PAGE_T page, ePageNumberFormat format) const;

    // ----- Setters -----
    void SetLeftMargin(COORD_T margin);
    void SetRightMargin(COORD_T margin);
    void SetParagraphMargin(COORD_T margin);
    void SetPageOffsetOdd(COORD_T offset);
    void SetPageOffsetEven(COORD_T offset);
    void SetShowControl(eShowControl mode);
    void SetActiveParagraph(PARAGRAPH_T para);
    virtual void SetDefaultFont(const std::string& font);
    virtual void SetDefaultTextColor(const sSeqRGBColor& color);
    void SetIsHelp(bool isHelp);
    void SetFilename(const std::string& filename);
    void SetFileDir(const std::string& dir);
    void SetDocument(cDocument* doc);

    // ----- Query Methods - Margin Getters -----
    COORD_T GetPageOffsetOdd(void) const { return mLayoutState->GetPageOffsetOdd(); }
    COORD_T GetPageOffsetEven(void) const { return mLayoutState->GetPageOffsetEven(); }
    COORD_T GetTopMargin(void) const { return mLayoutState->GetTopMargin(); }
    COORD_T GetBottomMargin(void) const { return mLayoutState->GetBottomMargin(); }
    COORD_T GetRightMargin(void) const { return mLayoutState->GetRightMargin(); }
    COORD_T GetLeftMargin(void) const { return mLayoutState->GetLeftMargin(); }
    COORD_T GetParagraphMargin(void) const { return mLayoutState->GetParagraphMargin(); }
    COORD_T GetHeaderMargin(void) const { return mLayoutState->GetHeaderMargin(); }
    COORD_T GetFooterMargin(void) const { return mLayoutState->GetFooterMargin(); }

    // ----- Query Methods - Box Queries -----
    const std::vector<sBoxes>& GetGlobalBoxList(void) const;
    int GetCurrentBoxIndex(void) const;
    const sBoxes* GetCurrentBox(void) const;
    const sBoxes* GetBoxByIndex(int boxIndex) const;
    const sBoxes* GetBoxForLine(LINE_T contentLineNumber) const;
    std::vector<int> GetBoxesOnPage(PAGE_T page) const;
    int GetBoxCount(void) const;
    COORD_T GetBoxLeft(void) const;
    COORD_T GetBoxRight(void) const;
    COORD_T GetBoxTop(void) const;
    COORD_T GetBoxBottom(void) const;

    // ----- Query Methods - Line Queries -----
    const sLineLayout* GetLineByRawLineNumber(LINE_T rawLineNumber) const;
    PARAGRAPH_T GetParagraphFromLine(LINE_T rawLineNumber) const;
    LINE_T GetNumberOfLines(void) const;
    LINE_T GetLineFromPosition(POSITION_T pos) const;
    COORD_T FindCoordInLine(POSITION_T pos, LINE_T lineNumber);
    POSITION_T FindPositionInLine(COORD_T targetX, LINE_T lineNumber);
    POSITION_T GetLineStartPosition(LINE_T line);
    POSITION_T GetLineStartDocumentPosition(LINE_T line);
    POSITION_T GetLineEndPosition(LINE_T line);
    LINE_T GetNumberofLinesinParagraph(PARAGRAPH_T para);
    PARAGRAPH_T GetFirstParagraphOnPage(PAGE_T page);
    LINE_T GetParagraphLineFromPosition(POSITION_T pos, PARAGRAPH_T para, bool up);
    sPageInfo GetPageInfo(PAGE_T page);
    sPageInfo GetPageInfoFromLine(LINE_T line);
    COORD_T GetLineBaseX(LINE_T line);
    COORD_T GetLineScreenY(LINE_T line);
    COORD_T GetLineHeight(LINE_T line);
    PAGE_T GetLinePageNumber(LINE_T line);
    LINE_T GetFirstLineOfParagraph(PARAGRAPH_T para) const;
    LINE_T GetLastLineOfParagraph(PARAGRAPH_T para) const;
    LINE_T GetLineContainingPosition(POSITION_T pos, PARAGRAPH_T para) const;

    // ----- Query Methods - Paragraph Queries -----
    const sParagraphLayout* GetParagraphLayout(PARAGRAPH_T number) const;
    PARAGRAPH_T GetNumberOfParagraphs(void) const;
    bool ParagraphIsCommand(PARAGRAPH_T para) const;
    bool ParagraphIsComment(PARAGRAPH_T para) const;
    eDotCommandStatus GetParagraphDotStatus(PARAGRAPH_T para) const;

    // ----- Query Methods - Page Queries -----
    PAGE_T GetPageFromLine(LINE_T rawLineNumber) const;
    PAGE_T GetNumberOfPages(void) const;

    // ----- Query Methods - Header/Footer Access -----
    const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& GetPageHeaders(void) const;
    const std::map<PAGE_T, std::vector<sHeaderFooterLine>>& GetPageFooters(void) const;

    // ----- Query Methods - Control Code Visibility -----
    eShowControl GetShowControl(void) const;
    PARAGRAPH_T GetActiveParagraph(void) const;
    bool GetIsHelp(void) const;

    // ----- Query Methods - Line Height -----
    COORD_T GetLineHeight(void) const;
    bool GetAutoLeading(void) const;

    // ----- Query Methods - Subscript/Superscript -----
    COORD_T GetSubSuperRoll(void) const;

    // ----- Query Methods - Paper Size -----
    COORD_T GetPaperWidth(void) const;
    COORD_T GetPaperHeight(void) const;

    // ----- Query Methods - Printer Orientation -----
    bool GetLandscapeMode(void) const;

    // ----- Query Methods - Page Numbering -----
    bool GetPrintPageNumbers(void) const;
    PAGE_T GetPageNumberOffset(void) const;
    ePageNumberFormat GetPageNumFormat(void) const;
    PAGE_T GetLogicalPageNumber(void) const;

    // ----- Query Methods - Text Modifiers -----
    const sModifiers& GetModifiers(void) const;

    // ----- Query Methods - Tab Stops -----
    const std::vector<sTabStop>& GetTabs(void) const;
    sTabStop GetNextTabStop(COORD_T currentPosition) const;

    // ----- Query Methods - Document Metrics -----
    COORD_T GetTotalDocumentHeight(void) const;
    COORD_T GetAverageLineHeight(void) const;
    cDocument* GetDocument(void) const;
    PAGE_T GetCurrentPage(void) const;
    PARAGRAPH_T GetCurrentParagraph(void) const;

    // ----- Character/Variable Display Helpers -----
    std::string GetDisplayCharacter(POSITION_T documentPos, const std::string& grapheme,
                                    PAGE_T pageNumber = 0) const;
    std::string GetVariableExpansion(eVariableType type) const;

    // ----- Font Formatting -----
    virtual std::string GetFontWithFormatting(bool bold, bool italic, bool underline) = 0;

    // ----- Text Measurement -----
    COORD_T GetTextWidth(const std::string& text);
    COORD_T GetTextWidth(const std::string& text, const std::string& font);
    COORD_T GetFontHeight(void);
    COORD_T GetFontLineSpacing(void) const;
    COORD_T GetFontLineSpacing(const std::string& font) const;

    // ----- Memory Management -----
    sLayoutMemoryUsage GetMemoryUsage(void) const;
    void ShrinkToFit(void);

protected:
    // ----- Header/Footer Layout Helpers -----
    void HandleHeaderFooterText(const std::string& text, const std::string& command);
    void InsertHeadersFooters(PAGE_T page);

    // ----- Dot Command Display -----
    std::vector<sLineLayout> LayoutDotCommandText(PARAGRAPH_T para, bool isComment);

    // ----- Text Measurement Setup -----
    void SetTextMeasurement(cTextMeasurement* textMeasurement);
    COORD_T CalculateFontBasedLineHeight(void) const;

    // ----- Segment Helpers -----
    void MarkControlCodesInSegment(sSegmentLayout& segment, const std::vector<std::string>& graphemes);
    void MarkSegmentIfInRange(sSegmentLayout& segment, POSITION_T paragraphStart);

    // ----- Justification/Tab Helpers -----
    void JustifyLine(sLineLayout& line, bool isFinalLineOfParagraph, COORD_T maxLineWidth);
    sTabStop FindNextTabStop(COORD_T currentPosition) const;

    // ----- Partial Layout Helpers -----
    void ResetFormattingState(void);
    void ApplyPreviousDotCommands(PARAGRAPH_T para);
    int FindBoxForParagraph(PARAGRAPH_T para);
    sParagraphSetupState SetupParagraph(PARAGRAPH_T para);

private:
    // ----- Formatting Checkpoint Helpers -----
    void SaveFormattingCheckpoint(PARAGRAPH_T para);
    void RestoreFormattingCheckpoint(const sFormattingCheckpoint& checkpoint);

    // ----- LayoutParagraph Helpers -----
    void RestoreFontStateFromPreviousParagraph(PARAGRAPH_T para);
    void SaveParagraphEndState(PARAGRAPH_T para);
    bool CompareWithOldLayout(PARAGRAPH_T para, const sParagraphLayout& oldLayout, bool hadPreviousLayout, bool wasDirty);
    bool LayoutDotCommandParagraph(PARAGRAPH_T para, const std::string& text, const sParagraphLayout& oldLayout, bool hadPreviousLayout, bool wasDirty);
    bool LayoutTextParagraph(PARAGRAPH_T para, const sParagraphLayout& oldLayout, bool hadPreviousLayout, bool wasDirty);
    void RegisterDotLineInBox(const sLineLayout& line);

    // ----- Line Lookup Helpers -----
    std::pair<PARAGRAPH_T, const sLineLayout*> FindLineByRawLineNumber(LINE_T rawLineNumber) const;

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

protected:
    // ----- Text Measurement -----
    cTextMeasurement* mTextMeasurement;  // NOT owned (created by derived class)

    // ----- Paragraph Layout Results -----

    // Paragraph layout results (protected for test subclass access)
    std::vector<sParagraphLayout> mParagraphLayout;

    // ----- Layout State Management -----

    // Layout state management
    cLayoutState* mLayoutState;    // OWNED by this class - encapsulates all settings/state

private:

    // ----- Layout Runtime State -----

    // Current layout state (NOT in mLayoutState - these are layout runtime variables)
    // NOTE: mCurrentBoxIndex is actively used and cached from cPageManager for performance
    // Box management is coordinated with mPageManager (access via mPageManager->GetGlobalBoxList())
    int mCurrentBoxIndex;              // Index into page manager's box list (cached from mPageManager)

    bool mFullLayout;                    // Are we doing a full layout?

    // ----- File Info -----

    // File info for variable expansion (&*&, &.&, &\&)
    std::string mFilename;               // Current filename (e.g., "test.rtf")
    std::string mFileDir;                // Current file directory (e.g., "/home/user")

    // ----- Active Paragraph -----

    // Active paragraph (caret paragraph) -- exempt from dot command hiding in SHOW_NONE
    PARAGRAPH_T mActiveParagraph;

    // Margin change tracking handled by cPageManager

    // ----- Current Box Coordinates -----

    // Current box coordinates (for wrapping)
    COORD_T mBoxLeft;
    COORD_T mBoxRight;
    COORD_T mBoxTop;
    COORD_T mBoxBottom;

    // ----- Document Access -----
    cDocument* mDocument;

    // ----- Owned Sub-Components -----

    // Dot command parser
    cDotCommandParser* mDotCommandParser;  // OWNED by this class

    // Page manager
    cPageManager* mPageManager;  // OWNED by this class

    // Header/Footer manager
    cHeaderFooterManager* mHeaderFooterManager;  // OWNED by this class

    // ----- Line Tracking -----
    LINE_T mCurrentContentLineNumber;  // Content line counter (excludes dot commands)
    LINE_T mCurrentRawLineNumber; // Raw line counter (every laid-out row, includes dot commands)
    LINE_T mCurrentPageLineNumber; // Page line counter (excludes dot commands; resets per page)

    // ----- Page Tracking -----
    PAGE_T mCurrentPage;           // Current physical page number during layout
    PAGE_T mLogicalPageNumber;     // Logical page number (for display, includes .PN offset)

    // ----- Height Tracking -----
    COORD_T mCurrentCumulativeHeight; // Cumulative height from document start (excludes dot commands)

    // ----- Paragraph Tracking -----
    PARAGRAPH_T mCurrentParagraph; // Current paragraph being processed

    // ----- Continuous Y Coordinate Tracking -----

    // Continuous Y coordinate tracking (for scrolling)
    COORD_T mScreenY;              // Cumulative Y across all pages (never resets)

    // ----- Formatting Checkpoints -----

    // Formatting state checkpoints for fast dot command replay (Tier 2 optimization)
    // Built during LayoutDocument() and rebuilt during idle layout
    std::vector<sFormattingCheckpoint> mFormattingCheckpoints;
    bool mLastCheckpointMatched;  // Set by UpdateCheckpointIfNeeded() for idle early stopping

    // ----- Initial Paper Dimensions -----

    // Initial paper dimensions (saved at start of LayoutDocument for ResetFormattingState)
    COORD_T mInitialPaperWidth;
    COORD_T mInitialPaperHeight;

    // ----- Performance Timing -----

    // Performance timing (enabled with LAYOUT_TIMER and DETAIL_LAYOUT_TIMER macros)
#ifdef LAYOUT_TIMER
    long long mPreliminaryTimeNs;           // Time spent in preliminary setup (nanoseconds)
    long long mDotCommandTimeNs;            // Time spent parsing dot commands (nanoseconds)
    long long mTextLayoutTimeNs;            // Time spent laying out text paragraphs (nanoseconds)
    long long mMeasureTextTimeNs;           // Time spent measuring text (nanoseconds)
    long long mPostLayoutTimeNs;            // Time spent in post-layout work (nanoseconds)

    // Comprehensive per-section timers (cover ALL work in LayoutParagraph + loop)
    long long mGetTextTimeNs;               // GetParagraphText() string copy
    long long mOldLayoutCopyTimeNs;         // sParagraphLayout copy for comparison
    long long mSetupParagraphTimeNs;        // SetupParagraph() + state restore
    long long mFontScanTimeNs;             // Backward scan for previous text paragraph font state
    long long mDotCommandLineTimeNs;        // LayoutDotCommandText() visual line creation
    long long mPageBoxTimeNs;               // CreatePageBox/CheckMarginChange/CheckPageChange
    long long mEndStateSaveTimeNs;          // Saving end state for next paragraph
    long long mLayoutCompareTimeNs;         // IsEqualTo() layout comparison
    long long mCheckpointTimeNs;            // SaveFormattingCheckpoint()
    long long mProgressCallbackTimeNs;      // Progress callback (includes processEvents)
    long long mResizeTimeNs;                // mParagraphLayout.resize()
    long long mLayoutParagraphTotalTimeNs;  // Total time in LayoutParagraph()

    // Counters
    long long mProgressCallbackCount;       // Number of processEvents calls
    long long mDotCommandParaCount;         // Number of dot command paragraphs
    long long mTextParaCount;               // Number of text paragraphs
    long long mPageBoxCreateCount;          // Number of page box creations
#endif

#ifdef DETAIL_LAYOUT_TIMER
    long long mApplyPreviousDotCommandsTimeNs; // Time spent replaying dot commands for partial layout (nanoseconds)
    long long mLayoutLineTimeNs;            // Time spent creating individual lines (nanoseconds)
    long long mMeasureTextCallCount;        // Number of font mMeasurement calls
    long long mMeasureTextAccumulatedTimeNs; // Accumulated time in font measurements (nanoseconds)
#endif
};

#endif // LAYOUTBASE_H
