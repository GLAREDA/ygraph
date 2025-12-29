#include "node.h"
#include "edge.h"
#include <QPainter>
#include <QStyleOption>
#include <QGraphicsScene>

// Конструктор
Node::Node(int id) : id(id)
{
    // Инициализируем цвет СРАЗУ конкретным значением, а не ссылкой на другую переменную
    currentColor = QColor(70, 130, 180);

    setFlag(ItemIsMovable);
    setFlag(ItemSendsGeometryChanges);
    setCacheMode(DeviceCoordinateCache);
    setFlag(ItemIsSelectable);
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
    // Явно задаем стандартный цвет (SteelBlue)
    currentColor = QColor(70, 130, 180);
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

void Node::calculateForces()
{
    if (!scene() || scene()->mouseGrabberItem() == this) {
        newPos = pos();
        return;
    }

    qreal xvel = 0;
    qreal yvel = 0;

    // 1. ОТТАЛКИВАНИЕ (Закон Кулона)
    // Узлы толкают друг друга.
    foreach (QGraphicsItem *item, scene()->items()) {
        Node *node = dynamic_cast<Node *>(item);
        if (!node) continue;

        QPointF vec = mapToItem(node, 0, 0);
        qreal dx = vec.x();
        qreal dy = vec.y();
        double l = 2.0 * (dx * dx + dy * dy);

        if (l > 0) {
            // КОЭФФИЦИЕНТ: 200.0 (можно уменьшить до 100, если сильно разлетаются)
            xvel += (dx * 200.0) / l;
            yvel += (dy * 200.0) / l;
        }
    }

    // 2. ПРИТЯЖЕНИЕ РЕБЕР (Закон Гука)
    // Соединенные узлы стягиваются.
    double weight = (edgeList.size() + 1) * 40; // Жесткость пружины

    for (Edge *edge : edgeList) {
        QPointF vec;
        if (edge->sourceNode() == this)
            vec = mapToItem(edge->destNode(), 0, 0);
        else
            vec = mapToItem(edge->sourceNode(), 0, 0);

        xvel -= vec.x() / weight;
        yvel -= vec.y() / weight;
    }

    // 3. ГРАВИТАЦИЯ К ЦЕНТРУ (Центростремительная сила)
    // Это самая важная часть, чтобы они не улетали в бесконечность.
    QPointF currentPos = pos();

    // Делим на 100.0 (было 2000.0). Чем МЕНЬШЕ число, тем СИЛЬНЕЕ тянет в центр.
    // Это заставит их держаться вместе.
    xvel -= currentPos.x() / 200.0;
    yvel -= currentPos.y() / 200.0;

    // 4. ТРЕНИЕ (Затухание)
    // Чтобы они не болтались вечно, а успокаивались.
    // Если сила очень маленькая — обнуляем (остановка).
    if (qAbs(xvel) < 0.7 && qAbs(yvel) < 0.7) {
        xvel = yvel = 0;
    }

    // Применяем силу
    newPos = pos() + QPointF(xvel, yvel);
}

// === ФИЗИКА: ПРИМЕНЕНИЕ ПОЗИЦИИ ===
bool Node::advancePosition()
{
    if (newPos == pos())
        return false; // Узел не сдвинулся

    setPos(newPos);
    return true; // Узел сдвинулся
}



QList<Edge *> Node::edges() const { return edgeList; }
