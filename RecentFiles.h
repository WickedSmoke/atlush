#ifndef RECENTFILES_H
#define RECENTFILES_H


#include <QString>


class QAction;
class QMenu;

class RecentFiles
{
public:

    void install( QMenu* menu, QObject* receiver, const char* method );
    void setFiles( const QStringList& );
    void addFile( const QString* );
    QString fileOpened( QObject* sender );

    QStringList files;

private:

    enum { MaxRecentFiles = 5 };
    QAction* _action[ MaxRecentFiles ];
    QAction* _separator;
};


#endif  // RECENTFILES_H
