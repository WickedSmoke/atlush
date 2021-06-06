//============================================================================
//
// AWindow
//
//============================================================================


#include <QApplication>
#include <QFileDialog>
#include <QGraphicsPixmapItem>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QToolBar>
#include "AWindow.h"
#include "Atlush.h"

#define UTF8(str)   str.toUtf8().constData()


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

    if (item->type() == QGraphicsPixmapItem::Type) {
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

protected:
    void keyPressEvent(QKeyEvent*);
    void mousePressEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent*);
    void wheelEvent(QWheelEvent*);

private:
    QPoint _panStart;
};

void AView::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
        case Qt::Key_Equal:
            scale(1.44, 1.44);
            break;
        case Qt::Key_Minus:
        {
            const qreal inv = 1.0 / 1.44;
            scale(inv, inv);
        }
            break;
        case Qt::Key_Home:
            resetTransform();
            break;
        default:
            QGraphicsView::keyPressEvent(event);
            break;
    }
}

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
{
    setWindowTitle(APP_NAME);

    createActions();
    createMenus();
    createTools();

    _scene = new QGraphicsScene;

    _view = new AView(_scene);
    _view->setMinimumSize(128, 128);
    _view->setBackgroundBrush(QBrush(Qt::darkGray));
    setCentralWidget(_view);

    QSettings settings;
    resize(settings.value("window-size", QSize(480, 480)).toSize());
    _prevProjPath  = settings.value("prev-project").toString();
    _prevImagePath = settings.value("prev-image").toString();
    _recent.setFiles(settings.value("recent-files").toStringList());
}


void AWindow::closeEvent( QCloseEvent* ev )
{
    QSettings settings;
    settings.setValue("window-size", size());
    settings.setValue("prev-project", _prevProjPath);
    settings.setValue("prev-image", _prevImagePath);
    settings.setValue("recent-files", _recent.files);

    QMainWindow::closeEvent( ev );
}

void AWindow::showAbout()
{
    QString str(
        "<h2>" APP_NAME " " APP_VERSION "</h2>\n"
        "%1, &copy; 2021 Karl Robillard\n"
        "<h4>Key Commands</h4>\n"
        "<table>\n"
        "<tr><td width=\"64\">Del</td><td>Delete item</td>"
        "<tr><td>Home</td> <td>Reset view</td>"
        "<tr><td>=</td> <td>Zoom in</td>"
        "<tr><td>-</td> <td>Zoom out</td>"
        "</table>\n"
    );

    QMessageBox* about = new QMessageBox(this);
    about->setWindowTitle( "About " APP_NAME );
    about->setIconPixmap( QPixmap(":/icons/logo.png") );
    about->setTextFormat( Qt::RichText );
    about->setText( str.arg( __DATE__ ) );
    about->show();
}


void AWindow::createActions()
{
#define STD_ICON(id)    style()->standardIcon(QStyle::id)

    _actNew = new QAction(STD_ICON(SP_FileIcon), "&New Project", this);
    _actNew->setShortcut(QKeySequence::New);
    connect( _actNew, SIGNAL(triggered()), this, SLOT(newProject()));

    _actOpen = new QAction(STD_ICON(SP_DialogOpenButton), "&Open...", this );
    _actOpen->setShortcut(QKeySequence::Open);
    connect( _actOpen, SIGNAL(triggered()), this, SLOT(open()));

    _actSave = new QAction(STD_ICON(SP_DialogSaveButton), "&Save", this );
    _actSave->setShortcut(QKeySequence::Save);
    connect( _actSave, SIGNAL(triggered()), this, SLOT(save()));

    _actSaveAs = new QAction("Save &As...", this);
    _actSaveAs->setShortcut(QKeySequence::SaveAs);
    connect( _actSaveAs, SIGNAL(triggered()), this, SLOT(saveAs()));

    _actQuit = new QAction( "&Quit", this );
    _actQuit->setShortcut(QKeySequence::Quit);
    connect( _actQuit, SIGNAL(triggered()), this, SLOT(close()));

    _actAbout = new QAction( "&About", this );
    connect( _actAbout, SIGNAL(triggered()), this, SLOT(showAbout()));

    _actAddImage = new QAction(QIcon(":/icons/image-add.png"),
                               "Add Image", this);
    _actAddImage->setShortcut(QKeySequence(Qt::Key_Insert));
    connect(_actAddImage, SIGNAL(triggered()), this, SLOT(addImage()));

    _actAddRegion = new QAction(QIcon(":/icons/region-add.png"),
                                "Add Region", this);
    connect(_actAddRegion, SIGNAL(triggered()), this, SLOT(addRegion()));

    _actRemove = new QAction(QIcon(":/icons/image-remove.png"),
                                "Remove Item", this);
    _actRemove->setShortcut(QKeySequence(Qt::Key_Delete));
    connect(_actRemove, SIGNAL(triggered()), this, SLOT(removeSelected()));
}


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
    edit->addAction( _actAddImage );
    edit->addAction( _actAddRegion );
    edit->addAction( _actRemove );

    bar->addSeparator();

    QMenu* help = bar->addMenu( "&Help" );
    help->addAction( _actAbout );
}


void AWindow::createTools()
{
    _tools = addToolBar("");
    _tools->addAction(_actOpen);
    _tools->addAction(_actSave);
    _tools->addAction(_actAddImage);
    _tools->addAction(_actAddRegion);
    _tools->addAction(_actRemove);
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
        if (item->type() != QGraphicsPixmapItem::Type)
            item = item->parentItem();
        if (item)
            makeRegion(item, 0, 0, 32, 32);
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
    item->setData(ID_NAME, QString("<unnamed>"));

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

            if (nested) {
                if (pitem) {
                    QPointF pp = pitem->scenePos();
                    makeRegion(pitem, x - int(pp.x()), y - int(pp.y()), w, h);
                }
            } else {
                QString fn(buf);
                QPixmap pix(fn);
                if (pix.isNull())
                    pix = QPixmap(":/icons/missing.png");

                pitem = makeImage(pix, x, y);
                pitem->setData(ID_NAME, fn);
            }
            break;

        case '[':
            ++nested;
            break;

        case ']':
            --nested;
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
        if (it->type() != QGraphicsPixmapItem::Type)
            continue;
        itemValues(val, it);
        if (regionWriteBoron(fp, val) < 0)
            goto fail;

        ItemList clist = it->childItems();
        if (! clist.empty()) {
            fprintf(fp, "[\n");
            for(const QGraphicsItem* ch : clist) {
                if (ch->type() != QGraphicsRectItem::Type)
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
