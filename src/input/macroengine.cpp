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

#include "src/input/macroengine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

#include "src/core/editor/editorbase.h"
#include "src/core/utils/config.h"
#include "src/input/defaultmacros.h"
#include "src/input/moderninput.h"
#include "src/input/wordtsarinput.h"


namespace
{

const char MACRO_FOLDER[] = "macros";
const char MACRO_EXTENSION[] = ".wsm";
const char KEY_LINES_EXTENSION[] = ".txt";
const char STATE_RECORDING[] = "Recording";
const char STATE_PAUSED[] = "Type, then press Enter";
const char STATE_PAUSED_KEY[] = "Press a key";
const size_t MAX_MACRO_FRAMES = 32;                 // macros played inside each other
const size_t MAX_MACRO_CALLS = 64;                  // Call depth in one macro
const long MAX_STATEMENTS_WITHOUT_KEY = 100000;     // statements run without playing a key


bool EndsWith(const std::string& text, const std::string& suffix)
{
    return (text.size() >= suffix.size()) &&
           (text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0);
}


std::string Lower(const std::string& text)
{
    std::string lower = text;
    for (char& c : lower)
    {
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    }
    return lower;
}


// Extension of a file name: the text after the last '.', 1 to 4 letters and
// digits starting with a letter; empty if there is none.
std::string Extension(const std::string& name)
{
    size_t dot = name.rfind('.');
    if ((dot == std::string::npos) || (dot == 0))
    {
        return std::string();
    }
    std::string ext = name.substr(dot + 1);
    if ((ext.empty() == true) || (ext.size() > 4) || (isalpha(static_cast<unsigned char>(ext[0])) == 0))
    {
        return std::string();
    }
    for (char c : ext)
    {
        if (isalnum(static_cast<unsigned char>(c)) == 0)
        {
            return std::string();
        }
    }
    return Lower(ext);
}


std::string StripLineEnd(const std::string& line)
{
    std::string text = line;
    while ((text.empty() == false) && ((text.back() == '\r') || (text.back() == '\n')))
    {
        text.pop_back();
    }
    return text;
}


// Macro names (file names without ".wsm") of the *.wsm files in a folder; the
// extension in any case.
std::vector<std::string> ListFolder(const std::string& dir)
{
    std::vector<std::string> names;
    if (dir.empty() == true)
    {
        return names;
    }

    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        return names;
    }

    for (const std::filesystem::directory_entry& entry : it)
    {
        std::string file = entry.path().filename().string();
        if ((entry.is_regular_file(ec) == true) && (EndsWith(Lower(file), MACRO_EXTENSION) == true) &&
            (file.size() > 4) && (file[0] != '.'))
        {
            names.push_back(file.substr(0, file.size() - 4));
        }
    }

    std::sort(names.begin(), names.end());
    return names;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  dir    [in]     macros folder
/// @param  failed [in,out] names of the .txt files that could not be
///                         converted are appended here
///
/// @brief
/// Converts each one-key-per-line <name>.txt in the folder to <name>.wsm
/// and removes the .txt. A .txt with an existing <name>.wsm is left alone.
///
/////////////////////////////////////////////////////////////////////////////
void ConvertFolder(const std::string& dir, std::vector<std::string>& failed)
{
    if (dir.empty() == true)
    {
        return;
    }

    std::error_code ec;
    std::filesystem::directory_iterator it(dir, ec);
    if (ec)
    {
        return;
    }

    std::vector<std::string> names;
    for (const std::filesystem::directory_entry& entry : it)
    {
        std::string file = entry.path().filename().string();
        if ((entry.is_regular_file(ec) == true) && (EndsWith(file, KEY_LINES_EXTENSION) == true) &&
            (file.size() > 4) && (file[0] != '.'))
        {
            names.push_back(file.substr(0, file.size() - 4));
        }
    }
    std::sort(names.begin(), names.end());

    for (const std::string& name : names)
    {
        std::filesystem::path source = std::filesystem::path(dir) / (name + KEY_LINES_EXTENSION);
        std::filesystem::path target = std::filesystem::path(dir) / (name + MACRO_EXTENSION);
        if (std::filesystem::exists(target, ec) == true)
        {
            continue;
        }

        std::vector<std::string> lines;
        {
            std::ifstream in(source, std::ios::binary);
            std::string line;
            while (std::getline(in, line))
            {
                lines.push_back(StripLineEnd(line));
            }
        }

        std::string text;
        if (KeyLinesToMacroSource(lines, text) == false)
        {
            failed.push_back(name);
            continue;
        }

        bool written = false;
        {
            std::ofstream out(target, std::ios::binary | std::ios::trunc);
            if (out)
            {
                out.write(text.data(), static_cast<std::streamsize>(text.size()));
                out.close();
                written = !out.fail();
            }
        }
        if (written == false)
        {
            std::filesystem::remove(target, ec);
            failed.push_back(name);
            continue;
        }
        std::filesystem::remove(source, ec);
    }
}


bool IsEnter(const sKeyStroke& key)
{
    return (key.kind == KEYSTROKE_SPECIAL) && (key.special == SPECIAL_ENTER);
}


bool IsEscape(const sKeyStroke& key)
{
    return (key.kind == KEYSTROKE_SPECIAL) && (key.special == SPECIAL_ESCAPE);
}


// Default key sink: the editor's FeedKey.
class cEditorKeySink : public IMacroKeySink
{
public:
    explicit cEditorKeySink(cEditorBase& editor) : mEditor(editor) {}
    bool FeedMacroKey(const sKeyStroke& key) override { return mEditor.FeedKey(key); }
    bool EditsDocument(void) const override { return true; }

private:
    cEditorBase& mEditor;
};

} // namespace


/////////////////////////////////////////////////////////////////////////////
///
/// @param  documentDir [in] folder of the open document, empty if untitled
/// @param  configDir   [in] folder of config.ini
///
/////////////////////////////////////////////////////////////////////////////
cMacroStore::cMacroStore(const std::string& documentDir, const std::string& configDir)
{
    if (documentDir.empty() == false)
    {
        mDocumentDir = (std::filesystem::path(documentDir) / MACRO_FOLDER).string();
    }
    if (configDir.empty() == false)
    {
        mConfigDir = (std::filesystem::path(configDir) / MACRO_FOLDER).string();
    }
}


bool cMacroStore::IsValidName(const std::string& name)
{
    std::string ext = Extension(name);
    return (name.empty() == false) && (name[0] != '.') &&
           (name.find('/') == std::string::npos) && (name.find(':') == std::string::npos) &&
           ((ext.empty() == true) || (ext == "wsm"));
}


std::string cMacroStore::TrimName(const std::string& name)
{
    const char* blanks = " \t";
    size_t first = name.find_first_not_of(blanks);
    if (first == std::string::npos)
    {
        return std::string();
    }
    size_t last = name.find_last_not_of(blanks);
    return name.substr(first, last - first + 1);
}


std::string cMacroStore::FileName(const std::string& name)
{
    if (Extension(name) == "wsm")
    {
        return name;
    }
    return name + MACRO_EXTENSION;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Creates the config macros folder with the default macros if the folder
/// does not exist. An existing folder, even an empty one, is left alone.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroStore::CreateDefaults(void) const
{
    std::error_code ec;
    if ((mConfigDir.empty() == true) || (std::filesystem::exists(mConfigDir, ec) == true) || ec)
    {
        return;
    }
    if (std::filesystem::create_directories(mConfigDir, ec) == false)
    {
        return;
    }

    for (size_t i = 0; i < kDefaultMacroCount; i++)
    {
        const sDefaultMacro& macro = kDefaultMacros[i];
        std::ofstream out((std::filesystem::path(mConfigDir) / FileName(macro.name)).string(),
                          std::ios::binary | std::ios::trunc);
        out.write(macro.data, static_cast<std::streamsize>(macro.size));
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  unconverted [out] if not nullptr, the names of .txt macros that
///                           could not be converted
///
/// @return macro names, document folder first
///
/// @brief
/// Converts the one-key-per-line .txt macros in both folders first.
///
/////////////////////////////////////////////////////////////////////////////
std::vector<std::string> cMacroStore::List(std::vector<std::string>* unconverted) const
{
    CreateDefaults();
    std::vector<std::string> failed;
    ConvertFolder(mDocumentDir, failed);
    ConvertFolder(mConfigDir, failed);
    if (unconverted != nullptr)
    {
        *unconverted = failed;
    }

    std::vector<std::string> names = ListFolder(mDocumentDir);
    size_t documentCount = names.size();

    for (const std::string& name : ListFolder(mConfigDir))
    {
        if (std::find(names.begin(), names.begin() + static_cast<long>(documentCount), name) ==
            names.begin() + static_cast<long>(documentCount))
        {
            names.push_back(name);
        }
    }
    return names;
}


std::string cMacroStore::FindPath(const std::string& name) const
{
    std::error_code ec;
    for (const std::string& dir : { mDocumentDir, mConfigDir })
    {
        if (dir.empty() == false)
        {
            std::filesystem::path path = std::filesystem::path(dir) / FileName(name);
            if (std::filesystem::is_regular_file(path, ec) == true)
            {
                return path.string();
            }
        }
    }
    return std::string();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  name [in] macro name
///
/// @return file in the document macros folder if that folder exists,
///         otherwise in the config macros folder; empty if neither applies
///
/////////////////////////////////////////////////////////////////////////////
std::string cMacroStore::SavePath(const std::string& name) const
{
    std::error_code ec;
    if ((mDocumentDir.empty() == false) && (std::filesystem::is_directory(mDocumentDir, ec) == true))
    {
        return (std::filesystem::path(mDocumentDir) / FileName(name)).string();
    }
    if (mConfigDir.empty() == true)
    {
        return std::string();
    }
    return (std::filesystem::path(mConfigDir) / FileName(name)).string();
}


bool cMacroStore::Load(const std::string& name, std::vector<std::string>& lines) const
{
    std::string path = FindPath(name);
    if (path.empty() == true)
    {
        return false;
    }

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        return false;
    }

    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    size_t end = text.find('\x1A');
    if (end != std::string::npos)
    {
        text.erase(end);
    }

    lines.clear();
    size_t start = 0;
    while (start < text.size())
    {
        size_t newline = text.find('\n', start);
        if (newline == std::string::npos)
        {
            newline = text.size();
        }
        lines.push_back(StripLineEnd(text.substr(start, newline - start)));
        start = newline + 1;
    }
    return true;
}


bool cMacroStore::Save(const std::string& name, const std::string& source) const
{
    std::string path = SavePath(name);
    if (path.empty() == true)
    {
        return false;
    }

    if (std::filesystem::path(path).parent_path() == std::filesystem::path(mConfigDir))
    {
        CreateDefaults();
        std::error_code ec;
        std::filesystem::create_directories(mConfigDir, ec);
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        return false;
    }
    out.write(source.data(), static_cast<std::streamsize>(source.size()));
    return out.good();
}


std::string cMacroStore::SiblingPath(const std::string& name, const std::string& newName) const
{
    std::string path = FindPath(name);
    if (path.empty() == true)
    {
        return std::string();
    }
    return (std::filesystem::path(path).parent_path() / FileName(newName)).string();
}


bool cMacroStore::Rename(const std::string& name, const std::string& newName) const
{
    std::string source = FindPath(name);
    std::string target = SiblingPath(name, newName);
    if ((source.empty() == true) || (target.empty() == true))
    {
        return false;
    }

    std::error_code ec;
    std::filesystem::rename(source, target, ec);
    return !ec;
}


bool cMacroStore::Copy(const std::string& name, const std::string& newName) const
{
    std::string source = FindPath(name);
    std::string target = SiblingPath(name, newName);
    if ((source.empty() == true) || (target.empty() == true))
    {
        return false;
    }

    std::error_code ec;
    if (std::filesystem::equivalent(source, target, ec) == true)
    {
        return false;
    }
    ec.clear();
    std::filesystem::copy_file(source, target, std::filesystem::copy_options::overwrite_existing, ec);
    return !ec;
}


bool cMacroStore::Delete(const std::string& name) const
{
    std::string path = FindPath(name);
    if (path.empty() == true)
    {
        return false;
    }

    std::error_code ec;
    return (std::filesystem::remove(path, ec) == true) && !ec;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  editor [in] editor the keys are fed to
///
/////////////////////////////////////////////////////////////////////////////
cMacroEngine::cMacroEngine(cEditorBase& editor)
    : mEditor(editor)
    , mEditorSink(std::make_unique<cEditorKeySink>(editor))
    , mSink(nullptr)
    , mRecording(false)
    , mPauseTyping(false)
    , mRecordMode(INPUT_WORDSTAR)
    , mCurrentKey(-1)
    , mDropKey(false)
    , mKeyDepth(0)
    , mPlaying(false)
    , mPaused(false)
    , mPauseForKey(false)
    , mResumeAfterKey(false)
    , mPauseTyped(0)
    , mCleared(false)
    , mUndoOpen(false)
    , mFeeding(false)
    , mJump(false)
    , mJumpTo(0)
    , mDropFirst(false)
    , mAnswerKind(MACRO_EXCEPTION_ACK)
    , mStatementsWithoutKey(0)
    , mAutoRestore(false)
    , mSavedInsert(true)
    , mSavedShowControl(0)
    , mSavedShowDot(true)
    , mSavedHelpLevel(0)
{
}


cMacroEngine::~cMacroEngine(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  sink [in] receiver of played keys; nullptr for the editor's FeedKey
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::SetKeySink(IMacroKeySink* sink)
{
    mSink = sink;
}


IMacroKeySink& cMacroEngine::Sink(void)
{
    return (mSink != nullptr) ? *mSink : *mEditorSink;
}


bool cMacroEngine::IsRecording(void) const
{
    return mRecording;
}


bool cMacroEngine::IsPlaying(void) const
{
    return mPlaying;
}


std::string cMacroEngine::StateText(void) const
{
    if (mRecording == true)
    {
        return STATE_RECORDING;
    }
    if ((mPlaying == true) && (mPaused == true))
    {
        if (mPauseForKey == true)
        {
            return STATE_PAUSED_KEY;
        }
        if (mPauseMessage.empty() == false)
        {
            return mPauseMessage;
        }
        if (IsEnter(mPauseEnd) == true)
        {
            return STATE_PAUSED;
        }
        return "Type, then press " + KeyToKeyString(mPauseEnd);
    }
    return std::string();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  key [in] key about to be dispatched
///
/// @return true if the key was used here and must not be dispatched
///
/// @brief
/// At PauseForInput the end key continues the macro and Esc cancels it;
/// other keys are dispatched. At PauseForKey the key is dispatched and the
/// macro continues after it. While recording, the Enter that ends the text
/// typed after a pause ends the pause. Neither end key is inserted or
/// recorded.
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::InterceptKey(const sKeyStroke& key)
{
    if ((mPlaying == true) && (mPaused == true))
    {
        if (mPauseForKey == true)
        {
            mPaused = false;
            mPauseForKey = false;
            mResumeAfterKey = true;
            return false;
        }
        if (IsPauseEnd(key) == true)
        {
            mPaused = false;
            if ((mPauseTyped == 0) && (mFrames.empty() == false))
            {
                // NCM: nothing typed before the end key
                sFrame& frame = mFrames.back();
                const std::vector<sMacroStatement>& statements = frame.script->Statements();
                if ((frame.pc < statements.size()) && (statements[frame.pc].kind == MACRO_IF_EXCEPTION))
                {
                    for (const auto& clause : statements[frame.pc].clauses)
                    {
                        if (clause.first == "NCM")
                        {
                            frame.pc = clause.second;
                            break;
                        }
                    }
                }
            }
            Run();
            return true;
        }
        if (IsEscape(key) == true)
        {
            mEditor.SetStatusMessage("Macro stopped");
            Finish();
            return true;
        }
        mPauseTyped++;
        return false;
    }

    if ((mRecording == true) && (mPauseTyping == true) && (IsEnter(key) == true))
    {
        mPauseTyping = false;
        return true;
    }

    return false;
}


// True for the PauseForInput end key.
bool cMacroEngine::IsPauseEnd(const sKeyStroke& key) const
{
    const sKeyStroke& end = mPauseEnd;
    if ((key.kind != end.kind) || (key.alt != end.alt))
    {
        return false;
    }
    switch (key.kind)
    {
        case KEYSTROKE_CHAR:    return key.ch == end.ch;
        case KEYSTROKE_SPECIAL: return (key.special == end.special) && (key.ctrl == end.ctrl) && (key.shift == end.shift);
        case KEYSTROKE_TEXT:    return Lower(key.utf8) == Lower(end.utf8);
        case KEYSTROKE_PAUSE:   return false;
    }
    return false;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the input handler for a macro recorded in the other input mode
///         while it plays, otherwise nullptr
///
/////////////////////////////////////////////////////////////////////////////
IInputHandler* cMacroEngine::PlaybackInput(void)
{
    if (mPlaying == true)
    {
        return mPlayInput.get();
    }
    return nullptr;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  key [in] key about to be dispatched
///
/// @brief
/// While recording, adds the key; EndKey removes it again if it was not
/// handled or a command dropped it. Keys typed after a pause are not
/// recorded. A key without a macro name is not recorded.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::BeginKey(const sKeyStroke& key)
{
    mKeyDepth++;
    mCurrentKey = -1;
    mDropKey = false;

    if ((mRecording == false) || (mPauseTyping == true))
    {
        return;
    }

    if (KeyToKeyString(key).empty() == true)
    {
        mEditor.SetStatusMessage("Key not recorded");
        return;
    }

    mCurrentKey = static_cast<long>(mRecorded.size());
    mRecorded.push_back(key);
}


void cMacroEngine::EndKey(bool handled)
{
    if (mKeyDepth > 0)
    {
        mKeyDepth--;
    }
    if ((mCurrentKey >= 0) && (mCurrentKey < static_cast<long>(mRecorded.size())) &&
        ((handled == false) || (mDropKey == true)))
    {
        mRecorded.erase(mRecorded.begin() + mCurrentKey);
    }
    mCurrentKey = -1;
    mDropKey = false;

    if ((mResumeAfterKey == true) && (mKeyDepth == 0))
    {
        mResumeAfterKey = false;
        Run();
    }
}


void cMacroEngine::DropCurrentKey(void)
{
    mDropKey = true;
}


cMacroStore cMacroEngine::MakeStore(void) const
{
    std::string documentDir;
    if ((mEditor.mFileSet == true) && (mEditor.mFileDir.empty() == false))
    {
        documentDir = mEditor.mFileDir;
    }
    return cMacroStore(documentDir, cConfig::GetConfigDir());
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// ^MR starts recording; ^MR while recording stops it without recording
/// the ^M R keys. Nothing while a macro plays.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::RecordCommand(void)
{
    if (mPlaying == true)
    {
        return;
    }

    if (mRecording == true)
    {
        // The chord prefix before this key is dropped here, this key in EndKey.
        if ((mCurrentKey > 0) && (mCurrentKey <= static_cast<long>(mRecorded.size())))
        {
            mRecorded.erase(mRecorded.begin() + (mCurrentKey - 1));
            mCurrentKey--;
        }
        DropCurrentKey();
        StopRecording();
        return;
    }

    mRecorded.clear();
    mPauseTyping = false;
    mRecordMode = (dynamic_cast<cModernInput*>(mEditor.GetInput()) != nullptr) ? INPUT_MODERN : INPUT_WORDSTAR;
    mRecording = true;
    mEditor.Repaint();
}


void cMacroEngine::EndRecordingCommand(void)
{
    if (mRecording == false)
    {
        return;
    }
    DropCurrentKey();
    StopRecording();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Alt+- while recording writes a PAUSE line. Text typed afterwards goes
/// into the document but is not recorded, up to and including the Enter
/// that ends it.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::PauseCommand(void)
{
    if ((mRecording == false) || (mPauseTyping == true))
    {
        return;
    }

    DropCurrentKey();
    mRecorded.push_back(PauseKeyStroke());
    mPauseTyping = true;
    mEditor.SetStatusMessage("Type, then press Enter");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Ends recording, asks for a name and saves the macro.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::StopRecording(void)
{
    mRecording = false;
    mPauseTyping = false;

    // The key being dispatched is not part of the macro.
    std::vector<sKeyStroke> keys = mRecorded;
    if ((mCurrentKey >= 0) && (mCurrentKey < static_cast<long>(keys.size())))
    {
        keys.erase(keys.begin() + mCurrentKey);
        mRecorded.erase(mRecorded.begin() + mCurrentKey);
        mCurrentKey = -1;
    }
    mRecorded.clear();

    std::string source = KeysToMacroSource(keys, mRecordMode);

    mEditor.Repaint();

    std::string name = cMacroStore::TrimName(mEditor.PromptText("Save Macro", "Macro name:"));
    if (name.empty() == true)
    {
        mEditor.SetStatusMessage("Macro not saved");
        return;
    }
    if (cMacroStore::IsValidName(name) == false)
    {
        mEditor.SetStatusMessage("Invalid macro name: " + name, false, 60, true);
        return;
    }

    cMacroStore store = MakeStore();
    store.CreateDefaults();
    std::error_code ec;
    if ((std::filesystem::exists(store.SavePath(name), ec) == true) &&
        (mEditor.AskYesNo("Save Macro", "Macro " + name + " exists. Replace it?") == false))
    {
        mEditor.SetStatusMessage("Macro not saved");
        return;
    }

    if (store.Save(name, source) == false)
    {
        mEditor.SetStatusMessage("Cannot save macro " + name, false, 60, true);
        return;
    }
    mEditor.SetStatusMessage("Macro " + name + " saved");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// ^MP: lists the macros and plays the chosen one. Nothing while a macro
/// plays or is being recorded.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::PlayCommand(void)
{
    if ((mPlaying == true) || (mRecording == true))
    {
        return;
    }

    cMacroStore store = MakeStore();
    std::vector<std::string> unconverted;
    std::vector<std::string> names = store.List(&unconverted);
    ReportUnconverted(unconverted);
    if (names.empty() == true)
    {
        if (unconverted.empty() == true)
        {
            mEditor.SetStatusMessage("No macros yet. Record one with ^MR.");
        }
        return;
    }

    std::string name = mEditor.ChooseFromList("Play Macro", names);
    if (name.empty() == true)
    {
        return;
    }

    std::vector<std::string> lines;
    if (store.Load(name, lines) == false)
    {
        mEditor.SetStatusMessage("Cannot read macro " + name, false, 60, true);
        return;
    }

    Play(lines, name);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  store [in] macro folders
/// @param  title [in] list title
///
/// @return the chosen macro name; empty if there are none (status "No
///         macros" or "Cannot convert") or the list was cancelled
///
/////////////////////////////////////////////////////////////////////////////
std::string cMacroEngine::ChooseMacro(const cMacroStore& store, const std::string& title)
{
    std::vector<std::string> unconverted;
    std::vector<std::string> names = store.List(&unconverted);
    ReportUnconverted(unconverted);
    if (names.empty() == true)
    {
        if (unconverted.empty() == true)
        {
            mEditor.SetStatusMessage("No macros");
        }
        return std::string();
    }
    return mEditor.ChooseFromList(title, names);
}


// "Cannot convert <name>.txt" for the first macro that was not converted.
void cMacroEngine::ReportUnconverted(const std::vector<std::string>& unconverted)
{
    if (unconverted.empty() == false)
    {
        mEditor.SetStatusMessage("Cannot convert " + unconverted.front() + KEY_LINES_EXTENSION, false, 60, true);
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// ^MD: opens the chosen macro file as a text document. Nothing while a
/// macro plays.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::EditCommand(void)
{
    if (mPlaying == true)
    {
        return;
    }

    cMacroStore store = MakeStore();
    std::string name = ChooseMacro(store, "Edit Macro");
    if (name.empty() == true)
    {
        return;
    }

    std::string path = store.FindPath(name);
    if (path.empty() == true)
    {
        mEditor.ShowError("Edit Macro", "Cannot find " + cMacroStore::FileName(name));
        return;
    }
    mEditor.OpenDocument(path);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  rename [in] true for ^ME (rename in place), false for ^MO (copy
///                     into the same folder)
///
/// @brief
/// Asks for the macro and the new name. An existing target is replaced only
/// after AskYesNo. Nothing while a macro plays.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::CopyOrRename(bool rename)
{
    if (mPlaying == true)
    {
        return;
    }

    const std::string title = (rename == true) ? "Rename Macro" : "Copy Macro";
    const std::string notDone = (rename == true) ? "Macro not renamed" : "Macro not copied";

    cMacroStore store = MakeStore();
    std::string name = ChooseMacro(store, title);
    if (name.empty() == true)
    {
        return;
    }

    std::string newName = cMacroStore::TrimName(
        mEditor.PromptText(title, (rename == true) ? "New name for " + name + ":" : "Copy " + name + " to:"));
    if (newName.empty() == true)
    {
        mEditor.SetStatusMessage(notDone);
        return;
    }
    if (cMacroStore::IsValidName(newName) == false)
    {
        mEditor.SetStatusMessage("Invalid macro name: " + newName, false, 60, true);
        return;
    }

    std::string source = store.FindPath(name);
    std::string target = store.SiblingPath(name, newName);
    if (source.empty() == true)
    {
        mEditor.ShowError(title, "Cannot find " + cMacroStore::FileName(name));
        return;
    }

    std::error_code ec;
    bool exists = std::filesystem::exists(target, ec);
    bool same = exists && std::filesystem::equivalent(source, target, ec);
    if ((same == true) && ((rename == false) ||
                           (std::filesystem::path(source).filename() == std::filesystem::path(target).filename())))
    {
        mEditor.SetStatusMessage(notDone);
        return;
    }
    if ((exists == true) && (same == false) &&
        (mEditor.AskYesNo(title, "Macro " + newName + " exists. Replace it?") == false))
    {
        mEditor.SetStatusMessage(notDone);
        return;
    }

    bool ok = (rename == true) ? store.Rename(name, newName) : store.Copy(name, newName);
    if (ok == false)
    {
        mEditor.ShowError(title, ((rename == true) ? "Cannot rename " : "Cannot copy ") + source);
        return;
    }
    mEditor.SetStatusMessage("Macro " + name + ((rename == true) ? " renamed to " : " copied to ") + newName);
}


void cMacroEngine::RenameCommand(void)
{
    CopyOrRename(true);
}


void cMacroEngine::CopyCommand(void)
{
    CopyOrRename(false);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// ^MY: deletes the chosen macro after AskYesNo. Nothing while a macro
/// plays.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::DeleteCommand(void)
{
    if (mPlaying == true)
    {
        return;
    }

    cMacroStore store = MakeStore();
    std::string name = ChooseMacro(store, "Delete Macro");
    if (name.empty() == true)
    {
        return;
    }

    if (mEditor.AskYesNo("Delete Macro", "Delete macro " + name + "?") == false)
    {
        return;
    }

    std::string path = store.FindPath(name);
    if (store.Delete(name) == false)
    {
        mEditor.ShowError("Delete Macro", "Cannot delete " + ((path.empty() == true) ? cMacroStore::FileName(name) : path));
        return;
    }
    mEditor.SetStatusMessage("Macro " + name + " deleted");
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  name   [in]     macro name, as PlayMacro names it
/// @param  script [in]     the parsed macro
/// @param  error  [out]    why a macro is refused
///
/// @return false if this macro or one it plays is refused or missing
///
/// @brief
/// Checks the keys of the macro and loads, parses and checks each macro it
/// plays or chains to, once per name.
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::LoadScripts(const std::string& name, std::shared_ptr<cMacroScript> script, std::string& error)
{
    if (script->CheckKeys() == false)
    {
        error = script->Error();
        if (mScripts.empty() == false)
        {
            error += " (" + name + ")";
        }
        return false;
    }
    mScripts[Lower(name)] = script;

    cMacroStore store = MakeStore();
    for (const sMacroStatement& statement : script->Statements())
    {
        if ((statement.kind != MACRO_PLAY_MACRO) && (statement.kind != MACRO_CHAIN_MACRO))
        {
            continue;
        }
        if (mScripts.find(Lower(statement.text)) != mScripts.end())
        {
            continue;
        }

        std::vector<std::string> lines;
        if (store.Load(statement.text, lines) == false)
        {
            error = "Cannot find macro " + statement.text + ", line " + std::to_string(statement.line);
            return false;
        }
        auto other = std::make_shared<cMacroScript>();
        if (other->Parse(lines) == false)
        {
            error = other->Error() + " (" + statement.text + ")";
            return false;
        }
        if (LoadScripts(statement.text, other, error) == false)
        {
            return false;
        }
    }
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  lines [in] macro source lines
/// @param  name  [in] macro name, empty if it has none
///
/// @return false if the macro is refused (status message
///         "Not supported in WordTsar: ...")
///
/// @brief
/// Parses and checks the macro and the macros it plays, then plays it
/// through the key sink as one undo step. The undo step starts when the
/// keys first reach a document.
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::Play(const std::vector<std::string>& lines, const std::string& name)
{
    if ((mPlaying == true) || (mRecording == true))
    {
        return false;
    }

    auto script = std::make_shared<cMacroScript>();
    std::string error;
    mScripts.clear();
    if ((script->Parse(lines) == false) || (LoadScripts(name, script, error) == false))
    {
        if (error.empty() == true)
        {
            error = script->Error();
        }
        mScripts.clear();
        mEditor.SetStatusMessage(error, false, 60, true);
        return false;
    }

    mFrames.clear();
    mFrames.push_back({ script, script->Main(), {} });
    mPending.clear();
    mPaused = false;
    mPauseForKey = false;
    mResumeAfterKey = false;
    mCleared = false;
    mJump = false;
    mDropFirst = false;
    mStatementsWithoutKey = 0;
    mAutoRestore = false;
    SaveSettings();

    bool modern = script->IsModern();
    bool currentModern = (dynamic_cast<cModernInput*>(mEditor.GetInput()) != nullptr);
    mPlayInput.reset();
    if (modern != currentModern)
    {
        if (modern == true)
        {
            mPlayInput = std::make_unique<cModernInput>(&mEditor);
        }
        else
        {
            mPlayInput = std::make_unique<cWordStarInput>(&mEditor);
        }
    }

    mPlaying = true;
    mUndoOpen = false;
    if (Sink().EditsDocument() == true)
    {
        mEditor.BeginMacroUndo();
        mUndoOpen = true;
    }
    Run();
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Runs statements until the macro ends, pauses, stops or a key closes the
/// document. Runs the editor's after-key update after every key. A key fed
/// to a sink without a document may load one; that does not stop playback.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::Run(void)
{
    while ((mPlaying == true) && (mPaused == false))
    {
        if (mPending.empty() == false)
        {
            sKeyStroke key = mPending.front();
            mPending.pop_front();
            mStatementsWithoutKey = 0;

            bool editing = Sink().EditsDocument();
            bool feeding = mFeeding;
            mFeeding = true;
            Sink().FeedMacroKey(key);
            mFeeding = feeding;
            if (editing == false)
            {
                mCleared = false;
                if ((Sink().EditsDocument() == true) && (mUndoOpen == false))
                {
                    mEditor.BeginMacroUndo();
                    mUndoOpen = true;
                }
            }

            if ((mJump == true) && (mFrames.empty() == false))
            {
                mJump = false;
                mFrames.back().pc = mJumpTo;
            }

            bool stop = (mCleared == true) || (mEditor.MacroStopRequested() == true);
            mEditor.AfterMacroKey();
            if (stop == true)
            {
                Finish();
                return;
            }
            continue;
        }

        if (mFrames.empty() == true)
        {
            Finish();
            return;
        }

        sFrame& frame = mFrames.back();
        const std::vector<sMacroStatement>& statements = frame.script->Statements();
        if (frame.pc >= statements.size())
        {
            EndFrame();
            continue;
        }

        const sMacroStatement& statement = statements[frame.pc];
        bool drop = mDropFirst;
        mDropFirst = false;

        mStatementsWithoutKey++;
        if (mStatementsWithoutKey > MAX_STATEMENTS_WITHOUT_KEY)
        {
            StopRunaway("no key in " + std::to_string(MAX_STATEMENTS_WITHOUT_KEY) + " statements", statement.line);
            return;
        }

        switch (statement.kind)
        {
            case MACRO_KEYS:
            {
                frame.pc++;
                size_t answer = (drop == true) ? cMacroScript::AnswerLength(statement.keys, mAnswerKind) : 0;
                mPending.assign(statement.keys.begin() + static_cast<long>(answer), statement.keys.end());
                break;
            }

            case MACRO_PAUSE_INPUT:
            case MACRO_PAUSE_KEY:
            {
                frame.pc++;
                mPaused = true;
                mPauseForKey = (statement.kind == MACRO_PAUSE_KEY);
                mPauseEnd = statement.keys.empty() ? SpecialKeyStroke(SPECIAL_ENTER) : statement.keys.front();
                mPauseMessage = statement.text;
                mPauseTyped = 0;
                mStatementsWithoutKey = 0;
                mEditor.AfterMacroKey();
                mEditor.Repaint();
                return;
            }

            case MACRO_GOTO:
            {
                frame.pc = statement.target;
                break;
            }

            case MACRO_CALL:
            {
                if (frame.calls.size() >= MAX_MACRO_CALLS)
                {
                    StopRunaway("Calls nested more than " + std::to_string(MAX_MACRO_CALLS) + " deep", statement.line);
                    return;
                }
                frame.calls.push_back(frame.pc + 1);
                frame.pc = statement.target;
                break;
            }

            case MACRO_RETURN:
            case MACRO_END_SUB:
            {
                if (frame.calls.empty() == false)
                {
                    frame.pc = frame.calls.back();
                    frame.calls.pop_back();
                }
                else
                {
                    EndFrame();
                }
                break;
            }

            case MACRO_STOP:
            {
                Finish();
                return;
            }

            case MACRO_PLAY_MACRO:
            case MACRO_CHAIN_MACRO:
            {
                frame.pc++;
                auto script = mScripts.find(Lower(statement.text));
                if (script == mScripts.end())
                {
                    Finish();
                    return;
                }
                if (statement.kind == MACRO_CHAIN_MACRO)
                {
                    mFrames.pop_back();
                }
                else if (mFrames.size() >= MAX_MACRO_FRAMES)
                {
                    StopRunaway("macros played inside each other more than " + std::to_string(MAX_MACRO_FRAMES) + " deep",
                                statement.line);
                    return;
                }
                mFrames.push_back({ script->second, script->second->Main(), {} });
                break;
            }

            case MACRO_SETTING:
            {
                frame.pc++;
                ApplySetting(statement);
                mDropFirst = drop;
                break;
            }

            case MACRO_IF_EXCEPTION:
            case MACRO_CLAUSE:
            {
                frame.pc = statement.target + 1;
                break;
            }

            case MACRO_END_IF_EXCEPTION:
            {
                frame.pc++;
                break;
            }
        }
    }
}


// The macro played by PlayMacro or the whole playback ends.
void cMacroEngine::EndFrame(void)
{
    if (mFrames.empty() == false)
    {
        mFrames.pop_back();
    }
    if (mFrames.empty() == true)
    {
        Finish();
    }
}


// Ends a macro that does not finish: "Macro stopped: <reason>, line <n>".
void cMacroEngine::StopRunaway(const std::string& reason, size_t line)
{
    mEditor.SetStatusMessage("Macro stopped: " + reason + ", line " + std::to_string(line), false, 60, true);
    Finish();
}


void cMacroEngine::Finish(void)
{
    if (mPlaying == false)
    {
        return;
    }

    mPlaying = false;
    mPaused = false;
    mPauseForKey = false;
    mResumeAfterKey = false;
    mPending.clear();
    mFrames.clear();
    mScripts.clear();
    mPlayInput.reset();
    if (mAutoRestore == true)
    {
        RestoreSettings();
    }
    if (mUndoOpen == true)
    {
        mUndoOpen = false;
        mEditor.EndMacroUndo(mCleared == false);
    }
    mEditor.Repaint();
}


void cMacroEngine::SaveSettings(void)
{
    mSavedInsert = mEditor.mInsertMode;
    mSavedShowControl = static_cast<int>(mEditor.GetShowControls());
    mSavedShowDot = mEditor.GetShowDot();
    mSavedHelpLevel = mEditor.mHelpLevel;
}


void cMacroEngine::RestoreSettings(void)
{
    mEditor.mInsertMode = mSavedInsert;
    mEditor.mHelpLevel = mSavedHelpLevel;
    mEditor.SetShowDot(mSavedShowDot);
    if (static_cast<int>(mEditor.GetShowControls()) != mSavedShowControl)
    {
        mEditor.SetShowControls(static_cast<eShowControl>(mSavedShowControl));
        mEditor.LayoutDocument(true);
    }
    mEditor.CalculateCaretPosition();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  statement [in] a MACRO_SETTING statement
///
/// @brief
/// Insert sets insert mode. CmdTags and HideDots set the control display:
/// dot commands hidden shows nothing (SHOW_NONE), otherwise command tags
/// show all (SHOW_ALL) or dot commands only (SHOW_DOT). SetHelpLevel 0-4
/// sets the help level 0-3. ColMode, ColReplace and WSQuiet have no
/// WordTsar setting.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::ApplySetting(const sMacroStatement& statement)
{
    eShowControl show = mEditor.GetShowControls();
    bool tags = (show == SHOW_ALL);
    bool hideDots = (show == SHOW_NONE);

    switch (statement.setting)
    {
        case MACRO_SETTING_AUTO_RESTORE:
            mAutoRestore = (statement.value != 0);
            return;

        case MACRO_SETTING_INSERT:
            mEditor.mInsertMode = (statement.value != 0);
            mEditor.CalculateCaretPosition();
            return;

        case MACRO_SETTING_HELP_LEVEL:
            mEditor.mHelpLevel = (statement.value > 0) ? (statement.value - 1) : 0;
            return;

        case MACRO_SETTING_CMD_TAGS:
            tags = (statement.value != 0);
            break;

        case MACRO_SETTING_HIDE_DOTS:
            hideDots = (statement.value != 0);
            break;

        case MACRO_SETTING_IGNORED:
            return;
    }

    eShowControl wanted = (hideDots == true) ? SHOW_NONE : ((tags == true) ? SHOW_ALL : SHOW_DOT);
    mEditor.SetShowDot(hideDots == false);
    if (wanted != show)
    {
        mEditor.SetShowControls(wanted);
        mEditor.LayoutDocument(true);
        mEditor.CalculateCaretPosition();
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  key [out] next key for a dialog the macro answers
///
/// @return false if the macro has no more keys before a statement other
///         than Key or a setting
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::NextDialogKey(sKeyStroke& key)
{
    while (mPending.empty() == true)
    {
        if (mFrames.empty() == true)
        {
            return false;
        }
        sFrame& frame = mFrames.back();
        const std::vector<sMacroStatement>& statements = frame.script->Statements();
        if (frame.pc >= statements.size())
        {
            return false;
        }
        const sMacroStatement& statement = statements[frame.pc];
        if (statement.kind == MACRO_SETTING)
        {
            frame.pc++;
            ApplySetting(statement);
            continue;
        }
        if (statement.kind != MACRO_KEYS)
        {
            return false;
        }
        frame.pc++;
        mPending.assign(statement.keys.begin(), statement.keys.end());
    }

    key = mPending.front();
    mPending.pop_front();
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return true if the Find or Replace prompt is answered from the macro:
///         the command came from a key the player feeds
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::AnswersPrompt(void) const
{
    return (mPlaying == true) && (mFeeding == true);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  request [in,out] current values on entry, the answer on return
/// @param  replace [in]     true for Find and Replace
///
/// @return false if the macro cancels the prompt (Esc) or has no more keys
///         for it
///
/// @brief
/// Reads the dialog keys from the macro, see cMacroDialog. A recorded
/// answer is text, Enter, (replacement, Enter,) option letters, Enter.
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::ReadPrompt(sFindRequest& request, bool replace)
{
    cMacroDialog dialog((replace == true) ? cMacroDialog::DIALOG_REPLACE : cMacroDialog::DIALOG_FIND);
    while (true)
    {
        sKeyStroke key;
        if (NextDialogKey(key) == false)
        {
            return false;
        }
        cMacroDialog::eResult result = dialog.Feed(key);
        if (result == cMacroDialog::DIALOG_CANCEL)
        {
            return false;
        }
        if (result == cMacroDialog::DIALOG_OK)
        {
            break;
        }
    }

    request.text = dialog.Text();
    request.replace = dialog.Replacement();
    ApplyFindOptionLetters(dialog.Options(), request);
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  name [out] the file name typed by the macro
///
/// @return false if the macro cancels the prompt (Esc) or has no more keys
///         for it
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::ReadFileName(std::string& name)
{
    cMacroDialog dialog(cMacroDialog::DIALOG_FILE);
    while (true)
    {
        sKeyStroke key;
        if (NextDialogKey(key) == false)
        {
            return false;
        }
        cMacroDialog::eResult result = dialog.Feed(key);
        if (result == cMacroDialog::DIALOG_CANCEL)
        {
            return false;
        }
        if (result == cMacroDialog::DIALOG_OK)
        {
            name = dialog.Text();
            return true;
        }
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  kind [in]  ACK for a message, QRY for a question
/// @param  yes  [out] for QRY, the answer
///
/// @return true if the IfException after the playing Key statement has a
///         clause for kind (or DEF); the clause runs after the key and the
///         keys that answer the exception (cMacroScript::AnswerLength) are
///         not played
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::AnswerException(eMacroException kind, bool* yes)
{
    if ((mPlaying == false) || (mFeeding == false) || (mFrames.empty() == true))
    {
        return false;
    }

    sFrame& frame = mFrames.back();
    const std::vector<sMacroStatement>& statements = frame.script->Statements();
    if ((frame.pc >= statements.size()) || (statements[frame.pc].kind != MACRO_IF_EXCEPTION))
    {
        return false;
    }

    const std::string name = (kind == MACRO_EXCEPTION_ACK) ? "ACK" : ((kind == MACRO_EXCEPTION_QRY) ? "QRY" : "NCM");
    size_t start = 0;
    bool found = false;
    for (const auto& clause : statements[frame.pc].clauses)
    {
        if (clause.first == name)
        {
            start = clause.second;
            found = true;
            break;
        }
    }
    if ((found == false) && (kind != MACRO_EXCEPTION_NCM))
    {
        for (const auto& clause : statements[frame.pc].clauses)
        {
            if (clause.first == "DEF")
            {
                start = clause.second;
                found = true;
                break;
            }
        }
    }
    if (found == false)
    {
        return false;
    }

    eMacroException answerKind = ((start > 0) && (statements[start - 1].kind == MACRO_CLAUSE) &&
                                  (statements[start - 1].text == "QRY")) ? MACRO_EXCEPTION_QRY : MACRO_EXCEPTION_ACK;
    if (yes != nullptr)
    {
        // One key: Enter or Y is yes. Several keys: yes if they end with
        // Enter, F10 or ^K and do not start with N.
        *yes = false;
        if ((start < statements.size()) && (statements[start].kind == MACRO_KEYS) &&
            (statements[start].keys.empty() == false))
        {
            const std::vector<sKeyStroke>& keys = statements[start].keys;
            size_t length = cMacroScript::AnswerLength(keys, answerKind);
            const sKeyStroke& first = keys.front();
            const sKeyStroke& last = keys[length - 1];
            bool firstNo = (first.kind == KEYSTROKE_TEXT) && (Lower(first.utf8) == "n");
            if (length == 1)
            {
                *yes = (IsEnter(first) == true) || ((first.kind == KEYSTROKE_TEXT) && (Lower(first.utf8) == "y"));
            }
            else
            {
                *yes = (firstNo == false) &&
                       ((IsEnter(last) == true) ||
                        ((last.kind == KEYSTROKE_SPECIAL) && (last.special == SPECIAL_F10)) ||
                        ((last.kind == KEYSTROKE_CHAR) && (last.ch == 'K' - '@')));
            }
        }
    }

    mPending.clear();
    mJump = true;
    mJumpTo = start;
    mDropFirst = (kind != MACRO_EXCEPTION_NCM);
    mAnswerKind = answerKind;
    return true;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  request [in] the prompt answer
/// @param  replace [in] true for Find and Replace
/// @param  ok      [in] false if the prompt was cancelled
///
/// @brief
/// While recording, writes the answer as the keys typed at the WS7
/// prompts; a cancelled prompt is written as Esc. A prompt opened outside
/// FeedKey (a menu command) is preceded by its command key: ^QF or ^QA,
/// in a Modern recording ^F or ^H.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::RecordPrompt(const sFindRequest& request, bool replace, bool ok)
{
    if ((mRecording == false) || (mPauseTyping == true))
    {
        return;
    }

    if (mKeyDepth == 0)
    {
        std::string command;
        if (mRecordMode == INPUT_MODERN)
        {
            command = (replace == true) ? "^H" : "^F";
        }
        else
        {
            command = (replace == true) ? "^QA" : "^QF";
        }
        std::vector<sKeyStroke> keys;
        if (KeyStringToKeys(command, keys) == true)
        {
            mRecorded.insert(mRecorded.end(), keys.begin(), keys.end());
        }
    }

    if (ok == false)
    {
        mRecorded.push_back(SpecialKeyStroke(SPECIAL_ESCAPE));
        return;
    }

    mRecorded.push_back(TextKeyStroke(request.text));
    mRecorded.push_back(SpecialKeyStroke(SPECIAL_ENTER));
    if (replace == true)
    {
        mRecorded.push_back(TextKeyStroke(request.replace));
        mRecorded.push_back(SpecialKeyStroke(SPECIAL_ENTER));
    }
    mRecorded.push_back(TextKeyStroke(FindOptionLetters(request)));
    mRecorded.push_back(SpecialKeyStroke(SPECIAL_ENTER));
}


void cMacroEngine::OnDocumentCleared(void)
{
    if (mPlaying == true)
    {
        mCleared = true;
    }
}
