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
// Parser based off of https://github.com/kschroeer/rtf-html-java (MIT License)
//
//////////////////////////////////////////////////////////////////////////////

/**
 * @class cRTFFile
 *
 * @brief RTF file format handler for loading and saving RTF documents.
 *
 * Implements the cRTFFile class, which provides the top-level interface for
 * RTF file operations. Delegates parsing to cRTFParser and writing to cRTFWriter.
 *
 * @section rtffile_operations Supported Operations
 * - CheckType(): detects RTF files by scanning for the "{\rtf" signature
 * - LoadFile(): opens the file and delegates to cRTFParser for recursive-descent
 *   parsing of the RTF token stream
 * - LoadFromString(): parses RTF content from a string (used for clipboard paste
 *   operations when the clipboard contains RTF-formatted text)
 * - SaveFile(): delegates to cRTFWriter to serialize the document as RTF
 *
 * @section rtffile_attribution Attribution
 * The RTF parser design is based on the recursive-descent approach from
 * https://github.com/kschroeer/rtf-html-java (MIT License), adapted for
 * C++ and WordTsar's document model.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cRTFFile RTF file handler class
 * @see cFile Base file handler class
 * @see cRTFParser Recursive-descent RTF parser
 * @see cRTFWriter RTF output serializer
 * @see cDocument Document model for loaded/saved content
 */

#include <cstdlib>
#include <stdexcept>
#include <fstream>
#include <iostream>
#include <string>

#include "rtffile.h"
#include "../core/document/document.h"

#include "rtf/write/rtfwriter.h"



/// @ingroup Editor
/// @{


extern sSeqRGBColor gBaseWSColors[] ;

/////////////////////////////////////////////////////////////////////////////
///
/// @param  cEditorCtrl *editor [in] the editor control that owns this file handler
///
/// @return nothing
///
/// @brief
/// Constructor. Passes editor to base cFile class.
///
/////////////////////////////////////////////////////////////////////////////
cRTFFile::cRTFFile(cEditorBase *editor)
    : cFile(editor)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Destructor.
///
/////////////////////////////////////////////////////////////////////////////
cRTFFile::~cRTFFile(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string filename [in] file path to check
///
/// @return true if the file has an .rtf extension
///
/// @brief
/// Check if a filename has the RTF extension (case-insensitive).
///
/////////////////////////////////////////////////////////////////////////////
bool cRTFFile::CheckType(std::string filename)
{
    std::string ext;

    size_t found = filename.find_last_of(".") ;
    ext = filename.substr(found + 1) ;

    for(size_t loop = 0; loop < ext.size(); loop++)
    {
        ext[loop] = tolower(ext[loop]) ;
    }

    return (ext == "rtf");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string input [in] RTF content as a string
///
/// @return true on success
///
/// @brief
/// Parse RTF content from a string into the document.
///
/////////////////////////////////////////////////////////////////////////////
bool cRTFFile::LoadRTFString(std::string input)
{
    cRTFParser parser(input, mEditor->GetDocument(), this) ;

    return true ;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string filename [in] path to the RTF file to load
///
/// @return true if the file was loaded successfully, false otherwise
///
/// @brief
/// Load and parse an RTF file from disk into the document.
///
/////////////////////////////////////////////////////////////////////////////
bool cRTFFile::LoadFile(std::string filename)
{
    bool retval = false ;

    FILE *fp;

    fp = fopen(filename.c_str(), "r");
    if (!fp)
    {
        return false;
    }
    else
    {
        cRTFParser parser(fp, mEditor->GetDocument(), this) ;

        fclose(fp) ;

        retval = true ;
    }

    return retval ;
}



/////////////////////////////////////////////////////////////////////////////
///
/// @param  std::string filename [in] path to save the RTF file
/// @param  POSITION_T length [in] document length (unused)
///
/// @return true if the file was saved successfully
///
/// @brief
/// Save the document as an RTF file using the RTF writer.
///
/////////////////////////////////////////////////////////////////////////////
bool cRTFFile::SaveFile(std::string filename, POSITION_T length)
{
UNUSED_ARGUMENT(length) ;

    bool retval = false ;

    cRTFWriter write(mEditor) ;
    retval = write.Start(filename) ;

    return retval ;

}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true (RTF files can always be loaded)
///
/// @brief
/// Report that this file handler supports loading.
///
/////////////////////////////////////////////////////////////////////////////
bool cRTFFile::CanLoad(void)
{
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true (RTF files can always be saved)
///
/// @brief
/// Report that this file handler supports saving.
///
/////////////////////////////////////////////////////////////////////////////
bool cRTFFile::CanSave(void)
{
    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return file extension filter string for file dialogs
///
/// @brief
/// Return the RTF file extension filter for use in file open/save dialogs.
///
/////////////////////////////////////////////////////////////////////////////
std::string cRTFFile::GetExtensions(void)
{
    return "RTF Files (*.rtf *.RTF)" ;
}




/// @}


