#include "mainwindow.h"
#include "graph.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QGraphicsEllipseItem>
#include <QGraphicsLineItem>
#include <QGraphicsSimpleTextItem>
#include <QHeaderView>
#include <QLineF>
#include <QScrollBar>
#include <QInputDialog>
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <QEvent>
#include <QInputDialog>
#include <QStack>
#include <QQueue>
#include <QSet>
#include <QFileDialog>
#include <QToolBar>
#include <QAction>
#include <QTimer>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), graph(new Graph()), updatingMatrix(false)
{
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
    traversalButton->setFixedHeight(30);
    startVertexCombo = new QComboBox();

    setupUI();
    setupMatrixConnections();

    view->viewport()->installEventFilter(this);
    view->setRenderHint(QPainter::Antialiasing);
    view->setRenderHint(QPainter::SmoothPixmapTransform);
    view->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

    setWindowTitle("Редактор графов");
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

    // Левая панель - ввод матрицы
    QGroupBox *inputGroup = new QGroupBox("Ввод матрицы");
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);

    representationCombo->addItem("Матрица смежности");
    representationCombo->addItem("Матрица инцидентности");
    representationCombo->setCurrentIndex(0);

    // Настройка выбора размера
    for (int i = 2; i <= 10; ++i) {
        sizeCombo->addItem(QString("%1x%1").arg(i), i);
    }
    sizeCombo->setCurrentIndex(1); // По умолчанию 3x3

    // Настройка таблицы
    matrixTable->setEditTriggers(QAbstractItemView::AllEditTriggers);
    matrixTable->setSelectionMode(QAbstractItemView::SingleSelection);
    matrixTable->setStyleSheet(
        "QTableWidget {"
        "   font: 12px;"
        "   background-color: transparent;" // Прозрачный фон
        "}"
        "QTableWidget::item {"
        "   border: 1px solid #dcdcdc;" // Границы ячеек
        "}"
    );
    matrixTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    matrixTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    matrixTable->setFixedHeight(300);
    matrixTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    matrixTable->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    matrixTable->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);

    // Настройка кнопок
    generateButton->setFixedHeight(30);
    calcButton->setFixedHeight(30);
    resizeButton->setFixedHeight(30);

    // Подсказка по формату матрицы
    hintLabel->setTextFormat(Qt::RichText);
    hintLabel->setText("Для матрицы инцидентности:<br>"
                      "- Каждый столбец соответствует ребру<br>"
                      "- Должно быть ровно две 1 в столбце<br>"
                      "- Остальные элементы 0");
    hintLabel->setWordWrap(true);
    hintLabel->setVisible(false);

    // Организация layout
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
    inputLayout->addWidget(statusLabel);


    // Центральная панель - визуализация
    QGroupBox *graphGroup = new QGroupBox("Визуализация графа");
    QVBoxLayout *graphLayout = new QVBoxLayout(graphGroup);
    view->setScene(scene);
    graphLayout->addWidget(view);

    // Правая панель - свойства
    QGroupBox *propsGroup = new QGroupBox("Свойства графа");
    QVBoxLayout *propsLayout = new QVBoxLayout(propsGroup);
    graphPropertiesDisplay->setReadOnly(true);
    graphPropertiesDisplay->setStyleSheet("font-family: monospace;");
    propsLayout->addWidget(graphPropertiesDisplay);

    mainLayout->addWidget(inputGroup, 1);
    mainLayout->addWidget(graphGroup, 2);
    mainLayout->addWidget(propsGroup, 1);
    setCentralWidget(centralWidget);

    // Инициализация таблицы
    onRepresentationChanged(0);

    cycleButton = new QPushButton("Поиск циклов");
    cycleButton->setFixedHeight(30);
    inputLayout->addWidget(cycleButton);
    colorButton = new QPushButton("Раскрасить граф");
    colorButton->setFixedHeight(30);
    inputLayout->addWidget(colorButton);
    bipartiteButton = new QPushButton("Проверить двудольность");
    bipartiteButton->setFixedHeight(30);
    inputLayout->addWidget(bipartiteButton);

    // Создаем тулбар для кнопок экспорта
    exportToolBar = new QToolBar("Экспорт", this);
    exportToolBar->setIconSize(QSize(24, 24)); // Размер иконок
    exportToolBar->setToolButtonStyle(Qt::ToolButtonIconOnly); // Только иконки

    // Создаем действия с иконками
    saveImageAction = new QAction(QIcon(":/icons/camera.png"), "Сохранить как изображение", this);
    saveDotAction = new QAction(QIcon(":/icons/dot.png"), "Сохранить как DOT", this);

    QPushButton *criticalButton = new QPushButton("Показать критические элементы");
    criticalButton->setFixedHeight(30);
    inputLayout->addWidget(criticalButton);
    connect(criticalButton, &QPushButton::clicked,
            this, &MainWindow::highlightBridgesAndArticulations);

    // Добавляем действия в тулбар
    exportToolBar->addAction(saveImageAction);
    exportToolBar->addAction(saveDotAction);
    saveImageAction->setIcon(QIcon::fromTheme("camera-photo"));
    saveDotAction->setIcon(QIcon::fromTheme("text-x-generic"));

    inputLayout->addWidget(new QLabel("Начальная вершина:"));
    inputLayout->addWidget(startVertexCombo);
    inputLayout->addWidget(traversalButton);

    // Размещаем тулбар в нужном месте (например, справа от основного содержимого)
    addToolBar(Qt::TopToolBarArea, exportToolBar);

    // Подключаем сигналы
    connect(saveImageAction, &QAction::triggered, this, &MainWindow::saveGraphToImage);
    connect(saveDotAction, &QAction::triggered, this, &MainWindow::saveGraphToDotFile);
    connect(traversalButton, &QPushButton::clicked, this, &MainWindow::visualizeTraversal);
}

void MainWindow::resetEdgeColors() {
    // Сброс цветов рёбер
    for (auto item : scene->items()) {
        if (auto edge = dynamic_cast<QGraphicsLineItem*>(item)) {
            edge->setPen(QPen(Qt::black, 2));
        }
    }

    // Сброс цветов вершин
    for (auto node : nodes) {
        node->setBrush(QColor(70, 130, 180)); // Исходный синий цвет
    }
}

void MainWindow::highlightTraversal(const QVector<int>& traversal, const QColor& color) {
    if (traversal.isEmpty()) return;

    // Сброс предыдущего выделения
    resetEdgeColors();
    for (auto node : nodes) {
        node->setBrush(QColor(70, 130, 180)); // Исходный цвет
    }

    // Подсветка вершин в порядке обхода с задержкой
    QTimer *timer = new QTimer(this);
    int index = 0;
    connect(timer, &QTimer::timeout, [=]() mutable {
        if (index < traversal.size()) {
            int vertex = traversal[index];
            if (nodes.contains(vertex)) {
                nodes[vertex]->setBrush(color);

                // Подсветка ребра от предыдущей вершины
                if (index > 0) {
                    int prevVertex = traversal[index-1];
                    highlightEdge(prevVertex, vertex, Qt::yellow);
                }
            }
            index++;
        } else {
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start(500); // Интервал 500 мс
}
QString MainWindow::traversalToString(const QVector<int>& traversal) {
    QStringList vertices;
    for (int v : traversal) {
        vertices << QString::number(v + 1);
    }
    return vertices.join(" → ");
}

void MainWindow::highlightEdge(int u, int v, const QColor& color) {
    for (auto item : scene->items()) {
        if (auto edge = dynamic_cast<QGraphicsLineItem*>(item)) {
            QLineF line = edge->line();
            QPointF uPos = nodes[u]->pos();
            QPointF vPos = nodes[v]->pos();

            if ((line.p1() == uPos && line.p2() == vPos) ||
                (line.p2() == uPos && line.p1() == vPos)) {
                edge->setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap));
            }
        }
    }
}

void MainWindow::highlightPath(const QVector<int>& path, const QColor& color) {
    if (path.isEmpty()) return;

    // Сброс предыдущего выделения
    resetEdgeColors();
    for (auto node : nodes) {
        node->setBrush(QColor(70, 130, 180)); // Исходный цвет
    }

    // Подсветка вершин пути
    for (int vertex : path) {
        if (nodes.contains(vertex)) {
            nodes[vertex]->setBrush(color);
        }
    }

    // Подсветка рёбер пути
    for (int i = 0; i < path.size() - 1; ++i) {
        highlightEdge(path[i], path[i+1], Qt::red);
    }
}

void MainWindow::visualizeTraversal() {
    if (!graph || graph->nodeCount() == 0) return;

    // Обновляем список вершин
    startVertexCombo->clear();
    for (int i = 0; i < graph->nodeCount(); ++i) {
        startVertexCombo->addItem(QString::number(i+1), i);
    }

    int start = startVertexCombo->currentData().toInt();
    if (start < 0 || start >= graph->nodeCount()) return;

    // Выбор типа обхода
    QMessageBox msgBox;
    msgBox.setText("Выберите тип обхода:");
    QPushButton *bfsButton = msgBox.addButton("BFS", QMessageBox::ActionRole);
    QPushButton *dfsButton = msgBox.addButton("DFS", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel);

    msgBox.exec();

    QVector<int> traversal;
    if (msgBox.clickedButton() == bfsButton) {
        traversal = graph->bfs(start);
        graphPropertiesDisplay->append("\n=== BFS обход ===\n" + traversalToString(traversal));
    } else if (msgBox.clickedButton() == dfsButton) {
        traversal = graph->dfs(start);
        graphPropertiesDisplay->append("\n=== DFS обход ===\n" + traversalToString(traversal));
    } else {
        return;
    }

    highlightTraversal(traversal, Qt::green);
}

void MainWindow::saveGraphToImage() {
    if (!graph || nodes.empty()) return;

    QString fileName = QFileDialog::getSaveFileName(this,
        "Сохранить граф как изображение",
        "",
        "PNG (*.png);;JPEG (*.jpg *.jpeg)");

    if (fileName.isEmpty()) return;

    // Создаем изображение сцены
    QRectF sceneRect = scene->itemsBoundingRect();
    QImage image(sceneRect.size().toSize(), QImage::Format_ARGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    scene->render(&painter);
    painter.end();

    if (!image.save(fileName)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить изображение");
    }
}

void MainWindow::exportToDotFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось создать файл");
        return;
    }

    QTextStream out(&file);
    out << "graph G {\n";
    out << "    node [shape=circle, style=filled, fillcolor=lightblue];\n";

    // Добавляем вершины
    for (int i = 0; i < graph->nodeCount(); ++i) {
        out << QString("    %1 [label=\"%2\"];\n").arg(i+1).arg(i+1);
    }

    // Добавляем ребра
    const auto &adjMatrix = graph->adjacencyMatrix();
    for (int i = 0; i < adjMatrix.size(); ++i) {
        for (int j = i; j < adjMatrix[i].size(); ++j) {
            if (adjMatrix[i][j] > 0) {
                out << QString("    %1 -- %2").arg(i+1).arg(j+1);
                if (adjMatrix[i][j] != 1) {
                    out << QString(" [label=\"%1\"]").arg(adjMatrix[i][j]);
                }
                out << ";\n";
            }
        }
    }

    out << "}\n";
    file.close();
}

void MainWindow::saveGraphToDotFile() {
    if (!graph || graph->nodeCount() == 0) return;

    QString fileName = QFileDialog::getSaveFileName(this,
        "Сохранить граф в DOT формате",
        "",
        "DOT Files (*.dot)");

    if (!fileName.isEmpty()) {
        exportToDotFile(fileName);
    }
}

void MainWindow::setupMatrixConnections()
{
    connect(cycleButton, &QPushButton::clicked, this, &MainWindow::checkCycles);
    connect(bipartiteButton, &QPushButton::clicked, this, &MainWindow::checkBipartite);
    connect(colorButton, &QPushButton::clicked, this, &MainWindow::colorGraph);
    connect(representationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onRepresentationChanged);
    connect(matrixTable, &QTableWidget::cellChanged, this, &MainWindow::onMatrixCellChanged);
    connect(generateButton, &QPushButton::clicked, this, &MainWindow::onGenerateClicked);
    connect(calcButton, &QPushButton::clicked, this, &MainWindow::calculateGraphProperties);
    connect(resizeButton, &QPushButton::clicked, this, &MainWindow::resizeMatrix);
}

void MainWindow::checkCycles() {
    if (!graph || graph->nodeCount() == 0) return;

    // Сброс предыдущего выделения
    resetEdgeColors();

    QVector<QVector<int>> allCycles = graph->findAllCycles();

    if (!allCycles.empty()) {
        QString cyclesInfo = "\n=== Найденные циклы ===\n";
        int cycleNum = 1;

        QVector<QColor> cycleColors = {Qt::red, Qt::blue, Qt::green, Qt::magenta};

        foreach (const QVector<int>& cycle, allCycles) {
            // Пропускаем некорректные циклы
            if (cycle.size() < 3 || cycle.first() != cycle.last()) continue;

            QStringList vertices;
            for (int i = 0; i < cycle.size()-1; ++i) {
                vertices << QString::number(cycle[i] + 1);
            }
            vertices << QString::number(cycle.first() + 1); // Замыкаем цикл

            cyclesInfo += QString("Цикл %1: %2\n").arg(cycleNum).arg(vertices.join(" → "));

            // Подсветка ребер
            QColor color = cycleColors[(cycleNum-1) % cycleColors.size()];
            for (int i = 0; i < cycle.size()-1; ++i) {
                highlightEdge(cycle[i], cycle[i+1], color);
            }

            cycleNum++;
        }

        graphPropertiesDisplay->append(cyclesInfo);
        statusLabel->setText(QString("Найдено циклов: %1").arg(cycleNum-1));
    } else {
        graphPropertiesDisplay->append("\nГраф ациклический (не содержит циклов)");
        statusLabel->setText("Граф не содержит циклов");
    }
}

void MainWindow::colorGraph() {
    if (!graph || graph->nodeCount() == 0) return;

    QVector<int> colors = graph->greedyColoring();
    QVector<QColor> colorPalette = {
        QColor(255, 0, 0),    // Красный
        QColor(0, 0, 255),    // Синий
        QColor(0, 255, 0),    // Зеленый
        QColor(255, 255, 0),  // Желтый
        QColor(255, 0, 255),  // Пурпурный
        QColor(0, 255, 255),  // Голубой
        QColor(255, 128, 0),  // Оранжевый
        QColor(128, 0, 255)   // Фиолетовый
    };

    // Обновляем цвета узлов на сцене
    for (auto it = nodes.begin(); it != nodes.end(); ++it) {
        int nodeIndex = it.key();
        QGraphicsEllipseItem* node = it.value();

        int colorIndex = colors[nodeIndex] % colorPalette.size();
        node->setBrush(colorPalette[colorIndex]);
    }

    // Добавим информацию о раскраске в свойства
    QString coloringInfo = "\n=== Раскраска графа ===\n";
    coloringInfo += "Использовано цветов: " + QString::number(*std::max_element(colors.begin(), colors.end()) + 1) + "\n";

    for (int i = 0; i < colors.size(); ++i) {
        coloringInfo += QString("Вершина %1: цвет %2\n").arg(i+1).arg(colors[i]+1);
    }

    graphPropertiesDisplay->append(coloringInfo);
}

void MainWindow::resizeMatrixTable(int rows, int cols)
{
    updatingMatrix = true;

    matrixTable->setRowCount(rows);
    matrixTable->setColumnCount(cols);

    // Сбросим все блокировки и стили
    for (int i = 0; i < rows; ++i) {
        matrixTable->setRowHeight(i, 30);
        for (int j = 0; j < cols; ++j) {
            QTableWidgetItem* item = matrixTable->item(i, j);
            if (!item) {
                item = new QTableWidgetItem("0");
                item->setTextAlignment(Qt::AlignCenter);
                matrixTable->setItem(i, j, item);
            }
            // Разблокируем все ячейки и сбросим фон
            item->setFlags(item->flags() | Qt::ItemIsEditable);
            item->setBackground(QBrush()); // Сбрасываем фон
            item->setText("0");
        }
    }

    // Блокируем диагональ только для матрицы смежности
    if (representationCombo->currentIndex() == 0) {
        enforceDiagonalZeros();
        hintLabel->hide();
    } else {
        hintLabel->show();
    }

    updatingMatrix = false;
}

void MainWindow::enforceDiagonalZeros()
{
    int size = qMin(matrixTable->rowCount(), matrixTable->columnCount());
    for (int i = 0; i < size; ++i) {
        QTableWidgetItem* item = matrixTable->item(i, i);
        if (item) {
            item->setText("0");
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setBackground(QColor(240, 240, 240)); // Серый фон только для заблокированных
        }
    }
}

void MainWindow::updateSymmetricCell(int row, int col)
{
    if (updatingMatrix || representationCombo->currentIndex() != 0 || row == col) return;

    updatingMatrix = true;

    QTableWidgetItem* changedItem = matrixTable->item(row, col);
    if (changedItem) {
        QTableWidgetItem* symmetricItem = matrixTable->item(col, row);
        if (!symmetricItem) {
            symmetricItem = new QTableWidgetItem();
            symmetricItem->setTextAlignment(Qt::AlignCenter);
            matrixTable->setItem(col, row, symmetricItem);
        }
        symmetricItem->setText(changedItem->text());
    }

    updatingMatrix = false;
}

void MainWindow::checkBipartite() {
    if (!graph || graph->nodeCount() == 0) return;

    if (graph->isBipartite()) {
        QVector<int> colors = graph->bipartiteColoring();

        // Визуализация
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            int nodeIndex = it.key();
            QGraphicsEllipseItem* node = it.value();

            if (colors[nodeIndex] == 0) {
                node->setBrush(QColor(255, 0, 0)); // Красный
            } else {
                node->setBrush(QColor(0, 0, 255)); // Синий
            }
        }

        graphPropertiesDisplay->append("\nГраф двудольный\nРаскраска:");
        for (int i = 0; i < colors.size(); ++i) {
            graphPropertiesDisplay->append(QString("Вершина %1: %2")
                                         .arg(i+1)
                                         .arg(colors[i] == 0 ? "Красный" : "Синий"));
        }
    } else {
        graphPropertiesDisplay->append("\nГраф не является двудольным");
        QMessageBox::information(this, "Результат", "Граф не двудольный");
    }
}

void MainWindow::onMatrixCellChanged(int row, int col)
{
    if (updatingMatrix) return;

    QTableWidgetItem* item = matrixTable->item(row, col);
    if (!item) return;

    bool ok;
    int value = item->text().toInt(&ok);
    if (!ok || value < 0) {
        QMessageBox::warning(this, "Ошибка ввода", "Пожалуйста, введите целое неотрицательное число");
        item->setText("0");
        return;
    }

    if (representationCombo->currentIndex() == 0) {
        updateSymmetricCell(row, col);

        if (row == col && value != 0) {
            item->setText("0");
            QMessageBox::information(this, "Диагональный элемент", "Диагональные элементы должны быть нулевыми");
        }
    }
}

void MainWindow::onRepresentationChanged(int index)
{
    if (index == 0) { // Матрица смежности
        int size = sizeCombo->currentData().toInt();
        resizeMatrixTable(size, size);
        matrixTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    } else { // Матрица инцидентности
        int vertices = sizeCombo->currentData().toInt();
        resizeMatrixTable(vertices, std::max(1, vertices-1));
        matrixTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }
}

void MainWindow::resizeMatrix()
{
    if (representationCombo->currentIndex() == 0) {
        int size = sizeCombo->currentData().toInt();
        resizeMatrixTable(size, size);
    } else {
        bool ok;
        int vertices = sizeCombo->currentData().toInt();
        int edges = QInputDialog::getInt(this, "Количество рёбер",
                                       "Введите количество рёбер:",
                                       std::max(1, vertices-1), 1, 100, 1, &ok);
        if (ok) {
            resizeMatrixTable(vertices, edges);
        }
    }
}

bool MainWindow::parseMatrix()
{
    int rows = matrixTable->rowCount();
    int cols = matrixTable->columnCount();

    if (rows == 0 || cols == 0) {
        statusLabel->setText("Матрица пуста");
        return false;
    }

    QVector<QVector<int>> matrix(rows, QVector<int>(cols));

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            QTableWidgetItem* item = matrixTable->item(i, j);
            if (!item) {
                statusLabel->setText(QString("Отсутствует значение в (%1,%2)").arg(i+1).arg(j+1));
                return false;
            }

            bool ok;
            int value = item->text().toInt(&ok);
            if (!ok || value < 0) {
                statusLabel->setText(QString("Некорректное число в (%1,%2)").arg(i+1).arg(j+1));
                return false;
            }
            matrix[i][j] = value;
        }
    }

    if (representationCombo->currentIndex() == 0) {
        // Проверка для матрицы смежности
        for (int i = 0; i < matrix.size(); ++i) {
            if (matrix.size() != matrix[i].size()) {
                statusLabel->setText("Матрица смежности должна быть квадратной");
                return false;
            }
            for (int j = 0; j < i; ++j) {
                if (matrix[i][j] != matrix[j][i]) {
                    statusLabel->setText("Матрица смежности должна быть симметричной");
                    return false;
                }
            }
        }
        graph->createFromAdjacencyMatrix(matrix);
    } else {
        // Проверка для матрицы инцидентности
        for (int j = 0; j < cols; ++j) {
            int onesCount = 0;
            for (int i = 0; i < rows; ++i) {
                if (matrix[i][j] == 1) onesCount++;
                else if (matrix[i][j] != 0) {
                    statusLabel->setText(QString("В столбце %1 должны быть только 0 и 1").arg(j+1));
                    return false;
                }
            }
            if (onesCount != 2) {
                statusLabel->setText(QString("В столбце %1 должно быть ровно две единицы").arg(j+1));
                return false;
            }
        }
        graph->createFromIncidenceMatrix(matrix);
    }

    statusLabel->setText("Матрица успешно загружена");
    return true;
}

void MainWindow::updateGraphView() {
    scene->clear();
    nodes.clear();
    if (!graph || graph->nodeCount() == 0) return;

    const qreal nodeRadius = 25.0;
    const qreal centerX = view->width() / 2.5;
    const qreal centerY = view->height() / 2.5;
    qreal circleRadius = qMin(centerX, centerY) * 0.8;

    // Рассчитываем позиции вершин с проверкой наложения
    QVector<QPointF> positions;
    for (int i = 0; i < graph->nodeCount(); ++i) {
        qreal angle = 2 * M_PI * i / graph->nodeCount();
        QPointF newPos(
            centerX + circleRadius * cos(angle),
            centerY + circleRadius * sin(angle)
        );

        // Проверяем наложение с существующими вершинами
        for (const QPointF& pos : positions) {
            while (QLineF(newPos, pos).length() < nodeRadius * 2.5) {
                circleRadius *= 1.05; // Увеличиваем радиус при наложении
                newPos = QPointF(
                    centerX + circleRadius * cos(angle),
                    centerY + circleRadius * sin(angle)
                );
            }
        }
        positions.append(newPos);
    }

    // Рисуем рёбра
    const auto &adjMatrix = graph->adjacencyMatrix();
    for (int i = 0; i < adjMatrix.size(); ++i) {
        for (int j = i; j < adjMatrix[i].size(); ++j) {
            if (adjMatrix[i][j] > 0 && i < positions.size() && j < positions.size()) {
                QLineF line(positions[i], positions[j]);

                // Корректируем конечные точки для избежания наложения с вершинами
                QLineF radiusLine1(positions[j], positions[i]);
                radiusLine1.setLength(nodeRadius);
                line.setP2(radiusLine1.p2());

                QLineF radiusLine2(positions[i], positions[j]);
                radiusLine2.setLength(nodeRadius);
                line.setP1(radiusLine2.p1());

                QGraphicsLineItem *edge = new QGraphicsLineItem(line);
                edge->setPen(QPen(Qt::black, 2, Qt::SolidLine, Qt::RoundCap));
                scene->addItem(edge);

                if (adjMatrix[i][j] != 1) {
                    QGraphicsSimpleTextItem *weight = new QGraphicsSimpleTextItem(
                        QString::number(adjMatrix[i][j]));
                    weight->setBrush(Qt::black);
                    weight->setFont(QFont("Arial", 10));
                    QPointF center = (line.p1() + line.p2()) / 2.0;
                    weight->setPos(center.x() - weight->boundingRect().width()/2,
                                center.y() - weight->boundingRect().height()/2);
                    scene->addItem(weight);
                }
            }
        }
    }

    // Рисуем вершины с z-value (для правильного порядка отрисовки)
    for (int i = 0; i < positions.size(); ++i) {
        QGraphicsEllipseItem *node = new QGraphicsEllipseItem(
            -nodeRadius, -nodeRadius, nodeRadius*2, nodeRadius*2);
        node->setPos(positions[i]);
        node->setBrush(QColor(70, 130, 180));
        node->setPen(QPen(Qt::black, 1.5));
        node->setFlag(QGraphicsItem::ItemIsMovable);
        node->setFlag(QGraphicsItem::ItemIsSelectable);
        node->setZValue(1); // Вершины поверх рёбер

        QGraphicsSimpleTextItem *label = new QGraphicsSimpleTextItem(QString::number(i+1));
        label->setBrush(Qt::white);
        label->setFont(QFont("Arial", 12, QFont::Bold));
        QRectF textRect = label->boundingRect();
        label->setPos(-textRect.width()/2, -textRect.height()/2);
        label->setParentItem(node);
        label->setZValue(2); // Текст поверх вершин

        scene->addItem(node);
        nodes[i] = node;
    }

    view->centerOn(centerX, centerY);
    startVertexCombo->clear();
}

void MainWindow::highlightBridgesAndArticulations() {
    if (!graph || graph->nodeCount() == 0) return;

    resetEdgeColors(); // Сброс предыдущей подсветки

    // Подсветка мостов (красным)
    QVector<QPair<int, int>> bridges = graph->getBridges();
    for (const auto& bridge : bridges) {
        highlightEdge(bridge.first, bridge.second, Qt::red);
    }

    // Подсветка точек сочленения (оранжевым)
    QVector<int> articulations = graph->getArticulationPoints();
    for (int v : articulations) {
        if (nodes.contains(v)) {
            nodes[v]->setBrush(QColor(255, 165, 0)); // Оранжевый
        }
    }

    // Обновим информацию в интерфейсе
    QString info = "\n=== Критические элементы ===\n";
    if (!bridges.isEmpty()) {
        QStringList bridgeList;
        for (const auto& b : bridges) {
            bridgeList << QString("%1-%2").arg(b.first+1).arg(b.second+1);
        }
        info += "Мосты (красные): " + bridgeList.join(", ") + "\n";
    } else {
        info += "Мостов не найдено\n";
    }

    if (!articulations.isEmpty()) {
        QStringList apList;
        for (int v : articulations) {
            apList << QString::number(v+1);
        }
        info += "Точки сочленения (оранжевые): " + apList.join(", ") + "\n";
    } else {
        info += "Точек сочленения не найдено\n";
    }

    graphPropertiesDisplay->append(info);
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
    int colorsUsed = *std::max_element(colors.begin(), colors.end()) + 1;
    propertiesText += QString("\nХроматическое число (оценка): %1\n").arg(colorsUsed);

    graphPropertiesDisplay->setPlainText(propertiesText);
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == view->viewport() && event->type() == QEvent::MouseMove) {
        for (auto item : scene->selectedItems()) {
            if (dynamic_cast<QGraphicsEllipseItem*>(item)) {
                updateGraphView();
                break;
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::onGenerateClicked()
{
    if (parseMatrix()) {
        updateGraphView();
    }
}
