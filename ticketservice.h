#pragma once
#include <QString>
#include <QDate>

// Операції з уже оформленими квитками.
class TicketService
{
public:
    bool refund(int ticketId, QString &error) const;
    QString writePdf(const QString &code, QString &error) const;
    QString reportHtml(QString &error, QDate day = {}) const;
};