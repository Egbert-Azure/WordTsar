#ifndef CTEXTFILE_H
#define CTEXTFILE_H

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

#include <string>
#include <vector>

#include "src/core/document/document.h"
#include "file.h"

/// @ingroup Editor
/// @{

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eTextEncoding
///
/// @brief
/// Text file encoding types detected during plain text file loading.
/// Determined by BOM detection or heuristic analysis of file content.
///
/////////////////////////////////////////////////////////////////////////////
enum class eTextEncoding
{
    UTF8,           // UTF-8 with or without BOM (EF BB BF)
    UTF16LE,        // UTF-16 Little Endian (BOM: FF FE)
    UTF16BE,        // UTF-16 Big Endian (BOM: FE FF)
    UTF32LE,        // UTF-32 Little Endian (BOM: FF FE 00 00)
    UTF32BE,        // UTF-32 Big Endian (BOM: 00 00 FE FF)
    LATIN1,         // ISO-8859-1 / Windows-1252
    DOS_CP437,      // DOS Code Page 437 (US English)
    DOS_CP850,      // DOS Code Page 850 (Western European)
    BINARY,         // Not a text file
    SYSTEM_LOCALE   // Fallback: ASCII/system locale encoding
};

class cTextFile : public cFile
{
public:
    cTextFile(cEditorBase *editor = nullptr);
    ~cTextFile(void);

    bool CheckType(std::string filename);
    bool LoadFile(std::string filename);
    bool SaveFile(std::string filename, POSITION_T size);

    bool CanLoad(void);
    bool CanSave(void);

    std::string GetExtensions(void);

protected:

private:
    eTextEncoding DetectBOM(const std::vector<unsigned char>& buffer, size_t& bomSize);
    eTextEncoding DetectEncodingHeuristic(const std::vector<unsigned char>& buffer, size_t length);
    bool IsValidUTF8(const unsigned char* data, size_t length);
    bool IsBinaryFile(const std::vector<unsigned char>& buffer, size_t length);
    eTextEncoding GuessCodePage(const std::vector<unsigned char>& buffer, size_t length);
    std::string ConvertUTF16ToUTF8(const std::vector<unsigned char>& buffer, size_t length, bool littleEndian);
    std::string ConvertUTF32ToUTF8(const std::vector<unsigned char>& buffer, size_t length, bool littleEndian);
    std::string ConvertLatin1ToUTF8(const std::vector<unsigned char>& buffer, size_t offset, size_t length);
    std::string ConvertCP437ToUTF8(const std::vector<unsigned char>& buffer, size_t offset, size_t length);
    std::string ConvertCP850ToUTF8(const std::vector<unsigned char>& buffer, size_t offset, size_t length);
    std::string NormalizeLineEndings(const std::string& content);
};


/// @}

#endif // CTEXTFILE_H
