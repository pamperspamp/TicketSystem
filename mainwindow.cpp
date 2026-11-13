#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidgetItem>
#include <QTimer>
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
// НАЛАШТУВАННЯ ВКЛАДОК, ПАНЕЛІ АДМІНІСТРАТОРА ТА ПОВЕДІНКИ ТАБЛИЦІ
// ============================================================================
void MainWindow::setupTabs()
{
    QWidget *designerPage = takeCentralWidget();

    // Перша вкладка — Афіша подій + панель адміністратора
    auto *afishaPage = new QWidget(this);
    auto *afishaLayout = new QVBoxLayout(afishaPage);
    afishaLayout->setContentsMargins(16, 16, 16, 16);
    afishaLayout->setSpacing(12);

    auto *adminBar = new QHBoxLayout();
    auto *adminTitle = new QLabel(QStringLiteral("Афіша подій (подвійний клік — відкрити схему зали)"), afishaPage);
    adminTitle->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #cfd3dc;"));

    auto *addEventBtn = new QPushButton(QStringLiteral("+ Нова подія"), afishaPage);
    addEventBtn->setCursor(Qt::PointingHandCursor);
    addEventBtn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 7px 14px; background: #1f8b4c; color: #ffffff; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background: #26a65b; }"
        ));

    auto *editEventBtn = new QPushButton(QStringLiteral("✎ Редагувати подію"), afishaPage);
    editEventBtn->setCursor(Qt::PointingHandCursor);
    editEventBtn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 7px 14px; background: #2d313c; color: #ffffff; border: 1px solid #4a5060; border-radius: 6px; }"
        "QPushButton:hover { background: #3a3f4d; }"
        ));

    adminBar->addWidget(adminTitle);
    adminBar->addStretch();
    adminBar->addWidget(addEventBtn);
    adminBar->addWidget(editEventBtn);

    afishaLayout->addLayout(adminBar);
    afishaLayout->addWidget(designerPage);

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

    auto *scrollArea = new QScrollArea(hallPage);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    auto *scrollContent = new QWidget(scrollArea);
    auto *centerLayout = new QVBoxLayout(scrollContent);
    centerLayout->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
    centerLayout->setContentsMargins(20, 12, 20, 28);
    centerLayout->setSpacing(24);

    screenLabel = new QLabel(QStringLiteral("ЕКРАН / СЦЕНА"), scrollContent);
    screenLabel->setAlignment(Qt::AlignCenter);
    screenLabel->setMinimumHeight(36);
    screenLabel->setMinimumWidth(680);
    screenLabel->setStyleSheet(QStringLiteral(
        "background: #3a3f4b; color: #e6e9ef; font-weight: bold; font-size: 13px; "
        "letter-spacing: 4px; border-radius: 10px; padding: 6px;"
        ));
    centerLayout->addWidget(screenLabel, 0, Qt::AlignCenter);

    auto *seatsWrapper = new QWidget(scrollContent);
    seatsGrid = new QGridLayout(seatsWrapper);
    seatsGrid->setHorizontalSpacing(6);
    seatsGrid->setVerticalSpacing(8);
    centerLayout->addWidget(seatsWrapper, 0, Qt::AlignCenter);

    scrollArea->setWidget(scrollContent);
    hallLayout->addWidget(scrollArea);

    tabs->addTab(hallPage, QStringLiteral("2. Схема зали"));
    setCentralWidget(tabs);

    // Налаштування таблиці афіші
    ui->events->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->events->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->events->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->events->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ui->events->setFocusPolicy(Qt::NoFocus);
    ui->events->setStyleSheet(QStringLiteral(
        "QTableWidget { outline: 0; }"
        "QTableWidget::item:focus { border: none; outline: none; }"
        "QTableWidget::item:selected { background-color: #3a3f4d; color: #f0c674; }"
        ));

    ui->events->viewport()->installEventFilter(this);
    afishaPage->installEventFilter(this);

    auto *clearSelectionTimer = new QTimer(this);
    clearSelectionTimer->setSingleShot(true);
    clearSelectionTimer->setInterval(4000);

    connect(ui->events, &QTableWidget::itemSelectionChanged, this, [this, clearSelectionTimer]() {
        if (!ui->events->selectedItems().isEmpty()) {
            clearSelectionTimer->start();
        }
    });
    connect(clearSelectionTimer, &QTimer::timeout, this, [this]() {
        ui->events->clearSelection();
        ui->events->setCurrentCell(-1, -1);
    });

    connect(ui->events, &QTableWidget::cellDoubleClicked, this, &MainWindow::openEventHall);
    connect(backBtn, &QPushButton::clicked, this, [this]() { tabs->setCurrentIndex(0); });
    connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(addEventBtn, &QPushButton::clicked, this, &MainWindow::onAddEventClicked);
    connect(editEventBtn, &QPushButton::clicked, this, &MainWindow::onEditEventClicked);
}

// ============================================================================
// ЗНЯТТЯ ВИДІЛЕННЯ ПРИ КЛІКУ В ПОРОЖНЄ МІСЦЕ ТАБЛИЦІ АБО ВІКНА
// ============================================================================
bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto *mouseEvent = static_cast<QMouseEvent *>(event);

        if (watched == ui->events->viewport()) {
            if (!ui->events->itemAt(mouseEvent->pos())) {
                ui->events->clearSelection();
                ui->events->setCurrentCell(-1, -1);
            }
        } else if (watched != ui->events) {
            ui->events->clearSelection();
            ui->events->setCurrentCell(-1, -1);
        }
    }

    return QMainWindow::eventFilter(watched, event);
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

// ============================================================================
// ДІАЛОГ АДМІНІСТРАТОРА: СТВОРЕННЯ ТА РЕДАГУВАННЯ ПОДІЙ
// ============================================================================
bool MainWindow::showEventDialog(Event &eventData, bool isEdit)
{
    QString error;
    const auto halls = database.loadHalls(&error);
    if (!error.isEmpty() || halls.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), QStringLiteral("Не вдалося завантажити список залів."));
        return false;
    }

    QDialog dialog(this);
    dialog.setWindowTitle(isEdit ? QStringLiteral("Редагування події") : QStringLiteral("Створення нової події"));
    dialog.setMinimumWidth(400);

    auto *form = new QFormLayout(&dialog);
    form->setSpacing(12);

    auto *kindCombo = new QComboBox(&dialog);
    kindCombo->addItems({QStringLiteral("Вистава"), QStringLiteral("Концерт"), QStringLiteral("Кіно")});
    if (isEdit) {
        const int idx = kindCombo->findText(eventData.kind);
        if (idx >= 0) kindCombo->setCurrentIndex(idx);
    }

    auto *titleEdit = new QLineEdit(eventData.title, &dialog);
    titleEdit->setPlaceholderText(QStringLiteral("Наприклад: Гамлет"));

    auto *dateEdit = new QLineEdit(isEdit ? eventData.date : QStringLiteral("25.11.2026"), &dialog);
    dateEdit->setPlaceholderText(QStringLiteral("ДД.ММ.РРРР"));

    auto *timeEdit = new QLineEdit(isEdit ? eventData.time : QStringLiteral("19:00"), &dialog);
    timeEdit->setPlaceholderText(QStringLiteral("ГГ:ХХ"));

    auto *hallCombo = new QComboBox(&dialog);
    for (const auto &h : halls) {
        hallCombo->addItem(h.second, h.first);
    }
    if (isEdit) {
        const int hallIdx = hallCombo->findData(eventData.hallId);
        if (hallIdx >= 0) hallCombo->setCurrentIndex(hallIdx);

        if (database.hasActiveTicketsForEvent(eventData.id)) {
            hallCombo->setEnabled(false);
            hallCombo->setToolTip(QStringLiteral("Не можна змінити залу, бо на подію вже є заброньовані або продані квитки"));
        }
    }

    auto *regPriceSpin = new QDoubleSpinBox(&dialog);
    regPriceSpin->setRange(0.0, 50000.0);
    regPriceSpin->setDecimals(2);
    regPriceSpin->setSuffix(QStringLiteral(" грн"));
    regPriceSpin->setValue(isEdit ? (eventData.priceKopiyky / 100.0) : 250.0);

    auto *vipPriceSpin = new QDoubleSpinBox(&dialog);
    vipPriceSpin->setRange(0.0, 50000.0);
    vipPriceSpin->setDecimals(2);
    vipPriceSpin->setSuffix(QStringLiteral(" грн"));
    vipPriceSpin->setValue(isEdit ? (eventData.vipPriceKopiyky / 100.0) : 400.0);

    form->addRow(QStringLiteral("Тип події:"), kindCombo);
    form->addRow(QStringLiteral("Назва:"), titleEdit);
    form->addRow(QStringLiteral("Дата:"), dateEdit);
    form->addRow(QStringLiteral("Час:"), timeEdit);
    form->addRow(QStringLiteral("Зала:"), hallCombo);
    form->addRow(QStringLiteral("Звичайна ціна:"), regPriceSpin);
    form->addRow(QStringLiteral("VIP ціна:"), vipPriceSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Зберегти"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Скасувати"));
    form->addRow(buttons);

    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&]() {
        if (titleEdit->text().trimmed().isEmpty() ||
            dateEdit->text().trimmed().isEmpty() ||
            timeEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("Увага"), QStringLiteral("Заповніть назву, дату та час події."));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    eventData.kind = kindCombo->currentText();
    eventData.title = titleEdit->text().trimmed();
    eventData.date = dateEdit->text().trimmed();
    eventData.time = timeEdit->text().trimmed();
    eventData.hallId = hallCombo->currentData().toInt();
    eventData.priceKopiyky = qRound(regPriceSpin->value() * 100.0);
    eventData.vipPriceKopiyky = qRound(vipPriceSpin->value() * 100.0);
    return true;
}

void MainWindow::onAddEventClicked()
{
    Event newEvent;
    if (!showEventDialog(newEvent, false))
        return;

    QString error;
    if (!database.addEvent(newEvent, error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }

    refreshEvents();
}

void MainWindow::onEditEventClicked()
{
    const int row = ui->events->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("Вибір події"), QStringLiteral("Спочатку виділіть подію в таблиці."));
        return;
    }

    const int eventId = ui->events->item(row, 0)->data(Qt::UserRole).toInt();

    QString error;
    const QVector<Event> events = database.loadEvents(&error);
    Event targetEvent;
    bool found = false;
    for (const Event &ev : events) {
        if (ev.id == eventId) {
            targetEvent = ev;
            found = true;
            break;
        }
    }

    if (!found)
        return;

    if (!showEventDialog(targetEvent, true))
        return;

    if (!database.updateEvent(targetEvent, error)) {
        QMessageBox::warning(this, QStringLiteral("Помилка"), error);
        return;
    }

    refreshEvents();
}

// ============================================================================
// ВІДКРИТТЯ СХЕМИ ЗАЛИ ТА МАЛЮВАННЯ МІСЦЬ
// ============================================================================
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

    const int centerAisleCol = (maxSeatsInRow / 2) + 1;
    seatsGrid->setColumnMinimumWidth(centerAisleCol, 22);
    seatsGrid->setRowMinimumHeight(maxRow - 1, 44);

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
            btn->setToolTip(QStringLiteral("Ряд %1, Місце %2 — ПРОДАНО\nКод квитка: %3")
                                .arg(seat.rowNumber).arg(seat.seatNumber).arg(seat.ticketCode));
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #c0392b; color: #ffffff; font-weight: bold; border-radius: 6px; border: 1px solid #e74c3c; }"
                "QPushButton:hover { background: #d64535; }"
                ));
        }

        connect(btn, &QPushButton::clicked, this, [this, seat]() {
            onSeatClicked(seat);
        });

        seatsGrid->addWidget(btn, seat.rowNumber - 1, gridCol);
    }
}

// ============================================================================
// ЕТАП 5: ПРОДАЖ, БРОНЮВАННЯ, ВИКУП БРОНІ ТА ПЕРЕГЛЯД ЧЕКА
// ============================================================================
void MainWindow::onSeatClicked(const Seat &seat)
{
    if (currentEventId == 0)
        return;

    const bool isVip = (seat.rowNumber == currentMaxRow);
    const int priceKop = isVip ? currentVipPriceKopiyky : currentRegularPriceKopiyky;
    const QString priceStr = QString::number(priceKop / 100.0, 'f', 2);
    const QString seatType = isVip ? QStringLiteral(" (VIP)") : QString();

    // ------------------------------------------------------------------------
    // 1. МІСЦЕ ВІЛЬНЕ -> Вибір: "Продати квиток" або "Забронювати"
    // ------------------------------------------------------------------------
    if (seat.status == SeatStatus::Free) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QStringLiteral("Оформлення квитка"));
        msgBox.setText(QStringLiteral("Ряд %1%2, Місце %3\nВартість квитка: %4 грн\n\nОберіть дію:")
                           .arg(seat.rowNumber).arg(seatType).arg(seat.seatNumber).arg(priceStr));

        QPushButton *sellBtn = msgBox.addButton(QStringLiteral("Продати квиток"), QMessageBox::AcceptRole);
        QPushButton *reserveBtn = msgBox.addButton(QStringLiteral("Забронювати"), QMessageBox::ActionRole);
        msgBox.addButton(QStringLiteral("Скасувати"), QMessageBox::RejectRole);

        msgBox.exec();

        if (msgBox.clickedButton() == sellBtn) {
            QString ticketCode, error;
            if (!database.sellDirectSeat(currentEventId, seat.id, priceKop, ticketCode, error)) {
                QMessageBox::warning(this, QStringLiteral("Помилка продажу"), error);
                return;
            }

            renderHallSeats(currentEventId);

            QMessageBox::information(
                this,
                QStringLiteral("Квиток продано!"),
                QStringLiteral("Оплата пройшла успішно.\n\n"
                               "Місце: Ряд %1%2, Місце %3\n"
                               "Сплачено: %4 грн\n"
                               "Код квитка: %5")
                    .arg(seat.rowNumber).arg(seatType).arg(seat.seatNumber).arg(priceStr, ticketCode)
                );
        } else if (msgBox.clickedButton() == reserveBtn) {
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
        }
    }
    // ------------------------------------------------------------------------
    // 2. МІСЦЕ ЗАБРОНЬОВАНЕ -> Вибір: "Викупити (Продати)" або "Скасувати бронь"
    // ------------------------------------------------------------------------
    else if (seat.status == SeatStatus::Reserved) {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle(QStringLiteral("Керування бронюванням"));
        msgBox.setText(QStringLiteral("Ряд %1%2, Місце %3\nЗаброньовано на ім'я: «%4»\nДо сплати: %5 грн\n\nОберіть дію:")
                           .arg(seat.rowNumber).arg(seatType).arg(seat.seatNumber)
                           .arg(seat.reservationName, priceStr));

        QPushButton *buyBtn = msgBox.addButton(QStringLiteral("Викупити (Продати)"), QMessageBox::AcceptRole);
        QPushButton *cancelBtn = msgBox.addButton(QStringLiteral("Скасувати бронь"), QMessageBox::DestructiveRole);
        msgBox.addButton(QStringLiteral("Закрити"), QMessageBox::RejectRole);

        msgBox.exec();

        if (msgBox.clickedButton() == buyBtn) {
            QString ticketCode, error;
            if (!database.sellReservedTicket(seat.ticketId, priceKop, ticketCode, error)) {
                QMessageBox::warning(this, QStringLiteral("Помилка викупу"), error);
                return;
            }

            renderHallSeats(currentEventId);

            QMessageBox::information(
                this,
                QStringLiteral("Бронь викуплена!"),
                QStringLiteral("Квиток успішно оформлено.\n\n"
                               "Покупець: %1\n"
                               "Місце: Ряд %2%3, Місце %4\n"
                               "Сплачено: %5 грн\n"
                               "Код квитка: %6")
                    .arg(seat.reservationName).arg(seat.rowNumber).arg(seatType)
                    .arg(seat.seatNumber).arg(priceStr, ticketCode)
                );
        } else if (msgBox.clickedButton() == cancelBtn) {
            QString error;
            if (!database.cancelReservation(seat.ticketId, error)) {
                QMessageBox::warning(this, QStringLiteral("Помилка"), error);
                return;
            }

            renderHallSeats(currentEventId);
        }
    }
    // ------------------------------------------------------------------------
    // 3. МІСЦЕ ПРОДАНЕ -> Показуємо чек та інформацію про квиток
    // ------------------------------------------------------------------------
    else {
        const QString paidStr = QString::number(seat.paidKopiyky / 100.0, 'f', 2);
        QString details = QStringLiteral("Ряд %1%2, Місце %3 — ПРОДАНО\n\n"
                                         "Код квитка: %4\n"
                                         "Сплачено: %5 грн\n"
                                         "Час продажу: %6")
                              .arg(seat.rowNumber).arg(seatType).arg(seat.seatNumber)
                              .arg(seat.ticketCode, paidStr, seat.soldAt);

        if (!seat.reservationName.isEmpty()) {
            details += QStringLiteral("\nБронював: %1").arg(seat.reservationName);
        }

        QMessageBox::information(this, QStringLiteral("Інформація про квиток"), details);
    }
}