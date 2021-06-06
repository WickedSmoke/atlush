#ifndef AWINDOW_H
#define AWINDOW_H
//============================================================================
//
// AWindow
//
//============================================================================


#include <QMainWindow>
#include <QGraphicsView>
#include "RecentFiles.h"


class QLineEdit;
class QSpinBox;

class AWindow : public QMainWindow
{
    Q_OBJECT

public:

    AWindow();

    bool openFile(const QString& file);

public slots:

    void showAbout();

protected:

    virtual void closeEvent( QCloseEvent* );

private slots:

    void open();
    void openRecent();
    void save();
    void saveAs();
    void addImage();
    void addRegion();
    void removeSelected();
    void undo();
    void syncSelection();
    void newProject();
    void modName();
    void stringEdit();
    void modX(int);
    void modY(int);
    void modW(int);
    void modH(int);

private:

    void createActions();
    void createMenus();
    void createTools();
    void updateProjectName(const QString& path);
    QGraphicsPixmapItem* makeImage(const QPixmap&, int x, int y);
    QGraphicsRectItem* makeRegion(QGraphicsItem* parent, int, int, int, int);
    bool loadProject(const QString& path);
    bool saveProject(const QString& path);

    QAction* _actNew;
    QAction* _actOpen;
    QAction* _actSave;
    QAction* _actSaveAs;
    QAction* _actQuit;
    QAction* _actAbout;
    QAction* _actAddImage;
    QAction* _actAddRegion;
    QAction* _actRemove;
    QAction* _actUndo;

    QToolBar* _tools;
    QLineEdit* _name;
    QSpinBox*  _spinX;
    QSpinBox*  _spinY;
    QSpinBox*  _spinW;
    QSpinBox*  _spinH;

    QGraphicsScene* _scene;     // Stores our project.
    QGraphicsView* _view;
    QGraphicsItem* _selItem;
    QPointF        _selPos;

    QString _prevProjPath;
    QString _prevImagePath;
    RecentFiles _recent;
    QObject* _modifiedStr;

    // Disabled copy constructor and operator=
    AWindow( const AWindow & ) : QMainWindow( 0 ) {}
    AWindow &operator=( const AWindow & ) { return *this; }
};


#endif  //AWINDOW_H
