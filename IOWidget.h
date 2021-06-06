#ifndef IOWIDGET_H
#define IOWIDGET_H
//============================================================================
//
// I/O Process Widget
//
//============================================================================


#include <QDialog>

class QComboBox;
class QPushButton;

class IOWidget : public QWidget
{
    Q_OBJECT

public:
    IOWidget();
    void setSpec(const QString& spec);

signals:
    void execute(int, int);

private slots:
    void emitExec();

private:
    QComboBox* _pipeline;
    QPushButton* _in;
    QPushButton* _out;
};


class QLineEdit;

class IODialog : public QDialog
{
    Q_OBJECT

public:
    IODialog(QWidget* parent = nullptr);
    void edit(QString* spec);
    void setCurrent(const QString& name);

private slots:
    void pipelineChanged(int);
    void addPipeline();
    void removePipeline();
    void updateSpec();
    void cmdChanged();

private:
    QComboBox* _pipeline;
    QLineEdit* _cmdIn;
    QLineEdit* _cmdOut;
    QStringList _spec;
    QString* _specDest;
};


int io_execute(const QString& spec, int pipeline, int push, QString&);


#endif  // IOWIDGET_H
