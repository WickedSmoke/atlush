#include <QBoxLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include "CanvasDialog.h"


CanvasDialog::CanvasDialog(QWidget* parent)
    : QDialog(parent), _sizeDest(nullptr)
{
    setWindowTitle("Canvas Size");

    _spinW = new QSpinBox;
    _spinW->setRange(0, 8192);

    _spinH = new QSpinBox;
    _spinH->setRange(0, 8192);

    _preset = new QComboBox;
    _preset->addItem(tr("None"));
    _preset->addItem(" 256 x 256");
    _preset->addItem(" 256 x 512");
    _preset->addItem(" 512 x 512");
    _preset->addItem(" 512 x 1024");
    _preset->addItem("1024 x 1024");
    _preset->addItem("1024 x 2048");
    _preset->addItem("2048 x 2048");
    connect(_preset, SIGNAL(currentIndexChanged(int)), SLOT(presetMod(int)));

    //_power2 = new QCheckBox(tr("Power of two"));

    QDialogButtonBox* bbox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(this, SIGNAL(accepted()), SLOT(updateData()));

    QFormLayout* form = new QFormLayout;
    form->addRow(tr("Width:"), _spinW);
    form->addRow(tr("Height:"), _spinH);
    //form->addRow(nullptr, _power2);

    QBoxLayout* hlo = new QHBoxLayout;
    hlo->addLayout(form);
    hlo->addWidget(_preset);

    QBoxLayout* lo = new QVBoxLayout(this);
    lo->addLayout(hlo);
    lo->addStretch();
    lo->addWidget(bbox);
}


void CanvasDialog::edit(QSize* sizeDest)
{
    _sizeDest = sizeDest;
    if (sizeDest) {
        _spinW->setValue(sizeDest->width());
        _spinH->setValue(sizeDest->height());
    }
}


void CanvasDialog::updateData()
{
    if (_sizeDest) {
        _sizeDest->setWidth(_spinW->value());
        _sizeDest->setHeight(_spinH->value());
    }
}

void CanvasDialog::presetMod(int n)
{
    static const uint16_t presetW[] = {
        0, 256, 256, 512, 512, 1024, 1024, 2048
    };
    static const uint16_t presetH[] = {
        0, 256, 512, 512,1024, 1024, 2048, 2048
    };

    if (n >= 0) {
        _spinW->setValue(presetW[n]);
        _spinH->setValue(presetH[n]);
    }
}
