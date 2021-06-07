#ifndef CANVASDIALOG_H
#define CANVASDIALOG_H

#include <QDialog>

class QComboBox;
class QSpinBox;

class CanvasDialog : public QDialog
{
    Q_OBJECT

public:
    CanvasDialog(QWidget* parent = nullptr);
    void edit(QSize* sizeDest);

private slots:
    void updateData();
    void presetMod(int);

private:
    QSpinBox*  _spinW;
    QSpinBox*  _spinH;
    QComboBox* _preset;
    QSize*     _sizeDest;
};

#endif  // CANVASDIALOG_H
