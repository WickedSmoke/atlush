//============================================================================
//
// AWindow
//
//============================================================================


#include <QApplication>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QToolBar>
#include "AWindow.h"
#include "IOWidget.h"
#include "Atlush.h"

#define UTF8(str)   str.toUtf8().constData()

#define GIT_PIXMAP  QGraphicsPixmapItem::Type
#define GIT_RECT    QGraphicsRectItem::Type


enum ItemData {
    ID_NAME
};

struct ItemValues {
    QByteArray name;
    int x, y, w, h;
};

static void itemValues(ItemValues& iv, const QGraphicsItem* item)
{
    iv.name = item->data(ID_NAME).toString().toUtf8();

    QPointF pos = item->scenePos();
    iv.x = (int) pos.x();
    iv.y = (int) pos.y();

    QRectF rect = item->boundingRect();
    iv.w = int(rect.width());
    iv.h = int(rect.height());

    if (item->type() == GIT_PIXMAP) {
        // boundingRect grows by 1 pixel when ItemIsSelectable is used!
        iv.w -= 1;
        iv.h -= 1;
    }
}

typedef QList<QGraphicsItem*> ItemList;

#define each_item(pi) \
    for(const QGraphicsItem* pi : _scene->items(Qt::AscendingOrder))

#define each_child(pi,ci) \
    for(const QGraphicsItem* ci : pi->childItems())

//----------------------------------------------------------------------------

class AView : public QGraphicsView
{
public:
    AView(QGraphicsScene* scene) : QGraphicsView(scene)
    {
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    }

    void zoomIn() {
        scale(1.44, 1.44);
    }

    void zoomOut() {
        const qreal inv = 1.0 / 1.44;
        scale(inv, inv);
    }

protected:
    void mousePressEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent*);
    void wheelEvent(QWheelEvent*);

private:
    QPoint _panStart;
};

void AView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        _panStart = event->pos();
        //setCursor(Qt::ClosedHandCursor);
        event->accept();
    } else
        QGraphicsView::mousePressEvent(event);
}

void AView::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::MiddleButton) {
        QScrollBar* sb;

        sb = horizontalScrollBar();
        sb->setValue(sb->value() - (event->x() - _panStart.x()));
        sb = verticalScrollBar();
        sb->setValue(sb->value() - (event->y() - _panStart.y()));
        _panStart = event->pos();

        event->accept();
    } else
        QGraphicsView::mouseMoveEvent(event);
}

void AView::wheelEvent(QWheelEvent* event)
{
    // Zoom In/Out.
    qreal sfactor = qreal(event->angleDelta().y()) / 100.0;
    if (sfactor < 0.0)
        sfactor = -1.0 / sfactor;
    scale(sfactor, sfactor);
}

//----------------------------------------------------------------------------

AWindow::AWindow() : _ioDialog(NULL), _selItem(NULL), _modifiedStr(NULL)
{
    setWindowTitle(APP_NAME);

    createActions();
    createMenus();
    createTools();

    _scene = new QGraphicsScene;
    connect(_scene, SIGNAL(selectionChanged()), SLOT(syncSelection()));

    _view = new AView(_scene);
    _view->setMinimumSize(128, 128);
    _view->setBackgroundBrush(QBrush(Qt::darkGray));
    setCentralWidget(_view);

    QSettings settings;
    resize(settings.value("window-size", QSize(480, 480)).toSize());
    _prevProjPath  = settings.value("prev-project").toString();
    _prevImagePath = settings.value("prev-image").toString();
    _ioSpec = settings.value("io-pipelines").toString();
    _recent.setFiles(settings.value("recent-files").toStringList());

    _io->setSpec(_ioSpec);
}


void AWindow::closeEvent( QCloseEvent* ev )
{
    QSettings settings;
    settings.setValue("window-size", size());
    settings.setValue("prev-project", _prevProjPath);
    settings.setValue("prev-image", _prevImagePath);
    settings.setValue("io-pipelines", _ioSpec);
    settings.setValue("recent-files", _recent.files);

    QMainWindow::closeEvent( ev );
}

void AWindow::showAbout()
{
    QString str(
        "<h2>" APP_NAME " " APP_VERSION "</h2>\n"
        "<p>Image Atlas Shuffler, &copy; 2021 Karl Robillard</p>\n"
        "<p>Built on %1</p>\n"
   );

    QMessageBox* about = new QMessageBox(this);
    about->setWindowTitle( "About " APP_NAME );
    about->setIconPixmap( QPixmap(":/icons/logo.png") );
    about->setTextFormat( Qt::RichText );
    about->setText( str.arg(__DATE__) );
    about->show();
}


void AWindow::createActions()
{
#define STD_ICON(id)    style()->standardIcon(QStyle::id)

    _actNew = new QAction(STD_ICON(SP_FileIcon), "&New Project", this);
    _actNew->setShortcut(QKeySequence::New);
    connect( _actNew, SIGNAL(triggered()), SLOT(newProject()));

    _actOpen = new QAction(STD_ICON(SP_DialogOpenButton), "&Open...", this );
    _actOpen->setShortcut(QKeySequence::Open);
    connect( _actOpen, SIGNAL(triggered()), SLOT(open()));

    _actSave = new QAction(STD_ICON(SP_DialogSaveButton), "&Save", this );
    _actSave->setShortcut(QKeySequence::Save);
    connect( _actSave, SIGNAL(triggered()), SLOT(save()));

    _actSaveAs = new QAction("Save &As...", this);
    _actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect( _actSaveAs, SIGNAL(triggered()), SLOT(saveAs()));

    _actQuit = new QAction( "&Quit", this );
    _actQuit->setShortcut(QKeySequence::Quit);
    connect( _actQuit, SIGNAL(triggered()), SLOT(close()));

    _actAbout = new QAction( "&About", this );
    connect( _actAbout, SIGNAL(triggered()), SLOT(showAbout()));

    _actAddImage = new QAction(QIcon(":/icons/image-add.png"),
                               "Add Image", this);
    _actAddImage->setShortcut(QKeySequence(Qt::Key_Insert));
    connect(_actAddImage, SIGNAL(triggered()), SLOT(addImage()));

    _actAddRegion = new QAction(QIcon(":/icons/region-add.png"),
                                "Add Region", this);
    _actAddRegion->setEnabled(false);
    connect(_actAddRegion, SIGNAL(triggered()), SLOT(addRegion()));

    _actRemove = new QAction(QIcon(":/icons/image-remove.png"),
                                "Remove Item", this);
    _actRemove->setShortcut(QKeySequence(Qt::Key_Delete));
    _actRemove->setEnabled(false);
    connect(_actRemove, SIGNAL(triggered()), SLOT(removeSelected()));

    _actUndo = new QAction( "&Undo", this );
    _actUndo->setShortcut(QKeySequence::Undo);
    _actUndo->setEnabled(false);
    connect(_actUndo, SIGNAL(triggered()), SLOT(undo()));

    _actViewReset = new QAction( "&Reset View", this );
    _actViewReset->setShortcut(QKeySequence(Qt::Key_Home));
    connect(_actViewReset, SIGNAL(triggered()), SLOT(viewReset()));

    _actZoomIn = new QAction(QIcon(":/icons/zoom-in.png"),
                             "Zoom &In", this );
    _actZoomIn->setShortcut(QKeySequence(Qt::Key_Equal));
    connect(_actZoomIn, SIGNAL(triggered()), SLOT(viewZoomIn()));

    _actZoomOut = new QAction(QIcon(":/icons/zoom-out.png"),
                              "Zoom &Out", this );
    _actZoomOut->setShortcut(QKeySequence(Qt::Key_Minus));
    connect(_actZoomOut, SIGNAL(triggered()), SLOT(viewZoomOut()));
}

void AWindow::viewReset() { static_cast<AView*>(_view)->resetTransform(); }

void AWindow::viewZoomIn() { static_cast<AView*>(_view)->zoomIn(); }

void AWindow::viewZoomOut() { static_cast<AView*>(_view)->zoomOut(); }


void AWindow::createMenus()
{
    QMenuBar* bar = menuBar();

    QMenu* file = bar->addMenu( "&File" );
    file->addAction( _actNew );
    file->addAction( _actOpen );
    file->addAction( _actSave );
    file->addAction( _actSaveAs );
    file->addSeparator();
    _recent.install(file, this, SLOT(openRecent()));
    file->addSeparator();
    file->addAction( _actQuit );

    QMenu* edit = bar->addMenu( "&Edit" );
    edit->addAction( _actUndo );
    edit->addSeparator();
    edit->addAction( _actAddImage );
    edit->addAction( _actAddRegion );
    edit->addAction( _actRemove );
    edit->addSeparator();
    edit->addAction("&Pipelines", this, SLOT(editPipelines()));

    QMenu* view = bar->addMenu( "&View" );
    view->addAction( _actViewReset );
    view->addAction( _actZoomIn );
    view->addAction( _actZoomOut );

    bar->addSeparator();

    QMenu* help = bar->addMenu( "&Help" );
    help->addAction( _actAbout );
}

QSpinBox* makeSpinBox()
{
    QSpinBox* spin = new QSpinBox;
    spin->setRange(0, 8192);
    spin->setEnabled(false);
    return spin;
}

void AWindow::createTools()
{
    _tools = addToolBar("");
    _tools->addAction(_actOpen);
    _tools->addAction(_actSave);

    _tools->addSeparator();
    _tools->addAction(_actAddImage);
    _tools->addAction(_actAddRegion);
    _tools->addAction(_actRemove);

    _tools->addSeparator();
    _tools->addWidget(_name = new QLineEdit);
    _tools->addWidget(_spinX = makeSpinBox());
    _tools->addWidget(_spinY = makeSpinBox());
    _tools->addWidget(_spinH = makeSpinBox());
    _tools->addWidget(_spinW = makeSpinBox());

    _name->setEnabled(false);
    connect(_name, SIGNAL(editingFinished()), SLOT(modName()) );
    connect(_name, SIGNAL(textEdited(const QString&)), SLOT(stringEdit()) );
    connect(_spinX, SIGNAL(valueChanged(int)), SLOT(modX(int)));
    connect(_spinY, SIGNAL(valueChanged(int)), SLOT(modY(int)));
    connect(_spinW, SIGNAL(valueChanged(int)), SLOT(modW(int)));
    connect(_spinH, SIGNAL(valueChanged(int)), SLOT(modH(int)));

    _io = new IOWidget;
    connect(_io, SIGNAL(execute(int,int)), SLOT(execute(int,int)));

    _iobar = new QToolBar;
    _iobar->addWidget(_io);
    addToolBar(Qt::TopToolBarArea, _iobar);
}

void AWindow::updateProjectName(const QString& path)
{
    _prevProjPath = path;

    QString title(path);
    title.append(" - " APP_NAME);
    setWindowTitle(title);
}

bool AWindow::openFile(const QString& file)
{
    _scene->clear();

    if (loadProject(file)) {
        updateProjectName(file);
        return true;
    } else {
        QString error( "Error opening file " );
        QMessageBox::warning( this, "Load", error + file );
        return false;
    }
}


void AWindow::open()
{
    QString fn;

    fn = QFileDialog::getOpenFileName(this, "Open File", _prevProjPath);
    if (! fn.isEmpty()) {
        if (openFile(fn))
            _recent.addFile(&fn);
    }
}

void AWindow::openRecent()
{
    QString fn = _recent.fileOpened(sender());
    if (! fn.isEmpty())
        openFile(fn);
}

static void saveFailed(QWidget* parent, const QString& fn) {
    QString error( "Error saving project " );
    QMessageBox::warning(parent, "Save Failed", error + fn);
}


void AWindow::save()
{
    if (_prevProjPath.isEmpty()) {
        saveAs();
    } else {
        if (saveProject(_prevProjPath))
            _recent.addFile(&_prevProjPath);
        else
            saveFailed(this, _prevProjPath);
    }
}


void AWindow::saveAs()
{
    QString fn;

    fn = QFileDialog::getSaveFileName(this, "Save Project As", _prevProjPath);
    if (! fn.isEmpty()) {
        if (saveProject(fn)) {
            updateProjectName(fn);
            _recent.addFile(&fn);
        } else
            saveFailed(this, fn);
    }
}


void AWindow::addImage()
{
    QString fn;

    fn = QFileDialog::getOpenFileName(this, "Add Image", _prevImagePath);
    if( ! fn.isEmpty() ) {
        _prevImagePath = fn;

        QPixmap pix(fn);
        if (! pix.isNull()) {
            QGraphicsItem* item = makeImage(pix, 0, 0);
            item->setData(ID_NAME, fn);
        }
    }
}

void AWindow::addRegion()
{
    ItemList sel = _scene->selectedItems();
    if (! sel.empty()) {
        QGraphicsItem* item = sel[0];
        if (item->type() != GIT_PIXMAP)
            item = item->parentItem();
        if (item) {
            QGraphicsItem* child = makeRegion(item, 0, 0, 32, 32);
            child->setData(ID_NAME, QString("<unnamed>"));
        }
    }
}

void AWindow::removeSelected()
{
    ItemList sel = _scene->selectedItems();
    if (! sel.empty()) {
        _scene->removeItem(sel[0]);
        delete sel[0];
    }
}

void AWindow::undo()
{
    if (_selItem) {
        _selItem->setPos(_selPos);
    }
}

// Set QSpinBox value without emitting the valueChanged() signal.
static void assignSpin(QSpinBox* spin, int val)
{
    spin->blockSignals(true);
    spin->setValue(val);
    spin->blockSignals(false);
}

void AWindow::syncSelection()
{
    ItemList sel = _scene->selectedItems();
    bool isSelected = ! sel.empty();
    bool isRegion = isSelected && (sel[0]->type() == GIT_RECT);

    _actAddRegion->setEnabled(isSelected);
    _actRemove->setEnabled(isSelected);
    _actUndo->setEnabled(isSelected);

    _name->setEnabled(isRegion);
    _spinX->setEnabled(isSelected);
    _spinY->setEnabled(isSelected);
    _spinW->setEnabled(isRegion);
    _spinH->setEnabled(isRegion);

    if (isSelected) {
        ItemValues val;
        itemValues(val, sel[0]);

        _name->setText(val.name);
        assignSpin(_spinX, val.x);
        assignSpin(_spinY, val.y);
        assignSpin(_spinW, val.w);
        assignSpin(_spinH, val.h);

        _selItem = sel[0];
        _selPos = _selItem->pos();
    } else {
        _selItem = NULL;
    }
}

void AWindow::modName()
{
    // Ignore editingFinished() signal unless string was actually changed.
    if (_modifiedStr == _name) {
        _modifiedStr = NULL;

        if (_selItem)
            _selItem->setData(ID_NAME, _name->text());
    }
}

// Record that a QLineEdit was edited.
void AWindow::stringEdit()
{
    _modifiedStr = sender();
}

void AWindow::modX(int n)
{
    if (_selItem) {
        qreal r = qreal(n);
        QGraphicsItem* pi = _selItem->parentItem();
        if (pi)
            r -= pi->scenePos().x();
        _selItem->setX(r);
    }
}

void AWindow::modY(int n)
{
    if (_selItem) {
        qreal r = qreal(n);
        QGraphicsItem* pi = _selItem->parentItem();
        if (pi)
            r -= pi->scenePos().y();
        _selItem->setY(r);
    }
}

// Set either width or height.
static void setRectDim(QGraphicsItem* gi, int w, int h)
{
    if (gi && gi->type() == GIT_RECT) {
        QGraphicsRectItem* item = static_cast<QGraphicsRectItem*>(gi);
        QRectF rect(item->rect());
        if (w < 0)
            rect.setHeight(h);
        else
            rect.setWidth(w);
        item->setRect(rect);
    }
}

void AWindow::modW(int n)
{
    setRectDim(_selItem, n, -1);
}

void AWindow::modH(int n)
{
    setRectDim(_selItem, -1, n);
}

void AWindow::editPipelines()
{
    if (! _ioDialog) {
        _ioDialog = new IODialog(this);
        _ioDialog->setModal(true);
        connect(_ioDialog, SIGNAL(accepted()), SLOT(pipelinesChanged()));
    }
    _ioDialog->edit(&_ioSpec);
    _ioDialog->show();
}

void AWindow::pipelinesChanged()
{
    _io->setSpec(_ioSpec);
}

void AWindow::execute(int pi, int push)
{
    char fileVar[40];
    sprintf(fileVar, "/tmp/atlush-%lld.atl", qApp->applicationPid());
    setenv("ATL", fileVar, 1);

    QString fn(fileVar);
    if (push) {
        if (! saveProject(fn)) {
            saveFailed(this, fn);
            return;
        }
    }

    QString cmd;
    int stat = io_execute(_ioSpec, pi, push, cmd);
    if (stat < 0) {
        QString error("Could not run: ");
        QMessageBox::warning(this, "System Failure", error + cmd);
    } else if (stat) {
        QString error("Command: ");
        error += cmd;
        error += "\n\nExit Status: ";
        error += QString::number(stat);
        QMessageBox::warning(this, "I/O Command Failure", error);
    } else if(! push) {
        openFile(fn);
    }
}

//----------------------------------------------------------------------------
// Project methods

void AWindow::newProject()
{
    _scene->clear();
}

QGraphicsPixmapItem* AWindow::makeImage(const QPixmap& pix, int x, int y)
{
    QGraphicsPixmapItem* item = _scene->addPixmap(pix);
    item->setFlags(QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemIsSelectable);
    item->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    item->setPos(x, y);
    return item;
}

QGraphicsRectItem* AWindow::makeRegion(QGraphicsItem* parent, int x, int y,
                                       int w, int h)
{
    QRectF rect(0.0, 0.0, w, h);
    QGraphicsRectItem* item = new QGraphicsRectItem(rect, parent);

    item->setPen(QPen(Qt::NoPen));
    item->setBrush(QColor(255, 20, 20));
    item->setOpacity(0.5);
    item->setFlags(QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemIsSelectable);
    item->setPos(x, y);

    //QPointF p = item->scenePos();
    //printf("KR region %f,%f\n", p.x(), p.y());

    return item;
}

/*
 * Append items in project file to scene.
 */
bool AWindow::loadProject(const QString& path)
{
    FILE* fp = fopen(UTF8(path), "r");
    if (! fp)
        return false;

    bool done = false;
    {
    QGraphicsItem* pitem = NULL;
    char* buf = new char[1000];
    int x, y, w, h;
    int nested = 0;

    while (fread(buf, 1, 1, fp) == 1) {
      switch (buf[0]) {
        case '"':
            if (fscanf(fp, "%999[^\"]\" %d,%d,%d,%d", buf, &x, &y, &w, &h) != 5)
                goto fail;
            //printf("KR %s %d,%d,%d,%d\n", buf, x, y, w, h);
        {
            QString name(buf);
            if (nested) {
                if (pitem) {
                    QPointF pp = pitem->scenePos();
                    QGraphicsItem* region =
                    makeRegion(pitem, x - int(pp.x()), y - int(pp.y()), w, h);
                    region->setData(ID_NAME, name);
                }
            } else {
                QPixmap pix(name);
                if (pix.isNull())
                    pix = QPixmap(":/icons/missing.png");

                pitem = makeImage(pix, x, y);
                pitem->setData(ID_NAME, name);
            }
        }
            break;

        case '[':
            ++nested;
            break;

        case ']':
            --nested;
            break;

        case ';':
            while ((x = fgetc(fp)) != EOF) {
                if (x == '\n')
                    break;
            }
            break;

        case ' ':
        case '\t':
        case '\n':
            break;

        default:
            goto fail;
      }
    }
    done = true;

fail:
    delete[] buf;
    }

    fclose(fp);
    return done;
}

static int regionWriteBoron(FILE* fp, const ItemValues& iv) {
    return fprintf(fp, "\"%s\" %d,%d,%d,%d\n",
                   iv.name.constData(), iv.x, iv.y, iv.w, iv.h);
}

/*
 * Replace project file with items in scene.
 */
bool AWindow::saveProject(const QString& path)
{
    ItemValues val;
    FILE* fp = fopen(UTF8(path), "w");
    if (! fp)
        return false;

    bool done = false;
    each_item(it) {
        if (it->type() != GIT_PIXMAP)
            continue;
        itemValues(val, it);
        if (regionWriteBoron(fp, val) < 0)
            goto fail;

        ItemList clist = it->childItems();
        if (! clist.empty()) {
            fprintf(fp, "[\n");
            for(const QGraphicsItem* ch : clist) {
                if (ch->type() != GIT_RECT)
                    continue;
                fprintf(fp, "  ");
                itemValues(val, ch);
                if (regionWriteBoron(fp, val) < 0)
                    goto fail;
            }
            fprintf(fp, "]\n");
        }
    }
    done = true;

fail:
    fclose(fp);
    return done;
}

//----------------------------------------------------------------------------

int main( int argc, char **argv )
{
    QApplication app( argc, argv );
    app.setOrganizationName( APP_NAME );
    app.setApplicationName( APP_NAME );

    AWindow w;
    w.show();

    if (argc > 1)
        w.openFile(argv[1]);

    return app.exec();
}


//EOF
