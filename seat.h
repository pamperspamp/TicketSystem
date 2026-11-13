#pragma once

#include <QString>

enum class SeatStatus
{
    Free,      // Вільне
    Reserved,  // Заброньоване
    Sold       // Продане
};

struct Seat
{
    int id = 0;
    int hallId = 0;
    int rowNumber = 0;
    int seatNumber = 0;
    SeatStatus status = SeatStatus::Free;
    int ticketId = 0;
    QString reservationName;
    QString ticketCode;        // Унікальний код проданого квитка (наприклад, TKT-1-42-9F2A)
    int paidKopiyky = 0;       // Сплачена сума в копійках
    QString soldAt;            // Дата та час продажу
    QString checkedInAt;       // Дата та час проходу контролю входу (порожньо, якщо ще не зайшов)
};