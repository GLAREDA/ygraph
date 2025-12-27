#ifndef EDGE_H
#define EDGE_H

#include <QGraphicsItem>

class Node;

class Edge : public QGraphicsItem
{
public:
    Edge(Node *sourceNode, Node *destNode, int weight = 1);


    Node* sourceNode() const { return source; }
    Node* destNode() const { return dest; }
    void adjust();
    void setColor(const QColor &color); // Для подсветки пути
    void resetColor();

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

protected:
    QRectF boundingRect() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
    Node *source, *dest;
    QPointF sourcePoint;
    QPointF destPoint;
    int weight;
    QColor currentColor;
};

#endif // EDGE_H
