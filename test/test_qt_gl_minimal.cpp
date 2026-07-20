// Minimal QOpenGLWidget test
#include <QApplication>
#include <QOpenGLWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QOpenGLWidget widget;
    widget.resize(400, 300);
    widget.show();
    return app.exec();
}
