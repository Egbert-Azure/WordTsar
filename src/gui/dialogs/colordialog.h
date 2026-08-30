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

#ifndef GUI_DIALOGS_COLORDIALOG_H
#define GUI_DIALOGS_COLORDIALOG_H

#include <QDialog>
#include <QWidget>
#include <QColor>

#include "src/core/document/doctstructs.h"

class QSpinBox ;
class QLineEdit ;
class QCheckBox ;
class QPaintEvent ;
class QMouseEvent ;


class cSVSquare : public QWidget
{
    Q_OBJECT

public:
    explicit cSVSquare(QWidget *parent = nullptr) ;

    void SetHue(qreal hue) ;
    void SetSatVal(qreal saturation, qreal value) ;
    qreal Saturation(void) const ;
    qreal Value(void) const ;

signals:
    void changed(void) ;

protected:
    void paintEvent(QPaintEvent *event) override ;
    void mousePressEvent(QMouseEvent *event) override ;
    void mouseMoveEvent(QMouseEvent *event) override ;

private:
    void UpdateFromPoint(int x, int y) ;

    qreal mHue ;
    qreal mSat ;
    qreal mVal ;
} ;


class cHueBar : public QWidget
{
    Q_OBJECT

public:
    explicit cHueBar(QWidget *parent = nullptr) ;

    void SetHue(qreal hue) ;
    qreal Hue(void) const ;

signals:
    void changed(void) ;

protected:
    void paintEvent(QPaintEvent *event) override ;
    void mousePressEvent(QMouseEvent *event) override ;
    void mouseMoveEvent(QMouseEvent *event) override ;

private:
    void UpdateFromPoint(int y) ;

    qreal mHue ;
} ;


class cColorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit cColorDialog(QWidget *parent = nullptr) ;
    ~cColorDialog(void) ;

    sSeqRGBColor GetSelectedColor(void) const ;

private slots:
    void OnSquareChanged(void) ;
    void OnHueChanged(void) ;
    void OnRGBChanged(void) ;
    void OnHexChanged(void) ;
    void OnDefaultToggled(bool checked) ;

private:
    void SetColor(const QColor &color) ;
    void SyncFromPicker(void) ;
    void UpdatePreview(void) ;
    QColor CurrentColor(void) const ;

    cSVSquare *mSquare ;
    cHueBar *mHueBar ;
    QSpinBox *mSpinRed ;
    QSpinBox *mSpinGreen ;
    QSpinBox *mSpinBlue ;
    QLineEdit *mHexEdit ;
    QCheckBox *mCheckDefault ;
    QWidget *mPreviewSwatch ;

    bool mUseDefault ;
    bool mUpdating ;
} ;


#endif // GUI_DIALOGS_COLORDIALOG_H
