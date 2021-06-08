#ifndef EXTRACTDIALOG_H
#define EXTRACTDIALOG_H

#include <QDialog>

class QLineEdit;
class ColorButton;

class ExtractDialog : public QDialog
{
    Q_OBJECT

public:
    ExtractDialog(QWidget* parent = nullptr);
    QString file() const;
    QColor color() const;

private slots:
    void saveSettings();
    void chooseFile();
    void chooseColor();

private:
    QLineEdit* _file;
    ColorButton* _color;
};

#endif  // EXTRACTDIALOG_H
