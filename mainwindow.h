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
#include <QStack>
#include <QDateTime>
#include <QDockWidget>
#include <QShortcut>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class Graph;
class Node; // Forward declaration
class QGraphicsLineItem;
class Edge;


struct GraphState {
    QVector<QVector<int>> matrix; // Связи
    bool isDirected;              // Тип
    QMap<int, QPointF> positions; // Координаты узлов
    QMap<int, QColor> colors;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
    QStringList actionLog;
    void logAction(const QString& message);


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
    void undo();
    void redo();
    void saveProject(); // Сохранить в JSON
    void loadProject(); // Загрузить из JSON
    void visualizeMST();
    void calculateChromPolynomial();
    void showDistanceMatrixDialog();

private:
    void setupUI();
    void setupMatrixConnections();
    void updateGraphView();
    bool parseMatrix();
    void resizeMatrixTable(int rows, int cols);
    void updateSymmetricCell(int row, int col);
    void enforceDiagonalZeros();
    void saveToHistory();
    void restoreState(const GraphState &state);
    int currentEditMode; // 0 = V1 (Classic), 1 = V2 (Context Menu)
    void loadSettings();
    void showContextMenu(const QPoint& pos);
    void changeEdgeWeight(Edge* edge);


    // Вспомогательные функции
    void resetEdgeColors();
    void stopAndReset(); // Остановка анимаций
    void clearSelectionState(); // Сброс выделения
    void rebuildGraphKeepPositions(int removedId = -1); // Добавлен аргумент по умолчанию
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

    // История
    QStack<GraphState> undoStack;
    QStack<GraphState> redoStack;

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
