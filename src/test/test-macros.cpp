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

#include "src/core/document/document.h"
#include "src/core/utils/config.h"
#include "src/input/keystroke.h"
#include "src/input/macroengine.h"
#include "src/test/tocindexgen-testutil.h"

#ifdef WORDTSAR_TUI_BUILD
#include "src/tui/editor/editorctrl.h"
#include "src/tui/editor/editorview.h"
typedef cWSEditorCtrl tMacroTestEditorBase;
#else
#include "src/gui/editor/editorctrl.h"
#include <QApplication>
typedef cEditorCtrl tMacroTestEditorBase;
#endif

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <unistd.h>


namespace
{

#ifndef WORDTSAR_TUI_BUILD
int gMacroArgc = 0;
char* gMacroArgv[] = { nullptr };

void EnsureMacroTestApplication()
{
    if (QApplication::instance() == nullptr)
    {
        new QApplication(gMacroArgc, gMacroArgv);
    }
}
#else
void EnsureMacroTestApplication()
{
}
#endif


/////////////////////////////////////////////////////////////////////////////
///
/// @brief Editor whose Find prompt answers "fox" with option U and whose
/// name prompt, macro list and yes/no question answer without a dialog.
/// In ws, a view runs the front end's update after each typed or played key.
///
/////////////////////////////////////////////////////////////////////////////
class cMacroTestEditor : public tMacroTestEditorBase
{
public:
    cMacroTestEditor()
    {
#ifdef WORDTSAR_TUI_BUILD
        mView = std::make_unique<cWSEditorView>(this);
        mView->SetBounds({0, 0, 60, 200});
        SetMacroKeyHook([this](void) { mView->EnsureCaretVisible(); });
#endif
    }

    void Key(const sKeyStroke& key)
    {
        bool handled = FeedKey(key);
#ifdef WORDTSAR_TUI_BUILD
        if (handled == true)
        {
            mView->EnsureCaretVisible();
        }
        if (HasPendingLayout() == true)
        {
            IdleLayout();
        }
#else
        (void)handled;
#endif
    }

    bool PromptFind(sFindRequest& request) override
    {
        mFindPrompts++;
        request.text = "fox";
        ApplyFindOptionLetters("U", request);
        return true;
    }

    std::string PromptText(const std::string& title, const std::string& label) override
    {
        (void)title;
        (void)label;
        return mName;
    }

    std::string ChooseFromList(const std::string& title, const std::vector<std::string>& items) override
    {
        (void)title;
        mListed = items;
        return mChoice;
    }

    bool AskYesNo(const std::string& title, const std::string& question) override
    {
        (void)title;
        (void)question;
        bool yes = false;
        if (MacroAnswersQuestion(yes) == true)
        {
            return yes;
        }
        return true;
    }

    int mFindPrompts = 0;
    std::string mName = "t1";
    std::string mChoice = "t1";
    std::vector<std::string> mListed;
#ifdef WORDTSAR_TUI_BUILD
    std::unique_ptr<cWSEditorView> mView;
#endif
};


void TypeText(cMacroTestEditor& editor, const std::string& text)
{
    for (char ch : text)
    {
        editor.Key(TextKeyStroke(std::string(1, ch)));
    }
}


void Chord(cMacroTestEditor& editor, char prefix, char follow)
{
    editor.Key(CharKeyStroke(static_cast<char>(prefix - '@')));
    editor.Key(TextKeyStroke(std::string(1, follow)));
}


void Special(cMacroTestEditor& editor, eSpecialKey key)
{
    editor.Key(SpecialKeyStroke(key));
}


std::string ReadBytes(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

}   // anonymous namespace


TEST_CASE("Macro: record typing, a block, a find and a pause, then play it back as one undo step")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    // Record
    cMacroTestEditor recorder;
    cDocument* recDoc = recorder.GetDocument();
    recDoc->Insert("end");
    recDoc->SetPosition(0);
    recorder.LayoutDocument(true);

    Chord(recorder, 'M', 'r');
    sStatus status;
    recorder.GetStatus(status);
    CHECK(status.macroState == "Recording");

    TypeText(recorder, "the quick brown fox");
    Special(recorder, SPECIAL_HOME);
    Chord(recorder, 'K', 'b');
    Special(recorder, SPECIAL_RIGHT);
    Special(recorder, SPECIAL_RIGHT);
    Special(recorder, SPECIAL_RIGHT);
    Chord(recorder, 'K', 'k');
    Special(recorder, SPECIAL_END);
    Chord(recorder, 'K', 'c');
    Special(recorder, SPECIAL_HOME);
    Chord(recorder, 'Q', 'f');
    recorder.Key(TextKeyStroke("-", false, false, true));
    TypeText(recorder, "red ");
    Special(recorder, SPECIAL_ENTER);
    Special(recorder, SPECIAL_END);
    TypeText(recorder, ".");
    recorder.Key(TextKeyStroke("=", false, false, true));

    recorder.GetStatus(status);
    CHECK(status.macroState.empty());
    CHECK(recorder.mFindPrompts == 1);

    const std::string recorded = recDoc->GetText();
    CHECK(recorded.find("the quick brown red foxendthe.") != std::string::npos);

    const std::filesystem::path file = std::filesystem::path(cConfig::GetConfigDir()) / "macros" / "t1.wsm";
    REQUIRE(std::filesystem::exists(file));

    const std::string expected =
        "Sub MAIN\r\n"
        "Key(\"the quick brown fox{Home}^KB{Right}{Right}{Right}^KK{End}^KC\")\r\n"
        "Key(\"{Home}^QFfox{Enter}\")\r\n"
        "Key(\"U{Enter}\")\r\n"
        "PauseForInput(\"{Enter}\", 'Type, then press Enter')\r\n"
        "Key(\"{End}.\")\r\n"
        "End Sub\r\n"
        "\x1A";
    CHECK(ReadBytes(file) == expected);

    // Play in a second editor
    cMacroTestEditor player;
    cDocument* playDoc = player.GetDocument();
    playDoc->Insert("end");
    playDoc->SetPosition(0);
    player.LayoutDocument(true);
    playDoc->ClearUndoHistory();
    const std::string before = playDoc->GetText();

    Chord(player, 'M', 'p');
    CHECK(player.mListed == std::vector<std::string>{ "CHAPTER", "INDEX", "NEXTCH", "PREVCH", "SCENE", "t1" });
    player.GetStatus(status);
    CHECK(status.macroState == "Type, then press Enter");

    TypeText(player, "blue ");
    Special(player, SPECIAL_ENTER);
    player.GetStatus(status);
    CHECK(status.macroState.empty());

    std::string played = recorded;
    played.replace(played.find("red "), 4, "blue ");
    CHECK(playDoc->GetText() == played);
    CHECK(player.mFindPrompts == 0);

    CHECK(player.Undo() == true);
    CHECK(playDoc->GetText() == before);

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: a recording is saved as WordStar 7 macro source and a .txt macro is converted")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    cMacroTestEditor recorder;
    Chord(recorder, 'M', 'r');
    TypeText(recorder, "say \"hi\" {ok}");
    Special(recorder, SPECIAL_HOME);
    Chord(recorder, 'K', 'b');
    Special(recorder, SPECIAL_END);
    Chord(recorder, 'K', 'k');
    Special(recorder, SPECIAL_ENTER);
    recorder.Key(TextKeyStroke("-", false, false, true));
    TypeText(recorder, "name");
    Special(recorder, SPECIAL_ENTER);
    TypeText(recorder, "!");
    recorder.Key(TextKeyStroke("=", false, false, true));
    const std::string recorded = recorder.GetDocument()->GetText();

    const std::filesystem::path folder = std::filesystem::path(cConfig::GetConfigDir()) / "macros";
    const std::filesystem::path file = folder / "t1.wsm";
    REQUIRE(std::filesystem::exists(file));
    const std::string source =
        "Sub MAIN\r\n"
        "Key(\"say \"\"hi\"\" {{ok}}{Home}^KB{End}^KK{Enter}\")\r\n"
        "PauseForInput(\"{Enter}\", 'Type, then press Enter')\r\n"
        "Key(\"!\")\r\n"
        "End Sub\r\n"
        "\x1A";
    CHECK(ReadBytes(file) == source);

    cMacroTestEditor player;
    Chord(player, 'M', 'p');
    TypeText(player, "who");
    Special(player, SPECIAL_ENTER);
    std::string played = recorded;
    played.replace(played.find("name"), 4, "who");
    CHECK(player.GetDocument()->GetText() == played);

    // ^MD opens the file as a text document; saving it keeps the bytes.
    cMacroTestEditor edit;
    REQUIRE(edit.LoadFile(file.string()));
    CHECK(edit.GetDocument()->IsTextFile());
    REQUIRE(edit.SaveFile((folder / "t2.wsm").string()));
    CHECK(ReadBytes(folder / "t2.wsm") == source);

    // A one-key-per-line .txt macro is converted when ^MP lists it.
    {
        std::ofstream old(folder / "old.txt", std::ios::binary);
        old << "a\nb\nEnter\n";
    }
    player.mChoice.clear();
    Chord(player, 'M', 'p');
    CHECK(player.mListed == std::vector<std::string>{ "CHAPTER", "INDEX", "NEXTCH", "PREVCH", "SCENE", "old", "t1", "t2" });
    CHECK(ReadBytes(folder / "old.wsm") == "Sub MAIN\r\nKey(\"ab{Enter}\")\r\nEnd Sub\r\n\x1A");
    CHECK(std::filesystem::exists(folder / "old.txt") == false);

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: a Find from the menu is recorded with ^QF and plays without typing its answer")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    cMacroTestEditor recorder;
    recorder.GetDocument()->Insert("the quick brown fox");
    recorder.GetDocument()->SetPosition(0);
    recorder.LayoutDocument(true);

    Chord(recorder, 'M', 'r');
    recorder.Find();
    recorder.Key(TextKeyStroke("-", false, false, true));
    Special(recorder, SPECIAL_ENTER);
    recorder.Key(TextKeyStroke("=", false, false, true));

    const std::filesystem::path file = std::filesystem::path(cConfig::GetConfigDir()) / "macros" / "t1.wsm";
    REQUIRE(std::filesystem::exists(file));
    CHECK(ReadBytes(file) == "Sub MAIN\r\n"
                             "Key(\"^QFfox{Enter}\")\r\n"
                             "Key(\"U{Enter}\")\r\n"
                             "PauseForInput(\"{Enter}\", 'Type, then press Enter')\r\n"
                             "End Sub\r\n"
                             "\x1A");

    cMacroTestEditor player;
    cDocument* playDoc = player.GetDocument();
    playDoc->Insert("the quick brown fox");
    playDoc->SetPosition(0);
    player.LayoutDocument(true);
    const std::string before = playDoc->GetText();

    Chord(player, 'M', 'p');
    CHECK(playDoc->GetText() == before);
    CHECK(playDoc->GetPosition() != 0);
    CHECK(player.mFindPrompts == 0);

    player.Find();
    CHECK(player.mFindPrompts == 1);
    Special(player, SPECIAL_ENTER);
    CHECK(player.IsMacroPlaying() == false);

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: Alt+- and Alt+= type an en dash and a not-equal sign unless recording")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    cMacroTestEditor editor;
    cDocument* doc = editor.GetDocument();

    editor.Key(TextKeyStroke("-", false, false, true));
    editor.Key(TextKeyStroke("=", false, false, true));
    CHECK(doc->GetText().find("\xE2\x80\x93\xE2\x89\xA0") != std::string::npos);

    Chord(editor, 'M', 'r');
    const std::string recording = doc->GetText();
    editor.Key(TextKeyStroke("-", false, false, true));
    Special(editor, SPECIAL_ENTER);
    editor.Key(TextKeyStroke("=", false, false, true));
    CHECK(doc->GetText() == recording);
    CHECK(editor.IsMacroRecording() == false);

    const std::filesystem::path file = std::filesystem::path(cConfig::GetConfigDir()) / "macros" / "t1.wsm";
    REQUIRE(std::filesystem::exists(file));
    CHECK(ReadBytes(file) == "Sub MAIN\r\n"
                             "PauseForInput(\"{Enter}\", 'Type, then press Enter')\r\n"
                             "End Sub\r\n"
                             "\x1A");

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: a recording is saved in the macros folder next to the document if there is one")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");
    const std::filesystem::path withMacros = home / "with";
    const std::filesystem::path withoutMacros = home / "without";
    std::filesystem::create_directories(withMacros / "macros");
    std::filesystem::create_directories(withoutMacros);

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    const std::filesystem::path config = std::filesystem::path(cConfig::GetConfigDir()) / "macros" / "t1.wsm";

    cMacroTestEditor editor;
    editor.mFileSet = true;
    editor.mFileDir = withMacros.string();
    Chord(editor, 'M', 'r');
    TypeText(editor, "a");
    editor.Key(TextKeyStroke("=", false, false, true));
    CHECK(std::filesystem::exists(withMacros / "macros" / "t1.wsm"));
    CHECK(std::filesystem::exists(config) == false);

    editor.mFileDir = withoutMacros.string();
    Chord(editor, 'M', 'r');
    TypeText(editor, "b");
    editor.Key(TextKeyStroke("=", false, false, true));
    CHECK(std::filesystem::exists(config));
    CHECK(std::filesystem::exists(withoutMacros / "macros") == false);

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: spaces around a macro name are trimmed")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    cMacroTestEditor editor;
    editor.mName = "  mac  ";
    Chord(editor, 'M', 'r');
    TypeText(editor, "a");
    editor.Key(TextKeyStroke("=", false, false, true));

    const std::filesystem::path folder = std::filesystem::path(cConfig::GetConfigDir()) / "macros";
    CHECK(std::filesystem::exists(folder / "mac.wsm"));
    CHECK(std::filesystem::exists(folder / "  mac  .wsm") == false);

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: a missing config macros folder is created with the default macros")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    const std::filesystem::path folder = std::filesystem::path(cConfig::GetConfigDir()) / "macros";
    REQUIRE(std::filesystem::exists(folder) == false);

    cMacroTestEditor editor;
    editor.mChoice.clear();
    Chord(editor, 'M', 'p');
    CHECK(editor.mListed == std::vector<std::string>{ "CHAPTER", "INDEX", "NEXTCH", "PREVCH", "SCENE" });
    for (const char* name : { "CHAPTER", "INDEX", "NEXTCH", "PREVCH", "SCENE" })
    {
        const std::filesystem::path file = folder / (std::string(name) + ".wsm");
        REQUIRE(std::filesystem::exists(file));
        CHECK(ReadBytes(file) == ReadBytes(std::filesystem::path("../templates/macros") / (std::string(name) + ".wsm")));
    }
    CHECK(std::filesystem::exists(folder / "SETUP.wsm") == false);

    editor.mChoice = "INDEX";
    Chord(editor, 'M', 'y');
    editor.mChoice.clear();
    editor.mListed.clear();
    Chord(editor, 'M', 'p');
    CHECK(editor.mListed == std::vector<std::string>{ "CHAPTER", "NEXTCH", "PREVCH", "SCENE" });

    for (const char* name : { "CHAPTER", "NEXTCH", "PREVCH", "SCENE" })
    {
        std::filesystem::remove(folder / (std::string(name) + ".wsm"));
    }
    editor.mListed.clear();
    Chord(editor, 'M', 'p');
    CHECK(editor.mListed.empty());
    CHECK(editor.GetStatusMessage() == "No macros yet. Record one with ^MR.");
    CHECK(std::filesystem::exists(folder / "CHAPTER.wsm") == false);

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: INDEX marks a word followed by punctuation without the punctuation")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    cMacroTestEditor editor;
    cDocument* doc = editor.GetDocument();
    doc->Insert("Alpha word, beta");
    doc->SetPosition(8);
    editor.LayoutDocument(true);
    editor.mChoice = "INDEX";
    Chord(editor, 'M', 'p');
    editor.LayoutDocument(true);

    std::string indexPath;
    REQUIRE(cTOCIndexGenerator::GenerateIndex(&editor, (home / "source.ws").string(), indexPath));
    tMacroTestEditorBase reader;
    CHECK(tocindextest::Join(tocindextest::LoadGenerated(reader, indexPath)) == "word, 1");

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: INDEX marks the last word of a paragraph followed by another paragraph")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    cMacroTestEditor editor;
    cDocument* doc = editor.GetDocument();
    doc->Insert("Alpha word.");
    doc->Insert(HARD_RETURN);
    doc->Insert("Beta gamma");
    editor.LayoutDocument(true);

    doc->SetPosition(8);
    editor.Key(CharKeyStroke(static_cast<char>('F' - '@')));
    CHECK(doc->GetPosition() == 12);

    doc->SetPosition(8);
    editor.mChoice = "INDEX";
    Chord(editor, 'M', 'p');
    editor.LayoutDocument(true);

    std::string indexPath;
    REQUIRE(cTOCIndexGenerator::GenerateIndex(&editor, (home / "source.ws").string(), indexPath));
    tMacroTestEditorBase reader;
    CHECK(tocindextest::Join(tocindextest::LoadGenerated(reader, indexPath)) == "word, 1");

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


namespace
{

// The file in dir or a folder below it whose name is name.WSM in any case.
std::filesystem::path FindArchiveMacro(const std::filesystem::path& dir, const std::string& name)
{
    std::error_code ec;
    std::string wanted = name + ".wsm";
    for (char& c : wanted)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    for (std::filesystem::recursive_directory_iterator it(dir, std::filesystem::directory_options::skip_permission_denied, ec), end;
         (ec.value() == 0) && (it != end); it.increment(ec))
    {
        std::string file = it->path().filename().string();
        for (char& c : file)
        {
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
        std::error_code fileEc;
        if ((file == wanted) && (it->is_regular_file(fileEc) == true))
        {
            return it->path();
        }
    }
    return std::filesystem::path();
}


// Document text with control codes as ^B, ^Y, ... and hard returns as |.
std::string MarkedText(cDocument* doc)
{
    std::string text;
    POSITION_T size = doc->GetTextSize();
    for (POSITION_T pos = 0; pos < size; pos++)
    {
        std::string grapheme = doc->GetCharNoAdvance(pos);
        if ((grapheme.empty() == false) && (grapheme[0] == static_cast<char>(MARKER_CHAR)))
        {
            text += std::string("^") + static_cast<char>('@' + doc->GetControlChar(pos));
        }
        else if (grapheme == std::string(1, static_cast<char>(HARD_RETURN)))
        {
            text += "|";
        }
        else
        {
            text += grapheme;
        }
    }
    return text;
}

}   // anonymous namespace


TEST_CASE("Macro: Sawyer's TRIM and TITLES do what their REM lines say")
{
    const char* archiveEnv = std::getenv("WORDTSAR_SAWYER_DIR");
    std::error_code ec;
    if ((archiveEnv == nullptr) || (std::filesystem::is_directory(archiveEnv, ec) == false))
    {
        MESSAGE("WORDTSAR_SAWYER_DIR not set or missing: Sawyer macro test skipped");
        return;
    }
    const std::filesystem::path trimSource = FindArchiveMacro(archiveEnv, "TRIM");
    const std::filesystem::path titlesSource = FindArchiveMacro(archiveEnv, "TITLES");
    if ((trimSource.empty() == true) || (titlesSource.empty() == true))
    {
        MESSAGE("TRIM.WSM or TITLES.WSM not in WORDTSAR_SAWYER_DIR: Sawyer macro test skipped");
        return;
    }

    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    const std::filesystem::path folder = std::filesystem::path(cConfig::GetConfigDir()) / "macros";
    std::filesystem::create_directories(folder);
    std::filesystem::copy_file(trimSource, folder / "TRIM.wsm");
    std::filesystem::copy_file(titlesSource, folder / "TITLES.wsm");

    // TRIM removes spaces at the ends of lines; a comment dot command,
    // hidden by the macro while it runs, keeps them.
    {
        cMacroTestEditor editor;
        cDocument* doc = editor.GetDocument();
        doc->Insert("Alpha   ");
        doc->Insert(HARD_RETURN);
        doc->Insert(".. note  ");
        doc->Insert(HARD_RETURN);
        doc->Insert("Beta ");
        doc->Insert(HARD_RETURN);
        doc->Insert("Gamma");
        doc->SetPosition(3);
        editor.LayoutDocument(true);
        const eShowControl show = editor.GetShowControls();

        editor.mChoice = "TRIM";
        Chord(editor, 'M', 'p');

        CHECK(editor.IsMacroPlaying() == false);
        const std::string trimmed = "Alpha|.. note  |Beta|Gamma";
        CHECK(MarkedText(doc).substr(0, trimmed.size()) == trimmed);
        CHECK(doc->GetPosition() == 0);
        CHECK(editor.GetShowControls() == show);
    }

    // TITLES turns the titles written in the macro into bold italics,
    // whatever their case; the sample holds two of them.
    {
        cMacroTestEditor editor;
        cDocument* doc = editor.GetDocument();
        doc->Insert("Books: Red Planet Blues and QUANTUM NIGHT.");
        doc->SetPosition(0);
        editor.LayoutDocument(true);

        editor.mChoice = "TITLES";
        Chord(editor, 'M', 'p');

        CHECK(editor.IsMacroPlaying() == false);
        const std::string titled = "Books: ^B^YRed Planet Blues^B^Y and ^B^YQuantum Night^B^Y.";
        CHECK(MarkedText(doc).substr(0, titled.size()) == titled);
    }

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::filesystem::remove_all(home, ec);
}


TEST_CASE("Macro: a macro file named NAME.WSM is listed by ^MP and plays")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    const std::filesystem::path folder = std::filesystem::path(cConfig::GetConfigDir()) / "macros";
    std::filesystem::create_directories(folder);
    {
        std::ofstream out(folder / "UPPER.WSM", std::ios::binary);
        out << "Sub MAIN\r\nKey(\"upper\")\r\nEnd Sub\r\n\x1A";
    }

    cMacroTestEditor editor;
    editor.mChoice = "UPPER";
    Chord(editor, 'M', 'p');

    CHECK(std::find(editor.mListed.begin(), editor.mListed.end(), "UPPER") != editor.mListed.end());
    CHECK(editor.GetDocument()->GetText().find("upper") == 0);

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


namespace
{

// Key sink of a screen without a document: counts the keys it receives.
class cCountingSink : public IMacroKeySink
{
public:
    bool FeedMacroKey(const sKeyStroke& key) override
    {
        (void)key;
        mKeys++;
        return true;
    }
    bool EditsDocument(void) const override
    {
        return false;
    }
    int mKeys = 0;
};


void WriteMacro(const std::filesystem::path& folder, const std::string& name, const std::string& body)
{
    std::ofstream out(folder / (name + ".wsm"), std::ios::binary);
    out << "Sub MAIN\r\n" << body << "End Sub\r\n\x1A";
}

}   // anonymous namespace


TEST_CASE("Macro: keys to a screen without a document are checked, runaway macros stop, a setting keeps the answer key")
{
    EnsureMacroTestApplication();

    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    const std::filesystem::path folder = std::filesystem::path(cConfig::GetConfigDir()) / "macros";
    std::filesystem::create_directories(folder);
    WriteMacro(folder, "OPENKO", "Key(\"^KO\")\r\n");
    WriteMacro(folder, "LOOP", "Again:\r\nGoto Again\r\n");
    WriteMacro(folder, "SELF", "PlayMacro 'SELF'\r\n");
    WriteMacro(folder, "DEEP", "Call Down\r\nEnd Sub\r\nSub Down\r\nKey(\"d\")\r\nCall Down\r\n");
    WriteMacro(folder, "ANSWER", "Key(\"^QFzzz{Enter}{Enter}\")\r\nIfException\r\nACK: Insert(ON)\r\n"
                                 "Key(\"{Enter}ok\")\r\nEnd IfException\r\n");

    // A command WordTsar lacks is refused before any key reaches the sink.
    {
        cMacroTestEditor editor;
        cCountingSink sink;
        editor.SetMacroKeySink(&sink);
        editor.mChoice = "OPENKO";
        Chord(editor, 'M', 'p');
        editor.SetMacroKeySink(nullptr);
        CHECK(sink.mKeys == 0);
        CHECK(editor.GetStatusMessage() == "Not supported in WordTsar: ^KO, line 2");
    }

    for (const char* name : { "LOOP", "SELF", "DEEP" })
    {
        CAPTURE(name);
        cMacroTestEditor editor;
        editor.mChoice = name;
        Chord(editor, 'M', 'p');
        CHECK(editor.IsMacroPlaying() == false);
        CHECK(editor.GetStatusMessage().find("Macro stopped") == 0);
    }

    // The ACK clause starts with a setting; its first key still answers the message.
    {
        cMacroTestEditor editor;
        editor.GetDocument()->Insert("text");
        editor.GetDocument()->SetPosition(0);
        editor.LayoutDocument(true);
        editor.mChoice = "ANSWER";
        Chord(editor, 'M', 'p');
        CHECK(editor.GetDocument()->GetText().find("oktext") == 0);
    }

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}


namespace
{

// Plays body as macro name from a fresh config macros folder; returns the
// document text and the status message.
struct sPlayed
{
    std::string text;
    std::string status;
};

sPlayed PlayInlineMacro(const std::string& name, const std::string& body, const std::string& start)
{
    std::string pattern = (std::filesystem::temp_directory_path() / "wordtsar-macro-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    const std::filesystem::path home = pattern;
    std::filesystem::create_directories(home / ".config");

    const char* oldHomeEnv = std::getenv("HOME");
    const char* oldXdgEnv = std::getenv("XDG_CONFIG_HOME");
    const std::string oldHome = (oldHomeEnv != nullptr) ? oldHomeEnv : "";
    const std::string oldXdg = (oldXdgEnv != nullptr) ? oldXdgEnv : "";
    setenv("HOME", home.c_str(), 1);
    setenv("XDG_CONFIG_HOME", (home / ".config").c_str(), 1);

    const std::filesystem::path folder = std::filesystem::path(cConfig::GetConfigDir()) / "macros";
    std::filesystem::create_directories(folder);
    {
        std::ofstream out(folder / (name + ".wsm"), std::ios::binary);
        std::string source = body;
        size_t at = source.find("$HOME");
        if (at != std::string::npos)
        {
            source.replace(at, 5, home.string());
        }
        out << "Sub MAIN\r\n" << source << "End Sub\r\n\x1A";
    }
    {
        std::ofstream out(home / "insert.txt", std::ios::binary);
        out << "inserted ";
    }

    sPlayed played;
    {
        cMacroTestEditor editor;
        TypeText(editor, start);
        editor.GetDocument()->SetPosition(0);
        editor.LayoutDocument(true);
        editor.mChoice = name;
        Chord(editor, 'M', 'p');
        played.text = editor.GetDocument()->GetText();
        played.status = editor.GetStatusMessage();
    }

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
    return played;
}

}   // anonymous namespace


TEST_CASE("Macro: ^B is accepted and changes nothing")
{
    EnsureMacroTestApplication();
    sPlayed played = PlayInlineMacro("ALIGN", "Key(\"ab^Bc\")\r\n", "text");
    CHECK(played.status.empty());
    CHECK(played.text.find("abctext") == 0);
}


TEST_CASE("Macro: a QRY clause typing a dialog answer up to F10 answers the question")
{
    EnsureMacroTestApplication();
    sPlayed played = PlayInlineMacro("ASKED", "Key(\"^KQ\")\r\nIfException\r\nQRY: Key(\"no.ws{F10}x\")\r\n"
                                              "End IfException\r\n", "text");
    CHECK(played.status.empty());
    CHECK(played.text.find("xtext") == 0);
}
