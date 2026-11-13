#ifndef EVENT_H
#define EVENT_H

#endif // EVENT_H

#pragma once
#include <QString>

struct Event
{
    int id = 0;
    QString kind;       // Тип (Вистава, Концерт, Кіно)
    QString title;      // Назва
    QString date;       // Дата
    QString time;       // Час
    int hallId = 0;     // Номер зали в базі
    QString hallName;   // Назва зали
    int priceKopiyky = 0; // Ціна в копійках
};
