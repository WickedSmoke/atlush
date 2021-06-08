#include <QBoxLayout>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QStyle>
#include <QToolButton>
#include "ExtractDialog.h"

#define STD_ICON(id)    style()->standardIcon(QStyle::id)


class ColorButton : public QToolButton
{
public:
    ColorButton( const QColor& color ) : _pix(24, 24)
    {
        setColor( color );
    }

    const QColor& color() const { return _col; }

    void setColor( const QColor& color )
    {
        _pix.fill( color );
        QIcon icon( _pix );
        setIcon(icon);
        _col = color;
    }

private:
    QPixmap _pix;
    QColor  _col;
};


ExtractDialog::ExtractDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("Extract Regions to Image");

    _file = new QLineEdit;
    _file->setMinimumWidth(200);

    QPushButton* btn = new QPushButton(STD_ICON(SP_DialogSaveButton),QString());
    btn->setMaximumWidth(btn->height() + 8);
    connect(btn, SIGNAL(clicked(bool)), SLOT(chooseFile()));

    _color = new ColorButton(QColor(0,0,0,0));
    connect(_color, SIGNAL(clicked(bool)), SLOT(chooseColor()));

    QDialogButtonBox* bbox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QBoxLayout* hl = new QHBoxLayout;
    hl->addWidget(_file);
    hl->addWidget(btn);

    QFormLayout* form = new QFormLayout;
    form->addRow(tr("Image File:"), hl);
    form->addRow(tr("Background:"), _color);

    QBoxLayout* lo = new QVBoxLayout(this);
    lo->addLayout(form);
    lo->addStretch();
    lo->addWidget(bbox);

    QSettings settings;
    _file->setText( settings.value("extract-file").toString() );
    _color->setColor( settings.value("extract-bg").value<QColor>() );
    connect(this, SIGNAL(accepted()), SLOT(saveSettings()));
}

void ExtractDialog::saveSettings()
{
    QSettings settings;
    settings.setValue("extract-file", _file->text());
    settings.setValue("extract-bg", _color->color());
}

QString ExtractDialog::file() const
{
    return _file->text();
}

void ExtractDialog::chooseFile()
{
    QString fn = QFileDialog::getSaveFileName(this, "Image File", _file->text());
    if (! fn.isEmpty())
        _file->setText(fn);
}

QColor ExtractDialog::color() const
{
    return _color->color();
}

void ExtractDialog::chooseColor()
{
    QColor pick = QColorDialog::getColor(_color->color(), this,
                                    "Background Color",
                                    QColorDialog::ShowAlphaChannel);
    if (pick.isValid())
        _color->setColor(pick);
}
