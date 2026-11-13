#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMap>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
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

// ============================================================================
// НАЛАШТУВАННЯ ВКЛАДОК ТА МАКЕТУ ЗАЛИ З ВІДСТУПАМИ
// ============================================================================
void MainWindow::setupTabs()
{
    QWidget *afishaPage = takeCentralWidget();

    tabs = new QTabWidget(this);
    tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #353842; border-radius: 6px; }"
        "QTabBar::tab { background: #23262e; color: #cfd3dc; padding: 9px 20px; margin-right: 4px; border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { background: #353945; color: #ffffff; font-weight: bold; }"
        ));

    tabs->addTab(afishaPage, QStringLiteral("1. Афіша подій"));

    // Друга вкладка — Схема зали
    auto *hallPage = new QWidget(this);
    auto *hallLayout = new QVBoxLayout(hallPage);
    hallLayout->setContentsMargins(24, 20, 24, 24);
    hallLayout->setSpacing(16);

    // Верхня панель із кнопкою повернення та заголовком події
    auto *topBar = new QHBoxLayout();
    auto *backBtn = new QPushButton(QStringLiteral("← Назад до афіші"), hallPage);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 7px 16px; background: #2d313c; color: #ffffff; border-radius: 6px; }"
        "QPushButton:hover { background: #3a3f4d; }"
        ));

    hallHeaderLabel = new QLabel(QStringLiteral("Оберіть подію в афіші (подвійний клік)"), hallPage);
    hallHeaderLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #f0c674;"));

    topBar->addWidget(backBtn);
    topBar->addSpacing(16);
    topBar->addWidget(hallHeaderLabel);
    topBar->addStretch();
    hallLayout->addLayout(topBar);

    // Прокрутка для великих залів (100+ місць)
    auto *scrollArea = new QScrollArea(hallPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollContent = new QWidget(scrollArea);
    auto *centerLayout = new QVBoxLayout(scrollContent);
    centerLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    centerLayout->setContentsMargins(20, 12, 20, 28);
    centerLayout->setSpacing(24);

    // Плашка "ЕКРАН" або "СЦЕНА"
    screenLabel = new QLabel(QStringLiteral("ЕКРАН / СЦЕНА"), scrollContent);
    screenLabel->setAlignment(Qt::AlignCenter);
    screenLabel->setMinimumHeight(36);
    screenLabel->setMinimumWidth(680);
    screenLabel->setStyleSheet(QStringLiteral(
        "background: #3a3f4b; color: #e6e9ef; font-weight: bold; font-size: 13px; "
        "letter-spacing: 4px; border-radius: 10px; padding: 6px;"
        ));
    centerLayout->addWidget(screenLabel, 0, Qt::AlignCenter);

    // Сітка крісел
    auto *seatsWrapper = new QWidget(scrollContent);
    seatsGrid = new QGridLayout(seatsWrapper);
    seatsGrid->setHorizontalSpacing(6);
    seatsGrid->setVerticalSpacing(8);
    centerLayout->addWidget(seatsWrapper, 0, Qt::AlignCenter);

    scrollArea->setWidget(scrollContent);
    hallLayout->addWidget(scrollArea);

    tabs->addTab(hallPage, QStringLiteral("2. Схема зали"));
    setCentralWidget(tabs);

    ui->events->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->events->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->events->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->events->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(ui->events, &QTableWidget::cellDoubleClicked, this, &MainWindow::openEventHall);
    connect(backBtn, &QPushButton::clicked, this, [this]() { tabs->setCurrentIndex(0); });
    connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
}

// ============================================================================
// ЗАВАНТАЖЕННЯ АФІШІ ЗІ ЗВИЧАЙНОЮ ТА VIP-ЦІНОЮ
// ============================================================================
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
        QStringLiteral("Ціна (Звич. / VIP)")
    });

    for (int i = 0; i < events.size(); ++i) {
        const Event &ev = events[i];
        ui->events->insertRow(i);

        auto *kindItem = new QTableWidgetItem(ev.kind);
        kindItem->setData(Qt::UserRole, ev.id);

        const double regPrice = static_cast<double>(ev.priceKopiyky) / 100.0;
        const double vipPrice = static_cast<double>(ev.vipPriceKopiyky) / 100.0;
        const QString priceText = QStringLiteral("%1 / %2 грн")
                                      .arg(regPrice, 0, 'f', 2)
                                      .arg(vipPrice, 0, 'f', 2);

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

// ============================================================================
// МАЛЮВАННЯ СХЕМИ ЗАЛИ: ПРОХІД ПО ЦЕНТРУ, ВІДСТУПИ, VIP-ДИВАНИ ТА ЦІНИ
// ============================================================================
void MainWindow::renderHallSeats(int eventId)
{
    if (!seatsGrid)
        return;

    while (QLayoutItem *item = seatsGrid->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }

    // Дізнаємося тип події та її ціни (звичайну і VIP) з БД
    const QString eventKind = database.getEventKind(eventId);
    const bool isCinema = (eventKind == QStringLiteral("Кіно"));
    screenLabel->setText(isCinema ? QStringLiteral("ЕКРАН") : QStringLiteral("СЦЕНА"));

    database.getEventPrices(eventId, currentRegularPriceKopiyky, currentVipPriceKopiyky);
    const QString regPriceStr = QString::number(currentRegularPriceKopiyky / 100.0, 'f', 2);
    const QString vipPriceStr = QString::number(currentVipPriceKopiyky / 100.0, 'f', 2);

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

    // Визначаємо максимальну довжину ряду та номер останнього (VIP) ряду
    int maxSeatsInRow = 0;
    int maxRow = 0;
    QMap<int, int> rowCounts;
    for (const Seat &seat : seats) {
        rowCounts[seat.rowNumber]++;
        if (rowCounts[seat.rowNumber] > maxSeatsInRow)
            maxSeatsInRow = rowCounts[seat.rowNumber];
        if (seat.rowNumber > maxRow)
            maxRow = seat.rowNumber;
    }
    currentMaxRow = maxRow;

    // Центральний прохід та відступ перед останнім рядом (VIP / Дивани)
    const int centerAisleCol = (maxSeatsInRow / 2) + 1;
    seatsGrid->setColumnMinimumWidth(centerAisleCol, 22);
    seatsGrid->setRowMinimumHeight(maxRow - 1, 44);

    // Підписи рядів зліва та справа
    for (auto it = rowCounts.constBegin(); it != rowCounts.constEnd(); ++it) {
        const int rowNum = it.key();
        const bool isLastSofaRow = (rowNum == maxRow);

        const QString rowText = isLastSofaRow
                                    ? QStringLiteral("Ряд %1 (VIP)").arg(rowNum)
                                    : QStringLiteral("Ряд %1").arg(rowNum);

        const QString labelStyle = isLastSofaRow
                                       ? QStringLiteral("color: #c39bd3; font-size: 12px; font-weight: bold; padding: 0 12px;")
                                       : QStringLiteral("color: #9aa2b1; font-size: 12px; font-weight: bold; padding: 0 12px;");

        auto *leftLbl = new QLabel(rowText);
        leftLbl->setStyleSheet(labelStyle);
        leftLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        auto *rightLbl = new QLabel(rowText);
        rightLbl->setStyleSheet(labelStyle);
        rightLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        seatsGrid->addWidget(leftLbl, rowNum - 1, 0);
        seatsGrid->addWidget(rightLbl, rowNum - 1, maxSeatsInRow + 2);
    }

    // Розставляємо крісла по рядах
    for (const Seat &seat : seats) {
        const int countInThisRow = rowCounts[seat.rowNumber];
        const bool isLastSofaRow = (seat.rowNumber == maxRow);

        const int centerOffset = (maxSeatsInRow - countInThisRow) / 2;
        int gridCol = seat.seatNumber + centerOffset;

        if (gridCol >= centerAisleCol) {
            gridCol += 1;
        }

        auto *btn = new QPushButton(QString::number(seat.seatNumber));
        btn->setCursor(Qt::PointingHandCursor);

        if (isLastSofaRow) {
            btn->setFixedSize(36, 32);
        } else {
            btn->setFixedSize(31, 29);
        }

        if (seat.status == SeatStatus::Free) {
            if (isLastSofaRow) {
                btn->setToolTip(QStringLiteral("Ряд %1 (VIP Диван), Місце %2 — Вільне\nЦіна: %3 грн")
                                    .arg(seat.rowNumber).arg(seat.seatNumber).arg(vipPriceStr));
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: #6c3483; color: #ffffff; font-weight: bold; border-radius: 7px; border: 1px solid #9b59b6; }"
                    "QPushButton:hover { background: #884ea0; }"
                    ));
            } else {
                btn->setToolTip(QStringLiteral("Ряд %1, Місце %2 — Вільне\nЦіна: %3 грн")
                                    .arg(seat.rowNumber).arg(seat.seatNumber).arg(regPriceStr));
                btn->setStyleSheet(QStringLiteral(
                    "QPushButton { background: #1f8b4c; color: #ffffff; font-weight: bold; border-radius: 6px; border: 1px solid #28a75c; }"
                    "QPushButton:hover { background: #26a65b; }"
                    ));
            }
        } else if (seat.status == SeatStatus::Reserved) {
            btn->setToolTip(QStringLiteral("Ряд %1, Місце %2 — Бронь: %3")
                                .arg(seat.rowNumber).arg(seat.seatNumber).arg(seat.reservationName));
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #d4a017; color: #1c1e24; font-weight: bold; border-radius: 6px; border: 1px solid #f0b929; }"
                "QPushButton:hover { background: #e6b022; }"
                ));
        } else {
            btn->setToolTip(QStringLiteral("Ряд %1, Місце %2 — Продано")
                                .arg(seat.rowNumber).arg(seat.seatNumber));
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #c0392b; color: #ffffff; font-weight: bold; border-radius: 6px; border: 1px solid #e74c3c; }"
                ));
        }

        connect(btn, &QPushButton::clicked, this, [this, seat]() {
            onSeatClicked(seat);
        });

        seatsGrid->addWidget(btn, seat.rowNumber - 1, gridCol);
    }
}

// ============================================================================
// БРОНЮВАННЯ З УРАХУВАННЯМ VIP-ЦІНИ ТА СКАСУВАННЯ БРОНІ
// ============================================================================
void MainWindow::onSeatClicked(const Seat &seat)
{
    if (currentEventId == 0)
        return;

    if (seat.status == SeatStatus::Free) {
        const bool isVip = (seat.rowNumber == currentMaxRow);
        const int priceKop = isVip ? currentVipPriceKopiyky : currentRegularPriceKopiyky;
        const QString priceStr = QString::number(priceKop / 100.0, 'f', 2);
        const QString seatType = isVip ? QStringLiteral(" (VIP)") : QString();

        bool ok = false;
        const QString name = QInputDialog::getText(
                                 this,
                                 QStringLiteral("Бронювання місця"),
                                 QStringLiteral("Ряд %1%2, Місце %3 — Вартість: %4 грн.\nВведіть ім'я та прізвище клієнта:")
                                     .arg(seat.rowNumber).arg(seatType).arg(seat.seatNumber).arg(priceStr),
                                 QLineEdit::Normal,
                                 QString(),
                                 &ok
                                 ).trimmed();

        if (!ok)
            return;

        if (name.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("Увага"), QStringLiteral("Ім'я клієнта не може бути порожнім."));
            return;
        }

        QString error;
        if (!database.reserveSeat(currentEventId, seat.id, name, error)) {
            QMessageBox::warning(this, QStringLiteral("Помилка бронювання"), error);
            return;
        }

        renderHallSeats(currentEventId);
    } else if (seat.status == SeatStatus::Reserved) {
        const auto answer = QMessageBox::question(
            this,
            QStringLiteral("Скасування броні"),
            QStringLiteral("Ряд %1, Місце %2 заброньовано на ім'я:\n«%3»\n\nСкасувати цю бронь?")
                .arg(seat.rowNumber).arg(seat.seatNumber).arg(seat.reservationName),
            QMessageBox::Yes | QMessageBox::No
            );

        if (answer != QMessageBox::Yes)
            return;

        QString error;
        if (!database.cancelReservation(seat.ticketId, error)) {
            QMessageBox::warning(this, QStringLiteral("Помилка"), error);
            return;
        }

        renderHallSeats(currentEventId);
    } else {
        QMessageBox::information(
            this,
            QStringLiteral("Місце зайняте"),
            QStringLiteral("Ряд %1, Місце %2 вже продане.").arg(seat.rowNumber).arg(seat.seatNumber)
            );
    }
}