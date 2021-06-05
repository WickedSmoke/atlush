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
    iv.w = (int) rect.width();
    iv.h = (int) rect.height();
}

#define each_item(pi) \
    for(const QGraphicsItem* pi : _scene->items(Qt::AscendingOrder))

#define each_child(pi,ci) \
    for(const QGraphicsItem* ci : pi->childItems())

//----------------------------------------------------------------------------

AWindow::AWindow()
{
    setWindowTitle(APP_NAME);

    createActions();
    createMenus();
    createTools();

    _scene = new QGraphicsScene;

    _view = new QGraphicsView(_scene);
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
    QMessageBox::information( this, "About " APP_NAME,
        "Version " APP_VERSION "\n\nCopyright (c) 2021 Karl Robillard" );
}


void AWindow::createActions()
{
#define STD_ICON(id)    style()->standardIcon(QStyle::id)

    _actNew = new QAction(STD_ICON(SP_FileIcon), "&New Project", this);
    _actNew->setShortcuts(QKeySequence::New);
    connect( _actNew, SIGNAL(triggered()), this, SLOT(newProject()));

    _actOpen = new QAction(STD_ICON(SP_DialogOpenButton), "&Open...", this );
    _actOpen->setShortcuts(QKeySequence::Open);
    connect( _actOpen, SIGNAL(triggered()), this, SLOT(open()));

    _actSave = new QAction(STD_ICON(SP_DialogSaveButton), "&Save", this );
    _actSave->setShortcuts(QKeySequence::Save);
    connect( _actSave, SIGNAL(triggered()), this, SLOT(save()));

    _actSaveAs = new QAction("Save &As...", this);
    _actSaveAs->setShortcuts(QKeySequence::SaveAs);
    connect( _actSaveAs, SIGNAL(triggered()), this, SLOT(saveAs()));

    _actQuit = new QAction( "&Quit", this );
    _actQuit->setShortcuts(QKeySequence::Quit);
    connect( _actQuit, SIGNAL(triggered()), this, SLOT(close()));

    _actAbout = new QAction( "&About", this );
    connect( _actAbout, SIGNAL(triggered()), this, SLOT(showAbout()));

    _actAddImage = new QAction(QIcon(":/icons/image-add.png"),
                               "Add Image", this);
    connect( _actAddImage, SIGNAL(triggered()), this, SLOT(addImage()));
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
}


bool AWindow::openFile(const QString& file)
{
    _scene->clear();

    if (loadProject(file)) {
        QString title(file);
        title.append(" - " APP_NAME);
        setWindowTitle(title);
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
    QString path(_prevProjPath);

    fn = QFileDialog::getOpenFileName(this, "Open File", path);
    if (! fn.isEmpty()) {
        _prevProjPath = fn;
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
    QString path(_prevProjPath);

    fn = QFileDialog::getSaveFileName(this, "Save Project As", path);
    if (! fn.isEmpty()) {
        _prevProjPath = fn;
        if (! saveProject(fn))
            saveFailed(this, fn);
    }
}


void AWindow::addImage()
{
    QString fn;
    QString path(_prevImagePath);

    fn = QFileDialog::getOpenFileName(this, "Open File", path);
    if( ! fn.isEmpty() ) {
        _prevImagePath = fn;

        QPixmap pix(fn);
        if (! pix.isNull()) {
            QGraphicsPixmapItem* item = _scene->addPixmap(pix);
            item->setFlags(QGraphicsItem::ItemIsMovable);
            item->setData(ID_NAME, fn);
        }
    }
}

//----------------------------------------------------------------------------
// Project methods

void AWindow::newProject()
{
    _scene->clear();
}

/*
 * Append items in project file to scene.
 */
bool AWindow::loadProject(const QString& path)
{
    FILE* fp = fopen(UTF8(path), "r");
    if (! fp)
        return false;

    {
    char* name = new char[1000];
    int x, y, w, h;
    while (fscanf(fp, "\"%999[^\"]\" %d,%d,%d,%d\n", name, &x,&y,&w,&h) == 5) {
        //printf("KR %s %d,%d,%d,%d\n", name, x, y, w, h);

        QString fn(name);
        QPixmap pix(fn);
        if (pix.isNull())
            pix = QPixmap("icons/missing.png");

        QGraphicsPixmapItem* item = _scene->addPixmap(pix);
        item->setFlags(QGraphicsItem::ItemIsMovable);
        item->setData(ID_NAME, fn);
        item->setOffset(x, y);
    }
    delete[] name;
    }

    fclose(fp);
    return true;
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
        itemValues(val, it);
        if (regionWriteBoron(fp, val) < 0)
            goto fail;
        each_child(it, ch) {
            itemValues(val, ch);
            if (regionWriteBoron(fp, val) < 0)
                goto fail;
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
