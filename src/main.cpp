#include <QApplication>

#include "widgets/WelcomeWidget.h"

int main(int argc, char* argv[]) 
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("StudyWidgets");
    QApplication::setOrganizationName("deavers");

    WelcomeWidget widget;
    widget.show();

    return app.exec();
}