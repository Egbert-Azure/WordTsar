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
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "src/input/keystroke.h"
#include "src/input/macroscript.h"

class cEditorBase;
struct sFindRequest;

/// @ingroup Input
/// @{


/////////////////////////////////////////////////////////////////////////////
///
/// @class cMacroStore
///
/// @brief
/// Macro files <name>.wsm (WordStar 7 macro source, see KeysToMacroSource)
/// in the "macros" folder next to the document and
/// in the "macros" folder next to config.ini. Lists the document folder
/// first; on equal names the document folder wins. A one-key-per-line
/// <name>.txt in either folder is converted to <name>.wsm when the list is
/// read, unless <name>.wsm exists. New macros are saved in
/// the document folder if it exists, otherwise in the config folder, which
/// is created on demand. A missing config folder is created with the
/// default macros when the list is read or a macro is saved there.
///
/////////////////////////////////////////////////////////////////////////////
class cMacroStore
{
public:
    cMacroStore(const std::string& documentDir, const std::string& configDir);

    static bool IsValidName(const std::string& name);       // not empty, no / or :, not starting with '.', no extension but .wsm
    static std::string TrimName(const std::string& name);    // without spaces and tabs at both ends
    static std::string FileName(const std::string& name);    // name with ".wsm"

    void CreateDefaults(void) const;                          // config folder with the default macros, if missing
    std::vector<std::string> List(std::vector<std::string>* unconverted = nullptr) const;   // macro names without ".wsm"
    std::string FindPath(const std::string& name) const;     // file of a listed macro, empty if none
    std::string SavePath(const std::string& name) const;     // file a new macro is saved to, empty if none
    bool Load(const std::string& name, std::vector<std::string>& lines) const;   // lines up to the first 0x1A
    bool Save(const std::string& name, const std::string& source) const;
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
/// from the macro without a dialog. A macro and the macros it plays are
/// parsed and checked as a whole before the first key (cMacroScript); a
/// refused macro does not run.
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
    bool Play(const std::vector<std::string>& lines, const std::string& name = std::string());   // false if refused (status message); a runaway macro is stopped
    void SetKeySink(IMacroKeySink* sink);                     // nullptr: the editor's FeedKey

    // Find and Replace prompts
    bool AnswersPrompt(void) const;                           // true while a played key runs
    bool ReadPrompt(sFindRequest& request, bool replace);     // answer from the playing macro
    bool ReadFileName(std::string& name);                     // file name from the playing macro; false if cancelled
    void RecordPrompt(const sFindRequest& request, bool replace, bool ok);

    // Exceptions of a played key: true if the IfException after the Key
    // statement answers it; then no dialog is shown. For QRY, yes is the
    // answer: see AnswerException. An ACK or QRY it does not answer stops
    // the macro after the key.
    bool AnswerException(eMacroException kind, bool* yes = nullptr);

    void OnDocumentCleared(void);

private:
    struct sFrame
    {
        std::shared_ptr<const cMacroScript> script;
        std::string name;                                     // macro name, empty if it has none
        size_t pc;                                            // next statement
        std::vector<size_t> calls;                            // return statements of Call
    };

    IMacroKeySink& Sink(void);
    cMacroStore MakeStore(void) const;
    std::string ChooseMacro(const cMacroStore& store, const std::string& title);   // empty if none or cancelled
    void ReportUnconverted(const std::vector<std::string>& unconverted);
    void CopyOrRename(bool rename);
    void DropCurrentKey(void);
    void StopRecording(void);
    bool LoadScripts(const std::string& name, std::shared_ptr<cMacroScript> script, std::string& error);
    void Run(void);
    void Finish(void);
    void EndFrame(void);
    void StopRunaway(const std::string& reason, size_t line);
    void StopUnanswered(eMacroException kind);
    void ApplySetting(const sMacroStatement& statement);
    void SaveSettings(void);
    void RestoreSettings(void);
    bool NextDialogKey(sKeyStroke& key);
    bool IsPauseEnd(const sKeyStroke& key) const;

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
    bool mPaused;                                             // at PauseForInput or PauseForKey
    bool mPauseForKey;                                        // at PauseForKey
    bool mResumeAfterKey;                                     // run on after the PauseForKey key
    sKeyStroke mPauseEnd;                                     // PauseForInput end key
    std::string mPauseMessage;
    long mPauseTyped;                                         // keys typed at PauseForInput
    bool mCleared;                                            // document cleared during playback
    bool mUndoOpen;                                           // the playback undo group is open
    bool mFeeding;                                            // a played key is being dispatched
    std::map<std::string, std::shared_ptr<const cMacroScript>> mScripts;   // macro and those it plays, by lower case name
    std::vector<sFrame> mFrames;                              // playing macros, innermost last
    std::deque<sKeyStroke> mPending;                          // rest of the Key statement
    bool mJump;                                               // an exception clause was chosen
    size_t mJumpTo;
    bool mDropFirst;                                          // the clause's first keys answered the exception
    eMacroException mAnswerKind;                              // how many keys answered it, see cMacroScript::AnswerLength
    long mStatementsWithoutKey;                               // statements run since the last played key
    size_t mKeyLine;                                          // line of the Key statement being played
    bool mUnanswered;                                         // a played key raised an exception no clause answers
    std::string mStopText;                                    // "Macro NAME stopped at line N" for mUnanswered
    std::unique_ptr<IInputHandler> mPlayInput;

    // Settings at the start, restored at the end after AutoRestore(ON)
    bool mAutoRestore;
    bool mSavedInsert;
    int mSavedShowControl;
    bool mSavedShowDot;
    int mSavedHelpLevel;
};

/// @}

#endif // MACROENGINE_H
