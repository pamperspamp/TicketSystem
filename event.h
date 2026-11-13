#pragma once

#include <QString>

struct Event
{
    int id = 0;
    QString kind;
    QString title;
    QString date;
    QString time;
    int hallId = 0;
    QString hallName;
    int priceKopiyky = 0;    // Звичайна ціна в копійках
    int vipPriceKopiyky = 0; // Ціна VIP-місць (останній ряд) у копійках
};