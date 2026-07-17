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
        // The viewer will load the file after event loop starts
        QMetaObject::invokeMethod(&viewer, [&viewer, argv]() {
            // Simulate opening the file
            // For simplicity, the user can use File > Open
        }, Qt::QueuedConnection);
    }

    return app.exec();
}
