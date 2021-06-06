OBJECTS_DIR = obj
MOC_DIR = qt_gen

CONFIG += qt debug
QT += widgets
RESOURCES += icons.qrc

HEADERS += AWindow.h IOWidget.h RecentFiles.h

SOURCES += AWindow.cpp IOWidget.cpp RecentFiles.cpp

