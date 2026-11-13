#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QHeaderView>
#include <QHoverEvent>
#include <QMessageBox>

namespace {
class RowHighlight final : public QObject
{
public:
    explicit RowHighlight(QTableWidget *view) : QObject(view), table(view)
    {
        table->viewport()->setAttribute(Qt::WA_Hover);
        table->viewport()->installEventFilter(this);
    }

private:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Resize) {
            auto *header = table->horizontalHeader();
            int available = table->viewport()->width();
            for (int i = 0; i < header->count(); ++i)
                if (i != 1)
                    available -= header->sectionSize(i);
            const int minimum = table->fontMetrics().horizontalAdvance(QStringLiteral("Джазовий вечір")) + 28;
            header->resizeSection(1, qMax(minimum, available));
            return false;
        }
        int row = -1;
        if (event->type() == QEvent::HoverMove || event->type() == QEvent::HoverEnter)
            row = table->rowAt(static_cast<QHoverEvent *>(event)->position().toPoint().y());
        else if (event->type() != QEvent::HoverLeave)
            return false;
        if (row != currentRow) {
            setBackground(currentRow, {});
            currentRow = row;
            setBackground(row, QColor("#403747"));
        }
        return false;
    }

    void setBackground(int row, const QBrush &brush)
    {
        if (row < 0)
            return;
        for (int column = 0; column < table->columnCount(); ++column)
            if (auto *item = table->item(row, column))
                item->setBackground(brush);
    }

    QTableWidget *table;
    int currentRow = -1;
};
}

MainWindow::MainWindow(Database &db, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , database(db)
{
    ui->setupUi(this);

    auto *table = ui->events;
    auto *header = table->horizontalHeader();
    header->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->horizontalHeaderItem(5)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    header->setSectionResizeMode(QHeaderView::ResizeToContents);
    header->setMinimumSectionSize(48);
    header->setSectionResizeMode(1, QHeaderView::Fixed);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    for (int column : {0, 2, 3, 5}) {
        const Qt::Alignment alignment = (column == 5 ? Qt::AlignRight : Qt::AlignHCenter) | Qt::AlignVCenter;
        table->horizontalHeaderItem(column)->setTextAlignment(alignment);
    }

    table->setTextElideMode(Qt::ElideNone);
    table->setWordWrap(true);
    table->verticalHeader()->setMinimumSectionSize(52);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(header, &QHeaderView::sectionResized, table, [table](int column) {
        if (column == 1)
            table->resizeRowsToContents();
    });

    new RowHighlight(table);

    // Завантажуємо реальні дані з БД при старті
    refreshEvents();
}

void MainWindow::refreshEvents()
{
    QString error;
    const QVector<Event> events = database.loadEvents(&error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Помилка завантаження"), error);
        return;
    }

    auto *table = ui->events;
    table->setRowCount(0); // прибираємо приклади з mainwindow.ui

    for (int row = 0; row < events.size(); ++row) {
        const Event &ev = events[row];
        table->insertRow(row);

        // Вертикальний заголовок з типом події ("Вистава", "Концерт", "Кіно")
        table->setVerticalHeaderItem(row, new QTableWidgetItem(ev.kind));

        const QString idText = QStringLiteral("%1").arg(ev.id, 2, 10, QLatin1Char('0'));
        const QString dateText = ev.date;
        const QString timeText = ev.time;
        const QString priceText = (ev.priceKopiyky % 100 == 0)
                                      ? QStringLiteral("%1 грн").arg(ev.priceKopiyky / 100)
                                      : QStringLiteral("%1 грн").arg(ev.priceKopiyky / 100.0, 0, 'f', 2);

        auto *idItem = new QTableWidgetItem(idText);
        // Зберігаємо реальний ID в Qt::UserRole (як вимагає етап 2 плану)
        idItem->setData(Qt::UserRole, ev.id);

        table->setItem(row, 0, idItem);
        table->setItem(row, 1, new QTableWidgetItem(ev.title));
        table->setItem(row, 2, new QTableWidgetItem(dateText));
        table->setItem(row, 3, new QTableWidgetItem(timeText));
        table->setItem(row, 4, new QTableWidgetItem(ev.hallName));
        table->setItem(row, 5, new QTableWidgetItem(priceText));

        for (int column : {0, 2, 3, 5}) {
            const Qt::Alignment alignment = (column == 5 ? Qt::AlignRight : Qt::AlignHCenter) | Qt::AlignVCenter;
            table->item(row, column)->setTextAlignment(alignment);
        }
    }

    table->resizeRowsToContents();
}

MainWindow::~MainWindow()
{
    delete ui;
}