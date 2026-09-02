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

#include <QtPrintSupport/qtprintsupportglobal.h>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QtWidgets>

#include "../editor/editorctrl.h"
#include "src/core/layout/layoutbase.h"

/////////////////////////////////////////////////////////////////////////////
///
/// @class cPrintout
///
/// @brief
/// Printing support for the new layout engine (Phase 0.6).
/// Uses the box-based layout with absolute page coordinates for printing.
///
/// The new layout engine stores all positions in absolute page coordinates
/// (twips), making printing trivial - we just use pagex/pagey directly
/// with QPainter coordinate transformation.
///
/////////////////////////////////////////////////////////////////////////////
class cPrintout : public QWidget
{
    Q_OBJECT

public:
    cPrintout(cEditorCtrl* editor, const QString &title = "Print Preview");
    virtual ~cPrintout(void);

    void PrintPreview(void);
    void PrintDocument(void);

public slots:
    void printDocument(QPrinter *printer);
    void printPage(int pageNum, QPainter *painter);

protected:
    void DrawLine(const sLineLayout &line, cDocument* document, QPainter *painter);
    void DrawSegment(const sSegmentLayout &segment, cDocument* document, COORD_T x, COORD_T y,
                    QPainter *painter);
    void DrawHeadersFooters(int pageNum, cDocument* document, QPainter *painter);
    void DrawHeaderFooterLine(const sHeaderFooterLine &hfLine, QPainter *painter);

private:
    cEditorCtrl *mEditor;
    cLayoutBase *mLayout;
    double mScale;  ///< Scale factor (pixels per twip) for font conversion
};

#endif // PRINTOUT_H
