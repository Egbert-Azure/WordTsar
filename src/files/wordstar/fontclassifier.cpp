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
 * @class cFontClassifier
 *
 * @brief Keyword-based font classification by name heuristics.
 *
 * Implements the cFontClassifier class, which categorizes fonts by their
 * name into style families and properties. Used by the WordStar file handler
 * to map WordStar font family codes to appropriate modern system fonts.
 *
 * @section fontclass_algorithm Classification Algorithm
 * The classifier lowercases the font name and checks it against curated
 * keyword lists using substring matching. The first matching keyword
 * determines the font's category. Keyword lists are defined as static
 * vectors and cover hundreds of known font names.
 *
 * @section fontclass_categories Font Categories
 * - Style (eFontStyle): sans-serif, serif, script, decorative, monospace
 * - Proportionality: proportional (variable-width) vs. monospaced (fixed-width)
 * - Special types: math fonts (containing mathematical symbols) and
 *   symbol fonts (non-text symbol sets like Wingdings, Webdings)
 *
 * @section fontclass_keywords Keyword Lists
 * - kSansSerif: Helvetica, Arial, Verdana, Roboto, Calibri, etc.
 * - kSerif: Times, Georgia, Garamond, Palatino, Cambria, etc.
 * - kScript: Brush Script, Zapfino, Pacifico, Snell Roundhand, etc.
 * - kDecorative: Blackletter, Old English, Stencil, Comic, etc.
 * - kMonospace: Courier, Consolas, Menlo, Source Code, etc.
 * - kMath: STIX, Asana Math, Cambria Math, etc.
 * - kSymbol: Wingdings, Webdings, Zapf Dingbats, Symbol, etc.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 *
 * @see cFontClassifier Font classifier class
 * @see eFontStyle Font style enumeration
 * @see sFontProperties Classified font properties structure
 * @see cWordstarFile WordStar file handler using font classification
 */

#include "fontclassifier.h"
#include <algorithm>

//----------------------------------------------------------------------
// Global Keyword Lists (Expanded Without Redundancy)
//----------------------------------------------------------------------

// Keywords for sans-serif fonts.
static const std::vector<std::string> kSansSerif =
{
    "helvetica", "arial", "verdana", "tahoma", "calibri", "lucida sans", "trebuchet",
    "futura", "gill sans", "myriad", "avenir", "din", "proxima nova", "roboto",
    "open sans", "lato", "montserrat", "source sans", "noto sans", "quicksand", "pt sans",
    "exo", "poppins", "oxygen", "inter", "nunito", "overpass", "gotham", "univers",
    "droid sans", "effra", "cera pro", "nexa", "segoe ui", "museo sans",
    "sansation", "quantico", "titillium", "muli", "assistant", "karla",
    "ubuntu", "oswald", "circular", "gilroy", "heebo", "work sans", "rubik", "dm sans"
};

// Keywords for serif fonts (merging traditional serif and script).
static const std::vector<std::string> kSerif =
{
    "serif", "roman", "times", "georgia", "garamond", "palatino", "courier"
    "bookman", "minion", "caslon", "bodoni", "didot", "cambria", "hoefler", "constantia",
    "new century schoolbook", "bembo", "centaur", "perpetua", "goudy",
    "plantin", "janson", "cheltenham", "lora", "tisa", "miller", "rockwell",
    "baskerville", "clarendon", "nimbus", "granjon", "walbaum", "trajan",
    "fell", "old style", "cormorant", "calluna", "cochin", "seymour", "galliard",
    "sabon", "utopia", "iowan", "aakar"
};

// Keywords for decorative fonts.
static const std::vector<std::string> kDecorative =
{
    "decorative", "display", "blackletter", "old english", "chiller", "novel", "gothic",
    "floral", "art deco", "vintage", "rustic", "hand-drawn", "freaky", "extravagant",
    "circus", "stencil", "retro", "comic", "punk", "grunge", "horror", "baroque",
    "ornate", "quirky", "theatrical", "pop", "bizarre", "funky", "festive", "grotesque",
    "sideshow", "caricature", "parody", "whimsical", "fantasy", "spooky",
    "distressed", "experimental", "eccentric", "offbeat", "irregular", "artistic",
    "festival", "outrageous", "scary", "zany", "vandal", "screaming",
    "fanciful", "barbaric", "festoon", "sculptural", "cartoonish", "expressive",
    "impulsive", "jazzy", "iconoclastic", "freakshow", "dada", "surreal", "avant-garde",
    "graffiti", "neon", "urban"
};

// Keywords for monospaced fonts.
static const std::vector<std::string> kMono =
{
    "mono", 
    "courier", "fixed", "consolas", "lucida console", "monaco", "menlo",
    "source code pro", "inconsolata", "fira code", "anonymous pro",
    "cutive", "hack", "cousine",
    "pragmatapro"
};

// Keywords for math fonts.
static const std::vector<std::string> kMath =
{
    "math", "stix", "xits", "mt", "detexify", "numerals"
};

// Keywords for symbol fonts.
static const std::vector<std::string> kSymbol =
{
    "symbol", "dingbats", "wingdings", "emoji", "pictograph",
    "icon", "marlett", "fontawesome", "icomoon", "glyph", "entypo",
    "logotype", "pictos", "sylfaen", "mystical", "zocial", "octicons", "fontello",
    "webdings"
};

// Keywords for script fonts.
static const std::vector<std::string> kScript =
{
    "script", "cursive", "handwriting", "brush", "chancery",
    "zapfino", "ballpark", "signature", "informal", "amatic", "calligraphy",
    "penmanship"
};

//----------------------------------------------------------------------
// cFontClassifier Method Definitions
//----------------------------------------------------------------------

sFontProperties cFontClassifier::classify(const std::string & fontName)
{
    sFontProperties props;
    props.style = classifyStyle(fontName);
    props.proportional = isProportional(fontName);
    props.math = checkMath(fontName);
    props.symbol = checkSymbol(fontName);
    return props;
}

eFontStyle cFontClassifier::classifyStyle(const std::string & fontName)
{
    std::string lower = fontName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Check sans-serif keywords.
    for (const auto & kw : kSansSerif)
    {
        if (lower.find(kw) != std::string::npos)
            return STYLE_SANS;
    }

    // Check serif keywords.
    for (const auto & kw : kSerif)
    {
        if (lower.find(kw) != std::string::npos)
            return STYLE_SERIF;
    }

    // Also check script keywords (merged into serif).
    for (const auto & kw : kScript)
    {
        if (lower.find(kw) != std::string::npos)
            return STYLE_SCRIPT;
    }

    return STYLE_UNKNOWN;
}

bool cFontClassifier::isProportional(const std::string & fontName)
{
    std::string lower = fontName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    for (const auto & kw : kMono)
    {
        if (lower.find(kw) != std::string::npos)
            return false;
    }
    return true;
}

bool cFontClassifier::checkMath(const std::string & fontName)
{
    std::string lower = fontName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto & kw : kMath)
    {
        if (lower.find(kw) != std::string::npos)
            return true;
    }
    return false;
}

bool cFontClassifier::checkSymbol(const std::string & fontName)
{
    std::string lower = fontName;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    for (const auto & kw : kSymbol)
    {
        if (lower.find(kw) != std::string::npos)
            return true;
    }
    return false;
}
