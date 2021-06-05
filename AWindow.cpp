//============================================================================
//
// AWindow
//
//============================================================================


#include <QApplication>
#include <QFileDialog>
#include <QKeyEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QStyle>
#include "AWindow.h"
#include "Atlush.h"


AWindow::AWindow()
{
    createActions();
    createMenus();
    createTools();
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


void AWindow::save()
{
}


void AWindow::saveAs()
{
}


//----------------------------------------------------------------------------


int main( int argc, char **argv )
{
    QApplication app( argc, argv );

    AWindow w;
    w.show();

    if( argc > 1 )
        w.open( argv[1] );

    return app.exec();
}


//EOF