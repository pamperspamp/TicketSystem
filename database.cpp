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

    if (!seedIfEmpty(query, error)) {
        connection.rollback();
        return false;
    }

    if (!connection.commit()) {
        error = connection.lastError().text();
        connection.rollback();
        return false;
    }
    return true;
}

bool Database::seedIfEmpty(QSqlQuery &query, QString &error)
{
    if (!query.exec("SELECT COUNT(*) FROM events")) {
        error = query.lastError().text();
        return false;
    }
    if (query.next() && query.value(0).toInt() > 0)
        return true;

    const QStringList seedQueries = {
        R"(INSERT OR IGNORE INTO halls (id, name) VALUES
            (1, 'Велика'), (2, 'Камерна'), (3, 'Кінозала'))",
        R"(INSERT INTO events (id, title, kind, starts_at, hall_id, price_kopiyky) VALUES
            (1, 'Лісова пісня', 'Вистава', '2026-11-20T18:00:00', 1, 25000),
            (2, 'Джазовий вечір', 'Концерт', '2026-11-21T19:30:00', 2, 32000),
            (3, 'Вечір кіно', 'Кіно', '2026-11-22T17:00:00', 3, 18000))"
    };

    for (const QString &sql : seedQueries) {
        if (!query.exec(sql)) {
            error = query.lastError().text();
            return false;
        }
    }
    return true;
}

QVector<Event> Database::loadEvents(QString *error) const
{
    QVector<Event> result;
    QSqlQuery query(connection);
    const QString sql = R"(
        SELECT e.id, e.title, e.kind, e.starts_at, e.hall_id, h.name, e.price_kopiyky
        FROM events e
        JOIN halls h ON h.id = e.hall_id
        ORDER BY e.starts_at ASC, e.id ASC
    )";

    if (!query.exec(sql)) {
        if (error)
            *error = query.lastError().text();
        return result;
    }

    while (query.next()) {
        Event ev;
        ev.id = query.value(0).toInt();
        ev.title = query.value(1).toString();
        ev.kind = query.value(2).toString();
        ev.startsAt = QDateTime::fromString(query.value(3).toString(), Qt::ISODate);
        ev.hallId = query.value(4).toInt();
        ev.hallName = query.value(5).toString();
        ev.priceKopiyky = query.value(6).toInt();
        result.push_back(ev);
    }
    return result;
}