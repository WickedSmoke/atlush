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
}


AWindow::~AWindow()
{
    for (auto it: _groups)
        delete it;
}

void AWindow::closeEvent( QCloseEvent* ev )
{
    QSettings settings;
    settings.setValue("window-size", size());
    settings.setValue("prev-project", _prevProjPath);
    settings.setValue("prev-image", _prevImagePath);

    QMainWindow::closeEvent( ev );
}

void AWindow::showAbout()
{
    QMessageBox::information( this, "About " APP_NAME,
        "Version " APP_VERSION "\n\nCopyright (c) 2021 Karl Robillard" );
}


void AWindow::keyPressEvent( QKeyEvent* e )
{
    switch( e->key() )
    {
        case Qt::Key_Q:
        case Qt::Key_Escape:
            close();
            break;

        default:
            e->ignore();
            break;
    }
}


void AWindow::mousePressEvent( QMouseEvent* )
{
}


void AWindow::createActions()
{
#define STD_ICON(id)    style()->standardIcon(QStyle::id)

    _actOpen = new QAction(STD_ICON(SP_DialogOpenButton), "&Open...", this );
    _actOpen->setShortcuts(QKeySequence::Open);
    connect( _actOpen, SIGNAL(triggered()), this, SLOT(open()));

    _actSave = new QAction(STD_ICON(SP_DialogSaveButton), "&Save", this );
    _actSave->setShortcuts(QKeySequence::Save);
    connect( _actSave, SIGNAL(triggered()), this, SLOT(save()));

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
    file->addAction( _actOpen );
    file->addAction( _actSave );
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


void AWindow::open( const QString& file )
{
    if( 1 )
    {
        setWindowTitle( file );
    }
    else
    {
        QString error( "Error opening file " );
        QMessageBox::warning( this, "Load", error + file );
    }
}


void AWindow::open()
{
    QString fn;
    QString path( "" /*lastSampleFileName*/ );

    //if( ! path.isNull() )
    //    pathTruncate( path );

    fn = QFileDialog::getOpenFileName( this, "Open File", path );
    if( ! fn.isEmpty() )
        open( fn );
}


#define UTF8(str)   str.toUtf8().constData()

static int iar_save(FILE* fp, const IARegion* reg) {
    return fprintf(fp, "\"%s\" %d,%d,%d,%d\n",
            UTF8(reg->name), reg->x, reg->y, reg->w, reg->h);
}

static bool saveProject(const QString& path, std::vector<IAGroup*> groups)
{
    FILE* fp = fopen(UTF8(path), "w");
    if (! fp)
        return false;

    bool done = false;
    for (auto gr: groups) {
        if (iar_save(fp, gr) < 0)
            goto fail;
        for (auto ch: gr->children) {
            if (iar_save(fp, &ch) < 0)
                goto fail;
        }
    }
    done = true;

fail:
    fclose(fp);
    return done;
}


void AWindow::save()
{
    saveAs();
}


void AWindow::saveAs()
{
    QString fn;
    QString path(_prevProjPath);

    fn = QFileDialog::getSaveFileName(this, "Save Project As", path);
    if (! fn.isEmpty()) {
        _prevProjPath = fn;
        if (! saveProject(fn, _groups)) {
            QString error( "Error saving project " );
            QMessageBox::warning(this, "Save Failed", error + fn);
        }
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
            IAGroup* grp = new IAGroup;
            grp->name = fn;
            grp->setArea(0, 0, pix.width(), pix.height());
            _groups.push_back(grp);

            QGraphicsPixmapItem* item = _scene->addPixmap(pix);
            item->setOffset(grp->x, grp->y);
        }
    }
}

//----------------------------------------------------------------------------


int main( int argc, char **argv )
{
    QApplication app( argc, argv );
    app.setOrganizationName( APP_NAME );
    app.setApplicationName( APP_NAME );

    AWindow w;
    w.show();

    if( argc > 1 )
        w.open( argv[1] );

    return app.exec();
}


//EOF
