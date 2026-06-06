#ifndef EDGE_H
#define EDGE_H

#include <QGraphicsItem>
#include <QColor>

class Node;

class Edge : public QGraphicsItem
{
public:
    Edge(Node *sourceNode, Node *destNode, int weight, bool isDirected);

    Node *sourceNode() const { return source; }
    Node *destNode()   const { return dest;   }
    int   getWeight()  const { return weight;  }

    void setColor(const QColor &color);
    void resetColor();
    void adjust();

    // Смещение для параллельных рёбер
    void   setParallelOffset(double offset) { parallelOffset = offset; update(); }
    double getParallelOffset() const        { return parallelOffset; }

    QRectF boundingRect() const override;
    void   paint(QPainter *painter,
                 const QStyleOptionGraphicsItem *option,
                 QWidget *widget) override;

    enum { Type = UserType + 2 };
    int type() const override { return Type; }

private:
    Node   *source;
    Node   *dest;
    int     weight;
    bool    isDirected;
    QColor  currentColor;
    QPointF sourcePoint;
    QPointF destPoint;

    double  parallelOffset = 0.0; // перпендикулярное смещение
    static constexpr double arrowSize = 12.0;
};

#endif // EDGE_H
