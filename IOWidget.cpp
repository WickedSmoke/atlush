#include <QBoxLayout>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QToolButton>
#include "IOWidget.h"

enum SpecFields {
    SPEC_NAME,
    SPEC_IMPORT,
    SPEC_EXPORT,
    SPEC_SIZE
};


IOWidget::IOWidget()
{
    _pipeline = new QComboBox;

    _in = new QPushButton(QString::fromUtf8("\xc2\xbb Import"));
    connect(_in, SIGNAL(clicked(bool)), SLOT(emitExec()));

    _out = new QPushButton(QString::fromUtf8("Export \xc2\xbb"));
    connect(_out, SIGNAL(clicked(bool)), SLOT(emitExec()));

    QBoxLayout* lo = new QHBoxLayout(this);
    lo->addWidget(new QLabel("I/O:"));
    lo->addWidget(_pipeline);
    lo->addWidget(_in);
    lo->addWidget(_out);
}

void IOWidget::emitExec()
{
    int n = _pipeline->currentIndex();
    if (n >= 0)
        emit execute(n, (sender() == _in) ? 0 : 1);
}

void IOWidget::setSpec(const QString& spec)
{
    _pipeline->clear();

    QStringList list = spec.split('\n');
    int count = list.size() / SPEC_SIZE;
    for (int i = 0; i < count; ++i)
        _pipeline->addItem(list[i * SPEC_SIZE]);

    setEnabled(_pipeline->count() > 0);
}

//----------------------------------------------------------------------------

IODialog::IODialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle("I/O Pipelines");

    QToolButton* add = new QToolButton;
    add->setToolTip(tr("Add Pipeline"));
    add->setIcon(QIcon(":/icons/item-add.png"));
    //add->setIconSize(QSize(20, 20));
    connect( add, SIGNAL(clicked(bool)), SLOT(addPipeline()) );

    QToolButton* rem = new QToolButton;
    rem->setToolTip(tr("Remove Pipeline"));
    rem->setIcon( QIcon(":/icons/item-remove.png") );
    //rem->setIconSize(QSize(20, 20));
    connect( rem, SIGNAL(clicked(bool)), SLOT(removePipeline()) );

    _pipeline = new QComboBox;
    connect(_pipeline, SIGNAL(currentIndexChanged(int)),
            SLOT(pipelineChanged(int)));

    _name = new QLineEdit;
    connect(_name, SIGNAL(editingFinished()), SLOT(cmdChanged()));

    _cmdIn = new QLineEdit;
    connect(_cmdIn, SIGNAL(editingFinished()), SLOT(cmdChanged()));

    _cmdOut = new QLineEdit;
    _cmdOut->setMinimumWidth(320);
    connect(_cmdOut, SIGNAL(editingFinished()), SLOT(cmdChanged()));

    QDialogButtonBox* bbox =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(bbox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(this, SIGNAL(accepted()), SLOT(updateSpec()));


     // Layouts

     QBoxLayout* hl = new QHBoxLayout;
     hl->addWidget(add);
     hl->addWidget(rem);
     hl->addWidget(_pipeline);

     QFormLayout* form = new QFormLayout;
      form->addRow(tr("Name:"), _name);
      form->addRow(tr("Import Command:"), _cmdIn);
      form->addRow(tr("Export Command:"), _cmdOut);

     QBoxLayout* lo = new QVBoxLayout(this);
     lo->addLayout(hl);
     lo->addLayout(form);
     lo->addWidget(bbox);
}

/*
 * Attach the dialog to the pipeline specification data.  The string contains
 * three lines for each pipeline: Name, Import Command, & Export Command.
 *
 * When the dialog is accepted, the spec string will be updated.
 */
void IODialog::edit(QString* spec)
{
    _specDest = spec;
    _pipeline->clear();

    if (spec) {
        _spec = spec->split('\n');
        int count = _spec.size() / SPEC_SIZE;
        if (! count)
            _spec.clear();  // Failed split adds one element.

        for (int i = 0; i < count; ++i)
            _pipeline->addItem(_spec[i * SPEC_SIZE]);
    } else
        _spec.clear();
}

void IODialog::setCurrent(const QString& name)
{
    int found = _pipeline->findText(name);
    if (found >= 0)
        _pipeline->setCurrentIndex(found);
}

void IODialog::pipelineChanged(int n)
{
    if (n >= 0) {
        n *= SPEC_SIZE;
        _name  ->setText(_spec[n + SPEC_NAME]);
        _cmdIn ->setText(_spec[n + SPEC_IMPORT]);
        _cmdOut->setText(_spec[n + SPEC_EXPORT]);
    }
}

void IODialog::addPipeline()
{
    QString name("<unnamed>");

    _spec.append(name);
    _spec.append("to-atlush input $ATL");
    _spec.append("from-atlush $ATL output");

    _pipeline->addItem(name);
    _pipeline->setCurrentIndex(_pipeline->count() - 1);

    _name->selectAll();
    _name->setFocus(Qt::OtherFocusReason);
}

void IODialog::removePipeline()
{
    int n = _pipeline->currentIndex();
    if (n >= 0) {
         QStringList::iterator it = _spec.begin() + (n * SPEC_SIZE);
        _spec.erase(it, it + int(SPEC_SIZE));

        _pipeline->removeItem(n);
        if (_pipeline->count() == 0) {
            _cmdIn->clear();
            _cmdOut->clear();
        }
    }
}

void IODialog::updateSpec()
{
    if (_specDest)
       *_specDest = _spec.join('\n');
}

void IODialog::cmdChanged()
{
    int n = _pipeline->currentIndex();
    if (n >= 0) {
        int si = n * SPEC_SIZE;
        QObject* obj = sender();
        if (obj == _name) {
            QString name(_name->text());
            _spec[si + SPEC_NAME] = name;
            _pipeline->setItemText(n, name);
        } else if (obj == _cmdIn)
            _spec[si + SPEC_IMPORT] = _cmdIn->text();
        else
            _spec[si + SPEC_EXPORT] = _cmdOut->text();
    }
}

//----------------------------------------------------------------------------

#include <stdlib.h>
#include <QRegularExpression>

static QString expandEnv(const QString& cmd)
{
    QString rep(cmd);
    QRegularExpression env_var("\\$([A-Za-z0-9_]+)");

    for (;;) {
        QRegularExpressionMatch em = env_var.match(rep);
        if (! em.hasMatch())
            break;
        QByteArray value(qgetenv(em.captured(1).toLatin1().data()));
        if (value.size() < 1)
            break;
        int i = em.capturedStart();
        rep.remove(i, em.capturedLength());
        rep.insert(i, value);
    }
    return rep;
}

/*
 * \param spec      Pipeline specifications (name, import, & export).
 * \param pipeline  Pipeline index in spec.
 * \param push      Non-zero to run export comamnd, othersize do import.
 */
int io_execute(const QString& spec, int pipeline, int push, QString& cmd)
{
    QStringList list = spec.split('\n');
    int count = list.size() / SPEC_SIZE;
    if (pipeline < 0 || pipeline >= count) {
        cmd = "<invalid pipeline>";
        return -1;
    }

    push = push ? SPEC_EXPORT : SPEC_IMPORT;
    QString raw(list[pipeline * SPEC_SIZE + push]);
    if (raw.isEmpty()) {
        cmd = "<empty command>";
        return -1;
    }

    cmd = expandEnv(raw);
    return system(cmd.toUtf8().constData());
}
