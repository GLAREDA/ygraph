#include "edge.h"
#include "node.h"
#include <QPainter>
#include <QtMath>

Edge::Edge(Node *sourceNode, Node *destNode, int weight, bool isDirected)
    : source(sourceNode), dest(destNode), weight(weight), currentColor(Qt::black), isDirected(isDirected)
{
    setAcceptedMouseButtons(Qt::NoButton);
    setFlag(ItemIsSelectable); // Чтобы можно было удалять
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
    update();
}

void Edge::adjust()
{
    if (!source || !dest) return;

    QLineF line(mapFromItem(source, 0, 0), mapFromItem(dest, 0, 0));
    qreal length = line.length();

    prepareGeometryChange();

    // Отступ от центра узла (радиус узла = 20 + запас)
    double nodeRadius = 25.0;

    if (length > nodeRadius * 2) {
        // Сдвигаем точки начала и конца, чтобы линия касалась границ кругов
        QPointF edgeOffset((line.dx() * nodeRadius) / length, (line.dy() * nodeRadius) / length);
        sourcePoint = line.p1() + edgeOffset;
        destPoint = line.p2() - edgeOffset;
    } else {
        // Если узлы слишком близко, рисуем от центра (чтобы не исчезло)
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

    // 1. Рисуем саму линию
    painter->setPen(QPen(currentColor, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->drawLine(line);

    // 2. Рисуем стрелку (если ориентированный)
    if (isDirected) {
        double angle = std::atan2(-line.dy(), line.dx());

        // Точки стрелки
        QPointF arrowP1 = destPoint - QPointF(sin(angle + M_PI / 3) * arrowSize,
                                              cos(angle + M_PI / 3) * arrowSize);
        QPointF arrowP2 = destPoint - QPointF(sin(angle + M_PI - M_PI / 3) * arrowSize,
                                              cos(angle + M_PI - M_PI / 3) * arrowSize);

        painter->setBrush(currentColor); // Заливка цветом линии
        painter->drawPolygon(QPolygonF() << line.p2() << arrowP1 << arrowP2);
    }

    // 3. Рисуем вес
    if (weight > 1) {
        QPointF center = (sourcePoint + destPoint) / 2;
        painter->setPen(Qt::black);
        painter->setBrush(Qt::white);

        // Сдвигаем текст, чтобы не перекрывал стрелку на коротких ребрах
        if (isDirected) center = (sourcePoint * 0.4 + destPoint * 0.6);

        QRectF textRect(center.x()-10, center.y()-10, 20, 20);
        painter->drawRect(textRect);
        painter->drawText(textRect, Qt::AlignCenter, QString::number(weight));
    }
}
