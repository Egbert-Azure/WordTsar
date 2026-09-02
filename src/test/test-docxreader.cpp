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

#include <string>
#include <vector>

#include "src/core/include/config.h"
#include "src/core/document/document.h"
#include "src/gui/editor/editorctrl.h"
#include "src/files/docxfile.h"
#include <QApplication>
#include "zip.h"

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
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: hand-build a minimal loadable .docx with a caller-supplied
/// word/document.xml body and optional word/numbering.xml, bypassing
/// cDOCXFile's own writer entirely -- it has no concept of tables or
/// numbering to write, so the only way to exercise the *reader* side of
/// either is to construct the raw XML directly, the same way a real Word
/// document would contain it.
///
/////////////////////////////////////////////////////////////////////////////
static void BuildTestDocx(const std::string &path, const std::string &bodyXml, const std::string &numberingXml = "")
{
    zip_t *zip = zip_open(path.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w') ;
    REQUIRE(zip != nullptr) ;

    auto writeEntry = [zip](const char *name, const std::string &content) -> bool
    {
        if (zip_entry_open(zip, name) != 0)
        {
            return false ;
        }
        bool ok = zip_entry_write(zip, content.data(), content.size()) == 0 ;
        zip_entry_close(zip) ;
        return ok ;
    } ;

    std::string stylesXml =
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"></w:styles>" ;

    std::string documentXml =
        "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:body>" + bodyXml + "</w:body></w:document>" ;

    REQUIRE(writeEntry("word/styles.xml", stylesXml)) ;
    REQUIRE(writeEntry("word/document.xml", documentXml)) ;

    if (!numberingXml.empty())
    {
        std::string wrapped =
            "<w:numbering xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
            + numberingXml + "</w:numbering>" ;
        REQUIRE(writeEntry("word/numbering.xml", wrapped)) ;
    }

    zip_close(zip) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// Helper: load path into a fresh editor's document and return every
/// paragraph's text for inspection.
///
/////////////////////////////////////////////////////////////////////////////
static std::vector<std::string> LoadDOCXParagraphs(const std::string &path)
{
    ensureQApplication() ;
    cEditorCtrl editor ;
    editor.GetDocument()->Clear() ;

    cDOCXFile reader(&editor) ;
    reader.LoadFile(path) ;

    std::vector<std::string> paragraphs ;
    cDocument *doc = editor.GetDocument() ;
    for (ssize_t i = 0 ; i < doc->GetNumberofParagraphs() ; i++)
    {
        paragraphs.push_back(doc->GetParagraphText(i)) ;
    }
    return paragraphs ;
}


TEST_CASE("DOCX reader - table imports as tab-separated rows, not a placeholder")
{
    std::string body =
        "<w:tbl>"
        "<w:tr><w:tc><w:p><w:r><w:t>Name</w:t></w:r></w:p></w:tc>"
               "<w:tc><w:p><w:r><w:t>Age</w:t></w:r></w:p></w:tc></w:tr>"
        "<w:tr><w:tc><w:p><w:r><w:t>Alice</w:t></w:r></w:p></w:tc>"
               "<w:tc><w:p><w:r><w:t>30</w:t></w:r></w:p></w:tc></w:tr>"
        "</w:tbl>" ;

    std::string path = "/tmp/wordtsar_docxtest_table.docx" ;
    BuildTestDocx(path, body) ;
    std::vector<std::string> paragraphs = LoadDOCXParagraphs(path) ;

    std::string all ;
    for (auto &p : paragraphs)
    {
        all += p ;
    }

    // No more placeholder marker.
    CHECK(all.find("<<< TABLE >>>") == std::string::npos) ;

    // Real cell text made it in.
    CHECK(all.find("Name") != std::string::npos) ;
    CHECK(all.find("Age") != std::string::npos) ;
    CHECK(all.find("Alice") != std::string::npos) ;
    CHECK(all.find("30") != std::string::npos) ;

    // Cells within a row are tab-separated (a real inserted tab stop, stored
    // as MARKER_CHAR with tab metadata alongside it -- not a literal '\t').
    bool sawTabBetweenCells = false ;
    for (auto &p : paragraphs)
    {
        size_t namePos = p.find("Name") ;
        size_t agePos = p.find("Age") ;
        if (namePos != std::string::npos && agePos != std::string::npos && agePos > namePos)
        {
            for (size_t i = namePos ; i < agePos ; i++)
            {
                if (static_cast<unsigned char>(p[i]) == static_cast<unsigned char>(MARKER_CHAR))
                {
                    sawTabBetweenCells = true ;
                }
            }
        }
    }
    CHECK(sawTabBetweenCells) ;

    // A real .tb dot command was emitted to align the columns.
    bool sawTabStopCommand = false ;
    for (auto &p : paragraphs)
    {
        if (p.rfind(".tb", 0) == 0 || p.rfind(".TB", 0) == 0)
        {
            sawTabStopCommand = true ;
        }
    }
    CHECK(sawTabStopCommand) ;
}


TEST_CASE("DOCX reader - numbered and bulleted lists get real marker text")
{
    std::string body =
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr>"
        "<w:r><w:t>First</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"1\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Second</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"2\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Bullet item</w:t></w:r></w:p>" ;

    std::string numbering =
        "<w:abstractNum w:abstractNumId=\"10\">"
        "<w:lvl w:ilvl=\"0\"><w:start w:val=\"1\"/><w:numFmt w:val=\"decimal\"/><w:lvlText w:val=\"%1.\"/></w:lvl>"
        "</w:abstractNum>"
        "<w:abstractNum w:abstractNumId=\"20\">"
        "<w:lvl w:ilvl=\"0\"><w:numFmt w:val=\"bullet\"/><w:lvlText w:val=\"\"/></w:lvl>"
        "</w:abstractNum>"
        "<w:num w:numId=\"1\"><w:abstractNumId w:val=\"10\"/></w:num>"
        "<w:num w:numId=\"2\"><w:abstractNumId w:val=\"20\"/></w:num>" ;

    std::string path = "/tmp/wordtsar_docxtest_numbering.docx" ;
    BuildTestDocx(path, body, numbering) ;
    std::vector<std::string> paragraphs = LoadDOCXParagraphs(path) ;

    REQUIRE(paragraphs.size() >= 3) ;

    // Real, correctly-incrementing decimal markers -- not stuck at "1." twice.
    bool sawFirst = false ;
    bool sawSecond = false ;
    bool sawBullet = false ;
    for (auto &p : paragraphs)
    {
        if (p.find("1.") != std::string::npos && p.find("First") != std::string::npos)
        {
            sawFirst = true ;
        }
        if (p.find("2.") != std::string::npos && p.find("Second") != std::string::npos)
        {
            sawSecond = true ;
        }
        if (p.find("-") != std::string::npos && p.find("Bullet item") != std::string::npos)
        {
            sawBullet = true ;
        }
    }
    CHECK(sawFirst) ;
    CHECK(sawSecond) ;
    CHECK(sawBullet) ;
}


TEST_CASE("DOCX reader - multi-level numbering substitutes every ancestor placeholder and restarts honoring w:start")
{
    // Level 0 starts at 5; level 1 (a sub-item) starts at 3 and uses a
    // compound lvlText referencing BOTH its own counter (%2) and its
    // parent's (%1) -- real outline/legal numbering shape.
    std::string body =
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"3\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Top A</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"1\"/><w:numId w:val=\"3\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Sub A1</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"1\"/><w:numId w:val=\"3\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Sub A2</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"0\"/><w:numId w:val=\"3\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Top B</w:t></w:r></w:p>"
        "<w:p><w:pPr><w:numPr><w:ilvl w:val=\"1\"/><w:numId w:val=\"3\"/></w:numPr></w:pPr>"
        "<w:r><w:t>Sub B1</w:t></w:r></w:p>" ;

    std::string numbering =
        "<w:abstractNum w:abstractNumId=\"30\">"
        "<w:lvl w:ilvl=\"0\"><w:start w:val=\"5\"/><w:numFmt w:val=\"decimal\"/><w:lvlText w:val=\"%1.\"/></w:lvl>"
        "<w:lvl w:ilvl=\"1\"><w:start w:val=\"3\"/><w:numFmt w:val=\"lowerLetter\"/><w:lvlText w:val=\"%1.%2)\"/></w:lvl>"
        "</w:abstractNum>"
        "<w:num w:numId=\"3\"><w:abstractNumId w:val=\"30\"/></w:num>" ;

    std::string path = "/tmp/wordtsar_docxtest_multilevel.docx" ;
    BuildTestDocx(path, body, numbering) ;
    std::vector<std::string> paragraphs = LoadDOCXParagraphs(path) ;

    REQUIRE(paragraphs.size() >= 5) ;

    auto hasMarkerAndText = [&](const std::string &marker, const std::string &text) -> bool
    {
        for (auto &p : paragraphs)
        {
            if (p.find(marker) != std::string::npos && p.find(text) != std::string::npos)
            {
                return true ;
            }
        }
        return false ;
    } ;

    // Level 0 starts at its own w:start (5), not 1.
    CHECK(hasMarkerAndText("5.", "Top A")) ;
    // Compound lvlText substitutes BOTH the parent's placeholder (%1 -> 5)
    // and its own (%2 -> c, since level 1 starts at 3 -- the third letter).
    CHECK(hasMarkerAndText("5.c)", "Sub A1")) ;
    CHECK(hasMarkerAndText("5.d)", "Sub A2")) ;
    // Level 0's second use advances to 6...
    CHECK(hasMarkerAndText("6.", "Top B")) ;
    // ...and restarting level 1 honors ITS OWN w:start (3 -> "c") again,
    // rather than resuming from 0 (which would wrongly read "6.a)").
    CHECK(hasMarkerAndText("6.c)", "Sub B1")) ;
}


TEST_CASE("DOCX reader - custom tab stops emit a real .tb dot command")
{
    std::string body =
        "<w:p><w:pPr><w:tabs>"
        "<w:tab w:val=\"center\" w:pos=\"2880\"/>"
        "<w:tab w:val=\"right\" w:pos=\"5760\"/>"
        "</w:tabs></w:pPr>"
        "<w:r><w:t>Tabbed line</w:t></w:r></w:p>" ;

    std::string path = "/tmp/wordtsar_docxtest_tabs.docx" ;
    BuildTestDocx(path, body) ;
    std::vector<std::string> paragraphs = LoadDOCXParagraphs(path) ;

    bool sawTabCommand = false ;
    for (auto &p : paragraphs)
    {
        if ((p.rfind(".tb", 0) == 0 || p.rfind(".TB", 0) == 0) &&
            p.find("^2.00i") != std::string::npos && p.find(">4.00i") != std::string::npos)
        {
            sawTabCommand = true ;
        }
    }
    CHECK(sawTabCommand) ;
}
