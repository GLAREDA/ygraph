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
#include <QToolBox>
#include <QCheckBox>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

class Graph;
class Node; // Forward declaration
class QGraphicsLineItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // Перехватчик событий (для зума и редактора)
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onGenerateClicked();
    void onRandomGraphClicked();
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
    void onPhysicsUpdate();
    void applyLayout();

private:
    void setupUI();
    void setupMatrixConnections();
    void updateGraphView();
    bool parseMatrix();
    void resizeMatrixTable(int rows, int cols);
    void updateSymmetricCell(int row, int col);
    void enforceDiagonalZeros();

    // Вспомогательные функции
    void resetEdgeColors();
    void stopAndReset(); // Остановка анимаций
    void clearSelectionState(); // Сброс выделения
    void rebuildGraphKeepPositions(); // Перестройка с сохранением координат
    void updateTableFromGraph(); // Синхронизация таблицы

    // Визуализация
    void highlightEdge(int u, int v, const QColor& color);
    void exportToDotFile(const QString& fileName);
    void highlightTraversal(const QVector<int>& traversal, const QColor& color);
    void highlightPath(const QVector<int>& path, const QColor& color);
    QString traversalToString(const QVector<int>& traversal);

    // Графика
    QGraphicsScene *scene;
    QGraphicsView *view;

    QCheckBox *useAnimationCheckbox;

    // Данные
    QMap<int, Node*> nodes;
    Graph *graph;
    long long graphGeneration; // Защита от багов анимации

    // UI
    QTableWidget *matrixTable;
    QPushButton *generateButton;
    QPushButton *calcButton;
    QPushButton *resizeButton;
    QPushButton *colorButton;
    QPushButton *bipartiteButton;
    QPushButton *cycleButton;
    QPushButton *traversalButton;
    QPushButton *editModeBtn;

    QComboBox *representationCombo;
    QComboBox *sizeCombo;
    QComboBox *startVertexCombo;
    QComboBox *layoutCombo;

    QLabel *statusLabel;
    QLabel *hintLabel;
    QTextEdit *graphPropertiesDisplay;
    QToolBar *exportToolBar;
    QAction *saveImageAction;
    QAction *saveDotAction;

    bool updatingMatrix;

    // Таймеры
    QTimer *animationTimer;
    QTimer *physicsTimer;

    // Редактор
    bool isEditMode;
    Node *selectedNode; // Текущий выделенный узел
    QCheckBox *directedCheck;

    // Временные (не используются в текущей логике, но оставим для совместимости)
    QGraphicsLineItem *tempLine;
    Node *tempSourceNode;
};

#endif // MAINWINDOW_H
