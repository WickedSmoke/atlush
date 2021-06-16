exe %atlush [
    qt [widgets]
    include_from %support
    sources [
        %AWindow.cpp
        %packImages.cpp
        %CanvasDialog.cpp
        %ExtractDialog.cpp
        %IOWidget.cpp
        %support/RecentFiles.cpp
        %icons.qrc
    ]
]
