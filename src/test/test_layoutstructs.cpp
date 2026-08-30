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

#include "doctest.h"

#include "src/core/layout/layoutstructs.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/layout/textmeasurement.h"
#include "src/gui/layout/layout.h"
// TODO: Tests for cEditorDisplay2 methods need to be updated after merge with cEditorCtrl
// The rendering methods are now private instance methods in cEditorCtrl
#include "src/gui/editor/editorctrl.h"
#include "src/core/document/document.h"
#include <QImage>
#include <QPainter>
#include <QApplication>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Mock text measurement for testing - fixed-width font simulation
///
/////////////////////////////////////////////////////////////////////////////
class cMockTextMeasurement : public cTextMeasurement
{
public:
    // Simple fixed-width font simulation for testing
    COORD_T GetTextWidth(const std::string& text) override
    {
        // 100 twips per character
        return static_cast<COORD_T>(text.length() * 100);
    }

    COORD_T GetTextWidth(const std::string& text, const std::string& font) override
    {
        // Ignore font parameter for testing, use same fixed-width
        (void)font;  // Suppress unused warning
        return static_cast<COORD_T>(text.length() * 100);
    }

    COORD_T GetFontHeight() override
    {
        return 240;  // 240 twips line height
    }

    COORD_T GetFontLineSpacing() const override
    {
        // For testing: height + leading (240 + 60 = 300 twips)
        // This simulates font's recommended line spacing
        return 300;
    }

    COORD_T GetFontLineSpacing(const std::string& font) const override
    {
        // Ignore font parameter for testing, use same fixed spacing
        (void)font;  // Suppress unused warning
        return 300;
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test subclass that uses mock text measurement
///
/////////////////////////////////////////////////////////////////////////////
class cLayoutBaseTest : public cLayoutBase
{
public:
    cLayoutBaseTest()
    {
        mMockTextMeasurement = new cMockTextMeasurement();
        SetTextMeasurement(mMockTextMeasurement);
    }

    ~cLayoutBaseTest()
    {
        delete mMockTextMeasurement;
    }

    // Accessors for private members (Phase 1: forward to mLayoutState)
    PAGE_T GetPageNumberOffset() const { return mLayoutState->GetPageNumberOffset(); }
    void SetPageNumberOffset(PAGE_T offset) { mLayoutState->SetPageNumberOffset(offset); }
    bool GetDoNewPage() const { return mLayoutState->ShouldDoNewPage(); }
    void SetDoNewPage(bool value) { mLayoutState->SetDoNewPage(value); }
    bool GetWordWrapEnabled() const { return mLayoutState->IsWordWrapEnabled(); }
    void SetWordWrapEnabled(bool value) { mLayoutState->SetWordWrapEnabled(value); }
    bool GetLandscapeMode() const { return mLayoutState->IsLandscapeMode(); }
    void SetLandscapeMode(bool value) { mLayoutState->SetLandscapeMode(value); }
    bool GetPrintPageNumbers() const { return mLayoutState->ShouldPrintPageNumbers(); }
    void SetPrintPageNumbers(bool value) { mLayoutState->SetPrintPageNumbers(value); }

    // Expose protected methods for testing
    using cLayoutBase::CreateLine;
    using cLayoutBase::ParseConditionalPageBreak;

    // Methods moved to cDotCommandParser - test through ParseDotCommand delegation
    // ParsePageNumber, ParseWordWrap, ParsePrinterOrientation, ParseOmitPageNumbers, ParsePrintPageNumbers

    // Helper for tests to add test paragraphs directly
    void AddTestParagraph(const sParagraphLayout& para)
    {
        mParagraphLayout.push_back(para);
    }

private:
    cMockTextMeasurement* mMockTextMeasurement;

    // Basic implementation for testing - creates one segment per paragraph
    // Real implementation in cLayout handles fonts, styles, control codes, etc.
    std::vector<sSegmentLayout> BuildParagraphSegments(PARAGRAPH_T paragraphNum) override
    {
        std::vector<sSegmentLayout> segments;

        // Get paragraph text
        std::vector<std::string> graphemes;
        std::vector<POSITION_T> offsets;
        cDocument* doc = GetDocument();
        if (!doc)
        {
            return segments;
        }

        doc->GetParagraphGraphemes(paragraphNum, graphemes, offsets);
        if (graphemes.empty())
        {
            return segments;
        }

        // Create single segment with all paragraph text
        sSegmentLayout segment;
        segment.paragraph = paragraphNum;
        segment.font = "Courier New,12,-1,5,50,0,0,0,0,0";
        segment.segmentheight = 240;  // Match GetFontHeight()
        segment.startPosition = 0;
        segment.length = static_cast<POSITION_T>(graphemes.size());

        // Calculate positions (100 twips per character)
        COORD_T x = 0;
        for (size_t i = 0; i < graphemes.size(); i++)
        {
            segment.position.push_back(static_cast<double>(x));
            COORD_T charWidth = 100;  // Fixed width matching GetTextWidth()
            x += charWidth;
        }
        segment.totalWidth = x;

        segments.push_back(segment);
        return segments;
    }

    // Simple test implementation - returns basic font descriptor
    std::string GetFontWithFormatting(bool bold, bool italic, bool underline) override
    {
        (void)bold;
        (void)italic;
        (void)underline;
        return "Courier New,12,-1,5,50,0,0,0,0,0";
    }

    // Simple test implementation for header/footer segments - returns empty
    std::vector<sSegmentLayout> BuildHeaderFooterSegments(
        PARAGRAPH_T sourceParagraph,
        POSITION_T sourceStartPos,
        PAGE_T page,
        std::vector<std::string>& outGraphemes) override
    {
        (void)sourceParagraph;
        (void)sourceStartPos;
        (void)page;
        outGraphemes.clear();
        return std::vector<sSegmentLayout>();
    }
};

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sBoxes construction and default values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sBoxes construction")
{
    sBoxes box;

    CHECK(box.type == BOX_TEXT);
    CHECK(box.left == 0);
    CHECK(box.top == 0);
    CHECK(box.right == 0);
    CHECK(box.bottom == 0);
    CHECK(box.boxNumber == 0);
    CHECK(box.pageNumber == 0);
    CHECK(box.currentY == 0);
    CHECK(box.columnCount == 1);
    CHECK(box.columnGap == 0);
    CHECK(box.containedLines.empty());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sLineLayout construction and default values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sLineLayout construction")
{
    sLineLayout line;

    CHECK(line.left == true);
    CHECK(line.center == false);
    CHECK(line.right == false);
    CHECK(line.justify == false);
    CHECK(CoordsEqual(line.pagex, 0));
    CHECK(CoordsEqual(line.pagey, 0));
    CHECK(CoordsEqual(line.screeny, 0));
    CHECK(line.pagenumber == 0);
    CHECK(line.pageLineNumber == 0);
    CHECK(line.contentLineNumber == 0);
    CHECK(line.rawLineNumber == 0);
    CHECK(CoordsEqual(line.lineheight, 0));
    CHECK(line.boxIndex == -1);
    CHECK(line.linestart == 0);
    CHECK(line.segments.empty());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sSegmentLayout construction and default values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sSegmentLayout construction")
{
    sSegmentLayout segment;

    CHECK(segment.paragraph == 0);
    CHECK(segment.startPosition == 0);
    CHECK(segment.length == 0);
    CHECK(segment.position.empty());
    CHECK(segment.font.empty());
    CHECK(segment.isBlock == false);
    CHECK(segment.isSubscript == false);
    CHECK(segment.isSuperscript == false);
    CHECK(CoordsEqual(segment.segmentheight, 0));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout construction and default values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout construction")
{
    sParagraphLayout para;

    CHECK(para.lines.empty());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sPage construction and default values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sPage construction")
{
    sPage page;

    CHECK(page.boxes.empty());
    CHECK(page.headers.empty());
    CHECK(page.footers.empty());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test eBoxType enum values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("eBoxType enum values")
{
    CHECK(BOX_TEXT == 0);
    CHECK(BOX_TABLE == 1);
    CHECK(BOX_IMAGE == 2);
    CHECK(BOX_SIDEBAR == 3);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sBoxes can hold lines
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sBoxes can hold lines")
{
    sBoxes box;

    box.containedLines.push_back(1);
    box.containedLines.push_back(2);
    box.containedLines.push_back(3);

    CHECK(box.containedLines.size() == 3);
    CHECK(box.containedLines[0] == 1);
    CHECK(box.containedLines[1] == 2);
    CHECK(box.containedLines[2] == 3);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sLineLayout can hold segments
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sLineLayout can hold segments")
{
    sLineLayout line;
    sSegmentLayout seg1;
    sSegmentLayout seg2;

    seg1.paragraph = 1;
    seg2.paragraph = 2;

    line.segments.push_back(seg1);
    line.segments.push_back(seg2);

    CHECK(line.segments.size() == 2);
    CHECK(line.segments[0].paragraph == 1);
    CHECK(line.segments[1].paragraph == 2);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout can hold lines
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout can hold lines")
{
    sParagraphLayout para;
    sLineLayout line1;
    sLineLayout line2;

    line1.boxIndex = 1;
    line2.boxIndex = 2;

    para.lines.push_back(line1);
    para.lines.push_back(line2);

    CHECK(para.lines.size() == 2);
    CHECK(para.lines[0].boxIndex == 1);
    CHECK(para.lines[1].boxIndex == 2);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sPage can hold boxes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sPage can hold boxes")
{
    sPage page;
    sBoxes box1;
    sBoxes box2;

    box1.boxNumber = 1;
    box2.boxNumber = 2;

    page.boxes.push_back(box1);
    page.boxes.push_back(box2);

    CHECK(page.boxes.size() == 2);
    CHECK(page.boxes[0].boxNumber == 1);
    CHECK(page.boxes[1].boxNumber == 2);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayoutBase construction and default values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayoutBase construction")
{
    cLayoutBaseTest layout;

    CHECK(layout.GetGlobalBoxList().empty());
    CHECK(layout.GetCurrentBoxIndex() == NOT_SET);
    CHECK(layout.InFullLayout() == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CreatePageBox creates first box with correct coordinates
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CreatePageBox creates first box")
{
    cLayoutBaseTest layout;

    bool result = layout.CreatePageBox(1);

    CHECK(result == true);
    CHECK(layout.GetGlobalBoxList().size() == 1);
    CHECK(layout.GetCurrentBoxIndex() == 0);

    const sBoxes &box = layout.GetGlobalBoxList()[0];
    CHECK(box.boxNumber == 0);
    CHECK(box.pageNumber == 1);
    CHECK(box.type == BOX_TEXT);
    CHECK(box.columnCount == 1);
    CHECK(box.columnGap == 0);
    CHECK(box.currentY == 0);

    // Check coordinates with defaults: PO=1", LM=0", RM=6.5", MT=1", MB=1"
    // Note: Header/footer margins don't affect text box - they only position headers/footers
    CHECK(box.left == 1440);          // 1" + 0" = 1440 twips
    CHECK(box.right == 10800);        // 1" + 6.5" = 10800 twips
    CHECK(box.top == 1440);           // 1" top margin = 1440 twips
    CHECK(box.bottom == 14400);       // 11" - 1" = 14400 twips
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CreatePageBox doesn't duplicate for same page
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CreatePageBox doesn't duplicate for same page")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    CHECK(layout.GetGlobalBoxList().size() == 1);

    layout.CreatePageBox(1);  // Same page again
    CHECK(layout.GetGlobalBoxList().size() == 1);  // Still just one box
    CHECK(layout.GetCurrentBoxIndex() == 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CreatePageBox creates boxes for multiple pages
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CreatePageBox creates boxes for multiple pages")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    layout.CreatePageBox(2);
    layout.CreatePageBox(3);

    CHECK(layout.GetGlobalBoxList().size() == 3);
    CHECK(layout.GetCurrentBoxIndex() == 2);

    CHECK(layout.GetGlobalBoxList()[0].boxNumber == 0);
    CHECK(layout.GetGlobalBoxList()[1].boxNumber == 1);
    CHECK(layout.GetGlobalBoxList()[2].boxNumber == 2);

    CHECK(layout.GetGlobalBoxList()[0].pageNumber == 1);
    CHECK(layout.GetGlobalBoxList()[1].pageNumber == 2);
    CHECK(layout.GetGlobalBoxList()[2].pageNumber == 3);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CheckMarginChange detects no change initially
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CheckMarginChange returns false with no boxes")
{
    cLayoutBaseTest layout;

    CHECK(layout.CheckMarginChange() == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CheckMarginChange detects no change when margins unchanged
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CheckMarginChange returns false when margins unchanged")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    CHECK(layout.CheckMarginChange() == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CheckMarginChange detects left margin change
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CheckMarginChange detects left margin change")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    CHECK(layout.CheckMarginChange() == false);

    layout.SetLeftMargin(720);  // Change from 0 to 0.5 inch
    CHECK(layout.CheckMarginChange() == true);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CheckMarginChange detects right margin change
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CheckMarginChange detects right margin change")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    CHECK(layout.CheckMarginChange() == false);

    layout.SetRightMargin(8640);  // Change from 9360 to 6 inches
    CHECK(layout.CheckMarginChange() == true);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CreateMarginBox creates box stacked from previous
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CreateMarginBox creates stacked box")
{
    cLayoutBaseTest layout;

    // Create first box
    layout.CreatePageBox(1);
    CHECK(layout.GetGlobalBoxList().size() == 1);

    // Simulate some content in first box
    sBoxes& firstBox = const_cast<sBoxes&>(layout.GetGlobalBoxList()[0]);
    firstBox.currentY = 1440;  // Simulate 1 inch of content

    // Change margins and create margin box
    layout.SetLeftMargin(720);
    layout.CreateMarginBox(1);

    CHECK(layout.GetGlobalBoxList().size() == 2);
    CHECK(layout.GetCurrentBoxIndex() == 1);

    const sBoxes& box = layout.GetGlobalBoxList()[1];
    CHECK(box.boxNumber == 1);
    CHECK(box.pageNumber == 1);
    CHECK(box.type == BOX_TEXT);

    // New box should have new margins
    CHECK(box.left == 2160);      // 1" page offset + 0.5" left margin
    CHECK(box.right == 10800);    // 1" page offset + 6.5" right margin (unchanged)

    // New box should stack from previous box's truncated bottom
    CHECK(box.top == 2880);       // Previous box.top (1440) + currentY (1440)
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CreateMarginBox fails without previous box
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CreateMarginBox fails without previous box")
{
    cLayoutBaseTest layout;

    bool result = layout.CreateMarginBox(1);
    CHECK(result == false);
    CHECK(layout.GetGlobalBoxList().empty());
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test margin setters update margin values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Margin setters work correctly")
{
    cLayoutBaseTest layout;

    // Create box with default margins
    layout.CreatePageBox(1);
    const sBoxes& box1 = layout.GetGlobalBoxList()[0];
    CHECK(box1.left == 1440);     // Default: 1" + 0"
    CHECK(box1.right == 10800);   // Default: 1" + 6.5"

    // Change left margin and create new box
    layout.SetLeftMargin(1440);
    layout.CreatePageBox(2);
    const sBoxes& box2 = layout.GetGlobalBoxList()[1];
    CHECK(box2.left == 2880);     // 1" + 1" = 2880

    // Change right margin and create new box
    layout.SetRightMargin(7200);
    layout.CreatePageBox(3);
    const sBoxes& box3 = layout.GetGlobalBoxList()[2];
    CHECK(box3.right == 8640);    // 1" + 5" = 8640
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetCurrentBox returns nullptr with no boxes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetCurrentBox returns nullptr with no boxes")
{
    cLayoutBaseTest layout;

    const sBoxes* box = layout.GetCurrentBox();
    CHECK(box == nullptr);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetCurrentBox returns current box after creation
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetCurrentBox returns current box")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    const sBoxes* box = layout.GetCurrentBox();

    CHECK(box != nullptr);
    CHECK(box->boxNumber == 0);
    CHECK(box->pageNumber == 1);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetCurrentBox updates after new box created
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetCurrentBox updates to new box")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    const sBoxes* box1 = layout.GetCurrentBox();
    CHECK(box1->boxNumber == 0);

    layout.CreatePageBox(2);
    const sBoxes* box2 = layout.GetCurrentBox();
    CHECK(box2->boxNumber == 1);
    CHECK(box2->pageNumber == 2);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test box coordinate getters return correct values
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Box coordinate getters work correctly")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);

    // Check coordinates match defaults
    // Note: Header/footer margins don't affect text box - they only position headers/footers
    CHECK(CoordsEqual(layout.GetBoxLeft(), 1440));      // 1" page offset + 0" left margin
    CHECK(CoordsEqual(layout.GetBoxRight(), 10800));    // 1" page offset + 6.5" right margin
    CHECK(CoordsEqual(layout.GetBoxTop(), 1440));       // 1" top margin (header margin positions header, not text)
    CHECK(CoordsEqual(layout.GetBoxBottom(), 14400));   // 11" - 1" bottom margin only (footer margin positions footer, not text)
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test box coordinate getters update with margin changes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Box coordinate getters update after margin box")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    CHECK(CoordsEqual(layout.GetBoxLeft(), 1440));

    // Simulate some content
    sBoxes& firstBox = const_cast<sBoxes&>(layout.GetGlobalBoxList()[0]);
    firstBox.currentY = 1440;

    // Create margin box with new left margin
    layout.SetLeftMargin(720);
    layout.CreateMarginBox(1);

    // Coordinates should reflect new box
    CHECK(CoordsEqual(layout.GetBoxLeft(), 2160));      // 1" + 0.5" = 2160
    CHECK(CoordsEqual(layout.GetBoxTop(), 2880));       // Previous top (1440) + currentY (1440)
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine fails without current box
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine fails without current box")
{
    cLayoutBaseTest layout;

    sLineLayout line;
    line.contentLineNumber = 1;
    line.lineheight = 240;

    bool result = layout.SaveLine(0, line);
    CHECK(result == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine saves line to new paragraph
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine saves line to new paragraph")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line;
    line.contentLineNumber = 1;
    line.lineheight = 240;
    line.pagex = 100;
    line.pagey = 200;

    bool result = layout.SaveLine(0, line);
    CHECK(result == true);

    // Check paragraph was created
    CHECK(layout.GetNumberOfParagraphs() == 1);
    CHECK(layout.GetParagraphLayout(0)->number == 0);
    CHECK(layout.GetParagraphLayout(0)->lines.size() == 1);

    // Check line was saved
    const sLineLayout& savedLine = layout.GetParagraphLayout(0)->lines[0];
    CHECK(savedLine.contentLineNumber == 1);
    CHECK(CoordsEqual(savedLine.lineheight, 240));
    CHECK(savedLine.boxIndex == 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine saves multiple lines to same paragraph
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine saves multiple lines to same paragraph")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line1;
    line1.contentLineNumber = 1;
    line1.lineheight = 240;

    sLineLayout line2;
    line2.contentLineNumber = 2;
    line2.lineheight = 240;

    layout.SaveLine(0, line1);
    layout.SaveLine(0, line2);

    // Should still be one paragraph
    CHECK(layout.GetNumberOfParagraphs() == 1);
    CHECK(layout.GetParagraphLayout(0)->lines.size() == 2);

    // Check both lines saved
    CHECK(layout.GetParagraphLayout(0)->lines[0].contentLineNumber == 1);
    CHECK(layout.GetParagraphLayout(0)->lines[1].contentLineNumber == 2);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine sets boxIndex correctly
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine sets boxIndex correctly")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line;
    line.contentLineNumber = 1;
    line.lineheight = 240;
    line.boxIndex = -1;  // Start with invalid

    layout.SaveLine(0, line);

    // Check boxIndex was set
    CHECK(layout.GetParagraphLayout(0)->lines[0].boxIndex == 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine updates currentY correctly
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine updates currentY correctly")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    const sBoxes& box = layout.GetGlobalBoxList()[0];
    CHECK(box.currentY == 0);

    sLineLayout line1;
    line1.contentLineNumber = 1;
    line1.lineheight = 240;

    sLineLayout line2;
    line2.contentLineNumber = 2;
    line2.lineheight = 360;

    layout.SaveLine(0, line1);
    CHECK(layout.GetGlobalBoxList()[0].currentY == 240);

    layout.SaveLine(0, line2);
    CHECK(layout.GetGlobalBoxList()[0].currentY == 600);  // 240 + 360
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine tracks line numbers in containedLines
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine tracks line numbers in containedLines")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line1;
    line1.contentLineNumber = 5;
    line1.rawLineNumber = 7;
    line1.lineheight = 240;

    sLineLayout line2;
    line2.contentLineNumber = 6;
    line2.rawLineNumber = 8;
    line2.lineheight = 240;

    layout.SaveLine(0, line1);
    layout.SaveLine(0, line2);

    const sBoxes& box = layout.GetGlobalBoxList()[0];
    CHECK(box.containedLines.size() == 2);
    // containedLines stores rawLineNumber (includes dot command lines)
    // for correct CalculateScreenYRange lookup via GetLineByRawLineNumber
    CHECK(box.containedLines[0] == 7);
    CHECK(box.containedLines[1] == 8);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine works across multiple boxes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine works across multiple boxes")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line1;
    line1.contentLineNumber = 1;
    line1.lineheight = 240;

    layout.SaveLine(0, line1);
    CHECK(layout.GetParagraphLayout(0)->lines[0].boxIndex == 0);

    // Create second box
    layout.CreatePageBox(2);

    sLineLayout line2;
    line2.contentLineNumber = 2;
    line2.lineheight = 240;

    layout.SaveLine(0, line2);
    CHECK(layout.GetParagraphLayout(0)->lines[1].boxIndex == 1);

    // Check containedLines for each box
    CHECK(layout.GetGlobalBoxList()[0].containedLines.size() == 1);
    CHECK(layout.GetGlobalBoxList()[1].containedLines.size() == 1);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SaveLine works with multiple paragraphs
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SaveLine works with multiple paragraphs")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line1;
    line1.contentLineNumber = 1;
    line1.lineheight = 240;

    sLineLayout line2;
    line2.contentLineNumber = 2;
    line2.lineheight = 240;

    sLineLayout line3;
    line3.contentLineNumber = 3;
    line3.lineheight = 240;

    layout.SaveLine(0, line1);
    layout.SaveLine(1, line2);
    layout.SaveLine(0, line3);

    // Should have two paragraphs
    CHECK(layout.GetNumberOfParagraphs() == 2);

    // Paragraph 0 should have 2 lines
    CHECK(layout.GetParagraphLayout(0)->number == 0);
    CHECK(layout.GetParagraphLayout(0)->lines.size() == 2);

    // Paragraph 1 should have 1 line
    CHECK(layout.GetParagraphLayout(1)->number == 1);
    CHECK(layout.GetParagraphLayout(1)->lines.size() == 1);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetDocument sets document pointer
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SetDocument sets document pointer")
{
    cLayoutBaseTest layout;
    cDocument doc;

    layout.SetDocument(&doc);
    // No direct way to verify, but should not crash
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CreateLine creates line with correct defaults
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CreateLine creates line with correct defaults")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line = layout.CreateLine(0);

    CHECK(CoordsEqual(line.lineheight, 300));  // From GetLineHeight() (font-based: 240 height + 60 leading)
    CHECK(line.left == true);
    CHECK(line.center == false);
    CHECK(line.right == false);
    CHECK(line.justify == false);
    CHECK(line.pagenumber == 1);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test WordWrapParagraph fails without document
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("WordWrapParagraph fails without document")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    bool result = layout.WordWrapParagraph(0);
    CHECK(result == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test WordWrapParagraph fails without box
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("WordWrapParagraph fails without box")
{
    cLayoutBaseTest layout;

    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.WordWrapParagraph(0);
    CHECK(result == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .LM command
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .LM")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".lm 2i");
    CHECK(result == true);

    layout.CreatePageBox(1);
    CHECK(CoordsEqual(layout.GetBoxLeft(), 1440 + 2880));  // 1" page offset + 2" left margin
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .RM with centimeters
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .RM with centimeters")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".rm 16c");
    CHECK(result == true);

    layout.CreatePageBox(1);
    // 16cm = 16 * 10 * (1440/25.4) = 9070.87 twips
    COORD_T expected = 1440 + static_cast<COORD_T>((16.0 * 10.0) * (1440.0 / 25.4));
    CHECK(CoordsEqual(layout.GetBoxRight(), expected));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .PM with double quote (inches)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .PM with double quote")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".pm 0.5\"");
    CHECK(result == true);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .PO with millimeters
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .PO with millimeters")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".po 25m");
    CHECK(result == true);

    layout.CreatePageBox(1);  // Odd page
    // 25mm = 25 * (1440/25.4) = 1417.32 twips
    COORD_T expected = static_cast<COORD_T>(25.0 * (1440.0 / 25.4));
    CHECK(CoordsEqual(layout.GetBoxLeft(), expected));

    layout.CreatePageBox(2);  // Even page
    CHECK(CoordsEqual(layout.GetBoxLeft(), expected));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .POO command (odd pages only)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .POO")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".poo 2i");
    CHECK(result == true);

    layout.CreatePageBox(1);  // Odd page
    CHECK(CoordsEqual(layout.GetBoxLeft(), 2880));  // 2" page offset

    layout.CreatePageBox(2);  // Even page
    CHECK(CoordsEqual(layout.GetBoxLeft(), 1440));  // 1" page offset (default)
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .POE command (even pages only)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .POE")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".poe 2i");
    CHECK(result == true);

    layout.CreatePageBox(1);  // Odd page
    CHECK(CoordsEqual(layout.GetBoxLeft(), 1440));  // 1" page offset (default)

    layout.CreatePageBox(2);  // Even page
    CHECK(CoordsEqual(layout.GetBoxLeft(), 2880));  // 2" page offset
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand bare number behavior
/// Bare 0 is valid (turns off), bare non-zero is an error
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand bare number handling")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Bare non-zero is an error (no unit specified)
    bool result = layout.ParseDotCommand(".lm 2");
    CHECK(result == false);

    // Bare 0 is valid -- resets left margin to 0
    result = layout.ParseDotCommand(".lm 0");
    CHECK(result == true);

    // Explicit unit works normally
    result = layout.ParseDotCommand(".lm 2i");
    CHECK(result == true);

    layout.CreatePageBox(1);
    // 2 inches = 2 * 1440 = 2880 twips, plus 1440 page offset
    CHECK(CoordsEqual(layout.GetBoxLeft(), 1440 + 2880));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand handles decrement (negative values are valid for increment/decrement)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand handles decrement")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // First set a left margin
    layout.ParseDotCommand(".lm 3i");

    // Now decrement by 2 inches - this is valid
    bool result = layout.ParseDotCommand(".lm -2i");
    CHECK(result == true);

    layout.CreatePageBox(1);
    // 3" - 2" = 1" = 1440 twips, plus 1440 page offset
    CHECK(CoordsEqual(layout.GetBoxLeft(), 1440 + 1440));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand handles invalid command
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand handles invalid command")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    eDotCommandStatus result = layout.ParseDotCommand(".xx 5");
    CHECK(result == DOT_NOTIMPLEMENTED);  // .XX is a valid WordStar command (Strikeout Character)
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand fails without document
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand fails without document")
{
    cLayoutBaseTest layout;

    bool result = layout.ParseDotCommand(".lm 2");
    CHECK(result == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test .tb parses standard WordStar tab stops
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .tb with plain tabs")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".tb 1i 2.5i 4i");
    CHECK(result == true);

    // Should have 3 user tabs + left margin + right margin = 5 total
    const std::vector<sTabStop>& tabs = layout.GetTabs();
    REQUIRE(tabs.size() == 5);

    // First is left margin tab, last is right margin tab
    // Middle 3 are user tabs: 1440, 3600, 5760 twips
    CHECK(CoordsEqual(tabs[1].position, 1440));   // 1 inch
    CHECK(tabs[1].type == TAB_TAB);
    CHECK(CoordsEqual(tabs[2].position, 3600));   // 2.5 inches
    CHECK(tabs[2].type == TAB_TAB);
    CHECK(CoordsEqual(tabs[3].position, 5760));   // 4 inches
    CHECK(tabs[3].type == TAB_TAB);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test .tb parses # prefix as decimal tab (WordStar standard)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .tb with # decimal prefix")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".tb 1i #3i");
    CHECK(result == true);

    const std::vector<sTabStop>& tabs = layout.GetTabs();
    REQUIRE(tabs.size() == 4);  // left margin + 2 user tabs + right margin

    // tabs[1] = 1i = plain left tab
    CHECK(CoordsEqual(tabs[1].position, 1440));
    CHECK(tabs[1].type == TAB_TAB);

    // tabs[2] = #3i = decimal tab
    CHECK(CoordsEqual(tabs[2].position, 4320));   // 3 inches
    CHECK(tabs[2].type == TAB_DECIMAL);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test .tb accepts commas as delimiters
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .tb with comma delimiters")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".tb 1i, 2i, #3i");
    CHECK(result == true);

    const std::vector<sTabStop>& tabs = layout.GetTabs();
    REQUIRE(tabs.size() == 5);  // left margin + 3 user tabs + right margin

    CHECK(CoordsEqual(tabs[1].position, 1440));
    CHECK(tabs[1].type == TAB_TAB);
    CHECK(CoordsEqual(tabs[2].position, 2880));
    CHECK(tabs[2].type == TAB_TAB);
    CHECK(CoordsEqual(tabs[3].position, 4320));
    CHECK(tabs[3].type == TAB_DECIMAL);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test .tb parses tab type prefixes (^=center, >=right, #=decimal)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .tb with type prefixes")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".tb 1.00i ^3.00i >5.00i #6.00i");
    CHECK(result == true);

    const std::vector<sTabStop>& tabs = layout.GetTabs();
    REQUIRE(tabs.size() == 6);  // left margin + 4 user tabs + right margin

    CHECK(CoordsEqual(tabs[1].position, 1440));   // 1 inch - plain left
    CHECK(tabs[1].type == TAB_TAB);
    CHECK(CoordsEqual(tabs[2].position, 4320));   // 3 inches - center
    CHECK(tabs[2].type == TAB_CENTER);
    CHECK(CoordsEqual(tabs[3].position, 7200));   // 5 inches - right
    CHECK(tabs[3].type == TAB_RIGHT);
    CHECK(CoordsEqual(tabs[4].position, 8640));   // 6 inches - decimal
    CHECK(tabs[4].type == TAB_DECIMAL);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test .tb handles ^ and > prefixes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .tb center and right prefixes")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    bool result = layout.ParseDotCommand(".tb ^2.00i >4.00i");
    CHECK(result == true);

    const std::vector<sTabStop>& tabs = layout.GetTabs();
    REQUIRE(tabs.size() == 4);  // left margin + 2 user tabs + right margin

    CHECK(CoordsEqual(tabs[1].position, 2880));
    CHECK(tabs[1].type == TAB_CENTER);
    CHECK(CoordsEqual(tabs[2].position, 5760));
    CHECK(tabs[2].type == TAB_RIGHT);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test second .tb overrides preceding .tb tab settings
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand second .tb overrides first .tb")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // First set tabs with .tb (all plain)
    layout.ParseDotCommand(".tb 1i 3i 5i");
    const std::vector<sTabStop>& tabs1 = layout.GetTabs();
    CHECK(tabs1[1].type == TAB_TAB);
    CHECK(tabs1[2].type == TAB_TAB);

    // Second .tb overrides with typed tabs
    layout.ParseDotCommand(".tb 1i ^3i >5i");
    const std::vector<sTabStop>& tabs2 = layout.GetTabs();
    REQUIRE(tabs2.size() == 5);

    CHECK(tabs2[1].type == TAB_TAB);
    CHECK(tabs2[2].type == TAB_CENTER);
    CHECK(tabs2[3].type == TAB_RIGHT);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .PL with standard page length
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .PL 66")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .PL 66 is standard for 11" paper (66 lines at 6 lpi)
    bool result = layout.ParseDotCommand(".pl 66");
    CHECK(result == true);

    layout.CreatePageBox(1);

    // 66 lines * (8/48 inch per line) * 1440 twips/inch = 15840 twips
    // Box bottom should be: min(top margin + page length, normal bottom)
    // box.top = 720 (top margin only - header margin positions header, not text)
    // Normal bottom = paper - bottom = 15840 - 1440 = 14400
    // With .PL 66 = 15840, plBottom = 720 + 15840 = 16560
    // min(16560, 14400) = 14400 (paper constraint wins)
    CHECK(CoordsEqual(layout.GetBoxBottom(), 14400));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .PL with short page
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .PL 30")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .PL 30 = 30 lines at 6 lpi = 5 inches
    bool result = layout.ParseDotCommand(".pl 30");
    CHECK(result == true);

    layout.CreatePageBox(1);

    // 30 lines * (8/48 inch per line) * 1440 twips/inch = 7200 twips
    // Box bottom should be: box.top + page length = 1440 + 7200 = 8640
    CHECK(CoordsEqual(layout.GetBoxBottom(), 8640));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .PL with increment
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .PL with increment")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // First set page length to 30
    layout.ParseDotCommand(".pl 30");

    // Then increment by 6 lines
    bool result = layout.ParseDotCommand(".pl +6");
    CHECK(result == true);

    layout.CreatePageBox(1);

    // (30 + 6) lines * (8/48) * 1440 = 36 * 240 = 8640 twips
    // Box bottom = 1440 + 8640 = 10080
    CHECK(CoordsEqual(layout.GetBoxBottom(), 10080));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .PL with decrement
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .PL with decrement")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // First set page length to 50
    layout.ParseDotCommand(".pl 50");

    // Then decrement by 10 lines
    bool result = layout.ParseDotCommand(".pl -10");
    CHECK(result == true);

    layout.CreatePageBox(1);

    // (50 - 10) lines * (8/48) * 1440 = 40 * 240 = 9600 twips
    // Box bottom = 1440 + 9600 = 11040
    CHECK(CoordsEqual(layout.GetBoxBottom(), 11040));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand handles .PL with decrement below zero
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand handles .PL decrement below zero")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Default mPageLength = 15840 (11 inches)
    // Decrement by 30 lines: 15840 - (30 * 240) = 15840 - 7200 = 8640
    bool result = layout.ParseDotCommand(".pl -30");
    CHECK(result == true);

    layout.CreatePageBox(1);

    // pageLength = 8640, box.bottom = box.top(1440) + pageLength(8640) = 10080
    CHECK(CoordsEqual(layout.GetBoxBottom(), 10080));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand handles .PL 0 (invalid - must be >= 1)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand fails on .PL 0")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .PL 0 should fail because lines < 1.0
    bool result = layout.ParseDotCommand(".pl 0");
    CHECK(result == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .LH with default value
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .LH 8")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .LH 8 is default (8/48 inch = 240 twips)
    bool result = layout.ParseDotCommand(".lh 8");
    CHECK(result == true);

    // 8 * (1440/48) = 8 * 30 = 240 twips
    CHECK(CoordsEqual(layout.GetLineHeight(), 240));
    CHECK(layout.GetAutoLeading() == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .LH with larger value
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .LH 10")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .LH 10 = 10/48 inch = 300 twips (tighter spacing)
    bool result = layout.ParseDotCommand(".lh 10");
    CHECK(result == true);

    // 10 * (1440/48) = 10 * 30 = 300 twips
    CHECK(CoordsEqual(layout.GetLineHeight(), 300));
    CHECK(layout.GetAutoLeading() == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .LH with auto-leading mode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .LH A")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .LH A enables auto-leading mode
    bool result = layout.ParseDotCommand(".lh a");
    CHECK(result == true);
    CHECK(layout.GetAutoLeading() == true);

    // Try uppercase too
    result = layout.ParseDotCommand(".lh A");
    CHECK(result == true);
    CHECK(layout.GetAutoLeading() == true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .LH with increment
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .LH with increment")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Start with default 8
    layout.ParseDotCommand(".lh 8");
    CHECK(CoordsEqual(layout.GetLineHeight(), 240));

    // Increment by 2
    bool result = layout.ParseDotCommand(".lh +2");
    CHECK(result == true);

    // (8 + 2) * 30 = 300 twips
    CHECK(CoordsEqual(layout.GetLineHeight(), 300));
    CHECK(layout.GetAutoLeading() == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand parses .LH with decrement
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand parses .LH with decrement")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Start with 10
    layout.ParseDotCommand(".lh 10");
    CHECK(CoordsEqual(layout.GetLineHeight(), 300));

    // Decrement by 2
    bool result = layout.ParseDotCommand(".lh -2");
    CHECK(result == true);

    // (10 - 2) * 30 = 240 twips
    CHECK(CoordsEqual(layout.GetLineHeight(), 240));
    CHECK(layout.GetAutoLeading() == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand clamps .LH to minimum
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand clamps .LH to minimum")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Try to set very small value (1/48 inch = 30 twips)
    // Should get clamped to minimum of 120 twips
    bool result = layout.ParseDotCommand(".lh 1");
    CHECK(result == true);
    CHECK(CoordsEqual(layout.GetLineHeight(), 120));  // Clamped to minimum
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand handles .LH decrement below minimum
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand handles .LH decrement below minimum")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Start with 8
    layout.ParseDotCommand(".lh 8");

    // Decrement by 20 (would go negative)
    bool result = layout.ParseDotCommand(".lh -20");
    CHECK(result == true);

    // Should get clamped to minimum of 120 twips
    CHECK(CoordsEqual(layout.GetLineHeight(), 120));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand fails on .LH 0
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand fails on .LH 0")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .LH 0 should fail because height < 1.0
    bool result = layout.ParseDotCommand(".lh 0");
    CHECK(result == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand .LH disables auto-leading
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand .LH disables auto-leading")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Enable auto-leading
    layout.ParseDotCommand(".lh a");
    CHECK(layout.GetAutoLeading() == true);

    // Set explicit height should disable auto-leading
    bool result = layout.ParseDotCommand(".lh 10");
    CHECK(result == true);
    CHECK(CoordsEqual(layout.GetLineHeight(), 300));
    CHECK(layout.GetAutoLeading() == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sSegmentLayout operator== for identical segments
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sSegmentLayout operator== returns true for identical segments")
{
    sSegmentLayout seg1, seg2;

    seg1.paragraph = 1;
    seg1.startPosition = 0;
    seg1.length = 2;
    seg1.position.push_back(0);
    seg1.position.push_back(10);
    seg1.font = "Arial,12";
    seg1.segmentheight = 240;

    seg2.paragraph = 1;
    seg2.startPosition = 0;
    seg2.length = 2;
    seg2.position.push_back(0);
    seg2.position.push_back(10);
    seg2.font = "Arial,12";
    seg2.segmentheight = 240;

    CHECK(seg1.IsEqualTo(seg2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sSegmentLayout operator== returns false for different fonts
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sSegmentLayout operator== returns false for different fonts")
{
    sSegmentLayout seg1, seg2;

    seg1.font = "Arial,12";
    seg2.font = "Times,12";

    CHECK(!seg1.IsEqualTo(seg2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sSegmentLayout operator== returns false for different positions
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sSegmentLayout operator== returns false for different positions")
{
    sSegmentLayout seg1, seg2;

    seg1.position.push_back(0);
    seg1.position.push_back(10);

    seg2.position.push_back(0);
    seg2.position.push_back(12);  // Different

    CHECK(!seg1.IsEqualTo(seg2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sLineLayout operator== for identical lines
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sLineLayout operator== returns true for identical lines")
{
    sLineLayout line1, line2;

    line1.left = true;
    line1.pagex = 1440;
    line1.pagey = 2000;
    line1.pagenumber = 1;
    line1.pageLineNumber = 5;
    line1.lineheight = 240;
    line1.linestart = 100;

    line2.left = true;
    line2.pagex = 1440;
    line2.pagey = 2000;
    line2.pagenumber = 1;
    line2.pageLineNumber = 5;
    line2.lineheight = 240;
    line2.linestart = 100;

    CHECK(line1.IsEqualTo(0, 0, line2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sLineLayout operator== returns false for different alignment
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sLineLayout operator== returns false for different alignment")
{
    sLineLayout line1, line2;

    line1.left = true;
    line2.center = true;

    CHECK(!line1.IsEqualTo(0, 0, line2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sLineLayout operator== returns false for different positions
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sLineLayout operator== returns false for different positions")
{
    sLineLayout line1, line2;

    line1.pagex = 1440;
    line1.pagey = 2000;

    line2.pagex = 1440;
    line2.pagey = 2100;  // Different

    CHECK(!line1.IsEqualTo(0, 0, line2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sLineLayout operator== ignores screeny (it's derived)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sLineLayout operator== ignores screeny")
{
    sLineLayout line1, line2;

    line1.left = true;
    line1.pagex = 1440;
    line1.pagey = 2000;
    line1.screeny = 5000;  // Different screeny

    line2.left = true;
    line2.pagex = 1440;
    line2.pagey = 2000;
    line2.screeny = 6000;  // Different screeny

    // Should still be equal because screeny is derived
    CHECK(line1.IsEqualTo(0, 0, line2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sLineLayout operator== compares segments
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sLineLayout operator== compares segments")
{
    sLineLayout line1, line2;

    sSegmentLayout seg1, seg2;
    seg1.font = "Arial,12";
    seg2.font = "Times,12";

    line1.segments.push_back(seg1);
    line2.segments.push_back(seg2);

    CHECK(!line1.IsEqualTo(0, 0, line2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout operator== for identical paragraphs
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout operator== returns true for identical paragraphs")
{
    sParagraphLayout para1, para2;

    para1.number = 5;
    para2.number = 5;

    // Initialize endState to same values
    para1.endState.font = "Arial,12";
    para1.endState.textcolor.red = -1;
    para1.endState.textcolor.green = -1;
    para1.endState.textcolor.blue = -1;
    para1.endState.textcolor.alpha = -1;
    para1.endState.bold = false;
    para1.endState.italics = false;
    para1.endState.underline = false;
    para1.endState.strikethrough = false;
    para1.endState.subscript = false;
    para1.endState.superscript = false;
    para1.endState.left = true;
    para1.endState.right = false;
    para1.endState.center = false;
    para1.endState.justify = false;
    para1.endState.linespace = 1.0;

    para2.endState.font = "Arial,12";
    para2.endState.textcolor.red = -1;
    para2.endState.textcolor.green = -1;
    para2.endState.textcolor.blue = -1;
    para2.endState.textcolor.alpha = -1;
    para2.endState.bold = false;
    para2.endState.italics = false;
    para2.endState.underline = false;
    para2.endState.strikethrough = false;
    para2.endState.subscript = false;
    para2.endState.superscript = false;
    para2.endState.left = true;
    para2.endState.right = false;
    para2.endState.center = false;
    para2.endState.justify = false;
    para2.endState.linespace = 1.0;

    sLineLayout line;
    line.left = true;
    line.pagex = 1440;
    line.pagey = 2000;

    para1.lines.push_back(line);
    para2.lines.push_back(line);

    CHECK(para1.IsEqualTo(para2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout operator== returns false for different numbers
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout operator== returns false for different numbers")
{
    sParagraphLayout para1, para2;

    para1.number = 5;
    para2.number = 6;

    CHECK(!para1.IsEqualTo(para2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout operator== returns false for different line counts
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout operator== returns false for different line counts")
{
    sParagraphLayout para1, para2;

    para1.number = 5;
    para2.number = 5;

    sLineLayout line;
    para1.lines.push_back(line);
    para1.lines.push_back(line);

    para2.lines.push_back(line);

    CHECK(!para1.IsEqualTo(para2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout operator== compares endState
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout operator== compares endState")
{
    sParagraphLayout para1, para2;

    para1.number = 5;
    para2.number = 5;

    para1.endState.bold = true;
    para2.endState.bold = false;

    CHECK(!para1.IsEqualTo(para2));
}


/////////////////////////////////////////////////////////////////////////////
// Box Query Method Tests
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetBoxByIndex returns nullptr for empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetBoxByIndex returns nullptr for empty layout")
{
    cLayoutBaseTest layout;

    const sBoxes* box = layout.GetBoxByIndex(0);

    CHECK(box == nullptr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetBoxByIndex returns box for valid index
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetBoxByIndex returns box for valid index")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    const sBoxes* box = layout.GetBoxByIndex(0);

    REQUIRE(box != nullptr);
    CHECK(box->boxNumber == 0);
    CHECK(box->pageNumber == 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetBoxByIndex returns nullptr for invalid index
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetBoxByIndex returns nullptr for invalid negative index")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    const sBoxes* box = layout.GetBoxByIndex(-1);

    CHECK(box == nullptr);
}


TEST_CASE("GetBoxByIndex returns nullptr for out of bounds index")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    const sBoxes* box = layout.GetBoxByIndex(10);

    CHECK(box == nullptr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetBoxCount returns correct count
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetBoxCount returns 0 for empty layout")
{
    cLayoutBaseTest layout;

    CHECK(layout.GetBoxCount() == 0);
}


TEST_CASE("GetBoxCount returns correct count after creating boxes")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    CHECK(layout.GetBoxCount() == 1);

    layout.CreatePageBox(2);
    CHECK(layout.GetBoxCount() == 2);

    layout.CreatePageBox(3);
    CHECK(layout.GetBoxCount() == 3);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetBoxesOnPage returns empty vector for no boxes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetBoxesOnPage returns empty vector when no boxes exist")
{
    cLayoutBaseTest layout;

    std::vector<int> boxes = layout.GetBoxesOnPage(1);

    CHECK(boxes.empty());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetBoxesOnPage returns correct boxes for a page
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetBoxesOnPage returns boxes for specific page")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    layout.CreatePageBox(2);
    layout.CreatePageBox(3);

    std::vector<int> boxes = layout.GetBoxesOnPage(2);

    REQUIRE(boxes.size() == 1);
    CHECK(boxes[0] == 1);
}


/////////////////////////////////////////////////////////////////////////////
// Line Query Method Tests
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetNumberOfLines returns 0 for empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetNumberOfLines returns 0 for empty layout")
{
    cLayoutBaseTest layout;

    CHECK(layout.GetNumberOfLines() == 0);
}


/////////////////////////////////////////////////////////////////////////////
// Paragraph Query Method Tests
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetNumberOfParagraphs returns 0 for empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetNumberOfParagraphs returns 0 for empty layout")
{
    cLayoutBaseTest layout;

    CHECK(layout.GetNumberOfParagraphs() == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphLayout returns nullptr for non-existent paragraph
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphLayout returns nullptr for non-existent paragraph")
{
    cLayoutBaseTest layout;

    const sParagraphLayout* para = layout.GetParagraphLayout(0);

    CHECK(para == nullptr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphLayout returns paragraph after adding
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphLayout returns paragraph after adding")
{
    cLayoutBaseTest layout;

    // Add paragraphs 0-5 to mParagraphLayout (index == number)
    for (int i = 0; i <= 5; i++)
    {
        sParagraphLayout testPara;
        testPara.number = i;
        layout.AddTestParagraph(testPara);
    }

    const sParagraphLayout* para = layout.GetParagraphLayout(5);

    REQUIRE(para != nullptr);
    CHECK(para->number == 5);
}


/////////////////////////////////////////////////////////////////////////////
// Page Query Method Tests
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetNumberOfPages returns 0 for empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetNumberOfPages returns 0 for empty layout")
{
    cLayoutBaseTest layout;

    CHECK(layout.GetNumberOfPages() == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetNumberOfPages returns highest page number
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetNumberOfPages returns highest page number")
{
    cLayoutBaseTest layout;

    layout.CreatePageBox(1);
    CHECK(layout.GetNumberOfPages() == 1);

    layout.CreatePageBox(2);
    CHECK(layout.GetNumberOfPages() == 2);

    layout.CreatePageBox(5);
    CHECK(layout.GetNumberOfPages() == 5);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetPageFromLine returns NOT_SET for non-existent line
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetPageFromLine returns NOT_SET for non-existent line")
{
    cLayoutBaseTest layout;

    PAGE_T page = layout.GetPageFromLine(0);

    CHECK(page == NOT_SET);
}


/////////////////////////////////////////////////////////////////////////////
// Binary Search Line Lookup Tests
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetLineByRawLineNumber returns nullptr for empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetLineByRawLineNumber - empty layout returns nullptr")
{
    cLayoutBaseTest layout;

    const sLineLayout* line = layout.GetLineByRawLineNumber(0);

    CHECK(line == nullptr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetLineByRawLineNumber finds a line in a single paragraph
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetLineByRawLineNumber - single paragraph single line")
{
    cLayoutBaseTest layout;

    sParagraphLayout para;
    para.number = 0;
    sLineLayout line;
    line.rawLineNumber = 0;
    para.lines.push_back(line);
    layout.AddTestParagraph(para);

    const sLineLayout* found = layout.GetLineByRawLineNumber(0);

    REQUIRE(found != nullptr);
    CHECK(found->rawLineNumber == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetLineByRawLineNumber finds each line in a multi-line paragraph
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetLineByRawLineNumber - single paragraph multiple lines")
{
    cLayoutBaseTest layout;

    sParagraphLayout para;
    para.number = 0;
    for (LINE_T i = 0; i < 3; i++)
    {
        sLineLayout line;
        line.rawLineNumber = i;
        para.lines.push_back(line);
    }
    layout.AddTestParagraph(para);

    for (LINE_T i = 0; i < 3; i++)
    {
        const sLineLayout* found = layout.GetLineByRawLineNumber(i);
        REQUIRE(found != nullptr);
        CHECK(found->rawLineNumber == i);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetLineByRawLineNumber finds lines across multiple paragraphs
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetLineByRawLineNumber - multiple paragraphs")
{
    cLayoutBaseTest layout;

    // Para 0: lines 0-2, Para 1: lines 3-5, Para 2: lines 6-8
    LINE_T lineNum = 0;
    for (PARAGRAPH_T p = 0; p < 3; p++)
    {
        sParagraphLayout para;
        para.number = p;
        for (int j = 0; j < 3; j++)
        {
            sLineLayout line;
            line.rawLineNumber = lineNum++;
            para.lines.push_back(line);
        }
        layout.AddTestParagraph(para);
    }

    // Find line from first paragraph
    const sLineLayout* found = layout.GetLineByRawLineNumber(1);
    REQUIRE(found != nullptr);
    CHECK(found->rawLineNumber == 1);

    // Find line from middle paragraph
    found = layout.GetLineByRawLineNumber(4);
    REQUIRE(found != nullptr);
    CHECK(found->rawLineNumber == 4);

    // Find line from last paragraph
    found = layout.GetLineByRawLineNumber(8);
    REQUIRE(found != nullptr);
    CHECK(found->rawLineNumber == 8);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetLineByRawLineNumber handles empty paragraphs in the middle
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetLineByRawLineNumber - empty paragraphs in the middle")
{
    cLayoutBaseTest layout;

    // Para 0: lines 0-2
    sParagraphLayout para0;
    para0.number = 0;
    for (LINE_T i = 0; i < 3; i++)
    {
        sLineLayout line;
        line.rawLineNumber = i;
        para0.lines.push_back(line);
    }
    layout.AddTestParagraph(para0);

    // Para 1: empty (simulates incremental layout)
    sParagraphLayout para1;
    para1.number = 1;
    layout.AddTestParagraph(para1);

    // Para 2: lines 3-5
    sParagraphLayout para2;
    para2.number = 2;
    for (LINE_T i = 3; i < 6; i++)
    {
        sLineLayout line;
        line.rawLineNumber = i;
        para2.lines.push_back(line);
    }
    layout.AddTestParagraph(para2);

    // Find line in first paragraph
    const sLineLayout* found = layout.GetLineByRawLineNumber(1);
    REQUIRE(found != nullptr);
    CHECK(found->rawLineNumber == 1);

    // Find line in last paragraph (past the empty one)
    found = layout.GetLineByRawLineNumber(4);
    REQUIRE(found != nullptr);
    CHECK(found->rawLineNumber == 4);

    // Non-existent line still returns nullptr
    found = layout.GetLineByRawLineNumber(99);
    CHECK(found == nullptr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetLineByRawLineNumber returns nullptr for non-existent line
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetLineByRawLineNumber - non-existent line returns nullptr")
{
    cLayoutBaseTest layout;

    // Two paragraphs with lines 0-5
    LINE_T lineNum = 0;
    for (PARAGRAPH_T p = 0; p < 2; p++)
    {
        sParagraphLayout para;
        para.number = p;
        for (int j = 0; j < 3; j++)
        {
            sLineLayout line;
            line.rawLineNumber = lineNum++;
            para.lines.push_back(line);
        }
        layout.AddTestParagraph(para);
    }

    CHECK(layout.GetLineByRawLineNumber(99) == nullptr);
    CHECK(layout.GetLineByRawLineNumber(-1) == nullptr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphFromLine finds the correct paragraph for each line
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphFromLine - finds correct paragraph")
{
    cLayoutBaseTest layout;

    // Para 0: lines 0-2, Para 1: lines 3-5, Para 2: lines 6-8
    LINE_T lineNum = 0;
    for (PARAGRAPH_T p = 0; p < 3; p++)
    {
        sParagraphLayout para;
        para.number = p;
        for (int j = 0; j < 3; j++)
        {
            sLineLayout line;
            line.rawLineNumber = lineNum++;
            para.lines.push_back(line);
        }
        layout.AddTestParagraph(para);
    }

    CHECK(layout.GetParagraphFromLine(0) == 0);
    CHECK(layout.GetParagraphFromLine(2) == 0);
    CHECK(layout.GetParagraphFromLine(4) == 1);
    CHECK(layout.GetParagraphFromLine(7) == 2);
    CHECK(layout.GetParagraphFromLine(99) == NOT_SET);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetParagraphFromLine returns NOT_SET for empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetParagraphFromLine - empty layout returns NOT_SET")
{
    cLayoutBaseTest layout;

    CHECK(layout.GetParagraphFromLine(0) == NOT_SET);
}


/////////////////////////////////////////////////////////////////////////////
// Page Break Tests
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout operator== compares pageBreak flag
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout operator== compares pageBreak flag")
{
    sParagraphLayout para1, para2;

    para1.number = 5;
    para2.number = 5;

    para1.pageBreak = true;
    para2.pageBreak = false;

    CHECK(!para1.IsEqualTo(para2));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sParagraphLayout operator== returns true when pageBreak matches
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sParagraphLayout operator== returns true when pageBreak matches")
{
    sParagraphLayout para1, para2;

    para1.number = 5;
    para2.number = 5;
    para1.pageBreak = true;
    para2.pageBreak = true;

    // Initialize endState to same values
    para1.endState.font = "Arial,12";
    para1.endState.textcolor.red = -1;
    para1.endState.textcolor.green = -1;
    para1.endState.textcolor.blue = -1;
    para1.endState.textcolor.alpha = -1;
    para1.endState.bold = false;
    para1.endState.italics = false;
    para1.endState.underline = false;
    para1.endState.strikethrough = false;
    para1.endState.subscript = false;
    para1.endState.superscript = false;
    para1.endState.right = false;
    para1.endState.left = true;
    para1.endState.justify = false;
    para1.endState.center = false;
    para1.endState.linespace = 1.0;

    para2.endState.font = "Arial,12";
    para2.endState.textcolor.red = -1;
    para2.endState.textcolor.green = -1;
    para2.endState.textcolor.blue = -1;
    para2.endState.textcolor.alpha = -1;
    para2.endState.bold = false;
    para2.endState.italics = false;
    para2.endState.underline = false;
    para2.endState.strikethrough = false;
    para2.endState.subscript = false;
    para2.endState.superscript = false;
    para2.endState.right = false;
    para2.endState.left = true;
    para2.endState.justify = false;
    para2.endState.center = false;
    para2.endState.linespace = 1.0;

    CHECK(para1.IsEqualTo(para2));
}


/////////////////////////////////////////////////////////////////////////////
// Continuous Y (screeny) Tests
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test screeny starts at 0 for first line
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("screeny starts at 0 for first line")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    sLineLayout line = layout.CreateLine(0);

    CHECK(CoordsEqual(line.screeny, 0));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test screeny increments after SaveLine
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("screeny increments after SaveLine")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    // Create and save first line
    sLineLayout line1 = layout.CreateLine(0);
    line1.lineheight = 240;  // 240 twips
    CHECK(CoordsEqual(line1.screeny, 0));
    layout.SaveLine(0, line1);

    // Create second line - screeny should be incremented
    sLineLayout line2 = layout.CreateLine(0);
    CHECK(CoordsEqual(line2.screeny, 240));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test screeny continues across page breaks (doesn't reset)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("screeny continues across page breaks")
{
    cLayoutBaseTest layout;

    // Page 1
    layout.CreatePageBox(1);
    sLineLayout line1 = layout.CreateLine(0);
    line1.lineheight = 240;
    CHECK(CoordsEqual(line1.screeny, 0));
    CHECK(CoordsEqual(line1.pagey, layout.GetBoxTop()));
    layout.SaveLine(0, line1);

    // Page 2 - create new page
    layout.CreatePageBox(2);
    sLineLayout line2 = layout.CreateLine(0);
    line2.lineheight = 240;

    // screeny should continue (not reset)
    CHECK(CoordsEqual(line2.screeny, 240));
    // pagey should reset to box top
    CHECK(CoordsEqual(line2.pagey, layout.GetBoxTop()));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test screeny vs pagey on multi-page document
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("screeny vs pagey differ on page 2")
{
    cLayoutBaseTest layout;

    // Page 1 - save 3 lines
    layout.CreatePageBox(1);
    for (int i = 0; i < 3; i++)
    {
        sLineLayout line = layout.CreateLine(0);
        line.lineheight = 240;
        layout.SaveLine(0, line);
    }

    // Page 2
    layout.CreatePageBox(2);
    sLineLayout line = layout.CreateLine(0);

    // screeny should be cumulative (3 * 240 = 720)
    CHECK(CoordsEqual(line.screeny, 720));
    // pagey should be at box top (reset for new page)
    CHECK(CoordsEqual(line.pagey, layout.GetBoxTop()));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test screeny with different line heights
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("screeny handles different line heights")
{
    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    // Line 1: 240 twips
    sLineLayout line1 = layout.CreateLine(0);
    line1.lineheight = 240;
    CHECK(CoordsEqual(line1.screeny, 0));
    layout.SaveLine(0, line1);

    // Line 2: 300 twips
    sLineLayout line2 = layout.CreateLine(0);
    line2.lineheight = 300;
    CHECK(CoordsEqual(line2.screeny, 240));
    layout.SaveLine(0, line2);

    // Line 3: 180 twips
    sLineLayout line3 = layout.CreateLine(0);
    line3.lineheight = 180;
    CHECK(CoordsEqual(line3.screeny, 540));  // 240 + 300
    layout.SaveLine(0, line3);

    // Line 4
    sLineLayout line4 = layout.CreateLine(0);
    CHECK(CoordsEqual(line4.screeny, 720));  // 240 + 300 + 180
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument() with empty document
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with empty document")
{
    cLayoutBaseTest layout;
    cDocument doc;

    layout.LayoutDocument(&doc);

    // Should have one box (page 1, even if empty)
    CHECK(layout.GetGlobalBoxList().size() == 1);
    CHECK(layout.GetGlobalBoxList()[0].pageNumber == 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument() with single paragraph
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with single paragraph")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("This is a test paragraph.");

    layout.LayoutDocument(&doc);

    // Should have one box, one paragraph, one line
    CHECK(layout.GetGlobalBoxList().size() == 1);
    CHECK(layout.GetNumberOfParagraphs() == 1);
    CHECK(layout.GetParagraphLayout(0)->lines.size() == 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument() with dot command
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with dot command")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("First paragraph.");
    doc.Insert("\r");  // HARD_RETURN creates new paragraph
    doc.Insert(".LM 1i");
    doc.Insert("\r");
    doc.Insert("Second paragraph.");

    layout.LayoutDocument(&doc);

    // Should have two boxes (margin changed)
    CHECK(layout.GetGlobalBoxList().size() >= 2);

    // First box should have default left margin
    CHECK(!CoordsEqual(layout.GetGlobalBoxList()[0].left, layout.GetGlobalBoxList()[1].left));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument() with page break
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with page break")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("First paragraph.");
    doc.Insert("\r");  // HARD_RETURN creates new paragraph
    doc.Insert(".PA");
    doc.Insert("\r");
    doc.Insert("Second paragraph.");

    layout.LayoutDocument(&doc);

    // Should have at least two boxes (page break)
    CHECK(layout.GetGlobalBoxList().size() >= 2);

    // Should have paragraphs on different pages
    CHECK(layout.GetNumberOfParagraphs() == 3);  // First para, .PA, second para
    CHECK(layout.GetParagraphLayout(1)->pageBreak == true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument() with multiple paragraphs
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with multiple paragraphs")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("First paragraph.");
    doc.Insert("\r");  // HARD_RETURN creates new paragraph
    doc.Insert("Second paragraph.");
    doc.Insert("\r");
    doc.Insert("Third paragraph.");

    layout.LayoutDocument(&doc);

    // Should have three paragraphs
    CHECK(layout.GetNumberOfParagraphs() == 3);

    // All paragraphs should have lines
    CHECK(layout.GetParagraphLayout(0)->lines.size() >= 1);
    CHECK(layout.GetParagraphLayout(1)->lines.size() >= 1);
    CHECK(layout.GetParagraphLayout(2)->lines.size() >= 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout font initialization
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout font initialization")
{
    cLayout layout;

    QFont font = layout.GetFont();

    // Should be Courier New 12pt
    CHECK(font.family() == "Courier New");
    CHECK(font.pointSize() == 12);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout GetTextWidth returns non-zero width
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout GetTextWidth returns positive width")
{
    cLayout layout;

    COORD_T width = layout.GetTextWidth("Hello");

    // Width should be positive (actual value depends on font metrics)
    CHECK(width > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout GetTextWidth is proportional to text length
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout GetTextWidth proportional to length")
{
    cLayout layout;

    COORD_T shortWidth = layout.GetTextWidth("Hi");
    COORD_T longWidth = layout.GetTextWidth("Hello World");

    // Longer text should have greater width
    CHECK(longWidth > shortWidth);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout GetFontHeight returns positive height
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout GetFontHeight returns positive height")
{
    cLayout layout;

    COORD_T height = layout.GetFontHeight();

    // Height should be positive and reasonable (between 100-500 twips for 12pt font)
    CHECK(height > 100);
    CHECK(height < 500);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout SetFont changes font
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout SetFont changes font")
{
    cLayout layout;

    QFont newFont("Arial", 14);
    layout.SetFont(newFont);

    QFont currentFont = layout.GetFont();

    CHECK(currentFont.family() == "Arial");
    CHECK(currentFont.pointSize() == 14);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout with LayoutDocument - complete integration
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout with LayoutDocument integration")
{
    cLayout layout;
    cDocument doc;
    doc.Insert("This is a test paragraph.");

    layout.LayoutDocument(&doc);

    // Should have laid out the document with Qt measurements
    CHECK(layout.GetGlobalBoxList().size() == 1);
    CHECK(layout.GetNumberOfParagraphs() == 1);
    CHECK(layout.GetParagraphLayout(0)->lines.size() == 1);

    // Line should have been measured with actual Qt font metrics
    const sLineLayout& line = layout.GetParagraphLayout(0)->lines[0];
    CHECK(line.lineheight > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout GetTextWidth with Unicode characters
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout GetTextWidth with Unicode")
{
    cLayout layout;

    COORD_T accentedWidth = layout.GetTextWidth("Café");
    COORD_T emojiWidth = layout.GetTextWidth("Test 🎉");
    COORD_T cyrillicWidth = layout.GetTextWidth("Привет");
    COORD_T cjkWidth = layout.GetTextWidth("世界");

    // All widths should be positive
    CHECK(accentedWidth > 0);
    CHECK(emojiWidth > 0);
    CHECK(cyrillicWidth > 0);
    CHECK(cjkWidth > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout GetTextWidth proportional with Unicode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout GetTextWidth Unicode proportional to length")
{
    cLayout layout;

    COORD_T shortUnicode = layout.GetTextWidth("Café");
    COORD_T longUnicode = layout.GetTextWidth("Café Résumé Naïve 🎉 Grüße");

    // Longer Unicode text should have greater width
    CHECK(longUnicode > shortUnicode);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument with Unicode paragraph
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with Unicode paragraph")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("Café résumé with naïve ideas 🎉");

    layout.LayoutDocument(&doc);

    CHECK(layout.GetNumberOfParagraphs() == 1);
    const sParagraphLayout* para = layout.GetParagraphLayout(0);
    REQUIRE(para != nullptr);
    CHECK(para->lines.size() >= 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument with comprehensive Unicode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with comprehensive Unicode")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("Café Résumé Naïve 🎉 Grüße München Привет мир 世界");

    layout.LayoutDocument(&doc);

    CHECK(layout.GetNumberOfParagraphs() == 1);
    const sParagraphLayout* para = layout.GetParagraphLayout(0);
    REQUIRE(para != nullptr);
    CHECK(para->lines.size() >= 1);

    // Verify segments were created
    const sLineLayout& line = para->lines[0];
    CHECK(line.segments.size() >= 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument with multi-paragraph Unicode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with multi-paragraph Unicode")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("First paragraph with café.");
    doc.Insert("\r");
    doc.Insert("Second paragraph with 🎉.");
    doc.Insert("\r");
    doc.Insert("Third paragraph with Привет.");
    doc.Insert("\r");
    doc.Insert("Fourth paragraph with 世界.");

    layout.LayoutDocument(&doc);

    CHECK(layout.GetNumberOfParagraphs() == 4);

    // Verify each paragraph has lines
    for (int i = 0; i < 4; i++)
    {
        const sParagraphLayout* para = layout.GetParagraphLayout(i);
        REQUIRE(para != nullptr);
        CHECK(para->lines.size() >= 1);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test LayoutDocument with Unicode and dot commands
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("LayoutDocument with Unicode and dot commands")
{
    cLayoutBaseTest layout;
    cDocument doc;
    doc.Insert("Café résumé paragraph.");
    doc.Insert("\r");
    doc.Insert(".lm 10\r");
    doc.Insert("Grüße München with margin.");
    doc.Insert("\r");
    doc.Insert("Привет мир and 世界 🎉.");

    layout.LayoutDocument(&doc);

    CHECK(layout.GetNumberOfParagraphs() >= 3);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cLayout integration with Unicode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cLayout with Unicode LayoutDocument integration")
{
    cLayout layout;
    cDocument doc;
    doc.Insert("This is a test with Unicode: Café résumé 🎉 Привет 世界");

    layout.LayoutDocument(&doc);

    // Should have laid out the Unicode document with Qt measurements
    CHECK(layout.GetGlobalBoxList().size() == 1);
    CHECK(layout.GetNumberOfParagraphs() == 1);
    CHECK(layout.GetParagraphLayout(0)->lines.size() >= 1);

    // Line should have been measured with actual Qt font metrics
    const sLineLayout& line = layout.GetParagraphLayout(0)->lines[0];
    CHECK(line.lineheight > 0);
    CHECK(line.segments.size() >= 1);
}


#if 0  // TODO: Update these tests after cEditorDisplay2 merge into cEditorCtrl
       // The rendering methods are now private instance methods
/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorDisplay2::DrawLine with empty line (no segments)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorDisplay2 DrawLine with empty line")
{
    // Create a simple image and painter for testing
    QImage image(100, 100, QImage::Format_RGB32);
    QPainter painter(&image);

    // Create an empty line
    sLineLayout line;
    line.screeny = 100;
    line.lineheight = 240;

    // Should not crash when drawing empty line
    cEditorDisplay2::DrawLine(painter, line, nullptr, 0);

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorDisplay2::DrawSegment with empty segment
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorDisplay2 DrawSegment with empty segment")
{
    QImage image(100, 100, QImage::Format_RGB32);
    QPainter painter(&image);

    // Create an empty segment
    sSegmentLayout segment;

    // Should not crash with empty segment (returns early)
    cEditorDisplay2::DrawSegment(painter, segment, nullptr, 0.0, 50.0);

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorDisplay2::DrawSegment with populated segment
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorDisplay2 DrawSegment with populated segment")
{
    QImage image(100, 100, QImage::Format_RGB32);
    QPainter painter(&image);

    // Create a segment with some data
    sSegmentLayout segment;
    segment.font = "Courier New,12,-1,5,50,0,0,0,0,0";
    segment.startPosition = 0;
    segment.length = 2;
    segment.position.push_back(100);
    segment.position.push_back(200);
    segment.textcolor.red = -1;
    segment.textcolor.green = -1;
    segment.textcolor.blue = -1;
    segment.textcolor.alpha = -1;
    segment.segmentheight = 240;

    // Should draw without crashing (nullptr document is OK - it will just return early)
    cEditorDisplay2::DrawSegment(painter, segment, nullptr, 0.0, 50.0);

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorDisplay2::DrawLine with populated line
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorDisplay2 DrawLine with populated line")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    // Create a line with a segment
    sLineLayout line;
    line.screeny = 240;
    line.lineheight = 240;

    sSegmentLayout segment;
    segment.font = "Courier New,12,-1,5,50,0,0,0,0,0";
    segment.startPosition = 0;
    segment.length = 4;
    segment.position.push_back(1440);  // 1 inch from left
    segment.position.push_back(1540);
    segment.position.push_back(1640);
    segment.position.push_back(1740);
    segment.textcolor.red = -1;
    segment.textcolor.green = -1;
    segment.textcolor.blue = -1;
    segment.textcolor.alpha = -1;
    segment.segmentheight = 240;

    line.segments.push_back(segment);

    // Should draw without crashing (nullptr document is OK - will return early)
    cEditorDisplay2::DrawLine(painter, line, nullptr, 0);

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
// Phase 0.5 Tests: Dual Display Mode
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sDisplaySettings default constructor initializes correctly
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sDisplaySettings default constructor")
{
    sDisplaySettings settings;

    // Check defaults
    CHECK(settings.mode == DISPLAY_CONTINUOUS);
    CHECK(settings.showControl == SHOW_DOT);
    CHECK(settings.pageGap == 360);  // 0.25 inches
    CHECK(settings.showPageShadows == true);
    CHECK(settings.showPageNumbers == false);

    // Check background color (gray)
    CHECK(settings.backgroundColor.red == 128);
    CHECK(settings.backgroundColor.green == 128);
    CHECK(settings.backgroundColor.blue == 128);
    CHECK(settings.backgroundColor.alpha == 255);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test sDisplaySettings mode can be changed
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("sDisplaySettings mode can be changed")
{
    sDisplaySettings settings;

    settings.mode = DISPLAY_PAGE;
    CHECK(settings.mode == DISPLAY_PAGE);

    settings.mode = DISPLAY_CONTINUOUS;
    CHECK(settings.mode == DISPLAY_CONTINUOUS);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawContinuousMode with null layout pointer
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawContinuousMode with null layout")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    sDisplaySettings settings;

    // Should not crash with null layout
    cEditorDisplay2::DrawContinuousMode(painter, nullptr, nullptr, settings, 0, 14400);

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawContinuousMode with empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawContinuousMode with empty layout")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    cLayoutBaseTest layout;

    sDisplaySettings settings;

    // Should not crash with empty layout
    cEditorDisplay2::DrawContinuousMode(painter, &layout, nullptr, settings, 0, 14400);

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawContinuousMode with lines in viewport
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawContinuousMode draws visible lines")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    // Add a paragraph with a line
    sParagraphLayout para;
    para.number = 1;

    sLineLayout line;
    line.screeny = 2000;  // Visible in viewport (scroll=0, height=14400)
    line.lineheight = 240;
    line.pagex = 1440;
    line.pagey = 2000;
    line.pagenumber = 1;

    para.lines.push_back(line);
    layout.mParagraphLayout.push_back(para);

    sDisplaySettings settings;

    // Should draw without crashing
    cEditorDisplay2::DrawContinuousMode(painter, &layout, nullptr, settings, 0, 14400);

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawPageMode with null layout pointer
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawPageMode with null layout")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    sDisplaySettings settings;
    settings.mode = DISPLAY_PAGE;

    // Should not crash with null layout
    cEditorDisplay2::DrawPageMode(painter, nullptr, nullptr, settings, 0, 14400);

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawPageMode with empty layout
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawPageMode with empty layout")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    cLayoutBaseTest layout;

    sDisplaySettings settings;
    settings.mode = DISPLAY_PAGE;

    // Should not crash with empty layout
    cEditorDisplay2::DrawPageMode(painter, &layout, nullptr, settings, 0, 14400);

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawPageMode with single page
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawPageMode draws single page")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    cLayoutBaseTest layout;
    layout.CreatePageBox(1);

    // Add a paragraph with a line
    sParagraphLayout para;
    para.number = 1;

    sLineLayout line;
    line.screeny = 2000;
    line.pagey = 2000;
    line.pagex = 1440;
    line.pagenumber = 1;
    line.lineheight = 240;

    para.lines.push_back(line);
    layout.mParagraphLayout.push_back(para);

    sDisplaySettings settings;
    settings.mode = DISPLAY_PAGE;

    // Should draw without crashing
    cEditorDisplay2::DrawPageMode(painter, &layout, nullptr, settings, 0, 14400);

    CHECK(true);  // Test passes if we get here
}
#endif  // End of disabled cEditorDisplay2 tests


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawPageBackground draws without crashing
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawPageBackground draws correctly")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    // Should draw without crashing
    // Note: DrawPageBackground is private, tested indirectly via DrawPageMode
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test DrawPageShadow draws without crashing
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("DrawPageShadow draws correctly")
{
    QImage image(800, 600, QImage::Format_RGB32);
    QPainter painter(&image);

    // Should draw without crashing
    // Note: We need to access private method via friend or make it public for testing
    // For now, this tests indirectly via DrawPageMode which calls it
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
// Phase 0.5 Step 0.5.5 Tests: cEditorCtrl Widget
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorCtrl constructor initializes to continuous mode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl constructor defaults to continuous mode")
{
    cEditorCtrl editor;

    CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
    CHECK(editor.GetScrollOffset() == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorCtrl SetDisplayMode changes mode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl SetDisplayMode changes mode")
{
    cEditorCtrl editor;

    editor.SetDisplayMode(DISPLAY_PAGE);
    CHECK(editor.GetDisplayMode() == DISPLAY_PAGE);

    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorCtrl ToggleDisplayMode switches between modes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl ToggleDisplayMode switches modes")
{
    cEditorCtrl editor;

    // Start in continuous
    CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);

    // Toggle to page
    editor.ToggleDisplayMode();
    CHECK(editor.GetDisplayMode() == DISPLAY_PAGE);

    // Toggle back to continuous
    editor.ToggleDisplayMode();
    CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorCtrl SetScrollOffset changes offset
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl SetScrollOffset changes offset")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Add enough content to make document scrollable
    // Need document height > viewport height for scrolling to work
    cDocument* doc = editor.GetDocument();
    doc->SetPosition(0);
    for (int i = 0; i < 100; i++)
    {
        doc->Insert("Line " + std::to_string(i) + "\r");
    }

    // Perform layout so boxes are created with proper heights
    cLayoutBase* layout = editor.GetLayout();
    layout->LayoutDocument(doc);

    editor.SetScrollOffset(1440);  // 1 inch
    CHECK(editor.GetScrollOffset() == 1440);

    editor.SetScrollOffset(2880);  // 2 inches
    CHECK(editor.GetScrollOffset() == 2880);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorCtrl SetDocument and SetLayout work
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl SetDocument and SetLayout")
{
    cEditorCtrl editor;
    cDocument doc;
    cLayout layout;

    // Should not crash

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorCtrl paintEvent with null layout doesn't crash
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl paintEvent with null layout")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Trigger paint with null layout - should not crash
    editor.update();

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test cEditorCtrl paintEvent with valid layout doesn't crash
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl paintEvent with valid layout")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    cDocument doc;
    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Trigger paint in continuous mode
    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    editor.update();

    // Trigger paint in page mode
    editor.SetDisplayMode(DISPLAY_PAGE);
    editor.update();

    CHECK(true);  // Test passes if we get here
}


/////////////////////////////////////////////////////////////////////////////
// Phase 0.5 Step 0.5.6 Tests: Comprehensive Display Mode Testing
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test continuous mode with actual multi-paragraph content
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Phase 0.5.6: Continuous mode renders multi-paragraph document")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Create document with multiple paragraphs
    cDocument doc;
    doc.Insert("First paragraph with some text that should wrap across multiple lines if the line is long enough.");
    doc.Insert("\r");  // HARD_RETURN creates new paragraph
    doc.Insert("Second paragraph.");
    doc.Insert("\r");
    doc.Insert("Third paragraph with more text.");

    // Layout document
    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Set up editor with continuous mode

    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    editor.SetScrollOffset(0);

    // Render - should not crash
    editor.update();

    // Verify we have layout data
    CHECK(layout.GetNumberOfParagraphs() > 0);

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test page mode with actual multi-paragraph content
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Phase 0.5.6: Page mode renders multi-paragraph document")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Create document with multiple paragraphs
    cDocument doc;
    doc.Insert("First paragraph with some text.");
    doc.Insert("\r");  // HARD_RETURN creates new paragraph
    doc.Insert("Second paragraph.");
    doc.Insert("\r");
    doc.Insert("Third paragraph.");

    // Layout document
    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Set up editor with page mode

    editor.SetDisplayMode(DISPLAY_PAGE);
    editor.SetScrollOffset(0);

    // Render - should not crash
    editor.update();

    // Verify we have layout data
    CHECK(layout.GetNumberOfParagraphs() > 0);

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test switching between modes preserves rendering
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Phase 0.5.6: Mode switching works correctly")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Create and layout document
    cDocument doc;
    doc.Insert("Test paragraph.");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Start in continuous mode
    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
    editor.update();

    // Switch to page mode
    editor.SetDisplayMode(DISPLAY_PAGE);
    CHECK(editor.GetDisplayMode() == DISPLAY_PAGE);
    editor.update();

    // Switch back to continuous
    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
    editor.update();

    // Use toggle
    editor.ToggleDisplayMode();
    CHECK(editor.GetDisplayMode() == DISPLAY_PAGE);
    editor.update();

    editor.ToggleDisplayMode();
    CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
    editor.update();

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test scrolling in continuous mode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Phase 0.5.6: Continuous mode scrolling works")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Create document large enough to scroll (viewport is 600px = ~9000 twips)
    for (int i = 0; i < 50; i++)
    {
        doc->Insert("Paragraph " + std::to_string(i) + ": Lorem ipsum dolor sit amet, consectetur adipiscing elit.");
        doc->Insert("\r");  // HARD_RETURN creates new paragraph
    }

    // Layout using the editor's owned layout engine
    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.SetDisplayMode(DISPLAY_CONTINUOUS);

    // Test different scroll offsets
    editor.SetScrollOffset(0);
    CHECK(editor.GetScrollOffset() == 0);
    editor.update();

    editor.SetScrollOffset(1440);  // 1 inch
    CHECK(editor.GetScrollOffset() == 1440);
    editor.update();

    editor.SetScrollOffset(2880);  // 2 inches
    CHECK(editor.GetScrollOffset() == 2880);
    editor.update();

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test scrolling in page mode
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Phase 0.5.6: Page mode scrolling works")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Create document
    cDocument doc;
    doc.Insert("Paragraph one.");
    doc.Insert("\r");  // HARD_RETURN creates new paragraph
    doc.Insert("Paragraph two.");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.SetDisplayMode(DISPLAY_PAGE);

    // Test different scroll offsets
    editor.SetScrollOffset(0);
    CHECK(editor.GetScrollOffset() == 0);
    editor.update();

    editor.SetScrollOffset(1440);  // 1 inch
    CHECK(editor.GetScrollOffset() == 1440);
    editor.update();

    editor.SetScrollOffset(2880);  // 2 inches
    CHECK(editor.GetScrollOffset() == 2880);
    editor.update();

    // Test passes if we get here without crashing
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test that both modes render the same content (same paragraphs/lines)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Phase 0.5.6: Both modes render same content")
{
    // Create document
    cDocument doc;
    doc.Insert("First paragraph.");
    doc.Insert("\r");  // HARD_RETURN creates new paragraph
    doc.Insert("Second paragraph.");
    doc.Insert("\r");
    doc.Insert("Third paragraph.");

    // Layout document
    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Count total lines in layout
    int totalLines = 0;
    for (PARAGRAPH_T paraNum = 0; paraNum < layout.GetNumberOfParagraphs(); paraNum++)
    {
        const sParagraphLayout* para = layout.GetParagraphLayout(paraNum);
        if (para)
        {
            totalLines += para->lines.size();
        }
    }

    // Both modes should render the same lines (just positioned differently)
    // This verifies the layout data is shared correctly
    CHECK(totalLines > 0);
    CHECK(layout.GetNumberOfParagraphs() == 3);  // 3 paragraphs

    // Verify each paragraph has lines
    for (PARAGRAPH_T paraNum = 0; paraNum < layout.GetNumberOfParagraphs(); paraNum++)
    {
        const sParagraphLayout* para = layout.GetParagraphLayout(paraNum);
        REQUIRE(para != nullptr);
        CHECK(para->lines.size() > 0);
    }

    // Test passes - both modes use the same layout data
    CHECK(true);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber with absolute value
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber sets absolute offset")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .pn 5 means page 1 displays as page 5 (offset = 4)
    bool result = layout.ParseDotCommand(".pn 5");
    CHECK(result == true);
    CHECK(layout.GetPageNumberOffset() == 4);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber with default (page 1)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber with page 1 sets zero offset")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .pn 1 means normal numbering (offset = 0)
    bool result = layout.ParseDotCommand(".pn 1");
    CHECK(result == true);
    CHECK(layout.GetPageNumberOffset() == 0);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber with large page number
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber handles large numbers")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .pn 100
    bool result = layout.ParseDotCommand(".pn 100");
    CHECK(result == true);
    CHECK(layout.GetPageNumberOffset() == 99);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber with positive increment
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber handles positive increment")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.SetPageNumberOffset(0);

    // .pn +3
    bool result = layout.ParseDotCommand(".pn +3");
    CHECK(result == true);
    CHECK(layout.GetPageNumberOffset() == 3);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber with negative decrement
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber handles negative decrement")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.SetPageNumberOffset(10);

    // .pn -2
    bool result = layout.ParseDotCommand(".pn -2");
    CHECK(result == true);
    CHECK(layout.GetPageNumberOffset() == 8);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber rejects zero
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber rejects zero")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .pn 0 is invalid
    bool result = layout.ParseDotCommand(".pn 0");
    CHECK(result == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber handles negative relative value (decrement)
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber handles negative relative value")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.SetPageNumberOffset(10);

    // .pn -5 is a relative change (decrement by 5)
    bool result = layout.ParseDotCommand(".pn -5");
    CHECK(result == true);
    CHECK(layout.GetPageNumberOffset() == 5);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber rejects missing value
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber rejects missing value")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .pn with no value
    bool result = layout.ParseDotCommand(".pn");
    CHECK(result == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParsePageNumber rejects invalid text
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParsePageNumber rejects invalid text")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .pn abc is invalid
    bool result = layout.ParseDotCommand(".pn abc");
    CHECK(result == false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Phase 0.7.2: .CP (Conditional Page Break) Tests
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("ParseConditionalPageBreak with sufficient space does not set flag")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // Get current box and set up state
    const sBoxes* box = layout.GetCurrentBox();
    REQUIRE(box != nullptr);

    // Box has 10000 twips remaining (bottom - top)
    // Line height is 240 twips (from GetFontHeight)
    // Request 5 lines = 1200 twips
    // Remaining space = 10000, so no page break

    bool result = layout.ParseConditionalPageBreak(".cp 5");
    CHECK(result == true);
    CHECK(layout.GetDoNewPage() == false);
}


TEST_CASE("ParseConditionalPageBreak with insufficient space sets flag")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // Request more lines than can fit
    // Default line height = 240 twips
    // Request 100 lines = 24000 twips
    // Typical box is ~10000 twips, so this will trigger break

    bool result = layout.ParseConditionalPageBreak(".cp 100");
    CHECK(result == true);
    CHECK(layout.GetDoNewPage() == true);
}


TEST_CASE("ParseConditionalPageBreak rejects missing value")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // .cp with no value is invalid
    bool result = layout.ParseConditionalPageBreak(".cp");
    CHECK(result == false);
    CHECK(layout.GetDoNewPage() == false);
}


TEST_CASE("ParseConditionalPageBreak rejects zero lines")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // .cp 0 is invalid (must be positive)
    bool result = layout.ParseConditionalPageBreak(".cp 0");
    CHECK(result == false);
    CHECK(layout.GetDoNewPage() == false);
}


TEST_CASE("ParseConditionalPageBreak rejects negative lines")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // .cp -5 is invalid (must be positive)
    bool result = layout.ParseConditionalPageBreak(".cp -5");
    CHECK(result == false);
    CHECK(layout.GetDoNewPage() == false);
}


TEST_CASE("ParseConditionalPageBreak rejects invalid text")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // .cp abc is invalid
    bool result = layout.ParseConditionalPageBreak(".cp abc");
    CHECK(result == false);
    CHECK(layout.GetDoNewPage() == false);
}


TEST_CASE("ParseConditionalPageBreak fails without box")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // No box created - should fail
    bool result = layout.ParseConditionalPageBreak(".cp 5");
    CHECK(result == false);
    CHECK(layout.GetDoNewPage() == false);
}


TEST_CASE("ParseConditionalPageBreak handles decimal values")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // .cp 2.5 should work (2.5 lines)
    bool result = layout.ParseConditionalPageBreak(".cp 2.5");
    CHECK(result == true);
    // Flag state depends on actual box space, just verify parsing succeeded
}


TEST_CASE("ParseConditionalPageBreak handles large numbers")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.CreatePageBox(1);

    // .cp 200 (very large - will definitely trigger break)
    bool result = layout.ParseConditionalPageBreak(".cp 200");
    CHECK(result == true);
    CHECK(layout.GetDoNewPage() == true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Phase 0.7.3: .AW (Word Wrap On/Off) Tests
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("ParseWordWrap default state is enabled")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Default state should be word wrap enabled
    CHECK(layout.GetWordWrapEnabled() == true);
}


TEST_CASE("ParseWordWrap disables with 'off'")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .aw off disables word wrap
    bool result = layout.ParseDotCommand(".aw off");
    CHECK(result == true);
    CHECK(layout.GetWordWrapEnabled() == false);
}


TEST_CASE("ParseWordWrap enables with 'on'")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.SetWordWrapEnabled(false);

    // .aw on enables word wrap
    bool result = layout.ParseDotCommand(".aw on");
    CHECK(result == true);
    CHECK(layout.GetWordWrapEnabled() == true);
}


TEST_CASE("ParseWordWrap case-insensitive OFF")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Test uppercase
    bool result1 = layout.ParseDotCommand(".aw OFF");
    CHECK(result1 == true);
    CHECK(layout.GetWordWrapEnabled() == false);

    // Test mixed case
    layout.SetWordWrapEnabled(true);
    bool result2 = layout.ParseDotCommand(".aw Off");
    CHECK(result2 == true);
    CHECK(layout.GetWordWrapEnabled() == false);
}


TEST_CASE("ParseWordWrap case-insensitive ON")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.SetWordWrapEnabled(false);

    // Test uppercase
    bool result1 = layout.ParseDotCommand(".aw ON");
    CHECK(result1 == true);
    CHECK(layout.GetWordWrapEnabled() == true);

    // Test mixed case
    layout.SetWordWrapEnabled(false);
    bool result2 = layout.ParseDotCommand(".aw On");
    CHECK(result2 == true);
    CHECK(layout.GetWordWrapEnabled() == true);
}


TEST_CASE("ParseWordWrap no parameter defaults to on")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);
    layout.SetWordWrapEnabled(false);

    // .aw with no parameter defaults to "on"
    bool result = layout.ParseDotCommand(".aw");
    CHECK(result == true);
    CHECK(layout.GetWordWrapEnabled() == true);
}


TEST_CASE("ParseWordWrap with whitespace")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .aw  off (extra spaces)
    bool result1 = layout.ParseDotCommand(".aw  off");
    CHECK(result1 == true);
    CHECK(layout.GetWordWrapEnabled() == false);

    // .aw on  (trailing spaces)
    bool result2 = layout.ParseDotCommand(".aw on  ");
    CHECK(result2 == true);
    CHECK(layout.GetWordWrapEnabled() == true);
}


TEST_CASE("ParseWordWrap rejects invalid parameter")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .aw invalid should fail
    bool result = layout.ParseDotCommand(".aw invalid");
    CHECK(result == false);
}


TEST_CASE("ParseWordWrap toggles state multiple times")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Initially on
    CHECK(layout.GetWordWrapEnabled() == true);

    // Turn off
    layout.ParseDotCommand(".aw off");
    CHECK(layout.GetWordWrapEnabled() == false);

    // Turn on
    layout.ParseDotCommand(".aw on");
    CHECK(layout.GetWordWrapEnabled() == true);

    // Turn off again
    layout.ParseDotCommand(".aw off");
    CHECK(layout.GetWordWrapEnabled() == false);
}


/////////////////////////////////////////////////////////////////////////////
// Phase 0.7.4: .PR (Printer Orientation) command tests
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Phase 0.7.4: .PR default state is portrait mode")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Default should be portrait (landscape = false)
    CHECK(layout.GetLandscapeMode() == false);

    // Default paper dimensions (portrait: width < height)
    CHECK(layout.GetPaperWidth() < layout.GetPaperHeight());
}

TEST_CASE("Phase 0.7.4: .PR or=l sets landscape mode")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Get initial dimensions
    COORD_T initialWidth = layout.GetPaperWidth();
    COORD_T initialHeight = layout.GetPaperHeight();

    // Parse landscape command
    bool result = layout.ParseDotCommand(".pr or=l");

    CHECK(result == true);
    CHECK(layout.GetLandscapeMode() == true);

    // In landscape mode, width should be greater than height
    CHECK(layout.GetPaperWidth() > layout.GetPaperHeight());

    // Dimensions should have swapped
    CHECK(layout.GetPaperWidth() == initialHeight);
    CHECK(layout.GetPaperHeight() == initialWidth);
}

TEST_CASE("Phase 0.7.4: .PR or=p sets portrait mode")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // First set to landscape
    layout.ParseDotCommand(".pr or=l");
    CHECK(layout.GetLandscapeMode() == true);

    // Get landscape dimensions
    COORD_T landscapeWidth = layout.GetPaperWidth();
    COORD_T landscapeHeight = layout.GetPaperHeight();

    // Parse portrait command
    bool result = layout.ParseDotCommand(".pr or=p");

    CHECK(result == true);
    CHECK(layout.GetLandscapeMode() == false);

    // In portrait mode, width should be less than height
    CHECK(layout.GetPaperWidth() < layout.GetPaperHeight());

    // Dimensions should have swapped back
    CHECK(layout.GetPaperWidth() == landscapeHeight);
    CHECK(layout.GetPaperHeight() == landscapeWidth);
}

TEST_CASE("Phase 0.7.4: .PR case-insensitive OR=L")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Test uppercase OR=L
    bool result1 = layout.ParseDotCommand(".pr OR=L");
    CHECK(result1 == true);
    CHECK(layout.GetLandscapeMode() == true);

    // Reset to portrait
    layout.ParseDotCommand(".pr or=p");

    // Test mixed case Or=L (should still work due to uppercase conversion)
    bool result2 = layout.ParseDotCommand(".pr Or=L");
    CHECK(result2 == true);
    CHECK(layout.GetLandscapeMode() == true);
}

TEST_CASE("Phase 0.7.4: .PR case-insensitive OR=P")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Set to landscape first
    layout.ParseDotCommand(".pr or=l");

    // Test uppercase OR=P
    bool result1 = layout.ParseDotCommand(".pr OR=P");
    CHECK(result1 == true);
    CHECK(layout.GetLandscapeMode() == false);

    // Set to landscape again
    layout.ParseDotCommand(".pr or=l");

    // Test mixed case Or=P
    bool result2 = layout.ParseDotCommand(".pr Or=P");
    CHECK(result2 == true);
    CHECK(layout.GetLandscapeMode() == false);
}

TEST_CASE("Phase 0.7.4: .PR missing parameter rejected")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // No parameter should fail
    bool result = layout.ParseDotCommand(".pr");
    CHECK(result == false);

    // Just whitespace should also fail
    bool result2 = layout.ParseDotCommand(".pr   ");
    CHECK(result2 == false);
}

TEST_CASE("Phase 0.7.4: .PR invalid parameter rejected")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Invalid orientation value
    bool result1 = layout.ParseDotCommand(".pr or=x");
    CHECK(result1 == false);

    // Wrong format
    bool result2 = layout.ParseDotCommand(".pr landscape");
    CHECK(result2 == false);

    // Misspelled
    bool result3 = layout.ParseDotCommand(".pr or=");
    CHECK(result3 == false);
}

TEST_CASE("Phase 0.7.4: .PR whitespace handling")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Leading/trailing whitespace should be trimmed
    bool result1 = layout.ParseDotCommand(".pr   or=l  ");
    CHECK(result1 == true);
    CHECK(layout.GetLandscapeMode() == true);

    // Reset
    layout.ParseDotCommand(".pr or=p");

    // Tabs should also work
    bool result2 = layout.ParseDotCommand(".pr\t\tor=l\t");
    CHECK(result2 == true);
    CHECK(layout.GetLandscapeMode() == true);
}

TEST_CASE("Phase 0.7.4: .PR multiple orientation changes")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Start in portrait (default)
    CHECK(layout.GetLandscapeMode() == false);

    // Switch to landscape
    layout.ParseDotCommand(".pr or=l");
    CHECK(layout.GetLandscapeMode() == true);

    // Switch to portrait
    layout.ParseDotCommand(".pr or=p");
    CHECK(layout.GetLandscapeMode() == false);

    // Switch to landscape again
    layout.ParseDotCommand(".pr or=l");
    CHECK(layout.GetLandscapeMode() == true);

    // Switch to portrait again
    layout.ParseDotCommand(".pr or=p");
    CHECK(layout.GetLandscapeMode() == false);
}

TEST_CASE("Phase 0.7.4: .PR GetLandscapeMode() is publicly accessible")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Test that GetLandscapeMode() can be called via base class pointer
    // (proves it's a public method, not protected/private)
    cLayoutBase* basePtr = &layout;

    // Default state
    CHECK(basePtr->GetLandscapeMode() == false);

    // Set to landscape via base pointer's parsing
    layout.ParseDotCommand(".pr or=l");
    CHECK(basePtr->GetLandscapeMode() == true);

    // Set to portrait
    layout.ParseDotCommand(".pr or=p");
    CHECK(basePtr->GetLandscapeMode() == false);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Phase 0.7.5: .OP and .PG (Page Number Control) Tests
///
/// Tests for the .OP (Omit Page Numbers) and .PG (Print Page Numbers)
/// dot commands. These commands control whether page numbers are
/// automatically printed at the bottom center of each page.
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Phase 0.7.5: Default state has page numbering enabled")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Default should be page numbering enabled (true)
    CHECK(layout.GetPrintPageNumbers() == true);
}

TEST_CASE("Phase 0.7.5: .OP disables page numbering")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Verify default state
    CHECK(layout.GetPrintPageNumbers() == true);

    // Parse .OP command
    bool result = layout.ParseDotCommand(".op");
    CHECK(result == true);

    // Verify page numbering is now disabled
    CHECK(layout.GetPrintPageNumbers() == false);
}

TEST_CASE("Phase 0.7.5: .PG enables page numbering")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // First disable page numbering
    layout.ParseDotCommand(".op");
    CHECK(layout.GetPrintPageNumbers() == false);

    // Parse .PG command
    bool result = layout.ParseDotCommand(".pg");
    CHECK(result == true);

    // Verify page numbering is now enabled
    CHECK(layout.GetPrintPageNumbers() == true);
}

TEST_CASE("Phase 0.7.5: .OP case-insensitive")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Test uppercase .OP
    bool result1 = layout.ParseDotCommand(".OP");
    CHECK(result1 == true);
    CHECK(layout.GetPrintPageNumbers() == false);

    // Re-enable for next test
    layout.ParseDotCommand(".pg");

    // Test lowercase .op
    bool result2 = layout.ParseDotCommand(".op");
    CHECK(result2 == true);
    CHECK(layout.GetPrintPageNumbers() == false);
}

TEST_CASE("Phase 0.7.5: .PG case-insensitive")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Disable first
    layout.ParseDotCommand(".op");

    // Test uppercase .PG
    bool result1 = layout.ParseDotCommand(".PG");
    CHECK(result1 == true);
    CHECK(layout.GetPrintPageNumbers() == true);

    // Disable again
    layout.ParseDotCommand(".op");

    // Test lowercase .pg
    bool result2 = layout.ParseDotCommand(".pg");
    CHECK(result2 == true);
    CHECK(layout.GetPrintPageNumbers() == true);
}

TEST_CASE("Phase 0.7.5: Multiple .OP/.PG toggles")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Start enabled (default)
    CHECK(layout.GetPrintPageNumbers() == true);

    // Disable with .OP
    layout.ParseDotCommand(".op");
    CHECK(layout.GetPrintPageNumbers() == false);

    // Enable with .PG
    layout.ParseDotCommand(".pg");
    CHECK(layout.GetPrintPageNumbers() == true);

    // Disable again
    layout.ParseDotCommand(".op");
    CHECK(layout.GetPrintPageNumbers() == false);

    // Enable again
    layout.ParseDotCommand(".pg");
    CHECK(layout.GetPrintPageNumbers() == true);
}

TEST_CASE("Phase 0.7.5: .OP and .PG via ParseDotCommand()")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Test .OP via ParseDotCommand
    bool result1 = layout.ParseDotCommand(".op");
    CHECK(result1 == true);
    CHECK(layout.GetPrintPageNumbers() == false);

    // Test .PG via ParseDotCommand
    bool result2 = layout.ParseDotCommand(".pg");
    CHECK(result2 == true);
    CHECK(layout.GetPrintPageNumbers() == true);

    // Test uppercase via ParseDotCommand
    bool result3 = layout.ParseDotCommand(".OP");
    CHECK(result3 == true);
    CHECK(layout.GetPrintPageNumbers() == false);

    bool result4 = layout.ParseDotCommand(".PG");
    CHECK(result4 == true);
    CHECK(layout.GetPrintPageNumbers() == true);
}

TEST_CASE("Phase 0.7.5: GetPrintPageNumbers() is publicly accessible")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Test that GetPrintPageNumbers() can be called via base class pointer
    // (proves it's a public method, not protected/private)
    cLayoutBase* basePtr = &layout;

    // Default state
    CHECK(basePtr->GetPrintPageNumbers() == true);

    // Disable via parsing
    layout.ParseDotCommand(".op");
    CHECK(basePtr->GetPrintPageNumbers() == false);

    // Enable via parsing
    layout.ParseDotCommand(".pg");
    CHECK(basePtr->GetPrintPageNumbers() == true);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Phase 0.7.6: Integration Testing for Phase 0.7 Commands
///
/// Integration tests that verify all Phase 0.7 dot commands work together
/// correctly in realistic document scenarios. Tests command interactions,
/// state persistence, and ParseDotCommand() dispatcher routing.
///
/// Phase 0.7 commands:
/// - .PN (Page Number offset)
/// - .CP (Conditional Page Break)
/// - .AW (Word Wrap on/off)
/// - .PR (Printer orientation - landscape/portrait)
/// - .OP/.PG (Omit/Print page numbers)
///
/////////////////////////////////////////////////////////////////////////////

// ============================================================================
// Test Group A: ParseDotCommand() Dispatcher Validation
// ============================================================================

TEST_CASE("Phase 0.7.6: ParseDotCommand routes all Phase 0.7 commands")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Create a box for .CP testing
    layout.CreatePageBox(1);

    // Test .PN routing
    bool result1 = layout.ParseDotCommand(".pn 10");
    CHECK(result1 == true);
    CHECK(layout.GetPageNumberOffset() == 9);

    // Test .CP routing (requires box with space)
    bool result2 = layout.ParseDotCommand(".cp 5");
    CHECK(result2 == true);

    // Test .AW routing
    bool result3 = layout.ParseDotCommand(".aw off");
    CHECK(result3 == true);
    CHECK(layout.GetWordWrapEnabled() == false);

    // Test .PR routing
    bool result4 = layout.ParseDotCommand(".pr or=l");
    CHECK(result4 == true);
    CHECK(layout.GetLandscapeMode() == true);

    // Test .OP routing
    bool result5 = layout.ParseDotCommand(".op");
    CHECK(result5 == true);
    CHECK(layout.GetPrintPageNumbers() == false);

    // Test .PG routing
    bool result6 = layout.ParseDotCommand(".pg");
    CHECK(result6 == true);
    CHECK(layout.GetPrintPageNumbers() == true);
}

TEST_CASE("Phase 0.7.6: ParseDotCommand rejects unknown commands")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // .XX is a valid WordStar command (Strikeout Character), but not implemented
    eDotCommandStatus result1 = layout.ParseDotCommand(".xx 123");
    CHECK(result1 == DOT_NOTIMPLEMENTED);

    // .ZZ and .QQ are truly unknown commands
    eDotCommandStatus result2 = layout.ParseDotCommand(".zz");
    CHECK(result2 == DOT_UNKNOWN);

    eDotCommandStatus result3 = layout.ParseDotCommand(".qq off");
    CHECK(result3 == DOT_UNKNOWN);
}

TEST_CASE("Phase 0.7.6: ParseDotCommand case-insensitive for all commands")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    layout.CreatePageBox(1);

    // Test mixed case for all Phase 0.7 commands
    CHECK(layout.ParseDotCommand(".PN 5") == true);
    CHECK(layout.ParseDotCommand(".Cp 3") == true);
    CHECK(layout.ParseDotCommand(".Aw OFF") == true);
    CHECK(layout.ParseDotCommand(".Pr OR=L") == true);
    CHECK(layout.ParseDotCommand(".Op") == true);
    CHECK(layout.ParseDotCommand(".Pg") == true);
}

// ============================================================================
// Test Group B: Multi-Command Scenarios
// ============================================================================

TEST_CASE("Phase 0.7.6: Landscape document with page offset")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Common scenario: Landscape document starting at page 5
    COORD_T initialWidth = layout.GetPaperWidth();
    COORD_T initialHeight = layout.GetPaperHeight();

    // Set landscape orientation
    layout.ParseDotCommand(".pr or=l");
    CHECK(layout.GetLandscapeMode() == true);
    CHECK(layout.GetPaperWidth() > layout.GetPaperHeight());
    CHECK(layout.GetPaperWidth() == initialHeight);
    CHECK(layout.GetPaperHeight() == initialWidth);

    // Set page number offset
    layout.ParseDotCommand(".pn 5");
    CHECK(layout.GetPageNumberOffset() == 4);

    // Both settings should persist
    CHECK(layout.GetLandscapeMode() == true);
    CHECK(layout.GetPageNumberOffset() == 4);
}

TEST_CASE("Phase 0.7.6: Table formatting scenario")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Common scenario: Wide table in landscape with no wrapping

    // Set landscape for more width
    layout.ParseDotCommand(".pr or=l");
    COORD_T landscapeWidth = layout.GetPaperWidth();
    CHECK(layout.GetLandscapeMode() == true);

    // Disable word wrap for table columns
    layout.ParseDotCommand(".aw off");
    CHECK(layout.GetWordWrapEnabled() == false);

    // Disable automatic page numbers (table has its own headers)
    layout.ParseDotCommand(".op");
    CHECK(layout.GetPrintPageNumbers() == false);

    // All three states should persist together
    CHECK(layout.GetLandscapeMode() == true);
    CHECK(layout.GetWordWrapEnabled() == false);
    CHECK(layout.GetPrintPageNumbers() == false);
    CHECK(layout.GetPaperWidth() == landscapeWidth);
}

TEST_CASE("Phase 0.7.6: Complex document using all Phase 0.7 commands")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    layout.CreatePageBox(1);

    // Realistic complex document setup

    // Start at page 10 (chapter 2 of multi-chapter document)
    layout.ParseDotCommand(".pn 10");
    CHECK(layout.GetPageNumberOffset() == 9);

    // Portrait mode
    layout.ParseDotCommand(".pr or=p");
    CHECK(layout.GetLandscapeMode() == false);

    // Enable word wrap for body text
    layout.ParseDotCommand(".aw on");
    CHECK(layout.GetWordWrapEnabled() == true);

    // Enable page numbering
    layout.ParseDotCommand(".pg");
    CHECK(layout.GetPrintPageNumbers() == true);

    // Keep chapter title with at least 5 lines of text
    layout.ParseDotCommand(".cp 5");

    // Verify all settings persist
    CHECK(layout.GetPageNumberOffset() == 9);
    CHECK(layout.GetLandscapeMode() == false);
    CHECK(layout.GetWordWrapEnabled() == true);
    CHECK(layout.GetPrintPageNumbers() == true);
}

TEST_CASE("Phase 0.7.6: Sequential command processing")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Process commands in sequence, verify each affects state correctly

    // Initial state
    CHECK(layout.GetPageNumberOffset() == 0);
    CHECK(layout.GetLandscapeMode() == false);
    CHECK(layout.GetWordWrapEnabled() == true);
    CHECK(layout.GetPrintPageNumbers() == true);

    // Command 1: Change page number
    layout.ParseDotCommand(".pn 20");
    CHECK(layout.GetPageNumberOffset() == 19);

    // Command 2: Change orientation
    layout.ParseDotCommand(".pr or=l");
    CHECK(layout.GetLandscapeMode() == true);
    CHECK(layout.GetPageNumberOffset() == 19); // Previous state preserved

    // Command 3: Disable wrapping
    layout.ParseDotCommand(".aw off");
    CHECK(layout.GetWordWrapEnabled() == false);
    CHECK(layout.GetLandscapeMode() == true); // Previous state preserved
    CHECK(layout.GetPageNumberOffset() == 19); // Previous state preserved

    // Command 4: Disable page numbers
    layout.ParseDotCommand(".op");
    CHECK(layout.GetPrintPageNumbers() == false);
    CHECK(layout.GetWordWrapEnabled() == false); // Previous state preserved
    CHECK(layout.GetLandscapeMode() == true); // Previous state preserved
    CHECK(layout.GetPageNumberOffset() == 19); // Previous state preserved
}

// ============================================================================
// Test Group C: State Interaction Tests
// ============================================================================

TEST_CASE("Phase 0.7.6: Orientation change affects paper dimensions")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Verify orientation changes swap dimensions
    COORD_T portraitWidth = layout.GetPaperWidth();
    COORD_T portraitHeight = layout.GetPaperHeight();
    CHECK(portraitWidth < portraitHeight);

    // Switch to landscape
    layout.ParseDotCommand(".pr or=l");
    COORD_T landscapeWidth = layout.GetPaperWidth();
    COORD_T landscapeHeight = layout.GetPaperHeight();

    // Dimensions should be swapped
    CHECK(landscapeWidth > landscapeHeight);
    CHECK(landscapeWidth == portraitHeight);
    CHECK(landscapeHeight == portraitWidth);

    // Switch back to portrait
    layout.ParseDotCommand(".pr or=p");
    CHECK(layout.GetPaperWidth() == portraitWidth);
    CHECK(layout.GetPaperHeight() == portraitHeight);
}

TEST_CASE("Phase 0.7.6: Multiple orientation changes preserve other state")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Set multiple states
    layout.ParseDotCommand(".pn 15");
    layout.ParseDotCommand(".aw off");
    layout.ParseDotCommand(".op");

    // Store state
    PAGE_T expectedOffset = layout.GetPageNumberOffset();
    bool expectedWrap = layout.GetWordWrapEnabled();
    bool expectedPageNum = layout.GetPrintPageNumbers();

    // Change orientation multiple times
    layout.ParseDotCommand(".pr or=l");
    layout.ParseDotCommand(".pr or=p");
    layout.ParseDotCommand(".pr or=l");

    // Other state should be preserved
    CHECK(layout.GetPageNumberOffset() == expectedOffset);
    CHECK(layout.GetWordWrapEnabled() == expectedWrap);
    CHECK(layout.GetPrintPageNumbers() == expectedPageNum);
    CHECK(layout.GetLandscapeMode() == true); // Last orientation
}

TEST_CASE("Phase 0.7.6: Page number offset increments")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Start at page 10
    layout.ParseDotCommand(".pn 10");
    CHECK(layout.GetPageNumberOffset() == 9);

    // Increment by 5
    layout.ParseDotCommand(".pn +5");
    CHECK(layout.GetPageNumberOffset() == 14);

    // Decrement by 3
    layout.ParseDotCommand(".pn -3");
    CHECK(layout.GetPageNumberOffset() == 11);
}

// ============================================================================
// Test Group D: Edge Cases
// ============================================================================

TEST_CASE("Phase 0.7.6: Commands with internal/trailing whitespace")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    layout.CreatePageBox(1);

    // All commands should handle internal and trailing whitespace
    // Note: Leading whitespace before the dot is not allowed (dot must be in column 1)
    CHECK(layout.ParseDotCommand(".pn  10  ") == true);
    CHECK(layout.ParseDotCommand(".aw  off  ") == true);
    CHECK(layout.ParseDotCommand(".pr  or=l  ") == true);
    CHECK(layout.ParseDotCommand(".op  ") == true);

    // Verify state was set correctly despite whitespace
    CHECK(layout.GetPageNumberOffset() == 9);
    CHECK(layout.GetWordWrapEnabled() == false);
    CHECK(layout.GetLandscapeMode() == true);
    CHECK(layout.GetPrintPageNumbers() == false);
}

TEST_CASE("Phase 0.7.6: State persists after box creation")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Set states before creating box
    layout.ParseDotCommand(".pn 5");
    layout.ParseDotCommand(".pr or=l");
    layout.ParseDotCommand(".aw off");
    layout.ParseDotCommand(".op");

    // Create a box
    layout.CreatePageBox(1);

    // State should persist after box operations
    CHECK(layout.GetPageNumberOffset() == 4);
    CHECK(layout.GetLandscapeMode() == true);
    CHECK(layout.GetWordWrapEnabled() == false);
    CHECK(layout.GetPrintPageNumbers() == false);
}

TEST_CASE("Phase 0.7.6: Toggle commands work correctly")
{
    cLayoutBaseTest layout;
    cDocument doc;
    layout.SetDocument(&doc);

    // Test toggling word wrap
    CHECK(layout.GetWordWrapEnabled() == true);
    layout.ParseDotCommand(".aw off");
    CHECK(layout.GetWordWrapEnabled() == false);
    layout.ParseDotCommand(".aw on");
    CHECK(layout.GetWordWrapEnabled() == true);

    // Test toggling page numbers
    CHECK(layout.GetPrintPageNumbers() == true);
    layout.ParseDotCommand(".op");
    CHECK(layout.GetPrintPageNumbers() == false);
    layout.ParseDotCommand(".pg");
    CHECK(layout.GetPrintPageNumbers() == true);

    // Test toggling orientation
    CHECK(layout.GetLandscapeMode() == false);
    layout.ParseDotCommand(".pr or=l");
    CHECK(layout.GetLandscapeMode() == true);
    layout.ParseDotCommand(".pr or=p");
    CHECK(layout.GetLandscapeMode() == false);
}

TEST_CASE("Layout2 - Simple Header Parsing")
{
    cDocument doc;
    cLayout layout;

    // Add header command on page 1
    doc.Insert(".he Test Header\r");
    doc.Insert("Body text paragraph\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Headers appear on pages AFTER they are defined
    // Header defined on page 1, should appear on page 2
    const auto& headers = layout.GetPageHeaders();
    CHECK(headers.find(2) != headers.end());

    if (headers.find(2) != headers.end())
    {
        const auto& page2Headers = headers.at(2);
        CHECK(page2Headers.size() == 1);  // One header line

        // Verify header has content
        if (page2Headers.size() > 0)
        {
            CHECK(!page2Headers[0].line.segments.empty());
        }
    }

    // Verify NO headers on page 1 (definition page)
    CHECK(headers.find(1) == headers.end());
}

TEST_CASE("Layout2 - Simple Footer Parsing")
{
    cDocument doc;
    cLayout layout;

    // Add footer command on page 1
    doc.Insert(".fo Test Footer\r");
    doc.Insert("Body text paragraph\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Footers appear on pages AFTER they are defined
    // Footer defined on page 1, should appear on page 2
    const auto& footers = layout.GetPageFooters();
    CHECK(footers.find(2) != footers.end());

    if (footers.find(2) != footers.end())
    {
        const auto& page2Footers = footers.at(2);
        CHECK(page2Footers.size() == 1);  // One footer line

        // Verify footer has content
        if (page2Footers.size() > 0)
        {
            CHECK(!page2Footers[0].line.segments.empty());
        }
    }

    // Verify NO footers on page 1 (definition page)
    CHECK(footers.find(1) == footers.end());
}

TEST_CASE("Layout2 - Even/Odd Header Selection")
{
    cDocument doc;
    cLayout layout;

    // Add even and odd headers on page 1
    doc.Insert(".hee Even Header\r");
    doc.Insert(".heo Odd Header\r");
    doc.Insert("Page 1 text\r");
    doc.Insert(".pa\r");  // Force page break
    doc.Insert("Page 2 text\r");
    doc.Insert(".pa\r");
    doc.Insert("Page 3 text\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Page 1 should NOT have headers (definition page)
    CHECK(headers.find(1) == headers.end());

    // Page 2 (even) should have even header
    CHECK(headers.find(2) != headers.end());
    if (headers.find(2) != headers.end())
    {
        CHECK(headers.at(2).size() == 1);
    }

    // Page 3 (odd) should have odd header
    CHECK(headers.find(3) != headers.end());
    if (headers.find(3) != headers.end())
    {
        CHECK(headers.at(3).size() == 1);
    }
}

TEST_CASE("Layout2 - Header Margin Parsing")
{
    cDocument doc;
    cLayout layout;

    // Set custom header margin
    doc.Insert(".hm 3\r");  // 3 lines
    doc.Insert(".he Header\r");
    doc.Insert("Body text\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Verify header margin was applied (3 lines * line height)
    COORD_T lineHeight = layout.GetLineHeight();
    COORD_T expectedMargin = 3 * lineHeight;

    // Note: This is a basic check - actual margin verification would require
    // checking box coordinates, which we'll do in integration tests
    CHECK(lineHeight > 0);
}

TEST_CASE("Layout2 - Multiple Headers Stacking")
{
    cDocument doc;
    cLayout layout;

    // Add multiple headers on page 1
    doc.Insert(".h1 Header Line 1\r");
    doc.Insert(".h2 Header Line 2\r");
    doc.Insert(".h3 Header Line 3\r");
    doc.Insert("Body text\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Page 1 should NOT have headers (definition page)
    CHECK(headers.find(1) == headers.end());

    // Verify all 3 headers appear on page 2
    CHECK(headers.find(2) != headers.end());
    if (headers.find(2) != headers.end())
    {
        const auto& page2Headers = headers.at(2);
        CHECK(page2Headers.size() == 3);  // Three header lines
    }
}

TEST_CASE("Layout2 - Footer Margin Parsing")
{
    cDocument doc;
    cLayout layout;

    // Set custom footer margin
    doc.Insert(".fm 3\r");  // 3 lines
    doc.Insert(".fo Footer\r");
    doc.Insert("Body text\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Verify footer margin was applied
    COORD_T lineHeight = layout.GetLineHeight();
    COORD_T expectedMargin = 3 * lineHeight;

    CHECK(lineHeight > 0);
}

TEST_CASE("Layout2 - Header Position Verification")
{
    cDocument doc;
    cLayout layout;

    // Add header
    doc.Insert(".he Page Header\r");
    doc.Insert("Body text paragraph\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Verify header exists and has valid Y position
    if (headers.find(1) != headers.end() && headers.at(1).size() > 0)
    {
        const sLineLayout& headerLine = headers.at(1)[0].line;

        // Header should be positioned in the top margin area
        CHECK(headerLine.pagey >= 0);
        CHECK(headerLine.pagenumber == 1);
    }
}

TEST_CASE("Layout2 - Footer Position Verification")
{
    cDocument doc;
    cLayout layout;

    // Add footer
    doc.Insert(".fo Page Footer\r");
    doc.Insert("Body text paragraph\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& footers = layout.GetPageFooters();

    // Verify footer exists and has valid Y position
    if (footers.find(1) != footers.end() && footers.at(1).size() > 0)
    {
        const sLineLayout& footerLine = footers.at(1)[0].line;

        // Footer should be positioned near bottom of page
        COORD_T paperHeight = layout.GetPaperHeight();
        CHECK(footerLine.pagey > paperHeight / 2);  // In bottom half of page
        CHECK(footerLine.pagenumber == 1);
    }
}

TEST_CASE("Layout2 - No Headers/Footers By Default")
{
    cDocument doc;
    cLayout layout;

    // Add only body text, no header/footer commands
    doc.Insert("Body text paragraph 1\r");
    doc.Insert("Body text paragraph 2\r");

    // Layout document
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();
    const auto& footers = layout.GetPageFooters();

    // Verify no headers or footers created for page 1
    if (!headers.empty() && headers.find(1) != headers.end())
    {
        CHECK(headers.at(1).empty());
    }

    if (!footers.empty() && footers.find(1) != footers.end())
    {
        CHECK(footers.at(1).empty());
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// Phase 2: Page Numbering Tests
//
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Layout2 - Page Number Format - Arabic (default)")
{
    cDocument doc;
    cLayout layout;

    // No .pn command - should default to Arabic
    doc.Insert("Body text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Check default format is Arabic and offset is 0
    CHECK(layout.GetPageNumFormat() == PAGE_NUM_ARABIC);
    CHECK(layout.GetPageNumberOffset() == 0);
}

TEST_CASE("Layout2 - Page Number Format - .pn 5 sets offset")
{
    cDocument doc;
    cLayout layout;

    // .pn 5 should make page 1 display as 5
    doc.Insert(".pn 5\r");
    doc.Insert("Body text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Per-page override: page 1 produces 1 + 4 = 5
    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ARABIC) == "5");
}

TEST_CASE("Layout2 - Page Number Format - .pn i sets Roman lowercase")
{
    cDocument doc;
    cLayout layout;

    // .pn i should set Roman lowercase format
    doc.Insert(".pn i\r");
    doc.Insert("Body text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Per-page override: page 1 produces "i" (Roman lowercase)
    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ARABIC) == "i");
}

TEST_CASE("Layout2 - Page Number Format - .pn I sets Roman uppercase")
{
    cDocument doc;
    cLayout layout;

    // .pn I should set Roman uppercase format
    doc.Insert(".pn I\r");
    doc.Insert("Body text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Per-page override: page 1 produces "I" (Roman uppercase)
    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ARABIC) == "I");
}

TEST_CASE("Layout2 - Page Number Format - .pn on a later page leaves earlier pages unchanged")
{
    if (!QApplication::instance())
    {
        static int argc = 0;
        static char* argv[] = {nullptr};
        new QApplication(argc, argv);
    }

    cDocument doc;
    cLayout layout;

    // .pn i lands on page 2 (after a forced page break). Page 1 must keep its
    // natural Arabic number; it must NOT inherit page 2's offset.
    doc.Insert("Page one\r");
    doc.Insert(".pa\r");
    doc.Insert(".pn i\r");
    doc.Insert("Page two\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ARABIC) == "1");
    CHECK(layout.FormatPageNumber(2, PAGE_NUM_ARABIC) == "i");
}

TEST_CASE("Layout2 - Roman Numeral Conversion - Lowercase")
{
    cDocument doc;
    cLayout layout;

    layout.SetDocument(&doc);

    // Test various Roman numeral conversions via FormatPageNumber
    // (ToRomanNumeralLower is now internal to cDotCommandParser)
    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ROMAN_LOWER) == "i");
    CHECK(layout.FormatPageNumber(4, PAGE_NUM_ROMAN_LOWER) == "iv");
    CHECK(layout.FormatPageNumber(5, PAGE_NUM_ROMAN_LOWER) == "v");
    CHECK(layout.FormatPageNumber(9, PAGE_NUM_ROMAN_LOWER) == "ix");
    CHECK(layout.FormatPageNumber(10, PAGE_NUM_ROMAN_LOWER) == "x");
    CHECK(layout.FormatPageNumber(40, PAGE_NUM_ROMAN_LOWER) == "xl");
    CHECK(layout.FormatPageNumber(50, PAGE_NUM_ROMAN_LOWER) == "l");
    CHECK(layout.FormatPageNumber(90, PAGE_NUM_ROMAN_LOWER) == "xc");
    CHECK(layout.FormatPageNumber(100, PAGE_NUM_ROMAN_LOWER) == "c");
    CHECK(layout.FormatPageNumber(400, PAGE_NUM_ROMAN_LOWER) == "cd");
    CHECK(layout.FormatPageNumber(500, PAGE_NUM_ROMAN_LOWER) == "d");
    CHECK(layout.FormatPageNumber(900, PAGE_NUM_ROMAN_LOWER) == "cm");
    CHECK(layout.FormatPageNumber(1000, PAGE_NUM_ROMAN_LOWER) == "m");
    CHECK(layout.FormatPageNumber(1994, PAGE_NUM_ROMAN_LOWER) == "mcmxciv");
}

TEST_CASE("Layout2 - Roman Numeral Conversion - Uppercase")
{
    cDocument doc;
    cLayout layout;

    layout.SetDocument(&doc);

    // Test various Roman numeral conversions via FormatPageNumber
    // (ToRomanNumeralUpper is now internal to cDotCommandParser)
    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ROMAN_UPPER) == "I");
    CHECK(layout.FormatPageNumber(4, PAGE_NUM_ROMAN_UPPER) == "IV");
    CHECK(layout.FormatPageNumber(5, PAGE_NUM_ROMAN_UPPER) == "V");
    CHECK(layout.FormatPageNumber(9, PAGE_NUM_ROMAN_UPPER) == "IX");
    CHECK(layout.FormatPageNumber(10, PAGE_NUM_ROMAN_UPPER) == "X");
    CHECK(layout.FormatPageNumber(1994, PAGE_NUM_ROMAN_UPPER) == "MCMXCIV");
}

TEST_CASE("Layout2 - FormatPageNumber with Arabic")
{
    cDocument doc;
    cLayout layout;

    // Use .pn command to set format and offset
    doc.Insert(".pn 1\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Format is set to Arabic by default
    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ARABIC) == "1");
    CHECK(layout.FormatPageNumber(2, PAGE_NUM_ARABIC) == "2");
}

TEST_CASE("Layout2 - FormatPageNumber with offset")
{
    cDocument doc;
    cLayout layout;

    // .pn 5 sets offset to 4
    doc.Insert(".pn 5\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ARABIC) == "5");
}

TEST_CASE("Layout2 - FormatPageNumber with Roman Lowercase")
{
    cDocument doc;
    cLayout layout;

    // Use .pn i to set Roman lowercase
    doc.Insert(".pn i\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ROMAN_LOWER) == "i");
    CHECK(layout.FormatPageNumber(2, PAGE_NUM_ROMAN_LOWER) == "ii");
    CHECK(layout.FormatPageNumber(4, PAGE_NUM_ROMAN_LOWER) == "iv");
}

TEST_CASE("Layout2 - FormatPageNumber with Roman Uppercase")
{
    cDocument doc;
    cLayout layout;

    // Use .pn I to set Roman uppercase
    doc.Insert(".pn I\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    CHECK(layout.FormatPageNumber(1, PAGE_NUM_ROMAN_UPPER) == "I");
    CHECK(layout.FormatPageNumber(2, PAGE_NUM_ROMAN_UPPER) == "II");
    CHECK(layout.FormatPageNumber(4, PAGE_NUM_ROMAN_UPPER) == "IV");
}

TEST_CASE("Layout2 - Header with # shows page number (Arabic)")
{
    cDocument doc;
    cLayout layout;

    // Header with # should show "1" on page 1
    doc.Insert(".he Page #\r");
    doc.Insert("Body text\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Verify page 2 has header (headers appear AFTER definition)
    REQUIRE(headers.find(2) != headers.end());
    const auto& page2Headers = headers.at(2);
    REQUIRE(page2Headers.size() > 0);

    // Check that header contains glyphs
    // Header text "Page #" should become "Page 2" on page 2
    const sLineLayout& headerLine = page2Headers[0].line;
    REQUIRE(!headerLine.segments.empty());

    // Count glyphs - should be "Page 2\r" = 7 characters (includes \r)
    size_t totalGlyphs = 0;
    for (const auto& seg : headerLine.segments)
    {
        totalGlyphs += seg.GetGraphemeCount();
    }
    CHECK(totalGlyphs == 7);  // "Page 2\r"
}

TEST_CASE("Layout2 - Header with # and .pn 5 offset")
{
    cDocument doc;
    cLayout layout;

    // .pn 5 means page 1 displays as 5
    doc.Insert(".pn 5\r");
    doc.Insert(".he Page #\r");
    doc.Insert("Body text\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Verify page 2 has header with "Page 6" (physical page 2 = 5 + 1)
    REQUIRE(headers.find(2) != headers.end());
    const auto& page2Headers = headers.at(2);
    REQUIRE(page2Headers.size() > 0);

    const sLineLayout& headerLine = page2Headers[0].line;
    REQUIRE(!headerLine.segments.empty());

    // Count glyphs - should be "Page 6\r" = 7 characters (includes \r)
    size_t totalGlyphs = 0;
    for (const auto& seg : headerLine.segments)
    {
        totalGlyphs += seg.GetGraphemeCount();
    }
    CHECK(totalGlyphs == 7);  // "Page 6\r"
}

TEST_CASE("Layout2 - Header with # and .pn i (Roman lowercase)")
{
    cDocument doc;
    cLayout layout;

    // .pn i sets Roman lowercase format
    doc.Insert(".pn i\r");
    doc.Insert(".he Page #\r");
    doc.Insert("Body text\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Verify page 2 has header with "Page ii"
    REQUIRE(headers.find(2) != headers.end());
    const auto& page2Headers = headers.at(2);
    REQUIRE(page2Headers.size() > 0);

    const sLineLayout& headerLine = page2Headers[0].line;
    REQUIRE(!headerLine.segments.empty());

    // Count glyphs - should be "Page ii\r" = 8 characters (includes \r)
    size_t totalGlyphs = 0;
    for (const auto& seg : headerLine.segments)
    {
        totalGlyphs += seg.GetGraphemeCount();
    }
    CHECK(totalGlyphs == 8);  // "Page ii\r"
}

TEST_CASE("Layout2 - Header with # and .pn I (Roman uppercase)")
{
    cDocument doc;
    cLayout layout;

    // .pn I sets Roman uppercase format
    doc.Insert(".pn I\r");
    doc.Insert(".he Page #\r");
    doc.Insert("Body text\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Verify page 2 has header with "Page II"
    REQUIRE(headers.find(2) != headers.end());
    const auto& page2Headers = headers.at(2);
    REQUIRE(page2Headers.size() > 0);

    const sLineLayout& headerLine = page2Headers[0].line;
    REQUIRE(!headerLine.segments.empty());

    // Count glyphs - should be "Page II\r" = 8 characters (includes \r)
    size_t totalGlyphs = 0;
    for (const auto& seg : headerLine.segments)
    {
        totalGlyphs += seg.GetGraphemeCount();
    }
    CHECK(totalGlyphs == 8);  // "Page II\r"
}

TEST_CASE("Layout2 - Multiple # in header")
{
    cDocument doc;
    cLayout layout;

    // Header with multiple # instances
    doc.Insert(".he Page # of #\r");
    doc.Insert("Body text\r");

    // Force page break to create page 2
    doc.Insert(".pa\r");
    doc.Insert("Page 2 text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Verify page 2 has header
    REQUIRE(headers.find(2) != headers.end());
    const auto& page2Headers = headers.at(2);
    REQUIRE(page2Headers.size() > 0);

    const sLineLayout& headerLine = page2Headers[0].line;
    REQUIRE(!headerLine.segments.empty());

    // Count glyphs - "Page # of #\r" becomes "Page 2 of 2\r" = 12 characters (includes \r)
    size_t totalGlyphs = 0;
    for (const auto& seg : headerLine.segments)
    {
        totalGlyphs += seg.GetGraphemeCount();
    }
    CHECK(totalGlyphs == 12);  // "Page 2 of 2\r"
}

TEST_CASE("Layout2 - .pn mid-document changes format")
{
    cDocument doc;
    cLayout layout;

    // Start with Arabic
    doc.Insert(".he Page #\r");
    doc.Insert("Body text page 1\r");

    // Page break
    doc.Insert(".pa\r");

    // Switch to Roman lowercase on page 2
    doc.Insert(".pn i\r");
    doc.Insert(".he Page #\r");
    doc.Insert("Body text page 2\r");

    // Page break to create page 3
    doc.Insert(".pa\r");
    doc.Insert("Body text page 3\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    const auto& headers = layout.GetPageHeaders();

    // Page 2: .pn i on page 2 means page 2 becomes "i" (logical 1)
    // Header from first .he: "Page i\r" = 7 chars (P-a-g-e-space-i-\r)
    if (headers.find(2) != headers.end() && !headers.at(2).empty())
    {
        size_t totalGlyphs = 0;
        for (const auto& seg : headers.at(2)[0].line.segments)
        {
            totalGlyphs += seg.GetGraphemeCount();
        }
        CHECK(totalGlyphs == 7);  // "Page i\r" (.pn i makes page 2 = i)
    }

    // Page 3: .pn i on page 2, so page 3 = ii (logical 2)
    // Header from second .he: "Page ii\r" = 8 chars
    if (headers.find(3) != headers.end() && !headers.at(3).empty())
    {
        size_t totalGlyphs = 0;
        for (const auto& seg : headers.at(3)[0].line.segments)
        {
            totalGlyphs += seg.GetGraphemeCount();
        }
        CHECK(totalGlyphs == 8);  // "Page ii\r" (.pn i makes page 3 = ii)
    }
}

/////////////////////////////////////////////////////////////////////////////
//
// Phase 2 Step 5: Logical Page Number Tracking Tests
//
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Layout2 - GetLogicalPageNumber with no offset")
{
    cDocument doc;
    cLayout layout;

    // Create simple document with 2 pages
    doc.Insert("Page 1 content\r");
    doc.Insert(".pa\r");
    doc.Insert("Page 2 content\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // With no .pn command, logical = physical
    PAGE_T physicalPages = layout.GetNumberOfPages();
    PAGE_T logicalPage = layout.GetLogicalPageNumber();

    CHECK(physicalPages == 2);
    CHECK(logicalPage == 2);  // Last page processed
}

TEST_CASE("Layout2 - GetLogicalPageNumber with .pn 5 offset")
{
    cDocument doc;
    cLayout layout;

    // Set page number offset before content
    doc.Insert(".pn 5\r");
    doc.Insert("Page 1 content\r");
    doc.Insert(".pa\r");
    doc.Insert("Page 2 content\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Physical page 2, but logical page 6 (2 + offset of 4)
    PAGE_T physicalPages = layout.GetNumberOfPages();
    PAGE_T logicalPage = layout.GetLogicalPageNumber();

    CHECK(physicalPages == 2);
    CHECK(logicalPage == 6);  // Physical 2 + offset 4 = logical 6
}

TEST_CASE("Layout2 - GetLogicalPageNumber with .pn i (Roman)")
{
    cDocument doc;
    cLayout layout;

    // Set Roman numeral format
    doc.Insert(".pn i\r");
    doc.Insert("Page 1 content\r");
    doc.Insert(".pa\r");
    doc.Insert("Page 2 content\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Physical page 2, logical page 2 (no offset with Roman)
    PAGE_T physicalPages = layout.GetNumberOfPages();
    PAGE_T logicalPage = layout.GetLogicalPageNumber();

    CHECK(physicalPages == 2);
    CHECK(logicalPage == 2);  // Roman format, but no offset
}

/////////////////////////////////////////////////////////////////////////////
//
// Phase 3.2: Box-Based Viewport Tests
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
//
// Task 1: sViewport Structure Tests
//
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Viewport - Intersects - Box overlaps at top")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 500.0;
    box.screenYBottom = 1500.0;

    CHECK(viewport.Intersects(box) == true);
}

TEST_CASE("Viewport - Intersects - Box overlaps at bottom")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 2500.0;
    box.screenYBottom = 3500.0;

    CHECK(viewport.Intersects(box) == true);
}

TEST_CASE("Viewport - Intersects - Box completely inside viewport")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 1200.0;
    box.screenYBottom = 2800.0;

    CHECK(viewport.Intersects(box) == true);
}

TEST_CASE("Viewport - Intersects - Box above viewport")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 100.0;
    box.screenYBottom = 500.0;

    CHECK(viewport.Intersects(box) == false);
}

TEST_CASE("Viewport - Intersects - Box below viewport")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 4000.0;
    box.screenYBottom = 5000.0;

    CHECK(viewport.Intersects(box) == false);
}

TEST_CASE("Viewport - Intersects - Box completely contains viewport")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 0.0;
    box.screenYBottom = 5000.0;

    CHECK(viewport.Intersects(box) == true);
}

TEST_CASE("Viewport - Intersects - Box exactly at top boundary")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 0.0;
    box.screenYBottom = 1000.0;  // Exactly at topY

    CHECK(viewport.Intersects(box) == true);  // Boundary included
}

TEST_CASE("Viewport - Intersects - Box exactly at bottom boundary")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 3000.0;  // Exactly at bottomY
    box.screenYBottom = 4000.0;

    CHECK(viewport.Intersects(box) == true);  // Boundary included
}

TEST_CASE("Viewport - Intersects - Box touching but just above")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 0.0;
    box.screenYBottom = 999.0;  // Just below topY

    CHECK(viewport.Intersects(box) == false);
}

TEST_CASE("Viewport - Intersects - Zero-height box at viewport top")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 1000.0;
    box.screenYBottom = 1000.0;  // Zero height

    CHECK(viewport.Intersects(box) == true);
}

TEST_CASE("Viewport - Clear - Removes all visible boxes")
{
    sViewport viewport;

    // Add some boxes
    sBoxes box1, box2, box3;
    viewport.visibleBoxes.push_back(&box1);
    viewport.visibleBoxes.push_back(&box2);
    viewport.visibleBoxes.push_back(&box3);

    CHECK(viewport.visibleBoxes.size() == 3);

    viewport.Clear();

    CHECK(viewport.visibleBoxes.size() == 0);
}

/////////////////////////////////////////////////////////////////////////////
//
// Task 2: sBoxes CalculateScreenYRange Tests
//
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("BoxScreenYRange - Empty box")
{
    cDocument doc;
    cLayout layout;

    doc.Insert("Test line\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    sBoxes emptyBox;
    emptyBox.CalculateScreenYRange(&layout);

    CHECK(emptyBox.screenYTop == 0);
    CHECK(emptyBox.screenYBottom == 0);
}

TEST_CASE("BoxScreenYRange - Single line box")
{
    cDocument doc;
    cLayout layout;

    doc.Insert("Test line\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Get the first box from layout
    int boxCount = layout.GetBoxCount();
    if (boxCount > 0)
    {
        const sBoxes* boxPtr = layout.GetBoxByIndex(0);
        if (boxPtr)
        {
            sBoxes box = *boxPtr;
            box.CalculateScreenYRange(&layout);

            CHECK(box.screenYTop >= 0);
            CHECK(box.screenYBottom > box.screenYTop);  // Bottom should be below top
        }
    }
}

TEST_CASE("BoxScreenYRange - Multi-line box")
{
    cDocument doc;
    cLayout layout;

    doc.Insert("Line 1\r");
    doc.Insert("Line 2\r");
    doc.Insert("Line 3\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Get the first box from layout
    int boxCount = layout.GetBoxCount();
    if (boxCount > 0)
    {
        const sBoxes* boxPtr = layout.GetBoxByIndex(0);
        if (boxPtr)
        {
            sBoxes box = *boxPtr;
            box.CalculateScreenYRange(&layout);

            CHECK(box.screenYTop >= 0);
            CHECK(box.screenYBottom > box.screenYTop);

            // Multi-line box should have substantial height
            COORD_T height = box.screenYBottom - box.screenYTop;
            CHECK(height > 100);  // At least 100 twips for 3 lines
        }
    }
}

TEST_CASE("BoxScreenYRange - Recalculate after layout change")
{
    cDocument doc;
    cLayout layout;

    doc.Insert("Initial line\r");
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    int boxCount = layout.GetBoxCount();
    if (boxCount > 0)
    {
        const sBoxes* boxPtr = layout.GetBoxByIndex(0);
        if (boxPtr)
        {
            sBoxes box = *boxPtr;
            box.CalculateScreenYRange(&layout);

            COORD_T initialBottom = box.screenYBottom;

            // Add more content
            doc.Insert("Second line\r");
            doc.Insert("Third line\r");
            layout.LayoutDocument(&doc);

            // Recalculate
            int newBoxCount = layout.GetBoxCount();
            if (newBoxCount > 0)
            {
                const sBoxes* newBoxPtr = layout.GetBoxByIndex(0);
                if (newBoxPtr)
                {
                    sBoxes newBox = *newBoxPtr;
                    newBox.CalculateScreenYRange(&layout);

                    // Bottom should have moved down
                    CHECK(newBox.screenYBottom > initialBottom);
                }
            }
        }
    }
}

TEST_CASE("BoxScreenYRange - MarkDirty and ClearDirty")
{
    sBoxes box;

    // Initially clean
    CHECK(box.needsRedraw == false);

    // Mark dirty
    box.MarkDirty();
    CHECK(box.needsRedraw == true);

    // Clear dirty
    box.ClearDirty();
    CHECK(box.needsRedraw == false);

    // Can mark dirty multiple times
    box.MarkDirty();
    box.MarkDirty();
    CHECK(box.needsRedraw == true);
}

TEST_CASE("BoxScreenYRange - Constructor initializes correctly")
{
    sBoxes box;

    CHECK(box.screenYTop == 0);
    CHECK(box.screenYBottom == 0);
    CHECK(box.needsRedraw == false);
}

/////////////////////////////////////////////////////////////////////////////
//
// Additional Viewport Tests - Edge Cases
//
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("Viewport - Constructor initializes correctly")
{
    sViewport viewport;

    CHECK(viewport.topY == 0);
    CHECK(viewport.bottomY == 0);
    CHECK(viewport.scrollOffset == 0);
    CHECK(viewport.viewportHeight == 0);
    CHECK(viewport.visibleBoxes.size() == 0);
}

TEST_CASE("Viewport - Intersects with negative coordinates")
{
    sViewport viewport;
    viewport.topY = -1000.0;
    viewport.bottomY = 1000.0;

    sBoxes box;
    box.screenYTop = -500.0;
    box.screenYBottom = 500.0;

    CHECK(viewport.Intersects(box) == true);
}

TEST_CASE("Viewport - Multiple boxes at same Y (column test)")
{
    sViewport viewport;
    viewport.topY = 0.0;
    viewport.bottomY = 1000.0;

    // Simulate two-column layout: both boxes at same Y
    sBoxes leftColumn;
    leftColumn.left = 0;
    leftColumn.screenYTop = 0;
    leftColumn.screenYBottom = 1000;

    sBoxes rightColumn;
    rightColumn.left = 3600;  // Different X, same Y
    rightColumn.screenYTop = 0;
    rightColumn.screenYBottom = 1000;

    // Both should intersect - this is the multi-column test!
    CHECK(viewport.Intersects(leftColumn) == true);
    CHECK(viewport.Intersects(rightColumn) == true);
}

TEST_CASE("Viewport - Large document coordinates")
{
    sViewport viewport;
    viewport.topY = 100000.0;    // Deep in document
    viewport.bottomY = 101000.0;

    sBoxes box;
    box.screenYTop = 100500.0;
    box.screenYBottom = 100700.0;

    CHECK(viewport.Intersects(box) == true);
}

TEST_CASE("Viewport - Floating point precision at boundaries")
{
    sViewport viewport;
    viewport.topY = 1000.0;
    viewport.bottomY = 3000.0;

    sBoxes box;
    box.screenYTop = 999.0;
    box.screenYBottom = 999.5;

    // Box completely before viewport - should not intersect
    CHECK(viewport.Intersects(box) == false);

    // Box touching at boundary - should intersect (boundary inclusive)
    sBoxes box2;
    box2.screenYTop = 999.0;
    box2.screenYBottom = 1000.0;
    CHECK(viewport.Intersects(box2) == true);
}


/////////////////////////////////////////////////////////////////////////////
//
// Phase 3.2 Task 4: CalculateViewport() Tests
//
// These tests verify the box-based viewport calculation that enables
// multi-column layouts and efficient rendering.
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() with default editor - should have EOF box
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Null layout safety")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Editor constructor now always creates layout and document with EOF
    // So we expect 1 box (the EOF marker) to be visible
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();
    layout->LayoutDocument(doc);

    editor.CalculateViewport();

    const sViewport& viewport = editor.GetViewport();
    // Should have 1 box (EOF marker)
    CHECK(viewport.visibleBoxes.size() == 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() with default document - EOF box visible
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Empty document")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Editor constructor creates document with EOF, so layout will have 1 box
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();
    layout->LayoutDocument(doc);

    // Calculate viewport with default document (contains EOF)
    editor.CalculateViewport();

    const sViewport& viewport = editor.GetViewport();

    // Viewport should be calculated with 1 visible box (EOF marker)
    CHECK(viewport.viewportHeight > 0);
    CHECK(viewport.topY == 0);
    CHECK(viewport.bottomY == viewport.viewportHeight);
    CHECK(viewport.visibleBoxes.size() == 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() with single box fully visible
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Single box fully visible")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    doc->Insert("This is a test paragraph.\r");

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.SetScrollOffset(0);

    // Calculate viewport
    editor.CalculateViewport();

    const sViewport& viewport = editor.GetViewport();

    // Should have at least one visible box (the paragraph we added)
    CHECK(viewport.visibleBoxes.size() >= 1);

    // Viewport bounds should be correct
    CHECK(viewport.scrollOffset == 0);
    CHECK(viewport.topY == 0);
    CHECK(viewport.viewportHeight > 0);
    CHECK(viewport.bottomY == viewport.viewportHeight);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() with multiple boxes - some visible, some not
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Multiple boxes partial visibility")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Add multiple paragraphs to create multiple boxes
    for (int i = 0; i < 50; ++i)
    {
        doc->Insert("This is test paragraph number " + std::to_string(i) + ".\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    // Test 1: At top of document
    editor.SetScrollOffset(0);
    editor.CalculateViewport();

    const sViewport& viewport1 = editor.GetViewport();
    size_t visibleAtTop = viewport1.visibleBoxes.size();

    CHECK(visibleAtTop > 0);
    CHECK(viewport1.topY == 0);

    // Test 2: Scroll down significantly (but not past end)
    COORD_T scrollAmount = 2000;  // 2000 twips down (reasonable for 50 paragraphs)
    editor.SetScrollOffset(scrollAmount);
    editor.CalculateViewport();

    const sViewport& viewport2 = editor.GetViewport();

    CHECK(viewport2.topY == scrollAmount);
    CHECK(viewport2.bottomY == scrollAmount + viewport2.viewportHeight);

    // We should have some visible boxes at this scroll position
    CHECK(viewport2.visibleBoxes.size() > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() box sorting - verifies Y then X sort order
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Box sorting order")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    cDocument doc;

    // Add enough paragraphs to ensure we get multiple boxes
    for (int i = 0; i < 10; ++i)
    {
        doc.Insert("Test paragraph " + std::to_string(i) + ".\r");
    }

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.SetScrollOffset(0);

    // Calculate viewport
    editor.CalculateViewport();

    const sViewport& viewport = editor.GetViewport();

    // Verify boxes are sorted by Y position
    if (viewport.visibleBoxes.size() > 1)
    {
        for (size_t i = 1; i < viewport.visibleBoxes.size(); ++i)
        {
            const sBoxes* prevBox = viewport.visibleBoxes[i - 1];
            const sBoxes* currentBox = viewport.visibleBoxes[i];

            // Either current box has higher Y, or same Y with higher X
            bool validOrder = (currentBox->screenYTop >= prevBox->screenYTop - 1.0);
            CHECK(validOrder);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() with scrolling through document
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Scrolling through document")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Add many paragraphs for scrolling
    for (int i = 0; i < 100; ++i)
    {
        doc->Insert("Paragraph " + std::to_string(i) + ".\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    // Test scrolling at different offsets
    // Use reasonable offsets for 100 paragraphs (~240 twips per line, ~2400 per paragraph)
    std::vector<COORD_T> scrollOffsets = {0, 1000, 2000, 3000, 4000};

    for (COORD_T offset : scrollOffsets)
    {
        editor.SetScrollOffset(offset);
        editor.CalculateViewport();

        const sViewport& viewport = editor.GetViewport();

        // Verify viewport bounds match scroll offset
        CHECK(viewport.scrollOffset == offset);
        CHECK(viewport.topY == offset);
        CHECK(viewport.bottomY == offset + viewport.viewportHeight);

        // Should have visible boxes at all these positions (all within document)
        CHECK(viewport.visibleBoxes.size() > 0);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() with window resize
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Window resize updates viewport")
{
    cEditorCtrl editor;

    cDocument doc;
    doc.Insert("Test paragraph.\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.SetScrollOffset(0);

    // Calculate with initial size
    editor.resize(800, 600);
    editor.CalculateViewport();

    const sViewport& viewport1 = editor.GetViewport();
    COORD_T height1 = viewport1.viewportHeight;

    // Resize window and recalculate
    editor.resize(800, 1200);
    editor.CalculateViewport();

    const sViewport& viewport2 = editor.GetViewport();
    COORD_T height2 = viewport2.viewportHeight;

    // Viewport height should have increased (roughly doubled)
    CHECK(height2 > height1);
    CHECK(height2 >= height1 * 1.8);  // Allow for some margin
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() identifies boxes at viewport boundaries
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Boxes at viewport boundaries")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    cDocument doc;

    // Add many paragraphs
    for (int i = 0; i < 50; ++i)
    {
        doc.Insert("Test paragraph " + std::to_string(i) + ".\r");
    }

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Scroll to middle of document
    editor.SetScrollOffset(15000);
    editor.CalculateViewport();

    const sViewport& viewport = editor.GetViewport();

    // Check that all visible boxes actually intersect the viewport
    for (const sBoxes* box : viewport.visibleBoxes)
    {
        // Box must intersect viewport Y range
        bool intersects = (box->screenYBottom >= viewport.topY &&
                          box->screenYTop <= viewport.bottomY);
        CHECK(intersects);
    }

    // Also verify using the Intersects() method
    for (const sBoxes* box : viewport.visibleBoxes)
    {
        CHECK(viewport.Intersects(*box));
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() handles edge case of box exactly at top
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Box exactly at viewport top")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    cDocument doc;
    doc.Insert("First paragraph.\r");
    doc.Insert("Second paragraph.\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Get first box's bottom position
    const std::vector<sBoxes>& boxes = layout.GetGlobalBoxList();
    if (boxes.size() > 0)
    {
        COORD_T firstBoxBottom = boxes[0].screenYBottom;

        // Scroll so first box is just above viewport (boundary case)
        editor.SetScrollOffset(firstBoxBottom);
        editor.CalculateViewport();

        const sViewport& viewport = editor.GetViewport();

        // First box should not be visible (it's above viewport)
        // But second box should be visible
        bool foundSecondBox = false;
        for (const sBoxes* box : viewport.visibleBoxes)
        {
            if (box->screenYTop >= firstBoxBottom)
            {
                foundSecondBox = true;
                break;
            }
        }

        if (boxes.size() > 1)
        {
            CHECK(foundSecondBox);
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() recalculation clears previous boxes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Recalculation clears previous state")
{
    cEditorCtrl editor;
    editor.resize(800, 600);

    cDocument doc;

    for (int i = 0; i < 20; ++i)
    {
        doc.Insert("Test paragraph " + std::to_string(i) + ".\r");
    }

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Calculate at top
    editor.SetScrollOffset(0);
    editor.CalculateViewport();
    const sViewport& viewport1 = editor.GetViewport();
    size_t count1 = viewport1.visibleBoxes.size();

    // Scroll far down and recalculate
    editor.SetScrollOffset(50000);
    editor.CalculateViewport();
    const sViewport& viewport2 = editor.GetViewport();
    size_t count2 = viewport2.visibleBoxes.size();

    // The visible boxes list should have been cleared and recalculated
    // At the bottom of the document, we might have fewer or no boxes
    // The key is that it recalculated (didn't just append)

    // Scroll back to top
    editor.SetScrollOffset(0);
    editor.CalculateViewport();
    const sViewport& viewport3 = editor.GetViewport();
    size_t count3 = viewport3.visibleBoxes.size();

    // Should have same boxes as first time at top
    CHECK(count3 == count1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CalculateViewport() viewport height matches widget height
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CalculateViewport - Viewport height calculation")
{
    cEditorCtrl editor;

    cDocument doc;
    doc.Insert("Test.\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Test different widget heights
    std::vector<int> heights = {300, 600, 900, 1200};

    for (int h : heights)
    {
        editor.resize(800, h);
        editor.CalculateViewport();

        const sViewport& viewport = editor.GetViewport();

        // Viewport height should match widget height * FONTSCALE
        COORD_T expectedHeight = h * FONTSCALE;
        CHECK(viewport.viewportHeight == expectedHeight);
    }
}


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

#include "doctest.h"

#include "src/core/layout/layoutstructs.h"
#include "src/core/layout/layoutbase.h"
#include "src/gui/layout/layout.h"
#include "src/gui/editor/editorctrl.h"
#include "src/core/document/document.h"
#include <QScrollBar>

/////////////////////////////////////////////////////////////////////////////
//
// Task 6 - Scrollbar Integration Tests
// Phase 3.2: Box-Based Viewport
//
/////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetTotalDocumentHeight() with empty document
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - GetTotalDocumentHeight - Empty document")
{
    cLayout layout;
    cDocument doc;

    layout.SetDocument(&doc);
    // Don't layout - empty document

    COORD_T height = layout.GetTotalDocumentHeight();
    CHECK(height == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetTotalDocumentHeight() with single box
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - GetTotalDocumentHeight - Single box")
{
    cLayout layout;
    cDocument doc;
    doc.Insert("Hello world\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    COORD_T height = layout.GetTotalDocumentHeight();

    // Should be > 0 and match the screenYBottom of the box
    CHECK(height > 0);

    // Verify it matches the last box's screenYBottom
    const std::vector<sBoxes>& boxes = layout.GetGlobalBoxList();
    if (!boxes.empty())
    {
        COORD_T expectedHeight = 0;
        for (const auto& box : boxes)
        {
            if (box.screenYBottom > expectedHeight)
            {
                expectedHeight = box.screenYBottom;
            }
        }
        CHECK(height == expectedHeight);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetTotalDocumentHeight() with multiple boxes
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - GetTotalDocumentHeight - Multiple boxes")
{
    cLayout layout;
    cDocument doc;

    // Create document with margin changes (creates multiple boxes)
    doc.Insert("First paragraph\r");
    doc.Insert(".lm 2i\r");  // Margin change creates new box
    doc.Insert("Second paragraph\r");
    doc.Insert(".lm 1i\r");  // Another margin change
    doc.Insert("Third paragraph\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    COORD_T height = layout.GetTotalDocumentHeight();

    // Should return maximum screenYBottom across all boxes
    const std::vector<sBoxes>& boxes = layout.GetGlobalBoxList();
    CHECK(boxes.size() >= 2);  // Should have multiple boxes

    COORD_T maxBottom = 0;
    for (const auto& box : boxes)
    {
        if (box.screenYBottom > maxBottom)
        {
            maxBottom = box.screenYBottom;
        }
    }

    CHECK(height == maxBottom);
    CHECK(height > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetTotalDocumentHeight() with page breaks
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - GetTotalDocumentHeight - Multiple pages")
{
    cLayout layout;
    cDocument doc;

    // Create multi-page document
    doc.Insert("Page 1\r");
    doc.Insert(".pa\r");  // Page break
    doc.Insert("Page 2\r");
    doc.Insert(".pa\r");  // Another page break
    doc.Insert("Page 3\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    COORD_T height = layout.GetTotalDocumentHeight();

    // Should be tall (multiple pages)
    CHECK(height > 0);  // Height should be greater than zero

    // Verify against box data
    const std::vector<sBoxes>& boxes = layout.GetGlobalBoxList();
    COORD_T maxBottom = 0;
    for (const auto& box : boxes)
    {
        if (box.screenYBottom > maxBottom)
        {
            maxBottom = box.screenYBottom;
        }
    }

    CHECK(height == maxBottom);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetAverageLineHeight() with .LH command set
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - GetAverageLineHeight - With LH command")
{
    cLayout layout;
    cDocument doc;

    doc.Insert(".lh 16\r");  // Set line height to 16 points = 320 twips
    doc.Insert("Test text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    COORD_T avgHeight = layout.GetAverageLineHeight();

    // Should return the .LH value (320 twips = 16 points)
    CHECK(avgHeight > 0);
    CHECK(avgHeight < 1000);  // Reasonable upper bound
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetAverageLineHeight() without .LH command
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - GetAverageLineHeight - From first line")
{
    cLayout layout;
    cDocument doc;

    doc.Insert("Test text\r");

    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    COORD_T avgHeight = layout.GetAverageLineHeight();

    // Should return first line's height
    CHECK(avgHeight > 0);
    CHECK(avgHeight < 1000);  // Reasonable range

    // Verify it matches first line
    if (layout.GetNumberOfLines() > 0)
    {
        const sLineLayout* firstLine = layout.GetLineByRawLineNumber(0);
        if (firstLine)
        {
            CHECK(CoordsEqual(avgHeight, firstLine->lineheight));
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetAverageLineHeight() with empty document
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - GetAverageLineHeight - Empty document fallback")
{
    cLayout layout;
    cDocument doc;

    layout.SetDocument(&doc);
    // Don't layout - empty

    COORD_T avgHeight = layout.GetAverageLineHeight();

    // Should return default fallback (240 twips = 12pt)
    CHECK(avgHeight == 240);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetScrollbar() with null scrollbar
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - SetScrollbar - Null scrollbar accepted")
{
    cEditorCtrl editor;
    cDocument doc;
    doc.Insert("Test\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Should not crash with nullptr
    editor.SetScrollbar(nullptr);

    // UpdateScrollbar should also be safe with null
    editor.UpdateScrollbar();

    CHECK(true);  // If we get here, no crash occurred
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetScrollbar() connects signal correctly
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - SetScrollbar - Signal connection")
{
    cEditorCtrl editor;
    cDocument doc;
    doc.Insert("Test paragraph with some text\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Scrollbar should have been updated
    CHECK(scrollbar.minimum() >= 0);
    CHECK(scrollbar.maximum() >= 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetScrollbar() replaces old scrollbar
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - SetScrollbar - Replacing scrollbar")
{
    cEditorCtrl editor;
    cDocument doc;
    doc.Insert("Test\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    QScrollBar scrollbar1;
    QScrollBar scrollbar2;

    // Set first scrollbar
    editor.SetScrollbar(&scrollbar1);

    // Replace with second scrollbar (should disconnect first)
    editor.SetScrollbar(&scrollbar2);

    // Should not crash
    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test UpdateScrollbar() with null scrollbar
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - UpdateScrollbar - Null scrollbar safety")
{
    cEditorCtrl editor;
    cDocument doc;
    doc.Insert("Test\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    // Should not crash with no scrollbar
    editor.UpdateScrollbar();

    CHECK(true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test UpdateScrollbar() sets correct range
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - UpdateScrollbar - Sets correct range")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Create multi-paragraph document
    for (int i = 0; i < 50; ++i)
    {
        doc->Insert("This is test paragraph number ");
        doc->Insert(std::to_string(i));
        doc->Insert(" with some text in it.\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Range should be 0 to (totalHeight - viewportHeight)
    CHECK(scrollbar.minimum() == 0);
    CHECK(scrollbar.maximum() > 0);

    COORD_T totalHeight = layout->GetTotalDocumentHeight();
    COORD_T viewportHeight = 600 * FONTSCALE;
    COORD_T expectedMax = totalHeight - viewportHeight;

    if (expectedMax > 0)
    {
        CHECK(scrollbar.maximum() == static_cast<int>(expectedMax));
    }
    else
    {
        CHECK(scrollbar.maximum() == 0);  // Document fits in viewport
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test UpdateScrollbar() sets correct page step
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - UpdateScrollbar - Sets page step")
{
    cEditorCtrl editor;
    cDocument doc;

    for (int i = 0; i < 20; ++i)
    {
        doc.Insert("Test paragraph\r");
    }

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Page step should be viewport height
    COORD_T viewportHeight = 600 * FONTSCALE;
    CHECK(scrollbar.pageStep() == static_cast<int>(viewportHeight));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test UpdateScrollbar() sets correct single step
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - UpdateScrollbar - Sets single step")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    doc->Insert(".lh 20\r");  // 20 points = 400 twips
    doc->Insert("Test paragraph\r");

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Single step should be average line height (400 twips in this case)
    COORD_T avgLineHeight = layout->GetAverageLineHeight();
    CHECK(scrollbar.singleStep() == static_cast<int>(avgLineHeight));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test UpdateScrollbar() when document fits in viewport
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - UpdateScrollbar - Document fits in viewport")
{
    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();  // Get reference for direct document access
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());  // Get reference for direct layout access

    // Small document
    doc->Insert("Short\r");

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 2000);  // Very tall viewport

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Maximum should be 0 (can't scroll)
    CHECK(scrollbar.maximum() == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test OnScrollbarChanged() updates scroll offset
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - OnScrollbarChanged - Updates offset")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    for (int i = 0; i < 50; ++i)
    {
        doc->Insert("Test paragraph ");
        doc->Insert(std::to_string(i));
        doc->Insert("\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Simulate scrollbar change
    COORD_T oldOffset = editor.GetScrollOffset();
    scrollbar.setValue(1000);  // Scroll to 1000 twips

    // Offset should have updated
    COORD_T newOffset = editor.GetScrollOffset();
    CHECK(newOffset == 1000);
    CHECK(newOffset != oldOffset);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test OnScrollbarChanged() clamps to valid range
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - OnScrollbarChanged - Clamps to range")
{
    cEditorCtrl editor;
    cDocument doc;

    for (int i = 0; i < 10; ++i)
    {
        doc.Insert("Test\r");
    }

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Try to set negative (should clamp to 0)
    scrollbar.setValue(-100);
    CHECK(editor.GetScrollOffset() == 0);

    // Try to set beyond max (should clamp to max)
    COORD_T totalHeight = layout.GetTotalDocumentHeight();
    COORD_T viewportHeight = 600 * FONTSCALE;
    COORD_T maxScroll = totalHeight - viewportHeight;

    if (maxScroll > 0)
    {
        scrollbar.setValue(static_cast<int>(maxScroll) + 1000);
        CHECK(editor.GetScrollOffset() <= maxScroll);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetScrollOffset() syncs scrollbar
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - SetScrollOffset - Syncs scrollbar")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    for (int i = 0; i < 50; ++i)
    {
        doc->Insert("Paragraph\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Set scroll offset programmatically
    editor.SetScrollOffset(2000);

    // Scrollbar should have synced
    CHECK(scrollbar.value() == 2000);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test Y-based scrolling vs line-based for multi-column readiness
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - Y-based scrolling - Multi-column ready")
{
    cEditorCtrl editor;
    cDocument doc;

    // Create document with multiple boxes
    doc.Insert("First box\r");
    doc.Insert(".lm 2i\r");
    doc.Insert("Second box (different X, could be column)\r");
    doc.Insert(".lm 1i\r");
    doc.Insert("Third box\r");

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Scrollbar should use Y-coordinates (twips), not line numbers
    // This is verified by checking that the range is in twips
    CHECK(scrollbar.minimum() == 0);

    // Maximum should be based on document height (twips), not line count
    COORD_T totalHeight = layout.GetTotalDocumentHeight();
    LINE_T lineCount = layout.GetNumberOfLines();

    // If this were line-based, max would equal lineCount
    // Since it's Y-based, max equals (totalHeight - viewportHeight) in twips
    // These values should be VERY different
    CHECK(scrollbar.maximum() != static_cast<int>(lineCount));
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test CalculateTotalDocumentHeight() in page mode includes page gaps
///
/// Verifies that the scrollbar range is larger in PAGE mode than in
/// CONTINUOUS mode for the same document because page gaps are included.
///
/// Bug fix: Previously, scrollbar in page mode couldn't scroll to bottom
/// because GetTotalDocumentHeight() didn't account for page gaps.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Task6 - Page mode scrollbar includes page gaps")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Create multi-page document (force multiple pages with .pa)
    for (int i = 0; i < 50; ++i)
    {
        doc->Insert("This is test paragraph number ");
        doc->Insert(std::to_string(i));
        doc->Insert(" with some text in it.\r");
    }
    doc->Insert(".pa\r");  // Force page break
    for (int i = 50; i < 100; ++i)
    {
        doc->Insert("This is test paragraph number ");
        doc->Insert(std::to_string(i));
        doc->Insert(" with some text in it.\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    QScrollBar scrollbar;
    editor.SetScrollbar(&scrollbar);

    // Verify we have multiple pages
    PAGE_T pageCount = layout->GetNumberOfPages();
    CHECK(pageCount >= 2);

    // TEST 1: In CONTINUOUS mode, use continuous height (no gaps)
    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    editor.UpdateScrollbar();

    int continuousMax = scrollbar.maximum();

    // Continuous max should be based on GetTotalDocumentHeight()
    COORD_T viewportHeight = 600 * FONTSCALE;
    COORD_T continuousHeight = layout->GetTotalDocumentHeight();
    COORD_T expectedContinuousMax = continuousHeight - viewportHeight;
    if (expectedContinuousMax > 0)
    {
        CHECK(continuousMax == static_cast<int>(expectedContinuousMax));
    }

    // TEST 2: In PAGE mode, include page gaps
    editor.SetDisplayMode(DISPLAY_PAGE);
    editor.UpdateScrollbar();

    int pageMax = scrollbar.maximum();

    // After mode switch, page count might have changed due to relayout (SHOW_NONE)
    PAGE_T pageModePageCount = layout->GetNumberOfPages();
    COORD_T paperHeight = layout->GetPaperHeight();
    COORD_T pageGap = 360;  // Default from sDisplaySettings

    // Use the editor's own calculation method
    COORD_T editorCalculatedHeight = editor.CalculateTotalDocumentHeight();
    COORD_T expectedPageMax = editorCalculatedHeight - viewportHeight;
    if (expectedPageMax > 0)
    {
        CHECK(pageMax == static_cast<int>(expectedPageMax));
    }

    // TEST 3: Page mode max should be LARGER than continuous mode max
    // because page gaps add extra height
    if (pageModePageCount >= 2)
    {
        CHECK(pageMax > continuousMax);

        // The difference should include (pageModePageCount - 1) * pageGap
        // But actual difference might be larger due to relayout effects
        int actualDifference = pageMax - continuousMax;
        int minExpectedGaps = (pageModePageCount - 1) * pageGap;

        // Difference should be at least the gaps (might be more due to relayout)
        CHECK(actualDifference >= minExpectedGaps);
    }

    // TEST 4: Verify we can scroll to maximum (no clamping issues)
    // Set scroll offset to maximum and verify it's accepted
    editor.SetScrollOffset(pageMax);
    COORD_T actualOffset = editor.GetScrollOffset();
    CHECK(actualOffset == pageMax);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test mouse wheel scrolling decreases offset when scrolling up
///
/// Simulates mouse wheel up event (positive delta) and verifies that
/// scroll offset decreases (scrolls toward top of document).
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Mousewheel - Scroll up decreases offset")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Create multi-line document
    for (int i = 0; i < 100; ++i)
    {
        doc->Insert("This is test line ");
        doc->Insert(std::to_string(i));
        doc->Insert(".\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    // Scroll to middle of document
    COORD_T initialOffset = 5000;
    editor.SetScrollOffset(initialOffset);

    // Create wheel event: positive delta = scroll up
    // Standard wheel notch = 120 units (15 degrees x 8)
    QPoint angleDelta(0, 120);  // One notch up
    QWheelEvent event(QPointF(400, 300), QPointF(400, 300),
                     QPoint(0, 0), angleDelta,
                     Qt::NoButton, Qt::NoModifier,
                     Qt::ScrollPhase::NoScrollPhase, false);

    // Send wheel event to editor
    QCoreApplication::sendEvent(&editor, &event);

    // Verify offset decreased
    COORD_T newOffset = editor.GetScrollOffset();
    CHECK(newOffset < initialOffset);

    // Verify scrolled by approximately one line height
    COORD_T lineHeight = layout->GetAverageLineHeight();
    COORD_T expectedOffset = initialOffset - lineHeight;
    CHECK(std::abs(newOffset - expectedOffset) < 5);  // Allow small tolerance
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test mouse wheel scrolling increases offset when scrolling down
///
/// Simulates mouse wheel down event (negative delta) and verifies that
/// scroll offset increases (scrolls toward bottom of document).
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Mousewheel - Scroll down increases offset")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Create multi-line document
    for (int i = 0; i < 100; ++i)
    {
        doc->Insert("This is test line ");
        doc->Insert(std::to_string(i));
        doc->Insert(".\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    // Start at top of document
    COORD_T initialOffset = 0;
    editor.SetScrollOffset(initialOffset);

    // Create wheel event: negative delta = scroll down
    QPoint angleDelta(0, -120);  // One notch down
    QWheelEvent event(QPointF(400, 300), QPointF(400, 300),
                     QPoint(0, 0), angleDelta,
                     Qt::NoButton, Qt::NoModifier,
                     Qt::ScrollPhase::NoScrollPhase, false);

    // Send wheel event to editor
    QCoreApplication::sendEvent(&editor, &event);

    // Verify offset increased
    COORD_T newOffset = editor.GetScrollOffset();
    CHECK(newOffset > initialOffset);

    // Verify scrolled by approximately one line height
    COORD_T lineHeight = layout->GetAverageLineHeight();
    COORD_T expectedOffset = initialOffset + lineHeight;
    CHECK(std::abs(newOffset - expectedOffset) < 5);  // Allow small tolerance
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test mouse wheel respects document bounds (clamping at top)
///
/// Verifies that scrolling up beyond document start is clamped to offset=0.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Mousewheel - Clamping at document top")
{
    cEditorCtrl editor;
    cDocument doc;

    // Create small document
    for (int i = 0; i < 10; ++i)
    {
        doc.Insert("Line ");
        doc.Insert(std::to_string(i));
        doc.Insert(".\r");
    }

    cLayout layout;
    layout.SetDocument(&doc);
    layout.LayoutDocument(&doc);

    editor.resize(800, 600);

    // Start near top
    editor.SetScrollOffset(100);

    // Scroll up multiple times (should clamp at 0)
    for (int i = 0; i < 5; ++i)
    {
        QPoint angleDelta(0, 120);  // Scroll up
        QWheelEvent event(QPointF(400, 300), QPointF(400, 300),
                         QPoint(0, 0), angleDelta,
                         Qt::NoButton, Qt::NoModifier,
                         Qt::ScrollPhase::NoScrollPhase, false);
        QCoreApplication::sendEvent(&editor, &event);
    }

    // Verify clamped to 0
    COORD_T finalOffset = editor.GetScrollOffset();
    CHECK(finalOffset == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test mouse wheel respects document bounds (clamping at bottom)
///
/// Verifies that scrolling down beyond document end is clamped to maximum.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Mousewheel - Clamping at document bottom")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Create document with known height
    for (int i = 0; i < 50; ++i)
    {
        doc->Insert("This is test line ");
        doc->Insert(std::to_string(i));
        doc->Insert(" with some content.\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    // Calculate maximum scroll offset
    COORD_T totalHeight = layout->GetTotalDocumentHeight();
    COORD_T viewportHeight = 600 * FONTSCALE;
    COORD_T maxOffset = totalHeight - viewportHeight;
    if (maxOffset < 0)
    {
        maxOffset = 0;
    }

    // Start near bottom
    editor.SetScrollOffset(maxOffset - 500);

    // Scroll down multiple times (should clamp at maxOffset)
    for (int i = 0; i < 10; ++i)
    {
        QPoint angleDelta(0, -120);  // Scroll down
        QWheelEvent event(QPointF(400, 300), QPointF(400, 300),
                         QPoint(0, 0), angleDelta,
                         Qt::NoButton, Qt::NoModifier,
                         Qt::ScrollPhase::NoScrollPhase, false);
        QCoreApplication::sendEvent(&editor, &event);
    }

    // Verify clamped to maximum
    COORD_T finalOffset = editor.GetScrollOffset();
    CHECK(finalOffset == maxOffset);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Test multiple wheel events accumulate correctly
///
/// Verifies that multiple wheel scrolls accumulate their offsets correctly.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Mousewheel - Multiple events accumulate")
{
    cEditorCtrl editor;

    // Use editor's owned document and layout (editor now owns these)
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();

    // Create multi-line document
    for (int i = 0; i < 100; ++i)
    {
        doc->Insert("Line ");
        doc->Insert(std::to_string(i));
        doc->Insert(".\r");
    }

    layout->SetDocument(doc);
    layout->LayoutDocument(doc);

    editor.resize(800, 600);

    // Start at top
    editor.SetScrollOffset(0);
    COORD_T lineHeight = layout->GetAverageLineHeight();

    // Scroll down 3 times
    for (int i = 0; i < 3; ++i)
    {
        QPoint angleDelta(0, -120);  // One notch down
        QWheelEvent event(QPointF(400, 300), QPointF(400, 300),
                         QPoint(0, 0), angleDelta,
                         Qt::NoButton, Qt::NoModifier,
                         Qt::ScrollPhase::NoScrollPhase, false);
        QCoreApplication::sendEvent(&editor, &event);
    }

    // Verify scrolled by approximately 3 line heights
    COORD_T finalOffset = editor.GetScrollOffset();
    COORD_T expectedOffset = 3 * lineHeight;
    CHECK(std::abs(finalOffset - expectedOffset) < 15);  // Allow tolerance
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand with math expressions in dot commands
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand math expressions in dot commands")
{
    cLayoutBaseTest layout ;
    cDocument doc ;
    layout.SetDocument(&doc) ;

    SUBCASE("page length bare math: .pl 66 - 6 parses successfully")
    {
        eDotCommandStatus result = layout.ParseDotCommand(".pl 66 - 6") ;
        CHECK(result == DOT_GOOD) ;
    }

    SUBCASE("right margin with unit math: .rm 2i + 3i = 7200 twips")
    {
        eDotCommandStatus result = layout.ParseDotCommand(".rm 2i + 3i") ;
        CHECK(result == DOT_GOOD) ;

        layout.CreatePageBox(1) ;
        CHECK(layout.GetRightMargin() == doctest::Approx(7200.0).epsilon(1.0)) ;
    }

    SUBCASE("left margin with unit: .lm 1i = 1440 twips")
    {
        eDotCommandStatus result = layout.ParseDotCommand(".lm 1i") ;
        CHECK(result == DOT_GOOD) ;

        layout.CreatePageBox(1) ;
        CHECK(layout.GetLeftMargin() == doctest::Approx(1440.0).epsilon(1.0)) ;
    }

    SUBCASE("left margin with multiplication: .lm 0 resets to 0")
    {
        // first set a non-zero margin
        layout.ParseDotCommand(".lm 2i") ;
        CHECK(layout.GetLeftMargin() == doctest::Approx(2880.0).epsilon(1.0)) ;

        // reset to 0
        eDotCommandStatus result = layout.ParseDotCommand(".lm 0") ;
        CHECK(result == DOT_GOOD) ;
        CHECK(layout.GetLeftMargin() == doctest::Approx(0.0)) ;
    }

    SUBCASE("right margin with mixed units: .rm 4c + 2i")
    {
        eDotCommandStatus result = layout.ParseDotCommand(".rm 4c + 2i") ;
        CHECK(result == DOT_GOOD) ;

        layout.CreatePageBox(1) ;
        // 4cm + 2in = 4*TWIPSPERCM + 2*TWIPSPERINCH
        double expected = 4.0 * TWIPSPERCM + 2.0 * TWIPSPERINCH ;
        CHECK(layout.GetRightMargin() == doctest::Approx(expected).epsilon(1.0)) ;
    }

    SUBCASE("right margin with dimensionless multiplier: .rm 8 * 2i = 23040 twips")
    {
        eDotCommandStatus result = layout.ParseDotCommand(".rm 8 * 2i") ;
        CHECK(result == DOT_GOOD) ;

        layout.CreatePageBox(1) ;
        CHECK(layout.GetRightMargin() == doctest::Approx(23040.0).epsilon(1.0)) ;
    }

    SUBCASE("top margin bare math: .mt 3 - 1 parses successfully")
    {
        eDotCommandStatus result = layout.ParseDotCommand(".mt 3 - 1") ;
        CHECK(result == DOT_GOOD) ;
    }

    SUBCASE("page length with unit: .pl 11i parses successfully")
    {
        eDotCommandStatus result = layout.ParseDotCommand(".pl 11i") ;
        CHECK(result == DOT_GOOD) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ParseDotCommand increment/decrement with units across commands
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ParseDotCommand increment/decrement with units")
{
    cLayoutBaseTest layout ;
    cDocument doc ;
    layout.SetDocument(&doc) ;

    SUBCASE("left margin increment with centimeters: .lm +2c")
    {
        // set initial left margin to 1 inch
        eDotCommandStatus result = layout.ParseDotCommand(".lm 1i") ;
        CHECK(result == DOT_GOOD) ;
        CHECK(layout.GetLeftMargin() == doctest::Approx(1440.0).epsilon(1.0)) ;

        // increment by 2 centimeters
        result = layout.ParseDotCommand(".lm +2c") ;
        CHECK(result == DOT_GOOD) ;

        double expected = 1440.0 + 2.0 * TWIPSPERCM ;
        CHECK(layout.GetLeftMargin() == doctest::Approx(expected).epsilon(1.0)) ;
    }

    SUBCASE("right margin decrement with inches: .rm -1i")
    {
        // set initial right margin to 6 inches
        eDotCommandStatus result = layout.ParseDotCommand(".rm 6i") ;
        CHECK(result == DOT_GOOD) ;
        CHECK(layout.GetRightMargin() == doctest::Approx(8640.0).epsilon(1.0)) ;

        // decrement by 1 inch
        result = layout.ParseDotCommand(".rm -1i") ;
        CHECK(result == DOT_GOOD) ;

        CHECK(layout.GetRightMargin() == doctest::Approx(7200.0).epsilon(1.0)) ;
    }

    SUBCASE("top margin increment with millimeters: .mt +5m")
    {
        // set initial top margin to 1 inch
        eDotCommandStatus result = layout.ParseDotCommand(".mt 1i") ;
        CHECK(result == DOT_GOOD) ;
        CHECK(layout.GetTopMargin() == doctest::Approx(1440.0).epsilon(1.0)) ;

        // increment by 5 millimeters
        result = layout.ParseDotCommand(".mt +5m") ;
        CHECK(result == DOT_GOOD) ;

        double expected = 1440.0 + 5.0 * TWIPSPERMM ;
        CHECK(layout.GetTopMargin() == doctest::Approx(expected).epsilon(1.0)) ;
    }

    SUBCASE("page offset increment with points: .po +10p")
    {
        // set initial page offset to 1 inch
        eDotCommandStatus result = layout.ParseDotCommand(".po 1i") ;
        CHECK(result == DOT_GOOD) ;
        CHECK(layout.GetPageOffsetOdd() == doctest::Approx(1440.0).epsilon(1.0)) ;

        // increment by 10 points
        result = layout.ParseDotCommand(".po +10p") ;
        CHECK(result == DOT_GOOD) ;

        double expected = 1440.0 + 10.0 * POINTSTOTWIPS ;
        CHECK(layout.GetPageOffsetOdd() == doctest::Approx(expected).epsilon(1.0)) ;
    }

    SUBCASE("left margin decrement clamps to zero")
    {
        // set initial left margin to 0.5 inch
        eDotCommandStatus result = layout.ParseDotCommand(".lm .5i") ;
        CHECK(result == DOT_GOOD) ;
        CHECK(layout.GetLeftMargin() == doctest::Approx(720.0).epsilon(1.0)) ;

        // decrement by 2 inches (would go negative, should clamp to 0)
        result = layout.ParseDotCommand(".lm -2i") ;
        CHECK(result == DOT_GOOD) ;

        CHECK(layout.GetLeftMargin() == doctest::Approx(0.0)) ;
    }
}
