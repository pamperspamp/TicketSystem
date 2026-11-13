#include "mainwindow.h"
#include "database.h"

#include <QApplication>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Database database;
    QString error;
    if (!database.open(error)) {
        QMessageBox::critical(nullptr, QStringLiteral("Помилка бази даних"), error);
        return 1;
    }
    MainWindow w;
    w.show();
    return QApplication::exec();
}
