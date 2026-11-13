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
            ON tickets(event_id, seat_id) WHERE status IN ('reserved','sold'))",
        R"(CREATE TABLE IF NOT EXISTS refunds (
            ticket_id INTEGER PRIMARY KEY REFERENCES tickets(id), refunded_at TEXT NOT NULL
        ))",
        R"(CREATE TRIGGER IF NOT EXISTS check_new_show_time BEFORE INSERT ON events
            WHEN EXISTS(SELECT 1 FROM events WHERE hall_id=NEW.hall_id AND event_date=NEW.event_date AND event_time=NEW.event_time)
            BEGIN SELECT RAISE(ABORT,'Зала вже зайнята на цей час'); END)",
        R"(CREATE TRIGGER IF NOT EXISTS check_changed_show_time BEFORE UPDATE OF hall_id,event_date,event_time ON events
            WHEN (OLD.hall_id<>NEW.hall_id OR OLD.event_date<>NEW.event_date OR OLD.event_time<>NEW.event_time)
            AND EXISTS(SELECT 1 FROM events WHERE id<>OLD.id AND hall_id=NEW.hall_id AND event_date=NEW.event_date AND event_time=NEW.event_time)
            BEGIN SELECT RAISE(ABORT,'Зала вже зайнята на цей час'); END)",
        R"(CREATE TRIGGER IF NOT EXISTS check_sale_price BEFORE INSERT ON tickets WHEN NEW.status='sold'
            AND NEW.paid_kopiyky IS NOT (SELECT CASE WHEN s.row_number=(SELECT MAX(row_number) FROM seats WHERE hall_id=e.hall_id)
                THEN e.vip_price_kopiyky ELSE e.price_kopiyky END FROM events e JOIN seats s ON s.hall_id=e.hall_id
                WHERE e.id=NEW.event_id AND s.id=NEW.seat_id)
            BEGIN SELECT RAISE(ABORT,'Тариф змінився. Оновіть схему зали'); END)",
        R"(CREATE TRIGGER IF NOT EXISTS check_buyout_price BEFORE UPDATE OF status ON tickets
            WHEN OLD.status='reserved' AND NEW.status='sold' AND NEW.paid_kopiyky IS NOT
                (SELECT CASE WHEN s.row_number=(SELECT MAX(row_number) FROM seats WHERE hall_id=e.hall_id)
                THEN e.vip_price_kopiyky ELSE e.price_kopiyky END FROM events e JOIN seats s ON s.hall_id=e.hall_id
                WHERE e.id=NEW.event_id AND s.id=NEW.seat_id)
            BEGIN SELECT RAISE(ABORT,'Тариф змінився. Оновіть схему зали'); END)",
        R"(CREATE TRIGGER IF NOT EXISTS record_ticket_refund AFTER UPDATE OF status ON tickets
            WHEN OLD.status='sold' AND NEW.status='cancelled'
            BEGIN INSERT INTO refunds(ticket_id,refunded_at) VALUES(NEW.id,CURRENT_TIMESTAMP); END)",
        R"(CREATE TRIGGER IF NOT EXISTS require_matching_hall BEFORE INSERT ON tickets
            WHEN NOT EXISTS (SELECT 1 FROM events e JOIN seats s ON s.hall_id=e.hall_id
                             WHERE e.id=NEW.event_id AND s.id=NEW.seat_id)
            BEGIN SELECT RAISE(ABORT,'Місце не належить залі показу'); END)",
        R"(CREATE TRIGGER IF NOT EXISTS protect_event_hall BEFORE UPDATE OF hall_id ON events
            WHEN NEW.hall_id<>OLD.hall_id AND EXISTS(SELECT 1 FROM tickets WHERE event_id=OLD.id)
            BEGIN SELECT RAISE(ABORT,'Показ має історію квитків'); END)"
    };

    for (const QString &statement : schema) {
        if (!query.exec(statement)) {
            error = query.lastError().text();
            connection.rollback();
            return false;
        }
    }

    if (!query.exec("SELECT 1 FROM pragma_table_info('events') WHERE name='finished'")) {
        error = query.lastError().text();
        connection.rollback();
        return false;
    }
    const bool hasFinished = query.next();
    query.finish();
    if (!hasFinished && !query.exec("ALTER TABLE events ADD COLUMN finished INTEGER NOT NULL DEFAULT 0 CHECK(finished IN (0,1))")) {
        error = query.lastError().text();
        connection.rollback();
        return false;
    }
    if (!query.exec("CREATE TABLE IF NOT EXISTS show_settings(event_id INTEGER PRIMARY KEY REFERENCES events(id) ON DELETE CASCADE, duration INTEGER NOT NULL CHECK(duration>0))")) {
        error=query.lastError().text(); connection.rollback(); return false;
    }
    query.exec("UPDATE tickets SET status='cancelled' WHERE status='reserved' AND event_id IN (SELECT id FROM events WHERE datetime(substr(event_date,7,4)||'-'||substr(event_date,4,2)||'-'||substr(event_date,1,2)||' '||event_time,'-30 minutes')<=datetime('now','localtime'))");
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
    query.prepare("SELECT COUNT(*) FROM tickets WHERE event_id = ?");
    query.addBindValue(eventId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}

bool Database::deleteEvent(int id, QString &error)
{
    QSqlQuery q(connection);
    q.prepare("DELETE FROM events WHERE id=? AND NOT EXISTS(SELECT 1 FROM tickets WHERE event_id=events.id)");
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() != 1) {
        error = q.lastError().isValid() ? q.lastError().text() : QStringLiteral("Показ уже видалено або він має історію операцій.");
        return false;
    }
    return true;
}

bool Database::addEvent(const Event &ev, QString &error)
{
    const auto start=QDateTime(QDate::fromString(ev.date,"dd.MM.yyyy"),QTime::fromString(ev.time,"HH:mm"));
    const auto end=start.addSecs(ev.durationMinutes*60);
    for(const auto &other:loadEvents()){
        if(other.id==ev.id || other.hallId!=ev.hallId)continue;
        const auto begin=QDateTime(QDate::fromString(other.date,"dd.MM.yyyy"),QTime::fromString(other.time,"HH:mm"));
        if(start<begin.addSecs(other.durationMinutes*60) && end>begin){
            error=QStringLiteral("Зала зайнята показом «%1».").arg(other.title);return false;
        }
    }
    if (!connection.transaction()) {error=connection.lastError().text();return false;}
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
        error = QStringLiteral("Не вдалося додати подію: ") + query.lastError().text(); connection.rollback();
        return false;
    }
    QSqlQuery settings(connection);
    settings.prepare("INSERT INTO show_settings(event_id,duration) VALUES(?,?)");
    settings.addBindValue(query.lastInsertId());settings.addBindValue(ev.durationMinutes);
    if(!settings.exec()){error=settings.lastError().text();connection.rollback();return false;}
    if(!connection.commit()){error=connection.lastError().text();connection.rollback();return false;}
    return true;
}

bool Database::updateEvent(const Event &ev, QString &error)
{
    const auto start=QDateTime(QDate::fromString(ev.date,"dd.MM.yyyy"),QTime::fromString(ev.time,"HH:mm"));
    const auto end=start.addSecs(ev.durationMinutes*60);
    for(const auto &other:loadEvents()){
        if(other.id==ev.id || other.hallId!=ev.hallId)continue;
        const auto begin=QDateTime(QDate::fromString(other.date,"dd.MM.yyyy"),QTime::fromString(other.time,"HH:mm"));
        if(start<begin.addSecs(other.durationMinutes*60) && end>begin){
            error=QStringLiteral("Зала зайнята показом «%1».").arg(other.title);return false;
        }
    }
    QSqlQuery query(connection);
    query.prepare(R"(
        UPDATE events
        SET kind = ?,
            title = ?,
            event_date = ?,
            event_time = ?,
            hall_id = ?,
            price_kopiyky = ?,
            vip_price_kopiyky = ?, finished = ?
        WHERE id = ?
    )");
    query.addBindValue(ev.kind.trimmed());
    query.addBindValue(ev.title.trimmed());
    query.addBindValue(ev.date.trimmed());
    query.addBindValue(ev.time.trimmed());
    query.addBindValue(ev.hallId);
    query.addBindValue(ev.priceKopiyky);
    query.addBindValue(ev.vipPriceKopiyky);
    query.addBindValue(ev.finished);
    query.addBindValue(ev.id);

    if (!query.exec()) {
        error = QStringLiteral("Не вдалося оновити подію: ") + query.lastError().text();
        return false;
    }
    QSqlQuery settings(connection);
    settings.prepare("INSERT INTO show_settings(event_id,duration) VALUES(?,?) ON CONFLICT(event_id) DO UPDATE SET duration=excluded.duration");
    settings.addBindValue(ev.id);settings.addBindValue(ev.durationMinutes);
    if(!settings.exec()){error=settings.lastError().text();return false;}
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
        SELECT e.id, e.kind, e.title, e.event_date, e.event_time, e.hall_id, h.name, e.price_kopiyky, e.vip_price_kopiyky, e.finished, COALESCE((SELECT duration FROM show_settings WHERE event_id=e.id),120)
        FROM events e
        JOIN halls h ON h.id = e.hall_id
        ORDER BY substr(e.event_date, 7, 4), substr(e.event_date, 4, 2), substr(e.event_date, 1, 2), e.event_time, e.id
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
        ev.finished = query.value(9).toBool();
        ev.durationMinutes = query.value(10).toInt();
        result.push_back(ev);
    }
    return result;
}

// ============================================================================
// 6. ЗАВАНТАЖЕННЯ МІСЦЬ ДЛЯ ПОДІЇ (ВКЛЮЧАЮЧИ ДАНІ ПРОДАНОГО КВИТКА ТА CHECK-IN)
// ============================================================================
QVector<Seat> Database::loadSeatsForEvent(int eventId, QString *error) const
{
    QVector<Seat> result;
    QSqlQuery query(connection);

    const QString sql = R"(
        SELECT s.id, s.hall_id, s.row_number, s.seat_number,
               t.status, t.id, t.reservation_name, t.code, t.paid_kopiyky, t.sold_at, t.checked_in_at
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
        seat.checkedInAt = query.value(10).toString();

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

bool Database::bookSeats(int eventId, const QMap<int,int> &prices, const QString &name,
                         QStringList &codes, QString &error)
{
    codes.clear();
    if (prices.isEmpty() || !connection.transaction()) {
        error = QStringLiteral("Не вдалося почати оформлення.");
        return false;
    }
    QStringList issued;
    for (auto it = prices.cbegin(); it != prices.cend(); ++it) {
        QString code;
        const bool ok = name.isEmpty()
            ? sellDirectSeat(eventId, it.key(), it.value(), code, error)
            : reserveSeat(eventId, it.key(), name, error);
        if (!ok) {
            connection.rollback();
            return false;
        }
        if (!code.isEmpty())
            issued.append(code);
    }
    if (!connection.commit()) {
        error = connection.lastError().text();
        connection.rollback();
        return false;
    }
    codes = issued;
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
          AND EXISTS(SELECT 1 FROM events e WHERE e.id=tickets.event_id AND e.finished=0
            AND datetime(substr(e.event_date,7,4)||'-'||substr(e.event_date,4,2)||'-'||substr(e.event_date,1,2)||' '||e.event_time,'-30 minutes')>datetime('now','localtime'))
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

// ============================================================================
// 9. КОНТРОЛЬ ВХОДУ ЗА КОДОМ КВИТКА (ЕТАП 6: CHECK-IN)
// ============================================================================
CheckInResult Database::checkInTicket(int eventId, const QString &rawCode)
{
    const QString code = rawCode.trimmed().toUpper();
    if (code.isEmpty()) {
        return {false, QStringLiteral("Введіть код квитка для перевірки.")};
    }

    QSqlQuery query(connection);
    query.prepare(R"(
        SELECT t.id, t.event_id, t.status, t.checked_in_at, s.row_number, s.seat_number, e.title
        FROM tickets t
        JOIN seats s ON s.id = t.seat_id
        JOIN events e ON e.id = t.event_id
        WHERE UPPER(t.code) = ?
    )");
    query.addBindValue(code);

    if (!query.exec()) {
        return {false, QStringLiteral("Помилка бази даних: ") + query.lastError().text()};
    }

    if (!query.next()) {
        return {false, QStringLiteral("Квиток із кодом «%1» не знайдено в системі!").arg(code)};
    }

    const int ticketId = query.value(0).toInt();
    const int ticketEventId = query.value(1).toInt();
    const QString status = query.value(2).toString();
    const QString checkedInAt = query.value(3).toString();
    const int rowNum = query.value(4).toInt();
    const int seatNum = query.value(5).toInt();
    const QString eventTitle = query.value(6).toString();

    if (ticketEventId != eventId) {
        return {false, QStringLiteral("УВАГА! Квиток «%1» придбано на іншу подію:\n«%2» (Ряд %3, Місце %4).")
                           .arg(code, eventTitle).arg(rowNum).arg(seatNum)};
    }

    if (status == QLatin1String("reserved")) {
        return {false, QStringLiteral("Квиток «%1» (Ряд %2, Місце %3) лише ЗАБРОНЬОВАНИЙ, але ще не оплачений!")
                           .arg(code).arg(rowNum).arg(seatNum)};
    }

    if (status != QLatin1String("sold")) {
        return {false, QStringLiteral("Квиток «%1» недійсний (статус: %2).").arg(code, status)};
    }

    if (!checkedInAt.isEmpty()) {
        return {false, QStringLiteral("ПОВТОРНИЙ ВХІД ЗАБОРОНЕНО!\n\n"
                                      "Квиток «%1» (Ряд %2, Місце %3) вже був використаний:\n%4")
                           .arg(code).arg(rowNum).arg(seatNum).arg(checkedInAt)};
    }

    const QString nowStr = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    QSqlQuery updateQuery(connection);
    updateQuery.prepare(R"(
        UPDATE tickets
        SET checked_in_at = ?
        WHERE id = ? AND status = 'sold' AND checked_in_at IS NULL
    )");
    updateQuery.addBindValue(nowStr);
    updateQuery.addBindValue(ticketId);

    if (!updateQuery.exec() || updateQuery.numRowsAffected() == 0) {
        return {false, QStringLiteral("Не вдалося зафіксувати вхід у базі даних.")};
    }

    return {true, QStringLiteral("ПРОХІД ДОЗВОЛЕНО!\n\n"
                                 "Код: %1\n"
                                 "Місце: Ряд %2, Місце %3\n"
                                 "Час контролю: %4")
                      .arg(code).arg(rowNum).arg(seatNum).arg(nowStr)};
}