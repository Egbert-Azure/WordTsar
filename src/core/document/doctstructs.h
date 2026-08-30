#ifndef DOCTSTRUCTS_H_INCLUDED
#define DOCTSTRUCTS_H_INCLUDED

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

#include <stdint.h>

#include <string>


#include "src/core/include/config.h"

#pragma pack(1)



const short WS_SEQ_HEADER = 2 ;                 ///< basically room for the size indicator
const short WS_SEQ_TRAILER = 3 ;                ///< all sequence end with 3 extra bytes that are included in the size




/////////////////////////////////////////////////////////////////////////////
///
/// @struct sSeqIntro
///
/// @brief
/// WordStar internal sequence introduction header.
/// Marks the start of an embedded control sequence in the file format.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sSeqIntro
{
    char start ;
    uint16_t size ;
    char type ;
} ; // __attribute__((packed)) ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sSeqClose
///
/// @brief
/// WordStar internal sequence closing footer.
/// Marks the end of an embedded control sequence in the file format.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sSeqClose
{
    uint16_t size ;
    char finish ;
} ; // __attribute__((packed)) ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSHeader
///
/// @brief
/// WordStar file header (128 bytes).
/// Contains version, printer driver name, and style pointer.
/// Present at the start of every WordStar document file.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSHeader
{
    unsigned char version ;                     ///< version of file in hex 0x55 for 5.5 0x60 for 6.0, etc
    char driver[9] ;                            ///< nul terminated driver name for document
    char reserved[2] ;                          ///< reserved
    uint32_t styles ;                           ///< pointer to start of tyles (end of document)
    char reserved1[105] ;                       ///< reserved to buffer to 128 chars (include 0x1d start and end bytes)
} ; // __attribute__((packed)) ;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSColor
///
/// @brief
/// WordStar color attribute.
/// Stores current and previous color index (0x00-0x0F) for text coloring.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSColor
{
    unsigned char colornumber ;                 ///< 0x00 to 0x0F
    unsigned char prevcolornumber ;
};


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sSeqRGBColor
///
/// @brief
/// RGBA color specification using short fields (range -1 to 255).
/// WordTsar extension (not in original WordStar) for full 32-bit color.
/// Used for text color and background color throughout the layout system.
/// A value of {-1,-1,-1,-1} is the "default" sentinel meaning
/// "use the editor's configured text color on screen, black for print".
///
/////////////////////////////////////////////////////////////////////////////
struct sSeqRGBColor
{
    short red ;
    short green ;
    short blue ;
    short alpha ;

    // Check if this color is the "use default" sentinel (-1,-1,-1)
    bool IsDefault() const { return red == -1 && green == -1 && blue == -1 ; }
} ;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSBasenote
///
/// @brief
/// Base structure for WordStar footnotes, endnotes, and annotations.
/// Contains line count and optional tag offset for nested sequences.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSBasenote
{
    uint16_t linecount ;                        ///< line count of note
    uint16_t  tag : 1 ;                         ///< if set, other bits define offset to an internal sequence
    uint16_t tagoffset : 15 ;                   ///< if bit 15 is one, this is the offset
    uint8_t unused ;                            ///< unused byte
//    std::string text ;                          ///< text of note or nested sequence
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSFootnote
///
/// @brief
/// WordStar footnote control sequence data.
/// Stores footnote number, display format, and conversion flags.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSFootnote
{
    uint16_t linecount ;                        ///< line count of footnote (unused)
    uint16_t number ;                           ///< footnote number
    uint8_t conversion : 4 ;                    ///< 4 convert to endnote, 6 convert to comment
    uint8_t format : 4 ;                        ///< 0 use symbols, 1 use upper case, 2 use lower case, 3 use numbers
};


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSEndnote
///
/// @brief
/// WordStar endnote control sequence data.
/// Stores endnote line count and tag number offset.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSEndnote
{
    uint16_t linecount ;                        ///< linecount of endnote
    uint16_t offset ;                           ///< offset of endnote tage number
    char unused ;
} ;



/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSAnnotation
///
/// @brief
/// WordStar annotation control sequence data.
/// Stores annotation line number and tag offset.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSAnnotation
{
    uint16_t linenumber ;                       ///< line number of annotation
    uint16_t number ;                           ///< offset of TAG annotation
    char unused ;
} ;



/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSComment
///
/// @brief
/// WordStar comment control sequence data.
/// Stores comment line count and conversion flag.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSComment
{
    uint16_t linecount ;                        ///< line count of comment
    uint16_t unused ;
    char conversion ;                           ///< conversion flag
} ;



/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSTab
///
/// @brief
/// WordStar tab stop definition.
/// Stores tab position, type (normal, center, right, decimal), and size.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSTab
{
    int16_t tabsize ;                           ///< 1/1800 inch
    int16_t abstabsize ;                        ///< 1/1800 inch
    unsigned char type ;                        ///< tab type (tab, center just, right just, etc) (use char, not enum, to get size right)
    char size ;                                 ///< 1/10 inch
} ; // __attribute__((packed)) ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSParagraphNumber
///
/// @brief
/// WordStar paragraph numbering state.
/// Tracks outline level, level numbers (1-8), and paragraph format string.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSParagraphNumber
{
    char levelincrease ;                        ///< 0-same level  1-move forward inlevel (2 - 2.1)  >1-level moves forward from previous pararaph number
    char leveldecrease ;                        ///< 0-same level  1-move forward inlevel (2.1 - 2)  >1-level moves backward from previous pararaph number
    char currentlevel ;                         ///< level of current paragraph
    uint16_t levelnumber[8] ;                   ///< 1 - 8
    char paraformat[31] ;                       ///< string conatining paragrah format
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSParagraphStyle
///
/// @brief
/// WordStar paragraph style reference.
/// Links current, previous, and modified style numbers for style tracking.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSParagraphStyle
{
    uint16_t number ;                           ///< new paragraph style number
    uint16_t previous ;                         ///< previous paragraph style number
    uint16_t prevmodified ;                     ///< previous modified style number
    uint16_t prevprev ;                         ///< previous previous for reverting
} ;



/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSStyleLibrary
///
/// @brief
/// WordStar style library header.
/// Points to the next free block and tracks object count.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSStyleLibrary
{
    uint16_t nextblock ;                        ///< next free 512 byte block (relative to start)
    unsigned char count ;                       ///< object count (currently 1)
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sWSFont
///
/// @brief
/// WordStar font definition from file format.
/// Stores current and previous font dimensions and style bits.
/// Width in 1/1800 inch, height in 1/1440 inch (twips).
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sWSFont
{
    uint16_t width ;                            ///< in 1/1800 of an inch
    uint16_t height ;                           ///< in 1/1440 of an inch
    uint16_t style ;                            ///< type style bits
    uint16_t prevwidth ;                        ///< in 1/1800 of an inch
    uint16_t prevheight ;                       ///< in 1/1440 of an inch
    uint16_t prevstyle ;                        ///< type style
};

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sNewFont
///
/// @brief
/// WordStar 7 new font specification.
/// Fixed-size font name (50 chars) and size for file format storage.
/// Packed for binary file I/O.
///
/////////////////////////////////////////////////////////////////////////////
struct sNewFont
{
    char fontname[50] ;                         ///< font name
    uint16_t size ;                             ///< font size
} ;

#pragma pack()

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sInternalFonts
///
/// @brief
/// Internal font representation used by the document model.
/// Maps between the real system font name/size and the original
/// WordStar font structure (if loaded from a WordStar file).
///
/////////////////////////////////////////////////////////////////////////////
struct sInternalFonts
{
    std::string fontname ;                      ///< real font name
    double size ;                               ///< real font size
    bool haveWSFont ;                           ///< only true if reading from a Wordstar file
    sWSFont wsfont ;                            ///< if we read from a file, this is the original font structure
    std::string name ;                          ///< wordstars name for this font
} ;

/////////////////////////////////////////////////////////////////////////////
///
/// @struct sExtendedChars
///
/// @brief
/// Code page conversion mapping entry.
/// Maps a WordStar extended character byte to its UTF-8 codepoint.
///
/////////////////////////////////////////////////////////////////////////////
struct sExtendedChars
{
    unsigned char wordstarchar ;
    unsigned long utf8char ;
} ;


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sNote
///
/// @brief
/// Footnote or endnote content.
/// Stores the note numbering symbol type and the note text.
///
/////////////////////////////////////////////////////////////////////////////
struct sNote
{
    eNoteSymbol symbol ;
    std::string text ;
};


#endif // DOCTSTRUCTS_H_INCLUDED
