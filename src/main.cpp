#include <QApplication>

#include "ControlPanel.h"

int main(int argc, char* argv[]) 
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("StudyWidgets");
    QApplication::setOrganizationName("deavers");

    app.setQuitOnLastWindowClosed(false);

    ControlPanel controlPanel;
    controlPanel.show();

    return app.exec();
}