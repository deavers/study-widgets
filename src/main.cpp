#include <QApplication>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("StudyWidgets");
    QApplication::setOrganizationName("StudyWidgets");

    QWidget window;
    window.setWindowTitle("StudyWidgets");
    window.resize(420,480);
    window.show();

    return app.exec();
}