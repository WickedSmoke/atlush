OBJECTS_DIR = obj
MOC_DIR = qt_gen

CONFIG += qt debug
QT += widgets
RESOURCES = icons.qrc

INCLUDEPATH = support

HEADERS = AWindow.h ItemValues.h CanvasDialog.h ExtractDialog.h \
	IOWidget.h support/RecentFiles.h support/undo.h

SOURCES = AWindow.cpp packImages.cpp CanvasDialog.cpp ExtractDialog.cpp \
	IOWidget.cpp support/RecentFiles.cpp support/undo.c
