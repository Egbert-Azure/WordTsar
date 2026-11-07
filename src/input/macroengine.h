#ifndef MACROENGINE_H
#define MACROENGINE_H

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

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "src/input/keystroke.h"

class cEditorBase;
struct sFindRequest;

/// @ingroup Input
/// @{


/////////////////////////////////////////////////////////////////////////////
///
/// @class cMacroStore
///
/// @brief
/// Macro files <name>.txt in the "macros" folder next to the document and
/// in the "macros" folder next to config.ini. Lists the document folder
/// first; on equal names the document folder wins. New macros are saved in
/// the document folder if it exists, otherwise in the config folder, which
/// is created on demand. A missing config folder is created with the
/// default macros when the list is read or a macro is saved there.
///
/////////////////////////////////////////////////////////////////////////////
class cMacroStore
{
public:
    cMacroStore(const std::string& documentDir, const std::string& configDir);

    static bool IsValidName(const std::string& name);       // not empty, no / or :, not starting with '.'
    static std::string FileName(const std::string& name);    // name with ".txt"

    void CreateDefaults(void) const;                          // config folder with the default macros, if missing
    std::vector<std::string> List(void) const;               // macro names without ".txt"
    std::string FindPath(const std::string& name) const;     // file of a listed macro, empty if none
    std::string SavePath(const std::string& name) const;     // file a new macro is saved to, empty if none
    bool Load(const std::string& name, std::vector<std::string>& lines) const;
    bool Save(const std::string& name, const std::vector<std::string>& lines) const;
    std::string SiblingPath(const std::string& name, const std::string& newName) const;  // newName in the folder of name
    bool Rename(const std::string& name, const std::string& newName) const;   // in place, replaces an existing target
    bool Copy(const std::string& name, const std::string& newName) const;     // same folder, replaces an existing target
    bool Delete(const std::string& name) const;

private:
    std::string mDocumentDir;   // empty for an untitled document
    std::string mConfigDir;
};


/////////////////////////////////////////////////////////////////////////////
///
/// @class IMacroKeySink
///
/// @brief
/// Receives the keys of a playing macro. The default sink is the editor's
/// FeedKey; a front end can set its own, e.g. for a screen without a
/// document.
///
/////////////////////////////////////////////////////////////////////////////
class IMacroKeySink
{
public:
    virtual ~IMacroKeySink(void) = default;
    virtual bool FeedMacroKey(const sKeyStroke& key) = 0;     // dispatch one played key
    virtual bool EditsDocument(void) const = 0;               // true while keys go to the document
};


/////////////////////////////////////////////////////////////////////////////
///
/// @class cMacroEngine
///
/// @brief
/// Records the keys fed to cEditorBase::FeedKey and plays macro files back
/// through the key sink (FeedKey by default). Find and Replace prompt
/// answers are recorded as the keys typed at the WS7 prompts and read back
/// from the macro without a dialog.
///
/////////////////////////////////////////////////////////////////////////////
class cMacroEngine
{
public:
    explicit cMacroEngine(cEditorBase& editor);
    ~cMacroEngine(void);

    bool IsRecording(void) const;
    bool IsPlaying(void) const;
    std::string StateText(void) const;                        // "Recording", "Type, then press Enter" or empty

    // FeedKey hooks
    bool InterceptKey(const sKeyStroke& key);                 // true if the key ends or cancels a pause
    IInputHandler* PlaybackInput(void);                       // handler for a macro recorded in the other mode
    void BeginKey(const sKeyStroke& key);
    void EndKey(bool handled);

    // Commands
    void RecordCommand(void);                                 // ^MR: start or stop recording
    void EndRecordingCommand(void);                           // Alt+=: stop recording
    void PauseCommand(void);                                  // Alt+-: write a pause
    void PlayCommand(void);                                   // ^MP: choose a macro and play it
    void EditCommand(void);                                   // ^MD: open a macro file as a text document
    void RenameCommand(void);                                 // ^ME
    void CopyCommand(void);                                   // ^MO
    void DeleteCommand(void);                                 // ^MY
    void Play(const std::vector<std::string>& lines);
    void SetKeySink(IMacroKeySink* sink);                     // nullptr: the editor's FeedKey

    // Find and Replace prompts
    bool AnswersPrompt(void) const;                           // true while a played key runs
    bool ReadPrompt(sFindRequest& request, bool replace);     // answer from the playing macro
    void RecordPrompt(const sFindRequest& request, bool replace, bool ok);

    void OnDocumentCleared(void);

private:
    enum eNext { NEXT_KEY, NEXT_END, NEXT_ERROR };

    IMacroKeySink& Sink(void);
    cMacroStore MakeStore(void) const;
    std::string ChooseMacro(const cMacroStore& store, const std::string& title);   // empty if none or cancelled
    void CopyOrRename(bool rename);
    void DropCurrentKey(void);
    void StopRecording(void);
    void Run(void);
    void Finish(void);
    eNext NextKey(sKeyStroke& key);
    int ReadField(std::string& text);                         // 1 Enter, 0 Esc, -1 end of macro

    cEditorBase& mEditor;
    std::unique_ptr<IMacroKeySink> mEditorSink;
    IMacroKeySink* mSink;                                     // front end sink, nullptr for mEditorSink

    // Recording
    bool mRecording;
    bool mPauseTyping;                                        // typing after a PAUSE, until Enter
    eInputMode mRecordMode;
    std::vector<sKeyStroke> mRecorded;
    long mCurrentKey;                                         // index of the key being dispatched, -1 none
    bool mDropKey;                                            // drop the key being dispatched
    int mKeyDepth;                                            // FeedKey calls in progress

    // Playback
    bool mPlaying;
    bool mPaused;
    bool mCleared;                                            // document cleared during playback
    bool mUndoOpen;                                           // the playback undo group is open
    bool mFeeding;                                            // a played key is being dispatched
    std::vector<std::string> mLines;
    size_t mNextLine;
    std::deque<sKeyStroke> mPending;
    std::unique_ptr<IInputHandler> mPlayInput;
};

/// @}

#endif // MACROENGINE_H
