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

#include "colorutils.h"

#include <cmath>
#include <string>

namespace wordstartui
{

// The six xterm 6x6x6 color-cube channel levels.
static const int kCubeLevels[6] = { 0, 95, 135, 175, 215, 255 };

// The 16 standard xterm system colors, in xterm index order (0-15).
static const sColor kSystem16[16] = {
    { false, 0, 0, 0 },       { false, 170, 0, 0 },
    { false, 0, 170, 0 },     { false, 170, 85, 0 },
    { false, 0, 0, 170 },     { false, 170, 0, 170 },
    { false, 0, 170, 170 },   { false, 170, 170, 170 },
    { false, 85, 85, 85 },    { false, 255, 85, 85 },
    { false, 85, 255, 85 },   { false, 255, 255, 85 },
    { false, 85, 85, 255 },   { false, 255, 85, 255 },
    { false, 85, 255, 255 },  { false, 255, 255, 255 }
};

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sColor& color [in] a 24-bit RGB color (isDefault ignored)
///
/// @return the color as hue (0-360), saturation and value (both 0-1)
///
/// @brief
/// Convert an RGB color to HSV for the saturation/value square and hue bar.
///
/////////////////////////////////////////////////////////////////////////////
sHsv RgbToHsv(const sColor& color)
{
    double r = static_cast<double>(color.r) / 255.0;
    double g = static_cast<double>(color.g) / 255.0;
    double b = static_cast<double>(color.b) / 255.0;

    double max = r;
    if (g > max)
    {
        max = g;
    }
    if (b > max)
    {
        max = b;
    }

    double min = r;
    if (g < min)
    {
        min = g;
    }
    if (b < min)
    {
        min = b;
    }

    double delta = max - min;

    sHsv hsv;
    hsv.h = 0.0;
    hsv.v = max;

    if (max <= 0.0)
    {
        hsv.s = 0.0;
    }
    else
    {
        hsv.s = delta / max;
    }

    if (delta <= 0.0)
    {
        return hsv;
    }

    double hue = 0.0;
    if (max == r)
    {
        hue = (g - b) / delta;
    }
    else if (max == g)
    {
        hue = 2.0 + (b - r) / delta;
    }
    else
    {
        hue = 4.0 + (r - g) / delta;
    }

    hue *= 60.0;
    if (hue < 0.0)
    {
        hue += 360.0;
    }

    hsv.h = hue;
    return hsv;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sHsv& hsv [in] hue (0-360), saturation and value (0-1)
///
/// @return the equivalent 24-bit RGB color
///
/// @brief
/// Convert an HSV color back to RGB. Inputs are normalized/clamped first.
///
/////////////////////////////////////////////////////////////////////////////
sColor HsvToRgb(const sHsv& hsv)
{
    double h = hsv.h;
    double s = hsv.s;
    double v = hsv.v;

    while (h >= 360.0)
    {
        h -= 360.0;
    }
    while (h < 0.0)
    {
        h += 360.0;
    }
    if (s < 0.0)
    {
        s = 0.0;
    }
    if (s > 1.0)
    {
        s = 1.0;
    }
    if (v < 0.0)
    {
        v = 0.0;
    }
    if (v > 1.0)
    {
        v = 1.0;
    }

    double c = v * s;
    double hp = h / 60.0;
    double x = c * (1.0 - std::fabs(std::fmod(hp, 2.0) - 1.0));
    double m = v - c;

    double r1 = 0.0;
    double g1 = 0.0;
    double b1 = 0.0;

    if (hp < 1.0)
    {
        r1 = c;
        g1 = x;
    }
    else if (hp < 2.0)
    {
        r1 = x;
        g1 = c;
    }
    else if (hp < 3.0)
    {
        g1 = c;
        b1 = x;
    }
    else if (hp < 4.0)
    {
        g1 = x;
        b1 = c;
    }
    else if (hp < 5.0)
    {
        r1 = x;
        b1 = c;
    }
    else
    {
        r1 = c;
        b1 = x;
    }

    uint8_t r = static_cast<uint8_t>((r1 + m) * 255.0 + 0.5);
    uint8_t g = static_cast<uint8_t>((g1 + m) * 255.0 + 0.5);
    uint8_t b = static_cast<uint8_t>((b1 + m) * 255.0 + 0.5);

    return MakeRgb(r, g, b);
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int value [in] one 0-255 channel value
///
/// @return the index (0-5) of the nearest color-cube level
///
/// @brief
/// Find which of the six xterm cube levels a channel is closest to.
///
/////////////////////////////////////////////////////////////////////////////
static int NearestCubeIndex(int value)
{
    int best = 0;
    int bestDistance = -1;

    for (int loop = 0; loop < 6; ++loop)
    {
        int diff = value - kCubeLevels[loop];
        if (diff < 0)
        {
            diff = -diff;
        }

        if ((bestDistance < 0) || (diff < bestDistance))
        {
            bestDistance = diff;
            best = loop;
        }
    }

    return best;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sColor& color [in] a 24-bit RGB color
///
/// @return the nearest xterm 256-color palette index (16-255)
///
/// @brief
/// Map an RGB color to the closest entry in the 6x6x6 color cube or the
/// grayscale ramp, whichever is nearer. Used to downgrade truecolor output to
/// 256-color terminals.
///
/////////////////////////////////////////////////////////////////////////////
int RgbTo256(const sColor& color)
{
    int ri = NearestCubeIndex(color.r);
    int gi = NearestCubeIndex(color.g);
    int bi = NearestCubeIndex(color.b);

    int cubeR = kCubeLevels[ri];
    int cubeG = kCubeLevels[gi];
    int cubeB = kCubeLevels[bi];

    long cubeDr = static_cast<long>(color.r) - cubeR;
    long cubeDg = static_cast<long>(color.g) - cubeG;
    long cubeDb = static_cast<long>(color.b) - cubeB;
    long cubeDistance = (cubeDr * cubeDr) + (cubeDg * cubeDg) + (cubeDb * cubeDb);

    int average = (static_cast<int>(color.r) + static_cast<int>(color.g) + static_cast<int>(color.b)) / 3;
    int grayStep = (average - 8 + 5) / 10;
    if (grayStep < 0)
    {
        grayStep = 0;
    }
    if (grayStep > 23)
    {
        grayStep = 23;
    }
    int grayValue = 8 + (grayStep * 10);

    long grayDr = static_cast<long>(color.r) - grayValue;
    long grayDg = static_cast<long>(color.g) - grayValue;
    long grayDb = static_cast<long>(color.b) - grayValue;
    long grayDistance = (grayDr * grayDr) + (grayDg * grayDg) + (grayDb * grayDb);

    if (grayDistance < cubeDistance)
    {
        return 232 + grayStep;
    }

    return 16 + (36 * ri) + (6 * gi) + bi;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  int index [in] an xterm 256-color palette index (0-255)
///
/// @return the RGB color of that palette entry
///
/// @brief
/// Convert an xterm 256-color index to RGB for the 256-color swatch grid.
///
/////////////////////////////////////////////////////////////////////////////
sColor Index256ToRgb(int index)
{
    if (index < 0)
    {
        index = 0;
    }
    if (index > 255)
    {
        index = 255;
    }

    if (index < 16)
    {
        return kSystem16[index];
    }

    if (index < 232)
    {
        int cube = index - 16;
        int ri = cube / 36;
        int gi = (cube / 6) % 6;
        int bi = cube % 6;

        return MakeRgb(static_cast<uint8_t>(kCubeLevels[ri]),
                       static_cast<uint8_t>(kCubeLevels[gi]),
                       static_cast<uint8_t>(kCubeLevels[bi]));
    }

    int gray = 8 + ((index - 232) * 10);
    return MakeRgb(static_cast<uint8_t>(gray), static_cast<uint8_t>(gray), static_cast<uint8_t>(gray));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sColor& color [in] a 24-bit RGB color
///
/// @return the nearest named 16-color palette entry
///
/// @brief
/// Map an RGB color to the closest of the 16 named ANSI colors by Euclidean
/// distance. Used to downgrade truecolor output to 16-color terminals.
///
/////////////////////////////////////////////////////////////////////////////
eColor RgbTo16(const sColor& color)
{
    eColor best = COLOR_BLACK;
    long bestDistance = -1;

    for (int index = COLOR_BLACK; index <= COLOR_WHITE; ++index)
    {
        eColor candidate = static_cast<eColor>(index);
        sColor rgb = MakePaletteColor(candidate);

        long dr = static_cast<long>(color.r) - static_cast<long>(rgb.r);
        long dg = static_cast<long>(color.g) - static_cast<long>(rgb.g);
        long db = static_cast<long>(color.b) - static_cast<long>(rgb.b);
        long distance = (dr * dr) + (dg * dg) + (db * db);

        if ((bestDistance < 0) || (distance < bestDistance))
        {
            bestDistance = distance;
            best = candidate;
        }
    }

    return best;
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  eColor color [in] a named 16-color palette entry
/// @param  bool foreground [in] true for a foreground SGR code
///
/// @return the ANSI SGR number (30-37 / 40-47 / 90-97 / 100-107)
///
/// @brief
/// Map a named 16-color entry to its ANSI SGR number for 16-color terminals.
///
/////////////////////////////////////////////////////////////////////////////
static int Ansi16Code(eColor color, bool foreground)
{
    int base = 30;
    if (foreground == false)
    {
        base = 40;
    }

    switch (color)
    {
        case COLOR_BLACK:         return base + 0;
        case COLOR_RED:           return base + 1;
        case COLOR_GREEN:         return base + 2;
        case COLOR_BROWN:         return base + 3;
        case COLOR_BLUE:          return base + 4;
        case COLOR_MAGENTA:       return base + 5;
        case COLOR_CYAN:          return base + 6;
        case COLOR_LIGHT_GRAY:    return base + 7;
        case COLOR_DARK_GRAY:     return base + 60 + 0;
        case COLOR_LIGHT_RED:     return base + 60 + 1;
        case COLOR_LIGHT_GREEN:   return base + 60 + 2;
        case COLOR_YELLOW:        return base + 60 + 3;
        case COLOR_LIGHT_BLUE:    return base + 60 + 4;
        case COLOR_LIGHT_MAGENTA: return base + 60 + 5;
        case COLOR_LIGHT_CYAN:    return base + 60 + 6;
        case COLOR_WHITE:         return base + 60 + 7;
        case COLOR_DEFAULT:
        default:                  return base + 9;
    }
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const sColor& color [in] the cell color to emit
/// @param  bool foreground [in] true for foreground, false for background
/// @param  eColorLevel level [in] the terminal's color depth
///
/// @return the SGR parameter fragment (no leading ';' or trailing 'm')
///
/// @brief
/// Build the SGR parameter fragment for one color at the terminal's supported
/// depth: 24-bit ("38;2;r;g;b"), 256-color ("38;5;n"), 16-color ("31"), or the
/// terminal default ("39"/"49"). Downgrades RGB via RgbTo256/RgbTo16 as needed.
///
/////////////////////////////////////////////////////////////////////////////
std::string AnsiColorParams(const sColor& color, bool foreground, eColorLevel level)
{
    if (color.isDefault == true)
    {
        if (foreground == true)
        {
            return "39";
        }
        return "49";
    }

    if (level == COLOR_LEVEL_TRUECOLOR)
    {
        std::string out;
        if (foreground == true)
        {
            out = "38;2;";
        }
        else
        {
            out = "48;2;";
        }
        out += std::to_string(static_cast<int>(color.r));
        out += ";";
        out += std::to_string(static_cast<int>(color.g));
        out += ";";
        out += std::to_string(static_cast<int>(color.b));
        return out;
    }

    if (level == COLOR_LEVEL_256)
    {
        std::string out;
        if (foreground == true)
        {
            out = "38;5;";
        }
        else
        {
            out = "48;5;";
        }
        out += std::to_string(RgbTo256(color));
        return out;
    }

    return std::to_string(Ansi16Code(RgbTo16(color), foreground));
}

/////////////////////////////////////////////////////////////////////////////
///
/// @param  const char* colorterm [in] the COLORTERM env var (may be null)
/// @param  const char* term [in] the TERM env var (may be null)
/// @param  int tputColors [in] terminfo max_colors, or -1 if unknown
///
/// @return the color level the terminal supports
///
/// @brief
/// Decide whether the terminal supports truecolor, 256 colors, or only 16.
/// Kept as a pure function of its inputs so it can be unit tested without a
/// live terminal.
///
/////////////////////////////////////////////////////////////////////////////
eColorLevel DetectColorLevel(const char* colorterm, const char* term, int tputColors)
{
    std::string ct;
    if (colorterm != nullptr)
    {
        ct = colorterm;
    }

    std::string tm;
    if (term != nullptr)
    {
        tm = term;
    }

    if ((ct.find("truecolor") != std::string::npos) || (ct.find("24bit") != std::string::npos))
    {
        return COLOR_LEVEL_TRUECOLOR;
    }

    if (tputColors >= 16777216)
    {
        return COLOR_LEVEL_TRUECOLOR;
    }

    if ((tputColors >= 256) || (tm.find("256color") != std::string::npos) || (tm.find("256") != std::string::npos))
    {
        return COLOR_LEVEL_256;
    }

    return COLOR_LEVEL_16;
}

}
