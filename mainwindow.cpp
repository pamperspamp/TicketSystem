#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QPdfWriter>
#include <QPrintDialog>
#include <QPrinter>
#include <QSaveFile>
#include <QTextBrowser>
#include <QUrl>
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
#include <QSqlQuery>
#include <QCalendarWidget>
#include <QTextCharFormat>
#include <QSpinBox>
#include <QDateTime>
#include <QDateEdit>
#include <QCheckBox>
#include <QTimeEdit>
#include <QVBoxLayout>

MainWindow::MainWindow(Database &db, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , database(db)
{
    ui->setupUi(this);
    ui->menubar->hide(); // All actions are available in the event list and hall toolbar.
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
    auto *remove = new QPushButton(tr("Видалити"), afishaPage);
    auto *report = new QPushButton(tr("Звіт за показами"), afishaPage);
    adminBar->addWidget(remove);
    adminBar->addWidget(report);
    connect(report, &QPushButton::clicked, this, &MainWindow::showReport);
    connect(remove, &QPushButton::clicked, this, [this] {
        auto *item = ui->events->item(ui->events->currentRow(), 0);
        if (!item) {
            QMessageBox::information(this, tr("Видалення"), tr("Спочатку виберіть показ."));
            return;
        }
        const int id = item->data(Qt::UserRole).toInt();
        if (QMessageBox::question(this, tr("Видалення"), tr("Видалити вибраний показ?"),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
        QString error;
        if (!database.deleteEvent(id, error)) {
            QMessageBox::warning(this, tr("Видалення"), error);
            return;
        }
        if (id == currentEventId) {
            currentEventId = 0;
            tabs->setTabEnabled(1, false);
        }
        refreshEvents();
    });

    auto *brand = new QLabel(QStringLiteral("<span style='font-size:42px;color:#e8bc73'>◒</span> <span style='font-size:34px;font-weight:600'>ОБРІЙ</span><br><span style='color:#aaa'>Зала подій · квиткова каса</span>"), afishaPage);
    afishaLayout->addWidget(brand);
    afishaLayout->addLayout(adminBar);
    auto *filters = new QHBoxLayout;
    scheduleFilter = new QComboBox(afishaPage);
    scheduleFilter->addItems({tr("Активна афіша"), tr("Архів показів")});
    filters->addWidget(scheduleFilter);
    filters->addWidget(new QLabel(tr("Дата показу"), afishaPage));
    scheduleDate = new QDateEdit(QDate::currentDate(), afishaPage);
    scheduleDate->setCalendarPopup(true);
    scheduleDate->setDisplayFormat("dd.MM.yyyy");
    filters->addWidget(scheduleDate);
    allDates = new QCheckBox(tr("Усі дати"), afishaPage);
    allDates->setChecked(true);
    filters->addWidget(allDates);
    auto *today = new QPushButton(tr("Сьогодні"), afishaPage);
    auto *next = new QPushButton(tr("Найближчий показ"), afishaPage);
    auto *search = new QPushButton(tr("Знайти квиток / бронь"), afishaPage);
    filters->addWidget(today);
    filters->addWidget(next);
    filters->addWidget(search);
    connect(today, &QPushButton::clicked, this, [this] {scheduleDate->setDate(QDate::currentDate());allDates->setChecked(false);});
    connect(next, &QPushButton::clicked, this, [this] {
        for (const auto &e : database.loadEvents()) {
            auto d=QDate::fromString(e.date,"dd.MM.yyyy");
            if(!e.finished && d>=QDate::currentDate()){scheduleFilter->setCurrentIndex(0);scheduleDate->setDate(d);allDates->setChecked(false);return;}
        }
        QMessageBox::information(this,tr("Афіша"),tr("Майбутніх показів немає."));
    });
    search->setText(tr("Знайти / викупити бронь"));
    connect(search, &QPushButton::clicked, this, [this] {
        bool ok;
        const auto term = QInputDialog::getText(this, tr("Пошук квитків і броней"),
            tr("Ім’я бронювальника або код квитка"), QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || term.isEmpty())
            return;
        QSqlQuery q;
        if (!q.exec("SELECT t.id,t.reservation_name,e.title,e.event_date,e.event_time,h.name,"
                    "s.row_number,s.seat_number,t.status,t.code,e.finished,"
                    "CASE WHEN s.row_number=(SELECT MAX(row_number) FROM seats WHERE hall_id=e.hall_id) "
                    "THEN e.vip_price_kopiyky ELSE e.price_kopiyky END "
                    "FROM tickets t JOIN events e ON e.id=t.event_id JOIN seats s ON s.id=t.seat_id "
                    "JOIN halls h ON h.id=e.hall_id ORDER BY e.event_date,e.event_time,t.id")) {
            QMessageBox::warning(this, tr("Пошук"), tr("Не вдалося завантажити броні."));
            return;
        }
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Збіги — виберіть конкретну бронь"));
        dialog.resize(1100, 430);
        auto *layout = new QVBoxLayout(&dialog);
        auto *table = new QTableWidget(0, 9, &dialog);
        table->setHorizontalHeaderLabels({tr("№ броні"),tr("Ім’я"),tr("Подія"),tr("Дата"),tr("Час"),tr("Зала"),tr("Ряд / місце"),tr("Стан"),tr("До сплати")});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        table->verticalHeader()->hide();
        while (q.next()) {
            if (!q.value(1).toString().contains(term, Qt::CaseInsensitive) &&
                q.value(9).toString().compare(term, Qt::CaseInsensitive) != 0)
                continue;
            const int row = table->rowCount();
            table->insertRow(row);
            const QString status = q.value(8).toString();
            const auto start = QDateTime(QDate::fromString(q.value(3).toString(),"dd.MM.yyyy"),
                                         QTime::fromString(q.value(4).toString(),"HH:mm"));
            const bool eligible = status == "reserved" && !q.value(10).toBool() &&
                                  start.addSecs(-30*60) > QDateTime::currentDateTime();
            const QStringList values = {q.value(0).toString(),q.value(1).toString(),q.value(2).toString(),
                q.value(3).toString(),q.value(4).toString(),q.value(5).toString(),
                QString("%1 / %2").arg(q.value(6).toString(),q.value(7).toString()),
                status=="reserved" ? (eligible ? tr("Бронь") : tr("Строк минув / показ завершено")) :
                status=="sold" ? tr("Продано") : tr("Скасовано"),
                QString::number(q.value(11).toInt()/100.0,'f',2)+" грн"};
            for(int col=0;col<values.size();++col)
                table->setItem(row,col,new QTableWidgetItem(values[col]));
            table->item(row,0)->setData(Qt::UserRole,q.value(0));
            table->item(row,0)->setData(Qt::UserRole+1,q.value(11));
            table->item(row,0)->setData(Qt::UserRole+2,eligible);
        }
        layout->addWidget(new QLabel(table->rowCount() ? tr("Звірте ім’я, подію й місце з покупцем. № броні однозначно визначає запис.") : tr("Збігів немає."), &dialog));
        layout->addWidget(table);
        auto *sell = new QPushButton(tr("Викупити вибрану бронь"), &dialog);
        sell->setEnabled(false);
        layout->addWidget(sell);
        connect(table,&QTableWidget::itemSelectionChanged,&dialog,[table,sell]{
            auto *id=table->item(table->currentRow(),0);
            sell->setEnabled(id && id->data(Qt::UserRole+2).toBool());
        });
        connect(sell,&QPushButton::clicked,&dialog,[&,table,sell]{
            auto *id=table->item(table->currentRow(),0);
            if(!id || !id->data(Qt::UserRole+2).toBool())return;
            const int ticketId=id->data(Qt::UserRole).toInt();
            if(QMessageBox::question(&dialog,tr("Підтвердити оплату"),
                tr("Викупити бронь №%1 за %2?").arg(ticketId).arg(table->item(table->currentRow(),8)->text()),
                QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes)return;
            QString code,error;
            if(!database.sellReservedTicket(ticketId,id->data(Qt::UserRole+1).toInt(),code,error)){
                QMessageBox::warning(&dialog,tr("Бронь не викуплена"),error);
                return;
            }
            sell->setEnabled(false);
            id->setData(Qt::UserRole+2,false);
            table->item(table->currentRow(),7)->setText(tr("Продано"));
            showTicketFile(code);
            refreshEvents();
        });
        dialog.exec();
    });
    filters->addStretch();
    afishaLayout->addLayout(filters);
    emptySchedule = new QLabel(tr("Показів за вибраними умовами немає."), afishaPage);
    afishaLayout->addWidget(emptySchedule);
    connect(scheduleDate, &QDateEdit::dateChanged, this, [this] {
        if (allDates->isChecked())
            allDates->setChecked(false);
        else
            refreshEvents();
    });
    connect(allDates, &QCheckBox::toggled, this, &MainWindow::refreshEvents);
    connect(scheduleFilter, &QComboBox::currentIndexChanged, this, &MainWindow::refreshEvents);
    afishaLayout->addWidget(designerPage);

    tabs = new QTabWidget(this);
    tabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: 1px solid #353842; border-radius: 6px; }"
        "QTabBar::tab { background: #23262e; color: #cfd3dc; padding: 9px 20px; margin-right: 4px; border-top-left-radius: 6px; border-top-right-radius: 6px; }"
        "QTabBar::tab:selected { background: #353945; color: #ffffff; font-weight: bold; }"
        ));

    tabs->addTab(afishaPage, QStringLiteral("1. Афіша подій"));

    // Друга вкладка — Схема зали + Статистика + Контроль входу
    auto *hallPage = new QWidget(this);
    auto *hallLayout = new QVBoxLayout(hallPage);
    hallLayout->setContentsMargins(24, 20, 24, 24);
    hallLayout->setSpacing(14);

    auto *topBar = new QHBoxLayout();
    auto *backBtn = new QPushButton(QStringLiteral("← Назад до афіші"), hallPage);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 7px 16px; background: #2d313c; color: #ffffff; border-radius: 6px; }"
        "QPushButton:hover { background: #3a3f4d; }"
        ));

    hallHeaderLabel = new QLabel(QStringLiteral("Оберіть подію в афіші (подвійний клік)"), hallPage);
    hallHeaderLabel->setStyleSheet(QStringLiteral("font-size: 16px; font-weight: bold; color: #f0c674;"));

    auto *checkInBtn = new QPushButton(QStringLiteral("🔍 Контроль входу (Check-in)"), hallPage);
    checkInBtn->setCursor(Qt::PointingHandCursor);
    checkInBtn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 7px 16px; background: #2471a3; color: #ffffff; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background: #2e86c1; }"
        ));

    topBar->addWidget(backBtn);
    topBar->addSpacing(16);
    topBar->addWidget(hallHeaderLabel);
    topBar->addStretch();
    topBar->addWidget(checkInBtn);
    hallLayout->addLayout(topBar);

    // Жива статистика по залі та виручці
    statsLabel = new QLabel(QStringLiteral("Оберіть подію для перегляду статистики"), hallPage);
    statsLabel->setAlignment(Qt::AlignCenter);
    statsLabel->setStyleSheet(QStringLiteral(
        "background: #23262e; color: #cfd3dc; border: 1px solid #353842; "
        "border-radius: 6px; padding: 8px 14px; font-size: 13px;"
        ));
    hallLayout->addWidget(statsLabel);
    auto *selectionBar = new QHBoxLayout;
    selectionLabel = new QLabel(hallPage);
    sellSelection = new QPushButton(tr("Продати вибрані"), hallPage);
    reserveSelection = new QPushButton(tr("Забронювати вибрані"), hallPage);
    selectionBar->addWidget(selectionLabel, 1);
    selectionBar->addWidget(sellSelection);
    selectionBar->addWidget(reserveSelection);
    hallLayout->addLayout(selectionBar);
    connect(sellSelection, &QPushButton::clicked, this, [this] { completeSelection(true); });
    connect(reserveSelection, &QPushButton::clicked, this, [this] { completeSelection(false); });
    updateSelection();

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
    tabs->setTabEnabled(1, false);
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



    connect(ui->events, &QTableWidget::cellDoubleClicked, this, &MainWindow::openEventHall);
    connect(backBtn, &QPushButton::clicked, this, [this]() { tabs->setCurrentIndex(0); });
    connect(tabs, &QTabWidget::currentChanged, this, &MainWindow::onTabChanged);
    connect(addEventBtn, &QPushButton::clicked, this, &MainWindow::onAddEventClicked);
    connect(editEventBtn, &QPushButton::clicked, this, &MainWindow::onEditEventClicked);
    connect(checkInBtn, &QPushButton::clicked, this, &MainWindow::onCheckInClicked);
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

    scheduleDate->calendarWidget()->setDateTextFormat(QDate(), QTextCharFormat());
    for(const auto &ev:events){
        QTextCharFormat mark;mark.setForeground(QColor("#c38e42"));mark.setFontWeight(QFont::Bold);
        scheduleDate->calendarWidget()->setDateTextFormat(QDate::fromString(ev.date,"dd.MM.yyyy"),mark);
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

    for (const Event &ev : events) {
        const QDate day = QDate::fromString(ev.date, "dd.MM.yyyy");
        const bool archived = ev.finished || QDateTime(day,QTime::fromString(ev.time,"HH:mm")).addSecs(ev.durationMinutes*60)<QDateTime::currentDateTime();
        if (archived != (scheduleFilter->currentIndex() == 1) ||
            (!allDates->isChecked() && day != scheduleDate->date()))
            continue;
        const int i = ui->events->rowCount();
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
    emptySchedule->setVisible(ui->events->rowCount() == 0);
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

    auto *dateEdit = new QDateEdit(isEdit ? QDate::fromString(eventData.date, "dd.MM.yyyy") : QDate::currentDate(), &dialog);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("dd.MM.yyyy");
    auto *timeEdit = new QTimeEdit(isEdit ? QTime::fromString(eventData.time, "HH:mm") : QTime(19, 0), &dialog);
    timeEdit->setDisplayFormat("HH:mm");

    auto *hallCombo = new QComboBox(&dialog);
    for (const auto &h : halls) {
        hallCombo->addItem(h.second, h.first);
    }
    if (isEdit) {
        const int hallIdx = hallCombo->findData(eventData.hallId);
        if (hallIdx >= 0) hallCombo->setCurrentIndex(hallIdx);

        if (database.hasActiveTicketsForEvent(eventData.id)) {
            hallCombo->setEnabled(false);
            hallCombo->setToolTip(QStringLiteral("Залу не можна змінити після першої квиткової операції."));
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
    auto *duration = new QSpinBox(&dialog);
    duration->setRange(1, 1440);
    duration->setSuffix(tr(" хв"));
    duration->setValue(120);
    if(isEdit){QSqlQuery q;q.prepare("SELECT duration FROM show_settings WHERE event_id=?");q.addBindValue(eventData.id);if(q.exec()&&q.next())duration->setValue(q.value(0).toInt());}
    form->addRow(tr("Тривалість:"),duration);
    form->addRow(QStringLiteral("Зала:"), hallCombo);
    form->addRow(QStringLiteral("Звичайна ціна:"), regPriceSpin);
    form->addRow(QStringLiteral("VIP ціна:"), vipPriceSpin);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Зберегти"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Скасувати"));
    auto *finished = new QCheckBox(tr("Завершити показ і перенести в архів"), &dialog);
    finished->setChecked(eventData.finished);
    if (isEdit)
        form->addRow(tr("Стан показу:"), finished);
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

    eventData.durationMinutes = duration->value();
    eventData.finished = finished->isChecked();
    eventData.kind = kindCombo->currentText();
    eventData.title = titleEdit->text().trimmed();
    eventData.date = dateEdit->date().toString("dd.MM.yyyy");
    eventData.time = timeEdit->time().toString("HH:mm");
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

    if (currentEventId == targetEvent.id) {
        currentEventId = 0;
        tabs->setTabEnabled(1, false);
    }
    refreshEvents();
}

// ============================================================================
// ВІДКРИТТЯ СХЕМИ ЗАЛИ, ПІДРАХУНОК СТАТИСТИКИ ТА МАЛЮВАННЯ МІСЦЬ
// ============================================================================
void MainWindow::openEventHall(int row, int)
{
    if (scheduleFilter->currentIndex() == 1) {
        QMessageBox::information(this, tr("Архів показів"), tr("Для змін виберіть «Редагувати подію». Дані показу доступні у звіті."));
        return;
    }
    auto *idItem = ui->events->item(row, 0);
    if (!idItem)
        return;

    tabs->setTabEnabled(1, true);
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

    selectedPrices.clear();
    updateSelection();
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

    // Підрахунок статистики по залі та виручці (Етап 6)
    const int totalSeats = seats.size();
    int freeCount = 0;
    int reservedCount = 0;
    int soldCount = 0;
    int checkedInCount = 0;
    qint64 totalRevenueKopiyky = 0;

    int maxSeatsInRow = 0;
    int maxRow = 0;
    QMap<int, int> rowCounts;

    for (const Seat &seat : seats) {
        rowCounts[seat.rowNumber]++;
        if (rowCounts[seat.rowNumber] > maxSeatsInRow)
            maxSeatsInRow = rowCounts[seat.rowNumber];
        if (seat.rowNumber > maxRow)
            maxRow = seat.rowNumber;

        if (seat.status == SeatStatus::Free) {
            freeCount++;
        } else if (seat.status == SeatStatus::Reserved) {
            reservedCount++;
        } else if (seat.status == SeatStatus::Sold) {
            soldCount++;
            totalRevenueKopiyky += seat.paidKopiyky;
            if (!seat.checkedInAt.isEmpty()) {
                checkedInCount++;
            }
        }
    }
    currentMaxRow = maxRow;

    if (statsLabel) {
        const QString revenueStr = QString::number(totalRevenueKopiyky / 100.0, 'f', 2);
        statsLabel->setText(QStringLiteral(
                                "Всього місць: <b>%1</b> &nbsp;|&nbsp; "
                                "<span style='color:#2ecc71;'>Вільно: <b>%2</b></span> &nbsp;|&nbsp; "
                                "<span style='color:#f1c40f;'>Заброньовано: <b>%3</b></span> &nbsp;|&nbsp; "
                                "<span style='color:#e74c3c;'>Продано: <b>%4</b></span> &nbsp;|&nbsp; "
                                "<span style='color:#5dade2;'>У залі (Check-in): <b>%5 / %4</b></span> &nbsp;|&nbsp; "
                                "<span style='color:#f0c674;'>Виручка: <b>%6 грн</b></span>")
                                .arg(totalSeats).arg(freeCount).arg(reservedCount)
                                .arg(soldCount).arg(checkedInCount).arg(revenueStr)
                            + tr(" · Заповненість: %1").arg(totalSeats
                                ? QString::number(100.0 * (reservedCount + soldCount) / totalSeats, 'f', 1) + "%"
                                : tr("немає місць")));
    }

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
            const QString checkInfo = seat.checkedInAt.isEmpty()
            ? QStringLiteral("Очікується на вході")
            : QStringLiteral("У залі з %1").arg(seat.checkedInAt);

            btn->setToolTip(QStringLiteral("Ряд %1, Місце %2 — ПРОДАНО\nКод: %3\nСтатус входу: %4")
                                .arg(seat.rowNumber).arg(seat.seatNumber).arg(seat.ticketCode, checkInfo));
            btn->setStyleSheet(QStringLiteral(
                "QPushButton { background: #c0392b; color: #ffffff; font-weight: bold; border-radius: 6px; border: 1px solid #e74c3c; }"
                "QPushButton:hover { background: #d64535; }"
                ));
        }

        if (seat.status == SeatStatus::Free) {
            btn->setCheckable(true);
            btn->setStyleSheet(btn->styleSheet() + "QPushButton:checked {background:#f6dc83;color:#181921;border:2px solid white;}");
            const int price = isLastSofaRow ? currentVipPriceKopiyky : currentRegularPriceKopiyky;
            connect(btn, &QPushButton::toggled, this, [this, seat, price](bool checked) {
                if (checked)
                    selectedPrices.insert(seat.id, price);
                else
                    selectedPrices.remove(seat.id);
                updateSelection();
            });
        } else {
            connect(btn, &QPushButton::clicked, this, [this, seat] { onSeatClicked(seat); });
        }

        seatsGrid->addWidget(btn, seat.rowNumber - 1, gridCol);
    }
}

// ============================================================================
// ПРОДАЖ, БРОНЮВАННЯ, ВИКУП БРОНІ ТА ПЕРЕГЛЯД ЧЕКА
// ============================================================================
void MainWindow::onSeatClicked(const Seat &seat)
{
    if (currentEventId == 0)
        return;

    const bool isVip = (seat.rowNumber == currentMaxRow);
    const int priceKop = isVip ? currentVipPriceKopiyky : currentRegularPriceKopiyky;
    const QString priceStr = QString::number(priceKop / 100.0, 'f', 2);
    const QString seatType = isVip ? QStringLiteral(" (VIP)") : QString();

    if (seat.status == SeatStatus::Reserved) {
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

            showTicketFile(ticketCode);
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
    } else {
        const QString paidStr = QString::number(seat.paidKopiyky / 100.0, 'f', 2);
        const QString checkStatus = seat.checkedInAt.isEmpty()
                                        ? QStringLiteral("Очікується на вході")
                                        : QStringLiteral("Пройшов у залу (%1)").arg(seat.checkedInAt);

        QString details = QStringLiteral("Ряд %1%2, Місце %3 — ПРОДАНО\n\n"
                                         "Код квитка: %4\n"
                                         "Сплачено: %5 грн\n"
                                         "Час продажу: %6\n"
                                         "Контроль входу: %7")
                              .arg(seat.rowNumber).arg(seatType).arg(seat.seatNumber)
                              .arg(seat.ticketCode, paidStr, seat.soldAt, checkStatus);

        if (!seat.reservationName.isEmpty()) {
            details += QStringLiteral("\nБронював: %1").arg(seat.reservationName);
        }

        QMessageBox dialog(this);
        dialog.setWindowTitle(tr("Проданий квиток"));
        dialog.setText(details);
        auto *pdf = dialog.addButton(tr("PDF для друку"), QMessageBox::ActionRole);
        auto *refund = dialog.addButton(tr("Повернути квиток"), QMessageBox::DestructiveRole);
        refund->setEnabled(seat.checkedInAt.isEmpty());
        dialog.addButton(QMessageBox::Close);
        dialog.exec();
        if (dialog.clickedButton() == pdf)
            showTicketFile(seat.ticketCode);
        if (dialog.clickedButton() == refund &&
            QMessageBox::question(this, tr("Скасувати продаж"), tr("Повернути %1 грн і звільнити місце?").arg(paidStr),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) == QMessageBox::Yes) {
            QString error;
            if (!ticketService.refund(seat.ticketId, error))
                QMessageBox::warning(this, tr("Повернення"), error);
            renderHallSeats(currentEventId);
        }
    }
}

// ============================================================================
// ЕТАП 6: КОНТРОЛЬ ВХОДУ ЗА КОДОМ КВИТКА (CHECK-IN)
// ============================================================================
// ============================================================================
// ЕТАП 6: ОКРЕМЕ ВІКНО КОНТРОЛЮ ВХОДУ (CHECK-IN ТЕРМІНАЛ)
// ============================================================================
void MainWindow::updateSelection()
{
    qint64 total = 0;
    for (int price : selectedPrices)
        total += price;
    selectionLabel->setText(selectedPrices.isEmpty() ? tr("Виберіть вільні місця на схемі")
        : tr("Вибрано місць: %1 · Разом: %2 грн").arg(selectedPrices.size()).arg(total / 100.0, 0, 'f', 2));
    sellSelection->setEnabled(!selectedPrices.isEmpty());
    reserveSelection->setEnabled(!selectedPrices.isEmpty());
}

void MainWindow::completeSelection(bool sale)
{
    QString name;
    if (sale) {
        if (QMessageBox::question(this, tr("Підтвердити продаж"), selectionLabel->text(),
                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
            return;
    } else {
        bool ok;
        name = QInputDialog::getText(this, tr("Бронювання місць"), tr("Ім’я для всіх вибраних місць"),
                                     QLineEdit::Normal, {}, &ok).trimmed();
        if (!ok || name.isEmpty())
            return;
    }
    QString error;
    QStringList codes;
    if (!database.bookSeats(currentEventId, selectedPrices, name, codes, error)) {
        QMessageBox::warning(this, tr("Місця не оформлено"), error);
        renderHallSeats(currentEventId);
        return;
    }
    renderHallSeats(currentEventId);
    if (!sale)
        return;
    QStringList failed;
    QString folder;
    for (const QString &code : codes) {
        auto path = ticketService.writePdf(code, error);
        if (path.isEmpty())
            failed << code;
        else
            folder = QFileInfo(path).absolutePath();
    }
    if (!failed.isEmpty())
        QMessageBox::warning(this, tr("Продаж записано"), tr("Повторіть друк через продане місце для кодів: ") + failed.join(", "));
    if (!folder.isEmpty() && QMessageBox::question(this, tr("Квитки оформлено"),
            tr("Відкрити папку з PDF-квитками?")) == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

void MainWindow::showTicketFile(const QString &code)
{
    QString error;
    const QString path = ticketService.writePdf(code, error);
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("PDF квитка"), tr("Продаж записано. PDF можна створити повторно через продане місце.\n") + error);
        return;
    }
    if (QMessageBox::question(this, tr("Квиток для друку"), tr("PDF збережено:\n%1\n\nВідкрити для друку?").arg(path)) == QMessageBox::Yes)
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::showReport()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Звіт за показами"));
    dialog.resize(760, 560);
    auto *layout = new QVBoxLayout(&dialog);
    auto *view = new QTextBrowser(&dialog);
    auto *period = new QComboBox(&dialog);
    period->addItems({tr("Увесь період"), tr("За датою показу")});
    period->setCurrentIndex(1);
    QDate initialDay = QDate::currentDate();
    for (int row = 0; row < ui->events->rowCount(); ++row) {
        auto *cell = ui->events->item(row, 2);
        if (!cell)
            continue;
        const auto scheduled = QDate::fromString(cell->text(), "dd.MM.yyyy");
        if (scheduled >= initialDay) {
            initialDay = scheduled;
            break;
        }
    }
    auto *date = new QDateEdit(initialDay, &dialog);
    date->setCalendarPopup(true);
    date->setDisplayFormat("dd.MM.yyyy");
    auto *filters = new QHBoxLayout;
    filters->addWidget(period);
    filters->addWidget(date);
    filters->addStretch();
    layout->addLayout(filters);
    view->setStyleSheet("QTextBrowser{background:white;color:#202d35;padding:10px}");
    layout->addWidget(view);
    auto *save = new QPushButton(tr("Зберегти звіт у PDF"), &dialog);
    auto *print = new QPushButton(tr("Друкувати…"), &dialog);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    actions->addWidget(save);
    actions->addWidget(print);
    layout->addLayout(actions);
    auto reload = [&] {
        const bool daily = period->currentIndex() == 1;
        date->setEnabled(daily);
        QString error;
        view->setHtml(ticketService.reportHtml(error, daily ? date->date() : QDate{}));
        save->setEnabled(error.isEmpty());
        print->setEnabled(error.isEmpty());
        if (!error.isEmpty())
            view->setPlainText(error);
    };
    connect(period, &QComboBox::currentIndexChanged, &dialog, reload);
    connect(date, &QDateEdit::dateChanged, &dialog, reload);
    reload();
    connect(print, &QPushButton::clicked, &dialog, [&dialog, view] {
        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog choose(&printer, &dialog);
        if (choose.exec() == QDialog::Accepted)
            view->document()->print(&printer);
    });
    connect(save, &QPushButton::clicked, &dialog, [&dialog, view, date, period] {
        auto path = QFileDialog::getSaveFileName(&dialog, tr("Зберегти звіт"), period->currentIndex() == 1 ? "report-" + date->date().toString(Qt::ISODate) + ".pdf" : "report-all.pdf", "PDF (*.pdf)");
        if (path.isEmpty())
            return;
        QSaveFile file(path);
        if (file.open(QIODevice::WriteOnly)) {
            {
                QPdfWriter writer(&file);
                view->document()->print(&writer);
            }
            if (file.commit())
                return;
        }
        QMessageBox::warning(&dialog, QObject::tr("Запис PDF"), file.errorString());
    });
    dialog.exec();
}

void MainWindow::onCheckInClicked()
{
    if (currentEventId == 0)
        return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Термінал контролю входу (Check-in)"));
    dialog.setMinimumSize(480, 420);

    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(14);

    // Заголовок із назвою поточної події
    auto *eventTitleLbl = new QLabel(hallHeaderLabel->text(), &dialog);
    eventTitleLbl->setWordWrap(true);
    eventTitleLbl->setStyleSheet(QStringLiteral("font-size: 14px; font-weight: bold; color: #f0c674;"));
    layout->addWidget(eventTitleLbl);

    // Поле вводу коду + кнопка перевірки
    auto *inputRow = new QHBoxLayout();
    auto *codeEdit = new QLineEdit(&dialog);
    codeEdit->setPlaceholderText(QStringLiteral("Введіть або відскануйте код (напр. TKT-1-42-A8F3)..."));
    codeEdit->setMinimumHeight(38);
    codeEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { font-size: 14px; font-weight: bold; padding: 6px 12px; "
        "border: 2px solid #3a3f4d; border-radius: 6px; background: #1e2128; color: #ffffff; }"
        "QLineEdit:focus { border-color: #2e86c1; }"
        ));

    auto *verifyBtn = new QPushButton(QStringLiteral("Перевірити"), &dialog);
    verifyBtn->setCursor(Qt::PointingHandCursor);
    verifyBtn->setMinimumHeight(38);
    verifyBtn->setDefault(true); // Спрацьовує по натисканню Enter
    verifyBtn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 0 18px; background: #2471a3; color: #ffffff; font-weight: bold; border-radius: 6px; }"
        "QPushButton:hover { background: #2e86c1; }"
        ));

    inputRow->addWidget(codeEdit);
    inputRow->addWidget(verifyBtn);
    layout->addLayout(inputRow);

    // Велика кольорова картка результату останньої перевірки
    auto *resultBanner = new QLabel(QStringLiteral("Очікування сканування квитка..."), &dialog);
    resultBanner->setAlignment(Qt::AlignCenter);
    resultBanner->setWordWrap(true);
    resultBanner->setMinimumHeight(105);
    resultBanner->setStyleSheet(QStringLiteral(
        "background: #23262e; color: #9aa2b1; border: 1px solid #353842; "
        "border-radius: 8px; padding: 12px; font-size: 13px;"
        ));
    layout->addWidget(resultBanner);

    // Журнал останніх проходів за поточну сесію контролю
    auto *historyTitle = new QLabel(QStringLiteral("Історія перевірок за сесію:"), &dialog);
    historyTitle->setStyleSheet(QStringLiteral("font-size: 12px; font-weight: bold; color: #9aa2b1;"));
    layout->addWidget(historyTitle);

    auto *historyTable = new QTableWidget(0, 3, &dialog);
    historyTable->setHorizontalHeaderLabels({
        QStringLiteral("Код квитка"),
        QStringLiteral("Статус"),
        QStringLiteral("Деталі")
    });
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setSelectionMode(QAbstractItemView::NoSelection);
    historyTable->setFocusPolicy(Qt::NoFocus);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    layout->addWidget(historyTable);

    // Нижня кнопка закриття
    auto *closeBtn = new QPushButton(QStringLiteral("Закрити термінал"), &dialog);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton { padding: 7px 16px; background: #2d313c; color: #ffffff; border-radius: 6px; }"
        "QPushButton:hover { background: #3a3f4d; }"
        ));
    layout->addWidget(closeBtn, 0, Qt::AlignRight);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    // Логіка обробки перевірки квитка (працює і по кнопці, і по Enter)
    auto performCheckIn = [&]() {
        const QString rawCode = codeEdit->text().trimmed();
        if (rawCode.isEmpty())
            return;

        const QString upperCode = rawCode.toUpper();
        const CheckInResult res = database.checkInTicket(currentEventId, upperCode);

        historyTable->insertRow(0);
        historyTable->setItem(0, 0, new QTableWidgetItem(upperCode));

        // Беремо перший рядок повідомлення для компактного запису в таблицю
        const QString shortMsg = res.message.section(QLatin1Char('\n'), 0, 0);

        if (res.success) {
            resultBanner->setText(res.message);
            resultBanner->setStyleSheet(QStringLiteral(
                "background: #194d33; color: #ffffff; border: 2px solid #2ecc71; "
                "border-radius: 8px; padding: 12px; font-size: 13px; font-weight: bold;"
                ));

            auto *statusItem = new QTableWidgetItem(QStringLiteral("✔ Дозволено"));
            statusItem->setForeground(QBrush(QColor(QStringLiteral("#2ecc71"))));
            historyTable->setItem(0, 1, statusItem);
            historyTable->setItem(0, 2, new QTableWidgetItem(res.message.section(QLatin1Char('\n'), 3)));

            // Одразу оновлюємо схему зали та лічильник "У залі (Check-in)" на фоні
            renderHallSeats(currentEventId);
        } else {
            resultBanner->setText(res.message);
            resultBanner->setStyleSheet(QStringLiteral(
                "background: #5c1e1e; color: #ffffff; border: 2px solid #e74c3c; "
                "border-radius: 8px; padding: 12px; font-size: 13px; font-weight: bold;"
                ));

            auto *statusItem = new QTableWidgetItem(QStringLiteral("✖ Відмова"));
            statusItem->setForeground(QBrush(QColor(QStringLiteral("#e74c3c"))));
            historyTable->setItem(0, 1, statusItem);
            historyTable->setItem(0, 2, new QTableWidgetItem(shortMsg));
        }

        // Очищаємо поле і повертаємо фокус для швидкого сканування наступного квитка
        codeEdit->clear();
        codeEdit->setFocus();
    };

    connect(verifyBtn, &QPushButton::clicked, &dialog, performCheckIn);
    connect(codeEdit, &QLineEdit::returnPressed, &dialog, performCheckIn);

    codeEdit->setFocus();
    dialog.exec();
}