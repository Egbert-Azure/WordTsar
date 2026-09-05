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

#include "printout.h"

#include "pdfprintout.h"
#include "pdfpreviewwindow.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

/////////////////////////////////////////////////////////////////////////////
cPrintout::cPrintout(cEditorCtrl* editor, const QString &title)
        : QWidget(editor)
{
    mEditor = editor;
    mTitle = title;
}


/////////////////////////////////////////////////////////////////////////////
cPrintout::~cPrintout(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Generates a real PDF of the document (cGUIPDFPrintout, Quartz/Core Text)
/// to a temp file, then shows it in a native PDFKit preview window whose
/// own Print button prints that same PDF -- no second, independent render
/// pass, unlike the old QPrintPreviewDialog path.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::PrintPreview(void)
{
    // Build a temp file path: <temp>/WordTsar-<basename>-preview-<uuid>.pdf.
    // The uuid avoids collisions if a previous preview's temp file is still
    // open in the PDFKit window (modal, but belt-and-suspenders).
    QString baseName = QFileInfo(mEditor->mFileName.c_str()).completeBaseName();
    if (baseName.isEmpty())
    {
        baseName = "Untitled";
    }
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString pdfPath = QDir(tempDir).filePath(
        QString("WordTsar-%1-preview-%2.pdf").arg(baseName, uuid));

    cGUIPDFPrintout pdfPrinter(mEditor);
    bool success = pdfPrinter.GeneratePDF(pdfPath.toStdString());

    if (!success)
    {
        QMessageBox::warning(mEditor, tr("Print Preview"),
            tr("Failed to generate the PDF for print preview."));
        return;
    }

    ShowPDFPrintPreview(pdfPath.toStdString(), mTitle.toStdString());

    // The preview window is modal and has closed by the time control
    // returns here -- safe to clean up the temp file now.
    QFile::remove(pdfPath);
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// See the class-doc note in printout.h -- there is no separate "print
/// without preview" path any more now that both are PDF-based; the preview
/// window's own Print button is the print path.
///
/////////////////////////////////////////////////////////////////////////////
void cPrintout::PrintDocument(void)
{
    PrintPreview();
}
