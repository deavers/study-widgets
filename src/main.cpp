#include <QApplication>
#include <QMessageBox>

#include "WidgetRegistry.h"

int main(int argc, char* argv[]) 
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("StudyWidgets");
    QApplication::setOrganizationName("deavers");

    QWidget* welcomeWidget = createWidgetById("welcome");

    if (welcomeWidget == nullptr) 
    {
        QMessageBox::critical(
            nullptr,
            "StudyWidgets",
            "The welcome widget was not registered."
        );

        return 1;
    }

    welcomeWidget->show();

    const int exitCode = app.exec();

    delete welcomeWidget;

    return exitCode;
}