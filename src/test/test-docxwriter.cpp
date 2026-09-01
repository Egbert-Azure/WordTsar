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
/// QApplication (see test-baseeditor.cpp for the same pattern).
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
/// Helper: save sourceEditor's document as a .docx at path, then load that
/// file back into a fresh editor's document and return its paragraph texts
/// for inspection. This exercises cDOCXFile::SaveFile() and LoadFile()
/// against each other -- the most meaningful check available without a
/// real Word/Pages install to cross-validate against.
///
/////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////
///
/// Helper: pull word/document.xml back out of a saved .docx as a raw string,
/// for checks that need to see the actual markup (e.g. w:vanish) rather than
/// what LoadFile() reconstructs into paragraph text.
///
/////////////////////////////////////////////////////////////////////////////
static std::string ExtractDocumentXml(const std::string &path)
{
    zip_t *zip = zip_open(path.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'r') ;
    REQUIRE(zip != nullptr) ;

    void *docbuf = nullptr ;
    size_t bufsize = 0 ;
    zip_entry_open(zip, "word/document.xml") ;
    zip_entry_read(zip, &docbuf, &bufsize) ;
    zip_entry_close(zip) ;
    zip_close(zip) ;

    std::string xml(static_cast<char *>(docbuf), bufsize) ;
    free(docbuf) ;
    return xml ;
}


static std::vector<std::string> RoundTripDOCX(cEditorCtrl &sourceEditor, const std::string &path)
{
    cDOCXFile writer(&sourceEditor) ;
    bool saved = writer.SaveFile(path, sourceEditor.GetDocument()->GetTextSize()) ;
    REQUIRE(saved) ;

    cEditorCtrl targetEditor ;
    targetEditor.GetDocument()->Clear() ;
    cDOCXFile reader(&targetEditor) ;
    reader.LoadFile(path) ;

    std::vector<std::string> paragraphs ;
    cDocument *doc = targetEditor.GetDocument() ;
    for (ssize_t i = 0 ; i < doc->GetNumberofParagraphs() ; i++)
    {
        paragraphs.push_back(doc->GetParagraphText(i)) ;
    }
    return paragraphs ;
}


TEST_CASE("DOCX writer - plain text round-trips")
{
    ensureQApplication() ;
    cEditorCtrl editor ;
    editor.GetDocument()->Clear() ;
    editor.GetDocument()->Insert("Hello, WordTsar.") ;

    std::vector<std::string> paragraphs = RoundTripDOCX(editor, "/tmp/wordtsar_docxtest_plain.docx") ;

    bool found = false ;
    for (auto &p : paragraphs)
    {
        if (p.find("Hello, WordTsar.") != std::string::npos)
        {
            found = true ;
        }
    }
    CHECK(found) ;
}


TEST_CASE("DOCX writer - bold text round-trips with MARKER_CHAR toggles")
{
    ensureQApplication() ;
    cEditorCtrl editor ;
    editor.GetDocument()->Clear() ;
    editor.GetDocument()->BeginBold() ;
    editor.GetDocument()->Insert("Bold text") ;
    editor.GetDocument()->BeginBold() ;
    editor.GetDocument()->Insert(" and plain text") ;

    std::vector<std::string> paragraphs = RoundTripDOCX(editor, "/tmp/wordtsar_docxtest_bold.docx") ;

    REQUIRE(paragraphs.size() >= 1) ;

    bool sawText = false ;
    bool sawMarker = false ;
    for (auto &p : paragraphs)
    {
        if (p.find("Bold text") != std::string::npos)
        {
            sawText = true ;
        }
        for (unsigned char c : p)
        {
            if (c == static_cast<unsigned char>(MARKER_CHAR))
            {
                sawMarker = true ;
            }
        }
    }
    CHECK(sawText) ;
    CHECK(sawMarker) ;
}


TEST_CASE("DOCX writer - centered paragraph round-trips as .oj/.oc dot command")
{
    ensureQApplication() ;
    cEditorCtrl editor ;
    editor.GetDocument()->Clear() ;
    editor.GetDocument()->BeginCenter() ;
    editor.GetDocument()->Insert("Centered heading") ;

    std::vector<std::string> paragraphs = RoundTripDOCX(editor, "/tmp/wordtsar_docxtest_center.docx") ;

    bool sawCenterDotCommand = false ;
    for (auto &p : paragraphs)
    {
        if (!p.empty() && p[0] == '.' &&
            (p.rfind(".oc", 0) == 0 || p.rfind(".OC", 0) == 0 ||
             p.rfind(".oj", 0) == 0 || p.rfind(".OJ", 0) == 0))
        {
            sawCenterDotCommand = true ;
        }
    }
    CHECK(sawCenterDotCommand) ;
}


TEST_CASE("DOCX writer - .. comment line is preserved as hidden text, not dropped")
{
    ensureQApplication() ;
    cEditorCtrl editor ;
    editor.GetDocument()->Clear() ;
    editor.GetDocument()->Insert(".. This is a comment\r") ;
    editor.GetDocument()->Insert("Visible paragraph") ;

    std::string path = "/tmp/wordtsar_docxtest_comment.docx" ;
    cDOCXFile writer(&editor) ;
    REQUIRE(writer.SaveFile(path, editor.GetDocument()->GetTextSize())) ;

    // Check the raw document.xml directly: the comment text must appear
    // inside a hidden (w:vanish) run, since the reader doesn't parse hidden
    // runs back into ".." lines -- this checks the writer side only.
    std::string xml = ExtractDocumentXml(path) ;
    CHECK(xml.find("This is a comment") != std::string::npos) ;
    CHECK(xml.find("w:vanish") != std::string::npos) ;

    std::vector<std::string> paragraphs = RoundTripDOCX(editor, path) ;
    bool sawVisible = false ;
    for (auto &p : paragraphs)
    {
        if (p.find("Visible paragraph") != std::string::npos)
        {
            sawVisible = true ;
        }
    }
    CHECK(sawVisible) ;
}
