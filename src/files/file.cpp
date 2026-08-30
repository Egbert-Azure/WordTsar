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

/**
 * @class cFile
 *
 * @brief Base file handler implementation for WordTsar file I/O.
 *
 * Implements the cFile base class, the abstract foundation for all file format
 * handlers in WordTsar. Provides the constructor (storing the editor and document
 * pointers) and the UpdateProgress() method that forwards file loading/saving
 * progress percentages to the editor for UI display.
 *
 * @section file_hierarchy File Handler Hierarchy
 * All format-specific file handlers derive from cFile and implement:
 * - CheckType(): detect whether a file matches this format
 * - LoadFile(): parse the file and insert content into cDocument
 * - SaveFile(): serialize document content to the file format
 * - CanLoad() / CanSave(): capability query for format support
 *
 * @section file_formats Supported Formats
 * - cWordstarFile: WordStar 4.0 through 7.0 document files
 * - cRTFFile: Rich Text Format (read and write)
 * - cDOCXFile: Microsoft DOCX (read only, partial)
 * - cTextFile: Plain text with encoding auto-detection (fallback)
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cEditorBase Editor interface for progress reporting
 * @see cDocument Document model receiving loaded content
 * @see cWordstarFile WordStar format handler
 * @see cRTFFile RTF format handler
 * @see cDOCXFile DOCX format handler
 * @see cTextFile Plain text format handler
 */

#include "file.h"

#ifdef _WIN32
//#define _HAS_STD_BYTE 0  // see https://developercommunity.visualstudio.com/content/problem/93889/error-c2872-byte-ambiguous-symbol.html
#endif

/// @ingroup File
/// @{

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cEditorBase *editor [in] the editor control that owns this file handler
///
/// @return nothing
///
/// @brief
/// Constructor. Stores the editor pointer and caches the document pointer.
///
/////////////////////////////////////////////////////////////////////////////
cFile::cFile(cEditorBase *editor)
{
    mEditor = editor ;
    mDocument = mEditor->GetDocument() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  int percent [in] progress percentage (0-100)
///
/// @return nothing
///
/// @brief
/// Report file I/O progress to the editor for display in the UI.
///
/////////////////////////////////////////////////////////////////////////////
void cFile::UpdateProgress(int percent)
{
    if(mEditor != nullptr)
    {
        mEditor->FileIOProgress(percent) ;
    }
}

/// @}
