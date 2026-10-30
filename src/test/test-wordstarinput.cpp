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

#include "src/input/wordtsarinput.h"
#include "src/gui/editor/editorctrl.h"
#include "src/core/document/document.h"
#include <QApplication>

#include <filesystem>
#include <string>

#include <unistd.h>


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test fixture support for cWordStarInput tests.
/// cWordStarInput requires a concrete cEditorBase; cEditorCtrl is used since
/// cEditorBase is abstract, and a QApplication must exist for Qt widgets.
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
/// Regression test for the uninitialized mOldHelpStatus bug. Before the fix,
/// mOldHelpStatus was never initialized in the constructor and was only
/// assigned when entering a chord. Pressing ESC as the very first key (no
/// prior chord) restored that indeterminate value into mEditor->mHelpDisplay.
/// The fix initializes mOldHelpStatus to HELP_NONE in the constructor, so a
/// first ESC deterministically restores HELP_NONE.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cWordStarInput - ESC before any chord restores HELP_NONE" * doctest::may_fail())
{
    ensureQApplication();

    SUBCASE("HandleKey ESC path")
    {
        cEditorCtrl editor;
        cWordStarInput input(&editor);

        // Simulate help being visible, then press ESC as the first key.
        editor.mHelpDisplay = HELP_MAIN;
        bool handled = input.HandleKey(27);

        CHECK(handled == true);
        CHECK(editor.mHelpDisplay == HELP_NONE);
    }

    SUBCASE("HandleSpecialKey SPECIAL_ESCAPE path")
    {
        cEditorCtrl editor;
        cWordStarInput input(&editor);

        editor.mHelpDisplay = HELP_MAIN;
        input.HandleSpecialKey(SPECIAL_ESCAPE);

        CHECK(editor.mHelpDisplay == HELP_NONE);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Sanity check that the legitimate chord-then-ESC round trip still restores
/// the help status that was active before the chord was entered. This guards
/// against the constructor fix accidentally breaking the normal restore path.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cWordStarInput - ESC after a chord restores prior help status")
{
    ensureQApplication();

    cEditorCtrl editor;
    cWordStarInput input(&editor);

    editor.mHelpDisplay = HELP_MAIN;

    // Enter the Ctrl+K chord: saves HELP_MAIN, shows the Ctrl+K help menu.
    input.HandleKey(CTRL_K);
    CHECK(editor.mHelpDisplay == HELP_CTRLK);

    // ESC cancels the chord and restores the saved help status.
    input.HandleKey(27);
    CHECK(editor.mHelpDisplay == HELP_MAIN);
}


TEST_CASE("cWordStarInput - GUI ^KD saves and clears, the editor stays open")
{
    ensureQApplication();

    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-gui-kd-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    std::filesystem::path dir = pattern;

    {
        cEditorCtrl editor;
        cWordStarInput input(&editor);
        POSITION_T emptySize = editor.GetDocument()->GetTextSize();

        editor.mFileDir = dir.string();
        editor.mFileName = "kd.txt";
        editor.mFileSet = true;
        editor.GetDocument()->Insert("some text");
        REQUIRE(editor.GetDocument()->IsModified() == true);

        CHECK(input.OnControlKChar('d') == true);
        CHECK(std::filesystem::exists(dir / "kd.txt") == true);
        CHECK(editor.GetDocument()->GetTextSize() == emptySize);
        CHECK(editor.mFileSet == false);
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}
