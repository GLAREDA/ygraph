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
#include <QEvent>
#include <QStack>
#include <QQueue>
#include <QSet>
#include <QFileDialog>
#include <QToolBar>
#include <QAction>
#include <QTimer>
#include <QRandomGenerator>
#include <QWheelEvent> // Не забудьте добавить этот инклюд в начале файла, если его нет!



#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), graph(new Graph()), updatingMatrix(false)
{
    scene = new QGraphicsScene(this);
    view = new QGraphicsView(scene);
    view = new QGraphicsView(scene);

    // Инициализация UI элементов
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
    traversalButton->setFixedHeight(30);
    startVertexCombo = new QComboBox();
    physicsTimer = new QTimer(this);
    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::onPhysicsUpdate);



    setupUI();
    setupMatrixConnections();

    view->setRenderHint(QPainter::Antialiasing);
    view->setRenderHint(QPainter::SmoothPixmapTransform);
    view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    view->setDragMode(QGraphicsView::RubberBandDrag);
    view->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    // Устанавливаем фильтр событий на viewport (саму область рисования)
    view->viewport()->installEventFilter(this);
    // =============================

    connect(physicsTimer, &QTimer::timeout, this, &MainWindow::onPhysicsUpdate);

    setWindowTitle("Редактор графов (Интерактивный)");
    resize(1200, 800);
}

MainWindow::~MainWindow()
{
    delete graph;
}

void MainWindow::setupUI()
{
    QWidget *centralWidget = new QWidget(this);
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);

    // Левая панель - ввод
    QGroupBox *inputGroup = new QGroupBox("Ввод матрицы");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);

    representationCombo->addItem("Матрица смежности");
    representationCombo->addItem("Матрица инцидентности");
    representationCombo->setCurrentIndex(0);

    for (int i = 2; i <= 10; ++i) {
        sizeCombo->addItem(QString("%1x%1").arg(i), i);
    }
    sizeCombo->setCurrentIndex(1);

    matrixTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    matrixTable->setSelectionMode(QAbstractItemView::SingleSelection);
    matrixTable->setStyleSheet(
        "QTableWidget { font: 12px; background-color: transparent; }"
        "QTableWidget::item { border: 1px solid #dcdcdc; }"
    );
    matrixTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    matrixTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    matrixTable->setFixedHeight(300);
    matrixTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    matrixTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    generateButton->setFixedHeight(30);
    calcButton->setFixedHeight(30);
    resizeButton->setFixedHeight(30);

    hintLabel->setText("Для матрицы инцидентности: 2 единицы в столбце");
    hintLabel->setVisible(false);

    QHBoxLayout *sizeLayout = new QHBoxLayout();
    sizeLayout->addWidget(new QLabel("Количество вершин:"));
    sizeLayout->addWidget(sizeCombo);
    sizeLayout->addWidget(resizeButton);

    inputLayout->addWidget(new QLabel("Тип матрицы:"));
    inputLayout->addWidget(representationCombo);
    inputLayout->addLayout(sizeLayout);
    inputLayout->addWidget(new QLabel("Матрица:"));
    inputLayout->addWidget(matrixTable);
    inputLayout->addWidget(hintLabel);
    inputLayout->addWidget(generateButton);
    inputLayout->addWidget(calcButton);

    // Кнопки алгоритмов
    cycleButton = new QPushButton("Поиск циклов");
    cycleButton->setFixedHeight(30);
    inputLayout->addWidget(cycleButton);

    colorButton = new QPushButton("Раскрасить граф");
    colorButton->setFixedHeight(30);
    inputLayout->addWidget(colorButton);

    bipartiteButton = new QPushButton("Проверить двудольность");
    bipartiteButton->setFixedHeight(30);
    inputLayout->addWidget(bipartiteButton);

    QPushButton *criticalButton = new QPushButton("Показать критические элементы");
    criticalButton->setFixedHeight(30);
    inputLayout->addWidget(criticalButton);
    connect(criticalButton, &QPushButton::clicked, this, &MainWindow::highlightBridgesAndArticulations);

    // Добавляем кнопку для Дейкстры (её не было в setupUI раньше)
    QPushButton *pathButton = new QPushButton("Найти кратчайший путь");
    pathButton->setFixedHeight(30);
    inputLayout->addWidget(pathButton);
    connect(pathButton, &QPushButton::clicked, this, &MainWindow::visualizeShortestPath);

    inputLayout->addWidget(statusLabel);

    QPushButton *randomButton = new QPushButton("Случайный граф");
    randomButton->setFixedHeight(30);
    connect(randomButton, &QPushButton::clicked, this, &MainWindow::onRandomGraphClicked);

    QPushButton *physicsBtn = new QPushButton("Вкл/Выкл Физику");
    physicsBtn->setCheckable(true); // Кнопка-переключатель (залипающая)
    physicsBtn->setFixedHeight(30);

    // Логика: если нажата - запускаем таймер, отжата - останавливаем
    connect(physicsBtn, &QPushButton::toggled, [=](bool checked){
        if(checked) physicsTimer->start(30); // 30 мс = ~30 FPS
        else physicsTimer->stop();
    });

    inputLayout->addWidget(physicsBtn);
    // ==========================
    inputLayout->addWidget(randomButton);
    inputLayout->addWidget(statusLabel);

    // Центральная панель
    QGroupBox *graphGroup = new QGroupBox("Визуализация графа");
    QVBoxLayout *graphLayout = new QVBoxLayout(graphGroup);
    view->setScene(scene);
    graphLayout->addWidget(view);

    // Тулбар экспорта
    exportToolBar = new QToolBar("Экспорт", this);
    saveImageAction = new QAction("Сохр. IMG", this);
    saveDotAction = new QAction("Сохр. DOT", this);
    exportToolBar->addAction(saveImageAction);
    exportToolBar->addAction(saveDotAction);
    addToolBar(Qt::TopToolBarArea, exportToolBar);

    connect(saveImageAction, &QAction::triggered, this, &MainWindow::saveGraphToImage);
    connect(saveDotAction, &QAction::triggered, this, &MainWindow::saveGraphToDotFile);

    // Панель обхода
    inputLayout->addWidget(new QLabel("Начальная вершина:"));
    inputLayout->addWidget(startVertexCombo);
    inputLayout->addWidget(traversalButton);
    connect(traversalButton, &QPushButton::clicked, this, &MainWindow::visualizeTraversal);

    // Правая панель
    QGroupBox *propsGroup = new QGroupBox("Свойства графа");
    QVBoxLayout *propsLayout = new QVBoxLayout(propsGroup);
    graphPropertiesDisplay->setReadOnly(true);
    graphPropertiesDisplay->setStyleSheet("font-family: monospace;");
    propsLayout->addWidget(graphPropertiesDisplay);

    mainLayout->addWidget(inputGroup, 1);
    mainLayout->addWidget(graphGroup, 2);
    mainLayout->addWidget(propsGroup, 1);
    setCentralWidget(centralWidget);

    onRepresentationChanged(0);
}

void MainWindow::onPhysicsUpdate() {
    // Шаг 1: Спросить каждый узел, куда он хочет сдвинуться (расчет сил)
    for (auto node : nodes) node->calculateForces();

    // Шаг 2: Реально сдвинуть узлы на новые места
    for (auto node : nodes) node->advancePosition();
}

void MainWindow::updateGraphView() {

    scene->clear();
    nodes.clear();

    if (!graph || graph->nodeCount() == 0) return;

    // === ИСПРАВЛЕНИЕ ===
    // Вместо view->width()/2 ставим 0.
    // Теперь центром вселенной графа будет математический ноль.
    const qreal centerX = 0;
    const qreal centerY = 0;

    // Радиус начального круга можно сделать поменьше
    qreal circleRadius = 200.0;
    // ===================

    QVector<QPointF> positions;
    for (int i = 0; i < graph->nodeCount(); ++i) {
        qreal angle = 2 * M_PI * i / graph->nodeCount();
        QPointF newPos(centerX + circleRadius * cos(angle), centerY + circleRadius * sin(angle));

        // Разброс (оставляем как есть)
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

    // === ВАЖНО: Направляем камеру в центр (0,0) ===
    view->centerOn(0, 0);
}

void MainWindow::resetEdgeColors() {
    for (auto item : scene->items()) {
        if (auto edge = dynamic_cast<Edge*>(item)) edge->resetColor();
    }
    for (auto node : nodes) node->resetColor();
}

void MainWindow::highlightEdge(int u, int v, const QColor& color) {
    if (!nodes.contains(u) || !nodes.contains(v)) return;
    Node* nodeU = nodes[u];
    Node* nodeV = nodes[v];

    for (auto item : scene->items()) {
        if (auto edge = dynamic_cast<Edge*>(item)) {
            if ((edge->sourceNode() == nodeU && edge->destNode() == nodeV) ||
                (edge->sourceNode() == nodeV && edge->destNode() == nodeU)) {
                edge->setColor(color);
            }
        }
    }
}

void MainWindow::highlightTraversal(const QVector<int>& traversal, const QColor& color) {
    if (traversal.isEmpty()) return;
    resetEdgeColors();

    QTimer *timer = new QTimer(this);
    int index = 0;
    connect(timer, &QTimer::timeout, [=]() mutable {
        if (index < traversal.size()) {
            int vertex = traversal[index];
            if (nodes.contains(vertex)) {
                nodes[vertex]->setColor(color);
                if (index > 0) {
                    highlightEdge(traversal[index-1], vertex, Qt::yellow);
                }
            }
            index++;
        } else {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start(500);
}

QString MainWindow::traversalToString(const QVector<int>& traversal) {
    QStringList vertices;
    for (int v : traversal) vertices << QString::number(v + 1);
    return vertices.join(" → ");
}

void MainWindow::highlightPath(const QVector<int>& path, const QColor& color) {
    if (path.isEmpty()) return;
    resetEdgeColors();

    for (int vertex : path) {
        if (nodes.contains(vertex)) nodes[vertex]->setColor(color);
    }
    for (int i = 0; i < path.size() - 1; ++i) {
        highlightEdge(path[i], path[i+1], Qt::red);
    }
}

void MainWindow::visualizeTraversal() {
    if (!graph || graph->nodeCount() == 0) return;
    int start = startVertexCombo->currentData().toInt();
    if (start < 0 || start >= graph->nodeCount()) return;

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

void MainWindow::visualizeShortestPath() {
    if (!graph || graph->nodeCount() == 0) return;

    bool ok;
    int start = QInputDialog::getInt(this, "Поиск пути",
        QString("От вершины (1-%1):").arg(graph->nodeCount()), 1, 1, graph->nodeCount(), 1, &ok);
    if (!ok) return;

    int end = QInputDialog::getInt(this, "Поиск пути",
        QString("До вершины (1-%1):").arg(graph->nodeCount()), graph->nodeCount(), 1, graph->nodeCount(), 1, &ok);
    if (!ok) return;

    QVector<int> path = graph->dijkstra(start - 1, end - 1);

    if (path.isEmpty()) {
        QMessageBox::information(this, "Результат", "Путь не найден");
    } else {
        QStringList pathStr;
        for (int v : path) pathStr << QString::number(v + 1);
        graphPropertiesDisplay->append(QString("\nПуть %1 -> %2: %3")
                                     .arg(start).arg(end).arg(pathStr.join(" -> ")));
        highlightPath(path, Qt::red);
    }
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

void MainWindow::exportToDotFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
    QTextStream out(&file);
    out << "graph G {\n    node [shape=circle, style=filled, fillcolor=lightblue];\n";
    for (int i = 0; i < graph->nodeCount(); ++i) out << QString("    %1 [label=\"%2\"];\n").arg(i+1).arg(i+1);
    const auto &adjMatrix = graph->adjacencyMatrix();
    for (int i = 0; i < adjMatrix.size(); ++i) {
        for (int j = i; j < adjMatrix[i].size(); ++j) {
            if (adjMatrix[i][j] > 0) {
                out << QString("    %1 -- %2").arg(i+1).arg(j+1);
                if (adjMatrix[i][j] != 1) out << QString(" [label=\"%1\"]").arg(adjMatrix[i][j]);
                out << ";\n";
            }
        }
    }
    out << "}\n";
    file.close();
}

void MainWindow::saveGraphToDotFile() {
    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить DOT", "", "DOT Files (*.dot)");
    if (!fileName.isEmpty()) exportToDotFile(fileName);
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

void MainWindow::checkCycles() {
    if (!graph || graph->nodeCount() == 0) return;
    resetEdgeColors();
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
    QVector<int> colors = graph->greedyColoring();
    QVector<QColor> colorPalette = { Qt::red, Qt::blue, Qt::green, Qt::yellow, Qt::magenta, Qt::cyan };
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        int colorIndex = colors[it.key()] % colorPalette.size();
        it.value()->setColor(colorPalette[colorIndex]);
    }
    graphPropertiesDisplay->append(QString("\nРаскраска завершена. Цветов: %1").arg(*std::max_element(colors.begin(), colors.end()) + 1));
}

void MainWindow::highlightBridgesAndArticulations() {
    if (!graph || graph->nodeCount() == 0) return;
    resetEdgeColors();
    QVector<QPair<int, int>> bridges = graph->getBridges();
    for (const auto& bridge : bridges) highlightEdge(bridge.first, bridge.second, Qt::red);
    QVector<int> articulations = graph->getArticulationPoints();
    for (int v : articulations) if (nodes.contains(v)) nodes[v]->setColor(QColor(255, 165, 0));
    graphPropertiesDisplay->append(QString("\nМостов: %1, Точек сочленения: %2").arg(bridges.size()).arg(articulations.size()));
}
void MainWindow::calculateGraphProperties() {
    if (!graph || graph->nodeCount() == 0) return;

    QString propertiesText;

    // 1. Основные характеристики
    propertiesText += "=== Основные характеристики ===\n";
    propertiesText += QString("Вершин: %1\n").arg(graph->nodeCount());
    propertiesText += QString("Рёбер: %1\n").arg(graph->edgeCount());
    propertiesText += "Тип графа: Неориентированный\n";

    // 2. Степени вершин
    propertiesText += "\n=== Степени вершин ===\n";
    QVector<int> degrees = graph->calculateDegrees();
    for (int i = 0; i < graph->nodeCount(); ++i) {
        propertiesText += QString("Вершина %1: степень %2\n").arg(i+1).arg(degrees[i]);
    }
    propertiesText += QString("Граф %1полный\n").arg(graph->isComplete() ? "" : "не ");

    // 3. Метрические характеристики
    propertiesText += "\n=== Метрические характеристики ===\n";
    QVector<int> ecc = graph->getEccentricities();
    int radius = graph->getRadius();
    int diameter = graph->getDiameter();

    propertiesText += QString("Радиус графа: %1\n").arg(radius);
    propertiesText += QString("Диаметр графа: %1\n").arg(diameter);
    propertiesText += QString("Медиана графа: вершина %1\n").arg(graph->getMedian()+1);
    propertiesText += QString("Число передачи: %1\n").arg(graph->getTransmissionNumber());

    // Центральные вершины
    propertiesText += "\nЦентральные вершины:\n";
    for (int i = 0; i < ecc.size(); ++i) {
        if (ecc[i] == radius) {
            propertiesText += QString("Вершина %1 (эксцентриситет: %2)\n").arg(i+1).arg(ecc[i]);
        }
    }

    // Периферийные вершины
    propertiesText += "\nПериферийные вершины:\n";
    for (int i = 0; i < ecc.size(); ++i) {
        if (ecc[i] == diameter) {
            propertiesText += QString("Вершина %1 (эксцентриситет: %2)\n").arg(i+1).arg(ecc[i]);
        }
    }

    // 2. Связность и специальные свойства
    propertiesText += "\n=== Связность и специальные свойства ===\n";
    propertiesText += QString("Граф %1эйлеров\n").arg(graph->isEulerian() ? "" : "не ");
    propertiesText += QString("Граф %1связный\n").arg(graph->isConnected() ? "" : "не ");

    // 3. Точки сочленения и мосты
    propertiesText += "\n=== Критические элементы ===\n";
    QVector<int> articulationPoints = graph->findArticulationPoints();
    if (!articulationPoints.isEmpty()) {
        QStringList apList;
        for (int v : articulationPoints) {
            apList << QString::number(v + 1);
        }
        propertiesText += QString("Точки сочленения: %1\n").arg(apList.join(", "));
    } else {
        propertiesText += "Точек сочленения нет\n";
    }

    QVector<QPair<int, int>> bridges = graph->findBridges();
    if (!bridges.isEmpty()) {
        QStringList bridgeList;
        for (auto bridge : bridges) {
            bridgeList << QString("(%1-%2)").arg(bridge.first + 1).arg(bridge.second + 1);
        }
        propertiesText += QString("Мосты: %1\n").arg(bridgeList.join(", "));
    } else {
        propertiesText += "Мостов нет\n";
    }

    // Проверка двудольности
    propertiesText += QString("\nДвудольный: %1\n").arg(graph->isBipartite() ? "Да" : "Нет");

    // 5. Матрица расстояний
    propertiesText += "\n=== Матрица расстояний ===\n";
    QVector<QVector<int>> distMatrix = graph->getDistanceMatrix();
    int n = distMatrix.size();

    // Заголовок с номерами вершин
    propertiesText += "    ";
    for (int j = 0; j < n; ++j) {
        propertiesText += QString("%1 ").arg(j+1, 3);
    }
    propertiesText += "\n";

    // Сама матрица
    for (int i = 0; i < n; ++i) {
        propertiesText += QString("%1: ").arg(i+1, 2);
        for (int j = 0; j < n; ++j) {
            if (distMatrix[i][j] == INT_MAX) {
                propertiesText += " ∞ ";
            } else {
                propertiesText += QString("%1 ").arg(distMatrix[i][j], 3);
            }
        }
        propertiesText += "\n";
    }

    propertiesText += "\n=== Компоненты связности ===\n";
    QVector<QVector<int>> components = graph->findConnectedComponents();
    if (components.size() == 1) {
        propertiesText += "Граф связный (1 компонента)\n";
    } else {
        propertiesText += QString("Граф несвязный (%1 компонент)\n").arg(components.size());
        for (int i = 0; i < components.size(); ++i) {
            QStringList vertices;
            for (int v : components[i]) {
                vertices << QString::number(v + 1);
            }
            propertiesText += QString("Компонента %1: %2\n").arg(i+1).arg(vertices.join(", "));
        }
    }

    // Раскраска графа
    QVector<int> colors = graph->greedyColoring();
    int colorsUsed = 0;
    if (!colors.isEmpty()) {
        colorsUsed = *std::max_element(colors.begin(), colors.end()) + 1;
    }
    propertiesText += QString("\nХроматическое число (оценка): %1\n").arg(colorsUsed);

    graphPropertiesDisplay->setPlainText(propertiesText);
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

void MainWindow::checkBipartite() {
    if (!graph || graph->nodeCount() == 0) return;
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

void MainWindow::onGenerateClicked() {
    if (parseMatrix()) updateGraphView();
}

void MainWindow::onRandomGraphClicked() {
    bool ok;
    // 1. Спрашиваем количество вершин (от 2 до 50)
    int count = QInputDialog::getInt(this, "Генерация",
                                   "Количество вершин:",
                                   5, 2, 50, 1, &ok);
    if (!ok) return;

    // 2. Переключаемся в режим "Матрица смежности" (так проще генерировать)
    representationCombo->setCurrentIndex(0);

    // 3. Подготавливаем таблицу
    // (Мы не меняем sizeCombo, так как там фиксированные значения,
    // просто меняем таблицу напрямую)
    resizeMatrixTable(count, count);

    // 4. Заполняем случайными числами
    // Используем вероятность 30% для ребра, иначе граф превратится в кашу
    int density = 30;

    // Блокируем обновления, чтобы не тормозило при заполнении
    updatingMatrix = true;

    for (int i = 0; i < count; ++i) {
        for (int j = i + 1; j < count; ++j) {
            // Генерируем число от 0 до 99
            if (QRandomGenerator::global()->bounded(100) < density) {
                // Ставим единицы симметрично
                if (auto item1 = matrixTable->item(i, j)) item1->setText("1");
                if (auto item2 = matrixTable->item(j, i)) item2->setText("1");
            } else {
                if (auto item1 = matrixTable->item(i, j)) item1->setText("0");
                if (auto item2 = matrixTable->item(j, i)) item2->setText("0");
            }
        }
    }

    updatingMatrix = false;

    // 5. Строим граф (вызываем ту же функцию, что и кнопка "Построить")
    onGenerateClicked();

    // Пишем в статус
    statusLabel->setText(QString("Сгенерирован граф: %1 вершин").arg(count));
}


bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    // Если событие произошло в окне просмотра (view) и это прокрутка колесика
    if (obj == view->viewport() && event->type() == QEvent::Wheel) {

        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);

        // Коэффициент зума (1.15 = 15% увеличения)
        const double scaleFactor = 1.15;

        if (wheelEvent->angleDelta().y() > 0) {
            // Крутим от себя -> Приближаем
            view->scale(scaleFactor, scaleFactor);
        } else {
            // Крутим на себя -> Отдаляем
            view->scale(1.0 / scaleFactor, 1.0 / scaleFactor);
        }

        return true; // Сообщаем Qt, что мы обработали событие (чтобы он не скроллил)
    }

    return QMainWindow::eventFilter(obj, event);
}
