#include "edge.h"
#include "node.h"
#include <QPainter>
#include <QtMath>

Edge::Edge(Node *sourceNode, Node *destNode, int weight, bool isDirected)
    : source(sourceNode), dest(destNode),
      weight(weight), currentColor(Qt::black),
      isDirected(isDirected), parallelOffset(0.0)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(ItemIsSelectable);
    source->addEdge(this);
    dest->addEdge(this);
    adjust();
}

void Edge::setColor(const QColor &color) {
    currentColor = color;
    update();
}

void Edge::resetColor() {
    currentColor = Qt::black;
    parallelOffset = 0.0;
    update();
}

void Edge::adjust()
{
    if (!source || !dest) return;

    QLineF line(mapFromItem(source, 0, 0), mapFromItem(dest, 0, 0));
    qreal length = line.length();

    prepareGeometryChange();

    double nodeRadius = 25.0;

    if (length > nodeRadius * 2) {
        QPointF edgeOffset(
            (line.dx() * nodeRadius) / length,
            (line.dy() * nodeRadius) / length
        );
        sourcePoint = line.p1() + edgeOffset;
        destPoint   = line.p2() - edgeOffset;
    } else {
        sourcePoint = destPoint = line.p1();
    }
}

QRectF Edge::boundingRect() const
{
    if (!source || !dest) return QRectF();

    // Учитываем возможное перпендикулярное смещение
    double extra = qAbs(parallelOffset) + 10.0;

    return QRectF(sourcePoint,
                  QSizeF(destPoint.x() - sourcePoint.x(),
                         destPoint.y() - sourcePoint.y()))
        .normalized()
        .adjusted(-extra, -extra, extra, extra);
}

void Edge::paint(QPainter *painter,
                 const QStyleOptionGraphicsItem *,
                 QWidget *)
{
    if (!source || !dest) return;

    // Базовые точки
    QLineF baseLine(sourcePoint, destPoint);
    if (qFuzzyCompare(baseLine.length(), qreal(0.0))) return;

    // ── Вычисляем перпендикулярное смещение ──────────────────────
    QPointF srcDraw = sourcePoint;
    QPointF dstDraw = destPoint;

    if (qAbs(parallelOffset) > 0.001) {
        double len = baseLine.length();
        // Единичный перпендикуляр (повёрнут на 90°)
        QPointF perp(-(destPoint.y() - sourcePoint.y()) / len,
                      (destPoint.x() - sourcePoint.x()) / len);

        srcDraw = sourcePoint + perp * parallelOffset;
        dstDraw = destPoint   + perp * parallelOffset;
    }

    QLineF line(srcDraw, dstDraw);

    // ── 1. Линия ──────────────────────────────────────────────────
    painter->setPen(QPen(currentColor, 2,
                         Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(line);

    // ── 2. Стрелка (ориентированный граф) ─────────────────────────
    if (isDirected) {
        double angle = std::atan2(-line.dy(), line.dx());

        QPointF arrowP1 = dstDraw - QPointF(
             sin(angle + M_PI / 3.0) * arrowSize,
             cos(angle + M_PI / 3.0) * arrowSize);
        QPointF arrowP2 = dstDraw - QPointF(
             sin(angle + M_PI - M_PI / 3.0) * arrowSize,
             cos(angle + M_PI - M_PI / 3.0) * arrowSize);

        painter->setBrush(currentColor);
        painter->drawPolygon(QPolygonF() << dstDraw << arrowP1 << arrowP2);
    }

    // ── 3. Вес ────────────────────────────────────────────────────
    if (weight > 1) {
        QPointF center = (srcDraw + dstDraw) / 2.0;
        if (isDirected)
            center = srcDraw * 0.4 + dstDraw * 0.6;

        painter->setPen(Qt::black);
        painter->setBrush(Qt::white);

        QRectF textRect(center.x() - 10, center.y() - 10, 20, 20);
        painter->drawRect(textRect);
        painter->drawText(textRect, Qt::AlignCenter, QString::number(weight));
    }
}
