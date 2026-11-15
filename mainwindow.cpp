#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "QDir"
#include "QFormLayout"
#include "QGraphicsItem"
#include "QComboBox"
#include "QPolygonF"
#include "QGraphicsScene"
#include "QGraphicsView"
#include "QMessageBox"
#include "QPushButton"
#include "QSpinBox"
#include "QSqlDatabase"
#include "QSqlError"
#include "QSqlQuery"
#include "QSqlTableModel"
#include "QStandardPaths"
#include "QTableView"
#include "QVBoxLayout"
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtypes.h>
#include <QtGui/qpolygon.h>
#include <QtSql/qsqldatabase.h>
#include <QtSql/qsqlquery.h>
#include <QtSql/qsqltablemodel.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qtableview.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("Редактор фигур"));

    auto *scene = new QGraphicsScene(this);
    auto *view = new QGraphicsView(scene);
    auto *table = new QTableView;
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    auto *widthInput = new QSpinBox;
    widthInput->setRange(1, 1000);
    widthInput->setValue(120);

    auto *heightInput = new QSpinBox;
    heightInput->setRange(1, 1000);
    heightInput->setValue(80);

    auto *typeInput = new QComboBox;
    typeInput->addItem(tr("Прямоугольник"), QStringLiteral("rectangle"));
    typeInput->addItem(tr("Эллипс"), QStringLiteral("ellipse"));
    typeInput->addItem(tr("Шестиугольник"), QStringLiteral("hexagon"));
    auto *button = new QPushButton(tr("Добавить фигуру"));
    button->setEnabled(false);

    auto *form = new QFormLayout;
    form->addRow(tr("Тип фигуры"), typeInput);
    form->addRow(tr("Ширина"), widthInput);
    form->addRow(tr("Высота"), heightInput);
    form->addRow(button);

    auto *layout = new QVBoxLayout(centralWidget());
    layout->addLayout(form);
    layout->addWidget(view, 2);
    layout->addWidget(table, 1);

    const auto showDatabaseError = [this](const QString &message) {
        QMessageBox::critical(this, tr("Ошибка SQLite"), message);
    };

    const QString directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir().mkpath(directory)) {
        showDatabaseError(tr("Не удалось создать папку для базы данных."));
        return;
    }

    auto database = QSqlDatabase::addDatabase("QSQLITE");
    database.setDatabaseName(QDir(directory).filePath("shapes.sqlite"));
    if (!database.open()) {
        showDatabaseError((database.lastError().text()));
        return;
    }

    QSqlQuery query(database);
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS shapes (
            id INTEGER PRIMARY KEY,
            type TEXT NOT NULL,
            width INTEGER NOT NULL CHECK (width > 0),
            height INTEGER NOT NULL CHECK (height > 0),
            x INTEGER NOT NULL,
            y INTEGER NOT NULL
        )
    )")) {
        showDatabaseError(query.lastError().text());
        return;
    }

    const auto drawShape = [scene](const QString &type, qreal x, qreal y, qreal width, qreal height) {
        QAbstractGraphicsShapeItem *item;

        if (type == "ellipse") {
            item = scene->addEllipse(0, 0, width, height);
        } else if (type == "hexagon") {
            QPolygonF points;
            points << QPointF(width / 4, 0) << QPointF(3 * width / 4, 0) << QPointF(width, height / 2) << QPointF(3 * width / 4, height) << QPointF(width / 4, height) << QPointF(0, height / 2);
            item = scene->addPolygon(points);
        } else {
            item = scene->addRect(0, 0, width, height);
        }

        item->setPos(x, y);
        item->setBrush(Qt::blue);
        return item;
    };

    if (!query.exec("SELECT x, y, width, height, type FROM shapes ORDER BY id")) {
        showDatabaseError(query.lastError().text());
        return;
    }

    qint64 nextX = 20;
    while (query.next()) {
        const qint64 x = query.value(0).toLongLong();
        const int y = query.value(1).toInt();
        const int width = query.value(2).toInt();
        const int height = query.value(3).toInt();
        const QString type = query.value(4).toString();
        drawShape(type, x, y, width, height);
        nextX = x + width + 20;
    }
    query.finish();

    auto *model = new QSqlTableModel(table, database);
    model->setTable("shapes");
    if (!model->select()) {
        showDatabaseError(model->lastError().text());
        return;
    }

    model->setHeaderData(1, Qt::Horizontal, tr("Тип"));
    model->setHeaderData(2, Qt::Horizontal, tr("Ширина"));
    model->setHeaderData(3, Qt::Horizontal, tr("Высота"));
    table->setModel(model);
    table->hideColumn(4);
    table->hideColumn(5);

    connect(button, &QPushButton::clicked, this, [=]() mutable {
        const QString type = typeInput->currentData().toString();
        const int width = widthInput->value();
        const int height = heightInput->value();
        const int y = 20;

        QSqlQuery insert(database);
        if (!insert.prepare(
                "INSERT INTO shapes (type, width, height, x, y) VALUES (?, ?, ?, ?, ?)")) {
            showDatabaseError(insert.lastError().text());
            return;
        }
        insert.addBindValue(type);
        insert.addBindValue(width);
        insert.addBindValue(height);
        insert.addBindValue(nextX);
        insert.addBindValue(y);
        if (!insert.exec()) {
            showDatabaseError(insert.lastError().text());
            return;
        }

        auto *item = drawShape(type, nextX, y, width, height);
        nextX += width + 20;
        view->ensureVisible(item);
        if (!model->select()) {
            showDatabaseError(model->lastError().text());
        }
    });
    button->setEnabled(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}