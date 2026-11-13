#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QHeaderView>
#include <QHoverEvent>

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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
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
        for (int row = 0; row < table->rowCount(); ++row)
            if (auto *item = table->item(row, column))
                item->setTextAlignment(alignment);
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
}

MainWindow::~MainWindow()
{
    delete ui;
}
