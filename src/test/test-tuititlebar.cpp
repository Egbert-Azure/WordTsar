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
#include "src/core/document/document.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
