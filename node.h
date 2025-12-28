#ifndef NODE_H
#define NODE_H

#include <QGraphicsItem>
#include <QList>
#include <QBrush>
#include <QColor> // Добавлено

class Edge;

class Node : public QGraphicsItem
{
public:
    Node(int id);

    void addEdge(Edge *edge);
    QList<Edge *> edges() const;
    int getId() const { return id; }

    // Управление цветом
    void setColor(const QColor &color);
    void resetColor(); // Сброс к синему

    // Физика
    void calculateForces();
    bool advancePosition();

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

protected:
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QList<Edge *> edgeList;
    int id;
    QColor currentColor; // Текущий цвет
    QPointF newPos;      // Для физики
};

#endif // NODE_H
