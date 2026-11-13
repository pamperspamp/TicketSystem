#pragma once

// Три можливі стани кожного крісла на конкретну подію
enum class SeatStatus
{
    Free,      // Вільне (зелене)
    Reserved,  // Заброньоване (жовте)
    Sold       // Продане (червоне)
};

// Структура, яка описує одне місце в залі
struct Seat
{
    int id = 0;           // Унікальний ID місця з таблиці seats
    int hallId = 0;       // До якої зали належить
    int rowNumber = 0;    // Номер ряду (1, 2, 3...)
    int seatNumber = 0;   // Номер місця в ряду (1, 2, 3...)
    SeatStatus status = SeatStatus::Free; // Поточний стан
};