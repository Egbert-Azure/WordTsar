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
 * - ParseComment() is a stub (annotations and comments are read but not processed)
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

#include <bitset>
#include <cstring>
#include <cstdint>
#include <numeric>

#include "wordstarfile.h"
#include "fontclassifier.h"
#include "wsfontclassifier.h"
#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp737.h"
#include "src/core/codepage/cp850.h"
#include "src/core/codepage/cp1252.h"
#include "src/core/include/utils.h"


/// @ingroup File
/// @{

extern sSeqRGBColor gBaseWSColors[] ;

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
 * First checks the file extension against known WordStar extensions.
 * If the extension is unknown or missing, falls back to content-based
 * detection by scanning the file for characteristic WordStar byte
 * patterns (WS5+ header or WS4 formatting codes).
 *
 * @param filename The full filename (including path) to check
 * @return true if the file is a WordStar document, false otherwise
 *
 * @note Supported extensions: ws, ws3, ws4, ws5, ws6, ws7, ws8, ws-bak, ws-$$$
 * @note Content detection handles WS5+ (0x1D header) and WS4 (control bytes,
 *       high-bit word wrap markers, extended char sequences)
 *
 * @see IsWordStarContent() for the content-based detection algorithm
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
/// extensions. Checks for both WordStar 5+ and WordStar 4.0 formats.
///
/// WordStar 5+ detection: reads the first 4 bytes and checks for a
/// valid sequence header (0x1D start, SEQ_HEADER type, valid size).
///
/// WordStar 4.0 detection: first rules out known non-WS formats
/// (ZIP/DOCX, RTF, PDF, ELF), then scans the first 512 bytes for
/// characteristic WS4 byte patterns:
///   - Formatting control bytes (Bold=0x02, Underline=0x13, etc.)
///   - Extended character sequences (0x1B ... 0x1C)
///   - Soft line markers (0x8D = phantom CR with high bit)
///   - High-bit-on-printable characters (word wrap markers where
///     byte & 0x7F is a letter)
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

    // Read the first 512 bytes for content analysis
    static const size_t SCAN_SIZE = 512 ;
    unsigned char buf[SCAN_SIZE] ;
    size_t bytesRead = fread(buf, 1, SCAN_SIZE, file) ;
    fclose(file) ;

    if (bytesRead < 4)
    {
        return false ;
    }

    // --- WordStar 5+ detection ---
    // Check for valid sequence header: 0x1D, size[2], type=0x00
    if (buf[0] == 0x1D && buf[3] == 0x00)
    {
        uint16_t size = static_cast<uint16_t>(buf[1]) |
                        (static_cast<uint16_t>(buf[2]) << 8) ;
        if (size >= 4)
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

    // --- WordStar 4.0 detection ---
    // WS4 files are mostly printable ASCII text with formatting control bytes
    // sprinkled in. Binary files may contain the same control byte values by
    // chance, so we first verify the file is predominantly text before checking
    // for WS4 formatting signals.

    // Count text-like bytes: printable ASCII (0x20-0x7E), tab, CR, LF
    size_t textByteCount = 0 ;
    for (size_t i = 0; i < bytesRead; i++)
    {
        unsigned char c = buf[i] ;
        if ((c >= 0x20 && c <= 0x7E) || c == 0x09 || c == 0x0D || c == 0x0A)
        {
            textByteCount++ ;
        }
    }

    // Require at least 75% text-like content to consider WS4 detection
    // Binary files typically have much lower ASCII density
    if (textByteCount * 100 / bytesRead < 75)
    {
        return false ;
    }

    // File is mostly text -- now scan for WS4 formatting signals
    int highBitLetterCount = 0 ;

    for (size_t i = 0; i < bytesRead; i++)
    {
        unsigned char c = buf[i] ;

        // Strong signals: WS formatting control bytes
        // In a mostly-ASCII file, these are definitive WS4 markers
        if (c == 0x02 ||    // STYLE_BOLD
            c == 0x13 ||    // STYLE_UNDERLINE
            c == 0x14 ||    // STYLE_SUPERSCRIPT
            c == 0x16 ||    // STYLE_SUBSCRIPT
            c == 0x18 ||    // STYLE_STRIKETHROUGH
            c == 0x19)      // STYLE_ITALICS
        {
            return true ;
        }

        // Strong signal: extended character sequence (0x1B ... 0x1C)
        if (c == 0x1B && i + 2 < bytesRead && buf[i + 2] == 0x1C)
        {
            return true ;
        }

        // Strong signal: soft line marker (phantom CR with high bit)
        if (c == 0x8D)
        {
            return true ;
        }

        // Supporting signal: high-bit-on-printable letter (word wrap marker)
        // WS4 sets bit 7 on the first letter of words after soft returns
        if (c >= 0x80)
        {
            unsigned char masked = c & 0x7F ;
            if ((masked >= 'A' && masked <= 'Z') ||
                (masked >= 'a' && masked <= 'z'))
            {
                highBitLetterCount++ ;
            }
        }
    }

    // Supporting signal threshold: 3+ high-bit letters suggest WS4 word wrap
    if (highBitLetterCount >= 3)
    {
        return true ;
    }

    return false ;
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
            char high = (ver & 0xF0) >> 4 ;

            std::string text = string_sprintf("Loading File as Wordstar %d.%d...", high, low) ;

            fseek(mFile, 0, SEEK_SET) ;
        }

        long modulo = filesize / 100 ;
        if(modulo == 0)
        {
            modulo = 1 ;
        }

        // WordStar is a 7 bit format, so that's what we read in here.
        for(int64_t readloop = 0; readloop < filesize; readloop++)
        {
            int percent =  static_cast<int>(static_cast<double>(readloop) / static_cast<double>(filesize) * 100.0) ;
            if(readloop % modulo == 0)
            {
                UpdateProgress(percent) ;
            }

            readresult = fread(&inchar, 1, 1, mFile) ;
            if(readresult == 0)
            {
                fclose(mFile) ;
                return false ;
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
    bool retval = false ;

    // Scan for Unicode characters that cannot be saved in WordStar format.
    // Count unmappable graphemes and warn the user before any data is lost.
    size_t unmappableCount = 0 ;
    eCodePage scanCodePage = mEditor->GetCodePage() ;

    for(POSITION_T scan = 0; scan < length; scan++)
    {
        std::string str = mDocument->GetChar(scan) ;
        if(str.length() > 1)
        {
            // Multi-byte UTF-8 grapheme -- check if the code page can represent it
            std::u32string codepoints ;
            size_t cpLen = mDocument->GetCodePoints(str, codepoints) ;

            if(cpLen > 0)
            {
                for(size_t cpIdx = 0; cpIdx < codepoints.length(); cpIdx++)
                {
                    unsigned char mapped = 0 ;

                    switch(scanCodePage)
                    {
                        case CP437 :
                        {
                            cCodePage437 cp ;
                            mapped = cp.toChar(codepoints[cpIdx]) ;
                            break ;
                        }

                        case CP737 :
                        {
                            cCodePage737 cp ;
                            mapped = cp.toChar(codepoints[cpIdx]) ;
                            break ;
                        }

                        case CP850 :
                        {
                            cCodePage850 cp ;
                            mapped = cp.toChar(codepoints[cpIdx]) ;
                            break ;
                        }

                        case CP1252 :
                        {
                            cCodePageWin1252 cp ;
                            mapped = cp.toChar(codepoints[cpIdx]) ;
                            break ;
                        }
                    }

                    if(mapped == 0)
                    {
                        unmappableCount++ ;
                    }
                }
            }
        }
    }

    // Warn the user if characters will be lost during save. The loss is a
    // property of the *code page* currently selected (Preferences > Code
    // Page), not of the WordStar format itself -- a document with the same
    // characters can round-trip cleanly under a different code page, so the
    // message names the one actually in effect instead of blaming the format.
    if(unmappableCount > 0)
    {
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

    mFile = fopen(filename.c_str(), "wb") ;
    if(mFile)
    {
        uint32_t stylecounter = 0 ;

        // write the WordStar 7.0d header
        sSeqIntro intro ;
        sSeqClose close ;

        sWSHeader header ;
        memset(&header, 0, sizeof(header)) ;

        intro.start = 0x1d ;
        intro.size = 125 ;
        intro.type = 0 ;

        close.size = 125 ;
        close.finish = 0x1d ;

        header.version = 0x70 ;
        sprintf(header.driver, "LASERJET") ;
        header.styles = static_cast<unsigned int>(length) ;

        fwrite(&intro, sizeof(sSeqIntro), 1, mFile) ;
        fwrite(&header, sizeof(sWSHeader), 1, mFile) ;
        fwrite(&close, sizeof(sSeqClose), 1, mFile) ;

        stylecounter += sizeof(sWSHeader) + sizeof(sSeqIntro) + sizeof(sSeqClose) ;

        // go through the buffer
        for(POSITION_T loop = 0; loop < length; loop++)
        {
            // write out the standard character
            std::string str = mDocument->GetChar(loop) ;
            char ch1 = str[0] ;

            // skip any characters we replace on-screen
            if(ch1 == REPLACE_CHAR || ch1 == SAVE_CHAR)
            {
                continue ;
            }
            if(str.length() == 1)
            {
                if(ch1 == MARKER_CHAR)
                {
                    ch1 = mDocument->GetControlChar(loop) ;
                }

                switch(ch1)
                {
                    case 13 :
                        // convert hard returns to CR/LF pairs
                        fputc(13, mFile) ;
                        fputc(10, mFile) ;
                        stylecounter += 2 ;
                        break ;

                    case STYLE_TAB :
                        {
                            sWSTab tab ;

                            tab = mDocument->GetTab(loop) ;

                            intro.size = 10 ;
                            intro.type = eSequence::SEQ_TAB ;

                            close.size = 10 ;

                            fwrite(&intro, sizeof(sSeqIntro), 1, mFile) ;
                            fwrite(&tab, sizeof(sWSTab), 1, mFile) ;
                            fwrite(&close, sizeof(sSeqClose), 1, mFile) ;

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
                                    for (int i = 0; i < 16; ++i)
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

                                fwrite(&intro, sizeof(sSeqIntro), 1, mFile) ;
                                fwrite(&wscolor, sizeof(sWSColor), 1, mFile) ;
                                fwrite(&close, sizeof(sSeqClose), 1, mFile) ;
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
                                fwrite(&intro, sizeof(sSeqIntro), 1, mFile) ;
                                fwrite(&wsfont, sizeof(sWSFont), 1, mFile) ;
                                fwrite(&close, sizeof(sSeqClose), 1, mFile) ;

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
                            fputc('&', mFile);
                            fputc(varChar, mFile);
                            fputc('&', mFile);
                            stylecounter += 3;
                        }
                        break;

                    case STYLE_EOF :
                        loop = length ;  // get out of for loop
                        break ;

                    default :
                        fputc(ch1, mFile) ;
                        stylecounter++ ;
                        break ;
                }    // switch
            }  // if
            else            // this is a unicode (UTF-8 char)
            {
                // check if this is a extended char.
                if(static_cast<unsigned long>(ch1) < 32 || static_cast<unsigned long>(ch1) > 126)
                {
                    std::u32string codepoints ;
                    eCodePage codepage = mEditor->GetCodePage() ;

                    size_t len = mDocument->GetCodePoints(str, codepoints) ;

                    if(len > 0)
                    {
                        for(size_t loop = 0; loop < codepoints.length(); loop++)
                        {
                            switch(codepage)
                            {
                                case CP437 :
                                {
                                    cCodePage437 cp ;
                                    unsigned char chws = cp.toChar(codepoints[loop]) ;
                                    if(chws != 0)
                                    {
                                        ch1 = STYLE_EXTSTART ;
                                        fputc(ch1, mFile) ;
                                        fputc(chws, mFile) ;
                                        ch1 = STYLE_EXTEND ;
                                        fputc(ch1, mFile) ;
                                        stylecounter += 3 ;
                                    }
                                    break ;
                                }

                                case CP737 :
                                {
                                    cCodePage737 cp ;
                                    unsigned char chws = cp.toChar(codepoints[loop]) ;
                                    if(chws != 0)
                                    {
                                        ch1 = STYLE_EXTSTART ;
                                        fputc(ch1, mFile) ;
                                        fputc(chws, mFile) ;
                                        ch1 = STYLE_EXTEND ;
                                        fputc(ch1, mFile) ;
                                        stylecounter += 3 ;
                                    }
                                    break ;
                                }

                                case CP850 :
                                {
                                    cCodePage850 cp ;
                                    unsigned char chws = cp.toChar(codepoints[loop]) ;
                                    if(chws != 0)
                                    {
                                        ch1 = STYLE_EXTSTART ;
                                        fputc(ch1, mFile) ;
                                        fputc(chws, mFile) ;
                                        ch1 = STYLE_EXTEND ;
                                        fputc(ch1, mFile) ;
                                        stylecounter += 3 ;
                                    }
                                    break ;
                                }

                                case CP1252 :
                                {
                                    cCodePageWin1252 cp ;
                                    unsigned char chws = cp.toChar(codepoints[loop]) ;
                                    if(chws != 0)
                                    {
                                        ch1 = STYLE_EXTSTART ;
                                        fputc(ch1, mFile) ;
                                        fputc(chws, mFile) ;
                                        ch1 = STYLE_EXTEND ;
                                        fputc(ch1, mFile) ;
                                        stylecounter += 3 ;
                                    }
                                    break ;
                                }
                            }
                        }
                    }
                }
            }
        }

        long eofsize = 128 - (stylecounter % 128) ;         // buffer end of file to 128 byte marker
        if(eofsize == 0)
        {
            eofsize = 128 ;
        }
        char ch = STYLE_EOF ;
        for(long eofloop = 0; eofloop < eofsize; eofloop++)
        {
            fputc(ch, mFile) ;
            stylecounter++ ;
        }

        // rewrite header now that we know style table offset
        fseek(mFile, 0, SEEK_SET) ;

        intro.start = 0x1d ;
        intro.size = 125 ;
        intro.type = 0 ;

        close.size = 125 ;
        close.finish = 0x1d ;

        header.version = 0x70 ;
        sprintf(header.driver, "LASERJET") ;
        header.styles = stylecounter ;

        fwrite(&intro, sizeof(sSeqIntro), 1, mFile) ;
        fwrite(&header, sizeof(sWSHeader), 1, mFile) ;
        fwrite(&close, sizeof(sSeqClose), 1, mFile) ;

        retval = true ;
        fclose(mFile) ;
    }
    return retval ;
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
    static unsigned char last = 0 ;
    static size_t position = 0 ;

    if(mExtendedChar == true)            // we've seen a STYLE_EXTSTART
    {
        if((loop - position >= 2) && (c != STYLE_EXTEND))       // if we should have seen a STYLE_EXTEND by now
        {
            mExtendedChar = false ;
            size_t npos = position + 1 ;
            HandleChar(last, npos) ;                    // treat the char after the STYLE_EXTSTART as a normal one
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
                mDocument->Insert(static_cast<char>(savedVarChar));
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

    // we'll only do this if we are not in an extended char or if we are and are about to exit an extended char
    if((mExtendedChar == false) || ((mExtendedChar == true) && (c == STYLE_EXTEND)))
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
                sWSTab tab ;
                tab.abstabsize = 0 ;
                tab.size = 0 ;
                tab.tabsize = 0 ;
                tab.type = TAB_TAB ;

                mDocument->InsertTab(tab) ;
                insert = false ;
                break ;

            case STYLE_NOBREAK_SPACE :
                insert = true ;
                break ;

            case STYLE_SEQUENCE :
                HandleSequence(loop) ;
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
            case STYLE_CTRL_O :
            case STYLE_NOT_USED7 :
            case STYLE_NOT_USED8 :
            case STYLE_NOT_USED9 :

            case STYLE_RESERVED1 :
                insert = false ;
                break ;
        }

        char print ;
        if(c > 128)                 // strip the high bit if it is set, on an ASCII character only
        {
            print = c - 128 ;
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
 * @param loop Reference to the current loop counter, updated when skipping unknown sequences
 * 
 * @details The method handles the following sequence types:
 * - SEQ_HEADER: Document header information
 * - SEQ_COLOR: Color formatting sequences
 * - SEQ_FONT: Font information
 * - SEQ_NEWFONT: New font definitions
 * - SEQ_TAB: Tab settings
 * - SEQ_FOOTNOTE/SEQ_ENDNOTE: Footnote and endnote content
 * - SEQ_ANNOTATION/SEQ_COMMENT: Comment and annotation content
 * 
 * Unimplemented sequence types (SEQ_ENDOFPAGE, SEQ_PAGEOFFSET, SEQ_PARAGRAPHNUMBER,
 * SEQ_INDEXENTRY, SEQ_PRINTERCONTROL, SEQ_GRAPHICS, SEQ_PARAGRAPHSTYLE, SEQ_ALTFONT)
 * are skipped entirely.
 * 
 * @note The method always skips 3 trailing bytes at the end of each sequence
 * @note For unknown sequences, the loop counter is incremented by (size - 1)
 * 
 * @see cWordstarFile::ParseHeader() for header parsing
 * @see cWordstarFile::ParseColor() for color parsing
 * @see cWordstarFile::ParseFont() for font parsing
 * @see cWordstarFile::ParseNewFont() for new font parsing  
 */
void cWordstarFile::HandleSequence(size_t &loop)
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
    long size = intro.size ;

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
            {
                char inchar ;
                std::string str ;

                for(long loop = 0; loop < size - 4; loop++)
                {
                    inchar = fgetc(mFile) ;
                    str.push_back(inchar) ;
                }

                ParseNote(str, type) ;
            }
            break ;

        case eSequence::SEQ_ANNOTATION :
        case eSequence::SEQ_COMMENT :
            {
                char inchar ;
                std::string str ;

                for(long loop = 0; loop < size - 4; loop++)
                {
                    inchar = fgetc(mFile) ;
                    str.push_back(inchar) ;
                }

                ParseComment(str, type) ;
            }
            break ;

        // not implemented
        case eSequence::SEQ_ENDOFPAGE :
        case eSequence::SEQ_PAGEOFFSET :
        case eSequence::SEQ_PARAGRAPHNUMBER :
        case eSequence::SEQ_INDEXENTRY :
        case eSequence::SEQ_PRINTERCONTROL :
        case eSequence::SEQ_GRAPHICS :
        case eSequence::SEQ_PARAGRAPHSTYLE :
        case eSequence::SEQ_ALTFONT :
        default :
            // we skip what we don't know (size - 1 == last byte of sequence)
            for(long skip = 0; skip < size - 4; skip++)
            {
                fgetc(mFile) ;
            }
            loop += size - 1 ;
            break ;
    }

    // skip trailing 3 bytes
    fgetc(mFile) ;
    fgetc(mFile) ;
    fgetc(mFile) ;
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
            unsigned long chutf8 = cp.toUTF8(c) ;
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
            unsigned long chutf8 = cp.toUTF8(c) ;
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
            unsigned long chutf8 = cp.toUTF8(c) ;
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

    size_t fontindex = (style.test(8) << 8) + (style.test(7) << 7) + (style.test(6) << 6) + (style.test(5) << 5) +
                      (style.test(4) << 4) + (style.test(3) << 3) + (style.test(2) << 2) + (style.test(1) << 1) + style.test(0) ;

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
        // case TAB_SOFT:
            // Valid tab type - insert it
            mDocument->InsertTab(tab) ;
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
 * This function processes a WordStar file sequence containing note data (footnote or endnote)
 * and extracts the relevant information including note formatting, text content, and metadata.
 * The function handles both simple notes and embedded footnotes with sub-sequences.
 * 
 * @param sequence The raw byte sequence from the WordStar file containing note data
 * @param type The type of note sequence being parsed (footnote or endnote)
 * 
 * @details The function performs the following operations:
 * - Extracts main sequence information using a union to interpret bytes as note structure
 * - Checks for embedded footnotes (tag == 1) and processes sub-sequence data if present
 * - Extracts the note text content from the remaining sequence
 * - Creates a note object with the parsed symbol format and text
 * - Inserts the note into the document as either a footnote or endnote
 * 
 * @note The conversion field in sub-sequence data is currently not implemented (see @todo)
 * @todo Use subdata.note.conversion if set to change note type and insert properly
 * 
 * @see sWSBasenote, sWSFootnote, sNote, cDocument::InsertFootnote, cWordstarFile::InsertEndnote
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


    size_t loop ;
    for(loop = 0; loop < 4; loop++)
    {
        data.byte[loop] = sequence[loop] ;
    }

    // see if we have an embedded footnote
    if(data.note.tag == 1)
    {
        for(loop = 9; loop < 13; loop++)
        {
            subdata.byte[loop - 9] = sequence[loop] ;
        }

        // get to end of sub sequence
        while(sequence[loop] != STYLE_SEQUENCE)
        {
            loop++ ;
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
    if(type == eSequence::SEQ_FOOTNOTE)
    {
        mDocument->InsertFootnote(note) ;
    }
    else if(type == eSequence::SEQ_ENDNOTE)
    {
        InsertEndnote() ;
    }
}


void cWordstarFile::ParseComment(std::string sequence, eSequence type)
{
    UNUSED_ARGUMENT(sequence) ;
    UNUSED_ARGUMENT(type) ;
}


void cWordstarFile::InsertFootnote(void)
{
}


void cWordstarFile::InsertEndnote(void)
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
