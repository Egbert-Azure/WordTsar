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

#ifndef GUI_PDFPREVIEWWINDOW_H
#define GUI_PDFPREVIEWWINDOW_H

#include <string>

/////////////////////////////////////////////////////////////////////////////
///
/// @param  pdfPath [in] - Path to an already-generated PDF file
/// @param  windowTitle [in] - Title for the preview window
///
/// @return nothing
///
/// @brief
/// Shows a native macOS PDFKit preview window (a PDFView, with zoom and
/// page-navigation controls) for the PDF at pdfPath, and a Print button
/// that prints that same PDF via NSPrintOperation -- so preview and print
/// are provably the same bytes, not two independent renders through Qt's
/// print pipeline. Modal: blocks the calling thread until the window is
/// closed, matching the QPrintPreviewDialog::exec() behaviour this replaces.
///
/// Implemented in pdfpreviewwindow.mm (Objective-C++); this header is
/// plain C++ so it can be included from Qt-side code without pulling in
/// Objective-C.
///
/////////////////////////////////////////////////////////////////////////////
void ShowPDFPrintPreview(const std::string& pdfPath, const std::string& windowTitle);

#endif // GUI_PDFPREVIEWWINDOW_H
