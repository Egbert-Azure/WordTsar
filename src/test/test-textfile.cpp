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

#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <unistd.h>

#include "src/core/include/config.h"
#include "src/core/document/document.h"
#include "src/files/textfile.h"


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: build a byte vector from a brace-init list of ints so UTF-8 test
/// sequences read like the hex in the ticket.
///
/////////////////////////////////////////////////////////////////////////////
static std::vector<unsigned char> Bytes(std::initializer_list<int> values)
{
    std::vector<unsigned char> result;
    for(int value : values)
    {
        result.push_back(static_cast<unsigned char>(value));
    }
    return result;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: validate a byte vector through the public static UTF-8 checker.
///
/////////////////////////////////////////////////////////////////////////////
static bool ValidUTF8(const std::vector<unsigned char> &bytes)
{
    return cTextFile::IsValidUTF8(bytes.data(), bytes.size());
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: allocate a unique temp path in the system temp directory.
///
/////////////////////////////////////////////////////////////////////////////
static std::filesystem::path TempPath(void)
{
    static int counter = 0;
    counter++;
    std::filesystem::path path = std::filesystem::temp_directory_path();
    path /= "wordtsar_textfile_test_" + std::to_string(counter) + ".txt";
    return path;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: write raw bytes to a file.
///
/////////////////////////////////////////////////////////////////////////////
static void WriteFileBytes(const std::filesystem::path &path, const std::string &bytes)
{
    std::ofstream file(path, std::ios::binary);
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: read a whole file back as raw bytes.
///
/////////////////////////////////////////////////////////////////////////////
static std::string ReadFileBytes(const std::filesystem::path &path)
{
    std::ifstream file(path, std::ios::binary);
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;
}

//===========================================================================
// B1 - cTextFile::IsValidUTF8 (RFC 3629)
//===========================================================================
TEST_CASE("cTextFile::IsValidUTF8 - accepts well-formed sequences")
{
    CHECK(ValidUTF8(Bytes({})) == true);                          // empty
    CHECK(ValidUTF8(Bytes({'H', 'e', 'l', 'l', 'o'})) == true);  // ASCII
    CHECK(ValidUTF8(Bytes({0xC3, 0xA9})) == true);               // U+00E9 e-acute
    CHECK(ValidUTF8(Bytes({0xE4, 0xB8, 0x96})) == true);         // U+4E16
    CHECK(ValidUTF8(Bytes({0xF0, 0x9F, 0x8E, 0x89})) == true);   // U+1F389
}

TEST_CASE("cTextFile::IsValidUTF8 - accepts boundary codepoints")
{
    CHECK(ValidUTF8(Bytes({0xC2, 0x80})) == true);               // U+0080 min 2-byte
    CHECK(ValidUTF8(Bytes({0xE0, 0xA0, 0x80})) == true);         // U+0800 min 3-byte
    CHECK(ValidUTF8(Bytes({0xF0, 0x90, 0x80, 0x80})) == true);   // U+10000 min 4-byte
    CHECK(ValidUTF8(Bytes({0xEF, 0xBF, 0xBF})) == true);         // U+FFFF
    CHECK(ValidUTF8(Bytes({0xF4, 0x8F, 0xBF, 0xBF})) == true);   // U+10FFFF max
}

TEST_CASE("cTextFile::IsValidUTF8 - rejects overlong encodings")
{
    CHECK(ValidUTF8(Bytes({0xC0, 0x80})) == false);              // overlong NUL
    CHECK(ValidUTF8(Bytes({0xC1, 0xBF})) == false);              // overlong U+7F
    CHECK(ValidUTF8(Bytes({0xE0, 0x80, 0x80})) == false);        // overlong 3-byte
    CHECK(ValidUTF8(Bytes({0xE0, 0x9F, 0xBF})) == false);        // U+07FF as 3-byte
    CHECK(ValidUTF8(Bytes({0xF0, 0x80, 0x80, 0x80})) == false);  // overlong 4-byte
    CHECK(ValidUTF8(Bytes({0xF0, 0x8F, 0xBF, 0xBF})) == false);  // U+FFFF as 4-byte
}

TEST_CASE("cTextFile::IsValidUTF8 - rejects surrogates and out-of-range")
{
    CHECK(ValidUTF8(Bytes({0xED, 0xA0, 0x80})) == false);        // U+D800
    CHECK(ValidUTF8(Bytes({0xED, 0xBF, 0xBF})) == false);        // U+DFFF
    CHECK(ValidUTF8(Bytes({0xF4, 0x90, 0x80, 0x80})) == false);  // U+110000
    CHECK(ValidUTF8(Bytes({0xF5, 0x80, 0x80, 0x80})) == false);  // > U+10FFFF lead
    CHECK(ValidUTF8(Bytes({0xF7, 0xBF, 0xBF, 0xBF})) == false);  // > U+10FFFF lead
}

TEST_CASE("cTextFile::IsValidUTF8 - rejects structural errors")
{
    CHECK(ValidUTF8(Bytes({0x80})) == false);                    // lone continuation
    CHECK(ValidUTF8(Bytes({0xFE})) == false);                    // invalid lead
    CHECK(ValidUTF8(Bytes({0xFF})) == false);                    // invalid lead
    CHECK(ValidUTF8(Bytes({0xE4, 0xB8})) == false);              // truncated 3-byte
    CHECK(ValidUTF8(Bytes({0xC3, 0x20})) == false);              // bad continuation
}


//===========================================================================
// B2 - cTextFile::IsAcceptableFileSize and LoadFile size guard
//===========================================================================
TEST_CASE("cTextFile::IsAcceptableFileSize - rejects negative and oversized")
{
    CHECK(cTextFile::IsAcceptableFileSize(-1) == false);              // tellg() failure
    CHECK(cTextFile::IsAcceptableFileSize(0) == true);               // empty is valid
    CHECK(cTextFile::IsAcceptableFileSize(1024) == true);            // normal
    CHECK(cTextFile::IsAcceptableFileSize(MAX_TEXT_FILE_SIZE) == true);
    CHECK(cTextFile::IsAcceptableFileSize(MAX_TEXT_FILE_SIZE + 1) == false);
}

TEST_CASE("cTextFile::LoadFile - loads a small UTF-8 file")
{
    std::filesystem::path path = TempPath();
    WriteFileBytes(path, "Hello world");

    cDocument doc;
    doc.Clear();
    cTextFile file;
    file.mDocument = &doc;

    CHECK(file.LoadFile(path.string()) == true);
    CHECK(doc.GetTextSize() > 0);

    std::filesystem::remove(path);
}

TEST_CASE("cTextFile::LoadFile - empty file is valid")
{
    std::filesystem::path path = TempPath();
    WriteFileBytes(path, "");

    cDocument doc;
    doc.Clear();
    cTextFile file;
    file.mDocument = &doc;

    CHECK(file.LoadFile(path.string()) == true);

    std::filesystem::remove(path);
}


//===========================================================================
// B3 - plain-text output: no internal markers
//===========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// Helper: create a unique empty temp directory.
///
/////////////////////////////////////////////////////////////////////////////
static std::filesystem::path MakeTempDir(void)
{
    std::string pattern =
        (std::filesystem::temp_directory_path() / "wordtsar-textfile-XXXXXX").string();
    REQUIRE(mkdtemp(pattern.data()) != nullptr);
    return std::filesystem::path(pattern);
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: load bytes as a text file, save it again, return the saved bytes.
///
/////////////////////////////////////////////////////////////////////////////
static std::string TextRoundTrip(const std::string &bytes)
{
    std::filesystem::path dir = MakeTempDir();
    std::filesystem::path in = dir / "in.txt";
    std::filesystem::path out = dir / "out.txt";
    WriteFileBytes(in, bytes);

    cDocument doc;
    doc.Clear();
    cTextFile file;
    file.mDocument = &doc;
    REQUIRE(file.LoadFile(in.string()));
    REQUIRE(file.SaveFile(out.string(), doc.GetTextSize()));

    std::string result = ReadFileBytes(out);
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return result;
}


TEST_CASE("cTextFile::SaveFile - a tab marker is a tab, other markers are nothing")
{
    std::filesystem::path dir = MakeTempDir();
    std::filesystem::path path = dir / "markers.txt";

    cDocument doc;
    doc.Clear();

    sWSTab tab;
    tab.abstabsize = 0;
    tab.size = 0;
    tab.tabsize = 0;
    tab.type = TAB_TAB;

    sInternalFonts font;
    font.fontname = "Courier";
    font.size = 12.0;
    font.haveWSFont = false;
    font.name = "Courier";

    sSeqRGBColor color;
    color.red = 170; color.green = 0; color.blue = 170; color.alpha = 255;

    sNote footnote;
    footnote.symbol = NOTE_NUMBER;
    footnote.text = "a footnote";

    sNote annotation;
    annotation.kind = NOTE_KIND_ANNOTATION;
    annotation.symbol = NOTE_NUMBER;
    annotation.text = "an annotation";

    sNote comment;
    comment.kind = NOTE_KIND_COMMENT;
    comment.symbol = NOTE_NUMBER;
    comment.text = "a comment";

    sNote endnote;
    endnote.kind = NOTE_KIND_ENDNOTE;
    endnote.symbol = NOTE_NUMBER;
    endnote.text = "an endnote";

    doc.Insert("A");
    doc.InsertTab(tab);
    doc.Insert("B");
    doc.Insert(static_cast<CHAR_T>(STYLE_BOLD));
    doc.Insert("C");
    doc.Insert(static_cast<CHAR_T>(STYLE_BOLD));
    doc.Insert(static_cast<CHAR_T>(STYLE_UNDERLINE));
    doc.InsertFont(font);
    doc.InsertColor(color);
    doc.InsertVariable(VAR_DATE);
    doc.InsertFootnote(footnote);
    doc.InsertFootnote(annotation);
    doc.InsertFootnote(comment);
    doc.InsertEndnote(endnote);
    doc.Insert("D");

    cTextFile file;
    file.mDocument = &doc;
    REQUIRE(file.SaveFile(path.string(), doc.GetTextSize()));
    CHECK(ReadFileBytes(path) == "A\tBCD");

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

TEST_CASE("cTextFile - a tab in a text file is loaded as a tab marker and saved as a tab")
{
    std::filesystem::path dir = MakeTempDir();
    std::filesystem::path path = dir / "tab.txt";
    WriteFileBytes(path, "a\tb");

    cDocument doc;
    doc.Clear();
    cTextFile file;
    file.mDocument = &doc;
    REQUIRE(file.LoadFile(path.string()));
    CHECK(doc.GetControlChar(1) == STYLE_TAB);

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);

    CHECK(TextRoundTrip("a\tb") == "a\tb");
    CHECK(TextRoundTrip("\ta\t\tb\t") == "\ta\t\tb\t");
}

TEST_CASE("cTextFile - a DEL character that is not a marker is saved")
{
    CHECK(TextRoundTrip("a\x7f" "b") == "a\x7f" "b");
}


//===========================================================================
// B4 - line endings of a text file
//===========================================================================

/////////////////////////////////////////////////////////////////////////////
///
/// Helper: load bytes as a text file and return the line ending it reports.
///
/////////////////////////////////////////////////////////////////////////////
static std::string LoadedLineEnding(const std::string &bytes)
{
    std::filesystem::path dir = MakeTempDir();
    std::filesystem::path path = dir / "in.txt";
    WriteFileBytes(path, bytes);

    cDocument doc;
    doc.Clear();
    cTextFile file;
    file.mDocument = &doc;
    REQUIRE(file.LoadFile(path.string()));

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
    return file.GetLineEnding();
}

TEST_CASE("cTextFile::LoadFile - reports the line ending of the file")
{
    CHECK(LoadedLineEnding("a\nb\n") == "\n");
    CHECK(LoadedLineEnding("a\r\nb\r\n") == "\r\n");
    CHECK(LoadedLineEnding("a\rb\r") == "\r");
    CHECK(LoadedLineEnding("a\nb\r\nc\r") == "\n");         // mixed: the first one
    CHECK(LoadedLineEnding("a\r\nb\nc\n") == "\r\n");
    CHECK(LoadedLineEnding("no line end") == "\n");         // none: LF
}

TEST_CASE("cTextFile::SaveFile - writes the document line ending")
{
    std::filesystem::path dir = MakeTempDir();
    std::filesystem::path path = dir / "out.txt";

    cDocument doc;
    doc.Clear();
    doc.Insert("a\rb\r");
    cTextFile file;
    file.mDocument = &doc;

    SUBCASE("a new document uses LF")
    {
        REQUIRE(file.SaveFile(path.string(), doc.GetTextSize()));
        CHECK(ReadFileBytes(path) == "a\nb\n");
    }

    SUBCASE("a document set to CRLF")
    {
        doc.SetLineEnding("\r\n");
        REQUIRE(file.SaveFile(path.string(), doc.GetTextSize()));
        CHECK(ReadFileBytes(path) == "a\r\nb\r\n");
    }

    SUBCASE("Clear resets to LF")
    {
        doc.SetLineEnding("\r");
        doc.Clear();
        doc.Insert("a\rb\r");
        REQUIRE(file.SaveFile(path.string(), doc.GetTextSize()));
        CHECK(ReadFileBytes(path) == "a\nb\n");
    }

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


//===========================================================================
