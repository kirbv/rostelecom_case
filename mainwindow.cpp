#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "shapescene.h"
#include "shapecountdelegate.h"

#include "QHBoxLayout"
#include "QLineEdit"
#include "QStringList"
#include "QItemSelectionModel"
#include "QHash"
#include "QPalette"
#include "QPen"
#include "algorithm"
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
#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qitemselectionmodel.h>
#include <QtCore/qline.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qtypes.h>
#include <QtGui/qpalette.h>
#include <QtGui/qpolygon.h>
#include <QtSql/qsqldatabase.h>
#include <QtSql/qsqlquery.h>
#include <QtSql/qsqltablemodel.h>
#include <QtWidgets/qabstractitemview.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qformlayout.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qtableview.h>

namespace {

QString setupShapeCounts(QSqlDatabase database) {
    if (!database.transaction()) {
        return database.lastError().text();
    }

    const QStringList statements = {
        R"(CREATE TABLE IF NOT EXISTS shape_type_counts (
            type TEXT PRIMARY KEY NOT NULL,
            total INTEGER NOT NULL CHECK (total >= 0)
        ))",
        R"(INSERT INTO shape_type_counts (type, total)
            SELECT type, COUNT(*) FROM shapes GROUP BY type
            ON CONFLICT(type) DO UPDATE SET total = excluded.total)",
        R"(CREATE TRIGGER IF NOT EXISTS shape_count_after_insert
           AFTER INSERT ON shapes
           BEGIN
               INSERT INTO shape_type_counts (type, total) VALUES (NEW.type, 1)
               ON CONFLICT(type) DO UPDATE SET total = total + 1;
           END)",
        R"(CREATE TRIGGER IF NOT EXISTS shape_count_after_delete
           AFTER DELETE ON shapes
           BEGIN
               UPDATE shape_type_counts SET total = total - 1 WHERE type = OLD.type;
           END)",
        R"(CREATE VIEW IF NOT EXISTS shapes_with_counts AS
           SELECT s.id, s.type, s.width, s.height, s.x, s.y, c.total AS type_count
           FROM shapes AS s
           JOIN shape_type_counts AS c ON c.type = s.type)"
    };

    QSqlQuery query(database);
    for (const QString &statement : statements) {
        if (!query.exec(statement)) {
            const QString error = query.lastError().text();
            database.rollback();
            return error;
        }
    }

    if (!database.commit()) {
        const QString error = database.lastError().text();
        database.rollback();
        return error;
    }

    return {};
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("Редактор фигур"));

    auto *scene = new ShapeScene(this);
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

    auto *linkButton = new QPushButton(tr("Связать две фигуры"));
    linkButton->setEnabled(false);
    form->addRow(linkButton);

    connect(scene, &QGraphicsScene::selectionChanged, linkButton, [scene, linkButton] {
        linkButton->setEnabled(scene->selectedItems().size() == 2);
    });

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

    if (!query.exec("PRAGMA foreign_keys = ON") || !query.exec(R"(
        CREATE TABLE IF NOT EXISTS connections (
            first_id INTEGER NOT NULL REFERENCES shapes(id) ON DELETE CASCADE,
            second_id INTEGER NOT NULL REFERENCES shapes(id) ON DELETE CASCADE,
            PRIMARY KEY (first_id, second_id),
            CHECK (first_id < second_id)
        )
    )")) {
        showDatabaseError(query.lastError().text());
        return;
    }

    const QString countError = setupShapeCounts(database);
    if (!countError.isEmpty()) {
        showDatabaseError(countError);
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

        item->setFlag(QGraphicsItem::ItemIsSelectable);
        item->setFlag(QGraphicsItem::ItemIsMovable);
        item->setPos(x, y);
        item->setBrush(Qt::blue);
        return item;
    };

    if (!query.exec("SELECT x, y, width, height, type, id FROM shapes ORDER BY id")) {
        showDatabaseError(query.lastError().text());
        return;
    }

    while (query.next()) {
        const qreal x = query.value(0).toDouble();
        const qreal y = query.value(1).toDouble();
        const int width = query.value(2).toInt();
        const int height = query.value(3).toInt();
        const QString type = query.value(4).toString();
        const qint64 id = query.value(5).toLongLong();
        auto *item = drawShape(type, x, y, width, height);
        scene->registerShape(id, item);
    }
    query.finish();

    const auto drawConnection = [scene, view](QGraphicsItem *first, QGraphicsItem *second) {
        scene->addConnection(first, second, QPen(view->palette().color(QPalette::Text), 2));
    };

    if (!query.exec("SELECT first_id, second_id FROM connections")) {
        showDatabaseError(query.lastError().text());
        return;
    }
    while (query.next()) {
        auto *first = scene->shapeById(query.value(0).toLongLong());
        auto *second = scene->shapeById(query.value(1).toLongLong());
        if (first && second) {
            drawConnection(first, second);
        }
    }
    query.finish();

    auto *model = new QSqlTableModel(table, database);
    model->setTable("shapes_with_counts");
    model->setSort(0, Qt::AscendingOrder);
    if (!model->select()) {
        showDatabaseError(model->lastError().text());
        return;
    }

    model->setHeaderData(1, Qt::Horizontal, tr("Тип"));
    model->setHeaderData(2, Qt::Horizontal, tr("Ширина"));
    model->setHeaderData(3, Qt::Horizontal, tr("Высота"));
    model->setHeaderData(6, Qt::Horizontal, tr("Всего этого типа"));

    table->setModel(model);
    table->setItemDelegateForColumn(6, new ShapeCountDelegate(table));
    table->hideColumn(4);
    table->hideColumn(5);

    auto *idFilter = new QLineEdit;
    idFilter->setPlaceholderText(tr("ID фигуры"));

    auto *typeFilter = new QComboBox;
    typeFilter->addItem(tr("Все типы"));
    for (int i = 0; i < typeInput->count(); ++i) {
        typeFilter->addItem(typeInput->itemText(i), typeInput->itemData(i));
    }

    auto *countFilter = new QLineEdit;
    countFilter->setPlaceholderText(tr("Количество по типу"));

    auto *applyFilterButton = new QPushButton(tr("Применить"));
    auto *clearFilterButton = new QPushButton(tr("Сбросить"));
    auto *filterRow = new QHBoxLayout;
    filterRow->addWidget(idFilter);
    filterRow->addWidget(typeFilter);
    filterRow->addWidget(countFilter);
    filterRow->addWidget(applyFilterButton);
    filterRow->addWidget(clearFilterButton);
    layout->insertLayout(layout->count() - 1, filterRow);

    const auto applyFilter = [=] {
        QStringList conditions;

        const auto addNumberCondition = [&conditions](QLineEdit *field, const QString &column) {
            const QString text = field->text().trimmed();
            if (text.isEmpty()) {
                return true;
            }

            bool ok = false;
            const qint64 value = text.toLongLong(&ok);
            if (!ok || value < 1) {
                field->setFocus();
                return false;
            }

            conditions.append(column + "=" + QString::number(value));
            return true;
        };

        if (!addNumberCondition(idFilter, "id") || !addNumberCondition(countFilter, "type_count")) {
            QMessageBox::warning(this, tr("Фильтр"), tr("Введите целое положительное число."));
            return;
        }

        const QString type = typeFilter->currentData().toString();
        if (!type.isEmpty()) {
            conditions.append(QStringLiteral("type = '%1'").arg(type));
        }

        model->setFilter(conditions.join(" AND "));
        if (model->lastError().isValid()) {
            showDatabaseError(model->lastError().text());
        }
    };

    connect(applyFilterButton, &QPushButton::clicked, this, applyFilter);
    connect(clearFilterButton, &QPushButton::clicked, this, [=] {
        idFilter->clear();
        typeFilter->setCurrentIndex(0);
        countFilter->clear();
        applyFilter();
    });

    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *deleteButton = new QPushButton(tr("Удалить выбранную фигуру"));
    deleteButton->setEnabled(false);
    form->addRow(deleteButton);

    auto *selection = table->selectionModel();
    connect(selection, &QItemSelectionModel::selectionChanged, deleteButton, [selection, deleteButton] {
        deleteButton->setEnabled(!selection->selectedRows().isEmpty());
    });
    connect(model, &QAbstractItemModel::modelReset, deleteButton, [deleteButton] {
        deleteButton->setEnabled(false);
    });

    connect(deleteButton, &QPushButton::clicked, this, [=] {
        const auto rows = selection->selectedRows(0);
        if (rows.isEmpty()) return;
        const qint64 id = rows.first().data().toLongLong();

        QSqlQuery remove(database);
        if (!remove.prepare("DELETE FROM shapes WHERE id = ?")) {
            showDatabaseError(remove.lastError().text());
            return;
        }

        remove.addBindValue(id);
        if (!remove.exec()) {
            showDatabaseError(remove.lastError().text());
            return;
        }

        scene->removeShape(id);
        if (!model->select()) {
            showDatabaseError(model->lastError().text());
        }
    });

    connect(scene, &ShapeScene::dragFinished, this, [database, scene, showDatabaseError](const QHash<QGraphicsItem *, QPointF> &originalPositions) mutable {
        const auto restorePositions = [&](const QString &message) {
            for (auto it = originalPositions.cbegin(); it != originalPositions.cend(); ++it) {
                it.key()->setPos(it.value());
            }
            scene->updateConnections();
            showDatabaseError(message);
        };

        if (!database.transaction()) {
            restorePositions(database.lastError().text());
            return;
        }

        QSqlQuery update(database);
        if (!update.prepare("UPDATE shapes SET x = ?, y = ? WHERE id = ?")) {
            database.rollback();
            restorePositions(update.lastError().text());
            return;
        }

        for (auto it = originalPositions.cbegin(); it != originalPositions.cend(); ++it) {
            auto *item = it.key();
            update.bindValue(0, item->pos().x());
            update.bindValue(1, item->pos().y());
            update.bindValue(2, item->data(0).toLongLong());

            if (!update.exec()) {
                const QString error = update.lastError().text();
                database.rollback();
                restorePositions(error);
                return;
            }
        }

        if (!database.commit()) {
            const QString error = database.lastError().text();
            database.rollback();
            restorePositions(error);
        }
    });

    connect(button, &QPushButton::clicked, this, [=]() mutable {
        const QString type = typeInput->currentData().toString();
        const int width = widthInput->value();
        const int height = heightInput->value();
        const qreal x = qMax(qreal(20), scene->itemsBoundingRect().right() + 20);
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
        insert.addBindValue(x);
        insert.addBindValue(y);
        if (!insert.exec()) {
            showDatabaseError(insert.lastError().text());
            return;
        }

        auto *item = drawShape(type, x, y, width, height);
        scene->registerShape(insert.lastInsertId().toLongLong(), item);
        view->ensureVisible(item);
        if (!model->select()) {
            showDatabaseError(model->lastError().text());
        }
    });

    connect(linkButton, &QPushButton::clicked, this, [=] {
        const auto selected = scene->selectedItems();
        if (selected.size() != 2) {
            return;
        }

        const qint64 firstId = selected[0]->data(0).toLongLong();
        const qint64 secondId = selected[1]->data(0).toLongLong();

        QSqlQuery insert(database);
        if (!insert.prepare(
            "INSERT INTO connections (first_id, second_id) VALUES (?, ?) "
            "ON CONFLICT(first_id, second_id) DO NOTHING"
        )) {
            showDatabaseError(insert.lastError().text());
            return;
        }
        insert.addBindValue(std::min(firstId, secondId));
        insert.addBindValue(std::max(firstId, secondId));
        if (!insert.exec()) {
            showDatabaseError(insert.lastError().text());
            return;
        }
        if (insert.numRowsAffected() == 1) {
            drawConnection(selected[0], selected[1]);
        }
    });

    button->setEnabled(true);
}

MainWindow::~MainWindow()
{
    delete ui;
}