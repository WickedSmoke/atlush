#ifndef AWINDOW_H
#define AWINDOW_H
//============================================================================
//
// AWindow
//
//============================================================================


#include <QMainWindow>


class AWindow : public QMainWindow
{
    Q_OBJECT

public:

    AWindow();

public slots:

    void open( const QString& file );
    void showAbout();

protected:

    virtual void keyPressEvent( QKeyEvent* );
    virtual void mousePressEvent( QMouseEvent* );

    /*
    virtual void mouseReleaseEvent( QMouseEvent* );
    virtual void mouseMoveEvent( QMouseEvent* );
    virtual void focusInEvent( QFocusEvent * );
    virtual void focusOutEvent( QFocusEvent * );
    */

private slots:

    void open();
    void save();
    void saveAs();

private:

    void createActions();
    void createMenus();
    void createTools();

    QAction* _actOpen;
    QAction* _actSave;
    QAction* _actQuit;
    QAction* _actAbout;

    // Disabled copy constructor and operator=
    AWindow( const AWindow & ) : QMainWindow( 0 ) {}
    AWindow &operator=( const AWindow & ) { return *this; }
};


#endif  //AWINDOW_H
