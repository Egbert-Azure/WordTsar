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

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "src/core/include/config.h"
#include "src/core/document/document.h"
#include "src/files/wordstar/wordstarfile.h"
#include "src/gui/editor/editorctrl.h"
#include <QApplication>


namespace
{

int argc = 0;
char* argv[] = {nullptr};

void ensureQApplication(void)
{
    if (!QApplication::instance())
    {
        new QApplication(argc, argv);
    }
}

// Temporary file path unique to this test file
std::filesystem::path TempPath(void)
{
    static int counter = 0;
    counter++;
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "wordtsar_wordstarfile_test_" + std::to_string(counter) + ".ws";
    return path;
}

void WriteFileBytes(const std::filesystem::path &path, const std::string &bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

// 0x1D sequence: size (LE, counts start + size + type + body), type, body, 3 trailer bytes
std::string SequenceBytes(unsigned char type, const std::string &body)
{
    uint16_t size = static_cast<uint16_t>(body.size() + 4);
    std::string seq;
    seq += '\x1D';
    seq += static_cast<char>(size & 0xFF);
    seq += static_cast<char>(size >> 8);
    seq += static_cast<char>(type);
    seq += body;
    seq += static_cast<char>(size & 0xFF);
    seq += static_cast<char>(size >> 8);
    seq += '\x1D';
    return seq;
}

}


TEST_CASE("cWordstarFile::LoadFile - text bytes with and without the high bit")
{
    cDocument doc;
    doc.Clear();
    cWordstarFile file;
    file.mDocument = &doc;

    SUBCASE("plain ASCII text loads as is")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("Hello\x0D\x0A\x1A", 8));
        CHECK(file.LoadFile(path.string()));
        CHECK(doc.GetParagraphText(0).rfind("Hello", 0) == 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a high-bit ASCII byte loads with the high bit stripped")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("A\xC2" "B\x0D\x0A\x1A", 6));    // 0xC2 = 'B' | 0x80
        CHECK(file.LoadFile(path.string()));
        CHECK(doc.GetParagraphText(0).rfind("ABB", 0) == 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a 0x80 byte loads as U+0080, not as an invalid code point")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("A\x80" "B\x0D\x0A\x1A", 6));
        CHECK(file.LoadFile(path.string()));
        CHECK(doc.GetParagraphText(0).rfind("A\xC2\x80" "B", 0) == 0);
        std::filesystem::remove(path);
    }
}


TEST_CASE("cWordstarFile::LoadFile - colour sequences: palette index range")
{
    cDocument document;
    document.Clear();
    cWordstarFile file;
    file.mDocument = &document;

    // a colour sequence between "Hi" and "!" is a marker at position 2
    auto loadColor = [&](unsigned char colornumber, sSeqRGBColor& color) -> bool
    {
        std::string body;
        body += static_cast<char>(colornumber);
        body += '\0';
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, "Hi" + SequenceBytes(SEQ_COLOR, body) + "!\x0D\x0A");
        bool loaded = file.LoadFile(path.string());
        std::filesystem::remove(path);
        if (loaded == false || document.GetControlChar(2) != STYLE_INTERNAL_COLOR)
        {
            return false;
        }
        return document.GetColor(2, color);
    };

    SUBCASE("index 4 loads as red")
    {
        sSeqRGBColor color;
        REQUIRE(loadColor(4, color));
        CHECK(color.red == 170);
        CHECK(color.green == 0);
        CHECK(color.blue == 0);
    }

    SUBCASE("index 15 loads as white")
    {
        sSeqRGBColor color;
        REQUIRE(loadColor(15, color));
        CHECK(color.red == 255);
        CHECK(color.green == 255);
        CHECK(color.blue == 255);
    }

    SUBCASE("an index outside the palette loads as Automatic")
    {
        for (unsigned char index : {16, 200, 255})
        {
            CAPTURE(static_cast<int>(index));
            sSeqRGBColor color;
            REQUIRE(loadColor(index, color));
            CHECK(color.IsDefault());
        }
    }
}


TEST_CASE("cWordstarFile::SaveFile - returns false when the file cannot be written")
{
    ensureQApplication();
    cEditorCtrl editor;
    editor.GetDocument()->Insert("Hello\r");
    cWordstarFile file(&editor);
    POSITION_T size = editor.GetDocument()->GetTextSize();

    SUBCASE("a writable path saves")
    {
        std::filesystem::path path = TempPath();
        CHECK(file.SaveFile(path.string(), size));
        CHECK(std::filesystem::file_size(path) > 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a path that cannot be opened fails")
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "wordtsar_no_such_dir" / "x.ws";
        CHECK_FALSE(file.SaveFile(path.string(), size));
    }

    SUBCASE("a full disk fails")
    {
        if (std::filesystem::exists("/dev/full"))
        {
            CHECK_FALSE(file.SaveFile("/dev/full", size));
        }
    }
}


TEST_CASE("cWordstarFile::LoadFile - sequences: bounded reads and the end of the file")
{
    cDocument document;
    document.Clear();
    cDocument* doc = &document;
    cWordstarFile file;
    file.mDocument = doc;

    SUBCASE("an unknown sequence is skipped and text after it loads, without a ^Z")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, "Hello" + SequenceBytes(SEQ_RESERVED1, "abcd") + "World\x0D\x0A");
        CHECK(file.LoadFile(path.string()));
        CHECK(doc->GetParagraphText(0).rfind("HelloWorld", 0) == 0);
        std::filesystem::remove(path);
    }

    SUBCASE("a footnote sequence longer than the file reads only what is there")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("Hi\x1D\xFF\xFF\x03" "abc", 9));    // size 0xFFFF, 3 body bytes
        CHECK(file.LoadFile(path.string()));
        CHECK(doc->GetParagraphText(0).rfind("Hi", 0) == 0);
        CHECK(doc->GetTextSize() < 10);
        std::filesystem::remove(path);
    }

    SUBCASE("an unknown sequence longer than the file stops the load cleanly")
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, std::string("Hi\x1D\xFF\xFF\x07" "abc", 9));
        CHECK(file.LoadFile(path.string()));
        CHECK(doc->GetParagraphText(0).rfind("Hi", 0) == 0);
        CHECK(doc->GetTextSize() < 10);
        std::filesystem::remove(path);
    }
}


TEST_CASE("cWordstarFile::LoadFile - footnote sequences")
{
    cDocument document;
    document.Clear();
    cWordstarFile file;
    file.mDocument = &document;

    // a footnote between "Hi" and "!" is a marker at position 2
    auto loadNote = [&](const std::string &body) -> bool
    {
        std::filesystem::path path = TempPath();
        WriteFileBytes(path, "Hi" + SequenceBytes(SEQ_FOOTNOTE, body) + "!\x0D\x0A");
        bool loaded = file.LoadFile(path.string());
        std::filesystem::remove(path);
        return loaded;
    };
    auto hasFootnote = [&](void) -> bool
    {
        return document.GetCharNoAdvance(2)[0] == static_cast<char>(MARKER_CHAR) &&
               document.GetControlChar(2) == STYLE_FOOTNOTE;
    };

    SUBCASE("a simple note is inserted")
    {
        CHECK(loadNote(std::string("\x00\x00\x00\x00\x00", 5) + "note text"));
        CHECK(hasFootnote());
    }

    SUBCASE("a note with an embedded footnote sub-sequence is inserted")
    {
        std::string body("\x00\x00\x01\x00", 4);               // tag = 1
        body += std::string(5, '\x00');                          // bytes 4-8
        body += std::string("\x00\x00\x01\x03", 4);            // sub-sequence data, bytes 9-12
        body += "xx\x1D";                                       // rest of the sub-sequence
        body += "note text";
        CHECK(loadNote(body));
        CHECK(hasFootnote());
    }

    SUBCASE("a body shorter than the note header is dropped")
    {
        CHECK(loadNote(std::string("\x00\x00", 2)));
        CHECK_FALSE(hasFootnote());
        CHECK(document.GetParagraphText(0).rfind("Hi!", 0) == 0);
    }

    SUBCASE("an embedded footnote without its sub-sequence data is dropped")
    {
        CHECK(loadNote(std::string("\x00\x00\x01\x00", 4)));
        CHECK_FALSE(hasFootnote());
        CHECK(document.GetParagraphText(0).rfind("Hi!", 0) == 0);
    }

    SUBCASE("an embedded footnote whose sub-sequence never ends is dropped")
    {
        std::string body("\x00\x00\x01\x00", 4);
        body += std::string(9, 'a');                              // bytes 4-12, no 0x1D after them
        body += "no end";
        CHECK(loadNote(body));
        CHECK_FALSE(hasFootnote());
        CHECK(document.GetParagraphText(0).rfind("Hi!", 0) == 0);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Loads every .WS file under the directory named by WORDTSAR_ARCHIVE_DIR
/// (searched recursively, extension matched without regard to case) through
/// cEditorCtrl::LoadFile(). A file fails when the load returns false, throws,
/// or leaves an empty document. All failures are reported together. Without
/// WORDTSAR_ARCHIVE_DIR the test does nothing.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("cEditorCtrl::LoadFile - every .WS file in WORDTSAR_ARCHIVE_DIR loads")
{
    const char* archiveEnv = std::getenv("WORDTSAR_ARCHIVE_DIR");
    if (archiveEnv == nullptr || archiveEnv[0] == '\0')
    {
        MESSAGE("WORDTSAR_ARCHIVE_DIR is not set, archive load test skipped");
        return;
    }

    const std::filesystem::path archiveDir(archiveEnv);
    REQUIRE_MESSAGE(std::filesystem::is_directory(archiveDir),
                    "WORDTSAR_ARCHIVE_DIR is not a directory: " << archiveDir.string());

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(archiveDir))
    {
        if (entry.is_regular_file() == false)
        {
            continue;
        }
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".ws")
        {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    REQUIRE_MESSAGE(files.empty() == false, "no .WS files in " << archiveDir.string());

    ensureQApplication();

    std::vector<std::string> failures;
    for (const std::filesystem::path& path : files)
    {
        std::string error;
        try
        {
            cEditorCtrl editor;
            if (editor.LoadFile(path.string()) == false)
            {
                error = "LoadFile returned false";
            }
            else if (editor.GetDocument()->GetTextSize() == 0)
            {
                error = "document is empty after load";
            }
        }
        catch (const std::exception& e)
        {
            error = std::string("exception: ") + e.what();
        }
        catch (...)
        {
            error = "unknown exception";
        }

        if (error.empty() == false)
        {
            failures.push_back(path.string() + ": " + error);
        }
    }

    std::string report;
    for (const std::string& failure : failures)
    {
        report += "\n  " + failure;
    }
    MESSAGE(files.size() << " .WS files loaded, " << failures.size() << " failed" << report);
    CHECK_MESSAGE(failures.empty(), failures.size() << " of " << files.size() << " .WS files failed to load" << report);
}
