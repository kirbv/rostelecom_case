#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "QGraphicsRectItem"
#include "QGraphicsScene"
#include "QGraphicsView"
#include "QPushButton"
#include "QVBoxLayout"
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicsview.h>
#include <QtWidgets/qpushbutton.h>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle(tr("Редактор фигур"));

    auto *scene = new QGraphicsScene(this);
    auto *view = new QGraphicsView(scene);
    auto *button = new QPushButton(tr("Добавить прямоугольник"));
    
    auto *layout = new QVBoxLayout(centralWidget());
    layout->addWidget(view, 1);
    layout->addWidget(button);

    connect(button, &QPushButton::clicked, this, [scene, view, nextX = 20]() mutable {
        auto *rectangle = scene->addRect(nextX, 20, 120, 80);
        rectangle->setBrush(Qt::blue);
        nextX += 140;
        view->ensureVisible(rectangle);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}