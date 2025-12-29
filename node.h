#ifndef NODE_H
#define NODE_H

#include <QGraphicsObject> // <--- БЫЛО QGraphicsItem
#include <QList>
#include <QBrush>
#include <QColor>

class Edge;

// Наследуемся от QGraphicsObject для поддержки анимации
class Node : public QGraphicsObject
{
    Q_OBJECT // <--- ОБЯЗАТЕЛЬНО для анимации

public:
    Node(int id);

    void addEdge(Edge *edge);
    QList<Edge *> edges() const;
    int getId() const { return id; }

    void setColor(const QColor &color);
    void resetColor();

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
    QColor currentColor;
    QPointF newPos;
};

#endif // NODE_H
