#include <QApplication>
#include <QSurfaceFormat>
#include "MeshViewer.h"

int main(int argc, char* argv[])
{
    // Request OpenGL 3.3 core profile explicitly — required for
    // QOpenGLFunctions_3_3_Core on systems where the default format
    // doesn't request a core profile, preventing startup crash.
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Hermes Mesh Viewer"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    MeshViewer viewer;

    // Load mesh BEFORE show() to ensure data is available
    // when initializeGL() is called during the first paint.
    const char* load_path = (argc > 1) ? argv[1] : "./data/sphere_spider.stl";
    viewer.loadFile(load_path);

    viewer.show();

    return app.exec();
}
