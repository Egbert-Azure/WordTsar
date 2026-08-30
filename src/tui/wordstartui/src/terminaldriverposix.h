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

#ifndef WORDTSAR_TUI_TERMINALDRIVERPOSIX_H
#define WORDTSAR_TUI_TERMINALDRIVERPOSIX_H

#include "terminaldriver.h"

#ifndef _WIN32
#include <termios.h>
#endif

#include <string>
#include <vector>

namespace wordstartui
{

class cTerminalDriverPosix final : public cTerminalDriver
{
    // =================================================================
    // METHODS
    // =================================================================

public:
    cTerminalDriverPosix(void);
    ~cTerminalDriverPosix(void) override;

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
    bool ReadByte(uint8_t& byte, int timeoutMs);
    bool DecodeBytes(sInputEvent& event, const std::vector<uint8_t>& bytes);
    bool DecodeEscape(sInputEvent& event, const std::vector<uint8_t>& bytes);
    bool DecodeCsi(sInputEvent& event, const std::string& sequence);
    bool DecodeSgrMouse(sInputEvent& event, const std::string& sequence) const;
    bool DecodeUtf8Text(sInputEvent& event, const std::vector<uint8_t>& bytes);
    std::string BuildAnsiStyle(const sStyle& style, const sStyle& previous, bool force) const;
    std::string MoveSequence(int row, int col) const;
    int AnsiColor(eColor color, bool foreground) const;
    void WriteString(const std::string& text);
    void DetectCapabilities(void);

    // =================================================================
    // MEMBER VARIABLES
    // =================================================================

private:
#ifndef _WIN32
    termios mOriginalTermios;
#endif
    bool mOpen;
    bool mHaveOriginalTermios;
    sTerminalCapabilities mCapabilities;
    mutable sTerminalSize mSize;
    std::vector<uint8_t> mPendingBytes;
    bool mCursorVisible;
    eCursorShape mCursorShape;
};

}

#endif
