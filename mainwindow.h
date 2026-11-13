#pragma once

#include "database.h"
#include "ticketservice.h"
#include <QMainWindow>

class QEvent;
class QDateEdit;
class QComboBox;
class QCheckBox;
class QGridLayout;
class QLabel;
class QPushButton;
class QTabWidget;

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(Database &db, QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void openEventHall(int row, int column);
    void onTabChanged(int index);
    void onSeatClicked(const Seat &seat);
    void onAddEventClicked();
    void onEditEventClicked();
    void onCheckInClicked();

private:
    void setupTabs();
    void updateSelection();
    void completeSelection(bool sale);
    QMap<int,int> selectedPrices;
    QLabel *selectionLabel = nullptr;
    QPushButton *sellSelection = nullptr;
    QPushButton *reserveSelection = nullptr;
    void showTicketFile(const QString &code);
    void showReport();
    TicketService ticketService;
    void refreshEvents();
    QDateEdit *scheduleDate = nullptr;
    QCheckBox *allDates = nullptr;
    QComboBox *scheduleFilter = nullptr;
    QLabel *emptySchedule = nullptr;
    void renderHallSeats(int eventId);
    bool showEventDialog(Event &eventData, bool isEdit);

    Ui::MainWindow *ui;
    Database &database;

    QTabWidget *tabs = nullptr;
    QLabel *hallHeaderLabel = nullptr;
    QLabel *statsLabel = nullptr;
    QLabel *screenLabel = nullptr;
    QGridLayout *seatsGrid = nullptr;

    int currentEventId = 0;
    int currentRegularPriceKopiyky = 0;
    int currentVipPriceKopiyky = 0;
    int currentMaxRow = 0;
};