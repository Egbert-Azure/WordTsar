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

#ifndef DOCUMENTLISTENER_H
#define DOCUMENTLISTENER_H

#include "src/core/include/config.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @class cDocumentListener
///
/// @brief
/// Interface for objects that need to be notified when document content
/// changes. Implementations register with cDocument via AddListener()
/// and receive callbacks on every content mutation.
///
/// Notifications are suppressed during file loading (mIsLoading) and
/// undo/redo replay (mUndoDisabled) to avoid excessive callbacks during
/// batch operations. The GUI layer handles layout for those cases
/// explicitly via LayoutDocument().
///
/////////////////////////////////////////////////////////////////////////////
class cDocumentListener
{
public:
    virtual ~cDocumentListener(void) = default;

    /////////////////////////////////////////////////////////////////////////
    ///
    /// @param  PARAGRAPH_T fromParagraph [in] first paragraph affected
    ///
    /// @return nothing
    ///
    /// @brief
    /// Called after any content mutation (insert, delete, format change).
    /// The fromParagraph parameter indicates where the change started.
    /// Listeners should mark themselves dirty and relayout on next idle.
    ///
    /////////////////////////////////////////////////////////////////////////
    virtual void OnDocumentChanged(PARAGRAPH_T fromParagraph) = 0;

    /////////////////////////////////////////////////////////////////////////
    ///
    /// @return nothing
    ///
    /// @brief
    /// Called after Clear() -- entire document has been replaced.
    /// Listeners should discard all cached state and do a full relayout.
    ///
    /////////////////////////////////////////////////////////////////////////
    virtual void OnDocumentCleared(void) = 0;
};

#endif // DOCUMENTLISTENER_H
