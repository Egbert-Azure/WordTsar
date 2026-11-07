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
#include <system_error>

#include "src/core/editor/editorbase.h"
#include "src/core/utils/config.h"
#include "src/input/defaultmacros.h"
#include "src/input/moderninput.h"
#include "src/input/wordtsarinput.h"


namespace
{

const char MACRO_FOLDER[] = "macros";
const char MACRO_EXTENSION[] = ".txt";
const char MODERN_LINE[] = "Modern";
const char STATE_RECORDING[] = "Recording";
const char STATE_PAUSED[] = "Type, then press Enter";


bool EndsWith(const std::string& text, const std::string& suffix)
{
    return (text.size() >= suffix.size()) &&
           (text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0);
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


// Macro names (file names without ".txt") of the *.txt files in a folder.
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
        if ((entry.is_regular_file(ec) == true) && (EndsWith(file, MACRO_EXTENSION) == true) &&
            (file.size() > 4) && (file[0] != '.'))
        {
            names.push_back(file.substr(0, file.size() - 4));
        }
    }

    std::sort(names.begin(), names.end());
    return names;
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
    return (name.empty() == false) && (name[0] != '.') &&
           (name.find('/') == std::string::npos) && (name.find(':') == std::string::npos);
}


std::string cMacroStore::FileName(const std::string& name)
{
    if (EndsWith(name, MACRO_EXTENSION) == true)
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


std::vector<std::string> cMacroStore::List(void) const
{
    CreateDefaults();
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

    lines.clear();
    std::string line;
    while (std::getline(in, line))
    {
        lines.push_back(StripLineEnd(line));
    }
    return true;
}


bool cMacroStore::Save(const std::string& name, const std::vector<std::string>& lines) const
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
    for (const std::string& line : lines)
    {
        out << line << '\n';
    }
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
    , mCleared(false)
    , mUndoOpen(false)
    , mFeeding(false)
    , mNextLine(0)
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
        return STATE_PAUSED;
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
/// At a playback pause Enter continues the macro and Esc cancels it. While
/// recording, the Enter that ends the text typed after a pause ends the
/// pause. Neither Enter is inserted or recorded.
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::InterceptKey(const sKeyStroke& key)
{
    if ((mPlaying == true) && (mPaused == true))
    {
        if (IsEnter(key) == true)
        {
            mPaused = false;
            Run();
            return true;
        }
        if (IsEscape(key) == true)
        {
            mEditor.SetStatusMessage("Macro stopped");
            Finish();
            return true;
        }
        return false;
    }

    if ((mRecording == true) && (mPauseTyping == true) && (IsEnter(key) == true))
    {
        mPauseTyping = false;
        return true;
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

    if (KeysToLines(std::vector<sKeyStroke>{ key }, INPUT_MODERN).empty() == true)
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

    std::vector<std::string> lines = KeysToLines(keys, mRecordMode);
    if (mRecordMode == INPUT_MODERN)
    {
        lines.insert(lines.begin(), MODERN_LINE);
    }

    mEditor.Repaint();

    std::string name = mEditor.PromptText("Save Macro", "Macro name:");
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

    if (store.Save(name, lines) == false)
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
    std::vector<std::string> names = store.List();
    if (names.empty() == true)
    {
        mEditor.SetStatusMessage("No macros yet. Record one with ^MR.");
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

    Play(lines);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  store [in] macro folders
/// @param  title [in] list title
///
/// @return the chosen macro name; empty if there are none (status "No
///         macros") or the list was cancelled
///
/////////////////////////////////////////////////////////////////////////////
std::string cMacroEngine::ChooseMacro(const cMacroStore& store, const std::string& title)
{
    std::vector<std::string> names = store.List();
    if (names.empty() == true)
    {
        mEditor.SetStatusMessage("No macros");
        return std::string();
    }
    return mEditor.ChooseFromList(title, names);
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

    std::string newName = mEditor.PromptText(title, (rename == true) ? "New name for " + name + ":"
                                                                      : "Copy " + name + " to:");
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
/// @param  lines [in] macro file lines
///
/// @brief
/// Plays the lines through the key sink as one undo step, up to the end or
/// the first PAUSE. The undo step starts when the keys first reach a
/// document.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::Play(const std::vector<std::string>& lines)
{
    if ((mPlaying == true) || (mRecording == true))
    {
        return;
    }

    mLines = lines;
    mNextLine = 0;
    mPending.clear();
    mPaused = false;
    mCleared = false;

    bool modern = (mLines.empty() == false) && (mLines[0] == MODERN_LINE);
    if (modern == true)
    {
        mNextLine = 1;
    }

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
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief
/// Feeds keys until the macro ends, pauses, fails or a key closes the
/// document. Runs the editor's after-key update after every key. A key fed
/// to a sink without a document may load one; that does not stop playback.
///
/////////////////////////////////////////////////////////////////////////////
void cMacroEngine::Run(void)
{
    while ((mPlaying == true) && (mPaused == false))
    {
        sKeyStroke key;
        eNext next = NextKey(key);
        if (next != NEXT_KEY)
        {
            Finish();
            return;
        }

        if (key.kind == KEYSTROKE_PAUSE)
        {
            mPaused = true;
            mEditor.AfterMacroKey();
            mEditor.Repaint();
            return;
        }

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

        bool stop = (mCleared == true) || (mEditor.MacroStopRequested() == true);
        mEditor.AfterMacroKey();
        if (stop == true)
        {
            Finish();
            return;
        }
    }
}


void cMacroEngine::Finish(void)
{
    if (mPlaying == false)
    {
        return;
    }

    mPlaying = false;
    mPaused = false;
    mPending.clear();
    mLines.clear();
    mPlayInput.reset();
    if (mUndoOpen == true)
    {
        mUndoOpen = false;
        mEditor.EndMacroUndo(mCleared == false);
    }
    mEditor.Repaint();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  key [out] next key of the macro
///
/// @return NEXT_KEY, NEXT_END at the end, NEXT_ERROR after an unknown key
///         name (with a status message)
///
/////////////////////////////////////////////////////////////////////////////
cMacroEngine::eNext cMacroEngine::NextKey(sKeyStroke& key)
{
    while (mPending.empty() == true)
    {
        if (mNextLine >= mLines.size())
        {
            return NEXT_END;
        }

        std::string line = StripLineEnd(mLines[mNextLine]);
        mNextLine++;
        if (line.empty() == true)
        {
            continue;
        }

        std::vector<sKeyStroke> keys;
        if (NameToKeys(line, keys) == false)
        {
            mEditor.SetStatusMessage("Unknown key on line " + std::to_string(mNextLine), false, 60, true);
            return NEXT_ERROR;
        }
        mPending.insert(mPending.end(), keys.begin(), keys.end());
    }

    key = mPending.front();
    mPending.pop_front();
    return NEXT_KEY;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  text [out] typed text of one prompt field
///
/// @return 1 for Enter, 0 for Esc, -1 at the end of the macro or a pause
///
/////////////////////////////////////////////////////////////////////////////
int cMacroEngine::ReadField(std::string& text)
{
    text.clear();

    while (true)
    {
        sKeyStroke key;
        if (NextKey(key) != NEXT_KEY)
        {
            mNextLine = mLines.size();
            return -1;
        }

        if (key.kind == KEYSTROKE_PAUSE)
        {
            mPending.push_front(key);
            return -1;
        }
        if (IsEnter(key) == true)
        {
            return 1;
        }
        if (IsEscape(key) == true)
        {
            return 0;
        }
        if ((key.kind == KEYSTROKE_SPECIAL) && (key.special == SPECIAL_BACKSPACE))
        {
            while ((text.empty() == false) && ((static_cast<unsigned char>(text.back()) & 0xC0U) == 0x80U))
            {
                text.pop_back();
            }
            if (text.empty() == false)
            {
                text.pop_back();
            }
            continue;
        }
        if ((key.kind == KEYSTROKE_TEXT) && (key.ctrl == false) && (key.alt == false))
        {
            text += key.utf8;
        }
    }
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
/// @return false if the macro cancels the prompt (Esc) or ends inside it
///
/// @brief
/// Reads the prompt answer from the macro: text, Enter, (replacement,
/// Enter,) option letters, Enter.
///
/////////////////////////////////////////////////////////////////////////////
bool cMacroEngine::ReadPrompt(sFindRequest& request, bool replace)
{
    std::string text;
    if (ReadField(text) != 1)
    {
        return false;
    }

    std::string replacement;
    if ((replace == true) && (ReadField(replacement) != 1))
    {
        return false;
    }

    std::string letters;
    if (ReadField(letters) != 1)
    {
        return false;
    }

    request.text = text;
    request.replace = replacement;
    ApplyFindOptionLetters(letters, request);
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
        if (NameToKeys(command, keys) == true)
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
