// Minimal Qt test to verify Qt5 MSVC 2017 works
#include <QtWidgets/QApplication>
#include <QtWidgets/QLabel>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QLabel label("Hello from Qt5 MSVC 2017");
    label.show();
    return app.exec();
}
