//============================================================================
//
// AWindow
//
//============================================================================


#include <math.h>
#include <QApplication>
#include <QCheckBox>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include <QKeyEvent>
#include <QLabel>
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


#define ITEM_PIXMAP(gi) static_cast<const QGraphicsPixmapItem*>(gi)->pixmap()

#define EXT_COUNT   4
static const char* imageExt[EXT_COUNT] = { ".png", ".jpeg", ".jpg", ".ppm" };

QStringList imageFilters()
{
    QStringList list;
    char wild[8];

    wild[0] = '*';
    for (int i = 0; i < EXT_COUNT; ++i) {
        strcpy(wild + 1, imageExt[i]);
        list << QString(wild);
    }
    return list;
}

bool hasImageExt(const QString& path)
{
    for (int i = 0; i < EXT_COUNT; ++i) {
        if (path.endsWith(QLatin1String(imageExt[i]), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

void itemValues(ItemValues& iv, const QGraphicsItem* item)
{
    iv.name = item->data(ID_NAME).toString().toUtf8();

    QPointF pos = item->scenePos();
    iv.x = (int) pos.x();
    iv.y = (int) pos.y();

    QRectF rect = item->boundingRect();
    iv.w = int(rect.width());
    iv.h = int(rect.height());

    if (item->type() == GIT_PIXMAP &&
        item->flags() & QGraphicsItem::ItemIsSelectable) {
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
    connect(_scene, SIGNAL(changed(const QList<QRectF>&)), SLOT(sceneChange()));

    _view = new AView(_scene);
    _view->setMinimumSize(128, 128);
    _view->setBackgroundBrush(QBrush(Qt::darkGray));
    _view->setDragMode(QGraphicsView::RubberBandDrag);
    setCentralWidget(_view);

    _bgPix = QPixmap(":/icons/transparent.png");

    QSettings settings;
    resize(settings.value("window-size", QSize(480, 480)).toSize());
    restoreState(settings.value("window-state").toByteArray());
    _prevProjPath  = settings.value("prev-project").toString();
    _prevImagePath = settings.value("prev-image").toString();
    _ioSpec = settings.value("io-pipelines").toString();
    _recent.setFiles(settings.value("recent-files").toStringList());
    _packPad->setValue( settings.value("pack-padding").toInt() );

    _io->setSpec(_ioSpec);
}


void AWindow::closeEvent( QCloseEvent* ev )
{
    QSettings settings;
    settings.setValue("window-size", size());
    settings.setValue("window-state", saveState());
    settings.setValue("prev-project", _prevProjPath);
    settings.setValue("prev-image", _prevImagePath);
    settings.setValue("io-pipelines", _ioSpec);
    settings.setValue("recent-files", _recent.files);
    settings.setValue("pack-padding", _packPad->value());

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

static QIcon iconAltPixmap(const char* imgOn, const char* imgOff)
{
    QIcon icon;
    icon.addPixmap(QPixmap(imgOn), QIcon::Normal, QIcon::On);
    icon.addPixmap(QPixmap(imgOff), QIcon::Normal, QIcon::Off);
    return icon;
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

    _actHideRegions = new QAction(
                iconAltPixmap(":/icons/eye-blocked.png", ":/icons/eye.png"),
                "Hide Regions", this );
    _actHideRegions->setCheckable(true);
    connect(_actHideRegions, SIGNAL(toggled(bool)), SLOT(hideRegions(bool)));

    _actLockRegions = new QAction(
                iconAltPixmap(":/icons/lock.png", ":/icons/lock-open.png"),
                "Lock Regions", this );
    _actLockRegions->setCheckable(true);
    connect(_actLockRegions, SIGNAL(toggled(bool)), SLOT(lockRegions(bool)));

    _actLockImages = new QAction(
                iconAltPixmap(":/icons/lock-image.png",
                              ":/icons/lock-image-open.png"),
                "Lock Images", this );
    _actLockImages->setCheckable(true);
    connect(_actLockImages, SIGNAL(toggled(bool)), SLOT(lockImages(bool)));

    _actPack = new QAction(QIcon(":/icons/pack.png"),
                           "&Pack Images", this );
    connect(_actPack, SIGNAL(triggered()), SLOT(packImages()));
}

void AWindow::viewReset() { static_cast<AView*>(_view)->resetTransform(); }

void AWindow::viewZoomIn() { static_cast<AView*>(_view)->zoomIn(); }

void AWindow::viewZoomOut() { static_cast<AView*>(_view)->zoomOut(); }

void AWindow::hideRegions(bool hide)
{
    bool visible = ! hide;
    each_item_mod(it) {
        if (IS_REGION(it))
            it->setVisible(visible);
    }
}

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

void AWindow::lockImages(bool lock)
{
    QGraphicsItem::GraphicsItemFlags flags;
    if (! lock)
        flags = QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable;

    each_item_mod(it) {
        if (IS_IMAGE(it))
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
    file->addAction("Export &Image...", this, SLOT(exportImage()));
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
    edit->addAction( _actPack );
    edit->addAction("Merge Images...", this, SLOT(mergeImages()),
                    QKeySequence(Qt::CTRL + Qt::Key_M));
    edit->addAction("Extract Regions...", this, SLOT(extractRegions()),
                    QKeySequence(Qt::CTRL + Qt::Key_E));
    edit->addAction("Regions to Images...", this, SLOT(convertToImage()),
                    QKeySequence(Qt::CTRL + Qt::Key_I));
    edit->addAction("Crop Images...", this, SLOT(cropImages()));
    edit->addSeparator();
    edit->addAction("Canvas &Size...", this, SLOT(editDocSize()));

    QMenu* view = bar->addMenu( "&View" );
    view->addAction( _actViewReset );
    view->addAction( _actZoomIn );
    view->addAction( _actZoomOut );
    view->addAction( _actHideRegions );
    view->addAction( _actLockRegions );
    view->addAction( _actLockImages );

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
    _tools->setObjectName("tools");
    _tools->addAction(_actOpen);
    _tools->addAction(_actSave);

    _tools->addSeparator();
    _tools->addAction(_actAddImage);
    _tools->addAction(_actAddRegion);
    _tools->addAction(_actRemove);

    _tools->addSeparator();
    _tools->addAction(_actHideRegions);
    _tools->addAction(_actLockRegions);
    _tools->addAction(_actLockImages);


    _propBar = new QToolBar;
    _propBar->setObjectName("propBar");
    _propBar->addWidget(_name = new QLineEdit);
    _propBar->addWidget(_spinX = makeSpinBox());
    _propBar->addWidget(_spinY = makeSpinBox());
    _propBar->addWidget(_spinW = makeSpinBox());
    _propBar->addWidget(_spinH = makeSpinBox());
    addToolBar(Qt::TopToolBarArea, _propBar);

    _name->setEnabled(false);
    connect(_name, SIGNAL(editingFinished()), SLOT(modName()) );
    connect(_name, SIGNAL(textEdited(const QString&)), SLOT(stringEdit()) );
    connect(_spinX, SIGNAL(valueChanged(int)), SLOT(modX(int)));
    connect(_spinY, SIGNAL(valueChanged(int)), SLOT(modY(int)));
    connect(_spinW, SIGNAL(valueChanged(int)), SLOT(modW(int)));
    connect(_spinH, SIGNAL(valueChanged(int)), SLOT(modH(int)));


    _packPad = new QSpinBox;
    _packPad->setRange(0,32);

    _packSort = new QCheckBox("Sort");

    _packBar = new QToolBar;
    _packBar->setObjectName("packBar");
    _packBar->addAction(_actPack);
    _packBar->addWidget(new QLabel("Pad:"));
    _packBar->addWidget(_packPad);
    _packBar->addWidget(_packSort);
    addToolBar(Qt::TopToolBarArea, _packBar);


    _io = new IOWidget;
    connect(_io, SIGNAL(execute(int,int)), SLOT(execute(int,int)));

    _ioBar = new QToolBar;
    _ioBar->setObjectName("ioBar");
    _ioBar->addWidget(_io);
    addToolBar(Qt::BottomToolBarArea, _ioBar);


    _search = new QLineEdit;
    _search->setPlaceholderText("search");
    connect(_search, SIGNAL(editingFinished()), SLOT(modSearch()));

    _searchBar = new QToolBar;
    _searchBar->setObjectName("searchBar");
    _searchBar->addWidget(_search);
    addToolBar(Qt::BottomToolBarArea, _searchBar);
}

void AWindow::updateProjectName(const QString& path)
{
    _prevProjPath = path;

    QString title(path);
    title.append(" - " APP_NAME);
    setWindowTitle(title);
}

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

    // Reset lock/hide to match the default state of newly loaded items.
    _actHideRegions->setChecked(false);
    _actLockRegions->setChecked(false);
    _actLockImages->setChecked(false);

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
    dir.setNameFilters(imageFilters());
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

bool AWindow::exportAtlasImage(const QString& path, int w, int h)
{
    QPixmap newPix(w, h);
    QPainter ip;

    newPix.fill(QColor(0,0,0,0));
    ip.begin(&newPix);
    each_item(gi) {
        if (IS_IMAGE(gi)) {
            QPointF pos = gi->pos();
            ip.drawPixmap(pos.x(), pos.y(), ITEM_PIXMAP(gi));
        }
    }
    ip.end();

    if (! newPix.save(path)) {
        QString error("Could not save image to file ");
        QMessageBox::critical(this, "Export Image", error + path);
        return false;
    }

    return true;
}

void AWindow::exportImage()
{
    int w, h;
    QRectF rect = _scene->itemsBoundingRect();
    if (rect.isEmpty()) {
        QMessageBox::warning(this, "Export Image",
                         "No Images found; nothing to export.");
        return;
    }

    if (_docSize.isEmpty()) {
        w = rect.width() - 1;
        h = rect.height() - 1;
    } else {
        w = _docSize.width();
        h = _docSize.height();
    }

    QString fn = QFileDialog::getSaveFileName(this, "Export Atlas Image",
                                              _prevImagePath);
    if (! fn.isEmpty()) {
        _prevImagePath = fn;
        exportAtlasImage(fn, w, h);
    }
}

void AWindow::mergeImages()
{
    int w, h;

    {
    QRectF rect = _scene->itemsBoundingRect();
    if (rect.isEmpty()) {
        QMessageBox::warning(this, "Merge Images",
                         "No Images found; nothing to merge.");
        return;
    }
    w = rect.width() - 1;
    h = rect.height() - 1;
    }

    QString file = QFileDialog::getSaveFileName(this, "Merged Image",
                                                _prevImagePath);
    if (file.isEmpty())
        return;
    _prevImagePath = file;

#if 1
    QList<QGraphicsItem *> list = _scene->items(Qt::AscendingOrder);
#else
    QList<QGraphicsItem *> list = _scene->selectedItems();
    if (list.empty())
        list = _scene->items(Qt::AscendingOrder);
#endif

    {
    QVector<QGraphicsItem*> removeList;
    QPixmap newPix(w, h);
    QPainter ip;
    QGraphicsItem* gi;
    QGraphicsPixmapItem* pitem;

    pitem = makeImage(QPixmap(), 0, 0);
    pitem->setData(ID_NAME, file);

    newPix.fill(QColor(0,0,0,0));
    ip.begin(&newPix);
    for(int i = 0; i < list.size(); ++i) {
        gi = list.at(i);
        if (IS_IMAGE(gi)) {
            QPointF pos = gi->pos();
            ip.drawPixmap(pos.x(), pos.y(), ITEM_PIXMAP(gi));

            if (removeList.indexOf(gi) < 0)
                removeList.push_back(gi);
        } else if(IS_REGION(gi)) {
            // Transfer region to new image.
            QPointF pos = gi->scenePos();
            //pos -= pitem->scenePos();
            gi->setParentItem(pitem);
            gi->setPos(pos);
        }
    }
    ip.end();
    pitem->setPixmap(newPix);

    removeItems(removeList.constData(), removeList.size());

    if (! newPix.save(file)) {
        QString error("Could not save image to file ");
        QMessageBox::critical(this, "Merge Images", error + file);
    }
    }
}

void AWindow::convertToImage()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                            "New Image Directory", _prevImagePath);
    if (dir.isEmpty())
        return;
    _prevImagePath = dir;

    if (dir.back() != '/')
        dir.append('/');

    ItemList list = _scene->selectedItems();
    if (list.empty())
        list = _scene->items(Qt::AscendingOrder);

    {
    QPainter ip;
    QString file;
    ItemValues val;
    QPointF pos;
    QGraphicsItem* gi;
    QGraphicsItem* si;
    QGraphicsPixmapItem* pitem;

    for(int i = 0; i < list.size(); ++i) {
        gi = list.at(i);
        if(IS_REGION(gi)) {
            si = gi->parentItem();
            if (si && IS_IMAGE(si)) {
                itemValues(val, gi);
                pos = gi->pos();

                QPixmap newPix(val.w, val.h);
                newPix.fill(QColor(0,0,0,0));
                ip.begin(&newPix);
                ip.drawPixmap(0, 0, ITEM_PIXMAP(si),
                              int(pos.x()), int(pos.y()), val.w, val.h);
                ip.end();

                file = dir;
                file.append(val.name);
                file.append(".png");

                if (newPix.save(file)) {
                    // Replace region with new image.
                    _scene->removeItem(gi);
                    delete gi;

                    pitem = makeImage(newPix, val.x, val.y);
                    pitem->setData(ID_NAME, file);
                } else {
                    QString error("Could not save image to file ");
                    QMessageBox::critical(this, "Convert Region", error + file);
                }
            }
        }
    }
    }
}

static int testRowAlpha(QImage& img, int y, int w) {
    QRgb rgb;
    int x;
    for (x = 0; x < w; ++x) {
        rgb = img.pixel(x, y);
        if (qAlpha(rgb) != 0)
            return x;
    }
    return x;
}

static int testRowAlphaEnd(QImage& img, int y, int w) {
    QRgb rgb;
    int x;
    for (x = w - 1; x > 0; --x) {
        rgb = img.pixel(x, y);
        if (qAlpha(rgb) != 0)
            return x;
    }
    return x;
}

static bool cropAlpha(QImage& img, QRect& rect)
{
    if (! img.hasAlphaChannel() || img.isNull())
        return false;

    int x = 0;
    int y, r;
    int lx, hx;
    int w = img.width();
    int h = img.height();

    for (y = 0; y < h; ++y) {
        x = testRowAlpha(img, y, w);
        if (x < w)
            break;
    }
    if (y == h)
        return false;   // Completely empty.
    lx = x;

    for (--h; h > y; --h) {
        x = testRowAlpha(img, h, w);
        if (x < w)
            break;
    }
    if (y == 0 && h == (img.height() - 1))
        return false;
    if (x < lx)
        lx = x;

    // Find minimum X.
    if (lx > 0) {
        for (r = y+1; r < h; ++r) {
            x = testRowAlpha(img, r, w);
            if (x < lx) {
                lx = x;
                if (lx == 0)
                    break;
            }
        }
    }

    // Find maximum X.
    hx = 0;
    for (r = y; r <= h; ++r) {
        x = testRowAlphaEnd(img, r, w);
        if (x > hx) {
            hx = x;
            if (hx == w-1)
                break;
        }
    }

    rect.setCoords(lx, y, hx, h);
    return true;
}

static void translateChildren(QGraphicsItem* item, QPointF& delta)
{
    ItemList list = item->childItems();
    QGraphicsItem* gi;
    for(int i = 0; i < list.size(); ++i) {
        gi = list.at(i);
        gi->setPos(gi->pos() - delta);
    }
}

void AWindow::cropImages()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                            "New Image Directory", _prevImagePath);
    if (dir.isEmpty())
        return;
    _prevImagePath = dir;

    if (dir.back() != '/')
        dir.append('/');

    ItemList list = _scene->selectedItems();
    if (list.empty())
        list = _scene->items(Qt::AscendingOrder);

    {
    QPainter ip;
    QString file;
    ItemValues val;
    QGraphicsItem* gi;
    QRect rect;

    for(int i = 0; i < list.size(); ++i) {
        gi = list.at(i);
        if(IS_IMAGE(gi)) {
            itemValues(val, gi);

            QImage img = ITEM_PIXMAP(gi).toImage();
            if (cropAlpha(img, rect)) {
                QPixmap newPix(rect.width(), rect.height());
                newPix.fill(QColor(0,0,0,0));
                ip.begin(&newPix);
                ip.drawImage(QPoint(0, 0), img, rect);
                ip.end();

                QFileInfo info(val.name);
                file = dir;
                file.append(info.fileName());

                if (newPix.save(file)) {
                    // Replace pixmap and move to cropped pos.
                    static_cast<QGraphicsPixmapItem*>(gi)->setPixmap(newPix);
                    QPointF delta(rect.x(), rect.y());
                    gi->setPos(gi->pos() + delta);
                    gi->setData(ID_NAME, file);
                    translateChildren(gi, delta);

                    syncSelection();    // Update information in toolbar.
                } else {
                    QString error("Could not save image to file ");
                    QMessageBox::critical(this, "Crop Images", error + file);
                }
            }
        }
    }
    }
}

void AWindow::removeItems(QGraphicsItem* const* list, int count)
{
    QGraphicsItem* item;
    for (int i = 0; i < count; ++i) {
        item = list[i];
        _scene->removeItem(item);
        delete item;
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
    if (sel.size() == 1) {
        _scene->removeItem(sel[0]);
        delete sel[0];
    } else {
        QVector<QGraphicsItem*> regList;
        QVector<QGraphicsItem*> imgList;
        QGraphicsItem* gi;
        QGraphicsItem* pi;

        // Sort items into region & image lists.
        for(int i = 0; i < sel.size(); ++i) {
            gi = sel.at(i);
            if (IS_REGION(gi)) {
                // If the parent is going away don't explicitly remove gi.
                pi = gi->parentItem();
                if (pi && pi->isSelected())
                    continue;
                regList.push_back(gi);
            } else if (IS_IMAGE(gi))
                imgList.push_back(gi);
        }

        removeItems(regList.constData(), regList.size());
        removeItems(imgList.constData(), imgList.size());
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
    if (val != spin->value()) {
        spin->blockSignals(true);
        spin->setValue(val);
        spin->blockSignals(false);
    }
}

void AWindow::sceneChange()
{
    if (_selItem) {
        QPoint pos = _selItem->scenePos().toPoint();
        assignSpin(_spinX, pos.x());
        assignSpin(_spinY, pos.y());

        if (IS_REGION(_selItem)) {
            QRectF br = static_cast<QGraphicsRectItem *>(_selItem)->rect();
            assignSpin(_spinW, br.width());
            assignSpin(_spinH, br.height());
        }
    }
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
        // _spinX & Y updated in sceneChange().
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

static void highlightItem(QGraphicsItem* item, bool on)
{
    if (IS_REGION(item)) {
        QRgb rgb = on ? qRgb(255, 255, 70) : qRgb(255, 20, 20);
        static_cast<QGraphicsRectItem*>(item)->setBrush(QColor(rgb));
    }
}

void AWindow::modSearch()
{
    QString name;
    QString pattern = _search->text();
    QGraphicsItem* prev = NULL;

    if (pattern.isEmpty()) {
        each_item_mod(gi) {
            highlightItem(gi, false);
        }
        return;
    }

    _scene->blockSignals(true);
    each_item_mod(gi) {
        if (IS_CANVAS(gi))
            continue;

        name = gi->data(ID_NAME).toString();
        if (name.contains(pattern)) {
            if (! prev)
                _scene->clearSelection();
            else {
                prev->setSelected(true);
                highlightItem(prev, true);
            }
            prev = gi;
        } else
            highlightItem(gi, false);
    }
    _scene->blockSignals(false);

    // Do the final selection change outside the loop now that signals are
    // enabled again.
    if (prev) {
        prev->setSelected(true);
        highlightItem(prev, true);
    } else
        _scene->clearSelection();
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
        if (IS_CANVAS(it)) {
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
#ifdef _WIN32
    _putenv_s("ATL", fileVar);
#else
    setenv("ATL", fileVar, 1);
#endif

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

#define HANDLE_SIZE     5

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

    void mouseMoveEvent(QGraphicsSceneMouseEvent* ev)
    {
        QPointF delta = ev->scenePos() - ev->buttonDownScenePos(Qt::LeftButton);
        switch(_activeHandle) {
            case 1:
                setPos(_resizePos + delta);
                pixelSnapDim(_resizeW - delta.x(), _resizeH - delta.y());
                break;
            case 2:
                setPos(_resizePos.x(), _resizePos.y() + delta.y());
                pixelSnapDim(_resizeW + delta.x(), _resizeH - delta.y());
                break;
            case 3:
                setPos(_resizePos.x() + delta.x(), _resizePos.y());
                pixelSnapDim(_resizeW - delta.x(), _resizeH + delta.y());
                break;
            case 4:
                pixelSnapDim(_resizeW + delta.x(), _resizeH + delta.y());
                break;
            default:
                QGraphicsRectItem::mouseMoveEvent(ev);
                break;
        }
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* ev)
    {
        static const Qt::CursorShape dragCursor[5] = {
            Qt::DragMoveCursor,
            Qt::SizeFDiagCursor, Qt::SizeBDiagCursor,
            Qt::SizeBDiagCursor, Qt::SizeFDiagCursor
            // Qt::SizeVerCursor, Qt::SizeHorCursor
        };
        if (ev->button() == Qt::LeftButton) {
            _activeHandle = handleAt(ev->pos());
            if (_activeHandle) {
                QRectF br = rect();
                _resizeW = br.width();
                _resizeH = br.height();
                _resizePos = pos();
            }
            setCursor(dragCursor[_activeHandle]);
        }
        QGraphicsRectItem::mousePressEvent(ev);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* ev)
    {
        if (ev->button() == Qt::LeftButton) {
            _activeHandle = 0;
            setCursor(Qt::ArrowCursor);
        }
        QGraphicsRectItem::mouseReleaseEvent(ev);
    }

private:
    void pixelSnapDim(qreal w, qreal h)
    {
        QPointF dim(w , h);
        pixelSnap(dim);
        if (dim.x() < 1.0)
            dim.setX(1.0);
        if (dim.y() < 1.0)
            dim.setY(1.0);
        setRect(0, 0, dim.x(), dim.y());
    }

    int handleAt(const QPointF& pos)
    {
        QRectF br = rect();
        int h;

        if (br.width() <= HANDLE_SIZE || br.height() <= HANDLE_SIZE)
            return 4;

        if (pos.y() < br.top() + HANDLE_SIZE)
            h = 1;
        else if (pos.y() > br.bottom() - HANDLE_SIZE)
            h = 3;
        else
            return 0;

        if (pos.x() < br.left() + HANDLE_SIZE)      // 1---2
            return h;                               // |   |
        if (pos.x() > br.right() - HANDLE_SIZE)     // 3---4
            return h + 1;
        return 0;
    }

    QPointF _resizePos;
    qreal _resizeW;
    qreal _resizeH;
    int _activeHandle;
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

#include "atl_read.h"

struct AtlReadContext {
    AWindow* win;
    QGraphicsItem* pitem;
};

void AWindow::atlElement(int type, const AtlRegion* reg, void* user)
{
    AtlReadContext* ctx = (AtlReadContext*) user;
    AWindow* win = ctx->win;
    switch (type) {
        case ATL_DOCUMENT:
            win->_docSize.setWidth(reg->w);
            win->_docSize.setHeight(reg->h);
            break;

        case ATL_IMAGE:
        {
            QPixmap pix(QString(reg->projPath));
            if (pix.isNull())
                pix = QPixmap(":/icons/missing.png");

            ctx->pitem = win->makeImage(pix, reg->x, reg->y);
            ctx->pitem->setData(ID_NAME, QString(reg->name));
        }
            break;

        case ATL_REGION:
            if (ctx->pitem) {
                QPointF pp = ctx->pitem->scenePos();
                QGraphicsItem* region =
                    win->makeRegion(ctx->pitem,
                                reg->x - int(pp.x()), reg->y - int(pp.y()),
                                reg->w, reg->h);
                region->setData(ID_NAME, QString(reg->name));
            }
            break;
    }
}

/*
 * Append items in project file to scene.
 *
 * Return false on error.  In this case errorLine is set to either the line
 * number where parsing failed or -1 on a file open or read error.
 */
bool AWindow::loadProject(const QString& path, int* errorLine)
{
    AtlReadContext ctx;
    ctx.win   = this;
    ctx.pitem = NULL;
    return atl_read(UTF8(path), errorLine, atlElement, &ctx);
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
        QString path(info.filePath());
        if (info.isDir())
            w.directoryImport(path);
        else if(hasImageExt(path))
            w.importImage(path);
        else
            w.openFile(path);
    }

    return app.exec();
}


//EOF
