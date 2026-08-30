#ifndef CWORDSTARFILE_H
#define CWORDSTARFILE_H

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


#include <vector>
#include <string>

#include "../../core/document/doctstructs.h"
//#include "doctstructs.h"
#include "../file.h"

#include "../../core/document/document.h"
#include "fontclassifier.h"

/// @ingroup File
/// @{

/////////////////////////////////////////////////////////////////////////////
///
/// @enum eSequence
///
/// @brief
/// WordStar internal sequence types.
/// Identifies the type of embedded control sequence encountered
/// during WordStar file parsing.
///
/////////////////////////////////////////////////////////////////////////////
enum eSequence
{
    SEQ_HEADER,                 // file header block (done)
    SEQ_COLOR,                  // foreground/background color change (done)
    SEQ_FONT,                   // font selection by index (done)
    SEQ_FOOTNOTE,               // footnote marker and text (done)
    SEQ_ENDNOTE,                // endnote marker and text (done)
    SEQ_ANNOTATION,             // annotation marker and text (partial)
    SEQ_COMMENT,                // embedded comment block (partial)
    SEQ_RESERVED1,              // reserved by WordStar
    SEQ_RESERVED2,              // reserved by WordStar
    SEQ_TAB,                    // tab stop definition (partial)
    SEQ_RESERVED3,              // reserved by WordStar
    SEQ_ENDOFPAGE,              // forced page break
    SEQ_PAGEOFFSET,             // page offset override
    SEQ_PARAGRAPHNUMBER,        // auto paragraph numbering
    SEQ_INDEXENTRY,             // index entry marker
    SEQ_PRINTERCONTROL,         // raw printer control codes
    SEQ_GRAPHICS,               // embedded graphics reference
    SEQ_PARAGRAPHSTYLE,         // paragraph style definition
    SEQ_RESERVED4,              // reserved by WordStar
    SEQ_RESERVED5,              // reserved by WordStar
    SEQ_RESERVED6,              // reserved by WordStar
    SEQ_ALTFONT,                // alternate font selection
    SEQ_TRUNCATE,               // truncation marker
    SEQ_JAPANESE,               // Japanese character set sequence

    // Non-standard sequences (WordTsar extensions).
    // WordStar 7.0D ignores unknown sequences gracefully.
    SEQ_RGBCOLOR,               // RGB color triplet (WordTsar extension)
    SEQ_NEWFONT                 // full font specification (WordTsar extension) (done)
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sOrgFont
///
/// @brief
/// Original font properties from a WordStar file.
/// Stores the font name and classified properties (style, proportional,
/// math, symbol) for mapping WordStar fonts to system fonts.
/// The systemName field holds the modern cross-platform font name
/// used by BuildFontList() for index-based font resolution.
///
/////////////////////////////////////////////////////////////////////////////
struct sOrgFont
{
    std::string name ;
    eFontStyle style ;
    bool proportional ;
    bool math ;
    bool symbol ;
    std::string systemName ;
} ;

class cWordstarFile : public cFile
{
public:
    cWordstarFile(cEditorBase *editor = nullptr);
    ~cWordstarFile(void);

    bool CheckType(std::string filename) ;

    bool LoadFile(std::string filename) ;
    bool SaveFile(std::string filename, POSITION_T size) ;

    bool CanLoad(void) ;
    bool CanSave(void) ;

    std::string GetExtensions(void) ;

    void HandleChar(unsigned char c, size_t loop) ;

private :
    void HandleSequence(size_t &loop) ;
    
    void InsertExtendedChar(unsigned char c) ;
    
    void BuildFontList(void) ;

    void ParseHeader(void) ;
    void ParseColor(void) ;
    void ParseFont(void) ;
    void ParseNewFont(void) ;
    void ParseTab(void) ;

    void ParseNote(std::string sequence, eSequence type) ;
    void ParseComment(std::string sequence, eSequence type) ;

    void InsertFootnote(void) ;
    void InsertEndnote(void) ;

protected:

private:
    FILE *mFile ;
    bool mInIndex ;                     ///< true if we are in an index word, else false.
    std::string mIndexWord ;               ///< the word we need to index

    bool mExtendedChar ;                ///< true if we've seen a STYLE_EXTSTART recently

    // Variable detection state machine (&X& patterns)
    bool mInVariable ;                  ///< true if we've seen the opening '&'
    unsigned char mVariableChar ;       ///< the character between the '&' markers

    uint32_t mStyleOffset ;                           ///< pointer to start of tyles (end of document)
};


/// @}

#endif // CWORDSTARFILE_H
