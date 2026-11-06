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
 * @class cWordstarFile
 *
 * @brief WordStar file format handler implementation.
 *
 * This file implements the cWordstarFile class which provides functionality for loading,
 * parsing, and saving WordStar document files. The implementation handles the WordStar
 * file format including:
 *
 * - File format detection and version identification
 * - Character encoding and extended character support (CP437, CP737, CP850, CP1252)
 * - Text formatting (bold, italic, underline, superscript, subscript, strikethrough)
 * - Font management and mapping between WordStar and system fonts
 * - Color formatting sequences
 * - Tab handling and positioning
 * - Footnotes and endnotes
 * - Comments and annotations
 * - Document headers and metadata
 * - Variable detection and insertion (&X& patterns)
 * - Index entry detection
 *
 * The WordStar format uses a combination of control characters and embedded sequences
 * to store formatting information. This implementation converts these format codes
 * into modern document representations while preserving the original formatting intent.
 *
 * @section font_mapping Font Mapping
 * The implementation includes a comprehensive font database (gOrgFonts) that maps
 * WordStar's original font names to modern system fonts. The font classifier
 * attempts to match fonts based on style (serif, sans-serif, script) and spacing
 * (monospace vs. proportional). Specialty fonts (borders, barcode, printer modes,
 * music notation) use STYLE_UNKNOWN since they do not fit standard categories.
 *
 * @section file_versions Supported Versions
 * - WordStar 4.0 and earlier (no version header)
 * - WordStar 5.0+ (with 0x1D version header)
 * - All versions through WordStar 8.0
 * - Also handles backup (.ws-bak) and temp (.ws-$$$) files
 *
 * @section limitations Current Limitations
 * - Some sequence types (graphics, paragraph styles) are not implemented
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cFile Base class for file handling
 * @see cEditorBase Editor base class interface
 * @see cDocument Document model representation
 * @see cFontClassifier Font classification utilities
 * @see sOrgFont Original font property storage
 */

#include <algorithm>
#include <bitset>
#include <cctype>
#include <climits>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <copyfile.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "wordstarfile.h"
#include "fontclassifier.h"
#include "wsfontclassifier.h"
#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp737.h"
#include "src/core/codepage/cp850.h"
#include "src/core/codepage/cp1252.h"
#include "src/core/include/utils.h"
#include "unicodelib_encodings.h"

// a WordStar binding space (0x0F) is a U+00A0 non-breaking space in the document
static constexpr CHAR_T NOBREAK_SPACE = 0x00A0 ;
static const std::string NOBREAK_SPACE_UTF8 = "\xC2\xA0" ;

/// @ingroup File
/// @{

extern sSeqRGBColor gBaseWSColors[WS_PALETTE_SIZE] ;

std::vector<std::string>gSysFontName ;  ///< closest matching system font name

std::vector<sOrgFont> gOrgFonts =
{
    // WordStar 7 font table mapping original printer fonts to modern system fonts
    {"LinePrinter",       STYLE_SANS,    false, false, false, "Courier New"},    // monospaced line printer
    {"Pica",              STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced serif
    {"Elite",             STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced serif
    {"Courier",           STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced serif
    {"Helvetica",         STYLE_SANS,    true,  false, false, "Arial"},          // neo-grotesque sans
    {"Times Roman",       STYLE_SERIF,   true,  false, false, "Times New Roman"},
    {"Gothic",            STYLE_SANS,    true,  false, false, "Arial"},          // gothic sans-serif
    {"Script",            STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"Prestige",          STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced typewriter
    {"Caslon",            STYLE_SERIF,   true,  false, false, "Georgia"},        // transitional serif
    {"Orator",            STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Presentations",     STYLE_SANS,    true,  false, false, "Arial"},          // presentation sans
    {"Helvetica Cond",    STYLE_SANS,    true,  false, false, "Arial"},          // condensed Helvetica
    {"Serifa",            STYLE_SERIF,   true,  false, false, "Georgia"},        // slab serif
    {"Blippo",            STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Windsor",           STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"Century",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// transitional serif
    {"ZapfHumanist",      STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans-serif
    {"Garamond",          STYLE_SERIF,   true,  false, false, "Times New Roman"},// old-style serif
    {"Cooper",            STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"Coronet",           STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"Broadway",          STYLE_SANS,    true,  false, false, "Arial"},          // decorative display sans
    {"Bondoni",           STYLE_SERIF,   true,  false, false, "Georgia"},        // transitional serif
    {"Century Schoolbook",STYLE_SERIF,   true,  false, false, "Times New Roman"},
    {"Universe Roman",    STYLE_SANS,    true,  false, false, "Arial"},          // Univers sans-serif
    {"Helvetica Outline", STYLE_SANS,    true,  false, false, "Arial"},          // outline Helvetica
    {"Peignot",           STYLE_SANS,    true,  false, false, "Arial"},          // art deco sans
    {"Clarendon",         STYLE_SERIF,   true,  false, false, "Georgia"},        // slab serif
    {"Stick",             STYLE_SANS,    true,  false, false, "Arial"},          // simple sans
    {"HP-GL Drafting",    STYLE_SANS,    false, false, false, "Courier New"},    // technical monospaced
    {"HP-GL Spline",      STYLE_SANS,    false, false, false, "Courier New"},    // technical monospaced
    {"Times",             STYLE_SERIF,   true,  false, false, "Times New Roman"},
    {"HPLJ Soft Font",    STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced printer font
    {"Borders",           STYLE_UNKNOWN, true,  false, false, "Courier New"},    // decorative borders
    {"Uncle Sam Open",    STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Raphael",           STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"Uncial",            STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// uncial script
    {"Manhattan",         STYLE_SANS,    true,  false, false, "Arial"},          // geometric sans
    {"Dom Casual",        STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// casual script
    {"Old English",       STYLE_SERIF,   true,  false, false, "Times New Roman"},// blackletter
    {"Trium Condensed",   STYLE_SANS,    true,  false, false, "Arial"},          // condensed sans
    {"Trium UltraComp",   STYLE_SANS,    true,  false, false, "Arial"},          // ultra-compressed sans
    {"Trade ExtraCond",   STYLE_SANS,    true,  false, false, "Arial"},          // extra-condensed sans
    {"American Classic",  STYLE_SERIF,   true,  false, false, "Times New Roman"},// classic serif
    {"Globe Gothic Outline", STYLE_SANS, true,  false, false, "Arial"},          // outline sans-serif
    {"UniversCondensed",  STYLE_SANS,    true,  false, false, "Arial"},          // condensed Univers
    {"Univers",           STYLE_SANS,    true,  false, false, "Arial"},          // neo-grotesque sans
    {"TmsRmnCond",        STYLE_SERIF,   true,  false, false, "Times New Roman"},// Times Roman Condensed
    {"PrstElite",         STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced typewriter
    {"Optima",            STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans-serif
    {"Aachen",            STYLE_SERIF,   true,  false, false, "Georgia"},        // slab serif display
    {"Am Typewriter",     STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced typewriter
    {"Avant Garde",       STYLE_SANS,    true,  false, false, "Arial"},          // geometric sans
    {"Beguiat",           STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"Brush Script",      STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// brush script
    {"Carta",             STYLE_UNKNOWN, true,  false, false, "Symbol"},         // cartographic symbols
    {"Centennial",        STYLE_SERIF,   true,  false, false, "Times New Roman"},// transitional serif
    {"Cheltenham",        STYLE_SERIF,   true,  false, false, "Georgia"},        // old-style serif
    {"FranklinGothic",    STYLE_SANS,    true,  false, false, "Arial"},          // gothic sans-serif
    {"FrstyleScrpt",      STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// freestyle script
    {"FrizQuadrata",      STYLE_SERIF,   true,  false, false, "Georgia"},        // display serif
    {"Futura",            STYLE_SANS,    true,  false, false, "Arial"},          // geometric sans
    {"Galliard",          STYLE_SERIF,   true,  false, false, "Georgia"},        // old-style serif
    {"Glypha",            STYLE_SERIF,   true,  false, false, "Georgia"},        // slab serif
    {"Goudy",             STYLE_SERIF,   true,  false, false, "Georgia"},        // old-style serif
    {"Hobo",              STYLE_SANS,    true,  false, false, "Arial"},          // decorative sans
    {"LubalinGraph",      STYLE_SANS,    true,  false, false, "Arial"},          // geometric slab
    {"Lucida",            STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans
    {"LucidaMath",        STYLE_SANS,    true,  true,  false, "Symbol"},         // math font
    {"Machine",           STYLE_SANS,    false, false, false, "Courier New"},    // monospaced industrial
    {"Melior",            STYLE_SERIF,   true,  false, false, "Georgia"},        // rounded serif
    {"NewBaskrvlle",      STYLE_SERIF,   true,  false, false, "Georgia"},        // New Baskerville
    {"NewCntSchlbk",      STYLE_SERIF,   true,  false, false, "Times New Roman"},// New Century Schoolbook
    {"News Gothic",       STYLE_SANS,    true,  false, false, "Arial"},          // gothic sans-serif
    {"Palantino",         STYLE_SERIF,   true,  false, false, "Times New Roman"},// old-style serif
    {"Park Avenue",       STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"Revue",             STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Sonata",            STYLE_UNKNOWN, true,  false, false, "Symbol"},         // music notation
    {"Stencil",           STYLE_SANS,    true,  false, false, "Arial"},          // stencil display sans
    {"Souvenir",          STYLE_SERIF,   true,  false, false, "Georgia"},        // soft serif
    {"TrmpMedievel",      STYLE_SERIF,   true,  false, false, "Times New Roman"},// medieval serif
    {"ZapfChancery",      STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// calligraphic script
    {"ZapfDingbats",      STYLE_UNKNOWN, true,  false, true,  "Symbol"},         // symbol font
    {"Stone",             STYLE_SERIF,   true,  false, false, "Times New Roman"},// humanist serif
    {"CntryOldStyle",     STYLE_SERIF,   true,  false, false, "Georgia"},        // old-style serif
    {"Corona",            STYLE_SERIF,   true,  false, false, "Times New Roman"},// newspaper serif
    {"GoudyOldStyle",     STYLE_SERIF,   true,  false, false, "Georgia"},        // old-style serif
    {"Excelsior",         STYLE_SERIF,   true,  false, false, "Times New Roman"},// newspaper serif
    {"FuturaCondensed",   STYLE_SANS,    true,  false, false, "Arial"},          // condensed Futura
    {"HelvCompressed",    STYLE_SANS,    true,  false, false, "Arial"},          // compressed Helvetica
    {"HelvExtraCompressed",STYLE_SANS,   true,  false, false, "Arial"},          // extra-compressed Helvetica
    {"Helv Narrow",       STYLE_SANS,    true,  false, false, "Arial"},          // narrow Helvetica
    {"HelvUltaCompressed",STYLE_SANS,    true,  false, false, "Arial"},          // ultra-compressed Helvetica
    {"KorinnaKursiv",     STYLE_SERIF,   true,  false, false, "Georgia"},        // italic Korinna serif
    {"Lucida Sans",       STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans
    {"Memphis",           STYLE_SERIF,   true,  false, false, "Georgia"},        // slab serif
    {"Stone Informal",    STYLE_SERIF,   true,  false, false, "Times New Roman"},// informal humanist serif
    {"Stone Sans",        STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans
    {"Stone Serif",       STYLE_SERIF,   true,  false, false, "Times New Roman"},// humanist serif
    {"Postscript",        STYLE_SERIF,   true,  false, false, "Times New Roman"},// PostScript default serif
    {"NPS Utility",       STYLE_UNKNOWN, false, false, false, "Courier New"},    // printer utility mode
    {"NPS Draft",         STYLE_UNKNOWN, false, false, false, "Courier New"},    // printer draft mode
    {"NPS Corr",          STYLE_SERIF,   true,  false, false, "Times New Roman"},// printer correspondence
    {"NPS SanSer Qual",   STYLE_SANS,    true,  false, false, "Arial"},          // printer sans quality
    {"NPS Serif Qual",    STYLE_SERIF,   true,  false, false, "Times New Roman"},// printer serif quality
    {"PS Utility",        STYLE_UNKNOWN, false, false, false, "Courier New"},    // printer utility mode
    {"PS Draft",          STYLE_UNKNOWN, false, false, false, "Courier New"},    // printer draft mode
    {"PS Corr",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// printer correspondence
    {"PS SanSer Qual",    STYLE_SANS,    true,  false, false, "Arial"},          // printer sans quality
    {"PS Serif Qual",     STYLE_SERIF,   true,  false, false, "Times New Roman"},// printer serif quality
    {"Download",          STYLE_UNKNOWN, true,  false, false, "Arial"},          // downloadable font
    {"NPS ECS Qual",      STYLE_SERIF,   true,  false, false, "Times New Roman"},// printer ECS quality
    {"PS Plastic",        STYLE_UNKNOWN, true,  false, false, "Arial"},          // printer plastic mode
    {"PS Metal",          STYLE_UNKNOWN, true,  false, false, "Arial"},          // printer metal mode
    {"CloisterBlack",     STYLE_SERIF,   true,  false, false, "Times New Roman"},// blackletter
    {"Gill Sans",         STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans
    {"Rockwell",          STYLE_SERIF,   true,  false, false, "Georgia"},        // slab serif
    {"Tiffany",           STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"Clearface",         STYLE_SERIF,   true,  false, false, "Georgia"},        // transitional serif
    {"Amelia",            STYLE_SANS,    true,  false, false, "Arial"},          // rounded display sans
    {"HandelGothic",      STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"OratorSC",          STYLE_SANS,    true,  false, false, "Arial"},          // small-caps sans
    {"Outline",           STYLE_SANS,    true,  false, false, "Arial"},          // outline sans
    {"Bookman Light",     STYLE_SERIF,   true,  false, false, "Times New Roman"},// light serif
    {"Humanist",          STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans-serif
    {"Swiss Narrow",      STYLE_SANS,    true,  false, false, "Arial"},          // narrow Swiss sans
    {"ZapfCalligraphic",  STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// calligraphic script
    {"Spreadsheet",       STYLE_SANS,    false, false, false, "Courier New"},    // monospaced tabular
    {"Broughm",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Anelia",            STYLE_SANS,    true,  false, false, "Arial"},          // sans-serif
    {"LtrGothic",         STYLE_SANS,    true,  false, false, "Arial"},          // gothic sans-serif
    {"Boldface",          STYLE_SANS,    true,  false, false, "Arial"},          // bold display sans
    {"High Density",      STYLE_UNKNOWN, false, false, false, "Courier New"},    // printer mode
    {"High Speed",        STYLE_UNKNOWN, false, false, false, "Courier New"},    // printer mode
    {"Super Focus",       STYLE_UNKNOWN, false, false, false, "Courier New"},    // printer mode
    {"Swiss Outline",     STYLE_SANS,    true,  false, false, "Arial"},          // outline Swiss sans
    {"Swiss Display",     STYLE_SANS,    true,  false, false, "Arial"},          // display Swiss sans
    {"Momento Outline",   STYLE_SANS,    true,  false, false, "Arial"},          // outline display sans
    {"Courier Italic",    STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced italic
    {"Text Light",        STYLE_SERIF,   true,  false, false, "Times New Roman"},// light text serif
    {"Momento Heavy",     STYLE_SANS,    true,  false, false, "Arial"},          // heavy display sans
    {"BarCode",           STYLE_UNKNOWN, false, false, false, "Courier New"},    // barcode font
    {"EAN/UPC",           STYLE_UNKNOWN, false, false, false, "Courier New"},    // barcode font
    {"Math-7",            STYLE_UNKNOWN, true,  true,  false, "Symbol"},         // math font
    {"Math-8",            STYLE_UNKNOWN, true,  true,  false, "Symbol"},         // math font
    {"Swiss",             STYLE_SANS,    true,  false, false, "Arial"},          // Swiss sans-serif
    {"Dutch",             STYLE_SERIF,   true,  false, false, "Times New Roman"},// Dutch serif
    {"Trend",             STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Holsatia",          STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Serif",             STYLE_SERIF,   true,  false, false, "Times New Roman"},// generic serif
    {"Bandit",            STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Bookman",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// old-style serif
    {"Casual",            STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// casual script
    {"Dot",               STYLE_UNKNOWN, false, false, false, "Courier New"},    // dot matrix monospaced
    {"EDP",               STYLE_UNKNOWN, true,  false, false, "Courier New"},    // data processing
    {"ExtGraphics",       STYLE_UNKNOWN, true,  false, false, "Courier New"},    // extended graphics
    {"Garland",           STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"PC Line",           STYLE_UNKNOWN, false, false, false, "Courier New"},    // line drawing monospaced
    {"HP Line",           STYLE_UNKNOWN, false, false, false, "Courier New"},    // line drawing monospaced
    {"Hamilton",          STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Korinna",           STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"LineDrw",           STYLE_UNKNOWN, true,  false, false, "Courier New"},    // line drawing
    {"Modern",            STYLE_SANS,    true,  false, false, "Arial"},          // modern sans
    {"Momento",           STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"MX",                STYLE_UNKNOWN, true,  false, false, "Arial"},          // printer mode
    {"PC",                STYLE_UNKNOWN, true,  false, false, "Arial"},          // PC character set
    {"PI",                STYLE_UNKNOWN, true,  false, false, "Symbol"},         // pi/symbol font
    {"Profile",           STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Q-Fmt",             STYLE_UNKNOWN, true,  false, false, "Courier New"},    // formatting utility
    {"Rule",              STYLE_UNKNOWN, true,  false, false, "Courier New"},    // rule drawing
    {"SB",                STYLE_UNKNOWN, true,  false, false, "Courier New"},    // special characters
    {"Taylor",            STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Text",              STYLE_SERIF,   true,  false, false, "Times New Roman"},// generic text serif
    {"APL",               STYLE_UNKNOWN, true,  false, false, "Symbol"},         // APL programming symbols
    {"Artisan",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Triumvirate",       STYLE_SANS,    true,  false, false, "Arial"},          // sans-serif
    {"Chart",             STYLE_UNKNOWN, true,  false, false, "Courier New"},    // chart drawing
    {"Classic",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// classic serif
    {"Data",              STYLE_UNKNOWN, true,  false, false, "Courier New"},    // data processing
    {"Document",          STYLE_SERIF,   true,  false, false, "Times New Roman"},// document serif
    {"Emperor",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Essay",             STYLE_SERIF,   true,  false, false, "Times New Roman"},// essay serif
    {"Forms",             STYLE_UNKNOWN, true,  false, false, "Courier New"},    // forms drawing
    {"Facet",             STYLE_SANS,    true,  false, false, "Arial"},          // geometric sans
    {"Micro",             STYLE_UNKNOWN, true,  false, false, "Courier New"},    // micro printing
    {"OCR-A",             STYLE_SANS,    false, false, false, "Courier New"},    // monospaced OCR
    {"OCR-B",             STYLE_SANS,    false, false, false, "Courier New"},    // monospaced OCR
    {"Apollo",            STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Math",              STYLE_UNKNOWN, true,  true,  false, "Symbol"},         // math font
    {"Scientific",        STYLE_SERIF,   true,  false, false, "Times New Roman"},// scientific serif
    {"Sonoran",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Square 3",          STYLE_SANS,    true,  false, false, "Arial"},          // geometric sans
    {"Symbol",            STYLE_UNKNOWN, true,  false, true,  "Symbol"},         // symbol font
    {"Tempora",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Title",             STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Titan",             STYLE_SANS,    true,  false, false, "Arial"},          // display sans
    {"Theme",             STYLE_SERIF,   true,  false, false, "Times New Roman"},// thematic serif
    {"TaxLineDraw",       STYLE_UNKNOWN, true,  false, false, "Courier New"},    // tax form line drawing
    {"Vintage",           STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"XCP",               STYLE_UNKNOWN, true,  false, false, "Arial"},          // extended character set
    {"Eletto",            STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Est Elite",         STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced typewriter
    {"Idea",              STYLE_SANS,    true,  false, false, "Arial"},          // sans-serif
    {"Italico",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// italic serif
    {"Kent",              STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Mikron",            STYLE_SANS,    true,  false, false, "Arial"},          // sans-serif
    {"Notizia",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// news serif
    {"Roma",              STYLE_SERIF,   true,  false, false, "Times New Roman"},// roman serif
    {"Presentor",         STYLE_SANS,    true,  false, false, "Arial"},          // presentation sans
    {"Victoria",          STYLE_SERIF,   true,  false, false, "Georgia"},        // decorative serif
    {"Draft Italic",      STYLE_SERIF,   false, false, false, "Courier New"},    // monospaced italic draft
    {"PS Capita",         STYLE_SERIF,   true,  false, false, "Times New Roman"},// PostScript serif
    {"Qual Italic",       STYLE_SERIF,   true,  false, false, "Times New Roman"},// quality italic serif
    {"Antique Olive",     STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans
    {"Bauhaus",           STYLE_SANS,    true,  false, false, "Arial"},          // geometric sans
    {"Era",               STYLE_SANS,    true,  false, false, "Arial"},          // sans-serif
    {"Mincho",            STYLE_SERIF,   true,  false, false, "Times New Roman"},// Japanese serif
    {"SerifGothic",       STYLE_SERIF,   true,  false, false, "Georgia"},        // serif-gothic hybrid
    {"Signet Roundland",  STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"Souvenir Gothic",   STYLE_SERIF,   true,  false, false, "Georgia"},        // serif variant
    {"Stymie",            STYLE_SANS,    true,  false, false, "Arial"},          // slab sans
    {"Bernhard Modern",   STYLE_SANS,    true,  false, false, "Times New Roman"},// modern serif
    {"Grand Ronde Script",STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// round script
    {"Ondine",            STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"PT Barnum",         STYLE_SERIF,   true,  false, false, "Georgia"},        // display serif
    {"Kaufmann",          STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"Bolt",              STYLE_SANS,    true,  false, false, "Arial"},          // bold display sans
    {"AntOliveCompact",   STYLE_SANS,    true,  false, false, "Arial"},          // compact Antique Olive
    {"Garth Graphic",     STYLE_SERIF,   true,  false, false, "Georgia"},        // graphic serif
    {"Ronda",             STYLE_SANS,    true,  false, false, "Arial"},          // rounded sans
    {"EngSchreibschrift", STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// German script
    {"Flash",             STYLE_SCRIPT,  true,  false, false, "Times New Roman"},// script font
    {"Gothic Outline",    STYLE_SANS,    true,  false, false, "Arial"},          // outline gothic sans
    {"Akzidenz-Grotesk",  STYLE_SANS,    true,  false, false, "Arial"},          // grotesque sans
    {"TD Logos",          STYLE_UNKNOWN, true,  false, false, "Arial"},          // logo font
    {"Shannon",           STYLE_SANS,    true,  false, false, "Arial"},          // humanist sans
    {"Oberon",            STYLE_SERIF,   true,  false, false, "Times New Roman"},// serif
    {"Callisto",          STYLE_SERIF,   true,  false, false, "Georgia"},        // transitional serif
    {"Charter",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// transitional serif
    {"Plantin",           STYLE_SERIF,   true,  false, false, "Times New Roman"},// old-style serif
    {"Helvetica Black",   STYLE_SANS,    true,  false, false, "Arial"},          // heavy Helvetica
    {"Helvetica Light",   STYLE_SANS,    true,  false, false, "Arial"},          // light Helvetica
    {"Arnold Bocklin",    STYLE_SANS,    true,  false, false, "Arial"},          // art nouveau display
    {"Fette Fraktur",     STYLE_SERIF,   true,  false, false, "Times New Roman"},// blackletter
    {"Greek PS",          STYLE_SERIF,   true,  true,  false, "Symbol"}          // Greek math font
};



/**
 * @brief Constructor for cWordstarFile class.
 * 
 * Initializes a new WordStar file instance with the provided editor control.
 * Sets up the editor reference, builds the font list if the system font name
 * is empty, and initializes internal state flags.
 * 
 * @param editor Pointer to the editor control that will be associated with this WordStar file
 * 
 * @see cFile
 */
cWordstarFile::cWordstarFile(cEditorBase *editor)
    : cFile(editor)
{
    if(gSysFontName.empty() == true)
    {
        BuildFontList() ;
    }
    
    mInIndex = false ;
    mExtendedChar = false;
    mLastChar = 0 ;
    mExtStartPosition = 0 ;
    mExtStartChar = 0 ;
    mInVariable = false ;
    mVariableChar = 0 ;
}

cWordstarFile::~cWordstarFile(void)
{
}


// Forward declaration of file-local content detection helper
static bool IsWordStarContent(const std::string& filename) ;


/**
 * @brief Checks if a file is a WordStar document.
 *
 * A file with a WordStar extension is accepted without reading it. Any
 * other file is accepted when IsWordStarContent() accepts its content.
 *
 * @param filename The full filename (including path) to check
 * @return true if the file is a WordStar document, false otherwise
 *
 * @note WordStar extensions: ws, ws3, ws4, ws5, ws6, ws7, ws8, ws-bak, ws-$$$
 *
 * @see IsWordStarContent() for the content rules
 * @see cWordstarFile::GetExtensions() for the list of supported extensions.
 */
bool cWordstarFile::CheckType(std::string filename)
{
    // Try extension-based detection first
    size_t found = filename.find_last_of(".") ;
    if (found != std::string::npos)
    {
        std::string ext = filename.substr(found + 1) ;

        for(size_t loop = 0; loop < ext.size(); loop++)
        {
            ext[loop] = tolower(ext[loop]) ;
        }

        if (ext == "ws" || ext == "ws3" || ext == "ws4" ||
            ext == "ws5" || ext == "ws6" || ext == "ws7" ||
            ext == "ws8" || ext == "ws-bak" || ext == "ws-$$$")
        {
            return true ;
        }
    }

    // Extension unknown or missing: fall back to content-based detection
    return IsWordStarContent(filename) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] path to the file to inspect
///
/// @return true if the file content looks like a WordStar document
///
/// @brief
/// Content-based WordStar detection for files with unknown or missing
/// extensions.
///
/// Accepts a file that starts with a WordStar 5+ header sequence (0x1D,
/// size, type 0x00) whose size + 3 bytes fit in the file. Rejects known non-WordStar formats by their magic
/// bytes. Otherwise scans up to 256 KiB, up to the first 0x1A outside a
/// sequence or an ESC x FS triple. A sequence is valid when its 16-bit
/// length L is at least 4, the bytes at +L and +L+1 repeat the length and
/// the byte at +L+2 is 0x1D; a valid sequence is skipped whole.
///
/// Rejects when:
///   - a 0x1D does not start a valid sequence
///   - a 0x00 is outside a sequence
///   - a byte 0xA1-0xFF is not followed by 0x20, 0x0D, 0x0A, 0x8D, 0xA0,
///     0x09, 0x1A, 0x1B or a style toggle (0x02, 0x13, 0x14, 0x16, 0x18,
///     0x19)
///   - the high bytes form valid UTF-8 with at least one multi-byte character
///   - 0x1A padding to a multiple of 128 is followed by 14 00 11 (WordStar
///     7 macro)
///
/// Then accepts when it finds a soft return (8D 0A), a valid sequence, an
/// ESC x FS triple, or a control byte (0x01-0x08, 0x0B, 0x0E-0x19, 0x1E,
/// 0x1F) together with a dot command line.
///
/// @see sSeqIntro for the sequence header structure
/// @see eModifiers for WordStar control byte definitions
///
/////////////////////////////////////////////////////////////////////////////
static bool IsWordStarContent(const std::string& filename)
{
    FILE* file = fopen(filename.c_str(), "rb") ;
    if (!file)
    {
        return false ;
    }

    static const size_t SCAN_SIZE = 256 * 1024 ;
    std::vector<unsigned char> data(SCAN_SIZE) ;
    size_t bytesRead = fread(data.data(), 1, SCAN_SIZE, file) ;
    fclose(file) ;
    const unsigned char* buf = data.data() ;

    if (bytesRead < 4)
    {
        return false ;
    }

    // --- WordStar 5+ detection ---
    // Check for valid sequence header: 0x1D, size[2], type=0x00.
    // The header occupies size + 3 bytes and must fit in the file.
    if (buf[0] == 0x1D && buf[3] == 0x00)
    {
        uint16_t size = static_cast<uint16_t>(buf[1]) |
                        (static_cast<uint16_t>(buf[2]) << 8) ;
        if (size >= 4 && static_cast<size_t>(size) + 3 <= bytesRead)
        {
            return true ;
        }
    }

    // --- Rule out known non-WS formats ---
    // Check magic bytes to avoid false positives from binary files
    // that might contain stray bytes matching WS4 heuristics

    // Tier 1: formats most likely to be opened by a user

    // ZIP/DOCX/XLSX/PPTX/ODT: starts with "PK" (0x50 0x4B)
    if (buf[0] == 0x50 && buf[1] == 0x4B)
    {
        return false ;
    }

    // RTF: starts with "{\rtf"
    if (bytesRead >= 5 &&
        buf[0] == '{' && buf[1] == '\\' && buf[2] == 'r' &&
        buf[3] == 't' && buf[4] == 'f')
    {
        return false ;
    }

    // PDF: starts with "%PDF"
    if (buf[0] == '%' && buf[1] == 'P' && buf[2] == 'D' && buf[3] == 'F')
    {
        return false ;
    }

    // ELF binary: starts with 0x7F "ELF"
    if (buf[0] == 0x7F && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F')
    {
        return false ;
    }

    // Windows executable: starts with "MZ"
    if (buf[0] == 0x4D && buf[1] == 0x5A)
    {
        return false ;
    }

    // OLE Compound Document (old .doc, .xls, .ppt): 0xD0 0xCF 0x11 0xE0
    if (buf[0] == 0xD0 && buf[1] == 0xCF && buf[2] == 0x11 && buf[3] == 0xE0)
    {
        return false ;
    }

    // PNG image: 0x89 "PNG"
    if (buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G')
    {
        return false ;
    }

    // JPEG image: 0xFF 0xD8 0xFF
    if (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF)
    {
        return false ;
    }

    // GIF image: "GIF8"
    if (buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == '8')
    {
        return false ;
    }

    // BMP image: "BM"
    if (buf[0] == 'B' && buf[1] == 'M')
    {
        return false ;
    }

    // XML: "<?xm"
    if (buf[0] == '<' && buf[1] == '?' && buf[2] == 'x' && buf[3] == 'm')
    {
        return false ;
    }

    // HTML: "<!DO" (<!DOCTYPE) or "<htm"
    if (buf[0] == '<' &&
        ((buf[1] == '!' && buf[2] == 'D' && buf[3] == 'O') ||
         (buf[1] == 'h' && buf[2] == 't' && buf[3] == 'm')))
    {
        return false ;
    }

    // Tier 2: less common but still worth catching

    // RIFF container (WAV, AVI): "RIFF"
    if (buf[0] == 'R' && buf[1] == 'I' && buf[2] == 'F' && buf[3] == 'F')
    {
        return false ;
    }

    // gzip: 0x1F 0x8B
    if (buf[0] == 0x1F && buf[1] == 0x8B)
    {
        return false ;
    }

    // RAR archive: "Rar!"
    if (buf[0] == 'R' && buf[1] == 'a' && buf[2] == 'r' && buf[3] == '!')
    {
        return false ;
    }

    // 7-Zip archive: 0x37 0x7A 0xBC 0xAF
    if (buf[0] == 0x37 && buf[1] == 0x7A && buf[2] == 0xBC && buf[3] == 0xAF)
    {
        return false ;
    }

    // Ogg container (audio/video): "OggS"
    if (buf[0] == 'O' && buf[1] == 'g' && buf[2] == 'g' && buf[3] == 'S')
    {
        return false ;
    }

    // MP3 with ID3 tag: "ID3"
    if (buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3')
    {
        return false ;
    }

    // --- WordStar 4.0 and headerless content ---
    size_t softReturns = 0 ;
    size_t sequences = 0 ;
    size_t extChars = 0 ;
    size_t controls = 0 ;
    size_t dotLines = 0 ;
    size_t highBytes = 0 ;
    size_t utf8Chars = 0 ;
    bool utf8Valid = true ;
    int utf8Need = 0 ;                      // continuation bytes still expected
    unsigned char utf8Lead = 0 ;            // lead byte, for the first continuation byte
    size_t end = bytesRead ;
    bool sawEof = false ;

    size_t i = 0 ;
    while (i < bytesRead)
    {
        unsigned char c = buf[i] ;

        if (c == 0x1D)
        {
            // 1D, length (2), type, body, length (2), 1D
            size_t len = (i + 2 < bytesRead) ? (buf[i + 1] | (buf[i + 2] << 8)) : 0 ;
            if (len >= 4 && i + len + 2 < bytesRead &&
                buf[i + len] == buf[i + 1] && buf[i + len + 1] == buf[i + 2] &&
                buf[i + len + 2] == 0x1D)
            {
                sequences++ ;
                if (utf8Need != 0)
                {
                    utf8Valid = false ;
                    utf8Need = 0 ;
                }
                i += len + 3 ;
                continue ;
            }
            return false ;
        }

        if (c == 0x1A)
        {
            sawEof = true ;
            end = i ;
            break ;
        }

        if (c == 0x00)
        {
            return false ;
        }

        if (c == 0x1B && i + 2 < bytesRead && buf[i + 2] == 0x1C)
        {
            extChars++ ;
            if (utf8Need != 0)
            {
                utf8Valid = false ;
                utf8Need = 0 ;
            }
            i += 3 ;
            continue ;
        }

        // WordStar 4 sets the high bit only on the last character of a word
        if (c >= 0xA1)
        {
            unsigned char next = (i + 1 < bytesRead) ? buf[i + 1] : 0x20 ;
            bool wordEnd = next == 0x20 || next == 0x0D || next == 0x0A || next == 0x8D || next == 0xA0 ||
                           next == 0x09 || next == 0x1A || next == 0x1B ||
                           next == STYLE_BOLD || next == STYLE_UNDERLINE || next == STYLE_SUPERSCRIPT ||
                           next == STYLE_SUBSCRIPT || next == STYLE_STRIKETHROUGH || next == STYLE_ITALICS ;
            if (wordEnd == false)
            {
                return false ;
            }
        }

        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0C && c != 0x0D &&
            c != 0x1B && c != 0x1C)
        {
            controls++ ;
        }

        if (c == 0x8D && i + 1 < bytesRead && buf[i + 1] == 0x0A)
        {
            softReturns++ ;
        }

        // dot command: '.' at a line start followed by '.' or a letter and a letter, digit or '#'
        if (c == '.' && (i == 0 || buf[i - 1] == 0x0A) && i + 2 < bytesRead)
        {
            unsigned char second = buf[i + 1] ;
            unsigned char third = buf[i + 2] ;
            if (second == '.' || (isalpha(second) && (isalnum(third) || third == '#')))
            {
                dotLines++ ;
            }
        }

        if (utf8Need != 0)
        {
            if (c >= 0x80 && c <= 0xBF &&
                !(utf8Lead == 0xE0 && c < 0xA0) && !(utf8Lead == 0xED && c > 0x9F) &&
                !(utf8Lead == 0xF0 && c < 0x90) && !(utf8Lead == 0xF4 && c > 0x8F))
            {
                utf8Lead = 0 ;
                utf8Need-- ;
                if (utf8Need == 0)
                {
                    utf8Chars++ ;
                }
                highBytes++ ;
                i++ ;
                continue ;
            }
            utf8Valid = false ;
            utf8Need = 0 ;
        }

        if (c >= 0x80)
        {
            highBytes++ ;
            if (c >= 0xC2 && c <= 0xDF)
            {
                utf8Need = 1 ;
                utf8Lead = c ;
            }
            else if (c >= 0xE0 && c <= 0xEF)
            {
                utf8Need = 2 ;
                utf8Lead = c ;
            }
            else if (c >= 0xF0 && c <= 0xF4)
            {
                utf8Need = 3 ;
                utf8Lead = c ;
            }
            else
            {
                utf8Valid = false ;
            }
        }
        i++ ;
    }

    if (utf8Need != 0)
    {
        utf8Valid = false ;
    }

    // WordStar 7 macro: text, 0x1A padding to a multiple of 128, then 14 00 11
    if (sawEof)
    {
        size_t trailer = end ;
        while (trailer < bytesRead && buf[trailer] == 0x1A)
        {
            trailer++ ;
        }
        if (trailer % 128 == 0 && trailer + 2 < bytesRead &&
            buf[trailer] == 0x14 && buf[trailer + 1] == 0x00 && buf[trailer + 2] == 0x11)
        {
            return false ;
        }
    }

    if (highBytes > 0 && utf8Valid && utf8Chars > 0)
    {
        return false ;
    }

    if (softReturns > 0 || sequences > 0 || extChars > 0)
    {
        return true ;
    }

    return (controls > 0 && dotLines > 0) ;
}


/**
 * @brief Loads a WordStar document file and parses its content
 * 
 * This method opens and reads a WordStar format file, determining the version
 * based on the file header and processing the content character by character.
 * The method supports both WordStar 4.0 and earlier versions (without version
 * header) and WordStar 5.0+ versions (with 0x1D version header).
 * 
 * @param filename The path to the WordStar file to be loaded
 * 
 * @return true if the file was successfully loaded and parsed, false otherwise
 * 
 * @details The loading process includes:
 * - Opening the file in binary mode
 * - Detecting WordStar version by checking for 0x1D header byte
 * - For v5.0+: Reading version information from bytes 2-5
 * - Reading file content byte-by-byte as 7-bit ASCII
 * - Updating progress during loading
 * - Handling special characters and EOF markers
 * - Calling HandleChar() for each character processed
 * 
 * @note The method sets mInIndex to false and handles progress updates
 *       during the loading process. File is automatically closed on completion
 *       or error.
 * 
 * @see cWordstarFile::HandleChar() for character processing.
 * @see cWordstarFile::UpdateProgress() for progress updates.   
 */
bool cWordstarFile::LoadFile(std::string filename)
{
    bool retval = false ;
    mInIndex = false ;
    size_t readresult = 0 ;
    
    mFile = fopen(filename.c_str(), "rb") ; 
    if(mFile)
    {
        // wordstar is an ascii based protocol, do 1 byte at a time
        fseek(mFile, 0, SEEK_END) ;
        long long filesize = ftell(mFile) ;
        fseek(mFile, 0, SEEK_SET) ;

        std::string label ;

        // load a wordstar file from version 4.0 and under
        char inchar ;
        readresult = fread(&inchar, 1, 1, mFile) ;
        if(readresult == 0)
        {
            fclose(mFile) ;
            return false ;
        }

        if(inchar != 0x1D)
        {
            std::string text = string_sprintf("Loading File as WordStar 4.0 or less...") ;
            fseek(mFile, 0, SEEK_SET) ;
        }
        else        // if the file starts with a 0x1D, then it's a file from version 5.0 or greater
        {
            readresult = fread(&inchar, 1, 1, mFile) ;
            readresult = fread(&inchar, 1, 1, mFile) ;
            readresult = fread(&inchar, 1, 1, mFile) ;
            readresult = fread(&inchar, 1, 1, mFile) ;
            if(readresult == 0)
            {
                fclose(mFile) ;
                return false ;
            }
            
            unsigned char ver = static_cast<unsigned char>(inchar) ;
            char low = ver & 0x0F ;
            char high = static_cast<char>((ver & 0xF0) >> 4) ;

            std::string text = string_sprintf("Loading File as Wordstar %d.%d...", high, low) ;

            fseek(mFile, 0, SEEK_SET) ;
        }

        long modulo = filesize / 100 ;
        if(modulo == 0)
        {
            modulo = 1 ;
        }

        // WordStar is a 7 bit format, so that's what we read in here.
        // Sequences read their own bytes, so the end of the file ends the loop, not readloop.
        for(int64_t readloop = 0; ; readloop++)
        {
            int percent =  static_cast<int>(static_cast<double>(readloop) / static_cast<double>(filesize) * 100.0) ;
            if(readloop % modulo == 0)
            {
                UpdateProgress(percent) ;
            }

            readresult = fread(&inchar, 1, 1, mFile) ;
            if(readresult == 0)
            {
                if(ferror(mFile) != 0)
                {
                    fclose(mFile) ;
                    return false ;
                }
                break ;
            }

            if((inchar == static_cast<unsigned char>(STYLE_EOF)) && (mExtendedChar == false))
            {
                UpdateProgress(100) ;
                break ;
            }

            unsigned char c = static_cast<unsigned char>(inchar) ;

            HandleChar(c, static_cast<size_t>(readloop)) ;
        }

        fclose(mFile) ;

        retval = true ;
    }

    return retval ;
}


/**
 * @brief Saves the document content to a WordStar format file.
 * 
 * This function exports the current document to a WordStar 7.0d compatible file format.
 * It writes the appropriate header structures, processes the document content character
 * by character, and handles special formatting elements like tabs, colors, and fonts.
 * The output file is padded to 128-byte boundaries as required by the WordStar format.
 * 
 * @param filename The path and name of the file to save the document to
 * @param length The number of characters/positions to process from the document
 * 
 * @return true if the file was successfully saved, false if there was an error
 *         (such as being unable to open the file for writing)
 * 
 * @note The function currently uses a hardcoded "LASERJET" driver in the header
 * @note Unicode characters are converted to appropriate code pages (CP437, CP737, CP850, CP1252)
 *       using extended character sequences when possible
 * @note Hard returns (character 13) are automatically converted to CR/LF pairs
 * @note The file is padded with EOF markers to align to 128-byte boundaries
 *
 * @todo Complete font type style handling (currently sets all bits to 1)
 *
 * @see cWordstarFile::LoadFile() for loading WordStar files
 * @see cWordstarFile::HandleChar() for character processing during loading
 * @see cWordstarFile::UpdateProgress() for progress updates during file operations 
 */
bool cWordstarFile::SaveFile(std::string filename, POSITION_T length)
{
    return SaveRange(filename, 0, length) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  codepage [in] - code page to use
/// @param  cp       [in] - code point
///
/// @return the byte of cp in codepage, 0 if the code page cannot hold it
///
/////////////////////////////////////////////////////////////////////////////
static unsigned char CodePageByte(eCodePage codepage, char32_t cp)
{
    switch(codepage)
    {
        case CP437 :
        {
            cCodePage437 page ;
            return page.toChar(cp) ;
        }

        case CP737 :
        {
            cCodePage737 page ;
            return page.toChar(cp) ;
        }

        case CP850 :
        {
            cCodePage850 page ;
            return page.toChar(cp) ;
        }

        case CP1252 :
        {
            cCodePageWin1252 page ;
            return page.toChar(cp) ;
        }
    }

    return 0 ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str [in] - one character, UTF-8
///
/// @return the number of code points of a multi-byte character other than
///         NBSP that the current code page cannot hold
///
/////////////////////////////////////////////////////////////////////////////
size_t cWordstarFile::CountUnmappable(const std::string &str)
{
    size_t unmappableCount = 0 ;
    if(str.length() > 1 && str != NOBREAK_SPACE_UTF8)
    {
        std::u32string codepoints ;
        mDocument->GetCodePoints(str, codepoints) ;
        eCodePage codepage = mEditor->GetCodePage() ;

        for(char32_t cp : codepoints)
        {
            if(CodePageByte(codepage, cp) == 0)
            {
                unmappableCount++ ;
            }
        }
    }
    return unmappableCount ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  unmappableCount [in] - characters that will be lost
///
/// @return false if the user declined the warning, true otherwise
///
/// @brief
/// If unmappableCount is not 0 and the file is not silent, asks whether
/// to write anyway.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::AskCodePage(size_t unmappableCount)
{
    // Warn the user if characters will be lost during save. The loss is a
    // property of the *code page* currently selected (Preferences > Code
    // Page), not of the WordStar format itself -- a document with the same
    // characters can round-trip cleanly under a different code page, so the
    // message names the one actually in effect instead of blaming the format.
    if(unmappableCount > 0 && !mSilent)
    {
        eCodePage scanCodePage = mEditor->GetCodePage() ;
        static const char *codePageNames[] = { "CP437", "CP737", "CP850", "CP1252" } ;
        const char *codePageName = "the current code page" ;
        if(static_cast<size_t>(scanCodePage) < sizeof(codePageNames) / sizeof(codePageNames[0]))
        {
            codePageName = codePageNames[scanCodePage] ;
        }

        std::string message = "This document contains " +
            std::to_string(unmappableCount) +
            " character(s) not supported by the " + codePageName +
            " code page currently in use, and they will be lost. Save anyway?" ;

        if(mEditor->AskYesNo("Unicode Warning", message) == false)
        {
            return false ;
        }
    }

    return true ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  start [in] - first position to write
/// @param  end   [in] - position after the last one to write
///
/// @return false if the user declined the warning, true otherwise
///
/// @brief
/// Counts the characters of start..end the current code page cannot
/// write and the raw byte markers EncodeRange() leaves out. If there are
/// any and the file is not silent, asks whether to write anyway.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::ConfirmCodePage(POSITION_T start, POSITION_T end)
{
    size_t unmappableCount = 0 ;
    for(POSITION_T scan = start; scan < end; scan++)
    {
        std::string str = mDocument->GetChar(scan) ;
        if(str.length() == 1 && str[0] == MARKER_CHAR &&
           mDocument->GetControlChar(scan) == STYLE_RAW_BYTE)
        {
            unmappableCount++ ;
            continue ;
        }
        unmappableCount += CountUnmappable(str) ;
    }

    return AskCodePage(unmappableCount) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] - UTF-8 text
///
/// @return false if the user declined the warning, true otherwise
///
/// @brief
/// ConfirmCodePage() for text that EncodeText() writes: counts the code
/// points the current code page cannot write and the bytes 0x1A, 0x1B and
/// 0x1D that EncodeText() leaves out.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::ConfirmCodePage(const std::string &text)
{
    size_t unmappableCount = 0 ;
    std::u32string codepoints ;
    mDocument->GetCodePoints(text, codepoints) ;

    for(char32_t cp : codepoints)
    {
        if(cp == 0x1A || cp == 0x1B || cp == 0x1D)
        {
            unmappableCount++ ;
        }
        else if(cp >= 0x80)
        {
            std::string str ;
            unicode::utf8::encode_codepoint(cp, str) ;
            unmappableCount += CountUnmappable(str) ;
        }
    }

    return AskCodePage(unmappableCount) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] - file to write
/// @param  start    [in] - first position to write
/// @param  end      [in] - position after the last one to write
///
/// @return true if the file was written, false otherwise
///
/// @brief
/// Writes positions start..end as a WordStar 7 file: the header, the
/// sequences EncodeRange() gives, then 0x1A up to the next multiple of 128
/// bytes. The header's style offset is the size of the file.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::SaveRange(std::string filename, POSITION_T start, POSITION_T end)
{
    bool retval = false ;

    if(ConfirmCodePage(start, end) == false)
    {
        return false ;
    }

    sSeqIntro intro ;
    sSeqClose close ;
    sWSHeader header ;
    memset(&header, 0, sizeof(header)) ;

    uint32_t stylecounter = sizeof(sWSHeader) + sizeof(sSeqIntro) + sizeof(sSeqClose) ;
    std::string body = EncodeRange(start, end, stylecounter) ;

    long eofsize = 128 - (stylecounter % 128) ;         // buffer end of file to 128 byte marker
    if(eofsize == 0)
    {
        eofsize = 128 ;
    }
    body.append(static_cast<size_t>(eofsize), static_cast<char>(STYLE_EOF)) ;
    stylecounter += static_cast<uint32_t>(eofsize) ;

    intro.start = 0x1d ;
    intro.size = 125 ;
    intro.type = 0 ;

    close.size = 125 ;
    close.finish = 0x1d ;

    header.version = 0x70 ;
    sprintf(header.driver, "LASERJET") ;
    header.styles = stylecounter ;

    mFile = fopen(filename.c_str(), "wb") ;
    if(mFile)
    {
        fwrite(&intro, sizeof(sSeqIntro), 1, mFile) ;
        fwrite(&header, sizeof(sWSHeader), 1, mFile) ;
        fwrite(&close, sizeof(sSeqClose), 1, mFile) ;
        fwrite(body.data(), 1, body.size(), mFile) ;

        // success only when every write and the final flush succeeded
        retval = (ferror(mFile) == 0) ;
        if(fclose(mFile) != 0)
        {
            retval = false ;
        }
    }
    return retval ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  start        [in]     - first position to encode
/// @param  end          [in]     - position after the last one to encode
/// @param  stylecounter [in,out] - byte count, advanced by what is encoded
///
/// @return the WordStar bytes of positions start..end, without header and
///         without the 0x1A end of file
///
/// @brief
/// Encodes start..end as they are: codes open at start or end are neither
/// added nor closed. Encoding stops at an end of file marker.
///
/////////////////////////////////////////////////////////////////////////////
std::string cWordstarFile::EncodeRange(POSITION_T start, POSITION_T end, uint32_t &stylecounter)
{
    std::string out ;
    sSeqIntro intro ;
    sSeqClose close ;

    intro.start = 0x1d ;
    close.finish = 0x1d ;

    // go through the buffer
    for(POSITION_T loop = start; loop < end; loop++)
    {
        // write out the standard character
        std::string str = mDocument->GetChar(loop) ;
        char ch1 = str[0] ;

        if(str.length() == 1)
        {
            if(ch1 == MARKER_CHAR)
            {
                eModifiers control = mDocument->GetControlChar(loop) ;
                if(control == STYLE_RAW_BYTE)
                {
                    continue ;      // a control byte from a text file is left out
                }
                if(control == STYLE_RAW_SEQUENCE)
                {
                    // a kept sequence is written back as read
                    sRawSequence sequence ;
                    if(mDocument->GetRawSequence(loop, sequence) == true)
                    {
                        intro.size = static_cast<uint16_t>(sequence.body.size() + 4) ;
                        intro.type = static_cast<char>(sequence.type) ;
                        close.size = intro.size ;

                        out.append(reinterpret_cast<const char *>(&intro), sizeof(sSeqIntro)) ;
                        out.append(sequence.body.data(), sequence.body.size()) ;
                        out.append(reinterpret_cast<const char *>(&close), sizeof(sSeqClose)) ;

                        stylecounter += sequence.body.size() + sizeof(sSeqIntro) + sizeof(sSeqClose) ;
                    }
                    continue ;
                }
                ch1 = control ;
            }

            switch(ch1)
            {
                case 13 :
                    // convert hard returns to CR/LF pairs
                    out += static_cast<char>(13) ;
                    out += static_cast<char>(10) ;
                    stylecounter += 2 ;
                    break ;

                case STYLE_TAB :
                    {
                        sWSTab tab ;

                        tab = mDocument->GetTab(loop) ;

                        // a soft tab is written as such only at the start of a paragraph
                        if(tab.type == TAB_SOFT)
                        {
                            POSITION_T start ;
                            POSITION_T end ;
                            mDocument->GetParagraphStartandEnd(mDocument->GetParagraphFromPosition(loop), start, end) ;
                            if(loop != start)
                            {
                                tab.type = TAB_TAB ;
                            }
                        }

                        intro.size = 10 ;
                        intro.type = eSequence::SEQ_TAB ;

                        close.size = 10 ;

                        out.append(reinterpret_cast<const char *>(&intro), sizeof(sSeqIntro)) ;
                        out.append(reinterpret_cast<const char *>(&tab), sizeof(sWSTab)) ;
                        out.append(reinterpret_cast<const char *>(&close), sizeof(sSeqClose)) ;

                        stylecounter += sizeof(sWSTab) + sizeof(sSeqIntro) + sizeof(sSeqClose) ;
                    }
                    break ;

                case STYLE_INTERNAL_COLOR :
                    {
                        sSeqRGBColor rgbColor ;

                        if(mDocument->GetColor(loop, rgbColor) == true)
                        {
                            // Convert RGB to nearest WordStar palette index for file format
                            sWSColor wscolor ;
                            wscolor.prevcolornumber = 0 ;

                            if (rgbColor.IsDefault())
                            {
                                // Default color is black (index 0) in WordStar format
                                wscolor.colornumber = 0 ;
                            }
                            else
                            {
                                // Find nearest palette color by Euclidean distance
                                int bestIndex = 0 ;
                                int bestDistance = INT_MAX ;
                                for (int i = 0; i < WS_PALETTE_SIZE; ++i)
                                {
                                    int dr = rgbColor.red - gBaseWSColors[i].red ;
                                    int dg = rgbColor.green - gBaseWSColors[i].green ;
                                    int db = rgbColor.blue - gBaseWSColors[i].blue ;
                                    int distance = dr * dr + dg * dg + db * db ;
                                    if (distance < bestDistance)
                                    {
                                        bestDistance = distance ;
                                        bestIndex = i ;
                                    }
                                }
                                wscolor.colornumber = static_cast<unsigned char>(bestIndex) ;
                            }

                            intro.size = sizeof(sWSColor) + sizeof(sSeqClose) + 1 ;
                            intro.type = SEQ_COLOR ;

                            close.size = intro.size ;

                            out.append(reinterpret_cast<const char *>(&intro), sizeof(sSeqIntro)) ;
                            out.append(reinterpret_cast<const char *>(&wscolor), sizeof(sWSColor)) ;
                            out.append(reinterpret_cast<const char *>(&close), sizeof(sSeqClose)) ;
                        }
                        stylecounter += sizeof(sWSColor) + sizeof(sSeqIntro) + sizeof(sSeqClose) ;
                    }
                    break ;

                case STYLE_FONT1 :
                    {
                        sInternalFonts font ;

                        sWSFont wsfont ;
                        if(mDocument->GetFont(loop, font) == true)
                        {
                            intro.size = sizeof(sWSFont) + sizeof(sSeqClose) + 1 ;
                            intro.type = eSequence::SEQ_FONT ;

                            close.size = intro.size ;

                            // If font was loaded from a WordStar file, preserve original bits
                            if(font.haveWSFont)
                            {
                                wsfont = font.wsfont ;
                            }
                            else
                            {
                                // Classify font using OS/2 table, PANOSE, glyph metrics,
                                // and keyword fallback. Produces complete typestyle bitfield.
                                cWSFontClassifier classifier ;
                                sWSFontClassification classification =
                                    classifier.Classify(font.name) ;

                                wsfont.style = classification.Assemble() ;
                                wsfont.height = static_cast<uint16_t>(font.size * 20) ;
                            }
                            out.append(reinterpret_cast<const char *>(&intro), sizeof(sSeqIntro)) ;
                            out.append(reinterpret_cast<const char *>(&wsfont), sizeof(sWSFont)) ;
                            out.append(reinterpret_cast<const char *>(&close), sizeof(sSeqClose)) ;

                            stylecounter += sizeof(sWSFont) + sizeof(sSeqIntro) + sizeof(sSeqClose) ;
                        }
                    }
                    break ;

                case STYLE_VARIABLE :
                    {
                        // Write variable as &X& sequence
                        eVariableType varType = mDocument->GetVariable(loop);
                        char varChar = '?';
                        switch (varType)
                        {
                            case VAR_DATE:
                            {
                                varChar = '@';
                                break;
                            }
                            case VAR_TIME:
                            {
                                varChar = '!';
                                break;
                            }
                            case VAR_PAGE_NUMBER:
                            {
                                varChar = '#';
                                break;
                            }
                            case VAR_LINE_NUMBER:
                            {
                                varChar = '_';
                                break;
                            }
                            case VAR_FILENAME:
                            {
                                varChar = '*';
                                break;
                            }
                            case VAR_DRIVE:
                            {
                                varChar = ':';
                                break;
                            }
                            case VAR_DIRECTORY:
                            {
                                varChar = '.';
                                break;
                            }
                            case VAR_FULLPATH:
                            {
                                varChar = '\\';
                                break;
                            }
                            case VAR_WORD_COUNT:
                            {
                                varChar = '?';
                                break;
                            }
                        }
                        out += static_cast<char>('&');
                        out += static_cast<char>(varChar);
                        out += static_cast<char>('&');
                        stylecounter += 3;
                    }
                    break;

                case STYLE_FOOTNOTE :
                case STYLE_ENDNOTE :
                    {
                        // a note loaded from a WordStar file is written back as its sequence;
                        // any other marker of this code is written as the byte itself
                        sNote note ;
                        if(mDocument->GetNote(loop, note) == true && note.raw.empty() == false)
                        {
                            intro.size = static_cast<uint16_t>(note.raw.size() + 4) ;
                            if(ch1 == STYLE_ENDNOTE)
                            {
                                intro.type = eSequence::SEQ_ENDNOTE ;
                            }
                            else if(note.kind == NOTE_KIND_ANNOTATION)
                            {
                                intro.type = eSequence::SEQ_ANNOTATION ;
                            }
                            else if(note.kind == NOTE_KIND_COMMENT)
                            {
                                intro.type = eSequence::SEQ_COMMENT ;
                            }
                            else
                            {
                                intro.type = eSequence::SEQ_FOOTNOTE ;
                            }

                            close.size = intro.size ;

                            out.append(reinterpret_cast<const char *>(&intro), sizeof(sSeqIntro)) ;
                            out.append(note.raw.data(), note.raw.size()) ;
                            out.append(reinterpret_cast<const char *>(&close), sizeof(sSeqClose)) ;

                            stylecounter += note.raw.size() + sizeof(sSeqIntro) + sizeof(sSeqClose) ;
                        }
                        else
                        {
                            out += static_cast<char>(ch1) ;
                            stylecounter++ ;
                        }
                    }
                    break ;

                case STYLE_EOF :
                    loop = end ;  // get out of for loop
                    break ;

                default :
                    out += static_cast<char>(ch1) ;
                    stylecounter++ ;
                    break ;
            }    // switch
        }  // if
        else
        {
            EncodeCharacter(str, out, stylecounter) ;
        }
    }

    return out ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  str          [in]     - one character of more than one byte, UTF-8
/// @param  out          [in,out] - WordStar bytes, the character is appended
/// @param  stylecounter [in,out] - byte count, advanced by what is appended
///
/// @brief
/// NBSP becomes ^O. A first code point in 0x20..0x7E is written as its
/// byte. Every other code point becomes ESC x FS, x being the byte of the
/// current code page; a code point the code page cannot hold is left out.
///
/////////////////////////////////////////////////////////////////////////////
void cWordstarFile::EncodeCharacter(const std::string &str, std::string &out, uint32_t &stylecounter)
{
    char ch1 = str[0] ;

    if(str == NOBREAK_SPACE_UTF8)
    {
        out += static_cast<char>(STYLE_CTRL_O) ;          // binding space
        stylecounter++ ;
    }
    else
    {
        bool printable = (static_cast<unsigned long>(ch1) >= 32 && static_cast<unsigned long>(ch1) <= 126) ;
        std::u32string codepoints ;
        eCodePage codepage = mEditor->GetCodePage() ;

        mDocument->GetCodePoints(str, codepoints) ;

        for(size_t index = 0; index < codepoints.size(); index++)
        {
            char32_t cp = codepoints[index] ;
            unsigned char chws = CodePageByte(codepage, cp) ;
            if(index == 0 && printable)
            {
                out += ch1 ;
                stylecounter++ ;
            }
            else if(chws != 0)
            {
                out += static_cast<char>(STYLE_EXTSTART) ;
                out += static_cast<char>(chws) ;
                out += static_cast<char>(STYLE_EXTEND) ;
                stylecounter += 3 ;
            }
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [in] - UTF-8 text
///
/// @return the WordStar bytes of text
///
/// @brief
/// ASCII stays as it is, except 0x1A, 0x1B and 0x1D, which are left out.
/// Any other code point is encoded as EncodeRange() encodes it
/// (EncodeCharacter()).
///
/////////////////////////////////////////////////////////////////////////////
std::string cWordstarFile::EncodeText(const std::string &text)
{
    std::string out ;
    uint32_t counted = 0 ;
    std::u32string codepoints ;
    mDocument->GetCodePoints(text, codepoints) ;

    for(char32_t cp : codepoints)
    {
        if(cp == 0x1A || cp == 0x1B || cp == 0x1D)
        {
            continue ;
        }

        if(cp < 0x80)
        {
            out += static_cast<char>(cp) ;
            continue ;
        }

        std::string str ;
        unicode::utf8::encode_codepoint(cp, str) ;
        EncodeCharacter(str, out, counted) ;
    }

    return out ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] - file to inspect
///
/// @return true if the content of the file is a WordStar document
///
/// @brief
/// IsWordStarContent() without regard to the extension.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::IsWordStarData(const std::string &filename)
{
    return IsWordStarContent(filename) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  data [in] - bytes of a file
///
/// @return true if data starts with a WordStar 5+ header sequence
///
/////////////////////////////////////////////////////////////////////////////
static bool HasHeaderData(const std::string &data)
{
    const size_t headerSize = sizeof(sSeqIntro) + sizeof(sWSHeader) + sizeof(sSeqClose) ;
    return data.size() >= headerSize && data[0] == 0x1D && data[3] == 0x00 ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] - file to inspect
///
/// @return true if the file starts with a WordStar 5+ header
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::HasHeader(const std::string &filename)
{
    std::string data(sizeof(sSeqIntro) + sizeof(sWSHeader) + sizeof(sSeqClose), '\0') ;
    std::ifstream in(filename, std::ios::binary) ;
    in.read(&data[0], static_cast<std::streamsize>(data.size())) ;
    data.resize(static_cast<size_t>(in.gcount())) ;
    return HasHeaderData(data) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] - WordStar file to change
/// @param  bytes    [in] - bytes to insert
///
/// @return true if the file was written, false otherwise
///
/// @brief
/// A 0x1D sequence has its length repeated before the closing 0x1D. In a
/// file with a WordStar 5+ header, the text starts after the header, and a
/// style offset at or after the text end and before the end of the file is
/// where the style library starts. The text ends at the first 0x1A outside
/// a 0x1D sequence and an ESC x FS extended character, or at the end of
/// the file. The bytes between the text end and the library (or the end
/// of the file) are padding. Writes the text, bytes, 0x1A up to the
/// next multiple of 128 (at least one), then the library unchanged, and
/// sets the style offset to the new library position. Without a library
/// the header is not changed. A file without a header and without 0x1A
/// gets the bytes after the text and no padding. Before the bytes comes
/// LineEndBeforeAppend() of the text with CR LF.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::InsertAtTextEnd(const std::string &filename, const std::string &bytes)
{
    std::string data ;
    {
        std::ifstream in(filename, std::ios::binary) ;
        if (!in.is_open())
        {
            return false ;
        }
        data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()) ;
    }

    const size_t headerSize = sizeof(sSeqIntro) + sizeof(sWSHeader) + sizeof(sSeqClose) ;
    bool header = HasHeaderData(data) ;

    // the text starts after the header, so a 0x1A in the header is not the text end
    size_t textEnd = header ? headerSize : 0 ;
    while (textEnd < data.size())
    {
        unsigned char c = static_cast<unsigned char>(data[textEnd]) ;
        if (c == 0x1B && textEnd + 2 < data.size() && data[textEnd + 2] == 0x1C)
        {
            textEnd += 3 ;
            continue ;
        }
        if (c == 0x1D && textEnd + 2 < data.size())
        {
            size_t len = static_cast<unsigned char>(data[textEnd + 1]) |
                         (static_cast<size_t>(static_cast<unsigned char>(data[textEnd + 2])) << 8) ;
            if (len >= 4 && textEnd + len + 2 < data.size() &&
                data[textEnd + len] == data[textEnd + 1] && data[textEnd + len + 1] == data[textEnd + 2] &&
                static_cast<unsigned char>(data[textEnd + len + 2]) == 0x1D)
            {
                textEnd += len + 3 ;
                continue ;
            }
        }
        if (c == static_cast<unsigned char>(STYLE_EOF))
        {
            break ;
        }
        textEnd++ ;
    }
    textEnd = std::min(textEnd, data.size()) ;

    const size_t stylesAt = sizeof(sSeqIntro) + offsetof(sWSHeader, styles) ;
    size_t library = data.size() ;
    if (header)
    {
        uint32_t styles = 0 ;
        memcpy(&styles, &data[stylesAt], sizeof(styles)) ;
        if (styles >= textEnd && styles < data.size())
        {
            library = styles ;
        }
    }

    // the block goes at the end of the text, before the 0x1A padding and the style library
    size_t textStart = header ? headerSize : 0 ;
    std::string out = data.substr(0, textEnd) +
                      LineEndBeforeAppend(data.substr(textStart, textEnd - textStart), "\r\n") + bytes ;
    if (header || textEnd < data.size())
    {
        out.append(128 - out.size() % 128, static_cast<char>(STYLE_EOF)) ;
    }
    if (library < data.size())
    {
        uint32_t styles = static_cast<uint32_t>(out.size()) ;
        memcpy(&out[stylesAt], &styles, sizeof(styles)) ;
        out.append(data, library, std::string::npos) ;
    }

    return ReplaceFileContent(filename, out) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text    [in] - text a block is appended to
/// @param  lineEnd [in] - line end of the file
///
/// @return lineEnd if text does not end with a line end, else nothing
///
/// @brief
/// A text ends with a line end if its last byte is LF or CR; this covers
/// CR LF, a WordStar soft return 0x8D LF, LF and CR. An empty text needs
/// no line end.
///
/////////////////////////////////////////////////////////////////////////////
std::string cWordstarFile::LineEndBeforeAppend(const std::string &text, const std::string &lineEnd)
{
    if (text.empty() || text.back() == '\n' || text.back() == '\r')
    {
        return std::string() ;
    }
    return lineEnd ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  filename [in] - existing file to replace
/// @param  data     [in] - new content
///
/// @return true if the file now holds data, false otherwise
///
/// @brief
/// Writes data to a temporary file in the folder of the file, with the
/// permission bits, ACL and extended attributes of the file, and renames
/// it over the file. A symbolic link is followed. On failure, including a
/// temporary file that cannot be created, the file is not changed.
///
/////////////////////////////////////////////////////////////////////////////
bool cWordstarFile::ReplaceFileContent(const std::string &filename, const std::string &data)
{
    std::error_code ec ;
    std::filesystem::path target = std::filesystem::canonical(filename, ec) ;
    struct stat info ;
    if (ec || stat(target.c_str(), &info) != 0)
    {
        return false ;
    }

    std::string temp = (target.parent_path() / ("." + target.filename().string() + ".XXXXXX")).string() ;
    int fd = mkstemp(&temp[0]) ;
    if (fd < 0)
    {
        return false ;
    }

    bool ok = (fchmod(fd, info.st_mode & 07777) == 0) ;
    size_t written = 0 ;
    while (ok && written < data.size())
    {
        ssize_t count = write(fd, data.data() + written, data.size() - written) ;
        ok = (count > 0) ;
        written += ok ? static_cast<size_t>(count) : 0 ;
    }
    ok = (close(fd) == 0) && ok ;
    ok = ok && (copyfile(target.c_str(), temp.c_str(), nullptr, COPYFILE_ACL | COPYFILE_XATTR) == 0) ;
    ok = ok && (rename(temp.c_str(), target.c_str()) == 0) ;
    if (ok == false)
    {
        unlink(temp.c_str()) ;
    }
    return ok ;
}


/**
 * @brief Determines if the WordStar file can be loaded.
 * 
 * This method checks whether the current WordStar file is in a state
 * that allows it to be loaded into memory or processed.
 * 
 * @return true if the file can be loaded, false otherwise
 * @note Currently always returns true, indicating all WordStar files
 *       are considered loadable
 * 
 * @see cWordstarFile::CanSave() for saving capabilities.
 * @see cWordstarFile::GetExtensions() for supported file extensions
 */
bool cWordstarFile::CanLoad(void)
{
    return true ;
}


/**
 * @brief Determines if the WordStar file can be saved.
 * 
 * This method checks whether the current WordStar file is in a state
 * that allows it to be saved to disk.
 * 
 * @return true if the file can be saved, false otherwise
 * 
 * @see cWordstarFile::CanLoad() for loading capabilities.
 * @see cWordstarFile::GetExtensions() for supported file extensions
 */
bool cWordstarFile::CanSave(void)
{
    return true ;
}



/**
 * @brief Gets the file extension filter string for WordStar files.
 * 
 * Returns a formatted string containing the file extensions and description
 * for WordStar document files, suitable for use in file dialog filters.
 * Supports both lowercase and uppercase variants of WordStar file extensions
 * from version 3 through 8, as well as the generic .ws extension.
 * 
 * @return std::string File filter string in the format "Description (*.ext1 *.ext2 ...)"
 *         containing all supported WordStar file extensions
 * 
 * @see cWordstarFile::CheckType() for checking file types
 * @see cWordstarFile::LoadFile() for loading WordStar files
 * @see cWordstarFile::SaveFile() for saving WordStar files
 */
std::string cWordstarFile::GetExtensions(void)
{
    return "WordStar Files (*.ws *.ws3 *.ws4 *.ws5 *.ws6 *.ws7 *.ws8 *.WS *.WS3 *.WS4 *.WS5 *.WS6 *.WS7 *.WS8)" ;
}



/**
 * @brief Handles a single character from a WordStar file format during parsing
 * 
 * This method processes individual characters from a WordStar document file, interpreting
 * control codes, formatting commands, and regular text characters. It manages extended
 * character sequences and converts WordStar-specific formatting codes to document formatting
 * commands.
 * 
 * @param c The character to process (unsigned char from 0-255)
 * @param loop The current position/loop counter in the file parsing process
 * 
 * @details The method handles:
 * - Extended character sequences (STYLE_EXTSTART/STYLE_EXTEND pairs)
 * - Line feed and carriage return processing with phantom character handling
 * - Text formatting toggles (bold, italics, underline, superscript, subscript, strikethrough)
 * - Special characters (tabs, non-breaking spaces, form feeds)
 * - Control sequences and unused/reserved style codes
 * - High-bit ASCII character normalization
 * - Index word building when in index mode
 * 
 * @note Uses static variables to maintain state between calls:
 * - last: Previous character processed
 * - position: Position when extended character sequence started
 * 
 * @see InsertExtendedChar(), HandleSequence()
 */
void cWordstarFile::HandleChar(unsigned char c, size_t loop)
{
    bool insert = true ;
    unsigned char& last = mLastChar ;
    size_t& position = mExtStartPosition ;

    if(mExtendedChar == true)            // we've seen a STYLE_EXTSTART
    {
        if((loop - position >= 2) && (c != STYLE_EXTEND))       // if we should have seen a STYLE_EXTEND by now
        {
            mExtendedChar = false ;
            if(last == STYLE_EXTEND)                    // ESC FS without a closing FS
            {
                InsertExtendedChar(mExtStartChar) ;
            }
            else
            {
                size_t npos = position + 1 ;
                HandleChar(last, npos) ;                // treat the char after the STYLE_EXTSTART as a normal one
            }
        }
    }

    // Variable detection state machine: &X& patterns
    // State 0: mInVariable=false, mVariableChar=0 -- looking for opening '&'
    // State 1: mInVariable=true, mVariableChar=0 -- seen opening '&', looking for variable char
    // State 2: mInVariable=true, mVariableChar!=0 -- seen '&X', looking for closing '&'
    if (mInVariable)
    {
        if (mVariableChar == 0)
        {
            // State 1: check if this is a valid variable character
            static const std::string validVarChars = "@!#_*:.\\?";
            if (validVarChars.find(static_cast<char>(c)) != std::string::npos)
            {
                mVariableChar = c;
                last = c;
                return;
            }
            else
            {
                // Not a variable -- flush the buffered '&' and process current char normally
                mInVariable = false;
                mVariableChar = 0;
                mDocument->Insert(static_cast<char>('&'));
                // Fall through to process current character normally
            }
        }
        else
        {
            // State 2: check for closing '&'
            if (c == '&')
            {
                // Complete variable found -- insert it
                eVariableType varType;
                switch (mVariableChar)
                {
                    case '@':
                    {
                        varType = VAR_DATE;
                        break;
                    }
                    case '!':
                    {
                        varType = VAR_TIME;
                        break;
                    }
                    case '#':
                    {
                        varType = VAR_PAGE_NUMBER;
                        break;
                    }
                    case '_':
                    {
                        varType = VAR_LINE_NUMBER;
                        break;
                    }
                    case '*':
                    {
                        varType = VAR_FILENAME;
                        break;
                    }
                    case ':':
                    {
                        varType = VAR_DRIVE;
                        break;
                    }
                    case '.':
                    {
                        varType = VAR_DIRECTORY;
                        break;
                    }
                    case '\\':
                    {
                        varType = VAR_FULLPATH;
                        break;
                    }
                    case '?':
                    {
                        varType = VAR_WORD_COUNT;
                        break;
                    }
                    default:
                    {
                        varType = VAR_DATE;
                        break;
                    }
                }

                mDocument->InsertVariable(varType);
                mInVariable = false;
                mVariableChar = 0;
                last = c;
                return;
            }
            else
            {
                // Not a variable -- flush '&' and variable char, then process current char
                unsigned char savedVarChar = mVariableChar;
                mInVariable = false;
                mVariableChar = 0;
                mDocument->Insert(static_cast<char>('&'));
                mDocument->Insert(static_cast<CHAR_T>(static_cast<unsigned char>(savedVarChar)));
                // Fall through to process current character normally
            }
        }
    }

    // Check for opening '&' to start variable detection
    if (c == '&' && mExtendedChar == false)
    {
        mInVariable = true;
        mVariableChar = 0;
        last = c;
        return;
    }

    // we'll only do this if we are not in an extended char or if we are and are about to exit an extended char;
    // an FS directly after the STYLE_EXTSTART is the character byte 0x1C
    if((mExtendedChar == false) || ((mExtendedChar == true) && (c == STYLE_EXTEND) && (loop - position >= 2)))
    {
        switch(c)
        {
            case 0x8A :                 // phantom soft linefeed
            case STYLE_LINEFEED :
                if(last == 0x8D)        // if the last was a phantom CR, then we need to ignore this LF
                {
                    insert = false ;
                }
                c = 13 ; // HARD_RETURN ;
                break ;

            case 0x8D :             // phantom soft CR
            case 13 :               // CR
                insert = false ;
                break ;

            case 0x82 :
            case STYLE_BOLD :       // ^B - boldface toggle
                mDocument->BeginBold() ;
                insert = false ;
                break ;

            case 0x99 :
            case STYLE_ITALICS :    // ^Y - italics toggle
                mDocument->BeginItalics() ;
                insert = false ;
                break ;

            case 0x93 :
            case STYLE_UNDERLINE :  // ^S - underline toggle
                mDocument->BeginUnderline() ;
                insert = false ;
                break ;

            case 0x94 :
            case STYLE_SUPERSCRIPT :
                mDocument->BeginSuperscript() ;
                insert = false ;
                break ;

            case 0x96 :
            case STYLE_SUBSCRIPT :
                mDocument->BeginSubscript() ;
                insert = false ;
                break ;

            case 0x98 :
            case STYLE_STRIKETHROUGH :
                mDocument->BeginStrikeThrough() ;
                insert = false ;
                break ;

            case STYLE_TAB :
            {
                sWSTab tab ;
                tab.abstabsize = 0 ;
                tab.size = 0 ;
                tab.tabsize = 0 ;
                tab.type = TAB_TAB ;

                mDocument->InsertTab(tab) ;
                insert = false ;
                break ;
            }

            case STYLE_NOBREAK_SPACE :
                insert = true ;
                break ;

            case STYLE_CTRL_O :                 // binding space
                mDocument->Insert(NOBREAK_SPACE) ;
                insert = false ;
                break ;

            case STYLE_SEQUENCE :
                HandleSequence() ;
                insert = false  ;
                break ;

            case STYLE_FONT1 :
            case STYLE_BACKSPACE :
            case 0xA0 :                         // wordstar makes extra spaces for alignment with this
            case STYLE_PHANTOM_SPACE :          // daisy wheel stuff
            case STYLE_PHANTOM_BACKSPACE :      // daisy wheel stuff
            case STYLE_EOF :                    // eof filler
                insert = false ;
                break ;

            case STYLE_INDEX :
                mDocument->BeginIndex() ;
                insert = false ;
                break ;

            case STYLE_FORMFEED :           // not quite right.  Wordstar ejects the page and doesn't print footers
                mDocument->Insert(".pa") ;
                insert = false ;
                break ;

            case 0x9B :
            case STYLE_EXTSTART :
                mExtendedChar = true ;
                position = loop ;
                mExtStartChar = c ;
                insert = false ;
                break ;
                
            case STYLE_EXTEND :
                InsertExtendedChar(last) ;
                mExtendedChar = false ;
                insert = false ;
                break ;

            // the following are not implemented or not used
            case STYLE_NOT_USED1 :
            case STYLE_NOT_USED3 :
            case STYLE_NOT_USED4 :
            case STYLE_NOT_USED7 :
            case STYLE_NOT_USED8 :
            case STYLE_NOT_USED9 :

            case STYLE_RESERVED1 :
                insert = false ;
                break ;
        }

        CHAR_T print ;
        if(c > 128)                 // strip the high bit if it is set, on an ASCII character only
        {
            print = static_cast<CHAR_T>(c - 128) ;
        }
        else
        {
            print = c ;
        }


        if(insert)
        {
            mDocument->Insert(print) ;

            if(mInIndex == true)
            {
                mIndexWord += print ;
            }
        }
    }
    
    last = c ;
}




/**
 * @brief Handles a sequence from the WordStar file format
 * 
 * This method reads and processes a sequence block from the WordStar file. It first reads
 * the sequence size (2 bytes), then the sequence type, and dispatches to the appropriate
 * parsing method based on the sequence type.
 * 
 * A sequence whose body runs past the end of the file is dropped.
 * 
 * @details The method handles the following sequence types:
 * - SEQ_HEADER: Document header information
 * - SEQ_COLOR: Color formatting sequences
 * - SEQ_FONT: Font information
 * - SEQ_NEWFONT: New font definitions
 * - SEQ_TAB: Tab settings
 * - SEQ_FOOTNOTE/SEQ_ENDNOTE/SEQ_ANNOTATION/SEQ_COMMENT: note content
 * - SEQ_PARAGRAPHNUMBER/SEQ_INDEXENTRY: kept as read (cDocument::InsertRawSequence)
 * 
 * Unimplemented sequence types (SEQ_ENDOFPAGE, SEQ_PAGEOFFSET, SEQ_PRINTERCONTROL,
 * SEQ_GRAPHICS, SEQ_PARAGRAPHSTYLE, SEQ_ALTFONT) are skipped entirely.
 * 
 * @note The method always skips 3 trailing bytes at the end of each sequence
 * 
 * @see cWordstarFile::ParseHeader() for header parsing
 * @see cWordstarFile::ParseColor() for color parsing
 * @see cWordstarFile::ParseFont() for font parsing
 * @see cWordstarFile::ParseNewFont() for new font parsing  
 */
void cWordstarFile::HandleSequence(void)
{
    // Caller (HandleChar) has already consumed the STYLE_SEQUENCE start byte;
    // fread the remaining intro bytes (size + type) directly into the packed
    // struct so the little-endian uint16_t size is assembled correctly.
    sSeqIntro intro ;
    intro.start = STYLE_SEQUENCE ;
    if (fread(&intro.size, sizeof(intro) - 1, 1, mFile) != 1)
    {
        return ;
    }

    // Body length is computed as (size - 4) downstream; reject anything that
    // would underflow that subtraction.
    if (intro.size < 4)
    {
        return ;
    }

    eSequence type = static_cast<eSequence>(intro.type) ;
    size_t bodySize = static_cast<size_t>(intro.size - 4) ;

    switch(type)
    {
        case eSequence::SEQ_HEADER :            // header
            ParseHeader() ;
            break ;

        case eSequence::SEQ_COLOR :            // colour sequence
            ParseColor() ;
            break ;

        case eSequence::SEQ_FONT :            // font
            ParseFont() ;
            break ;

        case eSequence::SEQ_NEWFONT :         // WordTsar extension font
            ParseNewFont() ;
            break ;

        /// @todo handle different tab sizes
        case eSequence::SEQ_TAB :             // tabs
            ParseTab() ;
            break ;

        case eSequence::SEQ_FOOTNOTE :
        case eSequence::SEQ_ENDNOTE :
        case eSequence::SEQ_ANNOTATION :
        case eSequence::SEQ_COMMENT :
            {
                std::string str ;
                if(ReadBytes(bodySize, str) == false)
                {
                    return ;
                }

                ParseNote(str, type) ;
            }
            break ;

        case eSequence::SEQ_PARAGRAPHNUMBER :
        case eSequence::SEQ_INDEXENTRY :
            {
                sRawSequence sequence ;
                sequence.type = static_cast<unsigned char>(intro.type) ;
                if(ReadBytes(bodySize, sequence.body) == false)
                {
                    return ;
                }

                mDocument->InsertRawSequence(sequence) ;
            }
            break ;

        // not implemented
        case eSequence::SEQ_ENDOFPAGE :
        case eSequence::SEQ_PAGEOFFSET :
        case eSequence::SEQ_PRINTERCONTROL :
        case eSequence::SEQ_GRAPHICS :
        case eSequence::SEQ_PARAGRAPHSTYLE :
        case eSequence::SEQ_ALTFONT :
        default :
            {
                // we skip what we don't know
                std::string skipped ;
                if(ReadBytes(bodySize, skipped) == false)
                {
                    return ;
                }
            }
            break ;
    }

    // skip trailing 3 bytes
    fgetc(mFile) ;
    fgetc(mFile) ;
    fgetc(mFile) ;
}


/**
 * @brief Reads a number of bytes from the file.
 *
 * @param count Number of bytes to read
 * @param bytes [out] The bytes read (fewer than count at the end of the file)
 *
 * @return true if all count bytes were read, false at the end of the file or on a read error
 */
bool cWordstarFile::ReadBytes(size_t count, std::string &bytes)
{
    bytes.resize(count) ;
    size_t got = fread(bytes.data(), 1, count, mFile) ;
    bytes.resize(got) ;
    return got == count ;
}


/**
 * @brief Inserts an extended character into the document using appropriate code page conversion.
 * 
 * This method converts an extended character from the current editor's code page to UTF-8
 * and inserts it into the document. If the character cannot be converted (returns 0),
 * the original character is inserted as-is.
 * 
 * Supported code pages:
 * - CP437: IBM PC original character set
 * - CP737: Greek character set
 * - CP850: DOS Latin-1 (Western European)
 * - CP1252: Windows Latin-1 (Western European)
 * 
 * @param c The extended character to insert (typically values > 127)
 * 
 * @note If the current code page is not supported or the character conversion fails,
 *       the original character value is inserted without conversion.
 * 
 * @see cCodePage437 for CP437 conversion
 * @see cCodePage737 for CP737 conversion
 * @see cCodePage850 for CP850 conversion
 * @see cCodePageWin1252 for CP1252 conversion
 * @see cWordstarFile::mEditor for accessing the current editor's code page
 */
void cWordstarFile::InsertExtendedChar(unsigned char c)
{
    bool found = false ;
    eCodePage page = mEditor->GetCodePage() ;

    switch(page)
    {
        case CP437 :
        {
            cCodePage437 cp ;
            unsigned long chutf8 = cp.controlGlyph(c) ;     // bytes 1-31 and 127 are glyphs here
            if(chutf8 == UINT32_MAX)
            {
                chutf8 = cp.toUTF8(c) ;
            }
            if(chutf8 != UINT32_MAX)
            {
                mDocument->Insert(chutf8) ;
                found = true ;
            }
        }
        break ;

        case CP737 :
        {
            cCodePage737 cp ;
            unsigned long chutf8 = cp.controlGlyph(c) ;     // bytes 1-31 and 127 are glyphs here
            if(chutf8 == UINT32_MAX)
            {
                chutf8 = cp.toUTF8(c) ;
            }
            if(chutf8 != UINT32_MAX)
            {
                mDocument->Insert(chutf8) ;
                found = true ;
            }
        }
        break ;

        case CP850 :
        {
            cCodePage850 cp ;
            unsigned long chutf8 = cp.controlGlyph(c) ;     // bytes 1-31 and 127 are glyphs here
            if(chutf8 == UINT32_MAX)
            {
                chutf8 = cp.toUTF8(c) ;
            }
            if(chutf8 != UINT32_MAX)
            {
                mDocument->Insert(chutf8) ;
                found = true ;
            }
        }
        break ;

        case CP1252 :
        {
            cCodePageWin1252 cp ;
            unsigned long chutf8 = cp.toUTF8(c) ;
            if(chutf8 != UINT32_MAX)
            {
                mDocument->Insert(chutf8) ;
                found = true ;
            }
        }
        break ;
    }

    if(found == false)
    {
        mDocument->Insert(c) ;
    }
}



/**
 * @brief Parses the WordStar file header and extracts relevant information.
 * 
 * This function reads the WordStar file header structure from the current file position
 * and extracts the style offset information. Most header fields are currently ignored
 * except for the styles offset which is stored in the member variable mStyleOffset.
 * 
 * @note The function performs a debug print of the style offset value.
 * @note If the file read fails (returns 0), the function returns early without processing.
 * 
 * @pre The file (mFile) must be open and positioned at the header location.
 * @post mStyleOffset is updated with the styles value from the header (if read succeeds).
 * 
 * @see sWSHeader for the structure of the WordStar header
 * @see mStyleOffset for storing the styles offset
 */
void cWordstarFile::ParseHeader(void)
{
    sWSHeader header ;
    size_t readresult = fread(&header, sizeof(sWSHeader), 1, mFile) ;
    if(readresult == 0)
    {
        return ;
    }

    // we currently ignore most of the headers
    mStyleOffset = header.styles ;
}


/**
 * @brief Parses a color entry from the WordStar file and inserts it into the document.
 * 
 * This function reads a single color structure (sWSColor) from the current position
 * in the file stream and adds it to the document's color collection. The function
 * performs error checking to ensure the read operation was successful before
 * processing the color data.
 * 
 * @note The function reads from the current file position and advances the file
 *       pointer by sizeof(sWSColor) bytes upon successful read.
 * @note If the read operation fails (returns 0), the function returns early
 *       without modifying the document.
 * 
 * @see sWSColor
 * @see cDocument::InsertColor()
 */
void cWordstarFile::ParseColor(void)
{
    sWSColor color ;
    size_t readresult = fread(&color, sizeof(sWSColor), 1, mFile) ;
    if(readresult == 0)
    {
        return ;
    }

    mDocument->InsertColorFromWSPalette(color) ;
}


/**
 * @brief Parses a WordStar font definition from the file and converts it to internal font representation.
 * 
 * This method reads a WordStar font structure from the current file position and extracts font
 * properties including style flags, font index, and type information. It then maps the WordStar
 * font to an appropriate system font name based on whether it's proportional or monospaced and
 * its generic style category.
 * 
 * The method analyzes the 16-bit style field to determine:
 * - Bit 15: Proportional spacing flag
 * - Bits 11-10: Generic style (0=sans serif, 1=serif, 2=script, 3=display)
 * - Bits 8-0: Font index for system font lookup
 * 
 * Font mapping strategy:
 * - If font index exists in system font table, uses that font name
 * - Otherwise, selects default fonts based on proportional flag and generic style:
 *   - Proportional: Helvetica, Times New Roman, Script, or Default
 *   - Monospaced: Mono, Courier New, Script, or Default
 * 
 * The parsed font is converted to internal format with size calculated from height in twips
 * and inserted into the document's font collection.
 * 
 * @note Returns early if file read fails (EOF or read error)
 * @note Font size is converted from twips to points using TWIPSTOPOINTS constant
 * 
 * @see sWSFont for the WordStar font structure
 * @see sInternalFonts for the internal font representation
 * @see cDocument::InsertFont() for inserting the font into the document
 */
void cWordstarFile::ParseFont(void)
{
    sWSFont font ;
    size_t readresult = fread(&font, sizeof(sWSFont), 1, mFile) ;
    if(readresult == 0)
    {
        return ;
    }

    std::bitset<16> style = font.style ;

    bool proportional = style.test(15) ;

    char genericstyle = static_cast<char>((style.test(11) << 1) + style.test(10)) ;

    size_t fontindex = static_cast<size_t>((style.test(8) << 8) + (style.test(7) << 7) + (style.test(6) << 6) + (style.test(5) << 5) +
                      (style.test(4) << 4) + (style.test(3) << 3) + (style.test(2) << 2) + (style.test(1) << 1) + style.test(0)) ;

    // build new font
    std::string fontname2 ;

    if(fontindex < gSysFontName.size())
    {
        fontname2 = gSysFontName[fontindex] ;
    }
    else
    {
        // Font index exceeds the table, fall back to generic style
        if(proportional == true)
        {
            switch(genericstyle)
            {
                case 0 :        // sans-serif font
                {
                    fontname2 = "Arial" ;
                    break ;
                }

                case 1 :        // serif font
                {
                    fontname2 = "Times New Roman" ;
                    break ;
                }

                case 2 :        // script font
                {
                    fontname2 = "Times New Roman" ;
                    break ;
                }

                case 3 :        // display font
                {
                    fontname2 = "Arial" ;
                    break ;
                }
            }
        }
        else
        {
            switch(genericstyle)
            {
                case 0 :        // sans-serif font
                {
                    fontname2 = "Courier New" ;
                    break ;
                }

                case 1 :        // serif font
                {
                    fontname2 = "Courier New" ;
                    break ;
                }

                case 2 :        // script font
                {
                    fontname2 = "Courier New" ;
                    break ;
                }

                case 3 :        // display font
                {
                    fontname2 = "Courier New" ;
                    break ;
                }
            }
        }
    }

    sInternalFonts sif ;
    sif.fontname = fontname2 ;
    sif.name = fontname2 ;
    sif.haveWSFont = true ;
    sif.wsfont = font ;
    sif.size = font.height / TWIPSTOPOINTS ;
    mDocument->InsertFont(sif) ;

}



/**
 * @brief Parses a new font definition from the WordStar file.
 * 
 * This method reads a font structure from the current file position, processes
 * the font name by removing trailing spaces and replacing '@' characters with
 * spaces, then creates an internal font representation and inserts it into
 * the document.
 * 
 * @details The method performs the following operations:
 * - Reads a sNewFont structure from the file
 * - Null-terminates the font name at position 49
 * - Trims the font name at the first space character
 * - Replaces '@' characters with spaces in the font name
 * - Creates an sInternalFonts structure with the processed data
 * - Inserts the font into the document
 * 
 * @note If the file read operation fails (returns 0), the method returns early
 *       without processing any font data.
 * 
 * @see sNewFont, sInternalFonts, cWordstarFile::mDocument
 */
void cWordstarFile::ParseNewFont(void)
{
    sNewFont font ;
    size_t readresult = fread(&font, sizeof(sNewFont), 1, mFile) ;
    if(readresult == 0)
    {
        return ;
    }

    // unmangle name
    font.fontname[49] = 0 ;
    std::string fontname = font.fontname ;
    fontname = fontname.substr(0, fontname.find(' '));
    std::replace(fontname.begin(), fontname.end(), '@', ' ');
    
    sInternalFonts sif ;
    sif.fontname = fontname ;
    sif.name = fontname ;
    sif.haveWSFont = false ;
    sif.size = font.size ;


    mDocument->InsertFont(sif) ;
}


/**
 * @brief Parses a tab structure from the WordStar file and inserts it into the document.
 * 
 * This function reads a single tab structure (sWSTab) from the current position in the
 * file stream. If the read operation is successful, the tab is inserted into the
 * associated document. If no data can be read (end of file or read error), the
 * function returns early without performing any document modifications.
 * 
 * @note The function assumes the file pointer is positioned at the beginning of a
 *       valid tab structure in the WordStar file format.
 * @note If fread fails to read the complete structure, no tab will be inserted.
 * 
 * @see sWSTab, cWordstarFile::mDocument
 */
void cWordstarFile::ParseTab(void)
{
    sWSTab tab ;
    size_t readresult = fread(&tab, sizeof(sWSTab), 1, mFile) ;
    if(readresult == 0)
    {
        return ;
    }

    // Validate tab type before inserting
    // Only insert tabs with known, supported types
    switch (tab.type)
    {
        case TAB_TAB:
        case TAB_DECIMAL:
        case TAB_CENTER:
        case TAB_RIGHT:
        case TAB_RIGHT1:
            // Valid tab type - insert it
            mDocument->InsertTab(tab) ;
            break;

        case TAB_SOFT:
            {
                // kept only at the start of a paragraph; dropped elsewhere
                POSITION_T pos = mDocument->GetPosition() ;
                POSITION_T start ;
                POSITION_T end ;
                mDocument->GetParagraphStartandEnd(mDocument->GetParagraphFromPosition(pos), start, end) ;
                if(pos == start)
                {
                    mDocument->InsertTab(tab) ;
                }
            }
            break;

        default:
            // Unknown/invalid tab type - skip it
            // This prevents corrupted or malformed WordStar files from
            // inserting invalid tabs that would cause issues in layout/rendering
            break;
    }
}



/**
 * @brief Parses a WordStar note sequence and extracts note information
 * 
 * This function processes a WordStar file sequence containing note data (footnote, endnote,
 * annotation or comment)
 * and extracts the relevant information including note formatting, text content, and metadata.
 * The function handles both simple notes and embedded footnotes with sub-sequences.
 * 
 * @param sequence The raw byte sequence from the WordStar file containing note data
 * @param type The type of note sequence being parsed (footnote, endnote, annotation or comment)
 * 
 * @details The function performs the following operations:
 * - Extracts main sequence information using a union to interpret bytes as note structure
 * - Checks for embedded footnotes (tag == 1) and processes sub-sequence data if present
 * - Extracts the note text content from the remaining sequence
 * - Creates a note object with the parsed symbol format and text
 * - Inserts the note into the document as a footnote (footnotes, annotations and
 *   comments, told apart by sNote::kind) or as an endnote
 * 
 * @note A note shorter than its 4-byte header, an embedded footnote shorter than its
 *       13 bytes of sub-sequence data, or a sub-sequence with no closing STYLE_SEQUENCE
 *       is dropped (nothing is inserted).
 * @note The conversion field in sub-sequence data is currently not implemented (see @todo)
 * @todo Use subdata.note.conversion if set to change note type and insert properly
 * 
 * @see sWSBasenote, sWSFootnote, sNote, cDocument::InsertFootnote, cDocument::InsertEndnote
 */
void cWordstarFile::ParseNote(std::string sequence, eSequence type)
{
    // main sequence information
    union udata
    {
        char byte[4] ;
        sWSBasenote note ;
    };
    udata data ;

    // sub sequence information
    union sdata
    {
        char byte[5] ;
        sWSFootnote note ;
    };
    sdata subdata ;

    // prefill incase we don't have a subsequence
    subdata.note.linecount = 0 ;
    subdata.note.number = 0 ;
    subdata.note.conversion = 0 ;
    subdata.note.format = 3 ;


    // a note shorter than its header is dropped
    if(sequence.size() < 4)
    {
        return ;
    }

    size_t loop ;
    for(loop = 0; loop < 4; loop++)
    {
        data.byte[loop] = sequence[loop] ;
    }

    // see if we have an embedded footnote
    if(data.note.tag == 1)
    {
        // an embedded footnote without its sub sequence data is dropped
        if(sequence.size() < 13)
        {
            return ;
        }

        for(loop = 9; loop < 13; loop++)
        {
            subdata.byte[loop - 9] = sequence[loop] ;
        }

        // get to end of sub sequence; one that never ends is dropped
        loop = sequence.find(static_cast<char>(STYLE_SEQUENCE), loop) ;
        if(loop == std::string::npos)
        {
            return ;
        }
    }

    loop++ ;  // get past the STYLE_SEQUENCE if in sub sequence or unused char if not


    std::string text ;
    for(; loop < sequence.size(); loop++)
    {
        text.push_back(sequence[loop]) ;
    }

    ///< @todo use subdata.note.conversion if set to change note type and insert properly

    sNote note ;
    note.symbol = static_cast<eNoteSymbol>(subdata.note.format) ;
    note.text = text ;
    note.raw = sequence ;
    if(type == eSequence::SEQ_FOOTNOTE)
    {
        note.kind = NOTE_KIND_FOOTNOTE ;
        mDocument->InsertFootnote(note) ;
    }
    else if(type == eSequence::SEQ_ANNOTATION)
    {
        note.kind = NOTE_KIND_ANNOTATION ;
        mDocument->InsertFootnote(note) ;
    }
    else if(type == eSequence::SEQ_COMMENT)
    {
        note.kind = NOTE_KIND_COMMENT ;
        mDocument->InsertFootnote(note) ;
    }
    else if(type == eSequence::SEQ_ENDNOTE)
    {
        note.kind = NOTE_KIND_ENDNOTE ;
        mDocument->InsertEndnote(note) ;
    }
}


void cWordstarFile::InsertFootnote(void)
{
}



/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Build a system font list from the gOrgFonts table.
/// Copies the systemName field from each gOrgFonts entry into
/// gSysFontName, which ParseFont() uses for index-based lookup.
///
/// @see gOrgFonts Global collection of original font descriptions
/// @see gSysFontName Global vector storing system font family names
///
/////////////////////////////////////////////////////////////////////////////
void cWordstarFile::BuildFontList(void)
{
    // Each gOrgFonts entry has a systemName field with the modern
    // cross-platform font name, populated at compile time.
    // The index in gSysFontName matches the font index in the WS7 file.
    for (const auto& fontdesc : gOrgFonts)
    {
        gSysFontName.push_back(fontdesc.systemName) ;
    }
}


/// @}
