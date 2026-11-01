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
        CHECK(ReadFileBytes(path) == expected);
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


TEST_CASE("cWSEditorCtrl - the save format follows the last save with a known extension")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-savefmt-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    SUBCASE("a text file saved as .ws, then with an unknown extension")
    {
        auto path = dir / "notes.def";
        WriteFileBytes(path, "key=value\r\n");
        REQUIRE(editor.LoadFile(path.string()));

        REQUIRE(editor.SaveFile((dir / "x.ws").string()));
        REQUIRE(editor.SaveFile((dir / "y.def").string()));

        std::string bytes = ReadFileBytes(dir / "y.def");
        REQUIRE(bytes.empty() == false);
        CHECK(static_cast<unsigned char>(bytes[0]) == 0x1D);
    }

    SUBCASE("a WordStar file saved as .txt, then with an unknown extension")
    {
        doc->Insert("Hello\r");
        REQUIRE(editor.SaveFile((dir / "doc.ws").string()));
        REQUIRE(editor.LoadFile((dir / "doc.ws").string()));

        REQUIRE(editor.SaveFile((dir / "x.txt").string()));
        REQUIRE(editor.SaveFile((dir / "y.def").string()));

        std::string expected = "Hello\n";
        CHECK(ReadFileBytes(dir / "y.def") == expected);
    }

    SUBCASE("an unknown extension keeps the format")
    {
        auto path = dir / "notes.def";
        WriteFileBytes(path, "key=value\r\n");
        REQUIRE(editor.LoadFile(path.string()));

        REQUIRE(editor.SaveFile((dir / "x.ini").string()));
        REQUIRE(editor.SaveFile((dir / "y.def").string()));

        std::string expected = "key=value\r\n";
        CHECK(ReadFileBytes(dir / "x.ini") == expected);
        CHECK(ReadFileBytes(dir / "y.def") == expected);
    }

    SUBCASE("inserting a WordStar file into a text file keeps text")
    {
        doc->Insert("Hello\r");
        REQUIRE(editor.SaveFile((dir / "doc.ws").string()));

        auto path = dir / "notes.def";
        WriteFileBytes(path, "key=value\r\n");
        REQUIRE(editor.LoadFile(path.string()));
        doc->SetPosition(0);
        REQUIRE(editor.InsertFileAtCursor((dir / "doc.ws").string()));
        REQUIRE(editor.SaveFile((dir / "y.def").string()));

        std::string bytes = ReadFileBytes(dir / "y.def");
        REQUIRE(bytes.empty() == false);
        CHECK(static_cast<unsigned char>(bytes[0]) != 0x1D);
        CHECK(bytes.find("key=value") != std::string::npos);
    }

    SUBCASE("a failed save keeps the format")
    {
        auto path = dir / "notes.def";
        WriteFileBytes(path, "key=value\r\n");
        REQUIRE(editor.LoadFile(path.string()));

        CHECK_FALSE(editor.SaveFile((dir / "missing" / "x.ws").string()));
        REQUIRE(editor.SaveFile((dir / "y.def").string()));

        std::string expected = "key=value\r\n";
        CHECK(ReadFileBytes(dir / "y.def") == expected);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


namespace
{

class cBlockTargetEditor : public cWSEditorCtrl
{
public:
    std::string mTarget;

    std::string PromptForSaveFile(void) override
    {
        return mTarget;
    }
};

}   // anonymous namespace


TEST_CASE("cWSEditorCtrl - plain-text output carries no end-of-file marker")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-noeof-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cBlockTargetEditor editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);
    doc->Insert("Hello\rWorld");

    SUBCASE("save as .txt")
    {
        REQUIRE(editor.SaveFile((dir / "a.txt").string()));
        CHECK(ReadFileBytes(dir / "a.txt") == "Hello\nWorld");
    }

    SUBCASE("a text file saved again")
    {
        auto path = dir / "notes.def";
        WriteFileBytes(path, "Hello\r\nWorld");
        REQUIRE(editor.LoadFile(path.string()));
        REQUIRE(editor.SaveFile(path.string()));
        REQUIRE(editor.LoadFile(path.string()));
        REQUIRE(editor.SaveFile(path.string()));
        CHECK(ReadFileBytes(path) == "Hello\r\nWorld");
    }

    SUBCASE("write the whole document as a block")
    {
        doc->SetPosition(0);
        doc->SetBeginBlock();
        doc->SetPosition(doc->GetTextSize());
        doc->SetEndBlock();
        doc->SetTextFile(true);
        editor.mTarget = (dir / "block.txt").string();
        editor.WriteBlockToFile();
        CHECK(ReadFileBytes(dir / "block.txt") == "Hello\nWorld");
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


TEST_CASE("cWSEditorCtrl - a text file saves with the line endings it was opened with")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-eol-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    auto roundTrip = [&](const std::string& name, const std::string& bytes)
    {
        auto path = dir / name;
        WriteFileBytes(path, bytes);
        REQUIRE(editor.LoadFile(path.string()));
        REQUIRE(editor.SaveFile(path.string()));
        return ReadFileBytes(path);
    };

    SUBCASE("LF")
    {
        std::string bytes = "one\ttwo\nthree\n\nfour\n";
        CHECK(roundTrip("lf.txt", bytes) == bytes);
    }

    SUBCASE("CRLF")
    {
        std::string bytes = "one\ttwo\r\nthree\r\n\r\nfour\r\n";
        CHECK(roundTrip("crlf.txt", bytes) == bytes);
    }

    SUBCASE("CR")
    {
        std::string bytes = "one\ttwo\rthree\r\rfour\r";
        CHECK(roundTrip("cr.txt", bytes) == bytes);
    }

    SUBCASE("no newline at the end")
    {
        std::string bytes = "one\ntwo";
        CHECK(roundTrip("nonl.txt", bytes) == bytes);
    }

    SUBCASE("mixed line endings use the first one")
    {
        CHECK(roundTrip("mixed.txt", "a\nb\r\nc\r") == "a\nb\nc\n");
    }

    SUBCASE("a new document saved as .txt uses LF")
    {
        doc->Insert("Hello\rWorld\r");
        REQUIRE(editor.SaveFile((dir / "new.txt").string()));
        CHECK(ReadFileBytes(dir / "new.txt") == "Hello\nWorld\n");
    }

    SUBCASE("a WordStar file saved as .txt uses LF")
    {
        doc->Insert("Hello\rWorld\r");
        REQUIRE(editor.SaveFile((dir / "doc.ws").string()));
        REQUIRE(editor.LoadFile((dir / "doc.ws").string()));
        REQUIRE(editor.SaveFile((dir / "doc.txt").string()));
        CHECK(ReadFileBytes(dir / "doc.txt") == "Hello\nWorld\n");
    }

    SUBCASE("a CRLF text file then a WordStar file")
    {
        doc->Insert("Hello\r");
        REQUIRE(editor.SaveFile((dir / "doc.ws").string()));
        WriteFileBytes(dir / "crlf.txt", "a\r\n");
        REQUIRE(editor.LoadFile((dir / "crlf.txt").string()));
        REQUIRE(editor.LoadFile((dir / "doc.ws").string()));
        REQUIRE(editor.SaveFile((dir / "doc.txt").string()));
        CHECK(ReadFileBytes(dir / "doc.txt") == "Hello\n");
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


TEST_CASE("cWSEditorCtrl - Write Block to File writes plain text from a text document")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-kwtext-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cBlockTargetEditor editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    auto writeWholeDocument = [&](void)
    {
        doc->SetPosition(0);
        doc->SetBeginBlock();
        doc->SetPosition(doc->GetTextSize());
        doc->SetEndBlock();
        editor.mTarget = (dir / "block.txt").string();
        editor.WriteBlockToFile();
        return ReadFileBytes(dir / "block.txt");
    };

    SUBCASE("a CRLF text file with a tab")
    {
        WriteFileBytes(dir / "crlf.txt", "a\tb\r\nc\r\n");
        REQUIRE(editor.LoadFile((dir / "crlf.txt").string()));
        CHECK(writeWholeDocument() == "a\tb\nc\n");
    }

    SUBCASE("markers in a text document")
    {
        sWSTab tab;
        tab.type = TAB_TAB;
        tab.tabsize = 0;
        tab.abstabsize = 0;
        tab.size = 0;

        doc->Insert("A");
        doc->InsertTab(tab);
        doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
        doc->Insert("B");
        doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
        doc->InsertVariable(VAR_DATE);
        doc->Insert("\rC\x7f" "D");
        doc->SetTextFile(true);
        CHECK(writeWholeDocument() == "A\tB\nC\x7f" "D");
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
