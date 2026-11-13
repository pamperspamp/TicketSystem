#pragma once

#include "database.h"
#include <QMainWindow>

class QGridLayout;
class QLabel;
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

private slots:
    void openEventHall(int row, int column);
    void onTabChanged(int index);
    void onSeatClicked(const Seat &seat);

private:
    void setupTabs();
    void refreshEvents();
    void renderHallSeats(int eventId);

    Ui::MainWindow *ui;
    Database &database;

    QTabWidget *tabs = nullptr;
    QLabel *hallHeaderLabel = nullptr;
    QLabel *screenLabel = nullptr;
    QGridLayout *seatsGrid = nullptr;

    int currentEventId = 0;
    int currentRegularPriceKopiyky = 0;
    int currentVipPriceKopiyky = 0;
    int currentMaxRow = 0;
};