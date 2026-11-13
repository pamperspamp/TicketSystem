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

    // Перевіряємо, чи не залишилася стара структура таблиці events
    if (query.exec("PRAGMA table_info(events)")) {
        bool hasEventDate = false;
        bool hasAnyColumns = false;
        while (query.next()) {
            hasAnyColumns = true;
            if (query.value(1).toString() == QLatin1String("event_date"))
                hasEventDate = true;
        }
        if (hasAnyColumns && !hasEventDate) {
            query.exec("DROP TABLE IF EXISTS tickets");
            query.exec("DROP TABLE IF EXISTS events");
        }
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
            id INTEGER PRIMARY KEY,
            kind TEXT NOT NULL,
            title TEXT NOT NULL,
            event_date TEXT NOT NULL,
            event_time TEXT NOT NULL,
            hall_id INTEGER NOT NULL REFERENCES halls(id),
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
    const int eventsCount = query.next() ? query.value(0).toInt() : 0;

    if (eventsCount == 0) {
        const QStringList seedQueries = {
            R"(INSERT OR IGNORE INTO halls (id, name) VALUES
                (1, 'Велика'), (2, 'Камерна'), (3, 'Кінозала'))",
            R"(INSERT INTO events (id, kind, title, event_date, event_time, hall_id, price_kopiyky) VALUES
                (1, 'Вистава', 'Лісова пісня', '20.11.2026', '18:00', 1, 25000),
                (2, 'Концерт', 'Джазовий вечір', '21.11.2026', '19:30', 2, 32000),
                (3, 'Кіно', 'Вечір кіно', '22.11.2026', '17:00', 3, 18000))"
        };

        for (const QString &sql : seedQueries) {
            if (!query.exec(sql)) {
                error = query.lastError().text();
                return false;
            }
        }
    }

    // Окремо перевіряємо, чи заповнена таблиця місць (seats)
    if (!query.exec("SELECT COUNT(*) FROM seats")) {
        error = query.lastError().text();
        return false;
    }
    const int seatsCount = query.next() ? query.value(0).toInt() : 0;

    if (seatsCount == 0) {
        struct HallConfig { int hallId; int rows; int cols; };
        const HallConfig configs[] = {
            {1, 5, 8}, // Велика: 5 рядів по 8 місць
            {2, 4, 6}, // Камерна: 4 ряди по 6 місць
            {3, 5, 7}  // Кінозала: 5 рядів по 7 місць
        };

        query.prepare("INSERT OR IGNORE INTO seats (hall_id, row_number, seat_number) VALUES (?, ?, ?)");
        for (const auto &cfg : configs) {
            for (int r = 1; r <= cfg.rows; ++r) {
                for (int s = 1; s <= cfg.cols; ++s) {
                    query.addBindValue(cfg.hallId);
                    query.addBindValue(r);
                    query.addBindValue(s);
                    if (!query.exec()) {
                        error = query.lastError().text();
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

QVector<Seat> Database::loadSeatsForEvent(int eventId, QString *error) const
{
    QVector<Seat> result;
    QSqlQuery query(connection);

    // Цей SQL-запит з'єднує 3 таблиці (це люблять питати на захисті!):
    // 1. Беремо всі місця (seats) тієї зали, де проходить подія (events).
    // 2. Через LEFT JOIN підклеюємо активні квитки (tickets), якщо місце вже заброньоване або продане.
    //    Саме LEFT JOIN гарантує, що вільні місця (де квитка ще немає, t.status IS NULL) теж потраплять у список!
    const QString sql = R"(
        SELECT s.id, s.hall_id, s.row_number, s.seat_number, t.status
        FROM seats s
        JOIN events e ON e.hall_id = s.hall_id
        LEFT JOIN tickets t ON t.seat_id = s.id
            AND t.event_id = e.id
            AND t.status IN ('reserved', 'sold')
        WHERE e.id = ?
        ORDER BY s.row_number ASC, s.seat_number ASC
    )";

    query.prepare(sql);
    query.addBindValue(eventId);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return result;
    }

    while (query.next()) {
        Seat seat;
        seat.id = query.value(0).toInt();
        seat.hallId = query.value(1).toInt();
        seat.rowNumber = query.value(2).toInt();
        seat.seatNumber = query.value(3).toInt();

        const QString statusStr = query.value(4).toString();
        if (statusStr == QLatin1String("reserved"))
            seat.status = SeatStatus::Reserved;
        else if (statusStr == QLatin1String("sold"))
            seat.status = SeatStatus::Sold;
        else
            seat.status = SeatStatus::Free; // Якщо в tickets порожньо — місце вільне

        result.push_back(seat);
    }
    return result;
}

QVector<Event> Database::loadEvents(QString *error) const
{
    QVector<Event> result;
    QSqlQuery query(connection);
    const QString sql = R"(
        SELECT e.id, e.kind, e.title, e.event_date, e.event_time, e.hall_id, h.name, e.price_kopiyky
        FROM events e
        JOIN halls h ON h.id = e.hall_id
        ORDER BY e.id ASC
    )";

    if (!query.exec(sql)) {
        if (error)
            *error = query.lastError().text();
        return result;
    }

    while (query.next()) {
        Event ev;
        ev.id = query.value(0).toInt();
        ev.kind = query.value(1).toString();
        ev.title = query.value(2).toString();
        ev.date = query.value(3).toString();
        ev.time = query.value(4).toString();
        ev.hallId = query.value(5).toInt();
        ev.hallName = query.value(6).toString();
        ev.priceKopiyky = query.value(7).toInt();
        result.push_back(ev);
    }
    return result;
}