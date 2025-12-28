#ifndef NODE_H
#define NODE_H

#include <QGraphicsItem>
#include <QList>
#include <QBrush>

class Edge;

class Node : public QGraphicsItem
{
public:
    Node(int id);

    void addEdge(Edge *edge);
    QList<Edge *> edges() const;
    int getId() const { return id; }

    // Новые методы для управления цветом из MainWindow
    void setColor(const QColor &color);
    void resetColor();

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
    const QColor defaultColor = QColor(70, 130, 180); // Стандартный синий
};

#endif // NODE_H
