//============================================================================
//
// AWindow
//
//============================================================================


#include <math.h>
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
#include "CanvasDialog.h"
#include "IOWidget.h"
#include "Atlush.h"
#include "ItemValues.h"


void itemValues(ItemValues& iv, const QGraphicsItem* item)
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

AWindow::AWindow()
    : _modifiedStr(NULL), _canvasDialog(NULL), _ioDialog(NULL), _selItem(NULL)
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

    _bgPix = QPixmap(":/icons/transparent.png");

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

    {
    QIcon lockIcon;
    lockIcon.addPixmap(QPixmap(":/icons/lock.png"), QIcon::Normal, QIcon::On);
    lockIcon.addPixmap(QPixmap(":/icons/lock-open.png"), QIcon::Normal, QIcon::Off);
    _actLockRegions = new QAction(lockIcon, "Lock Regions", this );
    _actLockRegions->setCheckable(true);
    connect(_actLockRegions, SIGNAL(toggled(bool)), SLOT(lockRegions(bool)));
    }
}

void AWindow::viewReset() { static_cast<AView*>(_view)->resetTransform(); }

void AWindow::viewZoomIn() { static_cast<AView*>(_view)->zoomIn(); }

void AWindow::viewZoomOut() { static_cast<AView*>(_view)->zoomOut(); }

void AWindow::lockRegions(bool lock)
{
    QGraphicsItem::GraphicsItemFlags flags;
    if (! lock)
        flags = QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable;

    each_item_mod(it) {
        if (IS_REGION(it))
            it->setFlags(flags);
    }
}

void AWindow::createMenus()
{
    QMenuBar* bar = menuBar();

    QMenu* file = bar->addMenu( "&File" );
    file->addAction( _actNew );
    file->addAction( _actOpen );
    file->addAction( _actSave );
    file->addAction( _actSaveAs );
    file->addAction("Import &Directory...", this, SLOT(importDir()));
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
    edit->addAction("&Pack Images", this, SLOT(packImages()));
    edit->addSeparator();
    edit->addAction("Canvas &Size...", this, SLOT(editDocSize()));

    QMenu* view = bar->addMenu( "&View" );
    view->addAction( _actViewReset );
    view->addAction( _actZoomIn );
    view->addAction( _actZoomOut );
    view->addAction( _actLockRegions );

    QMenu* sett = bar->addMenu( "&Settings" );
    sett->addAction("Configure &Pipelines...", this, SLOT(editPipelines()));

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
    _tools->addAction(_actLockRegions);

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

#define BG_Z    -1.0

static void setupBackground(QGraphicsScene* scene, const QSize& size,
                            const QBrush& brush)
{
    QRectF bound(0, 0, size.width(), size.height());
    auto rect = scene->addRect(bound, QPen(Qt::darkRed), brush);

    // Negative Z distinguishes this from region rectangle items.
    rect->setZValue(BG_Z);
}

bool AWindow::openFile(const QString& file)
{
    _scene->clear();

    int line;
    if (loadProject(file, &line)) {
        updateProjectName(file);
        if (! _docSize.isEmpty())
            setupBackground(_scene, _docSize, QBrush(_bgPix));
        return true;
    } else {
        QString error;
        if (line < 0)
            error = "Error opening file ";
        else {
            error = "Parse error on line ";
            error += QString::number(line);
            error += " of ";
        }
        QMessageBox::warning( this, "Load Error", error + file );
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

QGraphicsPixmapItem* AWindow::importImage(const QString& file)
{
    QPixmap pix(file);
    if (pix.isNull())
        return NULL;

    QGraphicsPixmapItem* item = makeImage(pix, 0, 0);
    item->setData(ID_NAME, file);
    return item;
}

bool AWindow::directoryImport(const QString& path)
{
    QDir dir(path);
    dir.setNameFilters(QStringList() << "*.png" << "*.jpg" << "*.jpeg");
    const QStringList list = dir.entryList();
    bool ok = true;

    for (const QString& s: list) {
        if (! importImage(dir.filePath(s)))
            ok = false;
    }
    return ok;
}

void AWindow::importDir()
{
    QString fn = QFileDialog::getExistingDirectory(this,
                            "Import Images from Directory", _prevImagePath);
    if (! fn.isEmpty()) {
        _prevImagePath = fn;
        if (! directoryImport(fn))
            QMessageBox::warning(this, "Import Failure",
                                 "Some images could not be loaded!");
    }
}

void AWindow::addImage()
{
    QString fn;

    fn = QFileDialog::getOpenFileName(this, "Add Image", _prevImagePath);
    if( ! fn.isEmpty() ) {
        _prevImagePath = fn;
        importImage(fn);
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

void AWindow::editDocSize()
{
    if (! _canvasDialog) {
        _canvasDialog = new CanvasDialog(this);
        _canvasDialog->setModal(true);
        connect(_canvasDialog, SIGNAL(accepted()), SLOT(canvasChanged()));
    }
    _canvasDialog->edit(&_docSize);
    _canvasDialog->show();
}

void AWindow::canvasChanged()
{
    each_item(it) {
        if (it->type() == GIT_RECT && it->zValue() == BG_Z) {
            QGraphicsRectItem* rit = (QGraphicsRectItem*) it;
            QRectF rect(rit->rect());
            rect.setWidth(_docSize.width());
            rect.setHeight(_docSize.height());
            rit->setRect(rect);
            return;
        }
    }
    setupBackground(_scene, _docSize, QBrush(_bgPix));
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
// Scene Classes

static inline QPointF& pixelSnap(QPointF& pnt)
{
    pnt.setX( round(pnt.x()) );
    pnt.setY( round(pnt.y()) );
    return pnt;
}

class AImage : public QGraphicsPixmapItem
{
protected:
     QVariant itemChange(GraphicsItemChange change, const QVariant& value)
     {
         if (change == ItemPositionChange) {
             QPointF newPos = value.toPointF();
             return pixelSnap(newPos);
         }
         return QGraphicsPixmapItem::itemChange(change, value);
     }
};

class ARegion : public QGraphicsRectItem
{
public:
    ARegion(QGraphicsItem* parent) : QGraphicsRectItem(parent)
    {
    }

protected:
     QVariant itemChange(GraphicsItemChange change, const QVariant& value)
     {
         if (change == ItemPositionChange) {
             QPointF newPos = value.toPointF();
             return pixelSnap(newPos);
         }
         return QGraphicsRectItem::itemChange(change, value);
     }
};

//----------------------------------------------------------------------------
// Project methods

void AWindow::newProject()
{
    _scene->clear();
}

QGraphicsPixmapItem* AWindow::makeImage(const QPixmap& pix, int x, int y)
{
    QGraphicsPixmapItem* item = new AImage;
    item->setPixmap(pix);
    item->setFlags(QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemIsSelectable |
                   QGraphicsItem::ItemSendsGeometryChanges);
    item->setShapeMode(QGraphicsPixmapItem::BoundingRectShape);
    item->setPos(x, y);

    _scene->addItem(item);
    return item;
}

QGraphicsRectItem* AWindow::makeRegion(QGraphicsItem* parent, int x, int y,
                                       int w, int h)
{
    QRectF rect(0.0, 0.0, w, h);
    QGraphicsRectItem* item = new ARegion(parent);

    item->setRect(rect);
    item->setPen(QPen(Qt::NoPen));
    item->setBrush(QColor(255, 20, 20));
    item->setOpacity(0.5);
    item->setFlags(QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemIsSelectable |
                   QGraphicsItem::ItemSendsGeometryChanges);
    item->setPos(x, y);

    //QPointF p = item->scenePos();
    //printf("KR region %f,%f\n", p.x(), p.y());

    return item;
}

/*
 * Append items in project file to scene.
 *
 * Return false on error.  In this case errorLine is set to either the line
 * number where parsing failed or -1 on a file open or read error.
 */
bool AWindow::loadProject(const QString& path, int* errorLine)
{
    FILE* fp = fopen(UTF8(path), "r");
    if (! fp) {
        *errorLine = -1;
        return false;
    }

    bool done = false;
    {
    QGraphicsItem* pitem = NULL;
    char* buf = new char[1000];
    int x, y, w, h;
    int nested = 0;
    int lineCount = 0;

    // Parse Boron string!/coord!/block! values.
    while (fread(buf, 1, 1, fp) == 1) {
      switch (buf[0]) {
        case '"':
            if (fscanf(fp, "%999[^\"]\" %d,%d,%d,%d", buf, &x, &y, &w, &h) != 5)
                goto fail;
            //printf("KR %s %d,%d,%d,%d\n", buf, x, y, w, h);
add_item:
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

        case 'i':
            if (fscanf(fp, "mage-atlas %d %d,%d", &x, &w, &h) != 3)
                goto fail;
            if (x != 1)
                goto fail;          // Invalid version.
            _docSize.setWidth(w);
            _docSize.setHeight(h);
            break;

        case '{':
            if (fscanf(fp, "%999[^}]} %d,%d,%d,%d", buf, &x, &y, &w, &h) != 5)
                goto fail;
            goto add_item;

        case ';':
            while ((x = fgetc(fp)) != EOF) {
                if (x == '\n')
                    break;
            }
            break;

        case ' ':
        case '\t':
            break;

        case '\n':
            ++lineCount;
            break;

        default:
            goto fail;
      }
    }
    done = true;

fail:
    *errorLine = lineCount;
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

    if (! _docSize.isEmpty()) {
        fprintf(fp, "image-atlas 1 %d,%d\n",
                _docSize.width(), _docSize.height());
    }

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
                if (IS_REGION(ch)) {
                    fprintf(fp, "  ");
                    itemValues(val, ch);
                    if (regionWriteBoron(fp, val) < 0)
                        goto fail;
                }
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

    if (argc > 1) {
        QFileInfo info(argv[1]);
        if (info.isDir())
            w.directoryImport(info.filePath());
        else
            w.openFile(info.filePath());
    }

    return app.exec();
}


//EOF
