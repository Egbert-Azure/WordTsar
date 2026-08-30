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

#ifndef WORDTSAR_TUI_TERMINALDRIVERWIN32_H
#define WORDTSAR_TUI_TERMINALDRIVERWIN32_H

#include "terminaldriver.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>

namespace wordstartui
{

class cTerminalDriverWin32 final : public cTerminalDriver
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cTerminalDriverWin32(void);
    ~cTerminalDriverWin32(void) override;

    bool Open(void) override;
    void Close(void) override;
    sTerminalSize GetSize(void) const override;
    sTerminalCapabilities GetCapabilities(void) const override;
    bool ReadEvent(sInputEvent& event, int timeoutMs) override;
    void Present(const std::vector<sCellRun>& runs) override;
    void SetCursor(int row, int col, bool visible) override;
    void SetCursorShape(eCursorShape shape) override;
    void Flush(void) override;

private:
    void InitializeEvent(sInputEvent& event) const;
#ifdef _WIN32
    bool DecodeKeyEvent(sInputEvent& event, const KEY_EVENT_RECORD& keyEvent);
    bool DecodeMouseEvent(sInputEvent& event, const MOUSE_EVENT_RECORD& mouseEvent);
#endif
    std::string WideToUtf8(wchar_t value) const;
    std::string BuildAnsiStyle(const sStyle& style, const sStyle& previous, bool force) const;
    std::string MoveSequence(int row, int col) const;
    int AnsiColor(eColor color, bool foreground) const;
    void WriteString(const std::string& text);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
#ifdef _WIN32
    HANDLE mInputHandle;
    HANDLE mOutputHandle;
    DWORD mOriginalInputMode;
    DWORD mOriginalOutputMode;
    UINT mOriginalInputCodePage;
    UINT mOriginalOutputCodePage;
#endif
    bool mOpen;
    sTerminalCapabilities mCapabilities;
    eCursorShape mCursorShape;
};

}

#endif
