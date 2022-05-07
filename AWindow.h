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
#include "undo.h"


class QCheckBox;
class QLineEdit;
class QSpinBox;
class IOWidget;
class IODialog;
class CanvasDialog;
struct AtlRegion;

class AWindow : public QMainWindow
{
    Q_OBJECT

public:

    AWindow();
    ~AWindow();

    bool openFile(const QString& file);
    bool directoryImport(const QString& path);
    QGraphicsPixmapItem* importImage(const QString& file);

public slots:

    void showAbout();

protected:

    virtual void closeEvent( QCloseEvent* );

private slots:

    void open();
    void openRecent();
    void save();
    void saveAs();
    void importDir();
    void exportImage();
    void addImage();
    void addRegion();
    void removeSelected();
    void undo();
    void redo();
    void viewReset();
    void viewZoomIn();
    void viewZoomOut();
    void hideRegions(bool);
    void lockRegions(bool);
    void lockImages(bool);
    void showHotspots(bool);
    void sceneChange();
    void syncSelection();
    void newProject();
    void modName();
    void modSearch();
    void stringEdit();
    void modX(int);
    void modY(int);
    void modW(int);
    void modH(int);
    void modHotX(int);
    void modHotY(int);
    void packImages();
    void mergeImages();
    void extractRegions();
    void convertToImage();
    void cropImages();
    void editDocSize();
    void canvasChanged();
    void editPipelines();
    void pipelinesChanged();
    void execute(int pi, int push);

private:

    static void atlElement(int, const AtlRegion*, void*);

    void createActions();
    void createMenus();
    void createTools();
    void undoClear();
    void updateProjectName(const QString& path);
    bool exportAtlasImage(const QString& path, int w, int h);
    void removeItems(QGraphicsItem* const* list, int count);
    QGraphicsPixmapItem* makeImage(const QPixmap&, int x, int y);
    QGraphicsRectItem* makeRegion(QGraphicsItem* parent, int, int, int, int,
                                  int, int);
    bool loadProject(const QString& path, int* errorLine);
    bool saveProject(const QString& path);
    void extractRegionsOp(const QString& file, const QColor& color);
    void updateHotspot(int x, int y);

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
    QAction* _actRedo;
    QAction* _actViewReset;
    QAction* _actZoomIn;
    QAction* _actZoomOut;
    QAction* _actHideRegions;
    QAction* _actLockRegions;
    QAction* _actLockImages;
    QAction* _actShowHot;
    QAction* _actPack;

    QToolBar* _tools;
    QToolBar* _propBar;
    QLineEdit* _name;
    QSpinBox*  _spinX;
    QSpinBox*  _spinY;
    QSpinBox*  _spinW;
    QSpinBox*  _spinH;
    QObject* _modifiedStr;
    CanvasDialog* _canvasDialog;

    QToolBar* _hotspotBar;
    QSpinBox* _spinHotX;
    QSpinBox* _spinHotY;

    QToolBar* _ioBar;
    IOWidget* _io;
    IODialog* _ioDialog;

    QToolBar* _packBar;
    QSpinBox* _packPad;
    QCheckBox* _packSort;

    QToolBar*  _searchBar;
    QLineEdit* _search;

    QGraphicsScene* _scene;     // Stores our project.
    QGraphicsView* _view;
    QGraphicsItem* _selItem;
    QPixmap        _bgPix;
    QSize          _docSize;
    UndoStack      _undo;
    uint32_t       _serialNo;

    // Settings
    QString _prevProjPath;
    QString _prevImagePath;
    QString _ioSpec;
    RecentFiles _recent;

    // Disabled copy constructor and operator=
    AWindow( const AWindow & ) : QMainWindow( 0 ) {}
    AWindow &operator=( const AWindow & ) { return *this; }

    friend class AView;
};


#endif  //AWINDOW_H
