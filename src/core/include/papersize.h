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

#ifndef PAPERSIZE_H
#define PAPERSIZE_H

#include <string>

/////////////////////////////////////////////////////////////////////////////
///
/// @enum ePaperSize
///
/// @brief
/// Paper size identifiers.
/// Mirrors Qt's QPageSize::PageSizeId values for paper type lookup.
/// Used with PaperDefinitions[] to get paper dimensions in twips.
///
/////////////////////////////////////////////////////////////////////////////
enum ePaperSize
{
    PaperLetter = 0,
    PaperLegal = 1,
    PaperExecutive_7_5x10in = 2,
    PaperA0 = 3,
    PaperA1 = 4,
    PaperA2 = 5,
    PaperA3 = 6,
    PaperA4 = 7,
    PaperA5 = 8,
    PaperA6 = 9,
    PaperA7 = 10,
    PaperA8 = 11,
    PaperA9 = 12,
    PaperA10 = 13,
    PaperISOB0 = 14,
    PaperISOB1 = 15,
    PaperISOB2 = 16,
    PaperISOB3 = 17,
    PaperISOB4 = 18,
    PaperISOB5 = 19,
    PaperISOB6 = 20,
    PaperISOB7 = 21,
    PaperISOB8 = 22,
    PaperISOB9 = 23,
    PaperISOB10 = 24,
    PaperEnvC5 = 25,
    PaperEnv10 = 26,
    PaperEnvDL = 27,
    PaperFolio = 28,
    PaperLedger = 29,
    PaperTabloid = 30,
    PaperA3Extra = 32,
    PaperA4Extra = 33,
    PaperA4Plus = 34,
    PaperA4Small = 35,
    PaperA5Extra = 36,
    PaperISOB5Extra = 37,
    PaperB0 = 38,
    PaperB1 = 39,
    PaperB2 = 40,
    PaperB3 = 41,
    PaperB4 = 42,
    PaperB5 = 43,
    PaperB6 = 44,
    PaperB7 = 45,
    PaperB8 = 46,
    PaperB9 = 47,
    PaperB10 = 48,
    PaperAnsiC = 49,
    PaperAnsiD = 50,
    PaperAnsiE = 51,
    PaperLegalExtra = 52,
    PaperLetterExtra = 53,
    PaperLetterPlus = 54,
    PaperLetterSmall = 55,
    PaperTabloidExtra = 56,
    PaperARCHA = 57,
    PaperARCHB = 58,
    PaperARCHC = 59,
    PaperARCHD = 60,
    PaperARCHE = 61,
    Paper7x9 = 62,
    Paper8x10 = 63,
    Paper9x11 = 64,
    Paper9x12 = 65,
    Paper10x11 = 66,
    Paper10x13 = 67,
    Paper10x14 = 68,
    Paper12x11 = 69,
    Paper15x11 = 70,
    PaperExecutive = 71,
    PaperNote = 72,
    PaperQuarto = 73,
    PaperStatement = 74,
    PaperSuperA = 75,
    PaperSuperB = 76,
    PaperPostcard = 77,
    PaperDoublePostcard = 78,
    PaperPRC16K = 79,
    PaperPRC32K = 80,
    PaperPRC32KBig = 81,
    PaperFanFoldUS = 82,
    PaperFanFoldGerman = 83,
    PaperFanFoldGermanLegal = 84,
    PaperEnvISOB4 = 85,
    PaperEnvISOB5 = 86,
    PaperEnvISOB6 = 87,
    PaperEnvC0 = 88,
    PaperEnvC1 = 89,
    PaperEnvC2 = 90,
    PaperEnvC3 = 91,
    PaperEnvC4 = 92,
    PaperEnvC6 = 93,
    PaperEnvC65 = 94,
    PaperEnvC7 = 95,
    PaperEnv9 = 96,
    PaperEnv11 = 97,
    PaperEnv12 = 98,
    PaperEnv14 = 99,
    PaperEnvMonarch = 100,
    PaperEnvPersonal = 101,
    PaperEnvChou3 = 102,
    PaperEnvChou4 = 103,
    PaperEnvInvite = 104,
    PaperEnvItalian = 105,
    PaperEnvKaku2 = 106,
    PaperEnvKaku3 = 107,
    PaperEnvPRC1 = 108,
    PaperEnvPRC2 = 109,
    PaperEnvPRC3 = 110,
    PaperEnvPRC4 = 111,
    PaperEnvPRC5 = 112,
    PaperEnvPRC6 = 113,
    PaperEnvPRC7 = 114,
    PaperEnvPRC8 = 115,
    PaperEnvPRC9 = 116,
    PaperEnvPRC10 = 117,
    PaperEnvYou4 = 118,
};


/////////////////////////////////////////////////////////////////////////////
///
/// @struct sPaperDefs
///
/// @brief
/// Paper size definition entry.
/// Maps a paper size enum to its name and dimensions in twips.
/// Used by the PaperDefinitions lookup table.
///
/////////////////////////////////////////////////////////////////////////////
struct sPaperDefs
{
    ePaperSize papersize ;                           ///< paper size index
    std::string papername ;                          ///< The textual page description
    unsigned long paperwidth ;                       ///< the page width in twips
    unsigned long paperheight ;                      ///< the page height in twips
};


const sPaperDefs PaperDefinitions[]
    {
        {
            static_cast<ePaperSize>(0),
            "Letter / ANSI A",
            12240,
            15840,
        },
        {
            static_cast<ePaperSize>(1),
            "Legal",
            12240,
            20160,
        },
        {
            static_cast<ePaperSize>(2),
            "Executive (7.5 x 10 in)",
            10800,
            14400,
        },
        {
            static_cast<ePaperSize>(3),
            "A0",
            47678,
            67407,
        },
        {
            static_cast<ePaperSize>(4),
            "A1",
            33675,
            47678,
        },
        {
            static_cast<ePaperSize>(5),
            "A2",
            23811,
            33675,
        },
        {
            static_cast<ePaperSize>(6),
            "A3",
            16837,
            23811,
        },
        {
            static_cast<ePaperSize>(7),
            "A4",
            11905,
            16837,
        },
        {
            static_cast<ePaperSize>(8),
            "A5",
            8390,
            11905,
        },
        {
            static_cast<ePaperSize>(9),
            "A6",
            5952,
            8390,
        },
        {
            static_cast<ePaperSize>(10),
            "A7",
            4195,
            5952,
        },
        {
            static_cast<ePaperSize>(11),
            "A8",
            2948,
            4195,
        },
        {
            static_cast<ePaperSize>(12),
            "A9",
            2097,
            2948,
        },
        {
            static_cast<ePaperSize>(13),
            "A10",
            1474,
            2097,
        },
        {
            static_cast<ePaperSize>(14),
            "B0",
            56692,
            80163,
        },
        {
            static_cast<ePaperSize>(15),
            "B1",
            40081,
            56692,
        },
        {
            static_cast<ePaperSize>(16),
            "B2",
            28346,
            40081,
        },
        {
            static_cast<ePaperSize>(17),
            "B3",
            20012,
            28346,
        },
        {
            static_cast<ePaperSize>(18),
            "B4",
            14173,
            20012,
        },
        {
            static_cast<ePaperSize>(19),
            "B5",
            9977,
            14173,
        },
        {
            static_cast<ePaperSize>(20),
            "B6",
            7086,
            9977,
        },
        {
            static_cast<ePaperSize>(21),
            "B7",
            4988,
            7086,
        },
        {
            static_cast<ePaperSize>(22),
            "B8",
            3514,
            4988,
        },
        {
            static_cast<ePaperSize>(23),
            "B9",
            2494,
            3514,
        },
        {
            static_cast<ePaperSize>(24),
            "B10",
            1757,
            2494,
        },
        {
            static_cast<ePaperSize>(25),
            "Envelope C5",
            9184,
            12982,
        },
        {
            static_cast<ePaperSize>(26),
            "Envelope US 10",
            5941,
            13680,
        },
        {
            static_cast<ePaperSize>(27),
            "Envelope DL",
            6236,
            12472,
        },
        {
            static_cast<ePaperSize>(28),
            "Folio (8.27 x 13 in)",
            11905,
            18708,
        },
        {
            static_cast<ePaperSize>(29),
            "Ledger / ANSI B",
            24480,
            15840,
        },
        {
            static_cast<ePaperSize>(30),
            "Tabloid / ANSI B",
            15840,
            24480,
        },
        {
            static_cast<ePaperSize>(32),
            "A3 Extra",
            18255,
            25228,
        },
        {
            static_cast<ePaperSize>(33),
            "A4 Extra",
            13351,
            18272,
        },
        {
            static_cast<ePaperSize>(34),
            "A4 Plus",
            11905,
            18708,
        },
        {
            static_cast<ePaperSize>(35),
            "A4 Small",
            11905,
            16837,
        },
        {
            static_cast<ePaperSize>(36),
            "A5 Extra",
            9864,
            13322,
        },
        {
            static_cast<ePaperSize>(37),
            "B5 Extra",
            11395,
            15647,
        },
        {
            static_cast<ePaperSize>(38),
            "JIS B0",
            58393,
            82544,
        },
        {
            static_cast<ePaperSize>(39),
            "JIS B1",
            41272,
            58393,
        },
        {
            static_cast<ePaperSize>(40),
            "JIS B2",
            29196,
            41272,
        },
        {
            static_cast<ePaperSize>(41),
            "JIS B3",
            20636,
            29196,
        },
        {
            static_cast<ePaperSize>(42),
            "JIS B4",
            14570,
            20636,
        },
        {
            static_cast<ePaperSize>(43),
            "JIS B5",
            10318,
            14570,
        },
        {
            static_cast<ePaperSize>(44),
            "JIS B6",
            7256,
            10318,
        },
        {
            static_cast<ePaperSize>(45),
            "JIS B7",
            5159,
            7256,
        },
        {
            static_cast<ePaperSize>(46),
            "JIS B8",
            3628,
            5159,
        },
        {
            static_cast<ePaperSize>(47),
            "JIS B9",
            2551,
            3628,
        },
        {
            static_cast<ePaperSize>(48),
            "JIS B10",
            1814,
            2551,
        },
        {
            static_cast<ePaperSize>(49),
            "ANSI C",
            24480,
            31680,
        },
        {
            static_cast<ePaperSize>(50),
            "ANSI D",
            31680,
            48960,
        },
        {
            static_cast<ePaperSize>(51),
            "ANSI E",
            48960,
            63382,
        },
        {
            static_cast<ePaperSize>(52),
            "Legal Extra",
            13680,
            21600,
        },
        {
            static_cast<ePaperSize>(53),
            "Letter Extra",
            13680,
            17280,
        },
        {
            static_cast<ePaperSize>(54),
            "Letter Plus",
            12240,
            18272,
        },
        {
            static_cast<ePaperSize>(55),
            "Letter Small",
            12240,
            15840,
        },
        {
            static_cast<ePaperSize>(56),
            "Tabloid Extra",
            17280,
            25920,
        },
        {
            static_cast<ePaperSize>(57),
            "Architect A",
            12960,
            17280,
        },
        {
            static_cast<ePaperSize>(58),
            "Architect B",
            17280,
            25920,
        },
        {
            static_cast<ePaperSize>(59),
            "Architect C",
            25920,
            34560,
        },
        {
            static_cast<ePaperSize>(60),
            "Architect D",
            34560,
            51840,
        },
        {
            static_cast<ePaperSize>(61),
            "Architect E",
            51840,
            69108,
        },
        {
            static_cast<ePaperSize>(62),
            "7 x 9 in",
            10080,
            12960,
        },
        {
            static_cast<ePaperSize>(63),
            "8 x 10 in",
            11520,
            14400,
        },
        {
            static_cast<ePaperSize>(64),
            "9 x 11 in",
            12960,
            15840,
        },
        {
            static_cast<ePaperSize>(65),
            "9 x 12 in",
            12960,
            17280,
        },
        {
            static_cast<ePaperSize>(66),
            "10 x 11 in",
            14400,
            15840,
        },
        {
            static_cast<ePaperSize>(67),
            "10 x 13 in",
            14400,
            18720,
        },
        {
            static_cast<ePaperSize>(68),
            "10 x 14 in",
            14400,
            20160,
        },
        {
            static_cast<ePaperSize>(69),
            "12 x 11 in",
            17280,
            15840,
        },
        {
            static_cast<ePaperSize>(70),
            "15 x 11 in",
            21600,
            15840,
        },
        {
            static_cast<ePaperSize>(71),
            "Executive (7.25 x 10.5 in)",
            10442,
            15120,
        },
        {
            static_cast<ePaperSize>(72),
            "Note",
            12240,
            15840,
        },
        {
            static_cast<ePaperSize>(73),
            "Quarto",
            12240,
            15596,
        },
        {
            static_cast<ePaperSize>(74),
            "Statement",
            7920,
            12240,
        },
        {
            static_cast<ePaperSize>(75),
            "Super A",
            12869,
            20182,
        },
        {
            static_cast<ePaperSize>(76),
            "Super B",
            17291,
            27609,
        },
        {
            static_cast<ePaperSize>(77),
            "Postcard",
            5669,
            8390,
        },
        {
            static_cast<ePaperSize>(78),
            "Double Postcard",
            11338,
            8390,
        },
        {
            static_cast<ePaperSize>(79),
            "PRC 16K",
            8277,
            12188,
        },
        {
            static_cast<ePaperSize>(80),
            "PRC 32K",
            5499,
            8560,
        },
        {
            static_cast<ePaperSize>(81),
            "PRC 32K Big",
            5499,
            8560,
        },
        {
            static_cast<ePaperSize>(82),
            "Fan-fold US (14.875 x 11 in)",
            21418,
            15840,
        },
        {
            static_cast<ePaperSize>(83),
            "Fan-fold German (8.5 x 12 in)",
            12240,
            17280,
        },
        {
            static_cast<ePaperSize>(84),
            "Fan-fold German Legal (8.5 x 13 in)",
            12240,
            18708,
        },
        {
            static_cast<ePaperSize>(85),
            "Envelope B4",
            14173,
            20012,
        },
        {
            static_cast<ePaperSize>(86),
            "Envelope B5",
            9977,
            14173,
        },
        {
            static_cast<ePaperSize>(87),
            "Envelope B6",
            9977,
            7086,
        },
        {
            static_cast<ePaperSize>(88),
            "Envelope C0",
            51987,
            73530,
        },
        {
            static_cast<ePaperSize>(89),
            "Envelope C1",
            36737,
            51987,
        },
        {
            static_cast<ePaperSize>(90),
            "Envelope C2",
            25965,
            36737,
        },
        {
            static_cast<ePaperSize>(91),
            "Envelope C3",
            18368,
            25965,
        },
        {
            static_cast<ePaperSize>(92),
            "Envelope C4",
            12982,
            18368,
        },
        {
            static_cast<ePaperSize>(93),
            "Envelope C6",
            6462,
            9184,
        },
        {
            static_cast<ePaperSize>(94),
            "Envelope C65",
            6462,
            12982,
        },
        {
            static_cast<ePaperSize>(95),
            "Envelope C7",
            4592,
            6462,
        },
        {
            static_cast<ePaperSize>(96),
            "Envelope US 9",
            5578,
            12778,
        },
        {
            static_cast<ePaperSize>(97),
            "Envelope US 11",
            6480,
            14938,
        },
        {
            static_cast<ePaperSize>(98),
            "Envelope US 12",
            6842,
            15840,
        },
        {
            static_cast<ePaperSize>(99),
            "Envelope US 14",
            7200,
            16560,
        },
        {
            static_cast<ePaperSize>(100),
            "Envelope Monarch",
            5580,
            10800,
        },
        {
            static_cast<ePaperSize>(101),
            "Envelope Personal",
            5220,
            9360,
        },
        {
            static_cast<ePaperSize>(102),
            "Envelope Chou 3",
            6803,
            13322,
        },
        {
            static_cast<ePaperSize>(103),
            "Envelope Chou 4",
            5102,
            11622,
        },
        {
            static_cast<ePaperSize>(104),
            "Envelope Invite",
            12472,
            12472,
        },
        {
            static_cast<ePaperSize>(105),
            "Envelope Italian",
            6236,
            13039,
        },
        {
            static_cast<ePaperSize>(106),
            "Envelope Kaku 2",
            13606,
            18822,
        },
        {
            static_cast<ePaperSize>(107),
            "Envelope Kaku 3",
            12245,
            15703,
        },
        {
            static_cast<ePaperSize>(108),
            "Envelope PRC 1",
            5782,
            9354,
        },
        {
            static_cast<ePaperSize>(109),
            "Envelope PRC 2",
            5782,
            9977,
        },
        {
            static_cast<ePaperSize>(110),
            "Envelope PRC 3",
            7086,
            9977,
        },
        {
            static_cast<ePaperSize>(111),
            "Envelope PRC 4",
            6236,
            11792,
        },
        {
            static_cast<ePaperSize>(112),
            "Envelope PRC 5",
            6236,
            12472,
        },
        {
            static_cast<ePaperSize>(113),
            "Envelope PRC 6",
            6803,
            13039,
        },
        {
            static_cast<ePaperSize>(114),
            "Envelope PRC 7",
            9070,
            13039,
        },
        {
            static_cast<ePaperSize>(115),
            "Envelope PRC 8",
            6803,
            17518,
        },
        {
            static_cast<ePaperSize>(116),
            "Envelope PRC 9",
            12982,
            18368,
        },
        {
            static_cast<ePaperSize>(117),
            "Envelope PRC 10",
            18368,
            25965,
        },
        {
            static_cast<ePaperSize>(118),
            "Envelope You 4",
            5952,
            13322,
        },
    };


#endif // PAPERSIZE_H
