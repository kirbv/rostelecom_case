#pragma once

#include <QApplication>
#include <QPainter>
#include <QPolygonF>
#include <QStyle>
#include <QStyledItemDelegate>

class ShapeCountDelegate : public QStyledItemDelegate {
    
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem cell(option);
        initStyleOption(&cell, index);
        cell.text.clear();

        const QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);

        const qint64 count = index.data().toLongLong();
        if (count <= 0) {
            return;
        }

        int icons = 1;
        if (count >= 11) {
            icons = 3;
        } else if (count >= 4) {
            icons = 2;
        }

        const QRect area = cell.rect.adjusted(6, 3, -6, -3);
        const int gap = 4;
        const int width = qMin(16, (area.width() - gap * (icons - 1)) / icons);
        const int height = qMin(12, area.height());
        if (width < 2 || height < 2) {
            return;
        }

        const int totalWidth = width * icons + gap * (icons - 1);
        const qreal x = area.x() + (area.width() - totalWidth) / 2.0;
        const qreal y = area.y() + (area.height() - height) / 2.0;
        const QString type = index.siblingAtColumn(1).data().toString();

        const auto role = cell.state.testFlag(QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;

        painter->save();
        painter->setClipRect(cell.rect, Qt::IntersectClip);
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(cell.palette.color(role), 1.5));
        painter->setBrush(Qt::NoBrush);

        for (int i = 0; i < icons; ++i) {
            const QRectF rect(x + i * (width + gap), y, width, height);

            if (type == "ellipse") {
                painter->drawEllipse(rect);
            } else if (type == "hexagon") {
                const qreal inset = rect.width() / 4;
                QPolygonF points;
                points << QPointF(rect.left() + inset, rect.top())
                       << QPointF(rect.right() - inset, rect.top())
                       << QPointF(rect.right(), rect.center().y())
                       << QPointF(rect.right() - inset, rect.bottom())
                       << QPointF(rect.left() + inset, rect.bottom())
                       << QPointF(rect.left(), rect.center().y());
                painter->drawPolygon(points);
            } else {
                painter->drawRect(rect);
            }
        }

        painter->restore();
    }
};