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
};