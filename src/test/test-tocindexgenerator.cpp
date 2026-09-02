//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
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

#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "src/core/document/document.h"
#include "src/core/generate/tocindexgenerator.h"
#include "src/core/include/config.h"
#include "src/core/layout/layoutbase.h"
#include "src/gui/editor/editorctrl.h"
#include <QApplication>

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// cEditorCtrl is a QWidget subclass, so constructing one requires a live
/// QApplication (see test-docxwriter.cpp for the same pattern).
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

std::string ReadWholeFile(const std::string &path)
{
    cEditorCtrl reader ;
    reader.LoadFile(path) ;

    std::string all ;
    cDocument *doc = reader.GetDocument() ;
    for (ssize_t i = 0 ; i < doc->GetNumberofParagraphs() ; i++)
    {
        all += doc->GetParagraphText(i) ;
    }
    return all ;
}
}


TEST_CASE("TOC/Index generator - table of contents resolves real page numbers, replaces #, splits by .tcN")
{
    ensureQApplication() ;

    cEditorCtrl editor ;
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;

    doc->Insert(".tc Chapter One . . . #\r") ;
    doc->Insert(".pa\r") ;
    doc->Insert("Second page content\r") ;
    doc->Insert(".tc Chapter Two . . . #\r") ;
    doc->Insert(".tc1 Figure 1 . . . #\r") ;

    editor.GetLayout()->LayoutDocument(doc) ;

    std::string sourcePath = "/tmp/wordtsar_tocgen_source.ws" ;
    std::vector<std::string> outputFiles ;
    bool ok = cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles) ;

    REQUIRE(ok) ;
    REQUIRE(outputFiles.size() == 2) ;

    std::string tocPath ;
    std::string t01Path ;
    for (auto &path : outputFiles)
    {
        if (path.find(".TOC") != std::string::npos) tocPath = path ;
        if (path.find(".T01") != std::string::npos) t01Path = path ;
    }
    REQUIRE(tocPath.empty() == false) ;
    REQUIRE(t01Path.empty() == false) ;

    std::string tocContent = ReadWholeFile(tocPath) ;
    // Chapter One is on page 1, Chapter Two is on page 2 (after the .pa).
    CHECK(tocContent.find("Chapter One . . . 1") != std::string::npos) ;
    CHECK(tocContent.find("Chapter Two . . . 2") != std::string::npos) ;
    // No unresolved placeholder should survive.
    CHECK(tocContent.find('#') == std::string::npos) ;

    std::string t01Content = ReadWholeFile(t01Path) ;
    CHECK(t01Content.find("Figure 1 . . . 2") != std::string::npos) ;
    // The numbered table must not also appear in the main .TOC file.
    CHECK(tocContent.find("Figure 1") == std::string::npos) ;
}


TEST_CASE("TOC/Index generator - a bare .tc/.tcN with no entry text is skipped, not written as a blank line")
{
    ensureQApplication() ;

    cEditorCtrl editor ;
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;

    doc->Insert(".tc\r") ;                  // bare -- no text at all
    doc->Insert(".tc Real Entry\r") ;
    doc->Insert(".tc3\r") ;                 // bare, numbered table -- no text at all
    doc->Insert(".tc3 Another Entry\r") ;

    editor.GetLayout()->LayoutDocument(doc) ;

    std::string sourcePath = "/tmp/wordtsar_tocgen_blank_source.ws" ;
    std::vector<std::string> outputFiles ;
    bool ok = cTOCIndexGenerator::GenerateTOC(&editor, sourcePath, outputFiles) ;

    REQUIRE(ok) ;
    REQUIRE(outputFiles.size() == 2) ;

    for (auto &path : outputFiles)
    {
        std::string content = ReadWholeFile(path) ;
        bool hasRealEntry = (content.find("Real Entry") != std::string::npos) ||
                             (content.find("Another Entry") != std::string::npos) ;
        CHECK(hasRealEntry) ;

        // No paragraph in either output file should be just a bare page
        // number with nothing in front of it -- that's the blank-line bug.
        std::istringstream stream(content) ;
        std::string line ;
        while (std::getline(stream, line))
        {
            bool isBareNumber = !line.empty() ;
            for (char c : line)
            {
                if (!std::isdigit(static_cast<unsigned char>(c)))
                {
                    isBareNumber = false ;
                    break ;
                }
            }
            CHECK_FALSE(isBareNumber) ;
        }
    }
}


TEST_CASE("TOC/Index generator - index sorts, merges duplicates, bolds, and skips page numbers for cross-references")
{
    ensureQApplication() ;

    cEditorCtrl editor ;
    cDocument *doc = editor.GetDocument() ;
    doc->Clear() ;

    doc->Insert(".ix Zebra\r") ;
    doc->Insert(".ix Apple\r") ;
    doc->Insert(".pa\r") ;
    doc->Insert("More content\r") ;
    doc->Insert(".ix Apple\r") ;                    // second occurrence, later page -- must merge
    doc->Insert(".ix +Bold Entry\r") ;               // bold page number
    doc->Insert(".ix -See Also, cross ref\r") ;      // cross-reference: no page number, comma kept literal

    editor.GetLayout()->LayoutDocument(doc) ;

    std::string sourcePath = "/tmp/wordtsar_indexgen_source.ws" ;
    std::string outputFile ;
    bool ok = cTOCIndexGenerator::GenerateIndex(&editor, sourcePath, outputFile) ;

    REQUIRE(ok) ;
    REQUIRE(outputFile.find(".IDX") != std::string::npos) ;

    std::string content = ReadWholeFile(outputFile) ;

    // Alphabetical: Apple before Bold Entry before See Also before Zebra.
    size_t applePos = content.find("Apple") ;
    size_t boldPos = content.find("Bold Entry") ;
    size_t seePos = content.find("See Also") ;
    size_t zebraPos = content.find("Zebra") ;
    REQUIRE(applePos != std::string::npos) ;
    REQUIRE(boldPos != std::string::npos) ;
    REQUIRE(seePos != std::string::npos) ;
    REQUIRE(zebraPos != std::string::npos) ;
    CHECK(applePos < boldPos) ;
    CHECK(boldPos < seePos) ;
    CHECK(seePos < zebraPos) ;

    // The two "Apple" entries (pages 1 and 2) merged onto one line.
    CHECK(content.find("Apple, 1, 2") != std::string::npos) ;

    // The cross-reference kept its comma literally and carries no page number.
    CHECK(content.find("See Also, cross ref") != std::string::npos) ;

    // Bold Entry's page number is wrapped in a real bold toggle (MARKER_CHAR).
    bool sawMarkerNearBold = false ;
    for (size_t i = boldPos ; i < content.size() && i < boldPos + 40 ; i++)
    {
        if (static_cast<unsigned char>(content[i]) == static_cast<unsigned char>(MARKER_CHAR))
        {
            sawMarkerNearBold = true ;
        }
    }
    CHECK(sawMarkerNearBold) ;
}
