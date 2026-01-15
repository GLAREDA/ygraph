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
    qDebug() << "APP: Запуск программы";

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
    view->viewport()->installEventFilter(this);
    view->setFocusPolicy(Qt::StrongFocus); // Важно для клавиатуры

    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::onPhysicsUpdate);

    setWindowTitle("Редактор графов");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    stopAndReset();
    delete graph;
    qDebug() << "APP: Завершение работы";
}

// === ЯДЕРНАЯ ФУНКЦИЯ ОЧИСТКИ ===
void MainWindow::stopAndReset() {
    if (animationTimer) {
        animationTimer->disconnect();
        if (animationTimer->isActive()) animationTimer->stop();
        delete animationTimer;
        animationTimer = nullptr;
    }
    clearSelectionState();
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

    if (!isEditMode) return QMainWindow::eventFilter(obj, event);

    // 2. КЛИКИ МЫШКИ
    if (obj == view->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::LeftButton) {
            view->setFocus(); // Забираем фокус для клавиатуры

            QPointF scenePos = view->mapToScene(mouseEvent->pos());
            QGraphicsItem *item = scene->itemAt(scenePos, QTransform());

            if (Node *clickedNode = dynamic_cast<Node*>(item)) {
                if (!selectedNode) {
                    selectedNode = clickedNode;
                    selectedNode->setColor(Qt::green);
                    statusLabel->setText(QString("Выделен узел %1.").arg(clickedNode->getId() + 1));
                    qDebug() << "EDIT: Selected node" << clickedNode->getId();
                }
                else if (selectedNode == clickedNode) {
                    clearSelectionState();
                    statusLabel->setText("Выделение снято");
                    qDebug() << "EDIT: Deselected node";
                }
                else {
                    // Создание ребра
                    saveToHistory(); // <-- Сохраняем перед изменением
                    qDebug() << "EDIT: Added edge" << selectedNode->getId() << "->" << clickedNode->getId();
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

    // 3. ДВОЙНОЙ КЛИК (Добавление)
    if (obj == view->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        QPointF scenePos = view->mapToScene(mouseEvent->pos());
        if (!scene->itemAt(scenePos, QTransform())) {
            saveToHistory(); // <-- Сохраняем
            qDebug() << "EDIT: Added vertex";
            graph->addVertex();
            rebuildGraphKeepPositions();

            // Ставим новый узел под курсор
            int lastIdx = graph->nodeCount() - 1;
            if (nodes.contains(lastIdx)) nodes[lastIdx]->setPos(scenePos);
            return true;
        }
    }

    // 4. УДАЛЕНИЕ (Клавиша)
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Delete || keyEvent->key() == Qt::Key_Backspace) {

            // Удаление узла
            if (selectedNode) {
                saveToHistory(); // <-- ИСПРАВЛЕНО: Сохраняем перед удалением
                int id = selectedNode->getId();
                qDebug() << "EDIT: Removing vertex ID:" << id;

                clearSelectionState();
                graph->removeVertex(id);

                // Передаем ID удаленного узла, чтобы правильно сдвинуть позиции
                rebuildGraphKeepPositions(id);

                statusLabel->setText("Узел удален");
                return true;
            }

            // Удаление ребра
            if (!scene->selectedItems().isEmpty()) {
                if (Edge *edge = dynamic_cast<Edge*>(scene->selectedItems().first())) {
                    saveToHistory(); // <-- Сохраняем
                    qDebug() << "EDIT: Removing edge";
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

    // Кнопка режима редактирования
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

    // Кнопка удаления
    QPushButton *deleteBtn = new QPushButton("Удалить выделенное (или Del)");
    deleteBtn->setStyleSheet("background-color: #ff9999; padding: 3px;");
    connect(deleteBtn, &QPushButton::clicked, [=](){
        if (selectedNode) {
             saveToHistory(); // Сохраняем перед удалением
             int id = selectedNode->getId();
             clearSelectionState();
             graph->removeVertex(id);
             rebuildGraphKeepPositions(id); // Удаляем с учетом сдвига индексов
             statusLabel->setText("Узел удален");
        }
    });
    layoutData->addWidget(deleteBtn);
    layoutData->addSpacing(10);

    // Галочка Ориентированный
    directedCheck = new QCheckBox("Ориентированный граф");
    directedCheck->setChecked(false);
    connect(directedCheck, &QCheckBox::toggled, [=](bool checked){
        saveToHistory();
        graph->setDirected(checked);
        updateGraphView();
        updateTableFromGraph();
        if(representationCombo->currentIndex()==1) onRepresentationChanged(1); // Обновить подсказку
    });
    layoutData->addWidget(directedCheck);

    // Настройки матрицы
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

    // Кнопка Случайный граф (над таблицей)
    QPushButton *randomButton = new QPushButton("Случайный граф");
    connect(randomButton, &QPushButton::clicked, this, &MainWindow::onRandomGraphClicked);
    layoutData->addWidget(randomButton);

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
    useAnimationCheckbox->setChecked(true);

    QPushButton *applyLayoutBtn = new QPushButton("Применить шаблон");
    connect(applyLayoutBtn, &QPushButton::clicked, this, &MainWindow::applyLayout);

    layoutView->addWidget(physicsBtn);
    layoutView->addSpacing(20);
    layoutView->addWidget(new QLabel("Шаблоны:"));
    layoutView->addWidget(layoutCombo);
    layoutView->addWidget(useAnimationCheckbox); // Добавили галочку
    layoutView->addWidget(applyLayoutBtn);
    layoutView->addStretch();

    toolsPanel->addItem(pageView, "3. Вид и Физика");

    // СБОРКА ОКОН
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

    // ТУЛБАР
    exportToolBar = new QToolBar("Экспорт", this);
    saveImageAction = new QAction("IMG", this);
    saveDotAction = new QAction("DOT", this);
    exportToolBar->addAction(saveImageAction);
    exportToolBar->addAction(saveDotAction);
    addToolBar(Qt::TopToolBarArea, exportToolBar);

    connect(saveImageAction, &QAction::triggered, this, &MainWindow::saveGraphToImage);
    connect(saveDotAction, &QAction::triggered, this, &MainWindow::saveGraphToDotFile);
    connect(traversalButton, &QPushButton::clicked, this, &MainWindow::visualizeTraversal);

    // ШОРТКАТЫ
    QShortcut *undoShortcut = new QShortcut(QKeySequence("Ctrl+Z"), this);
    connect(undoShortcut, &QShortcut::activated, this, &MainWindow::undo);

    QShortcut *redoShortcut = new QShortcut(QKeySequence("Ctrl+Shift+Z"), this);
    connect(redoShortcut, &QShortcut::activated, this, &MainWindow::redo);
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
    bool isDirected = graph->getDirected();

    for (int i = 0; i < adjMatrix.size(); ++i) {
        int startJ = isDirected ? 0 : i;
        for (int j = startJ; j < adjMatrix[i].size(); ++j) {
            if (adjMatrix[i][j] > 0 && nodes.contains(i) && nodes.contains(j)) {
                Edge *edge = new Edge(nodes[i], nodes[j], adjMatrix[i][j], isDirected);
                scene->addItem(edge);
            }
        }
    }

    startVertexCombo->clear();
    for (int i = 0; i < graph->nodeCount(); ++i) {
        startVertexCombo->addItem(QString::number(i+1), i);
    }

    view->centerOn(0,0);
}

void MainWindow::rebuildGraphKeepPositions(int removedId) {
    qDebug() << "VIEW: Перестройка с сохранением позиций. Удален ID:" << removedId;

    // 1. Сохраняем текущие позиции
    QVector<QPointF> oldPositions;
    for (int i = 0; i < nodes.size(); ++i) {
        if (nodes.contains(i)) oldPositions.append(nodes[i]->pos());
        else oldPositions.append(QPointF(0,0));
    }

    // 2. Строим новый граф
    updateGraphView();
    updateTableFromGraph();

    // 3. Восстанавливаем позиции УМНО (сдвигая индексы)
    int oldIdx = 0;
    int newNodesCount = nodes.size();

    for (int newIdx = 0; newIdx < newNodesCount; ++newIdx) {
        // Если этот индекс был удален в старом массиве, пропускаем его позицию
        if (oldIdx == removedId) {
            oldIdx++;
        }

        // Берем позицию из старого массива
        if (oldIdx < oldPositions.size() && nodes.contains(newIdx)) {
            nodes[newIdx]->setPos(oldPositions[oldIdx]);
        }

        oldIdx++;
    }
}

void MainWindow::updateTableFromGraph() {
    if (!graph) return;
    updatingMatrix = true;

    bool showIncidence = (representationCombo->currentIndex() == 1);

    if (showIncidence) {
        const auto& matrix = graph->incidenceMatrix();
        int rows = matrix.size();
        int cols = (rows > 0) ? matrix[0].size() : 0;

        if (sizeCombo->findData(rows) == -1) sizeCombo->addItem(QString("%1x%1").arg(rows), rows);
        sizeCombo->setCurrentIndex(sizeCombo->findData(rows));

        matrixTable->setRowCount(rows);
        matrixTable->setColumnCount(cols);

        for (int i = 0; i < rows; ++i) {
            matrixTable->setRowHeight(i, 30);
            for (int j = 0; j < cols; ++j) {
                if (!matrixTable->item(i, j)) {
                    matrixTable->setItem(i, j, new QTableWidgetItem());
                    matrixTable->item(i, j)->setTextAlignment(Qt::AlignCenter);
                }
                matrixTable->item(i, j)->setText(QString::number(matrix[i][j]));
            }
        }
    } else {
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
    }

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
    // 1. Спрашиваем количество вершин
    int count = QInputDialog::getInt(this, "Генерация", "Количество вершин:", 5, 2, 50, 1, &ok);
    if (!ok) return;

    // 2. Смотрим на состояние чекбокса
    // (directedCheckbox мы вынесли в .h файл на предыдущем шаге)
    bool makeDirected = directedCheck->isChecked();

    // Обновляем модель (на всякий случай)
    graph->setDirected(makeDirected);

    // 3. Подготовка таблицы
    representationCombo->setCurrentIndex(0); // Матрица смежности
    resizeMatrixTable(count, count);
    updatingMatrix = true;

    saveToHistory();

    // Очистка
    for(int i=0; i<count; ++i)
        for(int j=0; j<count; ++j)
            if(auto it = matrixTable->item(i,j)) it->setText("0");

    // 4. Генерация
    if (makeDirected) {
        // ОРИЕНТИРОВАННЫЙ: заполняем каждую ячейку независимо
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < count; ++j) {
                if (i == j) continue;
                // Вероятность 20% (чуть меньше, чтобы не было каши из стрелок)
                if (QRandomGenerator::global()->bounded(100) < 20) {
                    if (auto item = matrixTable->item(i, j)) item->setText("1");
                }
            }
        }
    } else {
        // ОБЫЧНЫЙ: заполняем симметрично
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                // Вероятность 30%
                if (QRandomGenerator::global()->bounded(100) < 30) {
                    if (auto item1 = matrixTable->item(i, j)) item1->setText("1");
                    if (auto item2 = matrixTable->item(j, i)) item2->setText("1");
                }
            }
        }
    }

    updatingMatrix = false;
    onGenerateClicked();

    statusLabel->setText(QString("Сгенерирован %1 граф").arg(makeDirected ? "орграф" : "обычный"));
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
    text += QString("Вершин: %1\n").arg(graph->nodeCount());
    text += QString("Рёбер: %1\n").arg(graph->edgeCount());

    // ИСПРАВЛЕНИЕ: Честный вывод типа графа
    QString typeStr = graph->getDirected() ? "Ориентированный" : "Неориентированный";
    text += QString("Тип: %1\n").arg(typeStr);

    QVector<int> degrees = graph->calculateDegrees();
    text += "\n=== Степени вершин ===\n";
    for(int i=0; i<degrees.size(); ++i) {
        // Для ориентированного графа это исходящая степень (out-degree)
        text += QString("В%1: %2\n").arg(i+1).arg(degrees[i]);
    }

    text += QString("Полный: %1\n").arg(graph->isComplete() ? "Да" : "Нет");

    // Метрики
    int rad = graph->getRadius();
    int diam = graph->getDiameter();

    // Если граф ориентированный и не сильно связный, радиус может быть бесконечным
    if (rad != INT_MAX) {
        text += QString("\nРадиус: %1\nДиаметр: %2\nМедиана: В%3\n").arg(rad).arg(diam).arg(graph->getMedian()+1);

        auto ecc = graph->getEccentricities();
        text += "Центральные: ";
        for(int i=0; i<ecc.size(); ++i) if(ecc[i]==rad) text += QString("%1 ").arg(i+1);

        text += "\nПериферийные: ";
        for(int i=0; i<ecc.size(); ++i) if(ecc[i]==diam) text += QString("%1 ").arg(i+1);
    } else {
        text += "\nГраф несвязный (или не сильно связный)\nМетрики не определены\n";
    }

    text += QString("\nЭйлеров: %1").arg(graph->isEulerian()?"Да":"Нет");
    text += QString("\nСвязный: %1").arg(graph->isConnected()?"Да":"Нет");

    // Критические элементы
    auto art = graph->getArticulationPoints();
    text += QString("\n\nТочки сочленения: ") + (art.isEmpty() ? "Нет" : "");
    for(int v : art) text += QString("%1 ").arg(v+1);

    auto br = graph->getBridges();
    text += QString("\nМосты: ") + (br.isEmpty() ? "Нет" : "");
    for(auto b : br) text += QString("(%1-%2) ").arg(b.first+1).arg(b.second+1);

    text += QString("\nДвудольный: %1").arg(graph->isBipartite()?"Да":"Нет");

    // Матрица расстояний
    text += "\n\n=== Матрица расстояний ===\n    ";
    auto dist = graph->getDistanceMatrix();
    // Заголовок
    for(int i=0; i<dist.size(); ++i) text += QString("%1 ").arg(i+1, 3);
    text += "\n";
    // Строки
    for(int i=0; i<dist.size(); ++i) {
        text += QString("%1: ").arg(i+1, 2);
        for(int j=0; j<dist[i].size(); ++j) {
            text += (dist[i][j]==INT_MAX ? "  ∞ " : QString("%1 ").arg(dist[i][j], 3));
        }
        text += "\n";
    }

    // Компоненты связности
    auto comp = graph->findConnectedComponents();
    text += QString("\nКомпонент связности: %1\n").arg(comp.size());
    for(int i=0; i<comp.size(); ++i) {
        QStringList l;
        for(int v : comp[i]) l << QString::number(v+1);
        text += QString("К%1: %2\n").arg(i+1).arg(l.join(", "));
    }

    // Раскраска
    auto clr = graph->greedyColoring();
    int maxC = 0;
    if(!clr.isEmpty()) maxC = *std::max_element(clr.begin(), clr.end()) + 1;
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
    int value = item->text().toInt(&ok);

    // 1. Проверяем, что это вообще число
    if (!ok) {
        // Если ввели букву - сбрасываем
        item->setText("0");
        return;
    }

    // 2. Логика проверки значений зависит от типа матрицы
    if (representationCombo->currentIndex() == 0) {
        // --- МАТРИЦА СМЕЖНОСТИ ---
        // Здесь веса обычно неотрицательные (хотя в теории графов бывают и <0)
        // Если вы хотите разрешить только >= 0:
        if (value < 0) {
            QMessageBox::warning(this, "Ошибка", "Вес ребра не может быть отрицательным");
            item->setText("0");
            return;
        }

        // Обновляем симметричную ячейку (если граф неориентированный)
        if (directedCheck && !directedCheck->isChecked()) {
            updateSymmetricCell(row, col);
        }

        // Диагональ должна быть 0 (если нет петель)
        if (row == col && value != 0) {
             item->setText("0");
        }
    }
    else {
        // --- МАТРИЦА ИНЦИДЕНТНОСТИ ---
        // Здесь разрешены только: 0, 1 (и -1 для орграфов)
        // Вес ребра тут обычно не указывается (или все ребра весом 1)

        bool isDirected = (directedCheck && directedCheck->isChecked());

        if (isDirected) {
            // Орграф: можно 0, 1, -1
            if (value != 0 && value != 1 && value != -1) {
                QMessageBox::warning(this, "Ошибка", "В матрице инцидентности орграфа только: 0, 1, -1");
                item->setText("0");
            }
        } else {
            // Обычный граф: можно 0, 1
            if (value != 0 && value != 1) {
                QMessageBox::warning(this, "Ошибка", "В матрице инцидентности только: 0, 1");
                item->setText("0");
            }
        }
    }
}

void MainWindow::onRepresentationChanged(int index) {
    int s = sizeCombo->currentData().toInt();
    if (index == 0) resizeMatrixTable(s, s);
    else resizeMatrixTable(s, std::max(1, s-1));
    if (index == 1) { // Инцидентность
        hintLabel->show();
        if (directedCheck && directedCheck->isChecked()) {
            hintLabel->setText("Орграф: 1=источник, -1=цель (в столбце)");
        } else {
            hintLabel->setText("Граф: Две 1 в столбце");
        }
    } else {
        hintLabel->hide();
    }

    // ВАЖНО: Если граф уже есть, перерисуем таблицу в новом формате
    updateTableFromGraph();
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
            // Теперь читаем любые числа (включая -1)
            m[i][j] = matrixTable->item(i, j)->text().toInt();
        }
    }
    bool directed = (directedCheck && directedCheck->isChecked());
    graph->setDirected(directed);

    // 2. Создаем граф
    if (representationCombo->currentIndex() == 0) {
        graph->createFromAdjacencyMatrix(m);
    } else {
        graph->createFromIncidenceMatrix(m);
    }

    statusLabel->setText("Матрица загружена");
    return true;
}

void MainWindow::onGenerateClicked() {
    saveToHistory();
    if (parseMatrix()) updateGraphView();
}

// === ИСТОРИЯ (UNDO/REDO) ===


void MainWindow::saveToHistory() {
    qDebug() << "HISTORY: Сохранение состояния...";
    GraphState state;
    state.matrix = graph->adjacencyMatrix();
    state.isDirected = graph->getDirected();

    for(auto it = nodes.begin(); it != nodes.end(); ++it) {
        state.positions.insert(it.key(), it.value()->pos());
    }

    undoStack.push(state);
    redoStack.clear();

    if (undoStack.size() > 50) undoStack.removeFirst();
}

void MainWindow::restoreState(const GraphState &state) {
    qDebug() << "HISTORY: Восстановление...";
    stopAndReset();

    graph->setDirected(state.isDirected);
    if (directedCheck) directedCheck->setChecked(state.isDirected);

    graph->createFromAdjacencyMatrix(state.matrix);

    updateGraphView();
    updateTableFromGraph();

    for(auto it = state.positions.begin(); it != state.positions.end(); ++it) {
        int id = it.key();
        if (nodes.contains(id)) {
            nodes[id]->setPos(it.value());
        }
    }
}

void MainWindow::undo() {
    if (undoStack.isEmpty()) {
        qDebug() << "HISTORY: Стек пуст";
        return;
    }

    GraphState currentState;
    currentState.matrix = graph->adjacencyMatrix();
    currentState.isDirected = graph->getDirected();
    for(auto it = nodes.begin(); it != nodes.end(); ++it) currentState.positions.insert(it.key(), it.value()->pos());
    redoStack.push(currentState);

    GraphState prevState = undoStack.pop();
    restoreState(prevState);
}

void MainWindow::redo() {
    if (redoStack.isEmpty()) return;

    GraphState currentState;
    currentState.matrix = graph->adjacencyMatrix();
    currentState.isDirected = graph->getDirected();
    for(auto it = nodes.begin(); it != nodes.end(); ++it) currentState.positions.insert(it.key(), it.value()->pos());
    undoStack.push(currentState);

    GraphState nextState = redoStack.pop();
    restoreState(nextState);
}
