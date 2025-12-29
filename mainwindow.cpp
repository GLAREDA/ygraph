#include "mainwindow.h"
#include "graph.h"
#include "node.h"
#include "edge.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QHeaderView>
#include <QLineF>
#include <QScrollBar>
#include <QInputDialog>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QStack>
#include <QQueue>
#include <QSet>
#include <QFileDialog>
#include <QToolBar>
#include <QAction>
#include <QTimer>
#include <QRandomGenerator>
#include <QWheelEvent>
#include <QKeyEvent>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), graph(new Graph()), updatingMatrix(false)
{
    // === ИНИЦИАЛИЗАЦИЯ ===
    animationTimer = nullptr;
    physicsTimer = new QTimer(this);
    graphGeneration = 0;

    isEditMode = false;
    selectedNode = nullptr;
    tempLine = nullptr;
    tempSourceNode = nullptr;
    // =====================

    scene = new QGraphicsScene(this);
    view = new QGraphicsView(scene);

    matrixTable = new QTableWidget();
    generateButton = new QPushButton("Построить граф");
    calcButton = new QPushButton("Вычислить свойства");
    resizeButton = new QPushButton("Изменить размер");
    representationCombo = new QComboBox();
    sizeCombo = new QComboBox();
    statusLabel = new QLabel("Готово");
    graphPropertiesDisplay = new QTextEdit();
    hintLabel = new QLabel();
    traversalButton = new QPushButton("Визуализировать обход");
    startVertexCombo = new QComboBox();

    setupUI();
    setupMatrixConnections();

    // Настройки View
    view->setRenderHint(QPainter::Antialiasing);
    view->setRenderHint(QPainter::SmoothPixmapTransform);
    view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view->setDragMode(QGraphicsView::RubberBandDrag);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    view->viewport()->installEventFilter(this); // Включаем перехват событий
    view->setFocusPolicy(Qt::StrongFocus);


    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::onPhysicsUpdate);

    setWindowTitle("Редактор графов");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    stopAndReset();
    delete graph;
}

// === ЯДЕРНАЯ ФУНКЦИЯ ОЧИСТКИ ===
void MainWindow::stopAndReset() {
    // 1. Убиваем таймер анимации
    if (animationTimer) {
        animationTimer->disconnect();
        if (animationTimer->isActive()) animationTimer->stop();
        delete animationTimer;
        animationTimer = nullptr;
    }

    // 2. Сбрасываем выделение
    clearSelectionState();

    // 3. Сбрасываем цвета
    resetEdgeColors();
}

void MainWindow::clearSelectionState() {
    if (selectedNode) {
        selectedNode->resetColor();
        selectedNode = nullptr;
    }
}

// === ГЛАВНЫЙ ОБРАБОТЧИК СОБЫТИЙ (ЗУМ, УДАЛЕНИЕ, КЛИКИ) ===
bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // 1. ЗУМ
    if (obj == view->viewport() && event->type() == QEvent::Wheel) {
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);
        const double scaleFactor = 1.15;
        if (wheelEvent->angleDelta().y() > 0) view->scale(scaleFactor, scaleFactor);
        else view->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        return true;
    }

    // Если не режим редактирования - выходим
    if (!isEditMode) return QMainWindow::eventFilter(obj, event);

    // 2. КЛИКИ МЫШКИ
    if (obj == view->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {

            // Забираем фокус (чтобы клавиатура заработала)
            view->setFocus();

            QPointF scenePos = view->mapToScene(mouseEvent->pos());
            QGraphicsItem *item = scene->itemAt(scenePos, QTransform());

            if (Node *clickedNode = dynamic_cast<Node*>(item)) {
                if (!selectedNode) {
                    selectedNode = clickedNode;
                    selectedNode->setColor(Qt::green);
                    statusLabel->setText(QString("Выделен узел %1.").arg(clickedNode->getId() + 1));
                }
                else if (selectedNode == clickedNode) {
                    clearSelectionState();
                    statusLabel->setText("Выделение снято");
                }
                else {
                    graph->addEdge(selectedNode->getId(), clickedNode->getId());
                    rebuildGraphKeepPositions();
                    statusLabel->setText("Связь создана");
                }
                return true;
            }
            else {
                clearSelectionState();
                statusLabel->setText("Режим правки");
            }
        }
    }

    // 3. ДВОЙНОЙ КЛИК
    if (obj == view->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF scenePos = view->mapToScene(mouseEvent->pos());
        if (!scene->itemAt(scenePos, QTransform())) {
            graph->addVertex();
            rebuildGraphKeepPositions();
            int lastIdx = graph->nodeCount() - 1;
            if (nodes.contains(lastIdx)) nodes[lastIdx]->setPos(scenePos);
            return true;
        }
    }

    // 4. УДАЛЕНИЕ (ИСПРАВЛЕНО)
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // Отладка: если работает, вы увидите это в консоли Qt Creator "Application Output"
        // qDebug() << "Key Pressed:" << keyEvent->key();

        // Проверяем Delete (код 16777223) ИЛИ Backspace (код 16777219)
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {

            // А. Удаляем ЗЕЛЕНЫЙ (выбранный кликом) узел
            if (selectedNode) {
                int id = selectedNode->getId();
                clearSelectionState();
                graph->removeVertex(id);
                rebuildGraphKeepPositions();
                statusLabel->setText("Узел удален");
                return true;
            }

            // Б. Удаляем то, что выделено рамкой (RubberBand)
            if (!scene->selectedItems().isEmpty()) {
                QGraphicsItem *item = scene->selectedItems().first();

                if (Node *node = dynamic_cast<Node*>(item)) {
                    graph->removeVertex(node->getId());
                    rebuildGraphKeepPositions();
                    statusLabel->setText("Узел удален");
                    return true;
                }
                else if (Edge *edge = dynamic_cast<Edge*>(item)) {
                    graph->removeEdge(edge->sourceNode()->getId(), edge->destNode()->getId());
                    rebuildGraphKeepPositions();
                    statusLabel->setText("Ребро удалено");
                    return true;
                }
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    QToolBox *toolsPanel = new QToolBox();
    toolsPanel->setFixedWidth(320);

    // --- СТРАНИЦА 1: ДАННЫЕ И РЕДАКТОР ---
    QWidget *pageData = new QWidget();
    QVBoxLayout *layoutData = new QVBoxLayout(pageData);

    // 1. Кнопка РЕДАКТОРА
    editModeBtn = new QPushButton("Режим редактирования: ВЫКЛ");
    editModeBtn->setCheckable(true);
    editModeBtn->setStyleSheet("background-color: #ffcccb; font-weight: bold; padding: 5px;");
    connect(editModeBtn, &QPushButton::toggled, [=](bool checked){
        isEditMode = checked;
        if (checked) {
            editModeBtn->setText("Режим редактирования: ВКЛ");
            editModeBtn->setStyleSheet("background-color: #90ee90; font-weight: bold; padding: 5px;");
            statusLabel->setText("Правка: DblClick-узел, Клик-связь, Del-удалить");
            if (physicsTimer->isActive()) {
                physicsTimer->stop();
                QList<QPushButton*> btns = this->findChildren<QPushButton*>();
                for(auto b : btns) if(b->text().contains("Физика")) b->setChecked(false);
            }
        } else {
            editModeBtn->setText("Режим редактирования: ВЫКЛ");
            editModeBtn->setStyleSheet("background-color: #ffcccb; font-weight: bold; padding: 5px;");
            statusLabel->setText("Готово");
            clearSelectionState();
        }
    });
    layoutData->addWidget(editModeBtn);

    QPushButton *deleteBtn = new QPushButton("Удалить выделенное (или Del)");
    deleteBtn->setStyleSheet("background-color: #ff9999; padding: 3px;");
    connect(deleteBtn, &QPushButton::clicked, [=](){
        if (selectedNode) {
             int id = selectedNode->getId();
             clearSelectionState();
             graph->removeVertex(id);
             rebuildGraphKeepPositions();
             statusLabel->setText("Узел удален");
        }
    });
    layoutData->addWidget(deleteBtn);

    layoutData->addSpacing(10); // Отступ для красоты

    // 2. Генерация и Настройки (Вернул Random кнопку сюда)
    representationCombo->addItem("Матрица смежности");
    representationCombo->addItem("Матрица инцидентности");

    for (int i = 2; i <= 20; ++i) sizeCombo->addItem(QString("%1x%1").arg(i), i);
    sizeCombo->setCurrentIndex(3);

    QHBoxLayout *sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(new QLabel("Вершин:"));
    sizeLayout->addWidget(sizeCombo);
    sizeLayout->addWidget(resizeButton);

    layoutData->addWidget(new QLabel("Параметры матрицы:"));
    layoutData->addWidget(representationCombo);
    layoutData->addLayout(sizeLayout);

    QPushButton *randomButton = new QPushButton("Случайный граф");
    connect(randomButton, &QPushButton::clicked, this, &MainWindow::onRandomGraphClicked);
    layoutData->addWidget(randomButton);
    // ===============================================

    layoutData->addWidget(new QLabel("Таблица:"));

    matrixTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    matrixTable->setSelectionMode(QAbstractItemView::SingleSelection);
    matrixTable->setStyleSheet("QTableWidget { font: 11px; }");
    matrixTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    matrixTable->horizontalHeader()->setDefaultSectionSize(25);
    layoutData->addWidget(matrixTable);

    layoutData->addWidget(hintLabel);
    layoutData->addWidget(generateButton);
    layoutData->addWidget(statusLabel);
    layoutData->addStretch();

    toolsPanel->addItem(pageData, "1. Редактор и Данные");

    // --- СТРАНИЦА 2: АЛГОРИТМЫ ---
    QWidget *pageAlgo = new QWidget();
    QVBoxLayout *layoutAlgo = new QVBoxLayout(pageAlgo);

    cycleButton = new QPushButton("Поиск циклов");
    colorButton = new QPushButton("Раскраска (Жадная)");
    bipartiteButton = new QPushButton("Проверка двудольности");
    QPushButton *criticalButton = new QPushButton("Мосты и Точки сочленения");
    QPushButton *pathButton = new QPushButton("Кратчайший путь (Dijkstra)");

    connect(criticalButton, &QPushButton::clicked, this, &MainWindow::highlightBridgesAndArticulations);
    connect(pathButton, &QPushButton::clicked, this, &MainWindow::visualizeShortestPath);

    layoutAlgo->addWidget(new QLabel("<b>Структура:</b>"));
    layoutAlgo->addWidget(cycleButton);
    layoutAlgo->addWidget(bipartiteButton);
    layoutAlgo->addWidget(criticalButton);
    layoutAlgo->addWidget(new QLabel("<b>Вычисления:</b>"));
    layoutAlgo->addWidget(colorButton);
    layoutAlgo->addWidget(pathButton);

    layoutAlgo->addWidget(new QLabel("<b>Обходы:</b>"));
    QHBoxLayout *travLayout = new QHBoxLayout();
    travLayout->addWidget(new QLabel("Start:"));
    travLayout->addWidget(startVertexCombo);
    layoutAlgo->addLayout(travLayout);
    layoutAlgo->addWidget(traversalButton);

    layoutAlgo->addStretch();
    toolsPanel->addItem(pageAlgo, "2. Алгоритмы");

    // --- СТРАНИЦА 3: ВИД ---
    QWidget *pageView = new QWidget();
    QVBoxLayout *layoutView = new QVBoxLayout(pageView);

    QPushButton *physicsBtn = new QPushButton("Вкл/Выкл Физику (Force Layout)");
    physicsBtn->setCheckable(true);
    connect(physicsBtn, &QPushButton::toggled, [=](bool c){ if(c) physicsTimer->start(30); else physicsTimer->stop(); });

    layoutCombo = new QComboBox();
    layoutCombo->addItem("По кругу (Circular)");
    layoutCombo->addItem("Сетка (Grid)");
    layoutCombo->addItem("Случайная (Random)");

    useAnimationCheckbox = new QCheckBox("Плавная анимация");
    useAnimationCheckbox->setChecked(true); // Включено по умолчанию

    QPushButton *applyLayoutBtn = new QPushButton("Применить шаблон");
    connect(applyLayoutBtn, &QPushButton::clicked, this, &MainWindow::applyLayout);

    layoutView->addWidget(physicsBtn);
    layoutView->addSpacing(20);
    layoutView->addWidget(new QLabel("Шаблоны:"));
    layoutView->addWidget(layoutCombo);
    layoutView->addWidget(useAnimationCheckbox);
    layoutView->addWidget(applyLayoutBtn);
    layoutView->addStretch();
    toolsPanel->addItem(pageView, "3. Вид и Физика");

    QGroupBox *graphGroup = new QGroupBox("Граф");
    QVBoxLayout *graphLayout = new QVBoxLayout(graphGroup);
    graphLayout->addWidget(view);

    QGroupBox *propsGroup = new QGroupBox("Свойства");
    propsGroup->setFixedWidth(250);
    QVBoxLayout *propsLayout = new QVBoxLayout(propsGroup);
    propsLayout->addWidget(calcButton);
    propsLayout->addWidget(graphPropertiesDisplay);

    mainLayout->addWidget(toolsPanel);
    mainLayout->addWidget(graphGroup, 1);
    mainLayout->addWidget(propsGroup);

    setCentralWidget(centralWidget);

    exportToolBar = new QToolBar("Экспорт", this);
    saveImageAction = new QAction("IMG", this);
    saveDotAction = new QAction("DOT", this);
    exportToolBar->addAction(saveImageAction);
    exportToolBar->addAction(saveDotAction);
    addToolBar(Qt::TopToolBarArea, exportToolBar);

    connect(saveImageAction, &QAction::triggered, this, &MainWindow::saveGraphToImage);
    connect(saveDotAction, &QAction::triggered, this, &MainWindow::saveGraphToDotFile);
    connect(traversalButton, &QPushButton::clicked, this, &MainWindow::visualizeTraversal);
}

void MainWindow::updateGraphView() {
    stopAndReset();
    graphGeneration++;

    scene->clear();
    nodes.clear();

    if (!graph || graph->nodeCount() == 0) return;

    const qreal centerX = 0;
    const qreal centerY = 0;
    qreal circleRadius = 200.0;

    QVector<QPointF> positions;
    for (int i = 0; i < graph->nodeCount(); ++i) {
        qreal angle = 2 * M_PI * i / graph->nodeCount();
        QPointF newPos(centerX + circleRadius * cos(angle), centerY + circleRadius * sin(angle));

        for (const QPointF& pos : positions) {
             if (QLineF(newPos, pos).length() < 60) {
                 circleRadius *= 1.1;
                 newPos = QPointF(centerX + circleRadius * cos(angle), centerY + circleRadius * sin(angle));
             }
        }
        positions.append(newPos);

        Node *node = new Node(i);
        node->setPos(newPos);
        scene->addItem(node);
        nodes.insert(i, node);
    }

    const auto &adjMatrix = graph->adjacencyMatrix();
    for (int i = 0; i < adjMatrix.size(); ++i) {
        for (int j = i; j < adjMatrix[i].size(); ++j) {
            if (adjMatrix[i][j] > 0 && nodes.contains(i) && nodes.contains(j)) {
                Edge *edge = new Edge(nodes[i], nodes[j], adjMatrix[i][j]);
                scene->addItem(edge);
            }
        }
    }

    startVertexCombo->clear();
    for (int i = 0; i < graph->nodeCount(); ++i) {
        startVertexCombo->addItem(QString::number(i+1), i);
    }

    // Центрируем
    view->centerOn(0,0);
}

void MainWindow::rebuildGraphKeepPositions() {
    QVector<QPointF> oldPositions;
    for (int i = 0; i < nodes.size(); ++i) {
        if (nodes.contains(i)) oldPositions.append(nodes[i]->pos());
        else oldPositions.append(QPointF(0,0));
    }

    updateGraphView();
    updateTableFromGraph();

    for (int i = 0; i < qMin(nodes.size(), oldPositions.size()); ++i) {
        if (nodes.contains(i)) nodes[i]->setPos(oldPositions[i]);
    }
}

void MainWindow::updateTableFromGraph() {
    if (!graph) return;
    updatingMatrix = true;

    const auto& matrix = graph->adjacencyMatrix();
    int n = matrix.size();

    if (sizeCombo->findData(n) == -1) sizeCombo->addItem(QString("%1x%1").arg(n), n);
    sizeCombo->setCurrentIndex(sizeCombo->findData(n));

    matrixTable->setRowCount(n);
    matrixTable->setColumnCount(n);

    for (int i = 0; i < n; ++i) {
        matrixTable->setRowHeight(i, 30);
        for (int j = 0; j < n; ++j) {
            if (!matrixTable->item(i, j)) {
                matrixTable->setItem(i, j, new QTableWidgetItem());
                matrixTable->item(i, j)->setTextAlignment(Qt::AlignCenter);
            }
            matrixTable->item(i, j)->setText(QString::number(matrix[i][j]));
        }
    }
    if (representationCombo->currentIndex() == 0) enforceDiagonalZeros();
    updatingMatrix = false;
}

void MainWindow::onPhysicsUpdate() {
    for (auto node : nodes) node->calculateForces();
    for (auto node : nodes) node->advancePosition();
}

void MainWindow::resetEdgeColors() {
    for (auto item : scene->items()) {
        if (auto edge = dynamic_cast<Edge*>(item)) edge->resetColor();
        if (auto node = dynamic_cast<Node*>(item)) node->resetColor();
    }
}

void MainWindow::highlightEdge(int u, int v, const QColor& color) {
    if (!nodes.contains(u) || !nodes.contains(v)) return;
    for (auto item : scene->items()) {
        if (auto edge = dynamic_cast<Edge*>(item)) {
            if ((edge->sourceNode() == nodes[u] && edge->destNode() == nodes[v]) ||
                (edge->sourceNode() == nodes[v] && edge->destNode() == nodes[u])) {
                edge->setColor(color);
            }
        }
    }
}

void MainWindow::highlightTraversal(const QVector<int>& traversal, const QColor& color) {
    stopAndReset();
    if (traversal.isEmpty()) return;

    animationTimer = new QTimer(this);
    int index = 0;
    long long currentGen = graphGeneration;

    connect(animationTimer, &QTimer::timeout, [=]() mutable {
        if (this->graphGeneration != currentGen) return;

        if (index < traversal.size()) {
            int vertex = traversal[index];
            if (nodes.contains(vertex)) {
                nodes[vertex]->setColor(color);
                if (index > 0) highlightEdge(traversal[index-1], vertex, Qt::yellow);
            }
            index++;
        } else {
            if (animationTimer) animationTimer->stop();
        }
    });
    animationTimer->start(500);
}

void MainWindow::visualizeTraversal() {
    if (!graph || graph->nodeCount() == 0) return;
    int start = 0;
    if (startVertexCombo->count() > 0) start = startVertexCombo->currentData().toInt();
    if (start < 0 || start >= graph->nodeCount()) start = 0;

    QMessageBox msgBox;
    msgBox.setText("Выберите тип обхода:");
    QPushButton *bfsButton = msgBox.addButton("BFS", QMessageBox::ActionRole);
    QPushButton *dfsButton = msgBox.addButton("DFS", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);
    msgBox.exec();

    QVector<int> traversal;
    if (msgBox.clickedButton() == bfsButton) traversal = graph->bfs(start);
    else if (msgBox.clickedButton() == dfsButton) traversal = graph->dfs(start);
    else return;

    graphPropertiesDisplay->append("\n=== Обход ===\n" + traversalToString(traversal));
    highlightTraversal(traversal, Qt::green);
}

void MainWindow::highlightPath(const QVector<int>& path, const QColor& color) {
    stopAndReset();
    if (path.isEmpty()) return;
    for (int vertex : path) if (nodes.contains(vertex)) nodes[vertex]->setColor(color);
    for (int i = 0; i < path.size() - 1; ++i) highlightEdge(path[i], path[i+1], Qt::red);
}

void MainWindow::checkCycles() {
    if (!graph || graph->nodeCount() == 0) return;
    stopAndReset();
    QVector<QVector<int>> allCycles = graph->findAllCycles();
    if (!allCycles.empty()) {
        QString cyclesInfo = "\n=== Найденные циклы ===\n";
        int cycleNum = 1;
        QVector<QColor> cycleColors = {Qt::red, Qt::blue, Qt::green, Qt::magenta};
        foreach (const QVector<int>& cycle, allCycles) {
            if (cycle.size() < 3 || cycle.first() != cycle.last()) continue;
            QStringList vertices;
            for (int i = 0; i < cycle.size()-1; ++i) vertices << QString::number(cycle[i] + 1);
            vertices << QString::number(cycle.first() + 1);
            cyclesInfo += QString("Цикл %1: %2\n").arg(cycleNum).arg(vertices.join(" → "));
            QColor color = cycleColors[(cycleNum-1) % cycleColors.size()];
            for (int i = 0; i < cycle.size()-1; ++i) highlightEdge(cycle[i], cycle[i+1], color);
            cycleNum++;
        }
        graphPropertiesDisplay->append(cyclesInfo);
    } else {
        graphPropertiesDisplay->append("\nГраф ациклический");
    }
}

void MainWindow::colorGraph() {
    if (!graph || graph->nodeCount() == 0) return;
    stopAndReset();
    QVector<int> colors = graph->greedyColoring();
    QVector<QColor> colorPalette = { Qt::red, Qt::blue, Qt::green, Qt::yellow, Qt::magenta, Qt::cyan };
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        it.value()->setColor(colorPalette[colors[it.key()] % colorPalette.size()]);
    }
    graphPropertiesDisplay->append(QString("\nРаскраска завершена."));
}

void MainWindow::highlightBridgesAndArticulations() {
    if (!graph || graph->nodeCount() == 0) return;
    stopAndReset();
    QVector<QPair<int, int>> bridges = graph->getBridges();
    for (const auto& bridge : bridges) highlightEdge(bridge.first, bridge.second, Qt::red);
    QVector<int> articulations = graph->getArticulationPoints();
    for (int v : articulations) if (nodes.contains(v)) nodes[v]->setColor(QColor(255, 165, 0));
    graphPropertiesDisplay->append(QString("\nМостов: %1, Точек сочленения: %2").arg(bridges.size()).arg(articulations.size()));
}

void MainWindow::visualizeShortestPath() {
    if (!graph || graph->nodeCount() == 0) return;
    bool ok;
    int start = QInputDialog::getInt(this, "Путь", QString("От (1-%1):").arg(graph->nodeCount()), 1, 1, graph->nodeCount(), 1, &ok);
    if (!ok) return;
    int end = QInputDialog::getInt(this, "Путь", QString("До (1-%1):").arg(graph->nodeCount()), graph->nodeCount(), 1, graph->nodeCount(), 1, &ok);
    if (!ok) return;

    QVector<int> path = graph->dijkstra(start - 1, end - 1);
    if (path.isEmpty()) QMessageBox::information(this, "Инфо", "Путь не найден");
    else {
        QStringList pathStr;
        for (int v : path) pathStr << QString::number(v + 1);
        graphPropertiesDisplay->append(QString("\nПуть: %1").arg(pathStr.join(" -> ")));
        highlightPath(path, Qt::red);
    }
}

void MainWindow::checkBipartite() {
    if (!graph || graph->nodeCount() == 0) return;
    stopAndReset();
    if (graph->isBipartite()) {
        QVector<int> colors = graph->bipartiteColoring();
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            it.value()->setColor(colors[it.key()] == 0 ? Qt::red : Qt::blue);
        }
        graphPropertiesDisplay->append("\nГраф двудольный");
    } else {
        graphPropertiesDisplay->append("\nГраф не двудольный");
    }
}

void MainWindow::onRandomGraphClicked() {
    bool ok;
    int count = QInputDialog::getInt(this, "Генерация", "Вершин:", 5, 2, 50, 1, &ok);
    if (!ok) return;

    representationCombo->setCurrentIndex(0);
    resizeMatrixTable(count, count);
    updatingMatrix = true;

    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            if (QRandomGenerator::global()->bounded(100) < 30) {
                if (auto item1 = matrixTable->item(i, j)) item1->setText("1");
                if (auto item2 = matrixTable->item(j, i)) item2->setText("1");
            } else {
                if (auto item1 = matrixTable->item(i, j)) item1->setText("0");
                if (auto item2 = matrixTable->item(j, i)) item2->setText("0");
            }
        }
    }
    updatingMatrix = false;
    onGenerateClicked();
}

void MainWindow::applyLayout()
{
    if (nodes.isEmpty()) return;

    // 1. Выключаем физику
    if (physicsTimer->isActive()) {
        physicsTimer->stop();
        QList<QPushButton*> btns = this->findChildren<QPushButton*>();
        for(auto b : btns) if(b->text().contains("Физика")) b->setChecked(false);
    }

    int type = layoutCombo->currentIndex();
    int n = nodes.size();
    int cx = 0, cy = 0;

    // Группа для одновременной анимации всех узлов
    QParallelAnimationGroup *animGroup = new QParallelAnimationGroup;
    bool animate = useAnimationCheckbox->isChecked();

    for (int i = 0; i < n; ++i) {
        if (!nodes.contains(i)) continue; // На всякий случай

        QPointF targetPos;

        // Расчет позиции (То же самое, что и было)
        if (type == 0) { // Circular
            double radius = n * 40;
            if (radius < 200) radius = 200;
            double angle = 2 * M_PI * i / n;
            targetPos = QPointF(cx + radius * cos(angle), cy + radius * sin(angle));
        }
        else if (type == 1) { // Grid
            int cols = ceil(sqrt(n));
            int spacing = 150;
            double offsetX = (cols - 1) * spacing / 2.0;
            double offsetY = (ceil((double)n/cols) - 1) * spacing / 2.0;
            targetPos = QPointF(cx + (i % cols) * spacing - offsetX, cy + (i / cols) * spacing - offsetY);
        }
        else if (type == 2) { // Random
            targetPos = QPointF(QRandomGenerator::global()->bounded(-400, 401),
                                QRandomGenerator::global()->bounded(-400, 401));
        }

        // === ПРИМЕНЕНИЕ ===
        if (animate) {
            // Создаем анимацию свойства "pos"
            QPropertyAnimation *anim = new QPropertyAnimation(nodes[i], "pos");
            anim->setDuration(2000); //2 секунды
            anim->setStartValue(nodes[i]->pos());
            anim->setEndValue(targetPos);
            anim->setEasingCurve(QEasingCurve::OutExpo); // Красивое замедление в конце
            animGroup->addAnimation(anim);
        } else {
            // Мгновенный телепорт
            nodes[i]->setPos(targetPos);
        }
    }

    if (animate) {
        // Запускаем группу анимаций
        // DeleteWhenStopped удалит animGroup и все вложенные анимации после завершения (очистка памяти)
        animGroup->start(QAbstractAnimation::DeleteWhenStopped);
    } else {
        // Если анимации нет, группу нужно удалить сразу (так как мы её создали, но не запустили)
        delete animGroup;
        view->centerOn(0, 0); // Центрируем сразу
    }
}

// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (Свойства и Матрица) ===

void MainWindow::calculateGraphProperties() {
    if (!graph || graph->nodeCount() == 0) return;

    QString text = "=== Основные характеристики ===\n";
    text += QString("Вершин: %1\nРёбер: %2\nТип: Неориентированный\n").arg(graph->nodeCount()).arg(graph->edgeCount());

    QVector<int> degrees = graph->calculateDegrees();
    text += "\n=== Степени ===\n";
    for(int i=0; i<degrees.size(); ++i) text += QString("В%1: %2\n").arg(i+1).arg(degrees[i]);
    text += QString("Полный: %1\n").arg(graph->isComplete() ? "Да" : "Нет");

    int rad = graph->getRadius();
    int diam = graph->getDiameter();
    if (rad != INT_MAX) {
        text += QString("\nРадиус: %1\nДиаметр: %2\nМедиана: В%3\n").arg(rad).arg(diam).arg(graph->getMedian()+1);
        auto ecc = graph->getEccentricities();
        text += "Центральные: "; for(int i=0; i<ecc.size(); ++i) if(ecc[i]==rad) text += QString("%1 ").arg(i+1);
        text += "\nПериферийные: "; for(int i=0; i<ecc.size(); ++i) if(ecc[i]==diam) text += QString("%1 ").arg(i+1);
    } else {
        text += "\nГраф несвязный (метрики не определены)\n";
    }

    text += QString("\nЭйлеров: %1").arg(graph->isEulerian()?"Да":"Нет");
    text += QString("\nСвязный: %1").arg(graph->isConnected()?"Да":"Нет");

    // Критические (ИСПРАВЛЕНО ЗДЕСЬ)
    auto art = graph->getArticulationPoints();
    text += QString("\n\nТочки сочленения: ") + (art.isEmpty() ? "Нет" : "");
    for(int v : art) text += QString("%1 ").arg(v+1);

    auto br = graph->getBridges();
    text += QString("\nМосты: ") + (br.isEmpty() ? "Нет" : "");
    for(auto b : br) text += QString("(%1-%2) ").arg(b.first+1).arg(b.second+1);

    text += QString("\nДвудольный: %1").arg(graph->isBipartite()?"Да":"Нет");

    text += "\n\n=== Матрица расстояний ===\n    ";
    auto dist = graph->getDistanceMatrix();
    for(int i=0; i<dist.size(); ++i) text += QString("%1 ").arg(i+1, 3);
    text += "\n";
    for(int i=0; i<dist.size(); ++i) {
        text += QString("%1: ").arg(i+1, 2);
        for(int j=0; j<dist[i].size(); ++j) text += (dist[i][j]==INT_MAX ? " ∞ " : QString("%1 ").arg(dist[i][j], 3));
        text += "\n";
    }

    auto comp = graph->findConnectedComponents();
    text += QString("\nКомпонент связности: %1\n").arg(comp.size());
    for(int i=0; i<comp.size(); ++i) {
        QStringList l; for(int v : comp[i]) l << QString::number(v+1);
        text += QString("К%1: %2\n").arg(i+1).arg(l.join(", "));
    }

    auto clr = graph->greedyColoring();
    int maxC = 0; if(!clr.isEmpty()) maxC = *std::max_element(clr.begin(), clr.end()) + 1;
    text += QString("\nХроматическое число: %1").arg(maxC);

    graphPropertiesDisplay->setPlainText(text);
}

QString MainWindow::traversalToString(const QVector<int>& traversal) {
    QStringList vertices;
    for (int v : traversal) vertices << QString::number(v + 1);
    return vertices.join(" → ");
}

void MainWindow::saveGraphToImage() {
    if (!graph || nodes.empty()) return;
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить", "", "PNG (*.png);;JPEG (*.jpg)");
    if (fileName.isEmpty()) return;
    scene->clearSelection();
    QRectF sceneRect = scene->itemsBoundingRect().adjusted(-20, -20, 20, 20);
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    scene->render(&painter, QRectF(), sceneRect);
    painter.end();
    image.save(fileName);
}

void MainWindow::saveGraphToDotFile() {
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить DOT", "", "DOT (*.dot)");
    if (fileName.isEmpty()) return;
    exportToDotFile(fileName);
}

void MainWindow::exportToDotFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out << "graph G {\n    node [shape=circle, style=filled, fillcolor=lightblue];\n";
    for (int i = 0; i < graph->nodeCount(); ++i) out << QString("    %1 [label=\"%2\"];\n").arg(i+1).arg(i+1);
    const auto &adjMatrix = graph->adjacencyMatrix();
    for (int i = 0; i < adjMatrix.size(); ++i) {
        for (int j = i; j < adjMatrix[i].size(); ++j) {
            if (adjMatrix[i][j] > 0) out << QString("    %1 -- %2;\n").arg(i+1).arg(j+1);
        }
    }
    out << "}\n";
    file.close();
}

void MainWindow::setupMatrixConnections() {
    connect(cycleButton, &QPushButton::clicked, this, &MainWindow::checkCycles);
    connect(bipartiteButton, &QPushButton::clicked, this, &MainWindow::checkBipartite);
    connect(colorButton, &QPushButton::clicked, this, &MainWindow::colorGraph);
    connect(representationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onRepresentationChanged);
    connect(matrixTable, &QTableWidget::cellChanged, this, &MainWindow::onMatrixCellChanged);
    connect(generateButton, &QPushButton::clicked, this, &MainWindow::onGenerateClicked);
    connect(calcButton, &QPushButton::clicked, this, &MainWindow::calculateGraphProperties);
    connect(resizeButton, &QPushButton::clicked, this, &MainWindow::resizeMatrix);
}

void MainWindow::resizeMatrixTable(int rows, int cols) {
    updatingMatrix = true;
    matrixTable->setRowCount(rows);
    matrixTable->setColumnCount(cols);
    for (int i = 0; i < rows; ++i) {
        matrixTable->setRowHeight(i, 30);
        for (int j = 0; j < cols; ++j) {
            if (!matrixTable->item(i, j)) {
                matrixTable->setItem(i, j, new QTableWidgetItem("0"));
                matrixTable->item(i, j)->setTextAlignment(Qt::AlignCenter);
            }
            matrixTable->item(i, j)->setText("0");
            matrixTable->item(i, j)->setBackground(QBrush());
            matrixTable->item(i, j)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        }
    }
    if (representationCombo->currentIndex() == 0) enforceDiagonalZeros();
    updatingMatrix = false;
}

void MainWindow::enforceDiagonalZeros() {
    int size = qMin(matrixTable->rowCount(), matrixTable->columnCount());
    for (int i = 0; i < size; ++i) {
        if (auto item = matrixTable->item(i, i)) {
            item->setText("0");
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setBackground(QColor(240, 240, 240));
        }
    }
}

void MainWindow::updateSymmetricCell(int row, int col) {
    if (updatingMatrix || representationCombo->currentIndex() != 0 || row == col) return;
    updatingMatrix = true;
    if (auto item = matrixTable->item(row, col)) {
        if (!matrixTable->item(col, row)) matrixTable->setItem(col, row, new QTableWidgetItem());
        matrixTable->item(col, row)->setText(item->text());
        matrixTable->item(col, row)->setTextAlignment(Qt::AlignCenter);
    }
    updatingMatrix = false;
}

void MainWindow::onMatrixCellChanged(int row, int col) {
    if (updatingMatrix) return;
    auto item = matrixTable->item(row, col);
    if (!item) return;
    bool ok;
    int val = item->text().toInt(&ok);
    if (!ok || val < 0) {
        item->setText("0");
        return;
    }
    if (representationCombo->currentIndex() == 0) updateSymmetricCell(row, col);
}

void MainWindow::onRepresentationChanged(int index) {
    int s = sizeCombo->currentData().toInt();
    if (index == 0) resizeMatrixTable(s, s);
    else resizeMatrixTable(s, std::max(1, s-1));
}

void MainWindow::resizeMatrix() {
    int s = sizeCombo->currentData().toInt();
    if (representationCombo->currentIndex() == 0) resizeMatrixTable(s, s);
    else {
        bool ok;
        int e = QInputDialog::getInt(this, "Рёбра", "Кол-во рёбер:", std::max(1, s-1), 1, 100, 1, &ok);
        if (ok) resizeMatrixTable(s, e);
    }
}

bool MainWindow::parseMatrix() {
    int rows = matrixTable->rowCount();
    int cols = matrixTable->columnCount();
    QVector<QVector<int>> m(rows, QVector<int>(cols));
    for(int i=0; i<rows; ++i) {
        for(int j=0; j<cols; ++j) {
            if(!matrixTable->item(i, j)) return false;
            m[i][j] = matrixTable->item(i, j)->text().toInt();
        }
    }
    if (representationCombo->currentIndex() == 0) graph->createFromAdjacencyMatrix(m);
    else graph->createFromIncidenceMatrix(m);
    statusLabel->setText("Матрица загружена");
    return true;
}

void MainWindow::onGenerateClicked() {
    if (parseMatrix()) updateGraphView();
}
