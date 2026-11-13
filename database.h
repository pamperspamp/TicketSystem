#pragma once

#include "event.h"
#include "seat.h"
#include <QSqlDatabase>
#include <QString>
#include <QVector>

class Database
{
public:
    bool open(QString &error);
    QVector<Event> loadEvents(QString *error = nullptr) const;
    QVector<Seat> loadSeatsForEvent(int eventId, QString *error = nullptr) const;
    QString getEventKind(int eventId) const;
    bool getEventPrices(int eventId, int &regularPriceKopiyky, int &vipPriceKopiyky) const;

    bool reserveSeat(int eventId, int seatId, const QString &customerName, QString &error);
    bool cancelReservation(int ticketId, QString &error);

private:
    bool seedIfEmpty(QSqlQuery &query, QString &error);
    QSqlDatabase connection;
};