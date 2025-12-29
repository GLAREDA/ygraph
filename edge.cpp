#include "edge.h"
#include "node.h"
#include <QPainter>
#include <QtMath>

Edge::Edge(Node *sourceNode, Node *destNode, int weight)
    : source(sourceNode), dest(destNode), weight(weight), currentColor(Qt::black)
{
    setAcceptedMouseButtons(Qt::NoButton);
    source->addEdge(this);
    dest->addEdge(this);
    setFlag(ItemIsSelectable);
    adjust();
}

void Edge::setColor(const QColor &color) {
    currentColor = color;
    update();
}

void Edge::resetColor() {
    currentColor = Qt::black;
    update();
}

void Edge::adjust()
{
    if (!source || !dest) return;
    QLineF line(mapFromItem(source, 0, 0), mapFromItem(dest, 0, 0));
    qreal length = line.length();

    prepareGeometryChange();
    if (length > 40) { // Отступ от центра узла (чтобы линия не заходила внутрь)
        QPointF edgeOffset((line.dx() * 20) / length, (line.dy() * 20) / length);
        sourcePoint = line.p1() + edgeOffset;
        destPoint = line.p2() - edgeOffset;
    } else {
        sourcePoint = destPoint = line.p1();
    }
}

QRectF Edge::boundingRect() const
{
    if (!source || !dest) return QRectF();
    return QRectF(sourcePoint, QSizeF(destPoint.x() - sourcePoint.x(),
                                      destPoint.y() - sourcePoint.y()))
        .normalized().adjusted(-10, -10, 10, 10); // Запас для текста
}

void Edge::paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
    if (!source || !dest) return;
    QLineF line(sourcePoint, destPoint);
    if (qFuzzyCompare(line.length(), qreal(0.))) return;

    // Рисуем линию
    painter->setPen(QPen(currentColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(line);

    // Рисуем вес, если он больше 1
    if (weight > 1) {
        QPointF center = (sourcePoint + destPoint) / 2;
        painter->setPen(Qt::black);
        // Рисуем белый фон под текстом, чтобы читалось
        painter->setBrush(Qt::white);
        QRectF textRect(center.x()-10, center.y()-10, 20, 20);
        painter->drawRect(textRect);
        painter->drawText(textRect, Qt::AlignCenter, QString::number(weight));
    }
}
