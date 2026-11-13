#pragma once

#include "event.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

class Database
{
public:
    bool open(QString &error);
    QVector<Event> loadEvents(QString *error = nullptr) const;

private:
    bool seedIfEmpty(QSqlQuery &query, QString &error);
    QSqlDatabase connection;
};