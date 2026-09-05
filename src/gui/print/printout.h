//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2018 Gerald Brandt
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

#ifndef PRINTOUT_H
#define PRINTOUT_H

#include <QtWidgets>

#include "../editor/editorctrl.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @class cPrintout
///
/// @brief
/// Printing support entry point. Generates a real PDF via cGUIPDFPrintout
/// (Quartz/Core Text, no Qt print pipeline involved) and shows it in a
/// native PDFKit preview window (pdfpreviewwindow.mm) whose own Print
/// button prints that same PDF -- so preview and print are provably the
/// same bytes.
///
/// Replaces the previous QPrintPreviewDialog/QPrinter-based renderer: that
/// path recomputed a font-size scale factor from QPrinter::resolution() on
/// every paint pass, and Qt invokes that pass twice per preview-then-print
/// session (once for on-screen preview, once more for the committed print)
/// with no guarantee the two agree -- confirmed to corrupt real printed
/// pages, not just the interactive preview window.
///
/////////////////////////////////////////////////////////////////////////////
class cPrintout : public QWidget
{
    Q_OBJECT

public:
    cPrintout(cEditorCtrl* editor, const QString &title = "Print Preview");
    virtual ~cPrintout(void);

    void PrintPreview(void);

    // No separate "print without preview" path any more -- see
    // KEY_MAPPING.md's [^print-deviation] footnote: the GUI's ^K,P and
    // File > Print have only ever reached PrintPreview() in practice, and
    // the preview window's own Print button is the sole print path now
    // that both are PDF-based. Kept as an alias so any future caller of
    // the historical PrintDocument() name still gets a working print path.
    void PrintDocument(void);

private:
    cEditorCtrl *mEditor;
    QString mTitle;
};

#endif // PRINTOUT_H
