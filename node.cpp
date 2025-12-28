#include "node.h"
#include "edge.h"
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsScene>

Node::Node(int id) : id(id), currentColor(defaultColor)
{
    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setZValue(1);
}

void Node::addEdge(Edge *edge) {
    edgeList << edge;
    edge->adjust();
}

void Node::setColor(const QColor &color) {
    currentColor = color;
    update(); // Перерисовать
}

void Node::resetColor() {
    currentColor = defaultColor;
    update();
}

QRectF Node::boundingRect() const {
    return QRectF(-20, -20, 40, 40);
}

void Node::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *) {
    // Тень
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::darkGray);
    painter->drawEllipse(-18, -18, 40, 40);

    // Градиент
    QRadialGradient gradient(-3, -3, 20);
    if (option->state & QStyle::State_Sunken) {
        gradient.setCenter(3, 3);
        gradient.setFocalPoint(3, 3);
        gradient.setColorAt(1, currentColor.lighter(120));
        gradient.setColorAt(0, currentColor.darker(120));
    } else {
        gradient.setColorAt(0, currentColor);
        gradient.setColorAt(1, currentColor.darker(110));
    }

    painter->setBrush(gradient);
    painter->setPen(QPen(Qt::black, 0));
    painter->drawEllipse(-20, -20, 40, 40);

    // ID
    painter->setPen(Qt::white);
    painter->setFont(QFont("Arial", 10, QFont::Bold));
    painter->drawText(boundingRect(), Qt::AlignCenter, QString::number(id + 1));
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionHasChanged) {
        for (Edge *edge : edgeList) edge->adjust();
    }
    return QGraphicsItem::itemChange(change, value);
}

void Node::mousePressEvent(QGraphicsSceneMouseEvent *event) {
    update();
    QGraphicsItem::mousePressEvent(event);
}

void Node::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) {
    update();
    QGraphicsItem::mouseReleaseEvent(event);
}

QList<Edge *> Node::edges() const { return edgeList; }
