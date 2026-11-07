//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2026 Egbert H. Schroeer
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

#include "src/input/macroscript.h"

#include <cctype>
#include <map>

#include "src/core/codepage/cp437.h"
#include "src/input/wordtsarinput.h"


namespace
{

const char REFUSED[] = "Not supported in WordTsar: ";
const char MODERN_LINE[] = "REM Modern";


std::string Lower(const std::string& text)
{
    std::string lower = text;
    for (char& c : lower)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}


std::string Trim(const std::string& text)
{
    const char* blanks = " \t\r\n";
    size_t first = text.find_first_not_of(blanks);
    if (first == std::string::npos)
    {
        return std::string();
    }
    size_t last = text.find_last_not_of(blanks);
    return text.substr(first, last - first + 1);
}


bool IsIdentifier(const std::string& text)
{
    if ((text.empty() == true) || (isalpha(static_cast<unsigned char>(text[0])) == 0))
    {
        return false;
    }
    for (char c : text)
    {
        if ((isalnum(static_cast<unsigned char>(c)) == 0) && (c != '_'))
        {
            return false;
        }
    }
    return true;
}


bool IsValidUtf8(const std::string& text)
{
    size_t pos = 0;
    while (pos < text.size())
    {
        unsigned char lead = static_cast<unsigned char>(text[pos]);
        size_t length = 1;
        if (lead >= 0x80U)
        {
            if ((lead & 0xE0U) == 0xC0U)
            {
                length = 2;
            }
            else if ((lead & 0xF0U) == 0xE0U)
            {
                length = 3;
            }
            else if ((lead & 0xF8U) == 0xF0U)
            {
                length = 4;
            }
            else
            {
                return false;
            }
        }
        if ((pos + length) > text.size())
        {
            return false;
        }
        for (size_t i = 1; i < length; i++)
        {
            if ((static_cast<unsigned char>(text[pos + i]) & 0xC0U) != 0x80U)
            {
                return false;
            }
        }
        pos += length;
    }
    return true;
}


std::string EncodeUtf8(unsigned long cp)
{
    std::string text;
    if (cp < 0x80UL)
    {
        text += static_cast<char>(cp);
    }
    else if (cp < 0x800UL)
    {
        text += static_cast<char>(0xC0UL | (cp >> 6));
        text += static_cast<char>(0x80UL | (cp & 0x3FUL));
    }
    else if (cp < 0x10000UL)
    {
        text += static_cast<char>(0xE0UL | (cp >> 12));
        text += static_cast<char>(0x80UL | ((cp >> 6) & 0x3FUL));
        text += static_cast<char>(0x80UL | (cp & 0x3FUL));
    }
    else
    {
        text += static_cast<char>(0xF0UL | (cp >> 18));
        text += static_cast<char>(0x80UL | ((cp >> 12) & 0x3FUL));
        text += static_cast<char>(0x80UL | ((cp >> 6) & 0x3FUL));
        text += static_cast<char>(0x80UL | (cp & 0x3FUL));
    }
    return text;
}


// Code page 437 text as UTF-8.
std::string FromCodePage437(const std::string& text)
{
    static cCodePage437 codePage;
    std::string result;
    for (char c : text)
    {
        unsigned long cp = codePage.toUTF8(static_cast<unsigned char>(c));
        if (cp == UINT32_MAX)
        {
            cp = 0xFFFDUL;
        }
        result += EncodeUtf8(cp);
    }
    return result;
}


// True if the first line after Sub MAIN is REM Modern.
bool IsModernSource(const std::vector<std::string>& lines)
{
    bool inMain = false;
    for (const std::string& line : lines)
    {
        std::string text = Trim(line);
        if (text.empty() == true)
        {
            continue;
        }
        if (inMain == true)
        {
            return text == MODERN_LINE;
        }
        if (Lower(text) != "sub main")
        {
            return false;
        }
        inMain = true;
    }
    return false;
}


// Removes /* */ comments; inComment carries an open comment to the next line.
std::string StripBlockComments(const std::string& line, bool& inComment)
{
    std::string text;
    char quote = '\0';
    size_t pos = 0;
    while (pos < line.size())
    {
        if (inComment == true)
        {
            size_t end = line.find("*/", pos);
            if (end == std::string::npos)
            {
                return text;
            }
            inComment = false;
            pos = end + 2;
            text += ' ';
            continue;
        }
        char c = line[pos];
        if (quote != '\0')
        {
            if (c == quote)
            {
                quote = '\0';
            }
        }
        else if (c == '"')
        {
            quote = c;
        }
        else if ((c == '/') && ((pos + 1) < line.size()) && (line[pos + 1] == '*'))
        {
            inComment = true;
            pos += 2;
            continue;
        }
        text += c;
        pos++;
    }
    return text;
}


struct sPart
{
    std::string text;
    bool colon = false;     // ended by ':'
};


/////////////////////////////////////////////////////////////////////////////
///
/// @param  line  [in]  one source line without block comments
/// @param  parts [out] the statements of the line
///
/// @return false on a string without its closing delimiter
///
/// @brief
/// Statements are separated by ':' outside strings and parentheses. An
/// apostrophe starts a string inside parentheses and after PlayMacro or
/// ChainMacro; elsewhere it starts a comment.
///
/////////////////////////////////////////////////////////////////////////////
bool SplitStatements(const std::string& line, std::vector<sPart>& parts)
{
    sPart current;
    char quote = '\0';
    int depth = 0;
    for (size_t i = 0; i < line.size(); i++)
    {
        char c = line[i];
        if (quote != '\0')
        {
            current.text += c;
            if (c == quote)
            {
                if (((i + 1) < line.size()) && (line[i + 1] == quote))
                {
                    current.text += c;
                    i++;
                }
                else
                {
                    quote = '\0';
                }
            }
            continue;
        }
        if (c == '\'')
        {
            std::string word = Lower(Trim(current.text));
            if ((depth == 0) && (word != "playmacro") && (word != "chainmacro"))
            {
                break;
            }
        }
        if ((c == '"') || (c == '\''))
        {
            quote = c;
            current.text += c;
            continue;
        }
        if (c == '(')
        {
            depth++;
        }
        else if ((c == ')') && (depth > 0))
        {
            depth--;
        }
        else if ((c == ':') && (depth == 0))
        {
            current.colon = true;
            parts.push_back(current);
            current = sPart();
            continue;
        }
        current.text += c;
    }
    if (quote != '\0')
    {
        return false;
    }
    parts.push_back(current);
    return true;
}


// One quoted string at the start of text; '' or "" inside stands for one
// delimiter. rest receives the text after it.
bool QuotedString(const std::string& text, std::string& value, char& quote, std::string& rest)
{
    std::string trimmed = Trim(text);
    if ((trimmed.empty() == true) || ((trimmed[0] != '"') && (trimmed[0] != '\'')))
    {
        return false;
    }
    quote = trimmed[0];
    value.clear();
    for (size_t pos = 1; pos < trimmed.size(); pos++)
    {
        if (trimmed[pos] == quote)
        {
            if (((pos + 1) < trimmed.size()) && (trimmed[pos + 1] == quote))
            {
                value += quote;
                pos++;
                continue;
            }
            rest = Trim(trimmed.substr(pos + 1));
            return true;
        }
        value += trimmed[pos];
    }
    return false;
}


// The quoted arguments of "", "(...)" or "(a, b)".
bool StringArguments(const std::string& text, std::vector<std::string>& values, std::string& quotes)
{
    std::string rest = Trim(text);
    values.clear();
    quotes.clear();
    if (rest.empty() == true)
    {
        return true;
    }
    if ((rest.front() != '(') || (rest.back() != ')'))
    {
        return false;
    }
    rest = Trim(rest.substr(1, rest.size() - 2));
    while (rest.empty() == false)
    {
        std::string value;
        char quote = '\0';
        std::string after;
        if (QuotedString(rest, value, quote, after) == false)
        {
            return false;
        }
        values.push_back(value);
        quotes += quote;
        if (after.empty() == true)
        {
            break;
        }
        if (after[0] != ',')
        {
            return false;
        }
        rest = Trim(after.substr(1));
        if (rest.empty() == true)
        {
            return false;
        }
    }
    return true;
}


// ON or OFF in parentheses.
bool OnOffArgument(const std::string& text, int& value)
{
    std::string rest = Trim(text);
    if ((rest.size() < 3) || (rest.front() != '(') || (rest.back() != ')'))
    {
        return false;
    }
    std::string arg = Lower(Trim(rest.substr(1, rest.size() - 2)));
    if ((arg == "on") || (arg == "1"))
    {
        value = 1;
        return true;
    }
    if ((arg == "off") || (arg == "0"))
    {
        value = 0;
        return true;
    }
    return false;
}


struct sSettingName
{
    const char* name;
    eMacroSetting setting;
};

const sSettingName SETTING_NAMES[] =
{
    { "autorestore", MACRO_SETTING_AUTO_RESTORE },
    { "insert",      MACRO_SETTING_INSERT },
    { "cmdtags",     MACRO_SETTING_CMD_TAGS },
    { "hidedots",    MACRO_SETTING_HIDE_DOTS },
    { "colmode",     MACRO_SETTING_IGNORED },
    { "colreplace",  MACRO_SETTING_IGNORED },
    { "colrepl",     MACRO_SETTING_IGNORED },
    { "wsquiet",     MACRO_SETTING_IGNORED }
};


std::string StatementName(eMacroStatement kind)
{
    switch (kind)
    {
        case MACRO_KEYS:             return "Key";
        case MACRO_PAUSE_INPUT:      return "PauseForInput";
        case MACRO_PAUSE_KEY:        return "PauseForKey";
        case MACRO_GOTO:             return "Goto";
        case MACRO_CALL:             return "Call";
        case MACRO_RETURN:           return "Return";
        case MACRO_END_SUB:          return "End Sub";
        case MACRO_STOP:             return "Stop";
        case MACRO_PLAY_MACRO:       return "PlayMacro";
        case MACRO_CHAIN_MACRO:      return "ChainMacro";
        case MACRO_SETTING:          return "setting";
        case MACRO_IF_EXCEPTION:     return "IfException";
        case MACRO_CLAUSE:           return "IfException";
        case MACRO_END_IF_EXCEPTION: return "End IfException";
    }
    return std::string();
}


// Name of the key after a chord prefix in a refusal: ^KO, ^Q{Del}.
std::string ChordName(char prefix, char c)
{
    std::string name = std::string("^") + static_cast<char>(toupper(static_cast<unsigned char>(prefix)));
    if (c == 0x7F)
    {
        return name + "{Del}";
    }
    if (c == ' ')
    {
        return name + "{Space}";
    }
    return name + static_cast<char>(toupper(static_cast<unsigned char>(c)));
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Follows played keys through the WordStar command tables.
///
/////////////////////////////////////////////////////////////////////////////
class cKeyChecker
{
public:
    // False with the refused key name in refused.
    bool Feed(const sKeyStroke& key, std::string& refused)
    {
        switch (mState)
        {
            case STATE_DIALOG:
            {
                if (mDialog.Feed(key) != cMacroDialog::DIALOG_OPEN)
                {
                    mState = STATE_TOP;
                }
                return true;
            }

            case STATE_SUBKEY:
            {
                mState = STATE_TOP;
                return true;
            }

            case STATE_CHORD:
            {
                char c = '\0';
                if ((key.kind == KEYSTROKE_SPECIAL) && (key.special == SPECIAL_ESCAPE))
                {
                    mState = STATE_TOP;
                    return true;
                }
                if ((key.kind == KEYSTROKE_SPECIAL) && (key.special == SPECIAL_DELETE))
                {
                    c = 0x7F;
                }
                else if ((key.kind == KEYSTROKE_SPECIAL) && (key.special == SPECIAL_BACKSPACE))
                {
                    c = 'h';
                }
                else if ((key.kind == KEYSTROKE_CHAR) && (key.alt == false))
                {
                    c = static_cast<char>(key.ch + '@');
                }
                else if ((key.kind == KEYSTROKE_TEXT) && (key.ctrl == false) && (key.alt == false) &&
                         (key.utf8.size() == 1) && (static_cast<unsigned char>(key.utf8[0]) < 0x80U))
                {
                    c = key.utf8[0];
                }
                else
                {
                    return Top(key, refused);
                }
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                mState = STATE_TOP;
                return Command(WordStarCommand(mChord, c), c, ChordName(mChord, c), refused);
            }

            case STATE_TOP:
            {
                return Top(key, refused);
            }
        }
        return true;
    }

    bool InDialog(void) const
    {
        return mState == STATE_DIALOG;
    }

    const std::string& DialogName(void) const
    {
        return mDialogName;
    }

    void Reset(void)
    {
        mState = STATE_TOP;
    }

private:
    enum eState { STATE_TOP, STATE_CHORD, STATE_SUBKEY, STATE_DIALOG };

    bool Top(const sKeyStroke& key, std::string& refused)
    {
        std::string name = KeyToKeyString(key);
        switch (key.kind)
        {
            case KEYSTROKE_CHAR:
            {
                char letter = static_cast<char>(tolower(key.ch + '@'));
                if (key.alt == true)
                {
                    return Command(WordStarCommand(WSCMD_ALT, key.ch), letter, name, refused);
                }
                return Command(WordStarCommand(WSCMD_CONTROL, key.ch), letter,
                               std::string("^") + static_cast<char>(key.ch + '@'), refused);
            }

            case KEYSTROKE_SPECIAL:
            {
                return Command(WordStarSpecialCommand(key.special), '\0', name, refused);
            }

            case KEYSTROKE_TEXT:
            {
                if (key.alt == true)
                {
                    char c = '\0';
                    if ((key.utf8.size() == 1) && (static_cast<unsigned char>(key.utf8[0]) < 0x80U))
                    {
                        c = static_cast<char>(tolower(static_cast<unsigned char>(key.utf8[0])));
                    }
                    return Command(WordStarCommand(WSCMD_ALT, c), c, name, refused);
                }
                if (key.ctrl == true)
                {
                    return Command(WSCMD_INVALID, '\0', name, refused);
                }
                return true;
            }

            case KEYSTROKE_PAUSE:
            {
                return true;
            }
        }
        return true;
    }

    bool Command(eWordStarCommand command, char letter, const std::string& name, std::string& refused)
    {
        switch (command)
        {
            case WSCMD_RUNS:
            {
                return true;
            }

            case WSCMD_PREFIX:
            {
                mState = STATE_CHORD;
                mChord = letter;
                return true;
            }

            case WSCMD_FIND:
            case WSCMD_REPLACE:
            {
                mState = STATE_DIALOG;
                mDialog = cMacroDialog(command == WSCMD_REPLACE);
                mDialogName = name;
                return true;
            }

            case WSCMD_SUBKEY:
            {
                mState = STATE_SUBKEY;
                return true;
            }

            case WSCMD_DIALOG:
            case WSCMD_NOT_IMPLEMENTED:
            case WSCMD_INVALID:
            {
                refused = name;
                return false;
            }
        }
        return true;
    }

    eState mState = STATE_TOP;
    char mChord = '\0';
    cMacroDialog mDialog = cMacroDialog(false);
    std::string mDialogName;
};

} // namespace


/////////////////////////////////////////////////////////////////////////////
///
/// @param  replace [in] true for Find and Replace
///
/////////////////////////////////////////////////////////////////////////////
cMacroDialog::cMacroDialog(bool replace)
    : mReplace(replace)
    , mField(0)
    , mControlP(false)
{
}


std::string& cMacroDialog::Field(void)
{
    size_t last = (mReplace == true) ? 2 : 1;
    if (mField >= last)
    {
        return mOptions;
    }
    if (mField == 1)
    {
        return mReplacement;
    }
    return mText;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  key [in] played key
///
/// @return DIALOG_OK or DIALOG_CANCEL when the key closes the dialog
///
/////////////////////////////////////////////////////////////////////////////
cMacroDialog::eResult cMacroDialog::Feed(const sKeyStroke& key)
{
    const size_t last = (mReplace == true) ? 2 : 1;
    bool options = (mField >= last);

    if (mControlP == true)
    {
        mControlP = false;
        if ((key.kind == KEYSTROKE_CHAR) && (options == false))
        {
            Field() += static_cast<char>(key.ch);
        }
        else if ((key.kind == KEYSTROKE_TEXT) && (key.utf8.size() == 1) && (options == false))
        {
            char c = key.utf8[0];
            if (isalpha(static_cast<unsigned char>(c)) != 0)
            {
                c = static_cast<char>(toupper(static_cast<unsigned char>(c)) - '@');
            }
            Field() += c;
        }
        return DIALOG_OPEN;
    }

    switch (key.kind)
    {
        case KEYSTROKE_SPECIAL:
        {
            switch (key.special)
            {
                case SPECIAL_ENTER:
                    if (options == true)
                    {
                        return DIALOG_OK;
                    }
                    mField++;
                    return DIALOG_OPEN;

                case SPECIAL_TAB:
                    if (key.shift == true)
                    {
                        mField = (mField > 0) ? (mField - 1) : 0;
                    }
                    else if (mField < last)
                    {
                        mField++;
                    }
                    return DIALOG_OPEN;

                case SPECIAL_F10:
                    return DIALOG_OK;

                case SPECIAL_ESCAPE:
                    return DIALOG_CANCEL;

                case SPECIAL_BACKSPACE:
                {
                    std::string& field = Field();
                    while ((field.empty() == false) && ((static_cast<unsigned char>(field.back()) & 0xC0U) == 0x80U))
                    {
                        field.pop_back();
                    }
                    if (field.empty() == false)
                    {
                        field.pop_back();
                    }
                    return DIALOG_OPEN;
                }

                default:
                    return DIALOG_OPEN;
            }
        }

        case KEYSTROKE_CHAR:
        {
            char letter = static_cast<char>(key.ch + '@');
            if (key.alt == true)
            {
                return DIALOG_OPEN;
            }
            if (letter == 'K')
            {
                return DIALOG_OK;
            }
            if (letter == 'Y')
            {
                Field().clear();
            }
            else if ((letter == 'N') && (options == false))
            {
                Field() += static_cast<char>(HARD_RETURN);
            }
            else if (letter == 'P')
            {
                mControlP = true;
            }
            return DIALOG_OPEN;
        }

        case KEYSTROKE_TEXT:
        {
            if (key.alt == true)
            {
                if ((key.utf8.size() == 1) && (isalpha(static_cast<unsigned char>(key.utf8[0])) != 0))
                {
                    char letter = static_cast<char>(toupper(static_cast<unsigned char>(key.utf8[0])));
                    if (letter == 'T')
                    {
                        mField = last;
                    }
                    else
                    {
                        size_t at = mOptions.find_first_of(std::string(1, letter) + static_cast<char>(tolower(letter)));
                        if (at == std::string::npos)
                        {
                            mOptions += letter;
                        }
                        else
                        {
                            mOptions.erase(at, 1);
                        }
                    }
                }
                return DIALOG_OPEN;
            }
            if (key.ctrl == false)
            {
                Field() += key.utf8;
            }
            return DIALOG_OPEN;
        }

        case KEYSTROKE_PAUSE:
        {
            return DIALOG_OPEN;
        }
    }
    return DIALOG_OPEN;
}


const std::string& cMacroDialog::Text(void) const
{
    return mText;
}


const std::string& cMacroDialog::Replacement(void) const
{
    return mReplacement;
}


const std::string& cMacroDialog::Options(void) const
{
    return mOptions;
}


bool cMacroScript::Refuse(const std::string& command, size_t line)
{
    mError = std::string(REFUSED) + command + ", line " + std::to_string(line);
    return false;
}


const std::string& cMacroScript::Error(void) const
{
    return mError;
}


const std::vector<sMacroStatement>& cMacroScript::Statements(void) const
{
    return mStatements;
}


size_t cMacroScript::Main(void) const
{
    return mMain;
}


bool cMacroScript::IsModern(void) const
{
    return mModern;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  reference [in] PlayMacro or ChainMacro argument, possibly with a
///                        DOS folder and .WSM
///
/// @return the macro name
///
/////////////////////////////////////////////////////////////////////////////
std::string cMacroScript::MacroName(const std::string& reference)
{
    std::string name = Trim(reference);
    size_t slash = name.find_last_of("\\/:");
    if (slash != std::string::npos)
    {
        name = name.substr(slash + 1);
    }
    if ((name.size() > 4) && (Lower(name.substr(name.size() - 4)) == ".wsm"))
    {
        name = name.substr(0, name.size() - 4);
    }
    return name;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  lines [in] macro source lines, code page 437 if not UTF-8
///
/// @return false with Error() at the first statement WordTsar does not
///         support
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroScript::Parse(const std::vector<std::string>& lines)
{
    mStatements.clear();
    mError.clear();
    mMain = 0;

    std::vector<std::string> source = lines;
    std::string all;
    for (const std::string& line : source)
    {
        all += line;
        all += '\n';
    }
    if (IsValidUtf8(all) == false)
    {
        for (std::string& line : source)
        {
            line = FromCodePage437(line);
        }
    }
    mModern = IsModernSource(source);

    struct sSub
    {
        std::string name;
        size_t start;
        std::map<std::string, size_t> labels;
        std::vector<size_t> gotos;
    };
    std::vector<sSub> subs;
    std::map<std::string, size_t> subStarts;
    bool inSub = false;
    bool hasMain = false;
    std::vector<size_t> openIfs;
    bool inComment = false;

    for (size_t index = 0; index < source.size(); index++)
    {
        size_t lineNumber = index + 1;
        std::string text = Trim(StripBlockComments(source[index], inComment));
        if ((text.empty() == true) ||
            ((Lower(text.substr(0, 3)) == "rem") && ((text.size() == 3) || (isalnum(static_cast<unsigned char>(text[3])) == 0))))
        {
            continue;
        }

        std::vector<sPart> parts;
        if (SplitStatements(text, parts) == false)
        {
            return Refuse(text, lineNumber);
        }

        for (const sPart& part : parts)
        {
            std::string statement = Trim(part.text);
            if (statement.empty() == true)
            {
                continue;
            }

            size_t length = 0;
            while ((length < statement.size()) &&
                   ((isalnum(static_cast<unsigned char>(statement[length])) != 0) || (statement[length] == '_')))
            {
                length++;
            }
            if (length == 0)
            {
                return Refuse(statement, lineNumber);
            }
            std::string word = Lower(statement.substr(0, length));
            std::string rest = Trim(statement.substr(length));

            if (word == "rem")
            {
                break;
            }

            // Label or IfException clause
            if ((part.colon == true) && (rest.empty() == true) && (word != "stop") && (word != "return"))
            {
                if (inSub == false)
                {
                    return Refuse(statement + ":", lineNumber);
                }
                std::string upper = statement;
                for (char& c : upper)
                {
                    c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
                }
                if ((openIfs.empty() == false) &&
                    ((upper == "ACK") || (upper == "QRY") || (upper == "NCM") || (upper == "DEF")))
                {
                    sMacroStatement clause;
                    clause.kind = MACRO_CLAUSE;
                    clause.line = lineNumber;
                    clause.text = upper;
                    mStatements[openIfs.back()].clauses.push_back({ upper, mStatements.size() + 1 });
                    mStatements.push_back(clause);
                    continue;
                }
                if (IsIdentifier(statement) == false)
                {
                    return Refuse(statement + ":", lineNumber);
                }
                subs.back().labels[word] = mStatements.size();
                continue;
            }

            sMacroStatement st;
            st.line = lineNumber;

            if (word == "sub")
            {
                if ((inSub == true) || (IsIdentifier(rest) == false))
                {
                    return Refuse(statement, lineNumber);
                }
                std::string name = Lower(rest);
                sSub sub;
                sub.name = name;
                sub.start = mStatements.size();
                subs.push_back(sub);
                subStarts[name] = sub.start;
                if (name == "main")
                {
                    mMain = sub.start;
                    hasMain = true;
                }
                inSub = true;
                continue;
            }

            if (inSub == false)
            {
                return Refuse(statement.substr(0, length), lineNumber);
            }

            if (word == "end")
            {
                std::string what = Lower(rest);
                if ((what == "sub") || (what == "main"))
                {
                    if (openIfs.empty() == false)
                    {
                        return Refuse("IfException without End IfException", mStatements[openIfs.back()].line);
                    }
                    st.kind = MACRO_END_SUB;
                    mStatements.push_back(st);
                    inSub = false;
                    continue;
                }
                if ((what == "ifexception") && (openIfs.empty() == false))
                {
                    st.kind = MACRO_END_IF_EXCEPTION;
                    size_t end = mStatements.size();
                    sMacroStatement& open = mStatements[openIfs.back()];
                    if (open.clauses.empty() == true)
                    {
                        return Refuse("IfException without ACK, QRY, NCM or DEF", open.line);
                    }
                    open.target = end;
                    for (size_t i = openIfs.back() + 1; i < end; i++)
                    {
                        if ((mStatements[i].kind == MACRO_CLAUSE) && (mStatements[i].target == 0))
                        {
                            mStatements[i].target = end;
                        }
                    }
                    openIfs.pop_back();
                    mStatements.push_back(st);
                    continue;
                }
                return Refuse(statement, lineNumber);
            }

            if (word == "key")
            {
                std::vector<std::string> values;
                std::string quotes;
                if ((StringArguments(rest, values, quotes) == false) || (values.size() != 1))
                {
                    return Refuse(statement, lineNumber);
                }
                std::string bad;
                if (KeyStringToKeys(values[0], st.keys, &bad) == false)
                {
                    return Refuse(bad, lineNumber);
                }
                st.kind = MACRO_KEYS;
                mStatements.push_back(st);
                continue;
            }

            if (word == "pauseforinput")
            {
                std::vector<std::string> values;
                std::string quotes;
                if ((StringArguments(rest, values, quotes) == false) || (values.size() > 2) ||
                    ((values.empty() == false) && (quotes[0] != '"')) ||
                    ((values.size() == 2) && (quotes[1] != '\'')))
                {
                    return Refuse(statement, lineNumber);
                }
                if ((values.empty() == true) || (values[0].empty() == true))
                {
                    st.keys.push_back(TextKeyStroke("-", false, false, true));
                }
                else
                {
                    std::string bad;
                    if ((KeyStringToKeys(values[0], st.keys, &bad) == false) || (st.keys.size() != 1))
                    {
                        return Refuse(statement, lineNumber);
                    }
                }
                if (values.size() == 2)
                {
                    st.text = values[1];
                }
                st.kind = MACRO_PAUSE_INPUT;
                mStatements.push_back(st);
                continue;
            }

            if ((word == "pauseforkey") && ((rest.empty() == true) || (rest == "()")))
            {
                st.kind = MACRO_PAUSE_KEY;
                mStatements.push_back(st);
                continue;
            }

            if ((word == "ifexception") && (rest.empty() == true))
            {
                st.kind = MACRO_IF_EXCEPTION;
                openIfs.push_back(mStatements.size());
                mStatements.push_back(st);
                continue;
            }

            if ((word == "goto") && (IsIdentifier(rest) == true))
            {
                st.kind = MACRO_GOTO;
                st.text = Lower(rest);
                subs.back().gotos.push_back(mStatements.size());
                mStatements.push_back(st);
                continue;
            }

            if ((word == "call") && (IsIdentifier(rest) == true))
            {
                st.kind = MACRO_CALL;
                st.text = Lower(rest);
                mStatements.push_back(st);
                continue;
            }

            if (((word == "return") || (word == "stop")) && (rest.empty() == true))
            {
                st.kind = (word == "return") ? MACRO_RETURN : MACRO_STOP;
                mStatements.push_back(st);
                continue;
            }

            if ((word == "playmacro") || (word == "chainmacro"))
            {
                std::string argument = rest;
                if ((argument.size() >= 2) && (argument.front() == '(') && (argument.back() == ')'))
                {
                    argument = argument.substr(1, argument.size() - 2);
                }
                std::string value;
                char quote = '\0';
                std::string after;
                if ((QuotedString(argument, value, quote, after) == false) || (after.empty() == false) ||
                    (MacroName(value).empty() == true))
                {
                    return Refuse(statement, lineNumber);
                }
                st.kind = (word == "playmacro") ? MACRO_PLAY_MACRO : MACRO_CHAIN_MACRO;
                st.text = MacroName(value);
                mStatements.push_back(st);
                continue;
            }

            if (word == "sethelplevel")
            {
                std::string arg = rest;
                if ((arg.size() >= 3) && (arg.front() == '(') && (arg.back() == ')'))
                {
                    arg = Trim(arg.substr(1, arg.size() - 2));
                }
                if ((arg.size() != 1) || (arg[0] < '0') || (arg[0] > '4'))
                {
                    return Refuse(statement, lineNumber);
                }
                st.kind = MACRO_SETTING;
                st.setting = MACRO_SETTING_HELP_LEVEL;
                st.value = arg[0] - '0';
                mStatements.push_back(st);
                continue;
            }

            bool setting = false;
            for (const sSettingName& entry : SETTING_NAMES)
            {
                if (word == entry.name)
                {
                    if (OnOffArgument(rest, st.value) == false)
                    {
                        return Refuse(statement, lineNumber);
                    }
                    st.kind = MACRO_SETTING;
                    st.setting = entry.setting;
                    setting = true;
                }
            }
            if (setting == true)
            {
                mStatements.push_back(st);
                continue;
            }

            return Refuse(statement.substr(0, length), lineNumber);
        }
    }

    if (openIfs.empty() == false)
    {
        return Refuse("IfException without End IfException", mStatements[openIfs.back()].line);
    }
    if (inSub == true)
    {
        sMacroStatement end;
        end.kind = MACRO_END_SUB;
        end.line = source.size();
        mStatements.push_back(end);
    }
    if (hasMain == false)
    {
        return Refuse("no Sub MAIN", 1);
    }

    for (const sSub& sub : subs)
    {
        for (size_t index : sub.gotos)
        {
            auto label = sub.labels.find(mStatements[index].text);
            if (label == sub.labels.end())
            {
                return Refuse("Goto " + mStatements[index].text, mStatements[index].line);
            }
            mStatements[index].target = label->second;
        }
    }
    for (sMacroStatement& st : mStatements)
    {
        if (st.kind == MACRO_CALL)
        {
            auto sub = subStarts.find(st.text);
            if (sub == subStarts.end())
            {
                return Refuse("Call " + st.text, st.line);
            }
            st.target = sub->second;
        }
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return false with Error() at the first key that reaches a command
///         WordTsar does not have
///
/// @brief
/// Follows the statements in source order. The keys of consecutive Key
/// statements may continue a chord or a Find dialog. Each IfException
/// clause starts from the editing screen; the first key of an ACK, QRY or
/// DEF clause answers the message and is not a command. A statement other
/// than a Key or a setting while a Find dialog is open is refused.
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroScript::CheckKeys(void)
{
    mError.clear();
    if (mModern == true)
    {
        return true;
    }

    cKeyChecker checker;
    std::vector<cKeyChecker> saved;
    bool dropFirst = false;

    for (const sMacroStatement& st : mStatements)
    {
        bool drop = dropFirst;
        dropFirst = false;

        switch (st.kind)
        {
            case MACRO_KEYS:
            {
                for (size_t i = 0; i < st.keys.size(); i++)
                {
                    if ((i == 0) && (drop == true))
                    {
                        continue;
                    }
                    std::string refused;
                    if (checker.Feed(st.keys[i], refused) == false)
                    {
                        return Refuse(refused, st.line);
                    }
                }
                break;
            }

            case MACRO_SETTING:
            {
                dropFirst = drop;
                break;
            }

            case MACRO_IF_EXCEPTION:
            {
                if (checker.InDialog() == true)
                {
                    return Refuse(StatementName(st.kind) + " in " + checker.DialogName(), st.line);
                }
                saved.push_back(checker);
                break;
            }

            case MACRO_CLAUSE:
            {
                checker.Reset();
                dropFirst = (st.text != "NCM");
                break;
            }

            case MACRO_END_IF_EXCEPTION:
            {
                if (saved.empty() == false)
                {
                    checker = saved.back();
                    saved.pop_back();
                }
                break;
            }

            default:
            {
                if (checker.InDialog() == true)
                {
                    return Refuse(StatementName(st.kind) + " in " + checker.DialogName(), st.line);
                }
                if (st.kind != MACRO_GOTO)
                {
                    checker.Reset();
                }
                break;
            }
        }
    }
    return true;
}
