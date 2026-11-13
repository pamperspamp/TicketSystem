#pragma once

#include <QString>

struct Event
{
    int id = 0;
    int durationMinutes = 120;
    bool finished = false;
    QString kind;
    QString title;
    QString date;
    QString time;
    int hallId = 0;
    QString hallName;
    int priceKopiyky = 0;    // Звичайна ціна квитка в копійках
    int vipPriceKopiyky = 0; // Окрема ціна для VIP-місць (останній ряд) у копійках
};