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

#ifndef WORDTSAR_WSTUI_DIALOGS_H
#define WORDTSAR_WSTUI_DIALOGS_H

#include <string>
#include <vector>

class iWSDialogHost;
class cConfig;

namespace wsdialogs
{

// ---- Simple message / prompt dialogs ----

// Show a one-button message box.
void MessageBox(iWSDialogHost* host, const std::string& title, const std::string& message);

// Yes/No question; returns true for Yes.
bool YesNo(iWSDialogHost* host, const std::string& title, const std::string& question);

// Three-button choice; returns 0/1/2 for the buttons, -1 for cancel/escape.
int ThreeChoice(iWSDialogHost* host, const std::string& title, const std::string& message,
                const std::string& button0, const std::string& button1, const std::string& button2);

// Single-line text prompt. Returns true on OK (value updated), false on cancel.
bool InputBox(iWSDialogHost* host, const std::string& title, const std::string& prompt,
              std::string& value);

// File open/save browser. Returns true on OK (result = chosen path), false on cancel.
bool FileBrowser(iWSDialogHost* host, const std::string& title, bool saveMode,
                 const std::string& startPath, std::string& result);

// A scrollable multi-line viewer (used for print preview). Returns when closed.
void TextViewer(iWSDialogHost* host, const std::string& title,
                const std::vector<std::string>& lines);

// ---- Find / Replace ----

struct sFindOptions
{
    std::string text;
    bool wholeWord = false;
    bool ignoreCase = false;
    bool backward = false;
    bool wildcard = false;
    int scope = 0;          // 0 = next, 1 = global
    bool ok = false;        // true if the user pressed Find/OK
};

sFindOptions FindDialog(iWSDialogHost* host, const sFindOptions& initial);

struct sReplaceOptions
{
    std::string find;
    std::string replace;
    bool wholeWord = false;
    bool ignoreCase = false;
    bool backward = false;
    bool wildcard = false;
    bool dontAsk = false;
    int scope = 0;          // 0 = next, 1 = entire file, 2 = rest of file
    bool ok = false;
};

sReplaceOptions ReplaceDialog(iWSDialogHost* host, const sReplaceOptions& initial);

// ---- Page layout ----

struct sPageLayoutValues
{
    std::string top;
    std::string bottom;
    std::string left;
    std::string right;
    std::string oddOffset;
    std::string evenOffset;
    std::string headerMargin;
    std::string footerMargin;
    bool ok = false;
};

sPageLayoutValues PageLayoutDialog(iWSDialogHost* host, const sPageLayoutValues& initial,
                                   const std::string& unitSuffix);

// ---- Font ----

// Pick a font family + size. Returns true on OK (family/size updated).
bool SelectFontDialog(iWSDialogHost* host, const std::vector<std::string>& families,
                      const std::string& currentFamily, const std::string& currentSize,
                      std::string& family, std::string& size);

// Pick a character style to apply. Returns true on OK with selectedIndex set to
// the chosen entry (see the style list in SelectStyleDialog's implementation).
bool SelectStyleDialog(iWSDialogHost* host, int& selectedIndex);

// ---- Color ----

struct sColorResult
{
    int red = 0;
    int green = 0;
    int blue = 0;
    bool useDefault = false;
    bool ok = false;
};

sColorResult SelectColorDialog(iWSDialogHost* host, int red, int green, int blue);

// ---- Spell check ----

// Returns 0 = ignore, 1 = replace (selectedIndex set), 2 = add, -1 = cancel/stop.
int SpellCheckDialog(iWSDialogHost* host, const std::string& word,
                     const std::vector<std::string>& suggestions, int& selectedIndex);

// ---- System preferences ----

// Six-tab modal editing all cConfig settings. Returns true on OK (config
// edited in place), false on Cancel/Escape.
bool SystemPreferences(iWSDialogHost* host, cConfig& config);

}

#endif // WORDTSAR_WSTUI_DIALOGS_H
