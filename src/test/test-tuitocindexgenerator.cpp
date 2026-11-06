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
