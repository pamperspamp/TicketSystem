#include "ticketservice.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QPainter>
#include "third_party/qrcodegen/qrcodegen.hpp"
#include <QVariant>

namespace {
QString money(qint64 kop)
{
    return (kop < 0 ? "-" : "") + QString::number(qAbs(kop / 100)) + "," + QString::number(qAbs(kop % 100)).rightJustified(2, '0');
}
}

bool TicketService::refund(int ticketId, QString &error) const
{
    // Один UPDATE звільняє місце й зберігає первісну оплату для звіту.
    QSqlQuery q;
    q.prepare("UPDATE tickets SET status='cancelled' WHERE id=? AND status='sold' AND checked_in_at IS NULL");
    q.addBindValue(ticketId);
    if (!q.exec()) {
        error = q.lastError().text();
        return false;
    }
    if (q.numRowsAffected() != 1) {
        error = QStringLiteral("Повернення неможливе: квиток уже використано або продаж скасовано.");
        return false;
    }
    return true;
}

QString TicketService::writePdf(const QString &code, QString &error) const
{
    QSqlQuery q;
    q.prepare("SELECT e.title,e.event_date,e.event_time,h.name,s.row_number,s.seat_number,"
              "t.paid_kopiyky,t.sold_at,t.id FROM tickets t JOIN events e ON e.id=t.event_id "
              "JOIN halls h ON h.id=e.hall_id JOIN seats s ON s.id=t.seat_id WHERE t.code=? AND t.status='sold'");
    q.addBindValue(code);
    if (!q.exec() || !q.next()) {
        error = q.lastError().isValid() ? q.lastError().text() : QStringLiteral("Чинний квиток не знайдено.");
        return {};
    }
    QDir dir(QFileInfo(QSqlDatabase::database().databaseName()).absolutePath());
    if (!dir.mkpath("tickets")) {
        error = QStringLiteral("Не вдалося створити папку tickets.");
        return {};
    }
    QSaveFile file(dir.filePath(QString("tickets/ticket-%1.pdf").arg(q.value(8).toLongLong())));
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return {};
    }
    {
        QPdfWriter pdf(&file);
        pdf.setPageSize(QPageSize(QPageSize::A5));
        pdf.setPageMargins(QMarginsF(9, 9, 9, 9), QPageLayout::Millimeter);
        pdf.setResolution(300);
        pdf.setTitle(QStringLiteral("Зала подій «Обрій» · Квиток ") + code);
        pdf.setCreator(QStringLiteral("Квиткова каса · Зала подій «Обрій»"));
        QPainter painter(&pdf);
        if (!painter.isActive()) {
            error = QStringLiteral("Не вдалося розпочати друк квитка.");
            return {};
        }
        painter.setWindow(0, 0, 650, 960);
        const QColor ink("#173845"), gold("#89612f");
        auto label = [&](QRect box, const QString &value, int size, bool bold = false,
                         QColor color = QColor("#202330"), QString family = "Segoe UI") {
            QFont font(family);
            font.setBold(bold);
            font.setPixelSize(size);
            painter.setFont(font);
            const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap;
            while (size > 10 && painter.boundingRect(box, flags, value).height() > box.height()) {
                font.setPixelSize(--size);
                painter.setFont(font);
            }
            painter.setPen(color);
            painter.drawText(box, flags, value);
        };
        painter.fillRect(0, 0, 650, 122, ink);
        painter.fillRect(0, 0, 8, 122, QColor("#e8bc73"));
        painter.setPen(QPen(QColor("#75a8af"), 2));
        painter.drawArc(QRect(468, 28, 152, 152), 0, 180 * 16);
        painter.drawLine(451, 105, 631, 105);
        painter.fillRect(0, 671, 650, 226, QColor("#eff4f3"));
        label({28, 12, 600, 30}, QStringLiteral("КУЛЬТУРНИЙ ПРОСТІР"), 14, true, QColor("#edd2b2"));
        label({26, 42, 430, 67}, QStringLiteral("Обрій"), 50, false, Qt::white, "Georgia");
        const QString serial = QString("OB %1").arg(q.value(8).toLongLong(), 6, 10, QChar('0'));
        label({4, 143, 400, 34}, QStringLiteral("ВХІДНИЙ КВИТОК"), 16, true, gold);
        label({438, 143, 205, 34}, serial, 18, true);
        label({2, 182, 642, 121}, q.value(0).toString(), 39, true, ink, "Georgia");
        painter.fillRect(2, 316, 645, 2, ink);
        auto field = [&](int x, int y, int width, const QString &name, const QString &value) {
            label({x, y, width, 25}, name, 14, true, gold);
            label({x, y + 26, width, 53}, value, 26, true);
        };
        field(4, 336, 350, QStringLiteral("ДАТА ПОДІЇ"), q.value(1).toString());
        field(403, 336, 238, QStringLiteral("ПОЧАТОК"), q.value(2).toString());
        field(4, 448, 285, QStringLiteral("ЗАЛА"), q.value(3).toString());
        field(310, 448, 125, QStringLiteral("РЯД"), q.value(4).toString());
        field(465, 448, 175, QStringLiteral("МІСЦЕ"), q.value(5).toString());
        painter.fillRect(2, 548, 645, 1, gold);
        field(4, 565, 300, QStringLiteral("ВАРТІСТЬ КВИТКА"), money(q.value(6).toLongLong()) + " грн");
        label({325, 565, 315, 25}, QStringLiteral("ДАТА ПРОДАЖУ"), 14, true, gold);
        label({325, 593, 315, 50}, q.value(7).toString(), 19);
        const auto qr = qrcodegen::QrCode::encodeText(code.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int module = 202 / (qr.getSize() + 8);
        painter.fillRect(2, 682, 205, 205, Qt::white);
        for (int y = 0; y < qr.getSize(); ++y)
            for (int x = 0; x < qr.getSize(); ++x)
                if (qr.getModule(x, y))
                    painter.fillRect(2 + (4+x)*module, 682 + (4+y)*module, module, module, Qt::black);
        label({226, 689, 410, 34}, QStringLiteral("КОНТРОЛЬ ВХОДУ"), 15, true, gold);
        label({226, 732, 410, 55}, code, 23, true);
        label({226, 803, 410, 65}, QStringLiteral("Пред’явіть QR-код при вході. Один квиток — одне місце."), 17);
        painter.fillRect(2, 907, 645, 1, ink);
        label({4, 916, 630, 39}, QStringLiteral("Навчальний зразок  ·  Електронний квиток"), 14);
    }
    if (file.size() == 0 || !file.commit()) {
        error = QStringLiteral("Не вдалося записати PDF. ") + file.errorString();
        return {};
    }
    return file.fileName();
}

QString TicketService::reportHtml(QString &error, QDate day) const
{
    QSqlQuery q;
    error.clear();
    const QString heading = QStringLiteral("<html><body style='font-family:Segoe UI;color:#202d35'><table width='100%' cellpadding='12' bgcolor='#203d48'><tr><td style='color:white;font-size:19pt'><b>ОБРІЙ</b> · Каса подій</td></tr></table>");
    q.prepare(R"(SELECT e.title,e.event_date,e.event_time,h.name,
        (SELECT COUNT(*) FROM seats s WHERE s.hall_id=e.hall_id),
        COUNT(CASE WHEN t.status='reserved' THEN 1 END),
        COUNT(CASE WHEN t.status='sold' THEN 1 END),
        COUNT(CASE WHEN t.checked_in_at IS NOT NULL THEN 1 END),
        COUNT(CASE WHEN t.status='cancelled' AND t.sold_at IS NOT NULL THEN 1 END),
        COALESCE(SUM(CASE WHEN t.sold_at IS NOT NULL THEN t.paid_kopiyky END),0),
        COALESCE(SUM(CASE WHEN t.status='cancelled' AND t.sold_at IS NOT NULL THEN t.paid_kopiyky END),0)
        FROM events e JOIN halls h ON h.id=e.hall_id LEFT JOIN tickets t ON t.event_id=e.id
        WHERE (:day IS NULL OR e.event_date = :day)
        GROUP BY e.id ORDER BY substr(e.event_date,7,4),substr(e.event_date,4,2),substr(e.event_date,1,2),
                               e.event_time,h.name,e.id)");
    q.bindValue(":day", day.isValid() ? day.toString("dd.MM.yyyy") : QString());
    if (!q.exec()) {
        error = q.lastError().text();
        return {};
    }
    QString html = heading + QStringLiteral("<h1>Звіт за показами</h1><p>%1 · Стан на %2</p>")
        .arg(day.isValid() ? QStringLiteral("Дата показу: <b>%1</b>").arg(day.toString("dd.MM.yyyy"))
                           : QStringLiteral("Усі дати показів"),
             QDateTime::currentDateTime().toString("dd.MM.yyyy HH:mm"));
    int shows = 0, activeTickets = 0, admissions = 0;
    qint64 totalGross=0, totalRefund=0;
    while (q.next()) {
        const int capacity=q.value(4).toInt(), booked=q.value(5).toInt(), sold=q.value(6).toInt();
        const qint64 gross=q.value(9).toLongLong(), refunded=q.value(10).toLongLong();
        ++shows;
        activeTickets += sold;
        admissions += q.value(7).toInt();
        totalGross += gross;
        totalRefund += refunded;
        html += QStringLiteral("<hr><h2>%1</h2><p>%2 о %3 · %4</p>")
            .arg(q.value(0).toString().toHtmlEscaped(), q.value(1).toString().toHtmlEscaped(),
                 q.value(2).toString().toHtmlEscaped(),q.value(3).toString().toHtmlEscaped());
        html += QStringLiteral("<p>Місць: %1 · Вільно: %2 · Броней: %3 · Продано (чинні): %4 · "
            "У залі: %5 · Заповненість: %6</p>")
            .arg(capacity).arg(capacity-booked-sold).arg(booked).arg(sold).arg(q.value(7).toInt())
            .arg(capacity ? QString::number(100.0*(booked+sold)/capacity,'f',1)+"%" : QStringLiteral("немає місць"));
        html += QStringLiteral("<p>Надходження: %1 · Повернення (%2): %3 · Чиста виручка: <b>%4</b></p>")
            .arg(money(gross)).arg(q.value(8).toInt()).arg(money(refunded),money(gross-refunded));
    }
    if (shows == 0)
        return html + QStringLiteral("<p>На вибрану дату показів немає.</p></body></html>");
    html += QStringLiteral("<hr><h2>Разом</h2><p>Показів: %1 · Чинних квитків: %2 · Проходів: %3</p>")
        .arg(shows).arg(activeTickets).arg(admissions);
    return html + QStringLiteral("<p>Надходження: %1 · Повернення: %2 · Чиста виручка: %3 грн</p>")
        .arg(money(totalGross),money(totalRefund),money(totalGross-totalRefund));
}
