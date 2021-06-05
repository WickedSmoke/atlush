#ifndef AWINDOW_H
#define AWINDOW_H
//============================================================================
//
// AWindow
//
//============================================================================


#include <QMainWindow>
#include <QGraphicsView>
#include "Atlush.h"


class AWindow : public QMainWindow
{
    Q_OBJECT

public:

    AWindow();
    ~AWindow();

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

private:

    void createActions();
    void createMenus();
    void createTools();
    void clearProject();
    IAGroup* addGroup(const QString& name, int x, int y, int w, int h);

    QAction* _actOpen;
    QAction* _actSave;
    QAction* _actQuit;
    QAction* _actAbout;
    QAction* _actAddImage;

    QToolBar* _tools;

    QGraphicsScene* _scene;
    QGraphicsView* _view;

    QString _prevProjPath;
    QString _prevImagePath;
    std::vector<IAGroup*> _groups;

    // Disabled copy constructor and operator=
    AWindow( const AWindow & ) : QMainWindow( 0 ) {}
    AWindow &operator=( const AWindow & ) { return *this; }
};


#endif  //AWINDOW_H
