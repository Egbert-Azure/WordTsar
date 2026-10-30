//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
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

#include "doctest.h"

#include "src/tui/wordtsar.h"
#include "src/tui/editor/editorctrl.h"
#include "src/input/wordtsarinput.h"
#include "src/tui/dialogs/dialoghost.h"
#include "src/tui/wordstartui/src/dialog.h"
#include "src/tui/wordstartui/src/theme.h"
#include "src/tui/wordstartui/src/tuidefs.h"
#include "src/core/include/version.h"

#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <unistd.h>


namespace
{

/////////////////////////////////////////////////////////////////////////////
///
/// @brief Temporary folder removed when the object goes out of scope.
///
/////////////////////////////////////////////////////////////////////////////
class cTempDir
{
public:
    cTempDir()
    {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "wordtsar-titlebar-XXXXXX").string();
        REQUIRE(mkdtemp(pattern.data()) != nullptr);
        mDir = pattern;
    }

    ~cTempDir()
    {
        std::error_code ec;
        std::filesystem::remove_all(mDir, ec);
    }

    std::string WriteText(const std::string& name, const std::string& text)
    {
        std::filesystem::path path = mDir / name;
        std::ofstream out(path);
        out << text;
        return path.string();
    }

    std::string Path(const std::string& name) const
    {
        return (mDir / name).string();
    }

private:
    std::filesystem::path mDir;
};

bool StartsWith(const std::string& text, const std::string& prefix)
{
    return text.compare(0, prefix.size(), prefix) == 0;
}

std::string ReadText(const std::string& path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

/////////////////////////////////////////////////////////////////////////////
///
/// @brief Changes the working directory and restores it on scope exit.
///
/////////////////////////////////////////////////////////////////////////////
class cWorkingDir
{
public:
    explicit cWorkingDir(const std::string& dir)
        : mPrevious(std::filesystem::current_path())
    {
        std::filesystem::current_path(dir);
    }

    ~cWorkingDir()
    {
        std::error_code ec;
        std::filesystem::current_path(mPrevious, ec);
    }

private:
    std::filesystem::path mPrevious;
};

/////////////////////////////////////////////////////////////////////////////
///
/// @brief Dialog host that feeds queued key events to modal dialogs and
///        counts the file browser modals it runs.
///
/////////////////////////////////////////////////////////////////////////////
class cScriptedHost : public iWSDialogHost
{
public:
    void QueueEnter(void)
    {
        wordstartui::sInputEvent event{};
        event.type = wordstartui::INPUT_TYPE_SPECIAL;
        event.special = wordstartui::SPECIAL_KEY_ENTER;
        mEvents.push_back(event);
    }

    void QueueEscape(void)
    {
        wordstartui::sInputEvent event{};
        event.type = wordstartui::INPUT_TYPE_SPECIAL;
        event.special = wordstartui::SPECIAL_KEY_ESCAPE;
        mEvents.push_back(event);
    }

    void QueueText(const std::string& text)
    {
        wordstartui::sInputEvent event{};
        event.type = wordstartui::INPUT_TYPE_TEXT;
        event.textUtf8 = text;
        mEvents.push_back(event);
    }

    void HostRunModal(wordstartui::cDialog& dialog) override
    {
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
        mRawModals = mRawModals + 1;
        while (true)
        {
            if (mEvents.empty() == true)
            {
                QueueEscape();
            }
            wordstartui::sInputEvent event = mEvents.front();
            mEvents.pop_front();
            if (handle(event) == false)
            {
                break;
            }
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

    int mRawModals = 0;

private:
    std::deque<wordstartui::sInputEvent> mEvents;
    wordstartui::cTheme mTheme;
};

}


TEST_CASE("TUI title bar - shows the file the editor holds")
{
    cTempDir dir;
    std::string first = dir.WriteText("first.txt", "first file\n");
    std::string second = dir.WriteText("second.txt", "second file\n");

    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    REQUIRE(editor != nullptr);
    app.TestOpenFromOpening(first);
    REQUIRE(editor->mFileName == "first.txt");

    SUBCASE("after loading a second file")
    {
        REQUIRE(editor->LoadFile(second) == true);
        CHECK(StartsWith(app.TestTitleText(), "second.txt - WordTsar "));
    }

    SUBCASE("after saving under a new name")
    {
        REQUIRE(editor->SaveFile(dir.Path("renamed.txt")) == true);
        CHECK(StartsWith(app.TestTitleText(), "renamed.txt - WordTsar "));
    }

    SUBCASE("after ^KD")
    {
        cWordStarInput input(editor);
        input.OnControlKChar('d');
        CHECK(app.TestTitleText().find("first.txt") == std::string::npos);
        CHECK(StartsWith(app.TestTitleText(), "untitled - WordTsar "));
    }
}


TEST_CASE("TUI title bar - file name and version without status")
{
    cTempDir dir;
    std::string file = dir.WriteText("plain.txt", "plain file\n");

    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    REQUIRE(editor != nullptr);
    CHECK(app.TestTitleText() == "untitled - WordTsar " + std::string(FULLVERSION_STRING));

    app.TestOpenFromOpening(file);
    REQUIRE(editor->mFileName == "plain.txt");
    std::string title = app.TestTitleText();
    CHECK(title == "plain.txt - WordTsar " + std::string(FULLVERSION_STRING));
    CHECK(title.find("Beta") == std::string::npos);
    CHECK(title.back() != ' ');
}


TEST_CASE("TUI title bar - untitled document after a save")
{
    cTempDir dir;

    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    REQUIRE(editor != nullptr);
    REQUIRE(editor->mFileSet == false);
    REQUIRE(StartsWith(app.TestTitleText(), "untitled - WordTsar "));

    SUBCASE("save shows the saved name")
    {
        REQUIRE(editor->SaveFile(dir.Path("saved.txt")) == true);
        CHECK(editor->mFileSet == true);
        CHECK(StartsWith(app.TestTitleText(), "saved.txt - WordTsar "));
    }

    SUBCASE("emergency save keeps the document untitled")
    {
        editor->mFileDir = dir.Path("");
        editor->InsertText("unsaved text");
        std::string emergency = dir.Path(editor->mFileName + ".emergency");
        editor->EmergencySaveFile(nullptr);
        REQUIRE(std::filesystem::exists(emergency) == true);
        CHECK(editor->mFileSet == false);
        CHECK(StartsWith(app.TestTitleText(), "untitled - WordTsar "));
    }
}


TEST_CASE("TUI close prompt - Save on an untitled document asks for a name")
{
    cTempDir dir;
    cTempDir cwd;
    cWorkingDir inCwd(cwd.Path(""));
    std::string chosen = dir.Path("chosen.txt");

    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    REQUIRE(editor != nullptr);
    cScriptedHost host;
    editor->SetDialogHost(&host);
    REQUIRE(editor->mFileSet == false);
    editor->InsertText("first text");
    REQUIRE(editor->GetDocument()->IsModified() == true);

    host.QueueEnter();
    host.QueueText(chosen);
    host.QueueEnter();

    CHECK(editor->CloseEvent() == true);
    CHECK(host.mRawModals == 1);
    CHECK(std::filesystem::exists(chosen) == true);
    CHECK(std::filesystem::is_empty(cwd.Path("")) == true);
    CHECK(editor->mFileDir == std::filesystem::path(chosen).parent_path().string());
    CHECK(editor->mFileName == "chosen.txt");
    CHECK(StartsWith(app.TestTitleText(), "chosen.txt - WordTsar "));

    editor->InsertText(" more");
    REQUIRE(editor->GetDocument()->IsModified() == true);
    cWordStarInput input(editor);
    input.OnControlKChar('s');
    CHECK(editor->GetDocument()->IsModified() == false);
    CHECK(ReadText(chosen).find("more") != std::string::npos);
    CHECK(std::filesystem::is_empty(cwd.Path("")) == true);

    editor->SetDialogHost(&app);
}


TEST_CASE("TUI close prompt - cancelled name prompt keeps an untitled document open")
{
    cTempDir cwd;
    cWorkingDir inCwd(cwd.Path(""));

    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    REQUIRE(editor != nullptr);
    cScriptedHost host;
    editor->SetDialogHost(&host);
    editor->InsertText("unsaved text");
    REQUIRE(editor->GetDocument()->IsModified() == true);

    host.QueueEnter();
    host.QueueEscape();

    CHECK(editor->CloseEvent() == false);
    CHECK(host.mRawModals == 1);
    CHECK(std::filesystem::is_empty(cwd.Path("")) == true);
    CHECK(editor->GetDocument()->IsModified() == true);
    CHECK(editor->mFileSet == false);

    editor->SetDialogHost(&app);
}


TEST_CASE("TUI close prompt - Save on a named document saves in place")
{
    cTempDir dir;
    cTempDir cwd;
    cWorkingDir inCwd(cwd.Path(""));
    std::string named = dir.WriteText("named.txt", "named file\n");

    cWSWordTsar app;
    cWSEditorCtrl* editor = app.TestGetEditor();
    REQUIRE(editor != nullptr);
    app.TestOpenFromOpening(named);
    REQUIRE(editor->mFileName == "named.txt");
    cScriptedHost host;
    editor->SetDialogHost(&host);
    editor->InsertText("added ");
    REQUIRE(editor->GetDocument()->IsModified() == true);

    host.QueueEnter();

    CHECK(editor->CloseEvent() == true);
    CHECK(host.mRawModals == 0);
    CHECK(ReadText(named).find("added") != std::string::npos);
    CHECK(std::filesystem::is_empty(cwd.Path("")) == true);

    editor->SetDialogHost(&app);
}
