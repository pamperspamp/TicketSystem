#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

MainWindow::MainWindow(Database &db, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , database(db)
{
    ui->setupUi(this);
    setupTabs();
    refreshEvents();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupTabs()
{
    // Забираємо вміст вікна з дизайнера — він стане першою вкладкою
    QWidget *afishaPage = takeCentralWidget();

    tabs = new QTabWidget(this);
    tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #353842; border-radius: 6px; }"
        "QTabBar::tab { background: #23262e; color: #cfd3dc; padding: 8px 18px; margin-right: 4px; border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { background: #353945; color: #ffffff; font-weight: bold; }"
        ));

    tabs->addTab(afishaPage, QStringLiteral("1. Афіша подій"));

    // Створюємо другу вкладку — схема зали
    auto *hallPage = new QWidget(this);
    auto *hallLayout = new QVBoxLayout(hallPage);
    hallLayout->setContentsMargins(20, 20, 20, 20);
    hallLayout->setSpacing(14);

    auto *topBar = new QHBoxLayout();
    auto *backBtn = new QPushButton(QStringLiteral("← Назад до афіші"), hallPage);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(QStringLiteral("padding: 6px 14px; background: #2d313c; color: #ffffff; border-radius: 6px;"));

    hallHeaderLabel = new QLabel(QStringLiteral("Оберіть подію в афіші (подвійний клік)"), hallPage);
    hallHeaderLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #f0c674;"));

    topBar->addWidget(backBtn);
    topBar->addSpacing(12);
    topBar->addWidget(hallHeaderLabel);
    topBar->addStretch();
    hallLayout->addLayout(topBar);

    auto *seatsWrapper = new QWidget(hallPage);
    seatsGrid = new QGridLayout(seatsWrapper);
    seatsGrid->setSpacing(10);
    hallLayout->addWidget(seatsWrapper, 0, Qt::AlignCenter);
    hallLayout->addStretch();

    tabs->addTab(hallPage, QStringLiteral("2. Схема зали"));
    setCentralWidget(tabs);

    // Налаштування поведінки таблиці подій
    ui->events->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->events->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->events->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->events->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui->events, &QTableWidget::cellDoubleClicked, this, &MainWindow::openEventHall);
    connect(backBtn, &QPushButton::clicked, this, [this]() { tabs->setCurrentIndex(0); });
    connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
}

void MainWindow::refreshEvents()
{
    QString error;
    const QVector<Event> events = database.loadEvents(&error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Помилка завантаження"), error);
        return;
    }

    ui->events->setRowCount(0);
    ui->events->setColumnCount(6);
    ui->events->setHorizontalHeaderLabels({
        QStringLiteral("Тип"),
        QStringLiteral("Назва"),
        QStringLiteral("Дата"),
        QStringLiteral("Час"),
        QStringLiteral("Зала"),
        QStringLiteral("Ціна")
    });

    for (int i = 0; i < events.size(); ++i) {
        const Event &ev = events[i];
        ui->events->insertRow(i);

        auto *kindItem = new QTableWidgetItem(ev.kind);
        kindItem->setData(Qt::UserRole, ev.id); // Ховаємо ID події в першу комірку

        const QString priceText = QStringLiteral("%1 грн")
                                      .arg(static_cast<double>(ev.priceKopiyky) / 100.0, 0, 'f', 2);

        ui->events->setItem(i, 0, kindItem);
        ui->events->setItem(i, 1, new QTableWidgetItem(ev.title));
        ui->events->setItem(i, 2, new QTableWidgetItem(ev.date));
        ui->events->setItem(i, 3, new QTableWidgetItem(ev.time));
        ui->events->setItem(i, 4, new QTableWidgetItem(ev.hallName));
        ui->events->setItem(i, 5, new QTableWidgetItem(priceText));
    }
}

void MainWindow::openEventHall(int row, int)
{
    auto *idItem = ui->events->item(row, 0);
    if (!idItem)
        return;

    currentEventId = idItem->data(Qt::UserRole).toInt();

    const QString title = ui->events->item(row, 1)->text();
    const QString date = ui->events->item(row, 2)->text();
    const QString time = ui->events->item(row, 3)->text();
    const QString hall = ui->events->item(row, 4)->text();

    hallHeaderLabel->setText(
        QStringLiteral("%1 — %2 о %3 (Зала: %4)").arg(title, date, time, hall));

    renderHallSeats(currentEventId);
    tabs->setCurrentIndex(1);
}

void MainWindow::onTabChanged(int index)
{
    if (index == 0) {
        refreshEvents();
    }
}

void MainWindow::renderHallSeats(int eventId)
{
    if (!seatsGrid)
        return;

    while (QLayoutItem *item = seatsGrid->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    QString error;
    const QVector<Seat> seats = database.loadSeatsForEvent(eventId, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }

    if (seats.isEmpty()) {
        seatsGrid->addWidget(new QLabel(QStringLiteral("Місця для цієї зали не налаштовані")), 0, 0);
        return;
    }

    for (const Seat &seat : seats) {
        auto *btn = new QPushButton(
            QStringLiteral("Р%1 М%2").arg(seat.rowNumber).arg(seat.seatNumber));
        btn->setMinimumSize(78, 44);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("seatId", seat.id);

        if (seat.status == SeatStatus::Free) {
            btn->setStyleSheet(QStringLiteral("background: #28382d; color: #b9f5c8; border: 1px solid #487354; border-radius: 6px;"));
        } else if (seat.status == SeatStatus::Reserved) {
            btn->setStyleSheet(QStringLiteral("background: #473c24; color: #ffe099; border: 1px solid #8a6d33; border-radius: 6px;"));
        } else {
            btn->setStyleSheet(QStringLiteral("background: #47262b; color: #ffb3bd; border: 1px solid #853d47; border-radius: 6px;"));
        }

        seatsGrid->addWidget(btn, seat.rowNumber - 1, seat.seatNumber - 1);
    }
}