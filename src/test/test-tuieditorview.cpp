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
#include "src/core/spellcheck/spellcheck.h"
#include "src/tui/dialogs/dialoghost.h"
#include "src/tui/wordstartui/src/dialog.h"
#include "src/tui/wordstartui/src/theme.h"
#include "src/tui/wordstartui/src/tuidefs.h"

#include <algorithm>
#include <deque>
#include <filesystem>
#include <functional>
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


TEST_CASE("cWSEditorCtrl - a failed backup marks the status message as an error")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-bakerr-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    {
        cWSEditorCtrl editor;
        editor.GetDocument()->Insert("some text");
        REQUIRE(editor.GetDocument()->IsModified() == true);
        editor.mBackupFileName = (dir / "missing" / "doc-bak.ws").string();

        editor.AutoSaveBackup();
        CHECK(editor.GetStatusMessage().rfind("Backup save failed: ", 0) == 0);
        CHECK(editor.IsStatusError() == true);

        editor.SetStatusMessage("Sent to x", false, 90);
        CHECK(editor.IsStatusError() == false);

        editor.SetStatusMessage("Backup save failed: x", false, 2, true);
        editor.TickStatusMessage();
        CHECK(editor.IsStatusError() == true);
        editor.TickStatusMessage();
        CHECK(editor.GetStatusMessage().empty() == true);
        CHECK(editor.IsStatusError() == false);
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

    eFileExistsChoice mChoice = eFileExistsChoice::Overwrite;

    std::string PromptForWriteBlockFile(bool& asciiFormat, bool asciiLocked) override
    {
        (void)asciiFormat;
        (void)asciiLocked;
        return mTarget;
    }

    eFileExistsChoice ConfirmOverwriteOrAppend(const std::string& filename) override
    {
        (void)filename;
        return mChoice;
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


TEST_CASE("cWSEditorCtrl - a text file ends at the first 0x1A, the rest is saved back unchanged")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-eot-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    const std::string tail = std::string("\x1A") + "TAILMARK" + std::string(3, '\x1A');

    auto shownText = [&](void)
    {
        return doc->GetPlainText(0, doc->GetTextSize(), "\n");
    };

    SUBCASE("LF: only the text is shown, an unedited save is byte-identical")
    {
        std::string bytes = "one\ttwo\nthree\n" + tail;
        WriteFileBytes(dir / "lf.txt", bytes);
        REQUIRE(editor.LoadFile((dir / "lf.txt").string()));
        CHECK(shownText() == "one\ttwo\nthree\n");
        REQUIRE(editor.SaveFile((dir / "lf.txt").string()));
        CHECK(ReadFileBytes(dir / "lf.txt") == bytes);
    }

    SUBCASE("CRLF: only the text is shown, an unedited save is byte-identical")
    {
        std::string bytes = "one\r\ntwo\r\n" + tail;
        WriteFileBytes(dir / "crlf.txt", bytes);
        REQUIRE(editor.LoadFile((dir / "crlf.txt").string()));
        CHECK(shownText() == "one\ntwo\n");
        REQUIRE(editor.SaveFile((dir / "crlf.txt").string()));
        CHECK(ReadFileBytes(dir / "crlf.txt") == bytes);
    }

    SUBCASE("an edit before the 0x1A keeps the same tail")
    {
        WriteFileBytes(dir / "edit.txt", "one\ntwo\n" + tail);
        REQUIRE(editor.LoadFile((dir / "edit.txt").string()));
        doc->SetPosition(0);
        doc->Insert("X");
        REQUIRE(editor.SaveFile((dir / "edit.txt").string()));
        CHECK(ReadFileBytes(dir / "edit.txt") == "Xone\ntwo\n" + tail);
    }

    SUBCASE("a text document saved to a name without extension keeps the tail")
    {
        WriteFileBytes(dir / "a.txt", "one\n" + tail);
        REQUIRE(editor.LoadFile((dir / "a.txt").string()));
        REQUIRE(editor.SaveFile((dir / "b").string()));
        CHECK(ReadFileBytes(dir / "b") == "one\n" + tail);
    }

    SUBCASE("a file without 0x1A gets none on save")
    {
        WriteFileBytes(dir / "none.txt", "one\ntwo");
        REQUIRE(editor.LoadFile((dir / "none.txt").string()));
        REQUIRE(editor.SaveFile((dir / "none.txt").string()));
        CHECK(ReadFileBytes(dir / "none.txt") == "one\ntwo");
    }

    SUBCASE("control bytes after the 0x1A over 5% of the file: still opens as text")
    {
        std::string bytes = "one\ntwo\n" + std::string("\x1A") + std::string(40, '\x01') + std::string(40, '\x1A');
        WriteFileBytes(dir / "macro.txt", bytes);
        REQUIRE(editor.LoadFile((dir / "macro.txt").string()));
        CHECK(doc->IsTextFile());
        CHECK(shownText() == "one\ntwo\n");
        REQUIRE(editor.SaveFile((dir / "macro.txt").string()));
        CHECK(ReadFileBytes(dir / "macro.txt") == bytes);
    }

    SUBCASE("a WordStar save drops the tail")
    {
        WriteFileBytes(dir / "ws.txt", "one\ntwo\n" + tail);
        REQUIRE(editor.LoadFile((dir / "ws.txt").string()));
        REQUIRE(editor.SaveFile((dir / "ws.ws").string()));
        CHECK(ReadFileBytes(dir / "ws.ws").find("TAILMARK") == std::string::npos);
        REQUIRE(editor.SaveFile((dir / "ws-again.txt").string()));
        CHECK(ReadFileBytes(dir / "ws-again.txt") == "one\ntwo\n");
    }

    SUBCASE("an RTF export keeps the tail for the next text save")
    {
        WriteFileBytes(dir / "rtf.txt", "one\n" + tail);
        REQUIRE(editor.LoadFile((dir / "rtf.txt").string()));
        REQUIRE(editor.SaveFile((dir / "rtf.rtf").string()));
        CHECK(ReadFileBytes(dir / "rtf.rtf").find("TAILMARK") == std::string::npos);
        REQUIRE(editor.SaveFile((dir / "rtf-again.txt").string()));
        CHECK(ReadFileBytes(dir / "rtf-again.txt") == "one\n" + tail);
        REQUIRE(editor.SaveFile((dir / "rtf.ws").string()));
        REQUIRE(editor.SaveFile((dir / "rtf-ws.txt").string()));
        CHECK(ReadFileBytes(dir / "rtf-ws.txt") == "one\n");
    }

    SUBCASE("a failed WordStar save keeps the tail")
    {
        WriteFileBytes(dir / "fail.txt", "one\n" + tail);
        REQUIRE(editor.LoadFile((dir / "fail.txt").string()));
        CHECK(editor.SaveFile((dir / "no-such-dir" / "fail.ws").string()) == false);
        CHECK(doc->IsTextFile());
        REQUIRE(editor.SaveFile((dir / "fail-again.txt").string()));
        CHECK(ReadFileBytes(dir / "fail-again.txt") == "one\n" + tail);
    }

    SUBCASE("a WordStar file saved as text gets no tail")
    {
        WriteFileBytes(dir / "src.txt", "one\n");
        REQUIRE(editor.LoadFile((dir / "src.txt").string()));
        REQUIRE(editor.SaveFile((dir / "src.ws").string()));
        REQUIRE(ReadFileBytes(dir / "src.ws").find('\x1A') != std::string::npos);
        WriteFileBytes(dir / "tail.txt", "two\n" + tail);
        REQUIRE(editor.LoadFile((dir / "tail.txt").string()));
        REQUIRE(editor.LoadFile((dir / "src.ws").string()));
        REQUIRE(editor.SaveFile((dir / "from-ws.txt").string()));
        std::string saved = ReadFileBytes(dir / "from-ws.txt");
        CHECK(saved.find("one") != std::string::npos);
        CHECK(saved.find('\x1A') == std::string::npos);
        CHECK(saved.find("TAILMARK") == std::string::npos);
    }

    SUBCASE("abandoning the document clears the tail")
    {
        WriteFileBytes(dir / "quit.txt", "one\n" + tail);
        REQUIRE(editor.LoadFile((dir / "quit.txt").string()));
        editor.AbandonFile();
        doc = editor.GetDocument();
        doc->Insert("new\r");
        REQUIRE(editor.SaveFile((dir / "new.txt").string()));
        CHECK(ReadFileBytes(dir / "new.txt") == "new\n");
    }

    SUBCASE("opening another text file replaces the tail")
    {
        WriteFileBytes(dir / "first.txt", "one\n" + tail);
        WriteFileBytes(dir / "second.txt", "two\n");
        REQUIRE(editor.LoadFile((dir / "first.txt").string()));
        REQUIRE(editor.LoadFile((dir / "second.txt").string()));
        REQUIRE(editor.SaveFile((dir / "second.txt").string()));
        CHECK(ReadFileBytes(dir / "second.txt") == "two\n");
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

    SUBCASE("a line that starts with a dot")
    {
        WriteFileBytes(dir / "dot.txt", "a\n.pa\n\tb\n");
        REQUIRE(editor.LoadFile((dir / "dot.txt").string()));
        CHECK(writeWholeDocument() == "a\n.pa\n\tb\n");
    }

    SUBCASE("append to an LF text file")
    {
        WriteFileBytes(dir / "doc.txt", "a\tb\n.pa x\n");
        REQUIRE(editor.LoadFile((dir / "doc.txt").string()));
        WriteFileBytes(dir / "block.txt", "x\n");
        editor.mChoice = cEditorBase::eFileExistsChoice::Append;
        CHECK(writeWholeDocument() == "x\na\tb\n.pa x\n");
    }

    SUBCASE("append to a CRLF text file")
    {
        WriteFileBytes(dir / "doc.txt", "a\tb\n.pa x\n");
        REQUIRE(editor.LoadFile((dir / "doc.txt").string()));
        WriteFileBytes(dir / "block.txt", "x\r\n");
        editor.mChoice = cEditorBase::eFileExistsChoice::Append;
        CHECK(writeWholeDocument() == "x\r\na\tb\r\n.pa x\r\n");
    }

    SUBCASE("markers in a text document")
    {
        sWSTab tab;
        tab.type = TAB_TAB;
        tab.tabsize = 0;
        tab.abstabsize = 0;
        tab.size = 0;

        doc->SetTextFile(true);
        doc->Insert("A");
        doc->InsertTab(tab);
        doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
        doc->Insert("B");
        doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
        doc->InsertVariable(VAR_DATE);
        editor.InsertPastedText("\rC\x7f" "D");       // 0x7F as a raw byte
        doc->Insert(static_cast<CHAR_T>(MARKER_CHAR)); // a bare marker
        CHECK(writeWholeDocument() == "A\tB\nC\x7f" "D");
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


TEST_CASE("cWSEditorCtrl - clipboard copy writes plain text with LF line endings")
{
    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    sWSTab tab;
    tab.type = TAB_TAB;
    tab.tabsize = 0;
    tab.abstabsize = 0;
    tab.size = 0;

    sInternalFonts font;
    font.fontname = "Courier";
    font.size = 12.0;
    font.haveWSFont = false;
    font.name = "Courier";

    sNote footnote;
    footnote.symbol = NOTE_NUMBER;
    footnote.text = "a footnote";

    doc->Insert("A");
    doc->InsertTab(tab);
    doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
    doc->Insert("B");
    doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
    doc->InsertFont(font);
    doc->InsertFootnote(footnote);
    doc->SetTextFile(true);
    editor.InsertPastedText("\rC\x7f" "D\r");      // 0x7F as a raw byte
    doc->Insert(static_cast<CHAR_T>(MARKER_CHAR));   // a bare marker

    doc->SetPosition(0);
    doc->SetBeginBlock();
    doc->SetPosition(doc->GetTextSize());
    doc->SetEndBlock();

    std::string expected = "A\tB\nC\x7f" "D\n";
    CHECK(editor.GetClipboardText() == expected);
}


TEST_CASE("cWSEditorCtrl - an export format leaves the save format unchanged")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-exportfmt-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    SUBCASE("a text file saved as .rtf, then with an unknown extension")
    {
        auto path = dir / "notes.def";
        WriteFileBytes(path, "key=value\r\n");
        REQUIRE(editor.LoadFile(path.string()));

        REQUIRE(editor.SaveFile((dir / "x.rtf").string()));
        REQUIRE(editor.SaveFile((dir / "y.def").string()));

        std::string expected = "key=value\r\n";
        CHECK(ReadFileBytes(dir / "y.def").substr(0, expected.size()) == expected);
    }

    SUBCASE("a WordStar file saved as .rtf, then with an unknown extension")
    {
        doc->Insert("Hello\r");
        REQUIRE(editor.SaveFile((dir / "doc.ws").string()));
        REQUIRE(editor.LoadFile((dir / "doc.ws").string()));

        REQUIRE(editor.SaveFile((dir / "x.rtf").string()));
        REQUIRE(editor.SaveFile((dir / "y.def").string()));

        std::string bytes = ReadFileBytes(dir / "y.def");
        REQUIRE(bytes.empty() == false);
        CHECK(static_cast<unsigned char>(bytes[0]) == 0x1D);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


TEST_CASE("cWSEditorView - cursor up moves past every automatic page-break line")
{
    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();
    REQUIRE(layout != nullptr);

    cWSEditorView view(&editor);
    view.SetBounds({0, 0, 30, 120});
    cWordStarInput input(&editor);

    for (int para = 0; para < 60; ++para)
    {
        doc->Insert("Absatz " + std::to_string(para) +
                    ": Die Größe der Übung wird später geprüft, und zwar "
                    "für alle Seiten des Dokuments gleichermaßen.\r");
    }
    editor.LayoutDocument(true);

    LINE_T total = layout->GetNumberOfLines();
    int pageBreaks = 0;
    int splitParagraphs = 0;
    for (LINE_T ln = 1; ln < total; ++ln)
    {
        if (layout->GetLineByRawLineNumber(ln)->pagenumber !=
            layout->GetLineByRawLineNumber(ln - 1)->pagenumber)
        {
            pageBreaks++;
            if (doc->GetParagraphFromPosition(layout->GetLineByRawLineNumber(ln)->documentPosition) ==
                doc->GetParagraphFromPosition(layout->GetLineByRawLineNumber(ln - 1)->documentPosition))
            {
                splitParagraphs++;
            }
        }
    }
    REQUIRE(pageBreaks >= 2);
    REQUIRE(splitParagraphs >= 1);

    for (eShowControl mode : { SHOW_ALL, SHOW_DOT, SHOW_NONE })
    {
        CAPTURE(static_cast<int>(mode));
        layout->SetShowControl(mode);
        editor.LayoutDocument(true);

        // Walk to the top from column 0 of the last line and from column 30 of
        // the line above it; every Up must land one line higher.
        total = layout->GetNumberOfLines();
        REQUIRE(total >= 2);
        for (POSITION_T column : { POSITION_T(0), POSITION_T(30) })
        {
            CAPTURE(column);
            LINE_T startLine = (column == 0) ? total - 1 : total - 2;
            const sLineLayout* first = layout->GetLineByRawLineNumber(startLine);
            REQUIRE(first != nullptr);
            if (column == 0)
            {
                editor.MoveCaretToDocEnd();
            }
            else
            {
                doc->SetPosition(first->documentPosition + column + 1);
                editor.MoveCaretLeft();
            }
            REQUIRE(layout->GetLineFromPosition(doc->GetPosition()) == startLine);
            wordstartui::cScreen screen;
            screen.Resize(30, 120);
            view.Draw(screen);

            for (LINE_T expected = startLine - 1; expected >= 0; --expected)
            {
                CAPTURE(expected);
                input.HandleSpecialKey(SPECIAL_UP);
                view.Draw(screen);
                CHECK(layout->GetLineFromPosition(doc->GetPosition()) == expected);
                if (layout->GetLineFromPosition(doc->GetPosition()) != expected)
                {
                    break;
                }
            }
        }
    }
}


namespace
{

// Dialog host that feeds queued key events to modal dialogs; an empty queue
// cancels the dialog.
class cSpellScriptHost : public iWSDialogHost
{
public:
    void QueueSpecial(wordstartui::eSpecialKey key)
    {
        wordstartui::sInputEvent event{};
        event.type = wordstartui::INPUT_TYPE_SPECIAL;
        event.special = key;
        mEvents.push_back(event);
    }

    void HostRunModal(wordstartui::cDialog& dialog) override
    {
        mModals = mModals + 1;
        dialog.FocusFirst();
        while (dialog.GetResult() == wordstartui::DIALOG_RESULT_NONE)
        {
            if (mEvents.empty() == true)
            {
                dialog.SetResult(wordstartui::DIALOG_RESULT_CANCEL);
                break;
            }
            dialog.HandleEvent(mEvents.front());
            mEvents.pop_front();
        }
    }

    void HostRunModalRaw(
        const std::function<void(wordstartui::cScreen&, const wordstartui::cTheme&)>& draw,
        const std::function<bool(const wordstartui::sInputEvent&)>& handle) override
    {
        (void)draw;
        wordstartui::sInputEvent escape{};
        escape.type = wordstartui::INPUT_TYPE_SPECIAL;
        escape.special = wordstartui::SPECIAL_KEY_ESCAPE;
        while (handle(escape) == true)
        {
        }
    }

    const wordstartui::cTheme& HostTheme(void) override
    {
        return mTheme;
    }

    wordstartui::sTerminalSize HostScreenSize(void) override
    {
        wordstartui::sTerminalSize size;
        size.rows = 40;
        size.cols = 120;
        return size;
    }

    wordstartui::sTerminalCapabilities HostCapabilities(void) override
    {
        wordstartui::sTerminalCapabilities caps{};
        return caps;
    }

    void HostShowLoadProgress(const std::string& message, int percent) override
    {
        (void)message;
        (void)percent;
    }

    void HostHideLoadProgress(void) override
    {
    }

    int mModals = 0;

private:
    std::deque<wordstartui::sInputEvent> mEvents;
    wordstartui::cTheme mTheme;
};

}


TEST_CASE("cWSEditorCtrl - spell check Replace with a suggestion that ends in the following period")
{
    const std::string language = "de_DE";
    cSpellChecker checker(language);
    REQUIRE(checker.CheckWord("evtl") == false);
    std::vector<std::string> suggestions = checker.suggestions("evtl");
    auto found = std::find(suggestions.begin(), suggestions.end(), "evtl.");
    INFO("suggestions: ", doctest::toString(suggestions.size()));
    REQUIRE(found != suggestions.end());
    size_t index = static_cast<size_t>(found - suggestions.begin());

    cWSEditorCtrl editor;
    cSpellScriptHost host;
    editor.SetDialogHost(&host);
    editor.mSpellCheckLanguage = language;
    cDocument* doc = editor.GetDocument();
    doc->Insert("evtl.");
    editor.LayoutDocument(true);

    // List has focus: move to the suggestion, Tab past Ignore to Replace.
    for (size_t step = 0; step < index; ++step)
    {
        host.QueueSpecial(wordstartui::SPECIAL_KEY_ARROW_DOWN);
    }
    host.QueueSpecial(wordstartui::SPECIAL_KEY_TAB);
    host.QueueSpecial(wordstartui::SPECIAL_KEY_TAB);
    host.QueueSpecial(wordstartui::SPECIAL_KEY_ENTER);

    editor.SpellCheckDocument();

    std::string text = doc->GetText();
    text = text.substr(0, text.find('\x1A'));
    text = text.substr(0, text.find('\x7F'));
    CHECK(host.mModals >= 1);
    CHECK(text == "evtl.");
}


TEST_CASE("cWSEditorCtrl - a tab inserted as text is stored as the Tab key stores it")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-texttab-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    SUBCASE("text document: typed and pasted tabs save as 0x09")
    {
        cWSEditorCtrl editor;
        cDocument* doc = editor.GetDocument();
        auto path = dir / "tabs.txt";
        WriteFileBytes(path, "XY\n");
        REQUIRE(editor.LoadFile(path.string()));
        REQUIRE(doc->IsTextFile());

        doc->SetPosition(1);
        editor.Tab();
        editor.InsertPastedText("a\tb");
        CHECK(doc->GetControlChar(3) == STYLE_TAB);

        REQUIRE(editor.SaveFile(path.string()));
        CHECK(ReadFileBytes(path) == "X\ta\tbY\n");
    }

    SUBCASE("WordStar document: a pasted tab saves the same as a typed one")
    {
        cWSEditorCtrl typed;
        typed.InsertText("a");
        typed.Tab();
        typed.InsertText("b");
        REQUIRE(typed.SaveFile((dir / "typed.ws").string()));

        cWSEditorCtrl pasted;
        pasted.InsertPastedText("a\tb");
        REQUIRE(pasted.SaveFile((dir / "pasted.ws").string()));

        CHECK(pasted.GetDocument()->GetControlChar(1) == STYLE_TAB);
        CHECK(ReadFileBytes(dir / "pasted.ws") == ReadFileBytes(dir / "typed.ws"));
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


namespace
{

// "abc", byte, "def" and two long lines so the file loads as text
std::string TuiOneByteFile(unsigned char byte)
{
    std::string filler = "a line of plain text, long enough to keep the file text";
    std::string bytes = "abc";
    bytes += static_cast<char>(byte);
    bytes += "def\n" + filler + "\n" + filler + "\n";
    return bytes;
}

}   // anonymous namespace


TEST_CASE("cWSEditorCtrl - ^KR and paste keep a control byte in a text document only")
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-tui-rawinsert-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    REQUIRE(doc != nullptr);

    auto base = dir / "base.txt";
    WriteFileBytes(base, "XY\n");
    auto insert = dir / "insert.txt";
    WriteFileBytes(insert, TuiOneByteFile(0x02));

    auto wholeText = [&](void)
    {
        return doc->GetPlainText(0, doc->GetTextSize(), "\n");
    };

    SUBCASE("^KR of a text file into a text document")
    {
        REQUIRE(editor.LoadFile(base.string()));
        REQUIRE(doc->IsTextFile());
        doc->SetPosition(1);
        REQUIRE(editor.InsertFileAtCursor(insert.string()));
        CHECK(wholeText() == "X" + TuiOneByteFile(0x02) + "Y\n");
        CHECK(doc->GetControlChar(4) != STYLE_BOLD);

        REQUIRE(editor.SaveFile(base.string()));
        CHECK(ReadFileBytes(base) == "X" + TuiOneByteFile(0x02) + "Y\n");
    }

    SUBCASE("^KR of a text file into a WordStar document")
    {
        REQUIRE(doc->IsTextFile() == false);
        REQUIRE(editor.InsertFileAtCursor(insert.string()));
        CHECK(doc->GetControlChar(3) == STYLE_BOLD);
        CHECK(doc->GetPlainText(0, 8, "\n") == "abcdef\n");
    }

    SUBCASE("plain-text paste into a text document")
    {
        REQUIRE(editor.LoadFile(base.string()));
        REQUIRE(doc->IsTextFile());
        doc->SetPosition(1);
        editor.InsertPastedText("a\x02" "b\x1B" "c\x7F");
        CHECK(wholeText() == "Xa\x02" "b\x1B" "c\x7FY\n");
        CHECK(doc->GetControlChar(2) != STYLE_BOLD);

        doc->SetPosition(0);
        doc->SetBeginBlock();
        doc->SetPosition(doc->GetTextSize());
        doc->SetEndBlock();
        CHECK(editor.GetClipboardText() == "Xa\x02" "b\x1B" "c\x7FY\n");

        REQUIRE(editor.SaveFile(base.string()));
        CHECK(ReadFileBytes(base) == "Xa\x02" "b\x1B" "c\x7FY\n");
    }

    SUBCASE("plain-text paste into a WordStar document is unchanged")
    {
        REQUIRE(doc->IsTextFile() == false);
        editor.InsertPastedText("a\x02" "b\x02" "c");
        CHECK(doc->GetControlChar(1) == STYLE_BOLD);
        CHECK(doc->GetControlChar(3) == STYLE_BOLD);
        CHECK(wholeText() == "abc");
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


TEST_CASE("cWSEditorView - a raw byte shows as ^B in the control code colours with ^OD on and off")
{
    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();
    REQUIRE(layout != nullptr);

    cWSEditorView view(&editor);
    view.SetBounds({0, 0, 60, 200});

    // A0 ^B1 B2 \r3
    doc->Insert("A");
    doc->InsertRawByte(0x02);
    doc->Insert("B\r");

    for (eShowControl mode : { SHOW_ALL, SHOW_NONE })
    {
        CAPTURE(static_cast<int>(mode));
        layout->SetShowControl(mode);
        editor.LayoutDocument(true);

        int colA = CaretColumnAt(editor, view, 0);
        int colByte = CaretColumnAt(editor, view, 1);
        int colB = CaretColumnAt(editor, view, 2);
        CHECK(colByte == colA + 1);
        CHECK(colB == colByte + 2);

        doc->SetPosition(0);
        wordstartui::cScreen screen;
        screen.Resize(60, 200);
        view.Draw(screen);
        int row = view.GetCaretRow();

        const wordstartui::sCell* caret = screen.GetCell(row, colByte);
        const wordstartui::sCell* letter = screen.GetCell(row, colByte + 1);
        const wordstartui::sCell* b = screen.GetCell(row, colB);
        REQUIRE(caret != nullptr);
        REQUIRE(letter != nullptr);
        REQUIRE(b != nullptr);
        CHECK(caret->textUtf8 == "^");
        CHECK(letter->textUtf8 == "B");
        CHECK(caret->style.fg == editor.mHighlightFgColour);
        CHECK(caret->style.bg == editor.mHighlightColour);
        CHECK(letter->style.bg == editor.mHighlightColour);
        CHECK(b->textUtf8 == "B");
        CHECK((b->style.attrs & wordstartui::CELL_ATTR_BOLD) == 0);
    }
}


namespace
{

// text of screen columns col..col+count-1 of row
std::string ScreenText(wordstartui::cScreen& screen, int row, int col, int count)
{
    std::string text;
    for (int c = col; c < col + count; c++)
    {
        const wordstartui::sCell* cell = screen.GetCell(row, c);
        text += (cell != nullptr) ? cell->textUtf8 : std::string("?");
    }
    return text;
}

}   // anonymous namespace


TEST_CASE("cWSEditorView - a kept index item shows, moves the caret and deletes as a bold marker does")
{
    // A0 B1 <code>2 C3 D4 \r5, the code a kept index item or a bold marker
    auto build = [](cWSEditorCtrl& editor, bool kept)
    {
        cDocument* doc = editor.GetDocument();
        doc->Insert("AB");
        if (kept)
        {
            sRawSequence sequence;
            sequence.type = 0x0E;
            sequence.body = "Alpha";
            doc->InsertRawSequence(sequence);
        }
        else
        {
            doc->Insert(static_cast<CHAR_T>(STYLE_BOLD));
        }
        doc->Insert("CD\r");
    };

    auto plainText = [](cDocument* doc)
    {
        return doc->GetPlainText(0, doc->GetTextSize(), "\n") + std::to_string(doc->GetTextSize()) +
               std::to_string(doc->GetPosition());
    };

    // the caret stops on Right and Left presses, and the text and caret after
    // Backspace behind the code and Delete on it; the line as drawn, whether
    // the code cell has the control code colours, and whether every Right
    // press moved the caret to a new column
    auto run = [&](bool kept, eShowControl mode, std::string& line, bool& coloured, bool& advances)
    {
        std::vector<std::string> result;
        cWSEditorCtrl editor;
        build(editor, kept);
        editor.GetLayout()->SetShowControl(mode);
        editor.LayoutDocument(true);
        cDocument* doc = editor.GetDocument();
        cWSEditorView view(&editor);
        view.SetBounds({0, 0, 60, 200});

        int colA = CaretColumnAt(editor, view, 0);
        int colCode = colA + 2;
        doc->SetPosition(0);
        wordstartui::cScreen screen;
        screen.Resize(60, 200);
        view.Draw(screen);
        line = ScreenText(screen, view.GetCaretRow(), colA, 12);
        const wordstartui::sCell* cell = screen.GetCell(view.GetCaretRow(), colCode);
        REQUIRE(cell != nullptr);
        coloured = (cell->style.bg == editor.mHighlightColour);

        std::string stops;
        advances = true;
        int lastCol = colA;
        for (int i = 0; i < 4; i++)
        {
            editor.MoveCaretRight();
            int col = CaretColumnAt(editor, view, doc->GetPosition());
            advances = advances && (col > lastCol);
            lastCol = col;
            stops += std::to_string(doc->GetPosition());
        }
        for (int i = 0; i < 4; i++)
        {
            editor.MoveCaretLeft();
            stops += std::to_string(doc->GetPosition());
        }
        result.push_back(stops);

        doc->SetPosition(3);
        editor.Backspace();
        result.push_back(plainText(doc));

        cWSEditorCtrl other;
        build(other, kept);
        other.GetLayout()->SetShowControl(mode);
        other.LayoutDocument(true);
        other.GetDocument()->SetPosition(2);
        other.DeleteKey();
        result.push_back(plainText(other.GetDocument()));
        return result;
    };

    for (eShowControl mode : { SHOW_ALL, SHOW_NONE })
    {
        CAPTURE(static_cast<int>(mode));
        std::string line;
        std::string boldLine;
        bool coloured = false;
        bool boldColoured = false;
        bool advances = false;
        bool boldAdvances = false;
        CHECK(run(true, mode, line, coloured, advances) == run(false, mode, boldLine, boldColoured, boldAdvances));
        CHECK(coloured == boldColoured);
        CHECK(advances == boldAdvances);
        CHECK(line == std::string((mode == SHOW_ALL) ? "AB<Index>CD " : "ABCD        "));
        CHECK(boldLine == std::string((mode == SHOW_ALL) ? "ABBCD       " : "ABCD        "));
    }
}


TEST_CASE("cWSEditorCtrl - ^PK inserts the index mark and ^PX strikethrough")
{
    for (eInputMode mode : { INPUT_WORDSTAR, INPUT_MODERN })
    {
        for (char key : { 'K', 'X', 'k', 'x', static_cast<char>(11), static_cast<char>(24) })
        {
            CAPTURE(static_cast<int>(mode));
            CAPTURE(static_cast<int>(key));

            cWSEditorCtrl editor;
            cDocument* doc = editor.GetDocument();
            editor.SetInputMode(mode);
            doc->Insert("Hello World\r");
            editor.LayoutDocument(true);
            doc->SetPosition(5);

            if (mode == INPUT_WORDSTAR)
            {
                editor.mInput->HandleKey(CTRL_P, false);
            }
            else
            {
                editor.mInput->HandleKey('p', false, true);
            }
            editor.mInput->HandleKey(key, false);

            CHECK(editor.mInput->CheckControlMode() == false);
            CHECK(doc->GetParagraphText(0) == "Hello\x7F World\r");
            bool index = (key == 'K' || key == 'k' || key == 11);
            CHECK(doc->GetControlChar(5) == (index ? STYLE_INDEX : STYLE_STRIKETHROUGH));
        }
    }
}


TEST_CASE("cWSEditorView - an index mark shows K in the control code colours with ^OD on, nothing with it off")
{
    cWSEditorCtrl editor;
    cDocument* doc = editor.GetDocument();
    cLayoutBase* layout = editor.GetLayout();
    REQUIRE(layout != nullptr);

    cWSEditorView view(&editor);
    view.SetBounds({0, 0, 60, 200});

    // A0 ^K1 w2 o3 r4 d5 ^K6 B7 \r8
    doc->Insert("A");
    doc->BeginIndex();
    doc->Insert("word");
    doc->BeginIndex();
    doc->Insert("B\r");

    for (eShowControl mode : { SHOW_ALL, SHOW_DOT, SHOW_NONE })
    {
        CAPTURE(static_cast<int>(mode));
        layout->SetShowControl(mode);
        editor.LayoutDocument(true);

        doc->SetPosition(0);
        wordstartui::cScreen screen;
        screen.Resize(60, 200);
        view.Draw(screen);
        int row = view.GetCaretRow();
        int colA = CaretColumnAt(editor, view, 0);

        if (mode == SHOW_ALL)
        {
            CHECK(ScreenText(screen, row, colA, 8) == "AKwordKB");
            for (int col : { colA + 1, colA + 6 })
            {
                CAPTURE(col);
                const wordstartui::sCell* tag = screen.GetCell(row, col);
                REQUIRE(tag != nullptr);
                CHECK(tag->style.fg == editor.mHighlightFgColour);
                CHECK(tag->style.bg == editor.mHighlightColour);
            }
            const wordstartui::sCell* letter = screen.GetCell(row, colA + 2);
            REQUIRE(letter != nullptr);
            CHECK(letter->style.bg != editor.mHighlightColour);
        }
        else
        {
            CHECK(ScreenText(screen, row, colA, 7) == "AwordB ");
        }
    }
}


TEST_CASE("cWSEditorView - Esc then - inserts an en dash, not a hyphen")
{
    cWSEditorCtrl editor;
    cWSEditorView view(&editor);
    view.SetBounds({0, 0, 30, 120});
    POSITION_T before = editor.GetDocument()->GetTextSize();

    wordstartui::sInputEvent event{};
    event.type = wordstartui::INPUT_TYPE_TEXT;
    event.textUtf8 = "-";
    event.codepoint = '-';
    event.alt = true;
    view.HandleEvent(event);

    CHECK(editor.GetDocument()->GetTextSize() == before + 1);
    CHECK(editor.GetDocument()->GetText().find("\xE2\x80\x93") != std::string::npos);
    CHECK(editor.GetDocument()->GetText().find('-') == std::string::npos);
}
