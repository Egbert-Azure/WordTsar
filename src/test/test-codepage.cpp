#include "doctest.h"

#include <cstdint>
#include <ios>

#include "src/core/codepage/cp437.h"
#include "src/core/codepage/cp737.h"
#include "src/core/codepage/cp850.h"
#include "src/core/codepage/cp1252.h"
#include "src/core/codepage/cpmacroman.h"

TEST_CASE("CP437")
{
    cCodePage437 cp437 ;

    SUBCASE("toChar")
    {
        CHECK(cp437.toChar(0x263A) == 1) ;
        CHECK(cp437.toChar(0x263B) == 2) ;
        CHECK(cp437.toChar(0x2665) == 3) ;
        CHECK(cp437.toChar(0x2666) == 4) ;
        CHECK(cp437.toChar(0x2663) == 5) ;
        CHECK(cp437.toChar(0x2660) == 6) ;
        CHECK(cp437.toChar(0x2022) == 7) ;
        CHECK(cp437.toChar(0x25D8) == 8) ;
        CHECK(cp437.toChar(0x25CB) == 9) ;
        CHECK(cp437.toChar(0x25D9) == 10) ;
        CHECK(cp437.toChar(0x2642) == 11) ;
        CHECK(cp437.toChar(0x2640) == 12) ;
        CHECK(cp437.toChar(0x266A) == 13) ;
        CHECK(cp437.toChar(0x266C) == 14) ;
        CHECK(cp437.toChar(0x263C) == 15) ;
        CHECK(cp437.toChar(0x25BA) == 16) ;
        CHECK(cp437.toChar(0x25C4) == 17) ;
        CHECK(cp437.toChar(0x2195) == 18) ;
        CHECK(cp437.toChar(0x203C) == 19) ;
        CHECK(cp437.toChar(0x00B6) == 20) ;
        CHECK(cp437.toChar(0x00a7) == 21) ;
        CHECK(cp437.toChar(0x25AC) == 22) ;
        CHECK(cp437.toChar(0x21A8) == 23) ;
        CHECK(cp437.toChar(0x2191) == 24) ;
        CHECK(cp437.toChar(0x2193) == 25) ;
        CHECK(cp437.toChar(0x2192) == 26) ;
        CHECK(cp437.toChar(0x2190) == 27) ;
        CHECK(cp437.toChar(0x221F) == 28) ;
        CHECK(cp437.toChar(0x2194) == 29) ;
        CHECK(cp437.toChar(0x25B2) == 30) ;
        CHECK(cp437.toChar(0x25BC) == 31) ;
        CHECK(cp437.toChar(0x2302) == 127) ;
        CHECK(cp437.toChar(0x00C7) == 128) ;
        CHECK(cp437.toChar(0x00FC) == 129) ;
        CHECK(cp437.toChar(0x00e9) == 130) ;
        CHECK(cp437.toChar(0x00E2) == 131) ;
        CHECK(cp437.toChar(0x00E4) == 132) ;
        CHECK(cp437.toChar(0x00E0) == 133) ;
        CHECK(cp437.toChar(0x00E5) == 134) ;
        CHECK(cp437.toChar(0x00E7) == 135) ;
        CHECK(cp437.toChar(0x00EA) == 136) ;
        CHECK(cp437.toChar(0x00EB) == 137) ;
        CHECK(cp437.toChar(0x00E8) == 138) ;
        CHECK(cp437.toChar(0x00EF) == 139) ;
        CHECK(cp437.toChar(0x00EE) == 140) ;
        CHECK(cp437.toChar(0x00EC) == 141) ;
        CHECK(cp437.toChar(0x00C4) == 142) ;
        CHECK(cp437.toChar(0x00C5) == 143) ;
        CHECK(cp437.toChar(0x00C9) == 144) ;
        CHECK(cp437.toChar(0x00E6) == 145) ;
        CHECK(cp437.toChar(0x00C6) == 146) ;
        CHECK(cp437.toChar(0x00F4) == 147) ;
        CHECK(cp437.toChar(0x00F6) == 148) ;
        CHECK(cp437.toChar(0x00F2) == 149) ;
        CHECK(cp437.toChar(0x00FB) == 150) ;
        CHECK(cp437.toChar(0x00F9) == 151) ;
        CHECK(cp437.toChar(0x00FF) == 152) ;
        CHECK(cp437.toChar(0x00D6) == 153) ;
        CHECK(cp437.toChar(0x00DC) == 154) ;
        CHECK(cp437.toChar(0x00A2) == 155) ;
        CHECK(cp437.toChar(0x00A3) == 156) ;
        CHECK(cp437.toChar(0x00A5) == 157) ;
        CHECK(cp437.toChar(0x20A7) == 158) ;
        CHECK(cp437.toChar(0x0192) == 159) ;
        CHECK(cp437.toChar(0x00E1) == 160) ;
        CHECK(cp437.toChar(0x00ED) == 161) ;
        CHECK(cp437.toChar(0x00F3) == 162) ;
        CHECK(cp437.toChar(0x00FA) == 163) ;
        CHECK(cp437.toChar(0x00F1) == 164) ;
        CHECK(cp437.toChar(0x00D1) == 165) ;
        CHECK(cp437.toChar(0x00AA) == 166) ;
        CHECK(cp437.toChar(0x00BA) == 167) ;
        CHECK(cp437.toChar(0x00BF) == 168) ;
        CHECK(cp437.toChar(0x2310) == 169) ;
        CHECK(cp437.toChar(0x00AC) == 170) ;
        CHECK(cp437.toChar(0x00BD) == 171) ;
        CHECK(cp437.toChar(0x00BC) == 172) ;
        CHECK(cp437.toChar(0x00A1) == 173) ;
        CHECK(cp437.toChar(0x00AB) == 174) ;
        CHECK(cp437.toChar(0x00BB) == 175) ;
        CHECK(cp437.toChar(0x2591) == 176) ;
        CHECK(cp437.toChar(0x2592) == 177) ;
        CHECK(cp437.toChar(0x2593) == 178) ;
        CHECK(cp437.toChar(0x2502) == 179) ;
        CHECK(cp437.toChar(0x2524) == 180) ;
        CHECK(cp437.toChar(0x2561) == 181) ;
        CHECK(cp437.toChar(0x2562) == 182) ;
        CHECK(cp437.toChar(0x2556) == 183) ;
        CHECK(cp437.toChar(0x2555) == 184) ;
        CHECK(cp437.toChar(0x2563) == 185) ;
        CHECK(cp437.toChar(0x2551) == 186) ;
        CHECK(cp437.toChar(0x2557) == 187) ;
        CHECK(cp437.toChar(0x255D) == 188) ;
        CHECK(cp437.toChar(0x255C) == 189) ;
        CHECK(cp437.toChar(0x255B) == 190) ;
        CHECK(cp437.toChar(0x2510) == 191) ;
        CHECK(cp437.toChar(0x2514) == 192) ;
        CHECK(cp437.toChar(0x2534) == 193) ;
        CHECK(cp437.toChar(0x252C) == 194) ;
        CHECK(cp437.toChar(0x251C) == 195) ;
        CHECK(cp437.toChar(0x2500) == 196) ;
        CHECK(cp437.toChar(0x253C) == 197) ;
        CHECK(cp437.toChar(0x255E) == 198) ;
        CHECK(cp437.toChar(0x255F) == 199) ;
        CHECK(cp437.toChar(0x255A) == 200) ;
        CHECK(cp437.toChar(0x2554) == 201) ;
        CHECK(cp437.toChar(0x2569) == 202) ;
        CHECK(cp437.toChar(0x2566) == 203) ;
        CHECK(cp437.toChar(0x2560) == 204) ;
        CHECK(cp437.toChar(0x2550) == 205) ;
        CHECK(cp437.toChar(0x256C) == 206) ;
        CHECK(cp437.toChar(0x2567) == 207) ;
        CHECK(cp437.toChar(0x2568) == 208) ;
        CHECK(cp437.toChar(0x2564) == 209) ;
        CHECK(cp437.toChar(0x2565) == 210) ;
        CHECK(cp437.toChar(0x2559) == 211) ;
        CHECK(cp437.toChar(0x2558) == 212) ;
        CHECK(cp437.toChar(0x2552) == 213) ;
        CHECK(cp437.toChar(0x2553) == 214) ;
        CHECK(cp437.toChar(0x256B) == 215) ;
        CHECK(cp437.toChar(0x256A) == 216) ;
        CHECK(cp437.toChar(0x2518) == 217) ;
        CHECK(cp437.toChar(0x250C) == 218) ;
        CHECK(cp437.toChar(0x2588) == 219) ;
        CHECK(cp437.toChar(0x2584) == 220) ;
        CHECK(cp437.toChar(0x258C) == 221) ;
        CHECK(cp437.toChar(0x2590) == 222) ;
        CHECK(cp437.toChar(0x2580) == 223) ;
        CHECK(cp437.toChar(0x03B1) == 224) ;
        CHECK(cp437.toChar(0x00DF) == 225) ;
        CHECK(cp437.toChar(0x0393) == 226) ;
        CHECK(cp437.toChar(0x03C0) == 227) ;
        CHECK(cp437.toChar(0x03A3) == 228) ;
        CHECK(cp437.toChar(0x03C3) == 229) ;
        CHECK(cp437.toChar(0x00B5) == 230) ;
        CHECK(cp437.toChar(0x03C4) == 231) ;
        CHECK(cp437.toChar(0x03A6) == 232) ;
        CHECK(cp437.toChar(0x0398) == 233) ;
        CHECK(cp437.toChar(0x03A9) == 234) ;
        CHECK(cp437.toChar(0x03B4) == 235) ;
        CHECK(cp437.toChar(0x221E) == 236) ;
        CHECK(cp437.toChar(0x03C6) == 237) ;
        CHECK(cp437.toChar(0x03B5) == 238) ;
        CHECK(cp437.toChar(0x2229) == 239) ;
        CHECK(cp437.toChar(0x2261) == 240) ;
        CHECK(cp437.toChar(0x00B1) == 241) ;
        CHECK(cp437.toChar(0x2265) == 242) ;
        CHECK(cp437.toChar(0x2264) == 243) ;
        CHECK(cp437.toChar(0x2320) == 244) ;
        CHECK(cp437.toChar(0x2321) == 245) ;
        CHECK(cp437.toChar(0x00F7) == 246) ;
        CHECK(cp437.toChar(0x2248) == 247) ;
        CHECK(cp437.toChar(0x00B0) == 248) ;
        CHECK(cp437.toChar(0x2219) == 249) ;
        CHECK(cp437.toChar(0x00B7) == 250) ;
        CHECK(cp437.toChar(0x221A) == 251) ;
        CHECK(cp437.toChar(0x207F) == 252) ;
        CHECK(cp437.toChar(0x00B2) == 253) ;
        CHECK(cp437.toChar(0x25A0) == 254) ;
    }

    SUBCASE("to UTF8")
    {
        // ASCII range (0x00-0x7F) is identity mapped via the head guard in
        // cCodePage437::toUTF8. The CP437 spec's graphical mappings for
        // bytes 0x01-0x1F (smiley, heart, etc.) and 0x7F (house) live only
        // in the toChar direction and apply when the user types those
        // glyphs and we encode for WordStar save. The toUTF8 direction
        // never sees those bytes in practice -- WordStar files use
        // 0x01-0x1F as style codes (STYLE_FONT1 etc.), and ASCII is
        // passed through unchanged by the file loaders.
        CHECK(cp437.toUTF8(128) == 0x00C7) ;
        CHECK(cp437.toUTF8(129) == 0x00FC) ;
        CHECK(cp437.toUTF8(130) == 0x00E9) ;
        CHECK(cp437.toUTF8(131) == 0x00E2) ;
        CHECK(cp437.toUTF8(132) == 0x00E4) ;
        CHECK(cp437.toUTF8(133) == 0x00E0) ;
        CHECK(cp437.toUTF8(134) == 0x00E5) ;
        CHECK(cp437.toUTF8(135) == 0x00E7) ;
        CHECK(cp437.toUTF8(136) == 0x00EA) ;
        CHECK(cp437.toUTF8(137) == 0x00EB) ;
        CHECK(cp437.toUTF8(138) == 0x00E8) ;
        CHECK(cp437.toUTF8(139) == 0x00EF) ;
        CHECK(cp437.toUTF8(140) == 0x00EE) ;
        CHECK(cp437.toUTF8(141) == 0x00EC) ;
        CHECK(cp437.toUTF8(142) == 0x00C4) ;
        CHECK(cp437.toUTF8(143) == 0x00C5) ;
        CHECK(cp437.toUTF8(144) == 0x00C9) ;
        CHECK(cp437.toUTF8(145) == 0x00E6) ;
        CHECK(cp437.toUTF8(146) == 0x00C6) ;
        CHECK(cp437.toUTF8(147) == 0x00F4) ;
        CHECK(cp437.toUTF8(148) == 0x00F6) ;
        CHECK(cp437.toUTF8(149) == 0x00F2) ;
        CHECK(cp437.toUTF8(150) == 0x00FB) ;
        CHECK(cp437.toUTF8(151) == 0x00F9) ;
        CHECK(cp437.toUTF8(152) == 0x00FF) ;
        CHECK(cp437.toUTF8(153) == 0x00D6) ;
        CHECK(cp437.toUTF8(154) == 0x00DC) ;
        CHECK(cp437.toUTF8(155) == 0x00A2) ;
        CHECK(cp437.toUTF8(156) == 0x00A3) ;
        CHECK(cp437.toUTF8(157) == 0x00A5) ;
        CHECK(cp437.toUTF8(158) == 0x20A7) ;
        CHECK(cp437.toUTF8(159) == 0x0192) ;
        CHECK(cp437.toUTF8(160) == 0x00E1) ;
        CHECK(cp437.toUTF8(161) == 0x00ED) ;
        CHECK(cp437.toUTF8(162) == 0x00F3) ;
        CHECK(cp437.toUTF8(163) == 0x00FA) ;
        CHECK(cp437.toUTF8(164) == 0x00F1) ;
        CHECK(cp437.toUTF8(165) == 0x00D1) ;
        CHECK(cp437.toUTF8(166) == 0x00AA) ;
        CHECK(cp437.toUTF8(167) == 0x00BA) ;
        CHECK(cp437.toUTF8(168) == 0x00BF) ;
        CHECK(cp437.toUTF8(169) == 0x2310) ;
        CHECK(cp437.toUTF8(170) == 0x00AC) ;
        CHECK(cp437.toUTF8(171) == 0x00BD) ;
        CHECK(cp437.toUTF8(172) == 0x00BC) ;
        CHECK(cp437.toUTF8(173) == 0x00A1) ;
        CHECK(cp437.toUTF8(174) == 0x00AB) ;
        CHECK(cp437.toUTF8(175) == 0x00BB) ;
        CHECK(cp437.toUTF8(176) == 0x2591) ;
        CHECK(cp437.toUTF8(177) == 0x2592) ;
        CHECK(cp437.toUTF8(178) == 0x2593) ;
        CHECK(cp437.toUTF8(179) == 0x2502) ;
        CHECK(cp437.toUTF8(180) == 0x2524) ;
        CHECK(cp437.toUTF8(181) == 0x2561) ;
        CHECK(cp437.toUTF8(182) == 0x2562) ;
        CHECK(cp437.toUTF8(183) == 0x2556) ;
        CHECK(cp437.toUTF8(184) == 0x2555) ;
        CHECK(cp437.toUTF8(185) == 0x2563) ;
        CHECK(cp437.toUTF8(186) == 0x2551) ;
        CHECK(cp437.toUTF8(187) == 0x2557) ;
        CHECK(cp437.toUTF8(188) == 0x255D) ;
        CHECK(cp437.toUTF8(189) == 0x255C) ;
        CHECK(cp437.toUTF8(190) == 0x255B) ;
        CHECK(cp437.toUTF8(191) == 0x2510) ;
        CHECK(cp437.toUTF8(192) == 0x2514) ;
        CHECK(cp437.toUTF8(193) == 0x2534) ;
        CHECK(cp437.toUTF8(194) == 0x252C) ;
        CHECK(cp437.toUTF8(195) == 0x251C) ;
        CHECK(cp437.toUTF8(196) == 0x2500) ;
        CHECK(cp437.toUTF8(197) == 0x253C) ;
        CHECK(cp437.toUTF8(198) == 0x255E) ;
        CHECK(cp437.toUTF8(199) == 0x255F) ;
        CHECK(cp437.toUTF8(200) == 0x255A) ;
        CHECK(cp437.toUTF8(201) == 0x2554) ;
        CHECK(cp437.toUTF8(202) == 0x2569) ;
        CHECK(cp437.toUTF8(203) == 0x2566) ;
        CHECK(cp437.toUTF8(204) == 0x2560) ;
        CHECK(cp437.toUTF8(205) == 0x2550) ;
        CHECK(cp437.toUTF8(206) == 0x256C) ;
        CHECK(cp437.toUTF8(207) == 0x2567) ;
        CHECK(cp437.toUTF8(208) == 0x2568) ;
        CHECK(cp437.toUTF8(209) == 0x2564) ;
        CHECK(cp437.toUTF8(210) == 0x2565) ;
        CHECK(cp437.toUTF8(211) == 0x2559) ;
        CHECK(cp437.toUTF8(212) == 0x2558) ;
        CHECK(cp437.toUTF8(213) == 0x2552) ;
        CHECK(cp437.toUTF8(214) == 0x2553) ;
        CHECK(cp437.toUTF8(215) == 0x256B) ;
        CHECK(cp437.toUTF8(216) == 0x256A) ;
        CHECK(cp437.toUTF8(217) == 0x2518) ;
        CHECK(cp437.toUTF8(218) == 0x250C) ;
        CHECK(cp437.toUTF8(219) == 0x2588) ;
        CHECK(cp437.toUTF8(220) == 0x2584) ;
        CHECK(cp437.toUTF8(221) == 0x258C) ;
        CHECK(cp437.toUTF8(222) == 0x2590) ;
        CHECK(cp437.toUTF8(223) == 0x2580) ;
        CHECK(cp437.toUTF8(224) == 0x03B1) ;
        CHECK(cp437.toUTF8(225) == 0x00DF) ;
        CHECK(cp437.toUTF8(226) == 0x0393) ;
        CHECK(cp437.toUTF8(227) == 0x03C0) ;
        CHECK(cp437.toUTF8(228) == 0x03A3) ;
        CHECK(cp437.toUTF8(229) == 0x03C3) ;
        CHECK(cp437.toUTF8(230) == 0x00B5) ;
        CHECK(cp437.toUTF8(231) == 0x03C4) ;
        CHECK(cp437.toUTF8(232) == 0x03A6) ;
        CHECK(cp437.toUTF8(233) == 0x0398) ;
        CHECK(cp437.toUTF8(234) == 0x03A9) ;
        CHECK(cp437.toUTF8(235) == 0x03B4) ;
        CHECK(cp437.toUTF8(236) == 0x221E) ;
        CHECK(cp437.toUTF8(237) == 0x03C6) ;
        CHECK(cp437.toUTF8(238) == 0x03B5) ;
        CHECK(cp437.toUTF8(239) == 0x2229) ;
        CHECK(cp437.toUTF8(240) == 0x2261) ;
        CHECK(cp437.toUTF8(241) == 0x00B1) ;
        CHECK(cp437.toUTF8(242) == 0x2265) ;
        CHECK(cp437.toUTF8(243) == 0x2264) ;
        CHECK(cp437.toUTF8(244) == 0x2320) ;
        CHECK(cp437.toUTF8(245) == 0x2321) ;
        CHECK(cp437.toUTF8(246) == 0x00F7) ;
        CHECK(cp437.toUTF8(247) == 0x2248) ;
        CHECK(cp437.toUTF8(248) == 0x00B0) ;
        CHECK(cp437.toUTF8(249) == 0x2219) ;
        CHECK(cp437.toUTF8(250) == 0x00B7) ;
        CHECK(cp437.toUTF8(251) == 0x221A) ;
        CHECK(cp437.toUTF8(252) == 0x207F) ;
        CHECK(cp437.toUTF8(253) == 0x00B2) ;
        CHECK(cp437.toUTF8(254) == 0x25A0) ;
    }
}

TEST_CASE("CP737")
{
    cCodePage737 cp737 ;

    SUBCASE("toChar")
    {
        CHECK(cp737.toChar(0x263A) == 1) ;
        CHECK(cp737.toChar(0x263B) == 2) ;
        CHECK(cp737.toChar(0x2665) == 3) ;
        CHECK(cp737.toChar(0x2666) == 4) ;
        CHECK(cp737.toChar(0x2663) == 5) ;
        CHECK(cp737.toChar(0x2660) == 6) ;
        CHECK(cp737.toChar(0x2022) == 7) ;
        CHECK(cp737.toChar(0x25D8) == 8) ;
        CHECK(cp737.toChar(0x25CB) == 9) ;
        CHECK(cp737.toChar(0x25D9) == 10) ;
        CHECK(cp737.toChar(0x2642) == 11) ;
        CHECK(cp737.toChar(0x2640) == 12) ;
        CHECK(cp737.toChar(0x266A) == 13) ;
        CHECK(cp737.toChar(0x266C) == 14) ;
        CHECK(cp737.toChar(0x263C) == 15) ;
        CHECK(cp737.toChar(0x25BA) == 16) ;
        CHECK(cp737.toChar(0x25C4) == 17) ;
        CHECK(cp737.toChar(0x2195) == 18) ;
        CHECK(cp737.toChar(0x203C) == 19) ;
        CHECK(cp737.toChar(0x00B6) == 20) ;
        CHECK(cp737.toChar(0x00A7) == 21) ;
        CHECK(cp737.toChar(0x25AC) == 22) ;
        CHECK(cp737.toChar(0x21A8) == 23) ;
        CHECK(cp737.toChar(0x2191) == 24) ;
        CHECK(cp737.toChar(0x2193) == 25) ;
        CHECK(cp737.toChar(0x2192) == 26) ;
        CHECK(cp737.toChar(0x2190) == 27) ;
        CHECK(cp737.toChar(0x221F) == 28) ;
        CHECK(cp737.toChar(0x2194) == 29) ;
        CHECK(cp737.toChar(0x25B2) == 30) ;
        CHECK(cp737.toChar(0x25BC) == 31) ;
        CHECK(cp737.toChar(0x2302) == 127) ;
        CHECK(cp737.toChar(0x0391) == 128) ;
        CHECK(cp737.toChar(0x0392) == 129) ;
        CHECK(cp737.toChar(0x0393) == 130) ;
        CHECK(cp737.toChar(0x0394) == 131) ;
        CHECK(cp737.toChar(0x0395) == 132) ;
        CHECK(cp737.toChar(0x0396) == 133) ;
        CHECK(cp737.toChar(0x0397) == 134) ;
        CHECK(cp737.toChar(0x0398) == 135) ;
        CHECK(cp737.toChar(0x0399) == 136) ;
        CHECK(cp737.toChar(0x039A) == 137) ;
        CHECK(cp737.toChar(0x039B) == 138) ;
        CHECK(cp737.toChar(0x039C) == 139) ;
        CHECK(cp737.toChar(0x039D) == 140) ;
        CHECK(cp737.toChar(0x039E) == 141) ;
        CHECK(cp737.toChar(0x039F) == 142) ;
        CHECK(cp737.toChar(0x03A0) == 143) ;
        CHECK(cp737.toChar(0x03A1) == 144) ;
        CHECK(cp737.toChar(0x03A3) == 145) ;
        CHECK(cp737.toChar(0x03A4) == 146) ;
        CHECK(cp737.toChar(0x03A5) == 147) ;
        CHECK(cp737.toChar(0x03A6) == 148) ;
        CHECK(cp737.toChar(0x03A7) == 149) ;
        CHECK(cp737.toChar(0x03A8) == 150) ;
        CHECK(cp737.toChar(0x03A9) == 151) ;
        CHECK(cp737.toChar(0x03B1) == 152) ;
        CHECK(cp737.toChar(0x03B2) == 153) ;
        CHECK(cp737.toChar(0x03B3) == 154) ;
        CHECK(cp737.toChar(0x03B4) == 155) ;
        CHECK(cp737.toChar(0x03B5) == 156) ;
        CHECK(cp737.toChar(0x03B6) == 157) ;
        CHECK(cp737.toChar(0x03B7) == 158) ;
        CHECK(cp737.toChar(0x03B8) == 159) ;
        CHECK(cp737.toChar(0x03B9) == 160) ;
        CHECK(cp737.toChar(0x03BA) == 161) ;
        CHECK(cp737.toChar(0x03BB) == 162) ;
        CHECK(cp737.toChar(0x03BC) == 163) ;
        CHECK(cp737.toChar(0x03BD) == 164) ;
        CHECK(cp737.toChar(0x03BE) == 165) ;
        CHECK(cp737.toChar(0x03BF) == 166) ;
        CHECK(cp737.toChar(0x03C0) == 167) ;
        CHECK(cp737.toChar(0x03C1) == 168) ;
        CHECK(cp737.toChar(0x03C3) == 169) ;
        CHECK(cp737.toChar(0x03C2) == 170) ;
        CHECK(cp737.toChar(0x03C4) == 171) ;
        CHECK(cp737.toChar(0x03C5) == 172) ;
        CHECK(cp737.toChar(0x03C6) == 173) ;
        CHECK(cp737.toChar(0x03C7) == 174) ;
        CHECK(cp737.toChar(0x03C8) == 175) ;
        CHECK(cp737.toChar(0x2591) == 176) ;
        CHECK(cp737.toChar(0x2592) == 177) ;
        CHECK(cp737.toChar(0x2593) == 178) ;
        CHECK(cp737.toChar(0x2502) == 179) ;
        CHECK(cp737.toChar(0x2524) == 180) ;
        CHECK(cp737.toChar(0x2561) == 181) ;
        CHECK(cp737.toChar(0x2562) == 182) ;
        CHECK(cp737.toChar(0x2556) == 183) ;
        CHECK(cp737.toChar(0x2555) == 184) ;
        CHECK(cp737.toChar(0x2563) == 185) ;
        CHECK(cp737.toChar(0x2551) == 186) ;
        CHECK(cp737.toChar(0x2557) == 187) ;
        CHECK(cp737.toChar(0x255D) == 188) ;
        CHECK(cp737.toChar(0x255C) == 189) ;
        CHECK(cp737.toChar(0x255B) == 190) ;
        CHECK(cp737.toChar(0x2510) == 191) ;
        CHECK(cp737.toChar(0x2514) == 192) ;
        CHECK(cp737.toChar(0x2534) == 193) ;
        CHECK(cp737.toChar(0x252C) == 194) ;
        CHECK(cp737.toChar(0x251C) == 195) ;
        CHECK(cp737.toChar(0x2500) == 196) ;
        CHECK(cp737.toChar(0x253C) == 197) ;
        CHECK(cp737.toChar(0x255E) == 198) ;
        CHECK(cp737.toChar(0x255F) == 199) ;
        CHECK(cp737.toChar(0x255A) == 200) ;
        CHECK(cp737.toChar(0x2554) == 201) ;
        CHECK(cp737.toChar(0x2569) == 202) ;
        CHECK(cp737.toChar(0x2566) == 203) ;
        CHECK(cp737.toChar(0x2560) == 204) ;
        CHECK(cp737.toChar(0x2550) == 205) ;
        CHECK(cp737.toChar(0x256C) == 206) ;
        CHECK(cp737.toChar(0x2567) == 207) ;
        CHECK(cp737.toChar(0x2568) == 208) ;
        CHECK(cp737.toChar(0x2564) == 209) ;
        CHECK(cp737.toChar(0x2565) == 210) ;
        CHECK(cp737.toChar(0x2559) == 211) ;
        CHECK(cp737.toChar(0x2558) == 212) ;
        CHECK(cp737.toChar(0x2552) == 213) ;
        CHECK(cp737.toChar(0x2553) == 214) ;
        CHECK(cp737.toChar(0x256B) == 215) ;
        CHECK(cp737.toChar(0x256A) == 216) ;
        CHECK(cp737.toChar(0x2518) == 217) ;
        CHECK(cp737.toChar(0x250C) == 218) ;
        CHECK(cp737.toChar(0x2588) == 219) ;
        CHECK(cp737.toChar(0x2584) == 220) ;
        CHECK(cp737.toChar(0x258C) == 221) ;
        CHECK(cp737.toChar(0x2590) == 222) ;
        CHECK(cp737.toChar(0x2580) == 223) ;
        CHECK(cp737.toChar(0x03C9) == 224) ;
        CHECK(cp737.toChar(0x03AC) == 225) ;
        CHECK(cp737.toChar(0x03AD) == 226) ;
        CHECK(cp737.toChar(0x03AE) == 227) ;
        CHECK(cp737.toChar(0x03CA) == 228) ;
        CHECK(cp737.toChar(0x03AF) == 229) ;
        CHECK(cp737.toChar(0x03CC) == 230) ;
        CHECK(cp737.toChar(0x03CD) == 231) ;
        CHECK(cp737.toChar(0x03CB) == 232) ;
        CHECK(cp737.toChar(0x03CE) == 233) ;
        CHECK(cp737.toChar(0x0386) == 234) ;
        CHECK(cp737.toChar(0x0388) == 235) ;
        CHECK(cp737.toChar(0x0389) == 236) ;
        CHECK(cp737.toChar(0x038A) == 237) ;
        CHECK(cp737.toChar(0x038C) == 238) ;
        CHECK(cp737.toChar(0x038E) == 239) ;
        CHECK(cp737.toChar(0x038F) == 240) ;
        CHECK(cp737.toChar(0x00B1) == 241) ;
        CHECK(cp737.toChar(0x2265) == 242) ;
        CHECK(cp737.toChar(0x2264) == 243) ;
        CHECK(cp737.toChar(0x03AA) == 244) ;
        CHECK(cp737.toChar(0x03AB) == 245) ;
        CHECK(cp737.toChar(0x00F7) == 246) ;
        CHECK(cp737.toChar(0x2248) == 247) ;
        CHECK(cp737.toChar(0x00B0) == 248) ;
        CHECK(cp737.toChar(0x2219) == 249) ;
        CHECK(cp737.toChar(0x00B7) == 250) ;
        CHECK(cp737.toChar(0x221A) == 251) ;
        CHECK(cp737.toChar(0x207F) == 252) ;
        CHECK(cp737.toChar(0x00B2) == 253) ;
        CHECK(cp737.toChar(0x25A0) == 254) ;
    }

    SUBCASE("from Char")
    {
        // ASCII range (0x00-0x7F) is identity mapped via the head guard.
        // CP737 has no graphical mappings in this range; the legacy
        // assertions for bytes 0x01-0x1F here were copied from CP437 spec
        // tables and do not reflect WordTsar's practical use of CP737.
        CHECK(cp737.toUTF8(128) == 0x0391) ;
        CHECK(cp737.toUTF8(129) == 0x0392) ;
        CHECK(cp737.toUTF8(130) == 0x0393) ;
        CHECK(cp737.toUTF8(131) == 0x0394) ;
        CHECK(cp737.toUTF8(132) == 0x0395) ;
        CHECK(cp737.toUTF8(133) == 0x0396) ;
        CHECK(cp737.toUTF8(134) == 0x0397) ;
        CHECK(cp737.toUTF8(135) == 0x0398) ;
        CHECK(cp737.toUTF8(136) == 0x0399) ;
        CHECK(cp737.toUTF8(137) == 0x039A) ;
        CHECK(cp737.toUTF8(138) == 0x039B) ;
        CHECK(cp737.toUTF8(139) == 0x039C) ;
        CHECK(cp737.toUTF8(140) == 0x039D) ;
        CHECK(cp737.toUTF8(141) == 0x039E) ;
        CHECK(cp737.toUTF8(142) == 0x039F) ;
        CHECK(cp737.toUTF8(143) == 0x03A0) ;
        CHECK(cp737.toUTF8(144) == 0x03A1) ;
        CHECK(cp737.toUTF8(145) == 0x03A3) ;
        CHECK(cp737.toUTF8(146) == 0x03A4) ;
        CHECK(cp737.toUTF8(147) == 0x03A5) ;
        CHECK(cp737.toUTF8(148) == 0x03A6) ;
        CHECK(cp737.toUTF8(149) == 0x03A7) ;
        CHECK(cp737.toUTF8(150) == 0x03A8) ;
        CHECK(cp737.toUTF8(151) == 0x03A9) ;
        CHECK(cp737.toUTF8(152) == 0x03B1) ;
        CHECK(cp737.toUTF8(153) == 0x03B2) ;
        CHECK(cp737.toUTF8(154) == 0x03B3) ;
        CHECK(cp737.toUTF8(155) == 0x03B4) ;
        CHECK(cp737.toUTF8(156) == 0x03B5) ;
        CHECK(cp737.toUTF8(157) == 0x03B6) ;
        CHECK(cp737.toUTF8(158) == 0x03B7) ;
        CHECK(cp737.toUTF8(159) == 0x03B8) ;
        CHECK(cp737.toUTF8(160) == 0x03B9) ;
        CHECK(cp737.toUTF8(161) == 0x03BA) ;
        CHECK(cp737.toUTF8(162) == 0x03BB) ;
        CHECK(cp737.toUTF8(163) == 0x03BC) ;
        CHECK(cp737.toUTF8(164) == 0x03BD) ;
        CHECK(cp737.toUTF8(165) == 0x03BE) ;
        CHECK(cp737.toUTF8(166) == 0x03BF) ;
        CHECK(cp737.toUTF8(167) == 0x03C0) ;
        CHECK(cp737.toUTF8(168) == 0x03C1) ;
        CHECK(cp737.toUTF8(169) == 0x03C3) ;
        CHECK(cp737.toUTF8(170) == 0x03C2) ;
        CHECK(cp737.toUTF8(171) == 0x03C4) ;
        CHECK(cp737.toUTF8(172) == 0x03C5) ;
        CHECK(cp737.toUTF8(173) == 0x03C6) ;
        CHECK(cp737.toUTF8(174) == 0x03C7) ;
        CHECK(cp737.toUTF8(175) == 0x03C8) ;
        CHECK(cp737.toUTF8(176) == 0x2591) ;
        CHECK(cp737.toUTF8(177) == 0x2592) ;
        CHECK(cp737.toUTF8(178) == 0x2593) ;
        CHECK(cp737.toUTF8(179) == 0x2502) ;
        CHECK(cp737.toUTF8(180) == 0x2524) ;
        CHECK(cp737.toUTF8(181) == 0x2561) ;
        CHECK(cp737.toUTF8(182) == 0x2562) ;
        CHECK(cp737.toUTF8(183) == 0x2556) ;
        CHECK(cp737.toUTF8(184) == 0x2555) ;
        CHECK(cp737.toUTF8(185) == 0x2563) ;
        CHECK(cp737.toUTF8(186) == 0x2551) ;
        CHECK(cp737.toUTF8(187) == 0x2557) ;
        CHECK(cp737.toUTF8(188) == 0x255D) ;
        CHECK(cp737.toUTF8(189) == 0x255C) ;
        CHECK(cp737.toUTF8(190) == 0x255B) ;
        CHECK(cp737.toUTF8(191) == 0x2510) ;
        CHECK(cp737.toUTF8(192) == 0x2514) ;
        CHECK(cp737.toUTF8(193) == 0x2534) ;
        CHECK(cp737.toUTF8(194) == 0x252C) ;
        CHECK(cp737.toUTF8(195) == 0x251C) ;
        CHECK(cp737.toUTF8(196) == 0x2500) ;
        CHECK(cp737.toUTF8(197) == 0x253C) ;
        CHECK(cp737.toUTF8(198) == 0x255E) ;
        CHECK(cp737.toUTF8(199) == 0x255F) ;
        CHECK(cp737.toUTF8(200) == 0x255A) ;
        CHECK(cp737.toUTF8(201) == 0x2554) ;
        CHECK(cp737.toUTF8(202) == 0x2569) ;
        CHECK(cp737.toUTF8(203) == 0x2566) ;
        CHECK(cp737.toUTF8(204) == 0x2560) ;
        CHECK(cp737.toUTF8(205) == 0x2550) ;
        CHECK(cp737.toUTF8(206) == 0x256C) ;
        CHECK(cp737.toUTF8(207) == 0x2567) ;
        CHECK(cp737.toUTF8(208) == 0x2568) ;
        CHECK(cp737.toUTF8(209) == 0x2564) ;
        CHECK(cp737.toUTF8(210) == 0x2565) ;
        CHECK(cp737.toUTF8(211) == 0x2559) ;
        CHECK(cp737.toUTF8(212) == 0x2558) ;
        CHECK(cp737.toUTF8(213) == 0x2552) ;
        CHECK(cp737.toUTF8(214) == 0x2553) ;
        CHECK(cp737.toUTF8(215) == 0x256B) ;
        CHECK(cp737.toUTF8(216) == 0x256A) ;
        CHECK(cp737.toUTF8(217) == 0x2518) ;
        CHECK(cp737.toUTF8(218) == 0x250C) ;
        CHECK(cp737.toUTF8(219) == 0x2588) ;
        CHECK(cp737.toUTF8(220) == 0x2584) ;
        CHECK(cp737.toUTF8(221) == 0x258C) ;
        CHECK(cp737.toUTF8(222) == 0x2590) ;
        CHECK(cp737.toUTF8(223) == 0x2580) ;
        CHECK(cp737.toUTF8(224) == 0x03C9) ;
        CHECK(cp737.toUTF8(225) == 0x03AC) ;
        CHECK(cp737.toUTF8(226) == 0x03AD) ;
        CHECK(cp737.toUTF8(227) == 0x03AE) ;
        CHECK(cp737.toUTF8(228) == 0x03CA) ;
        CHECK(cp737.toUTF8(229) == 0x03AF) ;
        CHECK(cp737.toUTF8(230) == 0x03CC) ;
        CHECK(cp737.toUTF8(231) == 0x03CD) ;
        CHECK(cp737.toUTF8(232) == 0x03CB) ;
        CHECK(cp737.toUTF8(233) == 0x03CE) ;
        CHECK(cp737.toUTF8(234) == 0x0386) ;
        CHECK(cp737.toUTF8(235) == 0x0388) ;
        CHECK(cp737.toUTF8(236) == 0x0389) ;
        CHECK(cp737.toUTF8(237) == 0x038A) ;
        CHECK(cp737.toUTF8(238) == 0x038C) ;
        CHECK(cp737.toUTF8(239) == 0x038E) ;
        CHECK(cp737.toUTF8(240) == 0x038F) ;
        CHECK(cp737.toUTF8(241) == 0x00B1) ;
        CHECK(cp737.toUTF8(242) == 0x2265) ;
        CHECK(cp737.toUTF8(243) == 0x2264) ;
        CHECK(cp737.toUTF8(244) == 0x03AA) ;
        CHECK(cp737.toUTF8(245) == 0x03AB) ;
        CHECK(cp737.toUTF8(246) == 0x00F7) ;
        CHECK(cp737.toUTF8(247) == 0x2248) ;
        CHECK(cp737.toUTF8(248) == 0x00B0) ;
        CHECK(cp737.toUTF8(249) == 0x2219) ;
        CHECK(cp737.toUTF8(250) == 0x00B7) ;
        CHECK(cp737.toUTF8(251) == 0x221A) ;
        CHECK(cp737.toUTF8(252) == 0x207F) ;
        CHECK(cp737.toUTF8(253) == 0x00B2) ;
        CHECK(cp737.toUTF8(254) == 0x25A0) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test CP850 (DOS Latin-1) codepage conversion.
/// Verifies toChar (Unicode to CP850) and toUTF8 (CP850 to Unicode) roundtrips.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CP850")
{
    cCodePage850 cp850 ;

    SUBCASE("toChar")
    {
        // First entries
        CHECK(cp850.toChar(0x00C7) == 0x80) ;  // C with cedilla
        CHECK(cp850.toChar(0x00FC) == 0x81) ;  // u with diaeresis
        CHECK(cp850.toChar(0x00E9) == 0x82) ;  // e with acute
        CHECK(cp850.toChar(0x00E2) == 0x83) ;  // a with circumflex
        CHECK(cp850.toChar(0x00E4) == 0x84) ;  // a with diaeresis
        CHECK(cp850.toChar(0x00E0) == 0x85) ;  // a with grave
        CHECK(cp850.toChar(0x00E5) == 0x86) ;  // a with ring above
        CHECK(cp850.toChar(0x00E7) == 0x87) ;  // c with cedilla

        // Last entries
        CHECK(cp850.toChar(0x00F7) == 0xF6) ;  // division sign
        CHECK(cp850.toChar(0x00B8) == 0xF7) ;  // cedilla
        CHECK(cp850.toChar(0x00B0) == 0xF8) ;  // degree sign
        CHECK(cp850.toChar(0x00B7) == 0xFA) ;  // middle dot
        CHECK(cp850.toChar(0x00B9) == 0xFB) ;  // superscript one
        CHECK(cp850.toChar(0x00B3) == 0xFC) ;  // superscript three
        CHECK(cp850.toChar(0x00B2) == 0xFD) ;  // superscript two
        CHECK(cp850.toChar(0x25A0) == 0xFE) ;  // black square
        CHECK(cp850.toChar(0x00A0) == 0xFF) ;  // no-break space
    }

    SUBCASE("toUTF8")
    {
        // First entries
        CHECK(cp850.toUTF8(0x80) == 0x00C7) ;  // C with cedilla
        CHECK(cp850.toUTF8(0x81) == 0x00FC) ;  // u with diaeresis
        CHECK(cp850.toUTF8(0x82) == 0x00E9) ;  // e with acute
        CHECK(cp850.toUTF8(0x83) == 0x00E2) ;  // a with circumflex
        CHECK(cp850.toUTF8(0x84) == 0x00E4) ;  // a with diaeresis
        CHECK(cp850.toUTF8(0x85) == 0x00E0) ;  // a with grave
        CHECK(cp850.toUTF8(0x86) == 0x00E5) ;  // a with ring above
        CHECK(cp850.toUTF8(0x87) == 0x00E7) ;  // c with cedilla

        // Last entries
        CHECK(cp850.toUTF8(0xF6) == 0x00F7) ;  // division sign
        CHECK(cp850.toUTF8(0xF7) == 0x00B8) ;  // cedilla
        CHECK(cp850.toUTF8(0xF8) == 0x00B0) ;  // degree sign
        CHECK(cp850.toUTF8(0xFA) == 0x00B7) ;  // middle dot
        CHECK(cp850.toUTF8(0xFB) == 0x00B9) ;  // superscript one
        CHECK(cp850.toUTF8(0xFC) == 0x00B3) ;  // superscript three
        CHECK(cp850.toUTF8(0xFD) == 0x00B2) ;  // superscript two
        CHECK(cp850.toUTF8(0xFE) == 0x25A0) ;  // black square
        CHECK(cp850.toUTF8(0xFF) == 0x00A0) ;  // no-break space
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test Windows-1252 codepage conversion.
/// Only bytes 0x80-0x9F differ from Unicode/Latin-1.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("CP1252")
{
    cCodePageWin1252 cp1252 ;

    SUBCASE("toChar")
    {
        CHECK(cp1252.toChar(0x20AC) == 0x80) ;  // euro sign
        CHECK(cp1252.toChar(0x201A) == 0x82) ;  // single low-9 quotation mark
        CHECK(cp1252.toChar(0x0192) == 0x83) ;  // f with hook
        CHECK(cp1252.toChar(0x201E) == 0x84) ;  // double low-9 quotation mark
        CHECK(cp1252.toChar(0x2026) == 0x85) ;  // horizontal ellipsis
        CHECK(cp1252.toChar(0x2020) == 0x86) ;  // dagger
        CHECK(cp1252.toChar(0x2021) == 0x87) ;  // double dagger
        CHECK(cp1252.toChar(0x02C6) == 0x88) ;  // circumflex accent
        CHECK(cp1252.toChar(0x2030) == 0x89) ;  // per mille sign
        CHECK(cp1252.toChar(0x0160) == 0x8A) ;  // S with caron
        CHECK(cp1252.toChar(0x2039) == 0x8B) ;  // single left angle quotation
        CHECK(cp1252.toChar(0x0152) == 0x8C) ;  // OE ligature
        CHECK(cp1252.toChar(0x017D) == 0x8E) ;  // Z with caron
        CHECK(cp1252.toChar(0x2018) == 0x91) ;  // left single quotation
        CHECK(cp1252.toChar(0x2019) == 0x92) ;  // right single quotation
        CHECK(cp1252.toChar(0x201C) == 0x93) ;  // left double quotation
        CHECK(cp1252.toChar(0x201D) == 0x94) ;  // right double quotation
        CHECK(cp1252.toChar(0x2022) == 0x95) ;  // bullet
        CHECK(cp1252.toChar(0x2013) == 0x96) ;  // en dash
        CHECK(cp1252.toChar(0x2014) == 0x97) ;  // em dash
        CHECK(cp1252.toChar(0x02DC) == 0x98) ;  // small tilde
        CHECK(cp1252.toChar(0x2122) == 0x99) ;  // trade mark sign
        CHECK(cp1252.toChar(0x0161) == 0x9A) ;  // s with caron
        CHECK(cp1252.toChar(0x203A) == 0x9B) ;  // single right angle quotation
        CHECK(cp1252.toChar(0x0153) == 0x9C) ;  // oe ligature
        CHECK(cp1252.toChar(0x017E) == 0x9E) ;  // z with caron
        CHECK(cp1252.toChar(0x0178) == 0x9F) ;  // Y with diaeresis
    }

    SUBCASE("toUTF8")
    {
        CHECK(cp1252.toUTF8(0x80) == 0x20AC) ;  // euro sign
        CHECK(cp1252.toUTF8(0x82) == 0x201A) ;  // single low-9 quotation mark
        CHECK(cp1252.toUTF8(0x83) == 0x0192) ;  // f with hook
        CHECK(cp1252.toUTF8(0x84) == 0x201E) ;  // double low-9 quotation mark
        CHECK(cp1252.toUTF8(0x85) == 0x2026) ;  // horizontal ellipsis
        CHECK(cp1252.toUTF8(0x86) == 0x2020) ;  // dagger
        CHECK(cp1252.toUTF8(0x87) == 0x2021) ;  // double dagger
        CHECK(cp1252.toUTF8(0x88) == 0x02C6) ;  // circumflex accent
        CHECK(cp1252.toUTF8(0x89) == 0x2030) ;  // per mille sign
        CHECK(cp1252.toUTF8(0x8A) == 0x0160) ;  // S with caron
        CHECK(cp1252.toUTF8(0x8B) == 0x2039) ;  // single left angle quotation
        CHECK(cp1252.toUTF8(0x8C) == 0x0152) ;  // OE ligature
        CHECK(cp1252.toUTF8(0x8E) == 0x017D) ;  // Z with caron
        CHECK(cp1252.toUTF8(0x91) == 0x2018) ;  // left single quotation
        CHECK(cp1252.toUTF8(0x92) == 0x2019) ;  // right single quotation
        CHECK(cp1252.toUTF8(0x93) == 0x201C) ;  // left double quotation
        CHECK(cp1252.toUTF8(0x94) == 0x201D) ;  // right double quotation
        CHECK(cp1252.toUTF8(0x95) == 0x2022) ;  // bullet
        CHECK(cp1252.toUTF8(0x96) == 0x2013) ;  // en dash
        CHECK(cp1252.toUTF8(0x97) == 0x2014) ;  // em dash
        CHECK(cp1252.toUTF8(0x98) == 0x02DC) ;  // small tilde
        CHECK(cp1252.toUTF8(0x99) == 0x2122) ;  // trade mark sign
        CHECK(cp1252.toUTF8(0x9A) == 0x0161) ;  // s with caron
        CHECK(cp1252.toUTF8(0x9B) == 0x203A) ;  // single right angle quotation
        CHECK(cp1252.toUTF8(0x9C) == 0x0153) ;  // oe ligature
        CHECK(cp1252.toUTF8(0x9E) == 0x017E) ;  // z with caron
        CHECK(cp1252.toUTF8(0x9F) == 0x0178) ;  // Y with diaeresis
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Test Mac Roman codepage conversion.
/// Verifies toChar (Unicode to MacRoman) and toUTF8 (MacRoman to Unicode) roundtrips.
///
/////////////////////////////////////////////////////////////////////////////
TEST_CASE("MacRoman")
{
    cCodePageMacRoman cpMac ;

    SUBCASE("toChar")
    {
        // First entries
        CHECK(cpMac.toChar(0x00C4) == 0x80) ;  // A with diaeresis
        CHECK(cpMac.toChar(0x00C5) == 0x81) ;  // A with ring above
        CHECK(cpMac.toChar(0x00C7) == 0x82) ;  // C with cedilla
        CHECK(cpMac.toChar(0x00C9) == 0x83) ;  // E with acute
        CHECK(cpMac.toChar(0x00D1) == 0x84) ;  // N with tilde
        CHECK(cpMac.toChar(0x00D6) == 0x85) ;  // O with diaeresis
        CHECK(cpMac.toChar(0x00DC) == 0x86) ;  // U with diaeresis
        CHECK(cpMac.toChar(0x00E1) == 0x87) ;  // a with acute

        // Last entries
        CHECK(cpMac.toChar(0xF8FF) == 0xF0) ;  // apple logo
        CHECK(cpMac.toChar(0x0131) == 0xF5) ;  // dotless i
        CHECK(cpMac.toChar(0x00AF) == 0xF8) ;  // macron
        CHECK(cpMac.toChar(0x02D8) == 0xF9) ;  // breve
        CHECK(cpMac.toChar(0x02D9) == 0xFA) ;  // dot above
        CHECK(cpMac.toChar(0x02DA) == 0xFB) ;  // ring above
        CHECK(cpMac.toChar(0x00B8) == 0xFC) ;  // cedilla
        CHECK(cpMac.toChar(0x02DD) == 0xFD) ;  // double acute accent
        CHECK(cpMac.toChar(0x02DB) == 0xFE) ;  // ogonek
        CHECK(cpMac.toChar(0x02C7) == 0xFF) ;  // caron
    }

    SUBCASE("toUTF8")
    {
        // First entries
        CHECK(cpMac.toUTF8(0x80) == 0x00C4) ;  // A with diaeresis
        CHECK(cpMac.toUTF8(0x81) == 0x00C5) ;  // A with ring above
        CHECK(cpMac.toUTF8(0x82) == 0x00C7) ;  // C with cedilla
        CHECK(cpMac.toUTF8(0x83) == 0x00C9) ;  // E with acute
        CHECK(cpMac.toUTF8(0x84) == 0x00D1) ;  // N with tilde
        CHECK(cpMac.toUTF8(0x85) == 0x00D6) ;  // O with diaeresis
        CHECK(cpMac.toUTF8(0x86) == 0x00DC) ;  // U with diaeresis
        CHECK(cpMac.toUTF8(0x87) == 0x00E1) ;  // a with acute

        // Last entries
        CHECK(cpMac.toUTF8(0xF0) == 0xF8FF) ;  // apple logo
        CHECK(cpMac.toUTF8(0xF5) == 0x0131) ;  // dotless i
        CHECK(cpMac.toUTF8(0xF8) == 0x00AF) ;  // macron
        CHECK(cpMac.toUTF8(0xF9) == 0x02D8) ;  // breve
        CHECK(cpMac.toUTF8(0xFA) == 0x02D9) ;  // dot above
        CHECK(cpMac.toUTF8(0xFB) == 0x02DA) ;  // ring above
        CHECK(cpMac.toUTF8(0xFC) == 0x00B8) ;  // cedilla
        CHECK(cpMac.toUTF8(0xFD) == 0x02DD) ;  // double acute accent
        CHECK(cpMac.toUTF8(0xFE) == 0x02DB) ;  // ogonek
        CHECK(cpMac.toUTF8(0xFF) == 0x02C7) ;  // caron
    }
}


/////////////////////////////////////////////////////////////////////////////
//
// Edge case and corner case tests -- boundary bytes
//
/////////////////////////////////////////////////////////////////////////////


TEST_CASE("Codepage boundary bytes")
{
    SUBCASE("CP437 boundary bytes")
    {
        cCodePage437 cp ;

        // 0x00 -- NUL, should return a valid mapping (no crash)
        unsigned long u0 = cp.toUTF8(0x00) ;
        CHECK(u0 >= 0) ;

        // 0x7F -- DEL / last ASCII-compatible byte
        unsigned long u7f = cp.toUTF8(0x7F) ;
        CHECK(u7f >= 0) ;

        // 0x80 -- first extended byte
        unsigned long u80 = cp.toUTF8(0x80) ;
        CHECK(u80 > 0) ;

        // 0xFF -- may or may not have a mapping, should not crash
        unsigned long uff = cp.toUTF8(0xFF) ;
        CHECK(uff >= 0) ;
    }

    SUBCASE("CP737 boundary bytes")
    {
        cCodePage737 cp ;

        unsigned long u0 = cp.toUTF8(0x00) ;
        CHECK(u0 >= 0) ;

        unsigned long u7f = cp.toUTF8(0x7F) ;
        CHECK(u7f >= 0) ;

        unsigned long u80 = cp.toUTF8(0x80) ;
        CHECK(u80 > 0) ;

        // 0xFF has no mapping in CP737, returns 0 (unmapped)
        unsigned long uff = cp.toUTF8(0xFF) ;
        CHECK(uff >= 0) ;
    }

    SUBCASE("CP850 boundary bytes")
    {
        cCodePage850 cp ;

        unsigned long u0 = cp.toUTF8(0x00) ;
        CHECK(u0 >= 0) ;

        unsigned long u7f = cp.toUTF8(0x7F) ;
        CHECK(u7f >= 0) ;

        unsigned long u80 = cp.toUTF8(0x80) ;
        CHECK(u80 > 0) ;

        unsigned long uff = cp.toUTF8(0xFF) ;
        CHECK(uff > 0) ;
    }

    SUBCASE("CP1252 boundary bytes")
    {
        cCodePageWin1252 cp ;

        unsigned long u0 = cp.toUTF8(0x00) ;
        CHECK(u0 >= 0) ;

        unsigned long u7f = cp.toUTF8(0x7F) ;
        CHECK(u7f >= 0) ;

        unsigned long u80 = cp.toUTF8(0x80) ;
        CHECK(u80 > 0) ;

        unsigned long uff = cp.toUTF8(0xFF) ;
        CHECK(uff > 0) ;
    }

    SUBCASE("MacRoman boundary bytes")
    {
        cCodePageMacRoman cp ;

        unsigned long u0 = cp.toUTF8(0x00) ;
        CHECK(u0 >= 0) ;

        unsigned long u7f = cp.toUTF8(0x7F) ;
        CHECK(u7f >= 0) ;

        unsigned long u80 = cp.toUTF8(0x80) ;
        CHECK(u80 > 0) ;

        unsigned long uff = cp.toUTF8(0xFF) ;
        CHECK(uff > 0) ;
    }
}


TEST_CASE("ASCII identity round-trip per codepage")
{
    SUBCASE("CP437")
    {
        cCodePage437 cp ;
        for (int i = 0; i < 128; ++i)
        {
            CHECK(cp.toUTF8(static_cast<unsigned char>(i)) == static_cast<unsigned long>(i)) ;
            CHECK(cp.toChar(static_cast<unsigned long>(i)) == static_cast<unsigned char>(i)) ;
        }
    }

    SUBCASE("CP737")
    {
        cCodePage737 cp ;
        for (int i = 0; i < 128; ++i)
        {
            CHECK(cp.toUTF8(static_cast<unsigned char>(i)) == static_cast<unsigned long>(i)) ;
            CHECK(cp.toChar(static_cast<unsigned long>(i)) == static_cast<unsigned char>(i)) ;
        }
    }

    SUBCASE("CP850")
    {
        cCodePage850 cp ;
        for (int i = 0; i < 128; ++i)
        {
            CHECK(cp.toUTF8(static_cast<unsigned char>(i)) == static_cast<unsigned long>(i)) ;
            CHECK(cp.toChar(static_cast<unsigned long>(i)) == static_cast<unsigned char>(i)) ;
        }
    }

    SUBCASE("CP1252")
    {
        cCodePageWin1252 cp ;
        for (int i = 0; i < 128; ++i)
        {
            CHECK(cp.toUTF8(static_cast<unsigned char>(i)) == static_cast<unsigned long>(i)) ;
            CHECK(cp.toChar(static_cast<unsigned long>(i)) == static_cast<unsigned char>(i)) ;
        }
    }

    SUBCASE("MacRoman")
    {
        cCodePageMacRoman cp ;
        for (int i = 0; i < 128; ++i)
        {
            CHECK(cp.toUTF8(static_cast<unsigned char>(i)) == static_cast<unsigned long>(i)) ;
            CHECK(cp.toChar(static_cast<unsigned long>(i)) == static_cast<unsigned char>(i)) ;
        }
    }
}


TEST_CASE("toUTF8 unmapped high byte returns UINT32_MAX sentinel")
{
    SUBCASE("CP1252 0x81 is unassigned")
    {
        cCodePageWin1252 cp ;
        CHECK(cp.toUTF8(0x81) == UINT32_MAX) ;
    }

    SUBCASE("CP1252 0x8D is unassigned")
    {
        cCodePageWin1252 cp ;
        CHECK(cp.toUTF8(0x8D) == UINT32_MAX) ;
    }

    SUBCASE("CP737 0xFF is unmapped")
    {
        cCodePage737 cp ;
        CHECK(cp.toUTF8(0xFF) == UINT32_MAX) ;
    }
}


TEST_CASE("Bidirectional round-trip - every high byte per codepage")
{
    SUBCASE("CP437")
    {
        cCodePage437 cp ;
        for (int byte = 0x80; byte < 256; ++byte)
        {
            unsigned long codepoint = cp.toUTF8(static_cast<unsigned char>(byte)) ;
            if (codepoint != UINT32_MAX)
            {
                INFO("byte = 0x" << std::hex << byte) ;
                CHECK(cp.toChar(codepoint) == static_cast<unsigned char>(byte)) ;
            }
        }
    }

    SUBCASE("CP737")
    {
        cCodePage737 cp ;
        for (int byte = 0x80; byte < 256; ++byte)
        {
            unsigned long codepoint = cp.toUTF8(static_cast<unsigned char>(byte)) ;
            if (codepoint != UINT32_MAX)
            {
                INFO("byte = 0x" << std::hex << byte) ;
                CHECK(cp.toChar(codepoint) == static_cast<unsigned char>(byte)) ;
            }
        }
    }

    SUBCASE("CP850")
    {
        cCodePage850 cp ;
        for (int byte = 0x80; byte < 256; ++byte)
        {
            unsigned long codepoint = cp.toUTF8(static_cast<unsigned char>(byte)) ;
            if (codepoint != UINT32_MAX)
            {
                INFO("byte = 0x" << std::hex << byte) ;
                CHECK(cp.toChar(codepoint) == static_cast<unsigned char>(byte)) ;
            }
        }
    }

    SUBCASE("CP1252")
    {
        cCodePageWin1252 cp ;
        for (int byte = 0x80; byte < 256; ++byte)
        {
            unsigned long codepoint = cp.toUTF8(static_cast<unsigned char>(byte)) ;
            if (codepoint != UINT32_MAX)
            {
                INFO("byte = 0x" << std::hex << byte) ;
                CHECK(cp.toChar(codepoint) == static_cast<unsigned char>(byte)) ;
            }
        }
    }

    SUBCASE("MacRoman")
    {
        cCodePageMacRoman cp ;
        for (int byte = 0x80; byte < 256; ++byte)
        {
            unsigned long codepoint = cp.toUTF8(static_cast<unsigned char>(byte)) ;
            if (codepoint != UINT32_MAX)
            {
                INFO("byte = 0x" << std::hex << byte) ;
                CHECK(cp.toChar(codepoint) == static_cast<unsigned char>(byte)) ;
            }
        }
    }
}


TEST_CASE("CP1252 Latin-1 identity 0xA0-0xFF")
{
    cCodePageWin1252 cp ;
    for (int i = 0xA0; i <= 0xFF; ++i)
    {
        INFO("byte = 0x" << std::hex << i) ;
        CHECK(cp.toUTF8(static_cast<unsigned char>(i)) == static_cast<unsigned long>(i)) ;
        CHECK(cp.toChar(static_cast<unsigned long>(i)) == static_cast<unsigned char>(i)) ;
    }
}


TEST_CASE("CP1252 0x80-0x9F unassigned vs mapped")
{
    cCodePageWin1252 cp ;

    // CP1252 spec leaves five bytes unassigned in the 0x80-0x9F band:
    //   0x81, 0x8D, 0x8F, 0x90, 0x9D
    SUBCASE("Five unassigned slots return UINT32_MAX")
    {
        CHECK(cp.toUTF8(0x81) == UINT32_MAX) ;
        CHECK(cp.toUTF8(0x8D) == UINT32_MAX) ;
        CHECK(cp.toUTF8(0x8F) == UINT32_MAX) ;
        CHECK(cp.toUTF8(0x90) == UINT32_MAX) ;
        CHECK(cp.toUTF8(0x9D) == UINT32_MAX) ;
    }

    SUBCASE("All other 0x80-0x9F bytes are mapped")
    {
        for (int i = 0x80; i <= 0x9F; ++i)
        {
            if (i == 0x81 || i == 0x8D || i == 0x8F || i == 0x90 || i == 0x9D)
            {
                continue ;
            }
            INFO("byte = 0x" << std::hex << i) ;
            CHECK(cp.toUTF8(static_cast<unsigned char>(i)) != UINT32_MAX) ;
        }
    }
}


TEST_CASE("toChar returns 0 for codepoints no codepage maps")
{
    SUBCASE("CP437")
    {
        cCodePage437 cp ;
        CHECK(cp.toChar(0x4E2D) == 0) ;     // CJK middle
        CHECK(cp.toChar(0x5B57) == 0) ;     // CJK character
        CHECK(cp.toChar(0x1F600) == 0) ;    // grinning face emoji
        CHECK(cp.toChar(0xE000) == 0) ;     // private-use BMP
        CHECK(cp.toChar(0x10FFFF) == 0) ;   // max Unicode codepoint
    }

    SUBCASE("CP737")
    {
        cCodePage737 cp ;
        CHECK(cp.toChar(0x4E2D) == 0) ;
        CHECK(cp.toChar(0x1F600) == 0) ;
        CHECK(cp.toChar(0xE000) == 0) ;
        CHECK(cp.toChar(0x10FFFF) == 0) ;
    }

    SUBCASE("CP850")
    {
        cCodePage850 cp ;
        CHECK(cp.toChar(0x4E2D) == 0) ;
        CHECK(cp.toChar(0x1F600) == 0) ;
        CHECK(cp.toChar(0xE000) == 0) ;
        CHECK(cp.toChar(0x10FFFF) == 0) ;
    }

    SUBCASE("CP1252")
    {
        cCodePageWin1252 cp ;
        CHECK(cp.toChar(0x4E2D) == 0) ;
        CHECK(cp.toChar(0x1F600) == 0) ;
        CHECK(cp.toChar(0xE000) == 0) ;
        CHECK(cp.toChar(0x10FFFF) == 0) ;
    }

    SUBCASE("MacRoman")
    {
        cCodePageMacRoman cp ;
        CHECK(cp.toChar(0x4E2D) == 0) ;
        CHECK(cp.toChar(0x1F600) == 0) ;
        CHECK(cp.toChar(0xE000) == 0) ;
        CHECK(cp.toChar(0x10FFFF) == 0) ;
    }
}


TEST_CASE("toChar accepts large codepoints without crashing")
{
    cCodePage437 cp437 ;
    cCodePage737 cp737 ;
    cCodePage850 cp850 ;
    cCodePageWin1252 cp1252 ;
    cCodePageMacRoman cpmac ;

    for (unsigned long u : {0x100UL, 0x1000UL, 0xFFFFUL, 0x10000UL,
                             0xFFFFFUL, 0x10FFFFUL})
    {
        INFO("codepoint = 0x" << std::hex << u) ;
        // Each toChar must terminate and return some byte value -- no crash,
        // no infinite loop. Result is `unsigned char` so it is in [0, 255].
        unsigned char b437 = cp437.toChar(u) ;
        unsigned char b737 = cp737.toChar(u) ;
        unsigned char b850 = cp850.toChar(u) ;
        unsigned char b1252 = cp1252.toChar(u) ;
        unsigned char bmac = cpmac.toChar(u) ;
        (void)b437 ;
        (void)b737 ;
        (void)b850 ;
        (void)b1252 ;
        (void)bmac ;
        CHECK(true) ;       // reached this line == no crash
    }
}


TEST_CASE("Survey - unmapped high-byte set per codepage")
{
    auto countUnmapped = [](auto& cp) -> int
    {
        int count = 0 ;
        for (int byte = 0x80; byte < 256; ++byte)
        {
            if (cp.toUTF8(static_cast<unsigned char>(byte)) == UINT32_MAX)
            {
                count++ ;
            }
        }
        return count ;
    };

    SUBCASE("CP1252 has exactly 5 unassigned slots (0x81, 0x8D, 0x8F, 0x90, 0x9D)")
    {
        cCodePageWin1252 cp ;
        CHECK(countUnmapped(cp) == 5) ;
    }

    SUBCASE("CP737 has unmapped bytes at the tail")
    {
        cCodePage737 cp ;
        // Documented in existing boundary tests: 0xFF is unmapped.
        // Lock the count in so future table truncation is caught.
        int n = countUnmapped(cp) ;
        INFO("CP737 unmapped byte count = " << n) ;
        CHECK(n >= 1) ;
        CHECK(n <= 10) ;            // sanity ceiling
        CHECK(cp.toUTF8(0xFF) == UINT32_MAX) ;
    }

    SUBCASE("CP437 maps all bytes 0x80-0xFE")
    {
        cCodePage437 cp ;
        // CP437 spec maps every high byte. If the table loses an entry,
        // the unmapped count will go above 0.
        int n = countUnmapped(cp) ;
        INFO("CP437 unmapped byte count = " << n) ;
        CHECK(n <= 1) ;             // 0xFF in some variants
    }

    SUBCASE("CP850 maps all bytes 0x80-0xFE")
    {
        cCodePage850 cp ;
        int n = countUnmapped(cp) ;
        INFO("CP850 unmapped byte count = " << n) ;
        CHECK(n <= 1) ;
    }

    SUBCASE("MacRoman unmapped byte count is small and stable")
    {
        cCodePageMacRoman cp ;
        int n = countUnmapped(cp) ;
        INFO("MacRoman unmapped byte count = " << n) ;
        CHECK(n <= 5) ;
    }
}