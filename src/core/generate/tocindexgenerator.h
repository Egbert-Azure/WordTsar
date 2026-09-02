#ifndef CTOCINDEXGENERATOR_H
#define CTOCINDEXGENERATOR_H

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

#include <string>
#include <vector>

class cEditorBase ;

/// @ingroup Editor
/// @{

/////////////////////////////////////////////////////////////////////////////
///
/// @class cTOCIndexGenerator
///
/// @brief
/// Generates real WordStar 7 table-of-contents and index files from .tc
/// (table of contents entry) and .ix (index entry) dot commands already
/// marked in a document -- the batch-generation half of the Opening Menu's
/// classic T (table of contents) and I (index a document) commands. The
/// other half, marking the entries themselves, already existed (GUI
/// Insert->Index/TOC Entry, or typing the dot command directly); this
/// class is what actually reads them and produces output.
///
/// Real WordStar also supports marking an index entry inline over a span
/// of existing text (Insert->Mark Text for Index, the classic ^PK command)
/// rather than as its own .ix dot-command line. That mechanism is a
/// separate, pre-existing gap -- WordTsar's native .ws reader already
/// discards it entirely on load (SEQ_INDEXENTRY is skipped, unimplemented)
/// -- and is not addressed here; only freestanding .ix lines are read.
///
/////////////////////////////////////////////////////////////////////////////
class cTOCIndexGenerator
{
public:
    /////////////////////////////////////////////////////////////////////////
    ///
    /// @param  editor [in/out] editor with the source document already
    ///                 loaded and fully laid out (the same precondition
    ///                 cWSEditorCtrl::Print() requires) -- its document is
    ///                 overwritten with each generated table's content in
    ///                 turn, as a side effect of building it
    /// @param  sourcePath [in] the path the source document was loaded
    ///                 from, used to derive each output file's name
    /// @param  outputFiles [out] paths actually written, one per non-empty
    ///                 table: plain .tc entries go to "name.TOC",
    ///                 .tc1-.tc9 entries go to "name.T01".."name.T09"
    ///
    /// @return true if at least one table had entries and was written
    ///
    /// @brief
    /// Collects every .tc/.tc1-.tc9 entry in document order, resolves each
    /// to its real printed page number, substitutes that number for any
    /// unescaped '#' in the entry's own text (a literal '#' is written
    /// \# in the source, matching the real Insert->TOC Entry dialog), and
    /// writes each non-empty table to its own file.
    ///
    /////////////////////////////////////////////////////////////////////////
    static bool GenerateTOC(cEditorBase *editor, const std::string &sourcePath, std::vector<std::string> &outputFiles) ;

    /////////////////////////////////////////////////////////////////////////
    ///
    /// @param  editor [in/out] editor with the source document already
    ///                 loaded and fully laid out; its document is
    ///                 overwritten with the generated index as a side
    ///                 effect of building it
    /// @param  sourcePath [in] the path the source document was loaded from
    /// @param  outputFile [out] the path written ("name.IDX")
    ///
    /// @return true if there was at least one index entry to write
    ///
    /// @brief
    /// Collects every .ix entry, resolves +bold/-cross-reference/,subentry
    /// markup and \-escapes per the real Index Entry dialog's rules, sorts
    /// alphabetically, merges duplicate entries into one comma-separated
    /// page list (bold pages win over non-bold when the same page appears
    /// both ways), and writes the result to "name.IDX". Cross-reference
    /// entries carry no page number, matching real WordStar.
    ///
    /////////////////////////////////////////////////////////////////////////
    static bool GenerateIndex(cEditorBase *editor, const std::string &sourcePath, std::string &outputFile) ;
};

/// @}

#endif // CTOCINDEXGENERATOR_H
