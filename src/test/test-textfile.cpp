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
