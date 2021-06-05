#ifndef AWINDOW_H
#define AWINDOW_H
//============================================================================
//
// AWindow
//
//============================================================================


#include <QMainWindow>
#include <QGraphicsView>


class AWindow : public QMainWindow
{
    Q_OBJECT

public:

    AWindow();

public slots:

    void open( const QString& file );
    void showAbout();

protected:

    virtual void closeEvent( QCloseEvent* );

private slots:

    void open();
    void save();
    void saveAs();
    void addImage();
    void newProject();

private:

    void createActions();
    void createMenus();
    void createTools();
    bool loadProject(const QString& path);
    bool saveProject(const QString& path);

    QAction* _actNew;
    QAction* _actOpen;
    QAction* _actSave;
    QAction* _actSaveAs;
    QAction* _actQuit;
    QAction* _actAbout;
    QAction* _actAddImage;

    QToolBar* _tools;

    QGraphicsScene* _scene;     // Stores our project.
    QGraphicsView* _view;

    QString _prevProjPath;
    QString _prevImagePath;

    // Disabled copy constructor and operator=
    AWindow( const AWindow & ) : QMainWindow( 0 ) {}
    AWindow &operator=( const AWindow & ) { return *this; }
};


#endif  //AWINDOW_H
