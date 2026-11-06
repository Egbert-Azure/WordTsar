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

#include "src/tui/editor/editorctrl.h"
#include "src/test/tocindexgen-testutil.h"


TEST_CASE("TUI TOC/Index generator - .tc dot entries are written verbatim in document order as working dot commands")
{
    tocindextest::CheckTOCDotEntries<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - .tc comment, bare, indented, arithmetic and numbered-table dot entries")
{
    tocindextest::CheckTOCDotEntryEdgeCases<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - a TOC holding only dot entries is written")
{
    tocindextest::CheckTOCOnlyDotEntries<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - .ix dot entries come first in document order, unparsed, without page numbers")
{
    tocindextest::CheckIndexDotEntries<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - .ix comment, page break, bare, indented, escaped and duplicate dot entries")
{
    tocindextest::CheckIndexDotEntryEdgeCases<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - an index holding only dot entries is written")
{
    tocindextest::CheckIndexOnlyDotEntries<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - .tc .po 1i and .ix .lm 5 are written as dot command lines")
{
    tocindextest::CheckDotEntrySource<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - a kept sequence in .tc and .ix entry text leaves no space")
{
    tocindextest::CheckSequenceInEntryText<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - index marks around a word merge with .ix entries for that word")
{
    tocindextest::CheckIndexMarksMergeWithIx<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - an index mark without a partner in its paragraph gives no entry")
{
    tocindextest::CheckUnpairedIndexMarks<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - index marks in dot command lines give no entry")
{
    tocindextest::CheckIndexMarksInDotLines<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - index mark entry text is the plain text, taken as it is")
{
    tocindextest::CheckIndexMarkPlainText<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - an index mark gets the page of its line")
{
    tocindextest::CheckIndexMarkPageOfLine<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - a kept 0Eh index item is an index entry like .ix with its body")
{
    tocindextest::CheckIndexItemSequence<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - a kept 0Eh index item body: empty or control bytes give no entry, high bytes use the code page")
{
    tocindextest::CheckIndexItemSequenceBody<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - files that cannot be written are reported")
{
    tocindextest::CheckUnwritableOutput<cWSEditorCtrl>() ;
}


TEST_CASE("TUI TOC/Index generator - a source file on disk gives the golden table of contents and index")
{
    tocindextest::cScratchDir dir("tuigolden") ;
    REQUIRE(dir.IsValid()) ;
    std::string sourcePath = dir.File("golden.ws") ;
    tocindextest::WriteGoldenSource<cWSEditorCtrl>(sourcePath) ;

    std::vector<std::string> written = tocindextest::GenerateFromDisk<cWSEditorCtrl>(sourcePath) ;
    CHECK(tocindextest::Join(written) == dir.File("golden.TOC") + "|" + dir.File("golden.TO1") + "|" + dir.File("golden.IDX")) ;
    tocindextest::CheckGoldenOutputs<cWSEditorCtrl>(dir.File("golden")) ;
}
