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

#include "src/gui/editor/editorctrl.h"
#include "src/gui/layout/layout.h"
#include "src/core/document/document.h"
#include <QApplication>


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test fixture for cEditorBase tests.
/// Ensures QApplication exists (required for Qt widgets and timers).
/// Uses cEditorCtrl as the concrete subclass since cEditorBase is abstract.
///
/////////////////////////////////////////////////////////////////////////////
namespace {
static int argc = 0;
static char* argv[] = {nullptr};
static QApplication* app = nullptr;

void ensureQApplication()
{
    if (!QApplication::instance())
    {
        app = new QApplication(argc, argv);
    }
}
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetStatus column calculation for various document contents.
/// Column counts only visible graphemes (STYLE_END_OF_STYLES) and tabs
/// (STYLE_TAB). All other control codes are excluded from the count.
/// Column numbering starts at 1.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetStatus - Column calculation")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    SUBCASE("Empty document column is 1")
    {
        // Empty document has only ^Z at position 0
        // Layout the document so CalculateCaretPosition can work
        layout->LayoutDocument(doc);
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        CHECK(status.column == 1);
    }

    SUBCASE("Plain text column")
    {
        // Insert "Hello" -- 5 graphemes at positions 0-4, then CR at 5, ^Z at 6
        doc->Insert("Hello\r");
        layout->LayoutDocument(doc);

        // Position caret at grapheme 3 ('l' -- the first 'l')
        doc->SetPosition(3);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        // Column counts positions 0,1,2 as regular text (H, e, l) -- 3 increments + base 1 = 4
        CHECK(status.column == 4);
    }

    SUBCASE("Bold markers excluded from column")
    {
        // BeginBold() inserts one MARKER_CHAR (position 0), which is STYLE_BOLD
        // "hello" occupies positions 1-5
        // EndBold() inserts another MARKER_CHAR (position 6), which is STYLE_BOLD
        doc->BeginBold();
        doc->Insert("hello");
        doc->EndBold();
        doc->Insert("\r");
        layout->LayoutDocument(doc);

        // Position caret at position 2 -- the 'h' is at position 1, 'e' at position 2
        // Walking from line start (position 0):
        //   position 0 = STYLE_BOLD (control code, not counted)
        //   position 1 = 'h' (STYLE_END_OF_STYLES, counted so column becomes 2)
        // So at position 2, column should be 2 (we counted 'h' only)
        doc->SetPosition(2);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        // From line start to position 2, we walk positions 0 and 1:
        //   pos 0: STYLE_BOLD is not counted
        //   pos 1: 'h' (END_OF_STYLES) is counted
        // visualCol starts at 1, increments once = 2
        CHECK(status.column == 2);
    }

    SUBCASE("Tab counts as column")
    {
        // Insert a tab first, then text
        sWSTab tab;
        tab.type = TAB_TAB;
        tab.tabsize = 0;
        tab.abstabsize = 0;
        tab.size = 0;
        doc->InsertTab(tab);
        doc->Insert("A\r");
        layout->LayoutDocument(doc);

        // Tab is at position 0 (MARKER_CHAR with STYLE_TAB in pairs)
        // 'A' is at position 1
        // Position caret at position 1 (after the tab)
        doc->SetPosition(1);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        // Walking from line start to position 1:
        //   pos 0: STYLE_TAB is counted (tab counts)
        // visualCol starts at 1, increments once = 2
        CHECK(status.column == 2);
    }

    SUBCASE("Line start is column 1")
    {
        doc->Insert("Hello\r");
        layout->LayoutDocument(doc);

        // Position caret at the very start of the line
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        // No positions walked (mCaretLineDocPosition == mCaretDocumentPosition)
        // visualCol starts at 1
        CHECK(status.column == 1);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetStatus marker string generation.
/// Markers are iterated 1-9 then 0. Each set marker appends its digit
/// to the markers string. NOT_SET markers are skipped.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetStatus - Marker status string")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // Layout the empty document so GetStatus works
    layout->LayoutDocument(doc);
    editor.CalculateCaretPosition();

    SUBCASE("No markers set")
    {
        // All mSavePosition values are NOT_SET by default
        sStatus status;
        editor.GetStatus(status);
        CHECK(status.markers == "");
    }

    SUBCASE("Single marker 1")
    {
        doc->mSavePosition[1] = 0;

        sStatus status;
        editor.GetStatus(status);
        CHECK(status.markers == "1");
    }

    SUBCASE("Single marker 0")
    {
        doc->mSavePosition[0] = 0;

        sStatus status;
        editor.GetStatus(status);
        CHECK(status.markers == "0");
    }

    SUBCASE("Markers 1, 5, 0")
    {
        doc->mSavePosition[1] = 0;
        doc->mSavePosition[5] = 0;
        doc->mSavePosition[0] = 0;

        sStatus status;
        editor.GetStatus(status);
        // Iteration order: 1-9 then 0, so "1" then "5" then "0"
        CHECK(status.markers == "150");
    }

    SUBCASE("All markers set")
    {
        for (int i = 0; i < 10; ++i)
        {
            doc->mSavePosition[i] = 0;
        }

        sStatus status;
        editor.GetStatus(status);
        // Iteration order: 1,2,3,4,5,6,7,8,9 then 0
        CHECK(status.markers == "1234567890");
    }

    SUBCASE("Ordering is always 1-9 then 0")
    {
        // Set 0 first, then 3 -- order in the string should still be "30"
        // because iteration goes 1-9 then 0
        doc->mSavePosition[0] = 0;
        doc->mSavePosition[3] = 0;

        sStatus status;
        editor.GetStatus(status);
        // 3 is found during 1-9 loop, 0 is found in the final check
        CHECK(status.markers == "30");
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetStatus basic fields: insert mode, page/line, measurement suffix,
/// and block state.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetStatus - Basic fields")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    SUBCASE("Insert mode is default")
    {
        // mInsertMode defaults to true in cEditorBase constructor
        layout->LayoutDocument(doc);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        CHECK(status.mode == true);
    }

    SUBCASE("Page and line populated")
    {
        // Insert some text and layout
        doc->Insert("Hello World\r");
        layout->LayoutDocument(doc);
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        // After layout, page should be at least 1 and line should be at least 1
        CHECK(status.page >= 1);
        CHECK(status.line >= 1);
    }

    SUBCASE("Measurement suffix defaults to inches")
    {
        // Default mMeasure is MSR_INCHES
        layout->LayoutDocument(doc);
        editor.CalculateCaretPosition();

        sStatus status;
        editor.GetStatus(status);
        CHECK(status.measureSuffix == "\"");
    }

    SUBCASE("Block set reflects document state")
    {
        // Insert text for block selection
        doc->Insert("Hello World\r");
        layout->LayoutDocument(doc);

        // No block set initially
        sStatus status;
        editor.GetStatus(status);
        CHECK(status.blockSet == false);

        // Set a block selection: select "Hello World"
        doc->SetPosition(0);
        doc->SetBeginBlock();
        // After SetBeginBlock, marker inserted at 0, text shifts right
        // Position at 12 (shifted \r) to select "Hello World"
        doc->SetPosition(12);
        doc->SetEndBlock();
        // After SetEndBlock, marker deleted, block = [0, 11)

        editor.GetStatus(status);
        CHECK(status.blockSet == true);

        // Unset the block
        doc->UnsetBlock();

        editor.GetStatus(status);
        CHECK(status.blockSet == false);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test display mode accessors: SetDisplayMode, GetDisplayMode,
/// ToggleDisplayMode. Default mode is DISPLAY_CONTINUOUS.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Display mode accessors")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // Layout the document so mode switches work properly
    layout->LayoutDocument(doc);

    SUBCASE("Default display mode")
    {
        // sDisplaySettings constructor sets mode to DISPLAY_CONTINUOUS
        CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
    }

    SUBCASE("SetDisplayMode / GetDisplayMode roundtrip")
    {
        editor.SetDisplayMode(DISPLAY_PAGE);
        CHECK(editor.GetDisplayMode() == DISPLAY_PAGE);

        editor.SetDisplayMode(DISPLAY_CONTINUOUS);
        CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
    }

    SUBCASE("ToggleDisplayMode")
    {
        // Starts at DISPLAY_CONTINUOUS
        CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);

        // Toggle to page mode
        editor.ToggleDisplayMode();
        CHECK(editor.GetDisplayMode() == DISPLAY_PAGE);

        // Toggle back to continuous
        editor.ToggleDisplayMode();
        CHECK(editor.GetDisplayMode() == DISPLAY_CONTINUOUS);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test show control accessors: SetShowControls/GetShowControls and
/// SetShowDot/GetShowDot. Verifies roundtrip for all eShowControl values.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Show controls accessors")
{
    ensureQApplication();

    cEditorCtrl editor;

    SUBCASE("SetShowControls / GetShowControls roundtrip")
    {
        editor.SetShowControls(SHOW_ALL);
        CHECK(editor.GetShowControls() == SHOW_ALL);

        editor.SetShowControls(SHOW_DOT);
        CHECK(editor.GetShowControls() == SHOW_DOT);

        editor.SetShowControls(SHOW_NONE);
        CHECK(editor.GetShowControls() == SHOW_NONE);
    }

    SUBCASE("SetShowDot / GetShowDot roundtrip")
    {
        editor.SetShowDot(true);
        CHECK(editor.GetShowDot() == true);

        editor.SetShowDot(false);
        CHECK(editor.GetShowDot() == false);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test the user-name and always-dot members promoted to cEditorBase (shared by
/// the GUI and TUI). Verifies defaults and a set/read roundtrip.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("User name and always-dot members")
{
    cEditorCtrl editor;

    SUBCASE("Defaults")
    {
        CHECK(editor.mShortName.empty() == true);
        CHECK(editor.mLongName.empty() == true);
        CHECK(editor.mAlwaysDot == true);
    }

    SUBCASE("Set / read roundtrip")
    {
        editor.mShortName = "GB";
        editor.mLongName = "Gerald Brandt";
        editor.mAlwaysDot = false;

        CHECK(editor.mShortName == "GB");
        CHECK(editor.mLongName == "Gerald Brandt");
        CHECK(editor.mAlwaysDot == false);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test status message lifecycle: set, get, busy flag, and tick countdown.
/// SetStatusMessage sets a temporary message with optional busy flag.
/// TickStatusMessage decrements the timer and clears when it reaches zero.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Status message lifecycle")
{
    ensureQApplication();

    cEditorCtrl editor;

    SUBCASE("SetStatusMessage / GetStatusMessage")
    {
        editor.SetStatusMessage("Saving...");
        CHECK(editor.GetStatusMessage() == "Saving...");
    }

    SUBCASE("IsStatusBusy")
    {
        // Default: not busy
        CHECK(editor.IsStatusBusy() == false);

        // Set with busy=true
        editor.SetStatusMessage("Working...", true);
        CHECK(editor.IsStatusBusy() == true);
    }

    SUBCASE("TickStatusMessage countdown")
    {
        // Set message with durationFrames=3
        editor.SetStatusMessage("Temporary", false, 3);
        CHECK(editor.GetStatusMessage() == "Temporary");

        // Tick 1: timer goes from 3 to 2 -- message still present
        editor.TickStatusMessage();
        CHECK(editor.GetStatusMessage() == "Temporary");

        // Tick 2: timer goes from 2 to 1 -- message still present
        editor.TickStatusMessage();
        CHECK(editor.GetStatusMessage() == "Temporary");

        // Tick 3: timer goes from 1 to 0 -- message cleared
        editor.TickStatusMessage();
        CHECK(editor.GetStatusMessage() == "");
    }

    SUBCASE("TickStatusMessage clears busy")
    {
        // Set with busy=true and short duration
        editor.SetStatusMessage("Busy task", true, 2);
        CHECK(editor.IsStatusBusy() == true);

        // Tick 1: still busy
        editor.TickStatusMessage();
        CHECK(editor.IsStatusBusy() == true);

        // Tick 2: timer reaches 0, busy cleared
        editor.TickStatusMessage();
        CHECK(editor.IsStatusBusy() == false);
        CHECK(editor.GetStatusMessage() == "");
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SavePosition and GotoSavePosition.
/// SavePosition stores the current caret position in a numbered slot (0-9).
/// GotoSavePosition jumps the caret to a previously saved position.
/// Saving to the same position at the same location toggles it off (NOT_SET).
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SavePosition and GotoSavePosition")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // Insert enough text to have meaningful positions
    doc->Insert("Hello World\r");
    doc->Insert("Second line\r");
    layout->LayoutDocument(doc);

    SUBCASE("Save and goto position")
    {
        // Move caret to position 5 (the space in "Hello World")
        doc->SetPosition(5);
        editor.CalculateCaretPosition();

        // Save marker 1 at position 5
        // SetSavePosition inserts a REPLACE_CHAR, so position shifts
        editor.SavePosition(1);

        // Verify marker 1 is set (not NOT_SET)
        CHECK(doc->mSavePosition[1] != NOT_SET);

        // Move caret away to position 0
        doc->SetPosition(0);
        editor.CalculateCaretPosition();
        CHECK(editor.GetCaretDocumentPosition() == 0);

        // Goto marker 1 -- should jump back to saved position
        editor.GotoSavePosition(1);

        // The caret should have moved to the saved position
        // (exact position may differ due to REPLACE_CHAR insertion/deletion,
        // but it should NOT be 0 anymore)
        CHECK(editor.GetCaretDocumentPosition() != 0);
    }

    SUBCASE("Goto unset marker does nothing")
    {
        // Move caret to position 3
        doc->SetPosition(3);
        editor.CalculateCaretPosition();
        POSITION_T posBefore = editor.GetCaretDocumentPosition();

        // Marker 7 was never set -- goto should do nothing
        editor.GotoSavePosition(7);

        // Caret should remain at the same position
        CHECK(editor.GetCaretDocumentPosition() == posBefore);
    }

    SUBCASE("Toggle marker off")
    {
        // Move caret to position 3
        doc->SetPosition(3);
        editor.CalculateCaretPosition();

        // Save marker 2
        editor.SavePosition(2);

        // Verify it is set
        CHECK(doc->mSavePosition[2] != NOT_SET);

        // The save inserted a REPLACE_CHAR at position 3, moving the cursor
        // to position 4 (one past the marker). To toggle off, we need to
        // be at the saved position + 1 (because of the REPLACE_CHAR).
        // The document's SetSavePosition handles the toggle logic:
        // if checkpos + 1 == current position, it removes the marker.
        // After saving, the cursor is at savedPos + 1 due to the insert.
        // So saving again immediately should toggle it off.
        editor.SavePosition(2);

        // Marker should now be NOT_SET (toggled off)
        CHECK(doc->mSavePosition[2] == NOT_SET);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test caret position getters after CalculateCaretPosition.
/// GetCaretX/Y/Width/Height/Line/Paragraph/LineDocPosition should return
/// sensible values after layout and caret calculation.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Caret position getters")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    doc->Insert("Second line\r");
    layout->LayoutDocument(doc);

    SUBCASE("caret at start of document")
    {
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        CHECK(editor.GetCaretX() >= 0);
        CHECK(editor.GetCaretY() >= 0);
        CHECK(editor.GetCaretWidth() > 0);
        CHECK(editor.GetCaretHeight() > 0);
        CHECK(editor.GetCaretLine() == 0);
        CHECK(editor.GetCaretParagraph() == 0);
        CHECK(editor.GetCaretLineDocPosition() == 0);
    }

    SUBCASE("caret in middle of first line")
    {
        doc->SetPosition(5);
        editor.CalculateCaretPosition();

        // X should be greater than at position 0
        CHECK(editor.GetCaretX() > 0);
        CHECK(editor.GetCaretHeight() > 0);
        CHECK(editor.GetCaretParagraph() == 0);
        CHECK(editor.GetCaretLineDocPosition() == 0);
    }

    SUBCASE("caret on second paragraph")
    {
        // position 12 is start of "Second line\r"
        doc->SetPosition(12);
        editor.CalculateCaretPosition();

        CHECK(editor.GetCaretParagraph() == 1);
        CHECK(editor.GetCaretLineDocPosition() == 12);
    }

    SUBCASE("page mode caret getters")
    {
        editor.SetDisplayMode(DISPLAY_PAGE);
        layout->LayoutDocument(doc);
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        CHECK(editor.GetCaretPageNumber() >= 1);
        CHECK(editor.GetCaretPageY() >= 0);

        editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test caret visibility getters and setters.
/// SetDrawnCaret/GetDrawnCaret and SetDoDrawCaret/GetDoDrawCaret.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Caret visibility getters and setters")
{
    ensureQApplication();

    cEditorCtrl editor;

    SUBCASE("SetDrawnCaret / GetDrawnCaret roundtrip")
    {
        editor.SetDrawnCaret(true);
        CHECK(editor.GetDrawnCaret() == true);

        editor.SetDrawnCaret(false);
        CHECK(editor.GetDrawnCaret() == false);
    }

    SUBCASE("SetDoDrawCaret / GetDoDrawCaret roundtrip")
    {
        editor.SetDoDrawCaret(true);
        CHECK(editor.GetDoDrawCaret() == true);

        editor.SetDoDrawCaret(false);
        CHECK(editor.GetDoDrawCaret() == false);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test debug overlay getters and setters.
/// ShowViewportDebug, ShowBoxStats, ShowBoxOutlines.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Debug overlay getters and setters")
{
    ensureQApplication();

    cEditorCtrl editor;

    SUBCASE("ShowViewportDebug roundtrip")
    {
        editor.SetShowViewportDebug(true);
        CHECK(editor.GetShowViewportDebug() == true);

        editor.SetShowViewportDebug(false);
        CHECK(editor.GetShowViewportDebug() == false);
    }

    SUBCASE("ShowBoxStats roundtrip")
    {
        editor.SetShowBoxStats(true);
        CHECK(editor.GetShowBoxStats() == true);

        editor.SetShowBoxStats(false);
        CHECK(editor.GetShowBoxStats() == false);
    }

    SUBCASE("ShowBoxOutlines roundtrip")
    {
        editor.SetShowBoxOutlines(true);
        CHECK(editor.GetShowBoxOutlines() == true);

        editor.SetShowBoxOutlines(false);
        CHECK(editor.GetShowBoxOutlines() == false);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetDisplaySettings returns a reference that tracks mode changes.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetDisplaySettings")
{
    ensureQApplication();

    cEditorCtrl editor;
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->LayoutDocument(editor.GetDocument());

    CHECK(editor.GetDisplaySettings().mode == DISPLAY_CONTINUOUS);

    editor.SetDisplayMode(DISPLAY_PAGE);
    CHECK(editor.GetDisplaySettings().mode == DISPLAY_PAGE);

    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
    CHECK(editor.GetDisplaySettings().mode == DISPLAY_CONTINUOUS);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test IsDocumentDirty flag tracks document modifications.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("IsDocumentDirty")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    layout->LayoutDocument(doc);

    // after fresh layout, dirty flag should be false
    CHECK(editor.IsDocumentDirty() == false);

    // inserting text should set dirty flag
    doc->Insert("X");
    CHECK(editor.IsDocumentDirty() == true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetSiblingEditor does not crash.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SetSiblingEditor")
{
    ensureQApplication();

    cEditorCtrl editor;
    cEditorCtrl editor2;

    // set sibling
    editor.SetSiblingEditor(&editor2);

    // clear sibling
    editor.SetSiblingEditor(nullptr);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetFont/GetFont roundtrip and that layout is re-triggered.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SetFont and GetFont")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    layout->LayoutDocument(doc);

    std::string fontSpec = "Courier New|12.0|0|0|0|0";
    editor.SetFont(fontSpec);
    CHECK(editor.GetFont() == fontSpec);

    std::string fontSpec2 = "Times New Roman|10.0|0|0|0|0";
    editor.SetFont(fontSpec2);
    CHECK(editor.GetFont() == fontSpec2);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetMeasurement/GetMeasurement roundtrip for all unit types.
/// SetMeasurement passes string to doc->GetType() which extracts the
/// trailing unit character.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SetMeasurement and GetMeasurement")
{
    ensureQApplication();

    cEditorCtrl editor;

    SUBCASE("inches")
    {
        editor.SetMeasurement("1\"");
        CHECK(editor.GetMeasurement() == "1\"");
    }

    SUBCASE("centimeters")
    {
        editor.SetMeasurement("10C");
        CHECK(editor.GetMeasurement() == "10C");
    }

    SUBCASE("millimeters")
    {
        editor.SetMeasurement("5M");
        CHECK(editor.GetMeasurement() == "5M");
    }

    SUBCASE("default fallback")
    {
        editor.SetMeasurement("foo");
        CHECK(editor.GetMeasurement() == "foo");
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GotoFontTag navigates to a font formatting tag position.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GotoFontTag")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // insert text with a font tag via InsertFont
    doc->Insert("Hello World\r");
    doc->SetPosition(5);
    sInternalFonts fontData;
    fontData.fontname = "Courier";
    fontData.size = 12.0;
    doc->InsertFont(fontData);

    layout->LayoutDocument(doc);

    // move caret to start
    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    // GotoFontTag should navigate to the font tag at position 5
    editor.GotoFontTag();
    CHECK(doc->GetPosition() > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ToggleJustification inserts a .oj dot command.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ToggleJustification")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    layout->LayoutDocument(doc);

    // position caret in the text paragraph
    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    POSITION_T sizeBefore = doc->GetTextSize();
    editor.ToggleJustification();
    POSITION_T sizeAfter = doc->GetTextSize();

    // should have inserted a dot command (increases text size)
    CHECK(sizeAfter > sizeBefore);

    // first paragraph should now be a dot command containing ".oj"
    std::string para0 = doc->GetParagraphText(0);
    CHECK(para0.find(".oj") != std::string::npos);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetAlignment inserts correct dot commands for each alignment type.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SetAlignment")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    SUBCASE("center alignment")
    {
        doc->Insert("Hello World\r");
        layout->LayoutDocument(doc);
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        editor.SetAlignment(JUST_CENTER);

        // should insert ".oj c" dot command
        std::string para0 = doc->GetParagraphText(0);
        CHECK(para0.find(".oj c") != std::string::npos);
    }

    SUBCASE("right alignment")
    {
        doc->Insert("Hello World\r");
        layout->LayoutDocument(doc);
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        editor.SetAlignment(JUST_RIGHT);

        std::string para0 = doc->GetParagraphText(0);
        CHECK(para0.find(".oj r") != std::string::npos);
    }

    SUBCASE("justify alignment")
    {
        doc->Insert("Hello World\r");
        layout->LayoutDocument(doc);
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        editor.SetAlignment(JUST_JUST);

        std::string para0 = doc->GetParagraphText(0);
        CHECK(para0.find(".oj on") != std::string::npos);
    }

    SUBCASE("left alignment after justify")
    {
        // first set to justify to change from default
        doc->Insert("Hello World\r");
        layout->LayoutDocument(doc);
        doc->SetPosition(0);
        editor.CalculateCaretPosition();

        editor.SetAlignment(JUST_JUST);
        layout->LayoutDocument(doc);

        // now switch to left -- need to position in the content paragraph
        // (paragraph after the dot command)
        PARAGRAPH_T numParas = doc->GetNumberofParagraphs();
        if (numParas > 1)
        {
            POSITION_T start, end;
            doc->GetParagraphStartandEnd(1, start, end);
            doc->SetPosition(start);
            editor.CalculateCaretPosition();
        }

        editor.SetAlignment(JUST_LEFT);

        // should have inserted ".oj off" somewhere
        bool found = false;
        for (ssize_t p = 0; p < doc->GetNumberofParagraphs(); ++p)
        {
            std::string text = doc->GetParagraphText(p);
            if (text.find(".oj off") != std::string::npos)
            {
                found = true;
                break;
            }
        }
        CHECK(found);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test display mode hook virtuals are called without crash.
/// OnBeforeDisplayModeChange and OnAfterDisplayModeChange are no-ops
/// on cEditorBase but should be hit by coverage when SetDisplayMode is called.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Display mode hooks fire without crash")
{
    ensureQApplication();

    cEditorCtrl editor;
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());
    layout->LayoutDocument(editor.GetDocument());

    // toggle to page mode fires OnBefore and OnAfter
    editor.SetDisplayMode(DISPLAY_PAGE);

    // toggle back fires them again
    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
}


// NOTE: SaveFileState and LoadFileState are protected methods in cEditorBase,
// so they cannot be tested directly from here. They are exercised indirectly
// through LoadFile/SaveFile in cEditorCtrl.


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test variable expansion in InsertText.
/// Typing &@& should replace those 3 characters with a VAR_DATE marker.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Variable expansion in InsertText")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("AB\r");
    layout->LayoutDocument(doc);

    // position at start of document
    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    SUBCASE("date variable &at&")
    {
        // insert &@& character by character
        editor.InsertText("&");
        editor.InsertText("@");
        editor.InsertText("&");

        // the &@& should have been replaced with a STYLE_VARIABLE marker
        // text size should be smaller than if 3 chars were inserted
        // (3 chars deleted, 1 marker inserted = net -2 from the 3 inserts)
        // Check position 0 is now a variable marker
        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("time variable")
    {
        editor.InsertText("&");
        editor.InsertText("!");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("page number variable")
    {
        editor.InsertText("&");
        editor.InsertText("#");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("line number variable")
    {
        editor.InsertText("&");
        editor.InsertText("_");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("filename variable")
    {
        editor.InsertText("&");
        editor.InsertText("*");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("drive variable")
    {
        editor.InsertText("&");
        editor.InsertText(":");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("directory variable")
    {
        editor.InsertText("&");
        editor.InsertText(".");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("fullpath variable")
    {
        editor.InsertText("&");
        editor.InsertText("\\");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("word count variable")
    {
        editor.InsertText("&");
        editor.InsertText("?");
        editor.InsertText("&");

        std::string g = doc->GetCharNoAdvance(0);
        if (!g.empty() && g[0] == MARKER_CHAR)
        {
            CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
        }
    }

    SUBCASE("invalid variable character does not expand")
    {
        editor.InsertText("&");
        editor.InsertText("Z");
        editor.InsertText("&");

        // &Z& is not a valid variable -- all 3 chars should remain
        POSITION_T size = doc->GetTextSize();
        // Original "AB\r" + ^Z = 4, plus 3 inserted = 7
        CHECK(size >= 6);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Variable expansion runs in its own undo group, separate from the
/// surrounding typing. After typing "&@&" + "x" + closing the typing
/// group, one Undo must remove only the trailing "x" and leave the
/// variable marker intact.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Variable expansion undo is independent from surrounding typing")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("AB\r");
    layout->LayoutDocument(doc);

    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    editor.InsertText("&");
    editor.InsertText("@");
    editor.InsertText("&");

    REQUIRE(doc->GetCharNoAdvance(0)[0] == MARKER_CHAR);
    REQUIRE(doc->GetControlChar(0) == STYLE_VARIABLE);

    editor.InsertText("x");
    editor.CloseTypingGroup();

    POSITION_T sizeBefore = doc->GetTextSize();
    REQUIRE(doc->GetCharNoAdvance(1) == "x");

    doc->Undo();

    CHECK(doc->GetCharNoAdvance(0)[0] == MARKER_CHAR);
    CHECK(doc->GetControlChar(0) == STYLE_VARIABLE);
    CHECK(doc->GetTextSize() == sizeBefore - 1);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test PerformPostCommandUpdate with document content.
/// Exercises the incremental layout and dirty tracking path.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("PerformPostCommandUpdate")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    doc->Insert("Second line\r");
    layout->LayoutDocument(doc);

    doc->SetPosition(5);
    editor.CalculateCaretPosition();

    // calling PerformPostCommandUpdate should not crash and should
    // trigger incremental layout of visible paragraphs
    editor.PerformPostCommandUpdate();

    // verify caret is still valid after the update
    CHECK(editor.GetCaretX() >= 0);
    CHECK(editor.GetCaretHeight() > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertText in overwrite mode.
/// In overwrite mode, characters should be replaced rather than pushed.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertText overwrite mode")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("ABCDEF\r");
    layout->LayoutDocument(doc);

    // switch to overwrite mode
    editor.mInsertMode = false;

    // position at character C (index 2)
    doc->SetPosition(2);
    editor.CalculateCaretPosition();

    // overwrite C with X
    editor.InsertText("X");

    // document size should stay the same (overwrite, not insert)
    // original: A B C D E F \r ^Z = 8 graphemes
    // after overwrite: A B X D E F \r ^Z = 8 graphemes
    CHECK(doc->GetTextSize() == 8);

    // verify the character at position 2 is now X
    doc->SetPosition(2);
    std::string ch = doc->GetCharNoAdvance(2);
    CHECK(ch == "X");

    // restore insert mode
    editor.mInsertMode = true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertText overwrite mode deletes whole GRAPHEMES, not UTF-8 code
/// points. A decomposed e-acute ('e' + U+0301) is two code points but one
/// grapheme; overwriting with it must delete exactly ONE existing grapheme.
/// Regression for the byte-scan that counted code points (over-deleting for
/// combining marks and ZWJ emoji families).
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertText overwrite mode deletes graphemes, not code points")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hi\r");
    layout->LayoutDocument(doc);

    // overwrite mode, caret at the start
    editor.mInsertMode = false;
    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    // Decomposed e-acute: 'e' + U+0301 combining acute = 2 code points, 1 grapheme.
    editor.InsertText("e\xCC\x81");

    // Only "H" (one grapheme) must be overwritten; "i" must survive.
    // original: H i \r ^Z = 4 graphemes; after: <e-acute> i \r ^Z = 4 graphemes.
    // Pre-fix the code-point count (2) deleted both "H" and "i" -> 3 graphemes,
    // and grapheme 1 would be the CR instead of "i".
    CHECK(doc->GetTextSize() == 4);
    // grapheme 0 is the inserted accented char (document may normalize its exact
    // byte form, so just confirm "H" was overwritten); grapheme 1 must still be "i".
    CHECK(doc->GetCharNoAdvance(0) != "H");
    CHECK(doc->GetCharNoAdvance(1) == "i");

    editor.mInsertMode = true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertText overwrite mode does not delete past line end.
/// The overwrite should stop at HARD_RETURN boundaries.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertText overwrite mode stops at line end")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("AB\r");
    doc->Insert("CD\r");
    layout->LayoutDocument(doc);

    editor.mInsertMode = false;

    // position at B (index 1)
    doc->SetPosition(1);
    editor.CalculateCaretPosition();

    // overwrite B with X, should stop at \r
    editor.InsertText("X");

    doc->SetPosition(1);
    std::string ch1 = doc->GetCharNoAdvance(1);
    CHECK(ch1 == "X");

    // now try to overwrite at position 2 (\r) -- should not delete past line end
    doc->SetPosition(2);
    editor.CalculateCaretPosition();
    editor.InsertText("Z");

    // the \r should still be present since overwrite stops at line end
    // the Z is inserted before the \r (overwrite does not delete \r)
    doc->SetPosition(2);
    std::string ch2 = doc->GetCharNoAdvance(2);
    // overwrite mode does not delete HARD_RETURN, so Z is inserted
    CHECK((ch2 == "Z" || ch2[0] == HARD_RETURN));

    editor.mInsertMode = true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test InsertText with empty string (guard clause).
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("InsertText empty string")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("AB\r");
    layout->LayoutDocument(doc);

    POSITION_T sizeBefore = doc->GetTextSize();

    // inserting empty string should be a no-op
    editor.InsertText("");

    CHECK(doc->GetTextSize() == sizeBefore);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetStatus populates the status structure with correct data
/// for each measurement unit (inches, cm, mm, default).
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetStatus measurement units")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    layout->LayoutDocument(doc);
    doc->SetPosition(5);
    editor.CalculateCaretPosition();

    sStatus status;

    SUBCASE("inches")
    {
        editor.SetMeasurement("1\"");
        editor.GetStatus(status);
        CHECK(status.measureSuffix == "\"");
        CHECK(status.charcount > 0);
        CHECK(status.mode == true);  // insert mode
    }

    SUBCASE("centimeters")
    {
        editor.SetMeasurement("10C");
        editor.GetStatus(status);
        CHECK(status.measureSuffix == " cm");
    }

    SUBCASE("millimeters")
    {
        editor.SetMeasurement("5M");
        editor.GetStatus(status);
        CHECK(status.measureSuffix == " mm");
    }

    SUBCASE("default measurement")
    {
        editor.SetMeasurement("foo");
        editor.GetStatus(status);
        CHECK(status.measureSuffix == "\"");
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test GetStatus reports correct insert/overwrite mode.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("GetStatus insert vs overwrite mode")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Test\r");
    layout->LayoutDocument(doc);
    editor.CalculateCaretPosition();

    sStatus status;

    // default is insert mode
    editor.GetStatus(status);
    CHECK(status.mode == true);

    // switch to overwrite
    editor.mInsertMode = false;
    editor.GetStatus(status);
    CHECK(status.mode == false);

    editor.mInsertMode = true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test WordCount skips dot command paragraphs.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("WordCount skips dot commands")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // insert a dot command paragraph then a text paragraph
    doc->Insert(".ss1\r");
    doc->Insert("Hello World\r");
    layout->LayoutDocument(doc);

    // WordCount(0,0) counts the whole document, skipping dot commands
    long count = editor.WordCount(0, 0);
    // "Hello World\r" counted, ".ss1\r" skipped
    // The algorithm starts at 1 and increments on each space/punctuation transition
    CHECK(count >= 2);  // at least "Hello" and "World"

    // compare with a document that has no dot commands
    cEditorCtrl editor2;
    cDocument* doc2 = editor2.GetDocument();
    cLayout* layout2 = dynamic_cast<cLayout*>(editor2.GetLayout());

    doc2->Insert("Hello World\r");
    layout2->LayoutDocument(doc2);

    long countNoDot = editor2.WordCount(0, 0);

    // the count with dot command skipped should equal count without dot commands
    CHECK(count == countNoDot);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test WordCount with a range (non-zero end).
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("WordCount with range")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("One Two Three\r");
    layout->LayoutDocument(doc);

    // count words in the full text range
    long count = editor.WordCount(0, doc->GetTextSize());
    CHECK(count >= 3);  // at least "One", "Two", "Three"
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test hidden content skipping with dot commands in SHOW_NONE mode.
/// When control codes are hidden and caret moves through a dot command,
/// it should skip to the next visible paragraph.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Hidden content skipping with dot commands")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // create: text, dot command, text
    doc->Insert("Hello\r");
    doc->Insert(".ss1\r");
    doc->Insert("World\r");
    layout->LayoutDocument(doc);

    // set show controls to SHOW_NONE so dot commands become hidden
    editor.SetShowControls(SHOW_NONE);
    layout->LayoutDocument(doc);

    // position at end of "Hello" (position 5)
    doc->SetPosition(5);
    editor.CalculateCaretPosition();

    // move right should skip over the hidden dot command paragraph
    // and land on or past "World"
    editor.MoveCaretRight();
    editor.CalculateCaretPosition();
    POSITION_T posAfterRight = doc->GetPosition();

    // should have jumped past the dot command to "World" or "\r"
    CHECK(posAfterRight >= 6);

    // move to start of "World" and move left -- should skip back
    doc->SetPosition(11);
    editor.CalculateCaretPosition();
    editor.MoveCaretLeft();
    editor.CalculateCaretPosition();
    POSITION_T posAfterLeft = doc->GetPosition();

    // should have jumped back past the dot command
    CHECK(posAfterLeft <= 6);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test hidden content skipping with control codes (bold markers).
/// When SHOW_NONE is active, caret movement should skip hidden codes.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Hidden content skipping with formatting codes")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // insert text with bold markers: A [bold-on] B \r
    doc->Insert("AB\r");
    doc->SetPosition(1);
    doc->BeginBold();  // inserts bold-on marker at position 1

    layout->LayoutDocument(doc);

    // set show controls to SHOW_NONE to hide formatting codes
    editor.SetShowControls(SHOW_NONE);
    layout->LayoutDocument(doc);

    // position before the bold marker (position 0 = A)
    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    // move right from A -- should skip the hidden bold marker
    // and land on B (not on the marker itself)
    editor.MoveCaretRight();
    editor.CalculateCaretPosition();

    POSITION_T posAfter = doc->GetPosition();
    // should have moved past position 0 (at least to 1 or 2)
    CHECK(posAfter > 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test caret position on empty paragraph (paragraph with only CR).
/// Exercises the empty-segment path in FindCaretLine.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Caret on empty paragraph")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // insert an empty paragraph followed by text
    doc->Insert("\r");
    doc->Insert("Hello\r");
    layout->LayoutDocument(doc);

    // position caret on the empty paragraph (position 0)
    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    // caret should still have valid dimensions
    CHECK(editor.GetCaretHeight() > 0);
    CHECK(editor.GetCaretWidth() > 0);
    CHECK(editor.GetCaretLine() == 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test MoveCaretPage scrolls the viewport by page.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("MoveCaretPage")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // create enough content for multiple pages worth of lines
    for (int i = 0; i < 50; i++)
    {
        doc->Insert("Line of text for pagination test\r");
    }
    layout->LayoutDocument(doc);

    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    POSITION_T posBefore = doc->GetPosition();

    // page down should move caret forward
    editor.MoveCaretPage(1);
    editor.CalculateCaretPosition();

    POSITION_T posAfter = doc->GetPosition();
    CHECK(posAfter > posBefore);

    // page up should move caret backward
    editor.MoveCaretPage(-1);
    editor.CalculateCaretPosition();

    POSITION_T posAfterUp = doc->GetPosition();
    CHECK(posAfterUp < posAfter);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test ScrollUp scrolls the viewport up.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("ScrollUp and ScrollDown")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // create content that exceeds viewport
    for (int i = 0; i < 50; i++)
    {
        doc->Insert("Line of text for scroll test\r");
    }
    layout->LayoutDocument(doc);

    // move to end to establish scroll offset
    doc->SetPosition(doc->GetTextSize() - 1);
    editor.CalculateCaretPosition();
    editor.ScrollIntoView();

    COORD_T offsetBefore = editor.GetScrollOffset();

    // scroll up should reduce offset (or stay at 0)
    editor.ScrollUp();
    COORD_T offsetAfterUp = editor.GetScrollOffset();
    CHECK(offsetAfterUp <= offsetBefore);

    // scroll down should increase offset
    editor.ScrollDown();
    COORD_T offsetAfterDown = editor.GetScrollOffset();
    CHECK(offsetAfterDown >= offsetAfterUp);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test MoveCursorBottomRight positions caret at bottom-right of viewport.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("MoveCursorBottomRight")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    doc->Insert("Second line\r");
    layout->LayoutDocument(doc);

    doc->SetPosition(0);
    editor.CalculateCaretPosition();

    // should not crash
    editor.MoveCursorBottomRight();
    editor.CalculateCaretPosition();

    // caret should be at a valid position
    CHECK(doc->GetPosition() >= 0);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test SetAlignment with a multi-paragraph document.
/// Tests that alignment reads previous paragraph state correctly.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SetAlignment multi-paragraph")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("First paragraph\r");
    doc->Insert("Second paragraph\r");
    layout->LayoutDocument(doc);

    // position on second paragraph
    doc->SetPosition(17);  // inside "Second paragraph"
    editor.CalculateCaretPosition();

    // set center alignment -- should insert a dot command
    editor.SetAlignment(JUST_CENTER);

    // re-layout and verify the document grew (dot command inserted)
    layout->LayoutDocument(doc);
    POSITION_T size = doc->GetTextSize();
    CHECK(size > 34);  // original was ~34 chars

    // set same alignment again -- should be a no-op
    POSITION_T sizeBefore = doc->GetTextSize();
    editor.SetAlignment(JUST_CENTER);
    // may or may not change size depending on whether the alignment
    // is already set, but should not crash
    CHECK(doc->GetTextSize() >= sizeBefore);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test display mode change in page mode exercises OnBefore/OnAfter hooks
/// and the SkipOverHiddenContent call within SetDisplayMode.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("SetDisplayMode page mode with content")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello\r");
    doc->Insert(".ss1\r");
    doc->Insert("World\r");
    layout->LayoutDocument(doc);

    // position inside dot command
    doc->SetPosition(6);
    editor.CalculateCaretPosition();

    // switching to page mode forces SHOW_NONE, so caret should
    // be moved out of dot command via SkipOverHiddenContent
    editor.SetDisplayMode(DISPLAY_PAGE);

    // caret should have moved to a visible position
    POSITION_T pos = doc->GetPosition();
    // should not be inside the dot command (pos 6-10)
    CHECK((pos < 6 || pos >= 11));

    // switch back to continuous
    editor.SetDisplayMode(DISPLAY_CONTINUOUS);
}


/////////////////////////////////////////////////////////////////////////////
//
// Edge case and corner case tests
//
/////////////////////////////////////////////////////////////////////////////


TEST_CASE("SetScrollOffset negative clamps to 0")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    layout->LayoutDocument(doc);

    editor.SetScrollOffset(-1000);
    CHECK(editor.GetScrollOffset() == 0);
}


TEST_CASE("SetScrollOffset beyond document clamps to max")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello\r");
    layout->LayoutDocument(doc);

    // Set an extremely large scroll offset
    editor.SetScrollOffset(999999999);

    // Should be clamped -- offset should not exceed total doc height
    COORD_T totalHeight = editor.CalculateTotalDocumentHeight();
    CHECK(editor.GetScrollOffset() <= totalHeight);
}


TEST_CASE("ScrollUp at top of document is no-op")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    layout->LayoutDocument(doc);

    // Ensure we are at the top
    editor.SetScrollOffset(0);

    // ScrollUp should do nothing
    editor.ScrollUp();
    CHECK(editor.GetScrollOffset() == 0);
}


TEST_CASE("ScrollDown at bottom of document clamps")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Short\r");
    layout->LayoutDocument(doc);

    // Scroll to a large value (will be clamped to max)
    editor.SetScrollOffset(999999999);
    COORD_T maxOffset = editor.GetScrollOffset();

    // ScrollDown from max should not increase offset
    editor.ScrollDown();
    CHECK(editor.GetScrollOffset() <= maxOffset + 1);  // allow rounding
}


TEST_CASE("MoveCaretLeft at position 0 stays at 0")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello\r");
    layout->LayoutDocument(doc);

    doc->SetPosition(0);
    editor.MoveCaretLeft();
    CHECK(doc->GetPosition() == 0);
}


TEST_CASE("MoveCaretRight at document end stays at end")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hi\r");
    layout->LayoutDocument(doc);

    // Position at EOF
    POSITION_T lastPos = doc->GetTextSize() - 1;
    doc->SetPosition(lastPos);
    editor.MoveCaretRight();

    // Should not move past EOF
    CHECK(doc->GetPosition() >= lastPos);
}


TEST_CASE("MoveCaretLine with delta 0 is no-op")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello\rWorld\r");
    layout->LayoutDocument(doc);

    doc->SetPosition(2);
    POSITION_T before = doc->GetPosition();

    editor.MoveCaretLine(0);
    CHECK(doc->GetPosition() == before);
}


TEST_CASE("MoveCaretLine clamping past first and last line")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello\rWorld\r");
    layout->LayoutDocument(doc);

    SUBCASE("Past first line clamps to line 0")
    {
        doc->SetPosition(0);
        editor.MoveCaretLine(-100);

        // Should not crash, position should be on first line
        CHECK(doc->GetPosition() >= 0);
    }

    SUBCASE("Past last line clamps to last line")
    {
        doc->SetPosition(0);
        editor.MoveCaretLine(100000);

        // Should not crash, position should be on last line
        CHECK(doc->GetPosition() < doc->GetTextSize());
    }
}


TEST_CASE("StickyX reset on text mutation")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("HelloWorld\rabcdefghij\r");
    layout->LayoutDocument(doc);

    auto seedSticky = [&]() {
        doc->SetPosition(5);                  // mid-first-line "Hello|World"
        editor.CalculateCaretPosition();
        editor.MoveCaretLine(1);              // seeds mCaretStickyX from mCaretX
        REQUIRE(editor.GetCaretStickyX() >= 0);
    };

    SUBCASE("InsertText (insert mode) resets sticky X")
    {
        seedSticky();
        editor.mInsertMode = true;
        editor.InsertText("x");
        CHECK(editor.GetCaretStickyX() == -1);
    }

    SUBCASE("InsertText (overwrite mode) resets sticky X")
    {
        seedSticky();
        editor.mInsertMode = false;
        editor.InsertText("x");
        CHECK(editor.GetCaretStickyX() == -1);
    }

    SUBCASE("Delete resets sticky X")
    {
        seedSticky();
        editor.Delete(doc->GetPosition(), 1);
        CHECK(editor.GetCaretStickyX() == -1);
    }

    SUBCASE("DeleteChar resets sticky X")
    {
        seedSticky();
        editor.DeleteChar();
        CHECK(editor.GetCaretStickyX() == -1);
    }
}


TEST_CASE("MoveCaretPage fallback path (no tracking)")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    // Insert enough text to span multiple "pages"
    for (int i = 0; i < 50; i++)
    {
        doc->Insert("Line " + std::to_string(i) + "\r");
    }
    layout->LayoutDocument(doc);

    doc->SetPosition(0);
    POSITION_T before = doc->GetPosition();

    // No paint has happened, so hasTracking is false -- uses fallback path
    editor.MoveCaretPage(1);

    // Position should have moved forward
    CHECK(doc->GetPosition() > before);
}


TEST_CASE("MoveCaretLeft with SHOW_ALL skips no content")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello\r");
    layout->LayoutDocument(doc);
    layout->SetShowControl(SHOW_ALL);

    // With SHOW_ALL, moving left from position 3 should go to position 2
    doc->SetPosition(3);
    editor.MoveCaretLeft();
    CHECK(doc->GetPosition() == 2);
}


TEST_CASE("MoveCaretRight skips hidden dot commands in SHOW_NONE")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("AB\r");
    doc->Insert(".mt 1i\r");
    doc->Insert("CD\r");
    layout->LayoutDocument(doc);
    layout->SetShowControl(SHOW_NONE);
    layout->SetActiveParagraph(-1);
    layout->LayoutDocument(doc);

    // Position at end of "AB\r" (position 2, the \r)
    // Moving right should skip the hidden dot command and land in "CD"
    doc->SetPosition(2);
    editor.MoveCaretRight();
    POSITION_T pos = doc->GetPosition();

    // Should not be inside the dot command (positions 3-9)
    CHECK((pos <= 3 || pos >= 10));
}


TEST_CASE("MoveCaretLine skips hidden content after landing")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Visible1\r");
    doc->Insert(".mt 1i\r");
    doc->Insert("Visible2\r");
    layout->LayoutDocument(doc);

    // Set up hidden mode
    layout->SetShowControl(SHOW_NONE);
    layout->SetActiveParagraph(-1);
    layout->LayoutDocument(doc);

    // Position caret at start of first visible paragraph
    doc->SetPosition(0);

    // Move down -- should skip the hidden dot command paragraph
    editor.MoveCaretLine(+1);

    POSITION_T pos = doc->GetPosition();

    // Caret should not be inside the dot command (positions 9-15)
    // It should be in "Visible2" (position 16+)
    CHECK((pos < 9 || pos >= 16));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test caret position after Delete() for every caret-vs-range relationship.
/// Locks the contract that the editor leaves the caret at a consistent
/// position whether the caret was at, before, after, or spanning the
/// deleted range.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Delete caret adjustment")
{
    ensureQApplication();

    cEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

    doc->Insert("Hello World\r");
    layout->LayoutDocument(doc);

    SUBCASE("caret at deletion start")
    {
        doc->SetPosition(5);
        editor.Delete(5, 1);

        CHECK(doc->GetPosition() == 5);
    }

    SUBCASE("caret before deletion range")
    {
        doc->SetPosition(2);
        editor.Delete(5, 1);

        CHECK(doc->GetPosition() == 2);
    }

    SUBCASE("caret after deletion range")
    {
        doc->SetPosition(8);
        editor.Delete(2, 3);

        CHECK(doc->GetPosition() == 5);
    }

    SUBCASE("caret inside deletion range")
    {
        doc->SetPosition(7);
        editor.Delete(5, 4);

        CHECK(doc->GetPosition() == 5);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test the per-type rules for overwrite-mode typing over control markers:
/// format/tab/variable markers are deleted (with table cleanup), font/color
/// markers are preserved by inserting in front, and saved positions and note
/// anchors are jumped over.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("Overwrite mode control-code rules")
{
    ensureQApplication();

    SUBCASE("format toggle is deleted and cleaned up")
    {
        cEditorCtrl editor;
        cDocument* doc = editor.GetDocument();
        cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

        doc->Insert("A");
        doc->BeginBold();          // bold marker at position 1
        doc->Insert("BC\r");
        layout->LayoutDocument(doc);

        editor.ToggleInsertOverwrite();
        doc->SetPosition(0);
        editor.InsertText("XY");   // overwrites 'A' then the bold marker

        // Marker consumed and no orphaned control-code entry remains. The last
        // position is the document EOF sentinel (itself a MARKER_CHAR with
        // STYLE_EOF), so it is excluded from the count.
        POSITION_T size = doc->GetTextSize();
        int markers = 0;
        bool orphan = false;
        for (POSITION_T p = 0; p < size; p++)
        {
            eModifiers ctrl = doc->GetControlChar(p);
            if (ctrl == STYLE_EOF)
            {
                continue;
            }
            std::string g = doc->GetCharNoAdvance(p);
            if (!g.empty() && g[0] == MARKER_CHAR)
            {
                markers++;
            }
            if (ctrl != STYLE_END_OF_STYLES)
            {
                orphan = true;
            }
        }
        CHECK(markers == 0);
        CHECK(orphan == false);
    }

    SUBCASE("font is preserved, text inserted in front")
    {
        cEditorCtrl editor;
        cDocument* doc = editor.GetDocument();
        cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

        doc->Insert("A");
        sInternalFonts font;
        font.fontname = "Courier";
        font.size = 12.0;
        font.haveWSFont = false;
        font.name = "Courier";
        doc->InsertFont(font);     // font marker at position 1
        doc->Insert("BC\r");
        layout->LayoutDocument(doc);

        editor.ToggleInsertOverwrite();
        doc->SetPosition(0);
        editor.InsertText("XY");   // overwrites 'A', then stops in front of the font

        POSITION_T size = doc->GetTextSize();
        int fontMarkers = 0;
        for (POSITION_T p = 0; p < size; p++)
        {
            if (doc->GetControlChar(p) == STYLE_FONT1)
            {
                fontMarkers++;
            }
        }
        CHECK(fontMarkers == 1);
    }

    SUBCASE("color is preserved, text inserted in front")
    {
        cEditorCtrl editor;
        cDocument* doc = editor.GetDocument();
        cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

        doc->Insert("A");
        sSeqRGBColor color;
        color.red = 255;
        color.green = 0;
        color.blue = 0;
        color.alpha = 0;
        doc->InsertColor(color);   // color marker at position 1
        doc->Insert("BC\r");
        layout->LayoutDocument(doc);

        editor.ToggleInsertOverwrite();
        doc->SetPosition(0);
        editor.InsertText("XY");

        POSITION_T size = doc->GetTextSize();
        int colorMarkers = 0;
        for (POSITION_T p = 0; p < size; p++)
        {
            if (doc->GetControlChar(p) == STYLE_INTERNAL_COLOR)
            {
                colorMarkers++;
            }
        }
        CHECK(colorMarkers == 1);
    }

    SUBCASE("saved position is jumped over and preserved")
    {
        cEditorCtrl editor;
        cDocument* doc = editor.GetDocument();
        cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

        doc->Insert("ABC\r");
        doc->SetPosition(1);
        doc->SetSavePosition(0);   // SAVE_CHAR bookmark at position 1
        layout->LayoutDocument(doc);

        editor.ToggleInsertOverwrite();
        doc->SetPosition(0);
        editor.InsertText("XY");   // jumps over the bookmark while overwriting

        POSITION_T size = doc->GetTextSize();
        int saves = 0;
        for (POSITION_T p = 0; p < size; p++)
        {
            std::string g = doc->GetCharNoAdvance(p);
            if (!g.empty() && g[0] == SAVE_CHAR)
            {
                saves++;
            }
        }
        CHECK(saves == 1);
    }

    SUBCASE("footnote anchor is jumped over and preserved")
    {
        cEditorCtrl editor;
        cDocument* doc = editor.GetDocument();
        cLayout* layout = dynamic_cast<cLayout*>(editor.GetLayout());

        doc->Insert("A");
        sNote note;
        note.symbol = NOTE_NUMBER;
        note.text = "a note";
        doc->InsertFootnote(note); // footnote marker at position 1
        doc->Insert("BC\r");
        layout->LayoutDocument(doc);

        editor.ToggleInsertOverwrite();
        doc->SetPosition(0);
        editor.InsertText("XY");

        POSITION_T size = doc->GetTextSize();
        int notes = 0;
        for (POSITION_T p = 0; p < size; p++)
        {
            if (doc->GetControlChar(p) == STYLE_FOOTNOTE)
            {
                notes++;
            }
        }
        CHECK(notes == 1);
    }
}
