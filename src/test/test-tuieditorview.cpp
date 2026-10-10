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

#include "src/tui/editor/editorctrl.h"
#include "src/tui/editor/editorview.h"
#include "src/tui/wordstartui/src/screen.h"
#include "src/core/document/document.h"
#include "src/core/layout/layoutbase.h"
#include "src/core/include/config.h"


namespace
{

// Screen column of the caret with the caret at a document position
int CaretColumnAt(cWSEditorCtrl& editor, cWSEditorView& view, POSITION_T pos)
{
    editor.GetDocument()->SetPosition(pos);
    wordstartui::cScreen screen;
    screen.Resize(60, 200);
    view.Draw(screen);
    REQUIRE(view.CaretVisible());
    return view.GetCaretCol();
}

}


TEST_CASE("cWSEditorView - reveal badges extend to the right of a centred line, the EOF takes no columns")
{
    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();
    REQUIRE(layout != nullptr);

    cWSEditorView view(&editor);
    view.SetBounds({0, 0, 60, 200});

    SUBCASE("a centred line: document columns centre it, badges push the text right")
    {
        // !0 a1 b2 c3 ^B4 d5 e6 f7 \r8
        sWSTab centerTab;
        centerTab.type = TAB_CENTER;
        centerTab.tabsize = 0;
        centerTab.abstabsize = 0;
        centerTab.size = 0;
        doc->InsertTab(centerTab);
        doc->Insert("abc");
        doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
        doc->Insert("def\r");

        layout->SetShowControl(SHOW_NONE);
        editor.LayoutDocument(true);
        int hiddenA = CaretColumnAt(editor, view, 1);
        int hiddenD = CaretColumnAt(editor, view, 5);
        CHECK(hiddenA > 10);                                    // centred, not at the margin
        CHECK(hiddenD == hiddenA + 3);
        CHECK(CaretColumnAt(editor, view, 0) == hiddenA);       // the hidden tab takes no column

        layout->SetShowControl(SHOW_ALL);
        editor.LayoutDocument(true);
        CHECK(CaretColumnAt(editor, view, 0) == hiddenA);       // the ! badge starts the line
        int shownA = CaretColumnAt(editor, view, 1);
        CHECK(shownA == hiddenA + 1);
        int badgeCols = CaretColumnAt(editor, view, 5) - CaretColumnAt(editor, view, 4);
        CHECK(badgeCols >= 1);
        CHECK(CaretColumnAt(editor, view, 5) == hiddenD + 1 + badgeCols);
    }

    SUBCASE("the end-of-file marker is never drawn")
    {
        doc->Insert("abc");                                     // EOF at 3
        for (eShowControl mode : { SHOW_ALL, SHOW_DOT, SHOW_NONE })
        {
            CAPTURE(static_cast<int>(mode));
            layout->SetShowControl(mode);
            editor.LayoutDocument(true);
            int lastColumn = CaretColumnAt(editor, view, 2);
            CHECK(CaretColumnAt(editor, view, 3) == lastColumn + 1);
        }
    }
}
