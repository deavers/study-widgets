#include <QApplication>
#include <QMessageBox>

#include "ControlPanel.h"
#include "DataStore.h"
#include "CategoryConfigLoader.h"

int main(int argc, char* argv[]) 
{
    QApplication app(argc, argv);

    QApplication::setApplicationName("StudyWidgets");
    QApplication::setOrganizationName("deavers");

    app.setQuitOnLastWindowClosed(false);

    if (!DataStore::instance().open()) 
    {
        QMessageBox::critical(
            nullptr,
            "StudyWidgets database error",
            "The local SQLite database could not be opened.\n\n" +
            DataStore::instance().lastError()
        );

        return 1;
    }

    QString categoryConfigError;

    if (!CategoryConfigLoader::importLocalCategories(
        &categoryConfigError
    )) {
        QMessageBox::warning(
            nullptr,
            "StudyWidgets category configuration",
            "Could not import local categories.\n\n" +
            categoryConfigError
        );
    }

    ControlPanel controlPanel;
    controlPanel.show();

    return app.exec();
}