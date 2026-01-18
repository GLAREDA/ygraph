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
#include <QMenu>
#include <QSettings>
#include <QCoreApplication>
#include <QColorDialog>
#include <QStyle>
#include <QPixmap>
#include <QApplication> // <--- ОБЯЗАТЕЛЬНО

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), graph(new Graph()), updatingMatrix(false)
{
    qDebug() << "APP: Запуск программы";

    loadSettings();

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

void MainWindow::logAction(const QString& message) {
    QString time = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString logMsg = QString("[%1] %2").arg(time, message);

    actionLog.append(logMsg); // Сохраняем в память

    // Выводим пользователю (в свойства или консоль)
    graphPropertiesDisplay->append(logMsg);
    qDebug() << logMsg;
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

    // Обработка ПРАВОГО клика (Только если режим V2 включен в конфиге)
    if (obj == view->viewport() && event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->button() == Qt::RightButton) {

            // Если в конфиге EditMode=1 -> показываем меню
            if (currentEditMode == 1) {
                showContextMenu(mouseEvent->globalPos());
                return true; // Блокируем стандартное меню
            }
        }
    }

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
             logAction(QString("Удалена вершина %1").arg(id+1));
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
        logAction(QString("Тип графа изменен на: %1").arg(checked ? "Ориентированный" : "Неориентированный"));
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
    QPushButton *mstButton = new QPushButton("Минимальное остовное дерево (MST)");
    connect(mstButton, &QPushButton::clicked, this, &MainWindow::visualizeMST);

    layoutAlgo->addWidget(new QLabel("<b>Оптимизация:</b>")); // Новый заголовок
    layoutAlgo->addWidget(mstButton);

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
    layoutCombo->addItem("Дерево (Hierarchical)");
    layoutCombo->addItem("Двудольная (Bipartite)");   // 4
    layoutCombo->addItem("Звезда (Star/Wheel)");      // 5
    layoutCombo->addItem("Спектральная (Spectral)");  // 6
    layoutCombo->addItem("Умное кольцо (Sorted)");

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

    QWidget *pageMath = new QWidget();
    QVBoxLayout *layoutMath = new QVBoxLayout(pageMath);

    QPushButton *calcChromBtn = new QPushButton("Хроматический многочлен");
    connect(calcChromBtn, &QPushButton::clicked, this, &MainWindow::calculateChromPolynomial);

    QLabel *mathInfo = new QLabel("Внимание: Алгоритм экспоненциальный!\nРаботает только для графов < 13 вершин.");
    mathInfo->setStyleSheet("color: gray; font-style: italic;");
    mathInfo->setWordWrap(true);

    layoutMath->addWidget(new QLabel("<b>Алгебраическая теория:</b>"));
    layoutMath->addWidget(calcChromBtn);
    layoutMath->addWidget(mathInfo);

    layoutMath->addStretch();
    toolsPanel->addItem(pageMath, "4. Математика");


    // СБОРКА ОКОН
    QGroupBox *graphGroup = new QGroupBox("Граф");
    QVBoxLayout *graphLayout = new QVBoxLayout(graphGroup);
    graphLayout->addWidget(view);

    QGroupBox *propsGroup = new QGroupBox("Свойства и Лог");
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

    // === ДОБАВЛЕНЫ НОВЫЕ КНОПКИ ===
    QAction *saveProjectAction = new QAction("Сохр. Проект", this);
    QAction *loadProjectAction = new QAction("Загр. Проект", this);

    // Кнопки картинок
    saveImageAction = new QAction("Экспорт IMG", this);
    saveDotAction = new QAction("Экспорт DOT", this);

    exportToolBar->addAction(saveProjectAction);
    exportToolBar->addAction(loadProjectAction);
    exportToolBar->addSeparator();
    exportToolBar->addAction(saveImageAction);
    exportToolBar->addAction(saveDotAction);

    addToolBar(Qt::TopToolBarArea, exportToolBar);

    connect(saveImageAction, &QAction::triggered, this, &MainWindow::saveGraphToImage);
    connect(saveDotAction, &QAction::triggered, this, &MainWindow::saveGraphToDotFile);

    // Подключение новых кнопок
    connect(saveProjectAction, &QAction::triggered, this, &MainWindow::saveProject);
    connect(loadProjectAction, &QAction::triggered, this, &MainWindow::loadProject);

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
    qDebug() << "VIEW: Rebuilding graph...";

    // 1. Сохраняем позиции И ЦВЕТА
    QVector<QPointF> oldPositions;
    QVector<QColor> oldColors; // <--- Добавили массив цветов

    for (int i = 0; i < nodes.size(); ++i) {
        if (nodes.contains(i)) {
            oldPositions.append(nodes[i]->pos());
            oldColors.append(nodes[i]->getBaseColor()); // Сохраняем цвет
        } else {
            oldPositions.append(QPointF(0,0));
            oldColors.append(QColor(70, 130, 180)); // Заглушка
        }
    }

    // 2. Строим новый граф (цвета сбросятся на синий)
    updateGraphView();
    updateTableFromGraph();

    // 3. Восстанавливаем позиции И ЦВЕТА
    int oldIdx = 0;
    int newNodesCount = nodes.size();

    for (int newIdx = 0; newIdx < newNodesCount; ++newIdx) {
        if (oldIdx == removedId) oldIdx++; // Пропускаем удаленный

        if (oldIdx < oldPositions.size() && nodes.contains(newIdx)) {
            nodes[newIdx]->setPos(oldPositions[oldIdx]);

            // Восстанавливаем цвет!
            nodes[newIdx]->setBaseColor(oldColors[oldIdx]);
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

    logAction("Выполнен обход: " + traversalToString(traversal));
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
        logAction(cyclesInfo);
    } else {
        logAction("Циклы не найдены (ациклический).");
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
    logAction("Раскраска выполнена.");
}

void MainWindow::highlightBridgesAndArticulations() {
    if (!graph || graph->nodeCount() == 0) return;
    stopAndReset();
    QVector<QPair<int, int>> bridges = graph->getBridges();
    for (const auto& bridge : bridges) highlightEdge(bridge.first, bridge.second, Qt::red);
    QVector<int> articulations = graph->getArticulationPoints();
    for (int v : articulations) if (nodes.contains(v)) nodes[v]->setColor(QColor(255, 165, 0));

    QString msg = QString("Найдено: %1 мостов, %2 точек сочленения").arg(bridges.size()).arg(articulations.size());
    logAction(msg);
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
        logAction(QString("Найден кратчайший путь: %1").arg(pathStr.join(" -> ")));
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
        logAction("Результат проверки: Граф двудольный");
    } else {
        logAction("Результат проверки: Граф не двудольный");
    }
}

void MainWindow::onRandomGraphClicked() {
    bool ok;
    int count = QInputDialog::getInt(this, "Генерация", "Количество вершин:", 5, 2, 50, 1, &ok);
    if (!ok) return;

    bool makeDirected = directedCheck->isChecked();
    graph->setDirected(makeDirected);

    representationCombo->setCurrentIndex(0);
    resizeMatrixTable(count, count);
    updatingMatrix = true;

    saveToHistory();

    for(int i=0; i<count; ++i)
        for(int j=0; j<count; ++j)
            if(auto it = matrixTable->item(i,j)) it->setText("0");

    if (makeDirected) {
        for (int i = 0; i < count; ++i) {
            for (int j = 0; j < count; ++j) {
                if (i == j) continue;
                if (QRandomGenerator::global()->bounded(100) < 20) {
                    if (auto item = matrixTable->item(i, j)) item->setText("1");
                }
            }
        }
    } else {
        for (int i = 0; i < count; ++i) {
            for (int j = i + 1; j < count; ++j) {
                if (QRandomGenerator::global()->bounded(100) < 30) {
                    if (auto item1 = matrixTable->item(i, j)) item1->setText("1");
                    if (auto item2 = matrixTable->item(j, i)) item2->setText("1");
                }
            }
        }
    }

    updatingMatrix = false;
    onGenerateClicked();

    logAction(QString("Сгенерирован %1 граф (%2 вершин)").arg(makeDirected ? "орграф" : "обычный").arg(count));
}

// Добавьте в начало файла, если нет
#include <QtMath>

void MainWindow::applyLayout()
{
    if (nodes.isEmpty()) return;

    // Выключаем физику
    if (physicsTimer->isActive()) {
        physicsTimer->stop();
        QList<QPushButton*> btns = this->findChildren<QPushButton*>();
        for(auto b : btns) if(b->text().contains("Физика")) b->setChecked(false);
    }

    int type = layoutCombo->currentIndex();
    int n = nodes.size();
    QParallelAnimationGroup *animGroup = new QParallelAnimationGroup;
    bool animate = useAnimationCheckbox->isChecked();

    QMap<int, QPointF> newPositions;

    // Вспомогательные переменные
    const auto& matrix = graph->adjacencyMatrix();
    int cx = 0, cy = 0;

    // --- 0. CIRCULAR (По кругу) ---
    if (type == 0) {
        double radius = qMax(200.0, n * 40.0);
        for (int i = 0; i < n; ++i) {
            double angle = 2 * M_PI * i / n;
            newPositions[i] = QPointF(radius * cos(angle), radius * sin(angle));
        }
    }
    // --- 1. GRID (Сетка) ---
    else if (type == 1) {
        int cols = ceil(sqrt(n));
        int spacing = 150;
        double offsetX = (cols - 1) * spacing / 2.0;
        double offsetY = (ceil((double)n/cols) - 1) * spacing / 2.0;
        for (int i = 0; i < n; ++i) {
            newPositions[i] = QPointF((i % cols) * spacing - offsetX, (i / cols) * spacing - offsetY);
        }
    }
    // --- 2. RANDOM (Случайная) ---
    else if (type == 2) {
        for (int i = 0; i < n; ++i) {
            newPositions[i] = QPointF(QRandomGenerator::global()->bounded(-400, 401),
                                      QRandomGenerator::global()->bounded(-400, 401));
        }
    }
    // --- 3. HIERARCHICAL (Дерево) ---
    else if (type == 3) {
        // (Ваш старый код BFS по уровням, который я давал раньше. Если его нет, скопируйте из прошлого ответа или используйте заглушку)
        // Для краткости здесь упрощенная версия:
        QVector<int> levels(n, -1);
        QQueue<int> q; q.enqueue(0); levels[0] = 0;
        QVector<bool> vis(n, false); vis[0] = true;

        while(!q.isEmpty()){
            int u = q.dequeue();
            for(int v=0; v<n; ++v) if(matrix[u][v] && !vis[v]) {
                vis[v]=true; levels[v]=levels[u]+1; q.enqueue(v);
            }
        }

        QMap<int, int> levelCounts;
        for(int i=0; i<n; ++i) {
            int lvl = (levels[i] == -1) ? 0 : levels[i]; // Изолированные на 0 уровень
            int posInLevel = levelCounts[lvl]++;
            newPositions[i] = QPointF(posInLevel * 100 - (lvl * 50), lvl * 120 - 200);
        }
    }
    // --- 4. BIPARTITE (Двудольная) ---
    else if (type == 4) {
        if (!graph->isBipartite()) {
            QMessageBox::warning(this, "Ошибка", "Граф не является двудольным!");
            // Фолбэк на круговую
            layoutCombo->setCurrentIndex(0); applyLayout(); return;
        }

        QVector<int> colors = graph->bipartiteColoring();
        int leftCount = 0, rightCount = 0;

        for (int i = 0; i < n; ++i) {
            if (colors[i] == 0) {
                newPositions[i] = QPointF(-200, leftCount * 80 - 200);
                leftCount++;
            } else {
                newPositions[i] = QPointF(200, rightCount * 80 - 200);
                rightCount++;
            }
        }
        // Центровка по вертикали
        double leftOffset = (leftCount * 80) / 2.0;
        double rightOffset = (rightCount * 80) / 2.0;
        for (int i = 0; i < n; ++i) {
            if (colors[i] == 0) newPositions[i] += QPointF(0, -leftOffset + 200 + 40);
            else newPositions[i] += QPointF(0, -rightOffset + 200 + 40);
        }
    }
    // --- 5. STAR / WHEEL (Звезда) ---
    else if (type == 5) {
        // Ищем вершину с макс степенью
        QVector<int> degrees = graph->calculateDegrees();
        int centerNode = 0;
        int maxDeg = -1;
        for(int i=0; i<n; ++i) if(degrees[i] > maxDeg) { maxDeg = degrees[i]; centerNode = i; }

        newPositions[centerNode] = QPointF(0, 0);

        double radius = 250;
        int current = 0;
        int outerCount = n - 1;
        if (outerCount < 1) outerCount = 1;

        for (int i = 0; i < n; ++i) {
            if (i == centerNode) continue;
            double angle = 2 * M_PI * current / outerCount;
            newPositions[i] = QPointF(radius * cos(angle), radius * sin(angle));
            current++;
        }
    }
    // --- 6. TUTTE / BARYCENTRIC (Исправленная "Спектральная") ---
    else if (type == 6) {
        // 1. Выбираем 3 "якоря" (Anchor Nodes), чтобы растянуть граф
        // Берем вершины с равным шагом, чтобы они были распределены
        int anchor1 = 0;
        int anchor2 = n / 3;
        int anchor3 = (2 * n) / 3;

        // Если вершин мало, логика простая, но для n >= 3 это сработает
        if (n < 3) { // Фолбэк для 2 вершин
             newPositions[0] = QPointF(-100, 0);
             if (n > 1) newPositions[1] = QPointF(100, 0);
        }
        else {
            // 2. Расставляем якоря треугольником
            double radius = 250;
            newPositions[anchor1] = QPointF(0, -radius);              // Верх
            newPositions[anchor2] = QPointF(-radius * 0.866, radius * 0.5); // Лево-Низ
            newPositions[anchor3] = QPointF(radius * 0.866, radius * 0.5);  // Право-Низ

            // Инициализируем остальные в центре (чтобы не улетели при старте)
            for (int i = 0; i < n; ++i) {
                if (i != anchor1 && i != anchor2 && i != anchor3) {
                    newPositions[i] = QPointF(0, 0);
                }
            }

            // 3. Итерации (Релаксация)
            // Все узлы, кроме якорей, двигаются в среднюю точку своих соседей
            for (int iter = 0; iter < 100; ++iter) {
                for (int i = 0; i < n; ++i) {
                    // Якоря не трогаем! Они держат "каркас"
                    if (i == anchor1 || i == anchor2 || i == anchor3) continue;

                    double sumX = 0, sumY = 0;
                    int count = 0;

                    for (int j = 0; j < n; ++j) {
                        // Если есть связь (ориентированная или нет - считаем как неориент)
                        bool connected = (matrix[i][j] != 0);
                        if (!connected && graph->getDirected()) connected = (matrix[j][i] != 0);

                        if (connected) {
                            sumX += newPositions[j].x();
                            sumY += newPositions[j].y();
                            count++;
                        }
                    }

                    if (count > 0) {
                        newPositions[i] = QPointF(sumX / count, sumY / count);
                    }
                }
            }
        }
    }
    // --- 7. SORTED CIRCLE (Умное кольцо) ---
    else if (type == 7) {
        // Сортируем вершины по степени
        QVector<int> degrees = graph->calculateDegrees();
        QVector<int> sortedIndices(n);
        std::iota(sortedIndices.begin(), sortedIndices.end(), 0);

        std::sort(sortedIndices.begin(), sortedIndices.end(), [&](int a, int b){
            return degrees[a] > degrees[b];
        });

        double radius = qMax(200.0, n * 40.0);
        for (int i = 0; i < n; ++i) {
            int nodeId = sortedIndices[i];
            double angle = 2 * M_PI * i / n;
            newPositions[nodeId] = QPointF(radius * cos(angle), radius * sin(angle));
        }
    }

    // === ПРИМЕНЕНИЕ ===
    for (int i = 0; i < n; ++i) {
        if (!nodes.contains(i)) continue;

        QPointF target = newPositions.value(i, QPointF(0,0));

        if (animate) {
            QPropertyAnimation *anim = new QPropertyAnimation(nodes[i], "pos");
            anim->setDuration(1500);
            anim->setStartValue(nodes[i]->pos());
            anim->setEndValue(target);
            anim->setEasingCurve(QEasingCurve::OutExpo);
            animGroup->addAnimation(anim);
        } else {
            nodes[i]->setPos(target);
        }
    }

    if (animate) animGroup->start(QAbstractAnimation::DeleteWhenStopped);
    else { delete animGroup; view->centerOn(0, 0); }
}
// === ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ (Свойства и Матрица) ===

void MainWindow::calculateGraphProperties() {
    if (!graph || graph->nodeCount() == 0) return;

    QString text = "=== Основные характеристики ===\n";
    text += QString("Вершин: %1\n").arg(graph->nodeCount());
    text += QString("Рёбер: %1\n").arg(graph->edgeCount());

    QString typeStr = graph->getDirected() ? "Ориентированный" : "Неориентированный";
    text += QString("Тип: %1\n").arg(typeStr);

    QVector<int> degrees = graph->calculateDegrees();
    text += "\n=== Степени вершин ===\n";
    for(int i=0; i<degrees.size(); ++i) {
        text += QString("В%1: %2\n").arg(i+1).arg(degrees[i]);
    }

    text += QString("Полный: %1\n").arg(graph->isComplete() ? "Да" : "Нет");

    int rad = graph->getRadius();
    int diam = graph->getDiameter();

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
        for(int j=0; j<dist[i].size(); ++j) {
            text += (dist[i][j]==INT_MAX ? "  ∞ " : QString("%1 ").arg(dist[i][j], 3));
        }
        text += "\n";
    }

    auto comp = graph->findConnectedComponents();
    text += QString("\nКомпонент связности: %1\n").arg(comp.size());
    for(int i=0; i<comp.size(); ++i) {
        QStringList l;
        for(int v : comp[i]) l << QString::number(v+1);
        text += QString("К%1: %2\n").arg(i+1).arg(l.join(", "));
    }

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
    if (parseMatrix()) {
        updateGraphView();
        logAction("Граф перестроен из таблицы.");
    }
}

// === ИСТОРИЯ (UNDO/REDO) ===


void MainWindow::saveToHistory() {
    if (graph->nodeCount() == 0) {
        return;
    }
    qDebug() << "HISTORY: Сохранение состояния...";
    GraphState state;
    state.matrix = graph->adjacencyMatrix();
    state.isDirected = graph->getDirected();

    for(auto it = nodes.begin(); it != nodes.end(); ++it) {
        state.positions.insert(it.key(), it.value()->pos());
        state.colors.insert(it.key(), it.value()->getBaseColor());
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

            if (state.colors.contains(id)) {
                nodes[id]->setBaseColor(state.colors[id]);
            }
        }
    }
}

void MainWindow::undo() {
    if (undoStack.isEmpty()) {
        qDebug() << "HISTORY: Стек пуст";
        return;
    }

    // 1. Сохраняем ТЕКУЩЕЕ состояние в стек Redo (чтобы можно было вернуться)
    GraphState currentState;
    currentState.matrix = graph->adjacencyMatrix();
    currentState.isDirected = graph->getDirected();

    for(auto it = nodes.begin(); it != nodes.end(); ++it) {
        currentState.positions.insert(it.key(), it.value()->pos());
        currentState.colors.insert(it.key(), it.value()->getBaseColor()); // <--- ДОБАВЛЕНО
    }

    redoStack.push(currentState);

    // 2. Восстанавливаем прошлое
    GraphState prevState = undoStack.pop();
    restoreState(prevState);
}

void MainWindow::redo() {
    if (redoStack.isEmpty()) return;

    // 1. Сохраняем ТЕКУЩЕЕ состояние в стек Undo
    GraphState currentState;
    currentState.matrix = graph->adjacencyMatrix();
    currentState.isDirected = graph->getDirected();

    for(auto it = nodes.begin(); it != nodes.end(); ++it) {
        currentState.positions.insert(it.key(), it.value()->pos());
        currentState.colors.insert(it.key(), it.value()->getBaseColor()); // <--- ДОБАВЛЕНО
    }

    undoStack.push(currentState);

    // 2. Восстанавливаем будущее
    GraphState nextState = redoStack.pop();
    restoreState(nextState);
}
// mainwindow.cpp

// 1. Загрузка из конфига

void MainWindow::loadSettings() {
    QString iniPath = QCoreApplication::applicationDirPath() + "/config.ini";
    QSettings settings(iniPath, QSettings::IniFormat);

    // Читаем режим (по умолчанию 0)
    currentEditMode = settings.value("Editor/EditMode", 0).toInt();

    // Если настройки нет - создаем запись, чтобы пользователь знал о ней
    if (!settings.contains("Editor/EditMode")) {
        settings.setValue("Editor/EditMode", 0);
        settings.sync();
    }
    qDebug() << "Settings: Loaded EditMode =" << currentEditMode;
}
// 2. Изменение веса ребра (для меню)
void MainWindow::changeEdgeWeight(Edge* edge) {
    if (!edge) return;

    bool ok;
    // Берем текущий вес из матрицы
    int u = edge->sourceNode()->getId();
    int v = edge->destNode()->getId();
    int oldWeight = graph->adjacencyMatrix()[u][v];

    int newWeight = QInputDialog::getInt(this, "Вес ребра",
                                       "Введите новый вес:",
                                       oldWeight, -10000, 10000, 1, &ok);
    if (ok) {
        saveToHistory();
        graph->addEdge(u, v, newWeight);
        rebuildGraphKeepPositions();
        qDebug() << "Edit: Weight changed to" << newWeight;
    }
}
// 3. Показ контекстного меню
void MainWindow::showContextMenu(const QPoint& pos) {
    QMenu contextMenu(this);

    // Стиль для иконок
    auto style = QApplication::style();

    // Определяем объект под курсором
    QPointF scenePos = view->mapToScene(view->mapFromGlobal(pos));
    QGraphicsItem *item = scene->itemAt(scenePos, QTransform());

    // --- ЛЯМБДА ДЛЯ СОЗДАНИЯ МЕНЮ ЦВЕТОВ ---
    // Это создает красивое подменю с цветными квадратиками
    auto addColorMenu = [&](QMenu* parent, std::function<void(QColor)> callback) {
        QMenu* colorMenu = parent->addMenu(style->standardIcon(QStyle::SP_DesktopIcon), "Изменить цвет");

        // Список быстрых цветов
        QList<QPair<QString, QColor>> colors = {
            {"Красный", Qt::red}, {"Синий", Qt::blue}, {"Зеленый", Qt::green},
            {"Желтый", Qt::yellow}, {"Оранжевый", QColor(255, 165, 0)},
            {"Фиолетовый", Qt::magenta}, {"Серый", Qt::gray}
        };

        for (auto pair : colors) {
            // Рисуем маленький цветной квадратик для иконки
            QPixmap pixmap(16, 16);
            pixmap.fill(pair.second);

            QAction* action = colorMenu->addAction(QIcon(pixmap), pair.first);
            connect(action, &QAction::triggered, [=](){ callback(pair.second); });
        }

        colorMenu->addSeparator();
        QAction* customAction = colorMenu->addAction("Выбрать другой...");
        connect(customAction, &QAction::triggered, [=](){
            QColor c = QColorDialog::getColor(Qt::white, this, "Выберите цвет");
            if(c.isValid()) callback(c);
        });
    };

    // ==========================================
    // ВАРИАНТ 1: УЗЕЛ
    // ==========================================
    if (Node *node = dynamic_cast<Node*>(item)) {
        // Заголовок (неактивный пункт)
        QAction* title = contextMenu.addAction(QString("Узел #%1").arg(node->getId() + 1));
        title->setEnabled(false);
        contextMenu.addSeparator();

        // 1. Связь
        QAction *actConnect = contextMenu.addAction(
            style->standardIcon(QStyle::SP_ArrowRight), "Начать связь");

        connect(actConnect, &QAction::triggered, [=](){
            selectedNode = node;
            node->setColor(Qt::green);
            statusLabel->setText("Режим V2: Кликните левой кнопкой по второму узлу");
        });

        // 2. Цвет (Подменю)
        addColorMenu(&contextMenu, [=](QColor c){
            saveToHistory();
            node->setBaseColor(c);
        });

        contextMenu.addSeparator();

        // 3. Удаление
        QAction *actDelete = contextMenu.addAction(
            style->standardIcon(QStyle::SP_TrashIcon), "Удалить узел");

        connect(actDelete, &QAction::triggered, [=](){
            saveToHistory();
            int id = node->getId();
            clearSelectionState();
            graph->removeVertex(id);
            rebuildGraphKeepPositions(id);
        });
    }

    // ==========================================
    // ВАРИАНТ 2: РЕБРО
    // ==========================================
    else if (Edge *edge = dynamic_cast<Edge*>(item)) {
        // Узнаем вес для заголовка
        int u = edge->sourceNode()->getId();
        int v = edge->destNode()->getId();
        int weight = graph->adjacencyMatrix()[u][v];

        QAction* title = contextMenu.addAction(QString("Ребро (%1-%2)").arg(u+1).arg(v+1));
        title->setEnabled(false);
        contextMenu.addSeparator();

        // 1. Вес
        QAction *actWeight = contextMenu.addAction(
            style->standardIcon(QStyle::SP_FileDialogDetailedView),
            QString("Изменить вес (Тек: %1)").arg(weight));

        connect(actWeight, &QAction::triggered, [=](){ changeEdgeWeight(edge); });

        // 2. Цвет
        addColorMenu(&contextMenu, [=](QColor c){
            edge->setColor(c);
        });

        contextMenu.addSeparator();

        // 3. Удаление
        QAction *actDelete = contextMenu.addAction(
            style->standardIcon(QStyle::SP_DialogCancelButton), "Удалить связь");

        connect(actDelete, &QAction::triggered, [=](){
            saveToHistory();
            graph->removeEdge(u, v);
            rebuildGraphKeepPositions();
        });
    }

    // ==========================================
    // ВАРИАНТ 3: ПУСТОТА
    // ==========================================
    else {
        QAction *actAdd = contextMenu.addAction(
            style->standardIcon(QStyle::SP_FileIcon), "Добавить вершину");

        connect(actAdd, &QAction::triggered, [=](){
            saveToHistory();
            graph->addVertex();
            rebuildGraphKeepPositions();
            int lastIdx = graph->nodeCount() - 1;
            if (nodes.contains(lastIdx)) nodes[lastIdx]->setPos(scenePos);
        });

        contextMenu.addSeparator();
        QAction *actLayout = contextMenu.addAction("Выровнять (Сетка)");
        connect(actLayout, &QAction::triggered, [=](){
            layoutCombo->setCurrentIndex(1); // Grid
            applyLayout();
        });
    }

    // Показываем
    contextMenu.exec(pos);
}

void MainWindow::saveProject() {
    if (!graph) return;

    QString fileName = QFileDialog::getSaveFileName(this, "Сохранить проект", "", "Graph Project (*.json)");
    if (fileName.isEmpty()) return;

    if (!fileName.endsWith(".json", Qt::CaseInsensitive)) {
        fileName += ".json";
    }

    QJsonObject rootObj;
    rootObj["directed"] = graph->getDirected();

    QJsonArray nodesArray;
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        QJsonObject nodeObj;
        nodeObj["id"] = it.key();
        nodeObj["x"] = it.value()->pos().x();
        nodeObj["y"] = it.value()->pos().y();
        nodeObj["color"] = it.value()->getBaseColor().name();
        nodesArray.append(nodeObj);
    }
    rootObj["nodes"] = nodesArray;

    QJsonArray edgesArray;
    const auto& matrix = graph->adjacencyMatrix();
    bool isDirected = graph->getDirected();

    for (int i = 0; i < matrix.size(); ++i) {
        int startJ = isDirected ? 0 : i;
        for (int j = startJ; j < matrix[i].size(); ++j) {
            if (matrix[i][j] != 0) {
                QJsonObject edgeObj;
                edgeObj["u"] = i;
                edgeObj["v"] = j;
                edgeObj["w"] = matrix[i][j];
                edgesArray.append(edgeObj);
            }
        }
    }
    rootObj["edges"] = edgesArray;

    QJsonArray logsArray;
    for (const QString &msg : actionLog) {
        logsArray.append(msg);
    }
    rootObj["logs"] = logsArray;

    QJsonDocument doc(rootObj);
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        logAction("Проект успешно сохранен.");
    }
}

void MainWindow::loadProject() {
    QString fileName = QFileDialog::getOpenFileName(this, "Загрузить проект", "", "Graph Project (*.json)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) return;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) return;

    QJsonObject rootObj = doc.object();

    stopAndReset();
    actionLog.clear();
    graphPropertiesDisplay->clear();

    bool directed = rootObj["directed"].toBool();
    graph->setDirected(directed);
    if (directedCheck) directedCheck->setChecked(directed);

    QJsonArray nodesArr = rootObj["nodes"].toArray();
    int nodeCount = nodesArr.size();

    QVector<QVector<int>> matrix(nodeCount, QVector<int>(nodeCount, 0));

    QJsonArray edgesArr = rootObj["edges"].toArray();
    for (const auto &val : edgesArr) {
        QJsonObject edge = val.toObject();
        int u = edge["u"].toInt();
        int v = edge["v"].toInt();
        int w = edge["w"].toInt();
        if (u < nodeCount && v < nodeCount) {
            matrix[u][v] = w;
            if (!directed) matrix[v][u] = w;
        }
    }

    graph->createFromAdjacencyMatrix(matrix);

    updateGraphView();

    for (const auto &val : nodesArr) {
        QJsonObject nodeObj = val.toObject();
        int id = nodeObj["id"].toInt();
        if (nodes.contains(id)) {
            nodes[id]->setPos(nodeObj["x"].toDouble(), nodeObj["y"].toDouble());
            if (nodeObj.contains("color")) {
                QColor c(nodeObj["color"].toString());
                if (c.isValid()) nodes[id]->setBaseColor(c);
           }
        }
    }

    QJsonArray logsArr = rootObj["logs"].toArray();
    graphPropertiesDisplay->append("<b>=== ИСТОРИЯ ПРОЕКТА ===</b>");
    for (const auto &val : logsArr) {
        QString msg = val.toString();
        actionLog.append(msg);
        graphPropertiesDisplay->append(msg);
    }

    updateTableFromGraph();
    logAction("Проект загружен.");
}

void MainWindow::visualizeMST() {
    if (!graph || graph->nodeCount() == 0) return;

    // Сбрасываем цвета, но НЕ останавливаем физику (можно смотреть MST на живом графе)
    resetEdgeColors();

    QVector<QPair<int, int>> mst = graph->getPrimMST();

    if (mst.isEmpty()) {
        logAction("MST не найдено (граф пуст или изолирован).");
        return;
    }

    // Подсветка
    int totalWeight = 0;
    for (auto edge : mst) {
        int u = edge.first;
        int v = edge.second;
        highlightEdge(u, v, QColor(255, 140, 0)); // Оранжевый

        // Суммируем вес (учитываем, что матрица может быть ориентированной)
        int w = graph->adjacencyMatrix()[u][v];
        if (w == 0 && !graph->getDirected()) w = graph->adjacencyMatrix()[v][u];
        totalWeight += w;
    }

    logAction(QString("Построено Минимальное Остовное Дерево. Общий вес: %1").arg(totalWeight));
}

void MainWindow::calculateChromPolynomial() {
    if (!graph) return;

    if (graph->nodeCount() > 12) {
        QMessageBox::warning(this, "Слишком сложно",
            "Граф слишком большой (>12 вершин).\nВычисление многочлена займет вечность.");
        return;
    }

    // Считаем
    QApplication::setOverrideCursor(Qt::WaitCursor); // Часики
    Polynomial poly = graph->getChromaticPolynomial();
    QApplication::restoreOverrideCursor();

    QString rawString = poly.toString();

    // Заменяем HTML-степени на обычный значок ^
    // Пример: x<sup>2</sup> -> x^2
    rawString.replace("<sup>", "^");
    rawString.replace("</sup>", "");

    QString result = QString("\n=== Хроматический многочлен ===\n%1").arg(rawString);

    logAction(result);

    logAction("Вычислен хроматический многочлен.");
}
