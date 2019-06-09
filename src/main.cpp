#include "mainwindow.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    auto qtTranslator = new QTranslator;
    if (qtTranslator->load(QLocale(), "mxs", "_", ":/res/i18n")) {
        a.installTranslator(qtTranslator);
    }
    MainWindow w;
    w.show();
    return a.exec();
}
