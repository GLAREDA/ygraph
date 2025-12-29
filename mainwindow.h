#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTableWidget>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QTextEdit>
#include <QMap>
#include "node.h"
#include "edge.h"

class Graph;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;


private slots:
    void onGenerateClicked();
    void onRepresentationChanged(int index);
    void calculateGraphProperties();
    void onMatrixCellChanged(int row, int col);
    void resizeMatrix();
    void colorGraph();
    void checkBipartite();
    void checkCycles();
    void saveGraphToImage();
    void saveGraphToDotFile();
    void visualizeTraversal();
    void visualizeShortestPath();
    void highlightBridgesAndArticulations();
    void onRandomGraphClicked();


private:
    void setupUI();
    void setupMatrixConnections();
    void updateGraphView();
    bool parseMatrix();
    void resizeMatrixTable(int rows, int cols);
    void updateSymmetricCell(int row, int col);
    void enforceDiagonalZeros();
    void validateIncidenceMatrix();
    void resetEdgeColors();
    void highlightEdge(int u, int v, const QColor& color);
     void exportToDotFile(const QString& fileName);
     void highlightTraversal(const QVector<int>& traversal, const QColor& color);
     void highlightPath(const QVector<int>& path, const QColor& color);
   void onPhysicsUpdate();


QString traversalToString(const QVector<int>& traversal);


    QGraphicsScene *scene;
    QGraphicsView *view;
    QTableWidget *matrixTable;
    QPushButton *generateButton;
    QPushButton *calcButton;
    QPushButton *resizeButton;
    QComboBox *representationCombo;
    QComboBox *sizeCombo;
    QLabel *statusLabel;
    QTextEdit *graphPropertiesDisplay;
    QLabel *hintLabel;
    QMap<int, Node*> nodes;
    QPushButton *colorButton;
    QPushButton *bipartiteButton;
    QPushButton *cycleButton;
    QToolBar *exportToolBar;
    QAction *saveImageAction;
    QAction *saveDotAction;
    QPushButton *traversalButton;
    QComboBox *startVertexCombo;
    QTimer *physicsTimer;


    Graph *graph;
    bool updatingMatrix;
};

#endif // MAINWINDOW_H
