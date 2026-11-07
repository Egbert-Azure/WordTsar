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

#ifdef WORDTSAR_TUI_BUILD
#include "src/tui/editor/editorctrl.h"
#include "src/tui/editor/editorview.h"
typedef cWSEditorCtrl tMacroTestEditorBase;
#else
#include "src/gui/editor/editorctrl.h"
#include <QApplication>
typedef cEditorCtrl tMacroTestEditorBase;
#endif

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
        return "t1";
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
        return true;
    }

    int mFindPrompts = 0;
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


std::vector<std::string> ReadLines(const std::filesystem::path& path)
{
    std::vector<std::string> lines;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        lines.push_back(line);
    }
    return lines;
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

    const std::filesystem::path file = std::filesystem::path(cConfig::GetConfigDir()) / "macros" / "t1.txt";
    REQUIRE(std::filesystem::exists(file));

    const std::vector<std::string> expected =
    {
        "t", "h", "e", "Space", "q", "u", "i", "c", "k", "Space", "b", "r", "o", "w", "n", "Space", "f", "o", "x",
        "Home", "^KB", "Right", "Right", "Right", "^KK", "End", "^KC", "Home",
        "^QF", "f", "o", "x", "Enter", "U", "Enter",
        "PAUSE", "End", "."
    };
    CHECK(ReadLines(file) == expected);

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
        const std::filesystem::path file = folder / (std::string(name) + ".txt");
        REQUIRE(std::filesystem::exists(file));
        CHECK(ReadBytes(file) == ReadBytes(std::filesystem::path("../templates/macros") / (std::string(name) + ".txt")));
    }
    CHECK(std::filesystem::exists(folder / "SETUP.txt") == false);

    editor.mChoice = "INDEX";
    Chord(editor, 'M', 'y');
    editor.mChoice.clear();
    editor.mListed.clear();
    Chord(editor, 'M', 'p');
    CHECK(editor.mListed == std::vector<std::string>{ "CHAPTER", "NEXTCH", "PREVCH", "SCENE" });

    setenv("HOME", oldHome.c_str(), 1);
    setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
    std::error_code ec;
    std::filesystem::remove_all(home, ec);
}
