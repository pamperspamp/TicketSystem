# Obrii

A Windows desktop application for managing events, seat reservations, ticket sales, and admission. Built as a C++17 object-oriented programming coursework project with Qt Widgets and SQLite.

The application interface is in Ukrainian. The venue name, **Обрій (Obrii)**, means **Horizon**.

## Features

- Event creation and editing with date, time, duration, and hall selection.
- Schedule overlap checks and an archive for completed shows.
- Calendar filters, highlighted event dates, and navigation to the nearest show.
- Interactive seating plans with regular and VIP prices.
- Selection and sale or reservation of multiple seats in one transaction.
- Reservation search by customer name and buyout by a unique reservation ID.
- Ticket refunds before admission, with the original sale retained in history.
- Individual A5 PDF tickets with QR codes and a printable text code.
- Admission validation with checks for the correct show, payment, and previous entry.
- Reports filtered by show date, including occupancy, sales, refunds, and net revenue.
- Report export to PDF and printing through the Windows print dialog.

## Run the Windows application

Download the Windows ZIP package from [Releases](https://github.com/pamperspamp/TicketSystem/releases).

1. Extract the entire archive into a writable folder.
2. Run **Obrii.exe**.
3. Keep the executable, Qt libraries, plugin folders, and database together.

The portable build stores **tickets.db** beside the executable. Ticket PDFs are saved in the **tickets** folder beside that database. The database contains sample events and transactions for demonstration.

Back up **tickets.db** while the application is closed before replacing an existing installation. Updating the executable can preserve the existing database.

## Build in Qt Creator

The project has been built with Qt 6.12.0 and MinGW 64-bit on Windows.

Required tools and modules:

- Qt Creator
- A C++17 compiler compatible with the installed Qt build
- CMake 3.16 or later
- Qt Widgets, Sql, and PrintSupport
- The QSQLITE driver

Open **CMakeLists.txt** in Qt Creator, select the Desktop Qt MinGW kit, configure the project, and build it.

By default, the development build opens **tickets.db** in the source directory. Enable the CMake option **COURSE_PORTABLE=ON** to use a database beside the executable.

For a distributable package, build in Release mode with that option enabled, place the executable in a separate directory, and deploy its dependencies with the matching Qt installation:

~~~powershell
$env:Path = "C:\Qt\Tools\mingw1310_64\bin;" + $env:Path
& "C:\Qt\6.12.0\mingw_64\bin\windeployqt.exe" --release "D:\Obrii\Obrii.exe"
~~~

Adjust the paths to your installation. Include the database and the required MinGW runtime DLLs in the package, then test it outside Qt Creator.

## Typical workflow

1. Select an event date in the schedule and open a show by double-clicking it.
2. Select available seats and confirm a sale or reservation.
3. Open the generated PDF tickets for printing.
4. Use the admission screen to enter or scan a ticket code.
5. Open **Звіт за показами** to select a show date and print or export its report.

A QR code contains the ticket's text code. A USB scanner operating as a keyboard can enter it into the admission field. Validation uses the local database.

To refund an unused ticket, open its sold seat and choose the refund action. The ticket becomes invalid and its seat becomes available again.

## Project structure

| File | Purpose |
| --- | --- |
| main.cpp | Application startup, database initialization, and application icon |
| mainwindow.ui | Base window layout edited in Qt Designer |
| mainwindow.h / mainwindow.cpp | Schedule, seating plan, dialogs, and user actions |
| database.h / database.cpp | SQLite schema, queries, reservations, sales, and admission |
| ticketservice.h / ticketservice.cpp | Refunds, PDF tickets, QR rendering, and reports |
| event.h / seat.h | Event and seat data structures |
| assets / resources.qrc / app.rc | Branding and Windows application icon |
| third_party/qrcodegen | Bundled QR Code generator library |
| tickets.db | Local database with demonstration data |

## Reporting and reservation rules

Reports select shows by their scheduled date and display their current recorded totals, including advance sales and subsequent refunds. Reservations count toward occupied seats. Admission counts are shown separately.

Reservations expire 30 minutes before the show starts. Expired reservations are cleared when the application starts. Buyout also checks the reservation deadline and show status.

## Third-party component

QR generation uses [Project Nayuki's QR Code generator](https://github.com/nayuki/QR-Code-generator), version 1.8.0, under the MIT License. Its copyright and license notice are included in the bundled source files.

## Project scope

This coursework application maintains a local box-office ledger. The cashier records payments and returns money to customers. Generated tickets are marked as educational samples. Built-in camera scanning and online payment processing are outside the current implementation.