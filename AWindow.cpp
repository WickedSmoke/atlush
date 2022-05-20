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

enum UndoOpcodes {
    UNDO_POS = 1,
    UNDO_RECT
};

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
        _activeHandle = 0;
    }

    int hotspot[2];

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget)
    {
        QGraphicsRectItem::paint(painter, option, widget);

        const QGraphicsScene* gs = scene();
        if (gs && gs->property("shot").toBool()) {
            int hx = hotspot[0];
            int hy = hotspot[1];
            if (hx || hy) {
                painter->setBrush(Qt::NoBrush);
                painter->setPen(Qt::black);
                painter->drawLine(hx - 4, hy, hx + 4, hy);
                painter->drawLine(hx, hy - 4, hx, hy + 4);
            }
        }
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
            case 5:
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
            /*
            if (ev->modifiers() & Qt::ShiftModifier) {
                ev->accept();
                _activeHandle = 5;
                QPointF pnt(ev->pos());
                hotspot[0] = int(pnt.x());
                hotspot[1] = int(pnt.y());
                update();
                return;
            } else*/ {
                _activeHandle = handleAt(ev->pos());
                if (_activeHandle) {
                    QRectF br = rect();
                    _resizeW = br.width();
                    _resizeH = br.height();
                    _resizePos = pos();
                }
                setCursor(dragCursor[_activeHandle]);
            }
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

AUndoSystem::ItemShapshot::ItemShapshot(QGraphicsItem* gi) {
    item = gi;
    x = gi->x();
    y = gi->y();
}

void AUndoSystem::undoRecord(int opcode, int stride)
{
    if (! values.empty()) {
        int len = values.size();
        int partSize = (UNDO_VAL_LIMIT / stride) * stride;

        // Break large change sets into smaller steps.
        while (len > partSize) {
            len -= partSize;
            undo_record(&stack, opcode, values.data() + len, partSize);
        }

        if (len)
            undo_record(&stack, opcode, values.data(), len);

        values.clear();
        act->setEnabled(true);
        //emit undoStackChanged(stack.used);
    }
}

void AUndoSystem::snapshot(const QList<QGraphicsItem*>& items)
{
    assert(snap.empty());

    int scount = items.size();
    if (scount == 1) {
        QGraphicsItem* gi = items[0];
        if (IS_REGION(gi)) {
            region = static_cast<ARegion*>(gi);
            regionRect = region->rect();
            regionRect.moveTo( region->pos() );
            return;
        }
    }

    region = NULL;
    for(int i = 0; i < scount; ++i)
        snap.emplace_back(items.at(i));
}

/*
 * Commit changes since last snapshot() call to the undo stack.
 *
 * Deletion of items between snapshot & commit is not handled.  It is assumed
 * that snapshotInProgress() is being used to prevent this.
 */
void AUndoSystem::commit()
{
    UndoValue uval;

    if (region)
    {
        UndoValue sval;
        QRectF cr = region->rect();

        uval.u = region->data(ID_SERIAL).toUInt();
        values.push_back(uval);

        uval.s[0] = int16_t(region->x() - regionRect.x());
        uval.s[1] = int16_t(region->y() - regionRect.y());
        values.push_back(uval);

        sval.s[0] = int16_t(cr.width() - regionRect.width());
        sval.s[1] = int16_t(cr.height() - regionRect.height());
        if (sval.s[0] || sval.s[1]) {
            values.push_back(sval);
            undoRecord(UNDO_RECT, 3);
        } else if (uval.s[0] || uval.s[1]) {
            undoRecord(UNDO_POS, 2);
        } else {
            values.clear();
        }

        region = NULL;
    }
    else if (! snap.empty())
    {
        //printf("KR snap %ld\n", snap.size());
        for (const auto& it : snap) {
            if (it.item->x() != it.x || it.item->y() != it.y) {
                uval.u = it.item->data(ID_SERIAL).toUInt();
                values.push_back(uval);
                //printf("KR   mod %d %d\n", uval.u, it.item->type());

                uval.s[0] = int16_t(it.item->x() - it.x);
                uval.s[1] = int16_t(it.item->y() - it.y);
                values.push_back(uval);
            }
        }

        undoRecord(UNDO_POS, 2);
        snap.clear();
    }
}

//----------------------------------------------------------------------------

class AView : public QGraphicsView
{
public:
    AView(QGraphicsScene* scene, AUndoSystem* undoSys)
        : QGraphicsView(scene), _undo(undoSys)
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
    void mouseReleaseEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent*);
    void wheelEvent(QWheelEvent*);

private:
    QPoint _panStart;
    AUndoSystem* _undo;
};

void AView::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton &&
        ev->modifiers() & Qt::ShiftModifier) {
        ItemList list = scene()->selectedItems();
        QGraphicsItem* gi = list.empty() ? itemAt(ev->pos()) : list[0];

        if (gi && IS_REGION(gi)) {
            QPointF pnt( gi->mapFromScene(mapToScene(ev->pos())) );
            ARegion* region = static_cast<ARegion*>(gi);
            region->hotspot[0] = int(pnt.x());
            region->hotspot[1] = int(pnt.y());
            region->update();

            static_cast<AWindow*>(parentWidget())->updateHotspot(
                                    region->hotspot[0], region->hotspot[1]);
        }
    } else if (ev->button() == Qt::MiddleButton) {
        _panStart = ev->pos();
        //setCursor(Qt::ClosedHandCursor);
        ev->accept();
    } else {
        QGraphicsView::mousePressEvent(ev);

        // Snapshot items for undo.
        if (ev->button() == Qt::LeftButton) {
            ItemList sel = scene()->selectedItems();
            _undo->snapshot(sel);
        }
    }
}

void AView::mouseReleaseEvent(QMouseEvent* ev)
{
    QGraphicsView::mouseReleaseEvent(ev);

    // Record any item changes.
    if (ev->button() == Qt::LeftButton)
        _undo->commit();
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
    _serialNo = 0;
    setWindowTitle(APP_NAME);

    createActions();
    createMenus();
    createTools();

    undo_init(&_undo.stack, 1024*16);
    _undo.act = _actUndo;

    _scene = new QGraphicsScene;
    connect(_scene, SIGNAL(selectionChanged()), SLOT(syncSelection()));
    connect(_scene, SIGNAL(changed(const QList<QRectF>&)), SLOT(sceneChange()));

    _view = new AView(_scene, &_undo);
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
    _actShowHot->setChecked(settings.value("show-hotspots", false).toBool());
    _packPad->setValue( settings.value("pack-padding").toInt() );

    _io->setSpec(_ioSpec);
}

AWindow::~AWindow()
{
    undo_free(&_undo.stack);
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
    settings.setValue("show-hotspots", _actShowHot->isChecked());
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

    _actRedo = new QAction( "&Redo", this );
    _actRedo->setShortcut(QKeySequence::Redo);
    _actRedo->setEnabled(false);
    connect(_actRedo, SIGNAL(triggered()), SLOT(redo()));

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

    _actShowHot = new QAction("Show &Hotspots", this);
    _actShowHot->setCheckable(true);

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
    edit->addAction( _actRedo );
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
    view->addAction( _actShowHot );

    QMenu* sett = bar->addMenu( "&Settings" );
    sett->addAction("Configure &Pipelines...", this, SLOT(editPipelines()));

    bar->addSeparator();

    QMenu* help = bar->addMenu( "&Help" );
    help->addAction( _actAbout );
}

static QSpinBox* makeSpinBox()
{
    QSpinBox* spin = new QSpinBox;
    spin->setRange(0, 8192);
    spin->setEnabled(false);
    return spin;
}

static QSpinBox* makeSpinHot()
{
    QSpinBox* spin = new QSpinBox;
    spin->setRange(-255, 1024);
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

    _hotspotBar = new QToolBar;
    _hotspotBar->setObjectName("hotspotBar");
    _hotspotBar->addWidget(_spinHotX = makeSpinHot());
    _hotspotBar->addWidget(_spinHotY = makeSpinHot());
    _hotspotBar->setVisible(false);
    addToolBar(Qt::TopToolBarArea, _hotspotBar);
    connect(_actShowHot, SIGNAL(toggled(bool)), SLOT(showHotspots(bool)));

    _name->setEnabled(false);
    connect(_name, SIGNAL(editingFinished()), SLOT(modName()) );
    connect(_name, SIGNAL(textEdited(const QString&)), SLOT(stringEdit()) );
    connect(_spinX, SIGNAL(valueChanged(int)), SLOT(modX(int)));
    connect(_spinY, SIGNAL(valueChanged(int)), SLOT(modY(int)));
    connect(_spinW, SIGNAL(valueChanged(int)), SLOT(modW(int)));
    connect(_spinH, SIGNAL(valueChanged(int)), SLOT(modH(int)));
    connect(_spinHotX, SIGNAL(valueChanged(int)), SLOT(modHotX(int)));
    connect(_spinHotY, SIGNAL(valueChanged(int)), SLOT(modHotY(int)));


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

void AWindow::showHotspots(bool shown)
{
    _hotspotBar->setVisible(shown);
    _scene->setProperty("shot", shown);
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
    newProject();

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
            QGraphicsItem* child = makeRegion(item, 0, 0, 32, 32, 0, 0);
            child->setData(ID_NAME, QString("<unnamed>"));
        }
    }
}

void AWindow::removeSelected()
{
    if (_undo.snapshotInProgress())
        return;

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

void AWindow::undoClear()
{
    undo_clear(&_undo.stack);
    _actUndo->setEnabled(false);
    _actRedo->setEnabled(false);
}

static void undoPosition(QGraphicsScene* scene, const UndoValue* step,
                         const UndoValue* end, bool redo)
{
    QList<QGraphicsItem *> list = scene->items();

    for (; step != end; step += 2) {
        // Slog through all items to find serial number.
        for (auto it : list) {
            if (it->data(ID_SERIAL).toUInt() == step->u) {
                QPointF pos = it->pos();
                QPointF delta(float(step[1].s[0]),
                              float(step[1].s[1]));
                //printf( "KR # %d %f,%f\n", step->u, delta.x(), delta.y());
                if (redo)
                    pos += delta;
                else
                    pos -= delta;
                it->setPos(pos);
                break;
            }
        }
    }
}

static void undoRect(QGraphicsScene* scene, const UndoValue* step,
                     const UndoValue* end, bool redo)
{
    QList<QGraphicsItem *> list = scene->items();

    for (; step != end; step += 3) {
        for (auto it : list) {
            if (it->data(ID_SERIAL).toUInt() == step->u) {
                QPointF dpos(float(step[1].s[0]), float(step[1].s[1]));
                QPointF ddim(float(step[2].s[0]), float(step[2].s[1]));
                if (! redo) {
                    dpos *= -1.0f;
                    ddim *= -1.0f;
                }

                ARegion* region = static_cast<ARegion*>(it);
                QPointF pos = region->pos();
                QRectF rect = region->rect();
                region->setPos(pos + dpos);
                region->setRect(0.0f, 0.0f, rect.width() + ddim.x(),
                                            rect.height() + ddim.y());
                break;
            }
        }
    }
}

void AWindow::undo()
{
    const UndoValue* step;
    int adv = undo_stepBack(&_undo.stack, &step);
    if (adv & Undo_AdvancedToEnd)
        _actUndo->setEnabled(false);
    if (adv & Undo_AdvancedFromStart)
        _actRedo->setEnabled(true);
    if (! adv)
        return;

    switch (step->op.code) {
        case UNDO_POS:
            undoPosition(_scene, step + 1, step + step->op.skipNext, false);
            break;
        case UNDO_RECT:
            undoRect(_scene, step + 1, step + step->op.skipNext, false);
            break;
    }
}

void AWindow::redo()
{
    const UndoValue* step;
    int adv = undo_stepForward(&_undo.stack, &step);
    if (adv & Undo_AdvancedToEnd)
        _actRedo->setEnabled(false);
    if (adv & Undo_AdvancedFromStart)
        _actUndo->setEnabled(true);
    if (! adv)
        return;

    switch (step->op.code) {
        case UNDO_POS:
            undoPosition(_scene, step + 1, step + step->op.skipNext, true);
            break;
        case UNDO_RECT:
            undoRect(_scene, step + 1, step + step->op.skipNext, true);
            break;
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

void AWindow::updateHotspot(int x, int y)
{
    assignSpin(_spinHotX, x);
    assignSpin(_spinHotY, y);
}

void AWindow::syncSelection()
{
    ItemList sel = _scene->selectedItems();
    bool isSelected = ! sel.empty();
    bool isRegion = isSelected && (sel[0]->type() == GIT_RECT);

    _actAddRegion->setEnabled(isSelected);
    _actRemove->setEnabled(isSelected);

    _name->setEnabled(isRegion);
    _spinX->setEnabled(isSelected);
    _spinY->setEnabled(isSelected);
    _spinW->setEnabled(isRegion);
    _spinH->setEnabled(isRegion);
    _spinHotX->setEnabled(isRegion);
    _spinHotY->setEnabled(isRegion);

    if (isSelected) {
        ItemValues val;
        itemValues(val, sel[0]);

        _name->setText(val.name);
        // _spinX & Y updated in sceneChange().
        assignSpin(_spinW, val.w);
        assignSpin(_spinH, val.h);

        if (isRegion && _actShowHot->isChecked()) {
            const int* hot = static_cast<ARegion *>(sel[0])->hotspot;
            assignSpin(_spinHotX, hot[0]);
            assignSpin(_spinHotY, hot[1]);
        }

        _selItem = sel[0];
        //_selPos = _selItem->pos();
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

// Set either hotX or hotY.
static void setHotspotN(QGraphicsItem* gi, int i, int val)
{
    if (gi && gi->type() == GIT_RECT) {
        ARegion* region = static_cast<ARegion*>(gi);
        region->hotspot[i] = val;
        gi->update();
    }
}

void AWindow::modHotX(int n)
{
    setHotspotN(_selItem, 0, n);
}

void AWindow::modHotY(int n)
{
    setHotspotN(_selItem, 1, n);
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
// Project methods

void AWindow::newProject()
{
    _scene->clear();
    undoClear();
    _serialNo = 0;
}

QGraphicsPixmapItem* AWindow::makeImage(const QPixmap& pix, int x, int y)
{
    QGraphicsPixmapItem* item = new AImage;
    item->setData(ID_SERIAL, ++_serialNo);
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
                                       int w, int h, int hotx, int hoty)
{
    QRectF rect(0.0, 0.0, w, h);
    ARegion* item = new ARegion(parent);

    item->setData(ID_SERIAL, ++_serialNo);
    item->setRect(rect);
    item->setPen(QPen(Qt::NoPen));
    item->setBrush(QColor(255, 20, 20));
    item->setOpacity(0.5);
    item->setFlags(QGraphicsItem::ItemIsMovable |
                   QGraphicsItem::ItemIsSelectable |
                   QGraphicsItem::ItemSendsGeometryChanges);
    item->setPos(x, y);
    item->hotspot[0] = hotx;
    item->hotspot[1] = hoty;

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
                                reg->w, reg->h, reg->hotx, reg->hoty);
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

static int regionWriteBoron(FILE* fp, const ItemValues& iv,
                            const int* hotspot) {
    char end = (hotspot && (hotspot[0] || hotspot[1])) ? ',' : '\n';
    int n = fprintf(fp, "\"%s\" %d,%d,%d,%d%c",
                    iv.name.constData(), iv.x, iv.y, iv.w, iv.h, end);
    if (n > 0 && end == ',')
        n = fprintf(fp, "%d,%d\n", hotspot[0], hotspot[1]);
    return n;
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
        if (regionWriteBoron(fp, val, NULL) < 0)
            goto fail;

        ItemList clist = it->childItems();
        if (! clist.empty()) {
            fprintf(fp, "[\n");
            for(const QGraphicsItem* ch : clist) {
                if (IS_REGION(ch)) {
                    const ARegion* region = static_cast<const ARegion *>(ch);
                    fprintf(fp, "  ");
                    itemValues(val, ch);
                    if (regionWriteBoron(fp, val, region->hotspot) < 0)
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
