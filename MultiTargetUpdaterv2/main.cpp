#include "multitargetupdaterv2.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("Sitep_Italia_spa");
    QCoreApplication::setApplicationName("MultiTargetUpdaterv2");
    MultiTargetUpdaterv2 window;
    window.show();
    return app.exec();
}
