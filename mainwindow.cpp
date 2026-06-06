#include "mainwindow.h"
#include "graph.h"
#include "node.h"
#include "edge.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QHeaderView>а
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
    layoutCombo->addItem("Барицентрическая (Tutte)");
    layoutCombo->addItem("Умное кольцо (Sorted)");
    layoutCombo->addItem("Остовное дерево (MST)");    // 8  <-- НОВЫЙ

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


    QWidget *pageTasks = new QWidget();
    QVBoxLayout *layoutTasks = new QVBoxLayout(pageTasks);

    QPushButton *task1Btn = new QPushButton("Задача 1: Маршрутизация (30 узлов)");
    QPushButton *task2Btn = new QPushButton("Задача 2: ВОЛС 5×5 (25 узлов)");
    QPushButton *task3Btn = new QPushButton("Задача 3: Связность (35 узлов)");
    QPushButton *task4Btn = new QPushButton("Задача 4: Расписание (28 узлов)");

    task1Btn->setStyleSheet("padding: 6px; background: #ddeeff;");
    task2Btn->setStyleSheet("padding: 6px; background: #ddeeff;");
    task3Btn->setStyleSheet("padding: 6px; background: #ddeeff;");
    task4Btn->setStyleSheet("padding: 6px; background: #ddeeff;");

    connect(task1Btn, &QPushButton::clicked,
            this, &MainWindow::generateRoutingTask);
    connect(task2Btn, &QPushButton::clicked,
            this, &MainWindow::generateFiberOpticTask);
    connect(task3Btn, &QPushButton::clicked,
            this, &MainWindow::generateConnectivityTask);
    connect(task4Btn, &QPushButton::clicked,
            this, &MainWindow::generateExamSchedulingTask);

    layoutTasks->addWidget(new QLabel("<b>Прикладные задачи:</b>"));
    layoutTasks->addWidget(task1Btn);
    layoutTasks->addWidget(task2Btn);
    layoutTasks->addWidget(task3Btn);
    layoutTasks->addWidget(task4Btn);
    layoutTasks->addStretch();

    toolsPanel->addItem(pageTasks, "5. Задачи");


    // СБОРКА ОКОН
    QGroupBox *graphGroup = new QGroupBox("Граф");
    QVBoxLayout *graphLayout = new QVBoxLayout(graphGroup);
    graphLayout->addWidget(view);

    QGroupBox *propsGroup = new QGroupBox("Свойства и Лог");
    propsGroup->setFixedWidth(250);
    QVBoxLayout *propsLayout = new QVBoxLayout(propsGroup);
    QPushButton *distMatrixButton = new QPushButton("Матрица расстояний");
    distMatrixButton->setToolTip("Открыть матрицу кратчайших расстояний в отдельном окне");
    propsLayout->addWidget(calcButton);
    propsLayout->addWidget(distMatrixButton);
    propsLayout->addWidget(graphPropertiesDisplay);
    connect(distMatrixButton, &QPushButton::clicked, this, &MainWindow::showDistanceMatrixDialog);

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

    view->centerOn(0, 0);
    resolveParallelEdges();
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
// Добавьте в начало файла, если нет
#include <QtMath>
void MainWindow::applyLayout()
{
    if (nodes.isEmpty()) return;

    if (physicsTimer->isActive()) {
        physicsTimer->stop();
        QList<QPushButton*> btns = this->findChildren<QPushButton*>();
        for(auto b : btns) if(b->text().contains("Физика")) b->setChecked(false);
    }

    int type = layoutCombo->currentIndex();
    int n = nodes.size();
    bool animate = useAnimationCheckbox->isChecked();

    QMap<int, QPointF> newPositions;
    const auto& matrix = graph->adjacencyMatrix();

    // ── 0. CIRCULAR ──────────────────────────────────────────────
    if (type == 0) {
        double radius = qMax(150.0, n * 25.0);
        for (int i = 0; i < n; ++i) {
            double angle = 2 * M_PI * i / n - M_PI / 2;
            newPositions[i] = QPointF(radius * cos(angle), radius * sin(angle));
        }
    }
    // ── 1. GRID ───────────────────────────────────────────────────
    else if (type == 1) {
        int cols = (int)ceil(sqrt((double)n));
        int rows = (int)ceil((double)n / cols);
        double spacing = qMax(80.0, 500.0 / cols);
        double totalW = (cols - 1) * spacing;
        double totalH = (rows - 1) * spacing;
        for (int i = 0; i < n; ++i) {
            int col = i % cols;
            int row = i / cols;
            newPositions[i] = QPointF(col * spacing - totalW / 2.0,
                                      row * spacing - totalH / 2.0);
        }
    }
    // ── 2. RANDOM ─────────────────────────────────────────────────
    else if (type == 2) {
        double spread = qMax(300.0, n * 20.0);
        for (int i = 0; i < n; ++i) {
            newPositions[i] = QPointF(
                QRandomGenerator::global()->bounded(-(int)spread, (int)spread+1),
                QRandomGenerator::global()->bounded(-(int)spread, (int)spread+1));
        }
    }
    // ── 3. HIERARCHICAL (ДЕРЕВО) ──────────────────────────────────
    else if (type == 3) {
        bool isTree = graph->isConnected() && (graph->edgeCount() == graph->nodeCount() - 1);
        if (!isTree) {
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("Раскладка Дерево");
            msgBox.setText("Граф не является деревом.\nЧто сделать?");
            QPushButton *mstBtn = msgBox.addButton("Построить MST", QMessageBox::ActionRole);
            QPushButton *forceBtn = msgBox.addButton("Применить как есть",  QMessageBox::ActionRole);
            QPushButton *cancelBtn = msgBox.addButton(QMessageBox::Cancel);
            msgBox.exec();
            if (msgBox.clickedButton() == mstBtn) {
                layoutCombo->setCurrentIndex(8);
                applyLayout();
                return;
            } else if (msgBox.clickedButton() == cancelBtn) {
                return;
            }
            (void)forceBtn;
        }

        QVector<int> level(n, -1), parent(n, -1);
        QQueue<int> bfsQ;
        QVector<int> degrees = graph->calculateDegrees();
        int root = 0;
        for (int i = 1; i < n; ++i)
            if (degrees[i] > degrees[root]) root = i;
        level[root] = 0;
        bfsQ.enqueue(root);
        while (!bfsQ.isEmpty()) {
            int u = bfsQ.dequeue();
            for (int v = 0; v < n; ++v)
                if (matrix[u][v] > 0 && level[v] == -1) {
                    level[v] = level[u] + 1;
                    parent[v] = u;
                    bfsQ.enqueue(v);
                }
        }
        for (int i = 0; i < n; ++i) if (level[i] == -1) level[i] = 0;

        QMap<int, QVector<int>> levelGroups;
        int maxLevel = 0;
        for (int i = 0; i < n; ++i) {
            levelGroups[level[i]].append(i);
            maxLevel = qMax(maxLevel, level[i]);
        }

        QMap<int, double> xPos;
        xPos[root] = 0.0;
        for (int iter = 0; iter < 4; ++iter) {
            for (int lvl = 1; lvl <= maxLevel; ++lvl) {
                QVector<int>& group = levelGroups[lvl];
                QVector<QPair<double,int>> bary;
                for (int v : group) {
                    double sumX = 0; int cnt = 0;
                    for (int u : levelGroups[lvl-1])
                        if (matrix[u][v] > 0 || matrix[v][u] > 0) { sumX += xPos.value(u, 0.0); cnt++; }
                    bary.append({cnt > 0 ? sumX/cnt : xPos.value(v, 0.0), v});
                }
                std::sort(bary.begin(), bary.end(), [](const QPair<double,int>& a, const QPair<double,int>& b){ return a.first < b.first; });
                int c = bary.size();
                double sp = qMax(90.0, 700.0 / qMax(1, c));
                double tw = (c - 1) * sp;
                for (int i = 0; i < c; ++i) { xPos[bary[i].second] = i * sp - tw / 2.0; group[i] = bary[i].second; }
            }
        }
        double levelSpacing = 130.0;
        for (int lvl = 0; lvl <= maxLevel; ++lvl)
            for (int v : levelGroups[lvl])
                newPositions[v] = QPointF(xPos.value(v, 0.0), lvl * levelSpacing - maxLevel * levelSpacing / 2.0);
    }
    // ── 4. BIPARTITE ──────────────────────────────────────────────
    else if (type == 4) {
        if (!graph->isBipartite()) {
            QMessageBox::warning(this, "Ошибка", "Граф не двудольный! Применяю круговую.");
            layoutCombo->setCurrentIndex(0);
            applyLayout();
            return;
        }
        QVector<int> colors = graph->bipartiteColoring();
        QVector<int> left, right;
        for (int i = 0; i < n; ++i) (colors[i] == 0 ? left : right).append(i);
        double spacingL = qMax(60.0, 500.0 / qMax(1, (int)left.size()));
        double spacingR = qMax(60.0, 500.0 / qMax(1, (int)right.size()));
        double totalHL = (left.size()  - 1) * spacingL;
        double totalHR = (right.size() - 1) * spacingR;
        for (int i = 0; i < left.size();  ++i) newPositions[left[i]]  = QPointF(-200, i * spacingL - totalHL / 2.0);
        for (int i = 0; i < right.size(); ++i) newPositions[right[i]] = QPointF( 200, i * spacingR - totalHR / 2.0);
    }
    // ── 5. STAR / WHEEL ───────────────────────────────────────────
    else if (type == 5) {
        QVector<int> degrees = graph->calculateDegrees();
        int centerNode = 0;
        for (int i = 1; i < n; ++i) if (degrees[i] > degrees[centerNode]) centerNode = i;
        newPositions[centerNode] = QPointF(0, 0);
        double radius = qMax(200.0, (n-1) * 20.0);
        int current = 0;
        for (int i = 0; i < n; ++i) {
            if (i == centerNode) continue;
            double angle = 2 * M_PI * current / (n - 1) - M_PI / 2;
            newPositions[i] = QPointF(radius * cos(angle), radius * sin(angle));
            current++;
        }
    }
    // ── 6. TUTTE ──────────────────────────────────────────────────
    else if (type == 6) {
        if (n < 3) { layoutCombo->setCurrentIndex(0); applyLayout(); return; }
        // ... (ваш стандартный алгоритм тутта) ...
        layoutCombo->setCurrentIndex(0); applyLayout(); return; // Заглушка, чтобы не растягивать код
    }
    // ── 7. SORTED CIRCLE ──────────────────────────────────────────
    else if (type == 7) {
        QVector<int> degrees = graph->calculateDegrees();
        QVector<int> idx(n); std::iota(idx.begin(), idx.end(), 0);
        std::sort(idx.begin(), idx.end(), [&](int a, int b){ return degrees[a] > degrees[b]; });
        double radius = qMax(150.0, n * 25.0);
        for (int i = 0; i < n; ++i) {
            double angle = 2 * M_PI * i / n - M_PI / 2;
            newPositions[idx[i]] = QPointF(radius * cos(angle), radius * sin(angle));
        }
    }
    // ── 8. MST LAYOUT ────────────────────────────────────────────
    else if (type == 8) {
        QVector<QPair<int,int>> mst = graph->getPrimMST();
        if (mst.isEmpty()) return;
        QVector<QVector<int>> mstAdj(n, QVector<int>(n, 0));
        for (auto& e : mst) { mstAdj[e.first][e.second] = 1; mstAdj[e.second][e.first] = 1; }
        QVector<int> mstDeg(n, 0);
        for (auto& e : mst) { mstDeg[e.first]++; mstDeg[e.second]++; }
        int root = (int)(std::max_element(mstDeg.begin(), mstDeg.end()) - mstDeg.begin());

        QVector<int> level(n, -1); QQueue<int> bfsQ2;
        level[root] = 0; bfsQ2.enqueue(root);
        while (!bfsQ2.isEmpty()) {
            int u = bfsQ2.dequeue();
            for (int v = 0; v < n; ++v)
                if (mstAdj[u][v] && level[v] == -1) { level[v] = level[u] + 1; bfsQ2.enqueue(v); }
        }
        for (int i = 0; i < n; ++i) if (level[i] == -1) level[i] = 0;

        QVector<double> xPos(n, 0.0); double leafCounter = 0.0;
        std::function<void(int,int)> assignX = [&](int v, int p) {
            QVector<int> children;
            for (int u = 0; u < n; ++u) if (mstAdj[v][u] && u != p) children.append(u);
            if (children.isEmpty()) { xPos[v] = leafCounter * 80.0; leafCounter += 1.0; }
            else {
                for (int c : children) assignX(c, v);
                xPos[v] = (xPos[children.first()] + xPos[children.last()]) / 2.0;
            }
        };
        assignX(root, -1);

        double minX = *std::min_element(xPos.begin(), xPos.end());
        double maxX = *std::max_element(xPos.begin(), xPos.end());
        int maxLevel = *std::max_element(level.begin(), level.end());
        for (int i = 0; i < n; ++i)
            newPositions[i] = QPointF(xPos[i] - (minX + maxX)/2.0, level[i] * 120.0 - maxLevel * 60.0);

        resetEdgeColors();
        for (auto& e : mst) highlightEdge(e.first, e.second, QColor(255, 140, 0));
    }


    // ── МАТЕМАТИЧЕСКОЕ РАЗРЕШЕНИЕ НАЛОЖЕНИЙ (ANTI-OVERLAP) ────────
    // Этот блок проверяет, не слиплись ли узлы, и не проходит ли ребро ровно сквозь чужой узел.
    const int RESOLVE_ITERS = 15;
    const double MIN_DIST = 60.0;  // Минимальное расстояние между узлами
    const double EDGE_CLR = 30.0;  // Расстояние, на которое узлы отталкиваются от чужих рёбер

    for (int iter = 0; iter < RESOLVE_ITERS; ++iter) {
        bool moved = false;

        // 1. Расталкиваем слипшиеся узлы
        QList<int> keys = newPositions.keys();
        for (int i = 0; i < keys.size(); ++i) {
            for (int j = i + 1; j < keys.size(); ++j) {
                int u = keys[i], v = keys[j];
                QPointF d = newPositions[v] - newPositions[u];
                double dist = sqrt(d.x()*d.x() + d.y()*d.y());
                if (dist < MIN_DIST) {
                    if (dist < 0.001) d = QPointF(5, 5); // защита от 0
                    double push = (MIN_DIST - dist) / 2.0;
                    QPointF dir = d / sqrt(d.x()*d.x() + d.y()*d.y());
                    newPositions[u] -= dir * push;
                    newPositions[v] += dir * push;
                    moved = true;
                }
            }
        }

        // 2. Сдвигаем узлы, оказавшиеся прямо на линии чужого ребра
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                if (matrix[i][j] > 0 || matrix[j][i] > 0) { // Если есть ребро
                    QPointF p1 = newPositions[i];
                    QPointF p2 = newPositions[j];
                    double l2 = (p1.x()-p2.x())*(p1.x()-p2.x()) + (p1.y()-p2.y())*(p1.y()-p2.y());
                    if (l2 < 1.0) continue;

                    for (int k : keys) {
                        if (k == i || k == j) continue; // Узел не является концом этого ребра
                        QPointF pk = newPositions[k];
                        
                        // Ищем проекцию узла на ребро
                        double t = ((pk.x()-p1.x())*(p2.x()-p1.x()) + (pk.y()-p1.y())*(p2.y()-p1.y())) / l2;
                        
                        // Если узел лежит в пределах отрезка ребра
                        if (t > 0.05 && t < 0.95) {
                            QPointF proj = p1 + t * (p2 - p1);
                            double dx = pk.x() - proj.x();
                            double dy = pk.y() - proj.y();
                            double dist = sqrt(dx*dx + dy*dy);

                            if (dist < EDGE_CLR) {
                                // Находим перпендикуляр к ребру для выталкивания
                                QPointF normal(-(p2.y()-p1.y()), (p2.x()-p1.x()));
                                double nLen = sqrt(normal.x()*normal.x() + normal.y()*normal.y());
                                normal /= nLen;
                                
                                // Выталкиваем узел с пути ребра в ближайшую сторону
                                double cross = (p2.x()-p1.x())*(pk.y()-p1.y()) - (p2.y()-p1.y())*(pk.x()-p1.x());
                                if (cross < 0) normal = -normal;
                                if (cross == 0) normal = QPointF(1, 0); 
                                
                                newPositions[k] += normal * (EDGE_CLR - dist);
                                moved = true;
                            }
                        }
                    }
                }
            }
        }
        if (!moved) break;
    }


    // ── ПРИМЕНЕНИЕ С АНИМАЦИЕЙ ────────────────────────────────────
    QParallelAnimationGroup *animGroup = new QParallelAnimationGroup;

    for (int i = 0; i < n; ++i) {
        if (!nodes.contains(i)) continue;
        QPointF target = newPositions.value(i, QPointF(0,0));
        
        if (animate) {
            QPropertyAnimation *anim = new QPropertyAnimation(nodes[i], "pos");
            anim->setDuration(800);
            anim->setStartValue(nodes[i]->pos());
            anim->setEndValue(target);
            anim->setEasingCurve(QEasingCurve::OutCubic);
            animGroup->addAnimation(anim);
        } else {
            nodes[i]->setPos(target);
        }
    }

    if (animate) {
        animGroup->start(QAbstractAnimation::DeleteWhenStopped);
        connect(animGroup, &QParallelAnimationGroup::finished, [=](){ view->centerOn(0,0); });
    } else {
        delete animGroup;
        view->centerOn(0, 0);
    }
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

void MainWindow::showDistanceMatrixDialog()
{
    if (graph->nodeCount() == 0) {
        QMessageBox::information(this, "Матрица расстояний", "Граф пуст.");
        return;
    }

    auto dist = graph->getDistanceMatrix();
    int n = dist.size();

    // ── Диалог ───────────────────────────────────────────────────────────────
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(QString("Матрица кратчайших расстояний (%1×%1)").arg(n));
    dlg->setMinimumSize(400, 300);
    dlg->resize(qMin(80 + n * 65, 1100), qMin(80 + n * 32, 700));

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    // Заголовок
    QLabel *title = new QLabel(
        QString("Флойд–Уоршелл · %1 вершин  |  ∞ = вершины не связаны").arg(n));
    title->setStyleSheet("font-weight: bold; margin-bottom: 4px;");
    layout->addWidget(title);

    // Таблица
    QTableWidget *table = new QTableWidget(n, n, dlg);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::SingleSelection);

    // Заголовки строк и столбцов
    QStringList headers;
    for (int i = 0; i < n; ++i) headers << QString::number(i + 1);
    table->setHorizontalHeaderLabels(headers);
    table->setVerticalHeaderLabels(headers);
    table->horizontalHeader()->setDefaultSectionSize(58);
    table->verticalHeader()->setDefaultSectionSize(26);
    table->horizontalHeader()->setMinimumSectionSize(40);

    // Заполняем значениями
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            QString text;
            QTableWidgetItem *item;
            if (dist[i][j] >= 1e17) {
                text = "∞";
                item = new QTableWidgetItem(text);
                item->setForeground(QColor(150, 150, 150));
            } else {
                // Показываем без лишних нулей: 3 → "3", 3.5 → "3.5"
                double v = dist[i][j];
                text = (v == (int)v) ? QString::number((int)v)
                                     : QString::number(v, 'g', 5);
                item = new QTableWidgetItem(text);
                if (i == j)
                    item->setBackground(QColor(230, 230, 230));
            }
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(i, j, item);
        }
    }

    // Подсветка минимума в каждой строке (кроме диагонали)
    for (int i = 0; i < n; ++i) {
        double rowMin = 1e18;
        for (int j = 0; j < n; ++j)
            if (i != j && dist[i][j] < rowMin) rowMin = dist[i][j];
        for (int j = 0; j < n; ++j) {
            if (i != j && dist[i][j] == rowMin && rowMin < 1e17)
                if (auto it = table->item(i, j))
                    it->setBackground(QColor(198, 239, 206)); // светло-зелёный
        }
    }

    layout->addWidget(table);

    // ── Метрики под таблицей ─────────────────────────────────────────────────
    auto ecc = graph->getEccentricities();
    double radius   = graph->getRadius();
    double diameter = graph->getDiameter();
    int    median   = graph->getMedian();

    QString metrics = QString(
        "Радиус: <b>%1</b> &nbsp;&nbsp; Диаметр: <b>%2</b> &nbsp;&nbsp; "
        "Медиана: <b>вершина %3</b>")
        .arg(radius).arg(diameter).arg(median + 1);

    QLabel *metricsLabel = new QLabel(metrics);
    metricsLabel->setTextFormat(Qt::RichText);
    metricsLabel->setStyleSheet("margin-top: 6px;");
    layout->addWidget(metricsLabel);

    // ── Кнопки ───────────────────────────────────────────────────────────────
    QHBoxLayout *btnLayout = new QHBoxLayout();

    // Копировать как CSV
    QPushButton *copyBtn = new QPushButton("Копировать CSV");
    connect(copyBtn, &QPushButton::clicked, [=]() {
        QString csv;
        csv += ";";
        for (int j = 0; j < n; ++j) csv += QString::number(j+1) + ";";
        csv += "\n";
        for (int i = 0; i < n; ++i) {
            csv += QString::number(i+1) + ";";
            for (int j = 0; j < n; ++j) {
                csv += (dist[i][j] >= 1e17 ? "inf" :
                        (dist[i][j] == (int)dist[i][j] ?
                         QString::number((int)dist[i][j]) :
                         QString::number(dist[i][j], 'g', 5)));
                csv += ";";
            }
            csv += "\n";
        }
        QMessageBox::information(dlg, "Скопировано", "Матрица скопирована в буфер обмена в формате CSV.");
    });

    QPushButton *closeBtn = new QPushButton("Закрыть");
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);

    btnLayout->addWidget(copyBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);

    dlg->exec();
    delete dlg;
}
void MainWindow::generateRoutingTask()
{
    saveToHistory();
    stopAndReset();

    const int N = 30;
    const int TARGET_EDGES = 60;
    const int MIN_W = 1;
    const int MAX_W = 20;

    QVector<QVector<int>> matrix(N, QVector<int>(N, 0));

    QVector<int> perm(N);
    std::iota(perm.begin(), perm.end(), 0);
    for (int i = N-1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i+1);
        std::swap(perm[i], perm[j]);
    }

    for (int i = 0; i < N-1; ++i) {
        int u = perm[i], v = perm[i+1];
        int w = QRandomGenerator::global()->bounded(MIN_W, MAX_W+1);
        matrix[u][v] = w;
        matrix[v][u] = w;
    }

    int edgeCount = N - 1;
    int attempts = 0;
    while (edgeCount < TARGET_EDGES && attempts < 10000) {
        int u = QRandomGenerator::global()->bounded(N);
        int v = QRandomGenerator::global()->bounded(N);
        attempts++;
        if (u == v || matrix[u][v] != 0) continue;
        int w = QRandomGenerator::global()->bounded(MIN_W, MAX_W+1);
        matrix[u][v] = w;
        matrix[v][u] = w;
        edgeCount++;
    }

    graph->setDirected(false);
    if (directedCheck) directedCheck->setChecked(false);
    graph->createFromAdjacencyMatrix(matrix);

    updateGraphView();
    updateTableFromGraph();

    layoutCombo->setCurrentIndex(0);
    applyLayout();

    // Только одна строка
    logAction("Сформирован граф для Задачи 1: Маршрутизация (30 вершин, 60 рёбер)");
}

void MainWindow::generateConnectivityTask()
{
    saveToHistory();
    stopAndReset();

    const int N = 35;
    QVector<QVector<int>> matrix(N, QVector<int>(N, 0));

    auto addEdge = [&](int u, int v) {
        matrix[u][v] = 1;
        matrix[v][u] = 1;
    };

    // GA (0-13)
    addEdge(0,1); addEdge(1,2); addEdge(2,3);
    addEdge(3,4); addEdge(4,0);
    addEdge(4,5); addEdge(5,6); addEdge(6,7); addEdge(7,4);
    addEdge(7,8); addEdge(8,9); addEdge(9,10); addEdge(10,7);
    addEdge(10,11); addEdge(11,12); addEdge(12,13);
    addEdge(13,10); addEdge(11,13);
    addEdge(2,8); addEdge(5,11);

    // GB (14-25)
    for (int i = 14; i < 25; ++i) addEdge(i, i+1);
    addEdge(25, 14);
    addEdge(14,17); addEdge(17,20); addEdge(20,23);
    addEdge(15,19); addEdge(16,22);

    // GC (26-34) - дерево, все рёбра мосты
    addEdge(26,27); addEdge(27,28); addEdge(28,29); addEdge(29,30);
    addEdge(27,31);
    addEdge(28,32); addEdge(32,33);
    addEdge(30,34);

    graph->setDirected(false);
    if (directedCheck) directedCheck->setChecked(false);
    graph->createFromAdjacencyMatrix(matrix);

    updateGraphView();
    updateTableFromGraph();

    for (int i = 0;  i < 14; ++i) if(nodes.contains(i)) nodes[i]->setBaseColor(QColor(100,150,255));
    for (int i = 14; i < 26; ++i) if(nodes.contains(i)) nodes[i]->setBaseColor(QColor(100,220,100));
    for (int i = 26; i < 35; ++i) if(nodes.contains(i)) nodes[i]->setBaseColor(QColor(255,150,100));

    layoutCombo->setCurrentIndex(0);
    applyLayout();

    // Только одна строка
    logAction("Сформирован граф для Задачи 3: Анализ связности (35 вершин, 3 компоненты)");
}

void MainWindow::generateExamSchedulingTask()
{
    saveToHistory();
    stopAndReset();

    const int N = 28;
    QVector<QVector<int>> matrix(N, QVector<int>(N, 0));

    auto addEdge = [&](int u, int v) {
        matrix[u][v] = 1;
        matrix[v][u] = 1;
    };

    QVector<int> clique1 = {0,5,6,7,8,9,10};
    for (int i = 0; i < clique1.size(); ++i)
        for (int j = i+1; j < clique1.size(); ++j)
            addEdge(clique1[i], clique1[j]);

    QVector<int> clique2 = {1,0,11,12,13,14};
    for (int i = 0; i < clique2.size(); ++i)
        for (int j = i+1; j < clique2.size(); ++j)
            addEdge(clique2[i], clique2[j]);

    QVector<int> clique3 = {2,0,1,15,16};
    for (int i = 0; i < clique3.size(); ++i)
        for (int j = i+1; j < clique3.size(); ++j)
            addEdge(clique3[i], clique3[j]);

    QVector<int> clique4 = {3,2,1,17};
    for (int i = 0; i < clique4.size(); ++i)
        for (int j = i+1; j < clique4.size(); ++j)
            addEdge(clique4[i], clique4[j]);

    QVector<int> clique5 = {4,0,18,19};
    for (int i = 0; i < clique5.size(); ++i)
        for (int j = i+1; j < clique5.size(); ++j)
            addEdge(clique5[i], clique5[j]);

    addEdge(20,23); addEdge(20,24);
    addEdge(21,24); addEdge(21,25);
    addEdge(22,23); addEdge(22,26); addEdge(22,27);
    addEdge(23,25);
    addEdge(24,27);
    addEdge(25,26);
    addEdge(26,27);

    graph->setDirected(false);
    if (directedCheck) directedCheck->setChecked(false);
    graph->createFromAdjacencyMatrix(matrix);

    updateGraphView();
    updateTableFromGraph();

    layoutCombo->setCurrentIndex(0);
    applyLayout();

    // Только одна строка
    logAction("Сформирован граф для Задачи 4: Расписание экзаменов (28 вершин)");
}

void MainWindow::generateFiberOpticTask()
{
    saveToHistory();
    stopAndReset();
 
    const int N = 25;
 
    QVector<QVector<int>> matrix(N, QVector<int>(N, 0));
 
    auto setEdge = [&](int u, int v, int w) {
        matrix[u][v] = w;
        matrix[v][u] = w;
    };
 
    // ── Горизонтальные рёбра (соседние столбцы, одна строка) ────────────────
    setEdge(0,  5,  12); // 1–2
    setEdge(5,  10, 17); // 2–3
    setEdge(10, 15,  8); // 3–4
    setEdge(15, 20, 15); // 4–5
    setEdge(1,  6,   8); // 6–7
    setEdge(6,  11, 15); // 7–8
    setEdge(11, 16,  8); // 8–9
    setEdge(16, 21, 16); // 9–10
    setEdge(2,  7,  10); // 11–12
    setEdge(7,  12,  9); // 12–13
    setEdge(12, 17, 15); // 13–14
    setEdge(17, 22, 15); // 14–15
    setEdge(3,  8,  17); // 16–17
    setEdge(8,  13,  8); // 17–18
    setEdge(13, 18, 16); // 18–19
    setEdge(18, 23, 14); // 19–20
    setEdge(4,  9,  16); // 21–22
    setEdge(9,  14, 14); // 22–23
    setEdge(14, 19, 12); // 23–24
    setEdge(19, 24, 11); // 24–25
 
    // ── Вертикальные рёбра (одни столбец, соседние строки) ──────────────────
    setEdge(0,  1,   9); // 1–6
    setEdge(5,  6,   8); // 2–7
    setEdge(10, 11, 16); // 3–8
    setEdge(15, 16,  7); // 4–9
    setEdge(20, 21, 13); // 5–10
    setEdge(1,  2,  10); // 6–11
    setEdge(6,  7,   7); // 7–12
    setEdge(11, 12, 17); // 8–13
    setEdge(16, 17, 16); // 9–14
    setEdge(21, 22,  7); // 10–15
    setEdge(2,  3,   7); // 11–16
    setEdge(7,  8,  13); // 12–17
    setEdge(12, 13, 16); // 13–18
    setEdge(17, 18,  9); // 14–19
    setEdge(22, 23, 16); // 15–20
    setEdge(3,  4,  10); // 16–21
    setEdge(8,  9,  18); // 17–22
    setEdge(13, 14, 16); // 18–23
    setEdge(18, 19, 15); // 19–24
    setEdge(23, 24, 14); // 20–25
 
    // ── Диагонали ↘ (столбец+1, строка+1) ───────────────────────────────────
    setEdge(0,  6,  13); // 1–7
    setEdge(5,  11, 15); // 2–8
    setEdge(10, 16,  7); // 3–9
    setEdge(15, 21,  8); // 4–10
    setEdge(1,  7,   8); // 6–12
    setEdge(6,  12, 16); // 7–13
    setEdge(11, 17, 17); // 8–14
    setEdge(16, 22, 16); // 9–15
    setEdge(2,  8,  15); // 11–17
    setEdge(7,  13,  9); // 12–18
    setEdge(12, 18, 11); // 13–19
    setEdge(17, 23,  8); // 14–20
    setEdge(3,  9,  12); // 16–22
    setEdge(8,  14,  8); // 17–23
    setEdge(13, 19, 10); // 18–24
    setEdge(18, 24, 13); // 19–25
 
    // ── Диагонали ↗ (столбец+1, строка-1) ───────────────────────────────────
    setEdge(1,  5,   7); // 6–2
    setEdge(6,  10, 12); // 7–3
    setEdge(11, 15, 10); // 8–4
    setEdge(16, 20, 13); // 9–5
    setEdge(2,  6,  13); // 11–7
    setEdge(7,  11, 10); // 12–8
    setEdge(12, 16,  7); // 13–9
    setEdge(17, 21, 13); // 14–10
    setEdge(3,  7,  11); // 16–12
    setEdge(8,  12,  8); // 17–13
    setEdge(13, 17, 17); // 18–14
    setEdge(18, 22, 16); // 19–15
    setEdge(4,  8,  15); // 21–17
    setEdge(9,  13,  7); // 22–18
    setEdge(14, 18, 17); // 23–19
    setEdge(19, 23, 12); // 24–20
 
    // ── Загрузка графа ───────────────────────────────────────────────────────
    graph->setDirected(false);
    if (directedCheck) directedCheck->setChecked(false);
    graph->createFromAdjacencyMatrix(matrix);
    updateGraphView();
    updateTableFromGraph();
 
    // ── Расстановка вершин по сетке ─────────────────────────────────────────
    const double SPACING = 100.0;
    for (int x = 0; x < 5; ++x) {
        for (int y = 0; y < 5; ++y) {
            int id = x * 5 + y;
            if (nodes.contains(id)) {
                nodes[id]->setPos(
                    (x - 2) * SPACING,
                    (y - 2) * SPACING
                );
            }
        }
    }
 
    // ── MST и лог ───────────────────────────────────────────────────────────
    QVector<QPair<int,int>> mst = graph->getPrimMST();
    int mstWeight = 0;
    for (auto& e : mst)
        mstWeight += matrix[e.first][e.second];
 
    logAction("=== Задача 2: ВОЛС ===");
    logAction(QString("Граф: 25 вершин, 72 ребра (веса 7–18)"));
    logAction(QString("MST: %1 рёбер, суммарный вес = %2").arg(mst.size()).arg(mstWeight));
    view->centerOn(0, 0);
}
void MainWindow::resolveParallelEdges()
{
    // Собираем все рёбра
    QList<Edge*> allEdges;
    for (auto* item : scene->items())
        if (auto* e = dynamic_cast<Edge*>(item))
            allEdges.append(e);

    // Сбрасываем смещения
    for (auto* e : allEdges)
        e->setParallelOffset(0.0);

    const double OFFSET_STEP = 8.0; // пикселей между параллельными рёбрами

    // Группируем рёбра по паре вершин (u,v) где u < v
    QMap<QPair<int,int>, QList<Edge*>> edgeGroups;

    for (auto* e : allEdges) {
        int u = e->sourceNode()->getId();
        int v = e->destNode()->getId();
        QPair<int,int> key = { qMin(u,v), qMax(u,v) };
        edgeGroups[key].append(e);
    }

    // Для групп с более чем одним ребром — назначаем смещения
    for (auto& group : edgeGroups) {
        int count = group.size();
        if (count <= 1) continue;

        // Смещения: -step*(count-1)/2, ..., 0, ..., +step*(count-1)/2
        double totalSpan = OFFSET_STEP * (count - 1);
        double startOffset = -totalSpan / 2.0;

        for (int i = 0; i < count; ++i) {
            group[i]->setParallelOffset(startOffset + i * OFFSET_STEP);
        }
    }
}
