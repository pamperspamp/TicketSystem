#pragma once

#include "event.h"
#include "seat.h"
#include <QPair>
#include <QMap>
#include <QStringList>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct CheckInResult
{
    bool success = false;
    QString message;
};

class Database
{
public:
    bool open(QString &error);
    QVector<Event> loadEvents(QString *error = nullptr) const;
    QVector<QPair<int, QString>> loadHalls(QString *error = nullptr) const;
    QVector<Seat> loadSeatsForEvent(int eventId, QString *error = nullptr) const;
    QString getEventKind(int eventId) const;
    bool getEventPrices(int eventId, int &regularPriceKopiyky, int &vipPriceKopiyky) const;

    // Адміністрування афіші (створення та редагування подій)
    bool deleteEvent(int id, QString &error);
    bool addEvent(const Event &ev, QString &error);
    bool updateEvent(const Event &ev, QString &error);
    bool hasActiveTicketsForEvent(int eventId) const;

    // Етап 4: Бронювання та скасування броні
    bool reserveSeat(int eventId, int seatId, const QString &customerName, QString &error);
    bool bookSeats(int eventId, const QMap<int,int> &prices, const QString &name,
                   QStringList &codes, QString &error);
    bool cancelReservation(int ticketId, QString &error);

    // Етап 5: Прямий продаж та викуп броні з генерацією коду квитка
    bool sellDirectSeat(int eventId, int seatId, int priceKopiyky, QString &generatedCode, QString &error);
    bool sellReservedTicket(int ticketId, int priceKopiyky, QString &generatedCode, QString &error);

    // Етап 6: Контроль входу (Check-in) за кодом квитка
    CheckInResult checkInTicket(int eventId, const QString &rawCode);

private:
    bool seedIfEmpty(QSqlQuery &query, QString &error);
    QString generateTicketCode(int prefixId, int secondaryId) const;
    QSqlDatabase connection;
};