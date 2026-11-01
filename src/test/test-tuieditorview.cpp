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
#include "src/input/wordtsarinput.h"

#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>


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


// Regression: the TUI applies the Preferences default font at startup with
// SetFont; the cached column width (cursor and ruler positions) has to follow
// the new font after the relayout
TEST_CASE("cWSEditorCtrl - a new default font updates the column width after relayout")
{
    cWSEditorCtrl editor;
    COORD_T width12 = editor.GetColumnWidth();

    editor.SetFont("Courier New|24.0|0|0|0|0|0");
    editor.LayoutDocument(true);
    CHECK(editor.GetFont() == "Courier New|24.0|0|0|0|0|0");
    CHECK(editor.GetColumnWidth() > width12);
}


TEST_CASE("cWSEditorCtrl - ^KD saves, clears and returns to the Opening screen")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-kd-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cWSEditorCtrl editor;
    cWordStarInput input(&editor);
    POSITION_T emptySize = editor.GetDocument()->GetTextSize();

    SUBCASE("a named document is saved and closed")
    {
        editor.mFileDir = dir.string();
        editor.mFileName = "kd.txt";
        editor.mFileSet = true;
        editor.GetDocument()->Insert("some text");
        REQUIRE(editor.GetDocument()->IsModified() == true);

        CHECK(input.OnControlKChar('d') == true);
        CHECK(editor.ReturnToOpeningRequested() == true);
        CHECK(editor.GetDocument()->GetTextSize() == emptySize);
        CHECK(std::filesystem::exists(dir / "kd.txt") == true);
    }

    SUBCASE("an untitled document whose Save As is cancelled stays open")
    {
        editor.GetDocument()->Insert("some text");
        REQUIRE(editor.mFileSet == false);

        CHECK(input.OnControlKChar('d') == false);
        CHECK(editor.ReturnToOpeningRequested() == false);
        CHECK(editor.GetDocument()->GetTextSize() == emptySize + 9);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


namespace
{

std::string ReadFileBytes(const std::filesystem::path& path)
{
    std::ifstream f(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

void WriteFileBytes(const std::filesystem::path& path, const std::string& bytes)
{
    std::ofstream f(path, std::ios::binary);
    f << bytes;
}

}


TEST_CASE("cWSEditorCtrl - a file opened as text saves as text, a WordStar file as WordStar")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-nondoc-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    SUBCASE("a text file with an unknown extension")
    {
        auto path = dir / "setup.def";
        WriteFileBytes(path, "key=value\r\nother=1\r\n");
        REQUIRE(editor.LoadFile(path.string()));

        doc->SetPosition(0);
        doc->Insert("# ");
        REQUIRE(editor.SaveFile(path.string()));

        std::string expected = "# key=value\r\nother=1\r\n";
        CHECK(ReadFileBytes(path).substr(0, expected.size()) == expected);
    }

    SUBCASE("a WordStar file with an unknown extension")
    {
        doc->Insert("Hello\r");
        REQUIRE(editor.SaveFile((dir / "doc.ws").string()));
        std::filesystem::copy_file(dir / "doc.ws", dir / "doc.def");

        REQUIRE(editor.LoadFile((dir / "doc.def").string()));
        doc->SetPosition(0);
        doc->Insert("X");
        REQUIRE(editor.SaveFile((dir / "doc.def").string()));

        std::string bytes = ReadFileBytes(dir / "doc.def");
        REQUIRE(bytes.empty() == false);
        CHECK(static_cast<unsigned char>(bytes[0]) == 0x1D);
    }

    SUBCASE("a new document saved with an unknown extension")
    {
        doc->Insert("Hello\r");
        REQUIRE(editor.SaveFile((dir / "new.def").string()));

        std::string bytes = ReadFileBytes(dir / "new.def");
        REQUIRE(bytes.empty() == false);
        CHECK(static_cast<unsigned char>(bytes[0]) == 0x1D);
    }

    SUBCASE("a new document after a text file was abandoned")
    {
        auto path = dir / "setup.def";
        WriteFileBytes(path, "key=value\r\n");
        REQUIRE(editor.LoadFile(path.string()));
        editor.AbandonFile();

        doc->Insert("Hello\r");
        REQUIRE(editor.SaveFile((dir / "new.def").string()));

        std::string bytes = ReadFileBytes(dir / "new.def");
        REQUIRE(bytes.empty() == false);
        CHECK(static_cast<unsigned char>(bytes[0]) == 0x1D);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
