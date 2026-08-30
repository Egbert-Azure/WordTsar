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

#include "doctest.h"

#include "src/tui/wordstartui/src/colorutils.h"

using namespace wordstartui;

/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Tests for the colorutils color-math and capability-detection helpers:
/// HSV/RGB conversion, RGB to xterm-256, RGB to 16-color, and terminal color
/// level detection.
///
/////////////////////////////////////////////////////////////////////////////

TEST_CASE("colorutils - HSV to RGB primaries")
{
    sHsv red;
    red.h = 0.0;
    red.s = 1.0;
    red.v = 1.0;
    CHECK(HsvToRgb(red) == MakeRgb(255, 0, 0));

    sHsv green;
    green.h = 120.0;
    green.s = 1.0;
    green.v = 1.0;
    CHECK(HsvToRgb(green) == MakeRgb(0, 255, 0));

    sHsv blue;
    blue.h = 240.0;
    blue.s = 1.0;
    blue.v = 1.0;
    CHECK(HsvToRgb(blue) == MakeRgb(0, 0, 255));

    sHsv black;
    black.h = 0.0;
    black.s = 0.0;
    black.v = 0.0;
    CHECK(HsvToRgb(black) == MakeRgb(0, 0, 0));

    sHsv white;
    white.h = 0.0;
    white.s = 0.0;
    white.v = 1.0;
    CHECK(HsvToRgb(white) == MakeRgb(255, 255, 255));
}

TEST_CASE("colorutils - RGB to HSV primaries")
{
    sHsv redHsv = RgbToHsv(MakeRgb(255, 0, 0));
    CHECK(redHsv.h == doctest::Approx(0.0));
    CHECK(redHsv.s == doctest::Approx(1.0));
    CHECK(redHsv.v == doctest::Approx(1.0));

    sHsv greenHsv = RgbToHsv(MakeRgb(0, 255, 0));
    CHECK(greenHsv.h == doctest::Approx(120.0));

    sHsv blueHsv = RgbToHsv(MakeRgb(0, 0, 255));
    CHECK(blueHsv.h == doctest::Approx(240.0));

    sHsv grayHsv = RgbToHsv(MakeRgb(128, 128, 128));
    CHECK(grayHsv.s == doctest::Approx(0.0));
}

TEST_CASE("colorutils - HSV round trip preserves saturated colors")
{
    sColor originals[4] = {
        MakeRgb(200, 40, 90),
        MakeRgb(10, 180, 220),
        MakeRgb(255, 128, 0),
        MakeRgb(64, 200, 32)
    };

    for (int loop = 0; loop < 4; ++loop)
    {
        sColor round = HsvToRgb(RgbToHsv(originals[loop]));
        CHECK(std::abs(static_cast<int>(round.r) - static_cast<int>(originals[loop].r)) <= 1);
        CHECK(std::abs(static_cast<int>(round.g) - static_cast<int>(originals[loop].g)) <= 1);
        CHECK(std::abs(static_cast<int>(round.b) - static_cast<int>(originals[loop].b)) <= 1);
    }
}

TEST_CASE("colorutils - RGB to xterm 256")
{
    CHECK(RgbTo256(MakeRgb(0, 0, 0)) == 16);
    CHECK(RgbTo256(MakeRgb(255, 255, 255)) == 231);
    CHECK(RgbTo256(MakeRgb(255, 0, 0)) == 196);
    CHECK(RgbTo256(MakeRgb(0, 255, 0)) == 46);
    CHECK(RgbTo256(MakeRgb(0, 0, 255)) == 21);
    // Mid gray is closer to the grayscale ramp than to the cube.
    CHECK(RgbTo256(MakeRgb(128, 128, 128)) == 244);
}

TEST_CASE("colorutils - Index256 to RGB")
{
    CHECK(Index256ToRgb(16) == MakeRgb(0, 0, 0));
    CHECK(Index256ToRgb(231) == MakeRgb(255, 255, 255));
    CHECK(Index256ToRgb(196) == MakeRgb(255, 0, 0));
    CHECK(Index256ToRgb(232) == MakeRgb(8, 8, 8));
    CHECK(Index256ToRgb(255) == MakeRgb(238, 238, 238));
    CHECK(Index256ToRgb(0) == MakeRgb(0, 0, 0));
}

TEST_CASE("colorutils - RGB to nearest 16-color")
{
    CHECK(RgbTo16(MakeRgb(0, 0, 0)) == COLOR_BLACK);
    CHECK(RgbTo16(MakeRgb(255, 255, 255)) == COLOR_WHITE);
    CHECK(RgbTo16(MakeRgb(180, 10, 10)) == COLOR_RED);
    CHECK(RgbTo16(MakeRgb(255, 90, 90)) == COLOR_LIGHT_RED);
    CHECK(RgbTo16(MakeRgb(10, 10, 160)) == COLOR_BLUE);
}

TEST_CASE("colorutils - ANSI color params per depth")
{
    // Truecolor emits the exact RGB.
    CHECK(AnsiColorParams(MakeRgb(10, 20, 30), true, COLOR_LEVEL_TRUECOLOR) == "38;2;10;20;30");
    CHECK(AnsiColorParams(MakeRgb(10, 20, 30), false, COLOR_LEVEL_TRUECOLOR) == "48;2;10;20;30");

    // 256-color downgrades through the cube.
    CHECK(AnsiColorParams(MakeRgb(255, 0, 0), true, COLOR_LEVEL_256) == "38;5;196");
    CHECK(AnsiColorParams(MakeRgb(255, 0, 0), false, COLOR_LEVEL_256) == "48;5;196");

    // 16-color downgrades to the nearest ANSI SGR number.
    CHECK(AnsiColorParams(MakeRgb(180, 10, 10), true, COLOR_LEVEL_16) == "31");
    CHECK(AnsiColorParams(MakeRgb(180, 10, 10), false, COLOR_LEVEL_16) == "41");
    CHECK(AnsiColorParams(MakeRgb(255, 255, 255), true, COLOR_LEVEL_16) == "97");

    // The terminal default is depth independent.
    CHECK(AnsiColorParams(MakeDefaultColor(), true, COLOR_LEVEL_TRUECOLOR) == "39");
    CHECK(AnsiColorParams(MakeDefaultColor(), false, COLOR_LEVEL_16) == "49");
}

TEST_CASE("colorutils - terminal color level detection")
{
    CHECK(DetectColorLevel("truecolor", "xterm", -1) == COLOR_LEVEL_TRUECOLOR);
    CHECK(DetectColorLevel("24bit", "xterm", -1) == COLOR_LEVEL_TRUECOLOR);
    CHECK(DetectColorLevel(nullptr, "xterm", 16777216) == COLOR_LEVEL_TRUECOLOR);
    CHECK(DetectColorLevel(nullptr, "xterm-256color", -1) == COLOR_LEVEL_256);
    CHECK(DetectColorLevel(nullptr, "xterm", 256) == COLOR_LEVEL_256);
    CHECK(DetectColorLevel(nullptr, "xterm", 8) == COLOR_LEVEL_16);
    CHECK(DetectColorLevel(nullptr, nullptr, -1) == COLOR_LEVEL_16);
}
