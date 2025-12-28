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
#include <QTimer>

class Graph;
class Node; // Forward declaration

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

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

private:
    void setupUI();
    void setupMatrixConnections();
    void updateGraphView();
    bool parseMatrix();
    void resizeMatrixTable(int rows, int cols);
    void updateSymmetricCell(int row, int col);
    void enforceDiagonalZeros();

    // Функции управления визуализацией
    void resetEdgeColors();
    void stopAndReset(); // Функция полной остановки анимаций
    void highlightEdge(int u, int v, const QColor& color);
    void exportToDotFile(const QString& fileName);
    void highlightTraversal(const QVector<int>& traversal, const QColor& color);
    void highlightPath(const QVector<int>& path, const QColor& color);
    QString traversalToString(const QVector<int>& traversal);

    QGraphicsScene *scene;
    QGraphicsView *view;
    QTableWidget *matrixTable;

    // UI элементы
    QPushButton *generateButton;
    QPushButton *calcButton;
    QPushButton *resizeButton;
    QPushButton *colorButton;
    QPushButton *bipartiteButton;
    QPushButton *cycleButton;
    QPushButton *traversalButton;

    QComboBox *representationCombo;
    QComboBox *sizeCombo;
    QComboBox *startVertexCombo;

    QLabel *statusLabel;
    QLabel *hintLabel;
    QTextEdit *graphPropertiesDisplay;
    QToolBar *exportToolBar;
    QAction *saveImageAction;
    QAction *saveDotAction;

    // Данные
    QMap<int, Node*> nodes;
    Graph *graph;
    bool updatingMatrix;

    // Таймеры
    QTimer *animationTimer;
    QTimer *physicsTimer;

    // ЗАЩИТА ОТ ГОНКИ ПОТОКОВ
    long long graphGeneration; // Уникальный ID текущей версии графа
};

#endif // MAINWINDOW_H
