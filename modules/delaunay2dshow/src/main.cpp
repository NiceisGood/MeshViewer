#include "Delaunay2DShow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Delaunay Triangulation Demo"));

    Delaunay2DShow window;
    window.show();

    return app.exec();
}
