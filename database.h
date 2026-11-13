#pragma once

#include <QSqlDatabase>
#include <QString>

class Database
{
public:
    bool open(QString &error);

private:
    QSqlDatabase connection;
};