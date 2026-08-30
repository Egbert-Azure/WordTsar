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

/**
 * @class cColorDialog
 *
 * @brief Custom text-color dialog matching the terminal UI color picker.
 *
 * Presents a saturation/value square and a hue bar (like the wstui color
 * picker), plus R/G/B spin boxes, a hex entry field, a live preview swatch and
 * a "Use Default Color" checkbox. GetSelectedColor() returns the chosen color,
 * or the sentinel {-1,-1,-1,-1} when "Use Default Color" is checked.
 *
 * @author Gerald Brandt
 * @copyright GNU Affero General Public License v3.0
 */

#include <QPainter>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QLinearGradient>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QDialogButtonBox>

#include "src/gui/dialogs/colordialog.h"


/////////////////////////////////////////////////////////////////////////////
///
/// @param  qreal hue [in] hue in the range 0-1
///
/// @return the hue clamped into the range Qt accepts for fromHsvF (0 to <1)
///
/// @brief
/// Keep a hue inside the half-open range QColor::fromHsvF requires.
///
/////////////////////////////////////////////////////////////////////////////
static qreal ClampHue(qreal hue)
{
    if (hue < 0.0)
    {
        return 0.0 ;
    }
    if (hue > 0.999999)
    {
        return 0.999999 ;
    }
    return hue ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  qreal value [in] a normalized component
///
/// @return the value clamped to 0-1
///
/// @brief
/// Clamp a saturation or value component to the unit range.
///
/////////////////////////////////////////////////////////////////////////////
static qreal ClampUnit(qreal value)
{
    if (value < 0.0)
    {
        return 0.0 ;
    }
    if (value > 1.0)
    {
        return 1.0 ;
    }
    return value ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QWidget *parent [in] parent widget
///
/// @return nothing
///
/// @brief
/// Construct the saturation/value square.
///
/////////////////////////////////////////////////////////////////////////////
cSVSquare::cSVSquare(QWidget *parent) : QWidget(parent)
{
    mHue = 0.0 ;
    mSat = 1.0 ;
    mVal = 1.0 ;
    setMinimumSize(200, 160) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  qreal hue [in] the hue (0-1) whose plane to display
///
/// @return nothing
///
/// @brief
/// Set the hue whose saturation/value plane is shown and repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cSVSquare::SetHue(qreal hue)
{
    mHue = hue ;
    update() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  qreal saturation [in] saturation 0-1
/// @param  qreal value [in] value 0-1
///
/// @return nothing
///
/// @brief
/// Set the selection point and repaint.
///
/////////////////////////////////////////////////////////////////////////////
void cSVSquare::SetSatVal(qreal saturation, qreal value)
{
    mSat = ClampUnit(saturation) ;
    mVal = ClampUnit(value) ;
    update() ;
}


qreal cSVSquare::Saturation(void) const
{
    return mSat ;
}


qreal cSVSquare::Value(void) const
{
    return mVal ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QPaintEvent *event [in] paint event (unused)
///
/// @return nothing
///
/// @brief
/// Paint the saturation/value gradient for the current hue and the marker.
///
/////////////////////////////////////////////////////////////////////////////
void cSVSquare::paintEvent(QPaintEvent *event)
{
    (void)event ;

    QPainter painter(this) ;
    int w = width() ;
    int h = height() ;

    painter.fillRect(rect(), QColor::fromHsvF(ClampHue(mHue), 1.0, 1.0)) ;

    QLinearGradient satGrad(0, 0, w, 0) ;
    satGrad.setColorAt(0.0, QColor(255, 255, 255, 255)) ;
    satGrad.setColorAt(1.0, QColor(255, 255, 255, 0)) ;
    painter.fillRect(rect(), satGrad) ;

    QLinearGradient valGrad(0, 0, 0, h) ;
    valGrad.setColorAt(0.0, QColor(0, 0, 0, 0)) ;
    valGrad.setColorAt(1.0, QColor(0, 0, 0, 255)) ;
    painter.fillRect(rect(), valGrad) ;

    int mx = static_cast<int>(mSat * (w - 1)) ;
    int my = static_cast<int>((1.0 - mVal) * (h - 1)) ;

    painter.setPen(QPen(Qt::black, 1)) ;
    painter.drawEllipse(QPoint(mx, my), 5, 5) ;
    painter.setPen(QPen(Qt::white, 1)) ;
    painter.drawEllipse(QPoint(mx, my), 4, 4) ;
}


void cSVSquare::mousePressEvent(QMouseEvent *event)
{
    UpdateFromPoint(static_cast<int>(event->position().x()), static_cast<int>(event->position().y())) ;
}


void cSVSquare::mouseMoveEvent(QMouseEvent *event)
{
    UpdateFromPoint(static_cast<int>(event->position().x()), static_cast<int>(event->position().y())) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  int x [in] mouse x within the widget
/// @param  int y [in] mouse y within the widget
///
/// @return nothing
///
/// @brief
/// Update the selection from a mouse position and notify listeners.
///
/////////////////////////////////////////////////////////////////////////////
void cSVSquare::UpdateFromPoint(int x, int y)
{
    int w = width() ;
    int h = height() ;

    if (w > 1)
    {
        mSat = ClampUnit(static_cast<qreal>(x) / static_cast<qreal>(w - 1)) ;
    }
    if (h > 1)
    {
        mVal = ClampUnit(1.0 - (static_cast<qreal>(y) / static_cast<qreal>(h - 1))) ;
    }

    update() ;
    emit changed() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QWidget *parent [in] parent widget
///
/// @return nothing
///
/// @brief
/// Construct the vertical hue bar.
///
/////////////////////////////////////////////////////////////////////////////
cHueBar::cHueBar(QWidget *parent) : QWidget(parent)
{
    mHue = 0.0 ;
    setFixedWidth(24) ;
    setMinimumHeight(160) ;
}


void cHueBar::SetHue(qreal hue)
{
    mHue = ClampUnit(hue) ;
    update() ;
}


qreal cHueBar::Hue(void) const
{
    return mHue ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QPaintEvent *event [in] paint event (unused)
///
/// @return nothing
///
/// @brief
/// Paint the hue gradient and the current-hue marker.
///
/////////////////////////////////////////////////////////////////////////////
void cHueBar::paintEvent(QPaintEvent *event)
{
    (void)event ;

    QPainter painter(this) ;
    int h = height() ;

    QLinearGradient grad(0, 0, 0, h) ;
    for (int loop = 0 ; loop <= 6 ; ++loop)
    {
        qreal stop = static_cast<qreal>(loop) / 6.0 ;
        grad.setColorAt(stop, QColor::fromHsvF(ClampHue(stop), 1.0, 1.0)) ;
    }
    painter.fillRect(rect(), grad) ;

    int my = static_cast<int>(mHue * (h - 1)) ;
    painter.setPen(QPen(Qt::black, 1)) ;
    painter.drawLine(0, my, width() - 1, my) ;
    painter.setPen(QPen(Qt::white, 1)) ;
    painter.drawLine(0, my - 1, width() - 1, my - 1) ;
}


void cHueBar::mousePressEvent(QMouseEvent *event)
{
    UpdateFromPoint(static_cast<int>(event->position().y())) ;
}


void cHueBar::mouseMoveEvent(QMouseEvent *event)
{
    UpdateFromPoint(static_cast<int>(event->position().y())) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  int y [in] mouse y within the widget
///
/// @return nothing
///
/// @brief
/// Update the hue from a mouse position and notify listeners.
///
/////////////////////////////////////////////////////////////////////////////
void cHueBar::UpdateFromPoint(int y)
{
    int h = height() ;

    if (h > 1)
    {
        mHue = ClampUnit(static_cast<qreal>(y) / static_cast<qreal>(h - 1)) ;
    }

    update() ;
    emit changed() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  QWidget *parent [in] parent widget
///
/// @return nothing
///
/// @brief
/// Build the color dialog: saturation/value square, hue bar, preview, hex and
/// RGB entry, the "Use Default Color" checkbox and OK/Cancel buttons.
///
/////////////////////////////////////////////////////////////////////////////
cColorDialog::cColorDialog(QWidget *parent) : QDialog(parent)
{
    mUseDefault = false ;
    mUpdating = false ;

    setWindowTitle(tr("Select Text Color")) ;

    mSquare = new cSVSquare(this) ;
    mHueBar = new cHueBar(this) ;

    mPreviewSwatch = new QWidget(this) ;
    mPreviewSwatch->setFixedSize(80, 48) ;
    mPreviewSwatch->setAutoFillBackground(true) ;

    mHexEdit = new QLineEdit(this) ;
    mHexEdit->setMaxLength(7) ;
    mHexEdit->setFixedWidth(80) ;

    mSpinRed = new QSpinBox(this) ;
    mSpinGreen = new QSpinBox(this) ;
    mSpinBlue = new QSpinBox(this) ;
    mSpinRed->setRange(0, 255) ;
    mSpinGreen->setRange(0, 255) ;
    mSpinBlue->setRange(0, 255) ;

    mCheckDefault = new QCheckBox(tr("Use Default Color"), this) ;

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this) ;

    // Picker row: square, hue bar, and a preview + hex column.
    QVBoxLayout *sideLayout = new QVBoxLayout() ;
    sideLayout->addWidget(mPreviewSwatch) ;
    QLabel *hexLabel = new QLabel(tr("Hex:"), this) ;
    sideLayout->addWidget(hexLabel) ;
    sideLayout->addWidget(mHexEdit) ;
    sideLayout->addStretch(1) ;

    QHBoxLayout *pickerLayout = new QHBoxLayout() ;
    pickerLayout->addWidget(mSquare, 1) ;
    pickerLayout->addWidget(mHueBar) ;
    pickerLayout->addLayout(sideLayout) ;

    // RGB entry row.
    QGridLayout *rgbLayout = new QGridLayout() ;
    rgbLayout->addWidget(new QLabel(tr("R:"), this), 0, 0) ;
    rgbLayout->addWidget(mSpinRed, 0, 1) ;
    rgbLayout->addWidget(new QLabel(tr("G:"), this), 0, 2) ;
    rgbLayout->addWidget(mSpinGreen, 0, 3) ;
    rgbLayout->addWidget(new QLabel(tr("B:"), this), 0, 4) ;
    rgbLayout->addWidget(mSpinBlue, 0, 5) ;
    rgbLayout->setColumnStretch(6, 1) ;

    QVBoxLayout *mainLayout = new QVBoxLayout(this) ;
    mainLayout->addLayout(pickerLayout) ;
    mainLayout->addLayout(rgbLayout) ;
    mainLayout->addWidget(mCheckDefault) ;
    mainLayout->addWidget(buttons) ;

    connect(mSquare, &cSVSquare::changed, this, &cColorDialog::OnSquareChanged) ;
    connect(mHueBar, &cHueBar::changed, this, &cColorDialog::OnHueChanged) ;
    connect(mSpinRed, &QSpinBox::valueChanged, this, &cColorDialog::OnRGBChanged) ;
    connect(mSpinGreen, &QSpinBox::valueChanged, this, &cColorDialog::OnRGBChanged) ;
    connect(mSpinBlue, &QSpinBox::valueChanged, this, &cColorDialog::OnRGBChanged) ;
    connect(mHexEdit, &QLineEdit::editingFinished, this, &cColorDialog::OnHexChanged) ;
    connect(mCheckDefault, &QCheckBox::toggled, this, &cColorDialog::OnDefaultToggled) ;
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept) ;
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject) ;

    SetColor(QColor(0, 0, 0)) ;
}


cColorDialog::~cColorDialog(void)
{
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return sSeqRGBColor - the selected color, or {-1,-1,-1,-1} for default
///
/// @brief
/// Return the color the user selected, or the sentinel {-1,-1,-1,-1} when
/// "Use Default Color" is checked.
///
/////////////////////////////////////////////////////////////////////////////
sSeqRGBColor cColorDialog::GetSelectedColor(void) const
{
    sSeqRGBColor color ;

    if (mUseDefault == true)
    {
        color.red = -1 ;
        color.green = -1 ;
        color.blue = -1 ;
        color.alpha = -1 ;
    }
    else
    {
        color.red = static_cast<short>(mSpinRed->value()) ;
        color.green = static_cast<short>(mSpinGreen->value()) ;
        color.blue = static_cast<short>(mSpinBlue->value()) ;
        color.alpha = 255 ;
    }

    return color ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return the color currently described by the square and hue bar
///
/// @brief
/// Build the QColor for the current hue, saturation and value.
///
/////////////////////////////////////////////////////////////////////////////
QColor cColorDialog::CurrentColor(void) const
{
    return QColor::fromHsvF(ClampHue(mHueBar->Hue()), mSquare->Saturation(), mSquare->Value()) ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  const QColor &color [in] the color to display everywhere
///
/// @return nothing
///
/// @brief
/// Set every control (square, hue bar, spins, hex, preview) from one color.
///
/////////////////////////////////////////////////////////////////////////////
void cColorDialog::SetColor(const QColor &color)
{
    mUpdating = true ;

    qreal hue = color.hueF() ;
    if (hue < 0.0)
    {
        hue = mHueBar->Hue() ;
    }

    mHueBar->SetHue(hue) ;
    mSquare->SetHue(hue) ;
    mSquare->SetSatVal(color.saturationF(), color.valueF()) ;

    mSpinRed->setValue(color.red()) ;
    mSpinGreen->setValue(color.green()) ;
    mSpinBlue->setValue(color.blue()) ;
    mHexEdit->setText(color.name().toUpper()) ;

    mUpdating = false ;

    UpdatePreview() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Push the current square/hue color into the spins, hex field and preview
/// without disturbing the square or hue bar.
///
/////////////////////////////////////////////////////////////////////////////
void cColorDialog::SyncFromPicker(void)
{
    QColor color = CurrentColor() ;

    mUpdating = true ;
    mSpinRed->setValue(color.red()) ;
    mSpinGreen->setValue(color.green()) ;
    mSpinBlue->setValue(color.blue()) ;
    mHexEdit->setText(color.name().toUpper()) ;
    mUpdating = false ;

    UpdatePreview() ;
}


/////////////////////////////////////////////////////////////////////////////
///
/// @return nothing
///
/// @brief
/// Fill the preview swatch with the current color.
///
/////////////////////////////////////////////////////////////////////////////
void cColorDialog::UpdatePreview(void)
{
    QColor color = CurrentColor() ;
    QPalette pal = mPreviewSwatch->palette() ;
    pal.setColor(QPalette::Window, color) ;
    mPreviewSwatch->setPalette(pal) ;
}


void cColorDialog::OnSquareChanged(void)
{
    if (mUpdating == true)
    {
        return ;
    }

    mUseDefault = false ;
    SyncFromPicker() ;
}


void cColorDialog::OnHueChanged(void)
{
    if (mUpdating == true)
    {
        return ;
    }

    mUseDefault = false ;
    mSquare->SetHue(mHueBar->Hue()) ;
    SyncFromPicker() ;
}


void cColorDialog::OnRGBChanged(void)
{
    if (mUpdating == true)
    {
        return ;
    }

    mUseDefault = false ;
    SetColor(QColor(mSpinRed->value(), mSpinGreen->value(), mSpinBlue->value())) ;
}


void cColorDialog::OnHexChanged(void)
{
    if (mUpdating == true)
    {
        return ;
    }

    QColor color(mHexEdit->text()) ;
    if (color.isValid() == true)
    {
        mUseDefault = false ;
        SetColor(color) ;
    }
}


/////////////////////////////////////////////////////////////////////////////
///
/// @param  bool checked [in] whether "Use Default Color" is checked
///
/// @return nothing
///
/// @brief
/// Toggle the default sentinel and enable or disable the color controls.
///
/////////////////////////////////////////////////////////////////////////////
void cColorDialog::OnDefaultToggled(bool checked)
{
    mUseDefault = checked ;

    mSquare->setEnabled(checked == false) ;
    mHueBar->setEnabled(checked == false) ;
    mSpinRed->setEnabled(checked == false) ;
    mSpinGreen->setEnabled(checked == false) ;
    mSpinBlue->setEnabled(checked == false) ;
    mHexEdit->setEnabled(checked == false) ;
}
