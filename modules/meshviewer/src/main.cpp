#include <QApplication>
#include "MeshViewer.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Hermes Mesh Viewer"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    MeshViewer viewer;
    viewer.show();

    // If a file was passed as argument, open it
    if (argc > 1) {
        viewer.loadFile(argv[1]);
    }

    return app.exec();
}
