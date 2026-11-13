#include "database.h"

#include <QSqlError>
#include <QSqlQuery>

bool Database::open(QString &error)
{
    connection = QSqlDatabase::addDatabase("QSQLITE");
    connection.setDatabaseName(QString::fromUtf8(COURSE_DATABASE_PATH));
    if (!connection.open()) {
        error = connection.lastError().text();
        return false;
    }

    QSqlQuery query(connection);
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        error = query.lastError().text();
        return false;
    }
    if (!connection.transaction()) {
        error = connection.lastError().text();
        return false;
    }

    const QStringList schema = {
        R"(CREATE TABLE IF NOT EXISTS halls (
            id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE))",
        R"(CREATE TABLE IF NOT EXISTS seats (
            id INTEGER PRIMARY KEY, hall_id INTEGER NOT NULL REFERENCES halls(id),
            row_number INTEGER NOT NULL CHECK(row_number > 0),
            seat_number INTEGER NOT NULL CHECK(seat_number > 0),
            UNIQUE(hall_id, row_number, seat_number)))",
        R"(CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY, title TEXT NOT NULL, kind TEXT NOT NULL,
            starts_at TEXT NOT NULL, hall_id INTEGER NOT NULL REFERENCES halls(id),
            price_kopiyky INTEGER NOT NULL CHECK(price_kopiyky >= 0)))",
        R"(CREATE TABLE IF NOT EXISTS tickets (
            id INTEGER PRIMARY KEY, event_id INTEGER NOT NULL REFERENCES events(id),
            seat_id INTEGER NOT NULL REFERENCES seats(id),
            status TEXT NOT NULL CHECK(status IN ('reserved','sold','cancelled')),
            reservation_name TEXT, code TEXT UNIQUE, paid_kopiyky INTEGER,
            sold_at TEXT, checked_in_at TEXT))",
        R"(CREATE UNIQUE INDEX IF NOT EXISTS active_ticket_seat
            ON tickets(event_id, seat_id) WHERE status IN ('reserved','sold'))"
    };
    for (const QString &statement : schema) {
        if (!query.exec(statement)) {
            error = query.lastError().text();
            connection.rollback();
            return false;
        }
    }
    if (!connection.commit()) {
        error = connection.lastError().text();
        connection.rollback();
        return false;
    }
    return true;
}