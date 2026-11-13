#include "database.h"

#include <QDateTime>
#include <QRandomGenerator>
#include <QSqlError>
#include <QSqlQuery>

// ============================================================================
// 1. ВІДКРИТТЯ БАЗИ ДАНИХ (ОДНА ПОСТІЙНА БД БЕЗ ПЕРЕСТВОРЕННЯ ТАБЛИЦЬ)
// ============================================================================
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
            id INTEGER PRIMARY KEY,
            name TEXT NOT NULL UNIQUE
        ))",
        R"(CREATE TABLE IF NOT EXISTS seats (
            id INTEGER PRIMARY KEY,
            hall_id INTEGER NOT NULL REFERENCES halls(id),
            row_number INTEGER NOT NULL CHECK(row_number > 0),
            seat_number INTEGER NOT NULL CHECK(seat_number > 0),
            UNIQUE(hall_id, row_number, seat_number)
        ))",
        R"(CREATE TABLE IF NOT EXISTS events (
            id INTEGER PRIMARY KEY,
            kind TEXT NOT NULL,
            title TEXT NOT NULL,
            event_date TEXT NOT NULL,
            event_time TEXT NOT NULL,
            hall_id INTEGER NOT NULL REFERENCES halls(id),
            price_kopiyky INTEGER NOT NULL CHECK(price_kopiyky >= 0),
            vip_price_kopiyky INTEGER NOT NULL CHECK(vip_price_kopiyky >= 0)
        ))",
        R"(CREATE TABLE IF NOT EXISTS tickets (
            id INTEGER PRIMARY KEY,
            event_id INTEGER NOT NULL REFERENCES events(id),
            seat_id INTEGER NOT NULL REFERENCES seats(id),
            status TEXT NOT NULL CHECK(status IN ('reserved','sold','cancelled')),
            reservation_name TEXT,
            code TEXT UNIQUE,
            paid_kopiyky INTEGER,
            sold_at TEXT,
            checked_in_at TEXT
        ))",
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

// ============================================================================
// 2. ПОЧАТКОВЕ НАПОВНЕННЯ БАЗИ (Спрацьовує лише раз на порожній БД)
// ============================================================================
bool Database::seedIfEmpty(QSqlQuery &query, QString &error)
{
    if (!query.exec("SELECT COUNT(*) FROM halls")) {
        error = query.lastError().text();
        return false;
    }
    const int hallsCount = query.next() ? query.value(0).toInt() : 0;

    if (hallsCount == 0) {
        const QStringList seedQueries = {
            R"(INSERT INTO halls (id, name) VALUES
                (1, 'Велика'), (2, 'Камерна'), (3, 'Кінозала'))",
            R"(INSERT INTO events (id, kind, title, event_date, event_time, hall_id, price_kopiyky, vip_price_kopiyky) VALUES
                (1, 'Вистава', 'Лісова пісня', '20.11.2026', '18:00', 1, 25000, 40000),
                (2, 'Концерт', 'Джазовий вечір', '21.11.2026', '19:30', 2, 32000, 50000),
                (3, 'Кіно', 'Вечір кіно', '22.11.2026', '17:00', 3, 18000, 30000))"
        };

        for (const QString &sql : seedQueries) {
            if (!query.exec(sql)) {
                error = query.lastError().text();
                return false;
            }
        }
    }

    if (!query.exec("SELECT COUNT(*) FROM seats")) {
        error = query.lastError().text();
        return false;
    }
    const int seatsCount = query.next() ? query.value(0).toInt() : 0;

    if (seatsCount == 0) {
        struct HallConfig { int hallId; int rows; int cols; };
        const HallConfig configs[] = {
            {1, 10, 18},
            {2, 8, 14},
            {3, 12, 20}
        };

        query.prepare("INSERT OR IGNORE INTO seats (hall_id, row_number, seat_number) VALUES (?, ?, ?)");

        for (const auto &cfg : configs) {
            for (int r = 1; r <= cfg.rows; ++r) {
                int seatsInRow = cfg.cols;

                if (cfg.hallId == 3) {
                    if (r == 1)
                        seatsInRow = 18;
                    else if (r == 12)
                        seatsInRow = 14;
                }

                for (int s = 1; s <= seatsInRow; ++s) {
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

// ============================================================================
// 3. АДМІНІСТРУВАННЯ ПОДІЙ (СПИСОК ЗАЛІВ, СТВОРЕННЯ ТА РЕДАГУВАННЯ ІВЕНТІВ)
// ============================================================================
QVector<QPair<int, QString>> Database::loadHalls(QString *error) const
{
    QVector<QPair<int, QString>> halls;
    QSqlQuery query(connection);

    if (!query.exec("SELECT id, name FROM halls ORDER BY id ASC")) {
        if (error)
            *error = query.lastError().text();
        return halls;
    }

    while (query.next()) {
        halls.push_back({query.value(0).toInt(), query.value(1).toString()});
    }
    return halls;
}

bool Database::hasActiveTicketsForEvent(int eventId) const
{
    QSqlQuery query(connection);
    query.prepare("SELECT COUNT(*) FROM tickets WHERE event_id = ? AND status IN ('reserved', 'sold')");
    query.addBindValue(eventId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

bool Database::addEvent(const Event &ev, QString &error)
{
    QSqlQuery query(connection);
    query.prepare(R"(
        INSERT INTO events (kind, title, event_date, event_time, hall_id, price_kopiyky, vip_price_kopiyky)
        VALUES (?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(ev.kind.trimmed());
    query.addBindValue(ev.title.trimmed());
    query.addBindValue(ev.date.trimmed());
    query.addBindValue(ev.time.trimmed());
    query.addBindValue(ev.hallId);
    query.addBindValue(ev.priceKopiyky);
    query.addBindValue(ev.vipPriceKopiyky);

    if (!query.exec()) {
        error = QStringLiteral("Не вдалося додати подію: ") + query.lastError().text();
        return false;
    }
    return true;
}

bool Database::updateEvent(const Event &ev, QString &error)
{
    QSqlQuery query(connection);
    query.prepare(R"(
        UPDATE events
        SET kind = ?,
            title = ?,
            event_date = ?,
            event_time = ?,
            hall_id = ?,
            price_kopiyky = ?,
            vip_price_kopiyky = ?
        WHERE id = ?
    )");
    query.addBindValue(ev.kind.trimmed());
    query.addBindValue(ev.title.trimmed());
    query.addBindValue(ev.date.trimmed());
    query.addBindValue(ev.time.trimmed());
    query.addBindValue(ev.hallId);
    query.addBindValue(ev.priceKopiyky);
    query.addBindValue(ev.vipPriceKopiyky);
    query.addBindValue(ev.id);

    if (!query.exec()) {
        error = QStringLiteral("Не вдалося оновити подію: ") + query.lastError().text();
        return false;
    }
    return true;
}

// ============================================================================
// 4. ДОПОМІЖНІ ЗАПИТИ ДЛЯ ПОДІЇ (ТИП ТА ЦІНИ)
// ============================================================================
QString Database::getEventKind(int eventId) const
{
    QSqlQuery query(connection);
    query.prepare("SELECT kind FROM events WHERE id = ?");
    query.addBindValue(eventId);

    if (query.exec() && query.next()) {
        return query.value(0).toString();
    }
    return QString();
}

bool Database::getEventPrices(int eventId, int &regularPriceKopiyky, int &vipPriceKopiyky) const
{
    QSqlQuery query(connection);
    query.prepare("SELECT price_kopiyky, vip_price_kopiyky FROM events WHERE id = ?");
    query.addBindValue(eventId);

    if (query.exec() && query.next()) {
        regularPriceKopiyky = query.value(0).toInt();
        vipPriceKopiyky = query.value(1).toInt();
        return true;
    }
    return false;
}

// ============================================================================
// 5. ЗАВАНТАЖЕННЯ АФІШІ ПОДІЙ
// ============================================================================
QVector<Event> Database::loadEvents(QString *error) const
{
    QVector<Event> result;
    QSqlQuery query(connection);

    const QString sql = R"(
        SELECT e.id, e.kind, e.title, e.event_date, e.event_time, e.hall_id, h.name, e.price_kopiyky, e.vip_price_kopiyky
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
        ev.vipPriceKopiyky = query.value(8).toInt();
        result.push_back(ev);
    }
    return result;
}

// ============================================================================
// 6. ЗАВАНТАЖЕННЯ МІСЦЬ ДЛЯ ПОДІЇ (ВКЛЮЧАЮЧИ ДАНІ ПРОДАНОГО КВИТКА)
// ============================================================================
QVector<Seat> Database::loadSeatsForEvent(int eventId, QString *error) const
{
    QVector<Seat> result;
    QSqlQuery query(connection);

    const QString sql = R"(
        SELECT s.id, s.hall_id, s.row_number, s.seat_number,
               t.status, t.id, t.reservation_name, t.code, t.paid_kopiyky, t.sold_at
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
            seat.status = SeatStatus::Free;

        seat.ticketId = query.value(5).toInt();
        seat.reservationName = query.value(6).toString();
        seat.ticketCode = query.value(7).toString();
        seat.paidKopiyky = query.value(8).toInt();
        seat.soldAt = query.value(9).toString();

        result.push_back(seat);
    }
    return result;
}

// ============================================================================
// 7. ОФОРМЛЕННЯ БРОНЮВАННЯ ТА СКАСУВАННЯ БРОНІ (ЕТАП 4)
// ============================================================================
bool Database::reserveSeat(int eventId, int seatId, const QString &customerName, QString &error)
{
    QSqlQuery query(connection);
    query.prepare(R"(
        INSERT INTO tickets (event_id, seat_id, status, reservation_name)
        VALUES (?, ?, 'reserved', ?)
    )");
    query.addBindValue(eventId);
    query.addBindValue(seatId);
    query.addBindValue(customerName.trimmed());

    if (!query.exec()) {
        error = QStringLiteral("Не вдалося забронювати місце: ") + query.lastError().text();
        return false;
    }
    return true;
}

bool Database::cancelReservation(int ticketId, QString &error)
{
    QSqlQuery query(connection);
    query.prepare(R"(
        UPDATE tickets
        SET status = 'cancelled'
        WHERE id = ? AND status = 'reserved'
    )");
    query.addBindValue(ticketId);

    if (!query.exec()) {
        error = query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() == 0) {
        error = QStringLiteral("Бронь уже скасована або квиток уже викуплений.");
        return false;
    }
    return true;
}

// ============================================================================
// 8. ГЕНЕРАЦІЯ УНІКАЛЬНОГО КОДУ КВИТКА ТА ПРОДАЖ (ЕТАП 5)
// ============================================================================
QString Database::generateTicketCode(int prefixId, int secondaryId) const
{
    const quint32 randPart = QRandomGenerator::global()->bounded(0x1000, 0xFFFF);
    return QStringLiteral("TKT-%1-%2-%3")
        .arg(prefixId)
        .arg(secondaryId)
        .arg(QString::number(randPart, 16).toUpper());
}

bool Database::sellDirectSeat(int eventId, int seatId, int priceKopiyky, QString &generatedCode, QString &error)
{
    generatedCode = generateTicketCode(eventId, seatId);
    const QString soldAt = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));

    QSqlQuery query(connection);
    query.prepare(R"(
        INSERT INTO tickets (event_id, seat_id, status, code, paid_kopiyky, sold_at)
        VALUES (?, ?, 'sold', ?, ?, ?)
    )");
    query.addBindValue(eventId);
    query.addBindValue(seatId);
    query.addBindValue(generatedCode);
    query.addBindValue(priceKopiyky);
    query.addBindValue(soldAt);

    if (!query.exec()) {
        error = QStringLiteral("Не вдалося продати квиток: ") + query.lastError().text();
        return false;
    }
    return true;
}

bool Database::sellReservedTicket(int ticketId, int priceKopiyky, QString &generatedCode, QString &error)
{
    generatedCode = generateTicketCode(ticketId, QRandomGenerator::global()->bounded(10, 99));
    const QString soldAt = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));

    QSqlQuery query(connection);
    query.prepare(R"(
        UPDATE tickets
        SET status = 'sold',
            code = ?,
            paid_kopiyky = ?,
            sold_at = ?
        WHERE id = ? AND status = 'reserved'
    )");
    query.addBindValue(generatedCode);
    query.addBindValue(priceKopiyky);
    query.addBindValue(soldAt);
    query.addBindValue(ticketId);

    if (!query.exec()) {
        error = QStringLiteral("Помилка при викупі броні: ") + query.lastError().text();
        return false;
    }
    if (query.numRowsAffected() == 0) {
        error = QStringLiteral("Не вдалося викупити квиток: бронь уже скасована або викуплена.");
        return false;
    }
    return true;
}