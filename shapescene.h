#pragma once

#include "QGraphicsItem"
#include "QGraphicsScene"
#include "QGraphicsSceneMouseEvent"
#include "QHash"
#include "QList"
#include "QPen"
#include <QtCore/qhash.h>
#include <QtCore/qline.h>
#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qpoint.h>
#include <QtCore/qtmetamacros.h>
#include <QtGui/qpen.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicsscene.h>
#include <QtWidgets/qgraphicssceneevent.h>

class ShapeScene : public QGraphicsScene {
    Q_OBJECT
public:
    using QGraphicsScene::QGraphicsScene;

    void addConnection(QGraphicsItem *first, QGraphicsItem *second, const QPen &pen) {
        auto *line = addLine(QLineF(), pen);
        line->setZValue(-1);
        line->setAcceptedMouseButtons(Qt::NoButton);
        connections.append({first, second, line});
        updateConnections();
    }

    void updateConnections() {
        for (const auto &connection : connections) {
            connection.line->setLine(QLineF(
                connection.first->sceneBoundingRect().center(),
                connection.second->sceneBoundingRect().center()
            ));
        }
    }

signals:
    void dragFinished(const QHash<QGraphicsItem *, QPointF> &originalPositions);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override {
        auto *grabbed = mouseGrabberItem();
        const bool dragging = grabbed && grabbed->flags().testFlag(QGraphicsItem::ItemIsMovable) && event->buttons().testFlag(Qt::LeftButton);

        if (dragging && positionsBeforeDrag.isEmpty()) {
            for (auto *item : selectedItems()) {
                positionsBeforeDrag.insert(item, item->pos());
            }
            positionsBeforeDrag.insert(grabbed, grabbed->pos());
        }

        QGraphicsScene::mouseMoveEvent(event);
        if (dragging) {
            updateConnections();
        }
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override {
        QGraphicsScene::mouseReleaseEvent(event);
        if (event->button() != Qt::LeftButton) {
            return;
        }

        QHash<QGraphicsItem *, QPointF> moved;
        moved.swap(positionsBeforeDrag);
        for (auto it = moved.begin(); it != moved.end();) {
            if (it.key()->pos() == it.value()) {
                it = moved.erase(it);
            } else {
                ++it;
            }
        }

        if (!moved.isEmpty()) {
            emit dragFinished(moved);
        }
    }

private:
    struct Connection {
        QGraphicsItem *first;
        QGraphicsItem *second;
        QGraphicsLineItem *line;
    };

    QList<Connection> connections;
    QHash<QGraphicsItem *, QPointF> positionsBeforeDrag;
};