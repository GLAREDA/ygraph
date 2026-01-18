#include "graph.h"
#include <algorithm>
#include <numeric>
// Добавьте в начало mainwindow.cpp
#include <QInputDialog>
#include <QStack>
#include <QQueue>
#include <QSet>

Graph::Graph() {}

int Graph::nodeCount() const { return adjMatrix.size(); }
int Graph::edgeCount() const { return edges.size(); }

void Graph::clear() {
    adjMatrix.clear();
    incMatrix.clear();
    edges.clear();
}

void Graph::createFromAdjacencyMatrix(const QVector<QVector<int>>& matrix) {
    clear();
    adjMatrix = matrix;

    edges.clear();
    int n = adjMatrix.size();

    for (int i = 0; i < n; ++i) {
        // Если граф ориентированный, проходим по всей матрице.
        // Если нет - только по верхнему треугольнику, чтобы не дублировать ребра.
        int startJ = isDirected ? 0 : i;

        for (int j = startJ; j < n; ++j) {
            if (adjMatrix[i][j] > 0) {
                edges.insert({i, j}, adjMatrix[i][j]);
            }
        }
    }
    updateIncidenceMatrix();
}

void Graph::createFromIncidenceMatrix(const QVector<QVector<int>>& matrix) {
    clear();
    incMatrix = matrix;

    int nodes = matrix.size();
    if (nodes == 0) return;
    int edgesCount = matrix[0].size();

    adjMatrix.resize(nodes);
    for (int i = 0; i < nodes; ++i) {
        adjMatrix[i].resize(nodes);
        adjMatrix[i].fill(0);
    }

    for (int j = 0; j < edgesCount; ++j) {
        int u = -1, v = -1;
        int valU = 0, valV = 0;

        for (int i = 0; i < nodes; ++i) {
            if (matrix[i][j] != 0) {
                if (u == -1) { u = i; valU = matrix[i][j]; }
                else if (v == -1) { v = i; valV = matrix[i][j]; }
            }
        }

        if (u != -1 && v != -1) {
            if (isDirected) {
                // ОРГРАФ
                if (valU == 1 && valV == -1) {
                    adjMatrix[u][v] = 1; // u -> v
                    edges.insert({u, v}, 1);
                }
                else if (valU == -1 && valV == 1) {
                    adjMatrix[v][u] = 1; // v -> u
                    edges.insert({v, u}, 1);
                }
                else {
                    // === ИСПРАВЛЕНИЕ ЗДЕСЬ ===
                    // Если (1, 1) или (-1, -1) -> Двусторонняя связь
                    adjMatrix[u][v] = 1;
                    adjMatrix[v][u] = 1;
                    edges.insert({u, v}, 1);
                    edges.insert({v, u}, 1);
                }
            }
            else {
                // НЕОРИЕНТИРОВАННЫЙ
                adjMatrix[u][v] = 1;
                adjMatrix[v][u] = 1;
                edges.insert({qMin(u, v), qMax(u, v)}, 1);
            }
        }
    }
}

const QVector<QVector<int>>& Graph::adjacencyMatrix() const { return adjMatrix; }
const QVector<QVector<int>>& Graph::incidenceMatrix() const { return incMatrix; }

QVector<int> Graph::calculateDegrees() const {
    QVector<int> degrees(nodeCount(), 0);
    for (int i = 0; i < nodeCount(); ++i) {
        for (int j = 0; j < nodeCount(); ++j) {
            if (adjMatrix[i][j] > 0) {
                degrees[i]++;
            }
        }
    }
    return degrees;
}

void Graph::floydWarshall(QVector<QVector<int>>& dist) const {
    int n = adjMatrix.size();
    for (int k = 0; k < n; ++k) {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (dist[i][k] != INT_MAX && dist[k][j] != INT_MAX &&
                    dist[i][k] + dist[k][j] < dist[i][j]) {
                    dist[i][j] = dist[i][k] + dist[k][j];
                }
            }
        }
    }
}
// В graph.cpp реализацию:
bool Graph::isConnected() const {
    if (adjMatrix.isEmpty()) return false;

    int n = adjMatrix.size();
    QVector<bool> visited(n, false);
    QQueue<int> queue;

    queue.enqueue(0);
    visited[0] = true;
    int visitedCount = 1;

    while (!queue.isEmpty()) {
        int u = queue.dequeue();
        for (int v = 0; v < n; ++v) {
            if (adjMatrix[u][v] > 0 && !visited[v]) {
                visited[v] = true;
                visitedCount++;
                queue.enqueue(v);
            }
        }
    }

    return visitedCount == n;
}

QVector<QVector<int>> Graph::findConnectedComponents() const {
    QVector<QVector<int>> components;
    if (adjMatrix.isEmpty()) return components;

    int n = adjMatrix.size();
    QVector<bool> visited(n, false);

    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            QVector<int> component;
            QStack<int> stack;
            stack.push(i);
            visited[i] = true;

            while (!stack.isEmpty()) {
                int u = stack.pop();
                component.append(u);

                for (int v = 0; v < n; ++v) {
                    if (adjMatrix[u][v] > 0 && !visited[v]) {
                        visited[v] = true;
                        stack.push(v);
                    }
                }
            }

            components.append(component);
        }
    }

    return components;
}

bool Graph::isComplete() const {
    if (adjMatrix.isEmpty()) return false;

    int n = adjMatrix.size();

    // Проверяем, что все недиагональные элементы равны 1, а диагональные - 0
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            if (i == j) {
                // Диагональный элемент должен быть 0
                if (adjMatrix[i][j] != 0) {
                    return false;
                }
            } else {
                // Недиагональный элемент должен быть 1 (или >0 для взвешенного графа)
                if (adjMatrix[i][j] <= 0) {
                    return false;
                }
            }
        }
    }

    return true;
}

bool Graph::isBipartite() const {
    int n = adjMatrix.size();
    if (n == 0) return true; // Пустой граф считается двудольным

    QVector<int> color(n, -1); // -1 - не раскрашено, 0 и 1 - цвета
    QQueue<int> q;

    for (int start = 0; start < n; ++start) {
        if (color[start] == -1) {
            color[start] = 0;
            q.enqueue(start);

            while (!q.isEmpty()) {
                int u = q.dequeue();

                for (int v = 0; v < n; ++v) {
                    if (adjMatrix[u][v] > 0) { // Есть ребро u-v
                        if (color[v] == -1) { // Вершина не раскрашена
                            color[v] = 1 - color[u]; // Инвертируем цвет
                            q.enqueue(v);
                        }
                        else if (color[v] == color[u]) {
                            return false; // Нашли конфликт
                        }
                    }
                }
            }
        }
    }
    return true;
}

bool Graph::hasCycle() const {
    if (adjMatrix.empty()) return false;

    QVector<int> visited(adjMatrix.size(), 0); // 0 - не посещена, 1 - в обработке, 2 - обработана
    QVector<QPair<int, int>> dummy; // Пустой вектор для перегрузки

    for (int i = 0; i < adjMatrix.size(); ++i) {
        if (visited[i] == 0 && hasCycleDFSUtil(i, -1, visited, dummy)) {
            return true;
        }
    }
    return false;
}

bool Graph::hasCycleDFSUtil(int v, int parent, QVector<int>& visited, QVector<QPair<int, int>>& cycleEdges) const {
    visited[v] = 1; // Помечаем вершину как посещённую

    // Проверяем всех соседей вершины v
    for (int u = 0; u < adjMatrix.size(); ++u) {
        if (adjMatrix[v][u] > 0) { // Если есть ребро между v и u
            if (!visited[u]) {
                if (hasCycleDFSUtil(u, v, visited, cycleEdges)) {
                    cycleEdges.append(qMakePair(v, u));
                    return true;
                }
            }
            else if (u != parent) {
                // Нашли цикл - добавляем ребро
                cycleEdges.append(qMakePair(v, u));
                return true;
            }
        }
    }

    visited[v] = 2; // Помечаем вершину как полностью обработанную
    return false;
}

QVector<QVector<int>> Graph::findAllCycles() const {
    QVector<QVector<int>> allCycles;
    if (adjMatrix.empty()) return allCycles;

    QVector<bool> visited(adjMatrix.size(), false);
    QVector<int> path;

    for (int i = 0; i < adjMatrix.size(); ++i) {
        if (!visited[i]) {
            findCyclesFromVertex(i, i, visited, path, allCycles);
        }
    }

    // Удаляем дубликаты циклов
    return removeDuplicateCycles(allCycles);
}

void Graph::findCyclesFromVertex(int start, int current, QVector<bool>& visited,
                               QVector<int>& path, QVector<QVector<int>>& allCycles) const {
    visited[current] = true;
    path.push_back(current);

    for (int neighbor = 0; neighbor < adjMatrix.size(); ++neighbor) {
        if (adjMatrix[current][neighbor] > 0) { // Есть ребро
            if (neighbor == start && path.size() >= 3) {
                // Найден цикл длиной >= 3
                QVector<int> cycle = path;
                cycle.push_back(start);
                allCycles.push_back(cycle);
            }
            else if (!visited[neighbor] && neighbor > start) {
                // Продолжаем поиск только для вершин с большим индексом
                findCyclesFromVertex(start, neighbor, visited, path, allCycles);
            }
        }
    }

    path.pop_back();
    visited[current] = false;
}

QVector<QVector<int>> Graph::removeDuplicateCycles(QVector<QVector<int>>& cycles) const {
    // Нормализуем циклы (начинаем с минимальной вершины)
    for (auto& cycle : cycles) {
        if (cycle.size() < 3) continue;

        int minPos = 0;
        for (int i = 1; i < cycle.size()-1; ++i) {
            if (cycle[i] < cycle[minPos]) minPos = i;
        }
        std::rotate(cycle.begin(), cycle.begin()+minPos, cycle.end()-1);
        cycle.back() = cycle.front(); // Замыкаем цикл
    }

    // Удаляем дубликаты
    std::sort(cycles.begin(), cycles.end());
    cycles.erase(std::unique(cycles.begin(), cycles.end()), cycles.end());

    return cycles;
}

QVector<int> Graph::bipartiteColoring() const {
    int n = adjMatrix.size();
    QVector<int> color(n, -1);
    if (n == 0) return color;

    QQueue<int> q;

    for (int start = 0; start < n; ++start) {
        if (color[start] == -1) {
            color[start] = 0;
            q.enqueue(start);

            while (!q.isEmpty()) {
                int u = q.dequeue();

                for (int v = 0; v < n; ++v) {
                    if (adjMatrix[u][v] > 0) {
                        if (color[v] == -1) {
                            color[v] = 1 - color[u];
                            q.enqueue(v);
                        }
                        else if (color[v] == color[u]) {
                            return QVector<int>(); // Возвращаем пустой вектор если не двудольный
                        }
                    }
                }
            }
        }
    }
    return color;
}

QVector<int> Graph::greedyColoring() const {
    int n = adjMatrix.size();
    QVector<int> result(n, -1); // Инициализируем все вершины как нераскрашенные
    if (n == 0) return result;

    // Создаем список вершин, отсортированных по убыванию степени
    QVector<QPair<int, int>> degrees; // (степень, вершина)
    for (int i = 0; i < n; ++i) {
        int degree = 0;
        for (int j = 0; j < n; ++j) {
            if (adjMatrix[i][j] > 0) degree++;
        }
        degrees.append(qMakePair(degree, i));
    }

    // Сортируем вершины по убыванию степени
    std::sort(degrees.begin(), degrees.end(),
              [](const QPair<int, int>& a, const QPair<int, int>& b) {
                  return a.first > b.first;
              });

    // Жадная раскраска
    result[degrees[0].second] = 0; // Первой вершине - первый цвет

    for (int i = 1; i < n; ++i) {
        int vertex = degrees[i].second;
        QSet<int> usedColors;

        // Собираем цвета соседей
        for (int j = 0; j < n; ++j) {
            if (adjMatrix[vertex][j] > 0 && result[j] != -1) {
                usedColors.insert(result[j]);
            }
        }

        // Находим минимальный доступный цвет
        int color = 0;
        while (true) {
            if (!usedColors.contains(color)) {
                result[vertex] = color;
                break;
            }
            color++;
        }
    }

    return result;
}

// Поиск точек сочленения с помощью алгоритма Тарьяна
QVector<int> Graph::findArticulationPoints() const {
    QVector<int> articulationPoints;
    if (adjMatrix.isEmpty()) return articulationPoints;

    int n = adjMatrix.size();
    QVector<int> disc(n, -1);
    QVector<int> low(n, -1);
    QVector<int> parent(n, -1);
    QVector<bool> visited(n, false);
    int time = 0;

    QStack<int> stack;
    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            stack.push(i);
            visited[i] = true;
            disc[i] = low[i] = ++time;

            while (!stack.isEmpty()) {
                int u = stack.top();
                bool isLeaf = true;

                for (int v = 0; v < n; ++v) {
                    if (adjMatrix[u][v] > 0) {
                        if (!visited[v]) {
                            isLeaf = false;
                            parent[v] = u;
                            visited[v] = true;
                            disc[v] = low[v] = ++time;
                            stack.push(v);
                            break;
                        } else if (v != parent[u]) {
                            low[u] = qMin(low[u], disc[v]);
                        }
                    }
                }

                if (isLeaf) {
                    stack.pop();
                    if (u != i) { // Не корень
                        if (parent[u] != -1) {
                            low[parent[u]] = qMin(low[parent[u]], low[u]);
                            if (low[u] >= disc[parent[u]] && parent[parent[u]] != -1) {
                                if (!articulationPoints.contains(parent[u])) {
                                    articulationPoints.append(parent[u]);
                                }
                            }
                        }
                    }
                    // Проверка корня
                    if (u == i) {
                        int children = 0;
                        for (int v = 0; v < n; ++v) {
                            if (adjMatrix[u][v] > 0 && parent[v] == u) {
                                children++;
                            }
                        }
                        if (children > 1) {
                            articulationPoints.append(u);
                        }
                    }
                }
            }
        }
    }

    return articulationPoints;
}

// Поиск мостов с помощью алгоритма Тарьяна
QVector<QPair<int, int>> Graph::findBridges() const {
    QVector<QPair<int, int>> bridges;
    if (adjMatrix.isEmpty()) return bridges;

    int n = adjMatrix.size();
    QVector<int> disc(n, -1);
    QVector<int> low(n, -1);
    QVector<int> parent(n, -1);
    QVector<bool> visited(n, false);
    int time = 0;

    QStack<int> stack;
    for (int i = 0; i < n; ++i) {
        if (!visited[i]) {
            stack.push(i);
            visited[i] = true;
            disc[i] = low[i] = ++time;

            while (!stack.isEmpty()) {
                int u = stack.top();
                bool isLeaf = true;

                for (int v = 0; v < n; ++v) {
                    if (adjMatrix[u][v] > 0) {
                        if (!visited[v]) {
                            isLeaf = false;
                            parent[v] = u;
                            visited[v] = true;
                            disc[v] = low[v] = ++time;
                            stack.push(v);
                            break;
                        } else if (v != parent[u]) {
                            low[u] = qMin(low[u], disc[v]);
                        }
                    }
                }

                if (isLeaf) {
                    stack.pop();
                    if (parent[u] != -1) {
                        low[parent[u]] = qMin(low[parent[u]], low[u]);
                        if (low[u] > disc[parent[u]]) {
                            bridges.append(qMakePair(qMin(u, parent[u]), qMax(u, parent[u])));
                        }
                    }
                }
            }
        }
    }

    return bridges;
}

QVector<QVector<int>> Graph::getDistanceMatrix() const {
    int n = adjMatrix.size();
    QVector<QVector<int>> dist(n, QVector<int>(n, INT_MAX));

    for (int i = 0; i < n; ++i) {
        dist[i][i] = 0;
        for (int j = 0; j < n; ++j) {
            if (adjMatrix[i][j] > 0) {
                dist[i][j] = adjMatrix[i][j];
            }
        }
    }

    floydWarshall(dist);
    return dist;
}

QVector<int> Graph::getEccentricities() const {
    auto dist = getDistanceMatrix();
    QVector<int> ecc(dist.size(), 0);

    for (int i = 0; i < dist.size(); ++i) {
        int maxDist = 0;
        for (int j = 0; j < dist[i].size(); ++j) {
            if (dist[i][j] > maxDist && dist[i][j] != INT_MAX) {
                maxDist = dist[i][j];
            }
        }
        ecc[i] = maxDist;
    }

    return ecc;
}

int Graph::getRadius() const {
    auto ecc = getEccentricities();
    return *std::min_element(ecc.begin(), ecc.end());
}

int Graph::getDiameter() const {
    auto ecc = getEccentricities();
    return *std::max_element(ecc.begin(), ecc.end());
}

int Graph::getMedian() const {
    auto dist = getDistanceMatrix();
    int minSum = INT_MAX;
    int median = -1;

    for (int i = 0; i < dist.size(); ++i) {
        int sum = std::accumulate(dist[i].begin(), dist[i].end(), 0);
        if (sum < minSum) {
            minSum = sum;
            median = i;
        }
    }

    return median;
}

QVector<int> Graph::bfs(int start) const {
    QVector<int> traversal;
    if (adjMatrix.isEmpty() || start < 0 || start >= adjMatrix.size()) return traversal;

    QVector<bool> visited(adjMatrix.size(), false);
    QQueue<int> queue;
    queue.enqueue(start);
    visited[start] = true;

    while (!queue.isEmpty()) {
        int u = queue.dequeue();
        traversal.append(u);

        for (int v = 0; v < adjMatrix.size(); ++v) {
            if (adjMatrix[u][v] > 0 && !visited[v]) {
                visited[v] = true;
                queue.enqueue(v);
            }
        }
    }

    return traversal;
}


QVector<QPair<int, int>> Graph::getBridges() const {
    return findBridges(); // Используем существующий метод
}

QVector<int> Graph::getArticulationPoints() const {
    return findArticulationPoints(); // Используем существующий метод
}

QVector<int> Graph::dfs(int start) const {
    QVector<int> traversal;
    if (adjMatrix.isEmpty() || start < 0 || start >= adjMatrix.size()) return traversal;

    QVector<bool> visited(adjMatrix.size(), false);
    QStack<int> stack;
    stack.push(start);

    while (!stack.isEmpty()) {
        int u = stack.pop();
        if (!visited[u]) {
            visited[u] = true;
            traversal.append(u);

            // Добавляем соседей в обратном порядке для сохранения порядка обхода
            for (int v = adjMatrix.size() - 1; v >= 0; --v) {
                if (adjMatrix[u][v] > 0 && !visited[v]) {
                    stack.push(v);
                }
            }
        }
    }

    return traversal;
}

QVector<int> Graph::dijkstra(int start, int end) const {
    QVector<int> path;
    if (adjMatrix.isEmpty() || start < 0 || start >= adjMatrix.size() ||
        end < 0 || end >= adjMatrix.size()) return path;

    int n = adjMatrix.size();
    QVector<int> dist(n, INT_MAX);
    QVector<int> prev(n, -1);
    QVector<bool> visited(n, false);

    dist[start] = 0;

    for (int i = 0; i < n - 1; ++i) {
        int u = -1;
        for (int j = 0; j < n; ++j) {
            if (!visited[j] && (u == -1 || dist[j] < dist[u])) {
                u = j;
            }
        }

        if (dist[u] == INT_MAX) break;

        visited[u] = true;

        for (int v = 0; v < n; ++v) {
            if (adjMatrix[u][v] > 0 && !visited[v]) {
                int alt = dist[u] + adjMatrix[u][v];
                if (alt < dist[v]) {
                    dist[v] = alt;
                    prev[v] = u;
                }
            }
        }
    }

    // Восстановление пути
    if (prev[end] != -1 || start == end) {
        for (int at = end; at != -1; at = prev[at]) {
            path.prepend(at);
        }
    }

    return path;
}

int Graph::getTransmissionNumber() const {
    return getDiameter() - getRadius();
}

bool Graph::isEulerian() const {
    auto degrees = calculateDegrees();
    int oddCount = 0;
    for (int deg : degrees) {
        if (deg % 2 != 0) oddCount++;
    }
    return oddCount == 0 || oddCount == 2;
}

void Graph::updateIncidenceMatrix() {
    incMatrix.clear();
    if (adjMatrix.isEmpty()) return;

    int nodes = adjMatrix.size();
    int edgeCount = edges.size();
    incMatrix.resize(nodes);

    for (int i = 0; i < nodes; ++i) {
        incMatrix[i].resize(edgeCount);
        incMatrix[i].fill(0);
    }

    int edgeIndex = 0;
    // Используем итератор по edges (QMap<QPair<int,int>, int>)
    for (auto it = edges.begin(); it != edges.end(); ++it) {
        int from = it.key().first;
        int to = it.key().second;

        if (isDirected) {
            // Ориентированный: 1 (start) -> -1 (end)
            // Если петля (from == to), можно ставить 2 или что-то особое, но пока пропустим
            if (from != to) {
                incMatrix[from][edgeIndex] = 1;
                incMatrix[to][edgeIndex] = -1;
            }
        } else {
            // Неориентированный: 1 --- 1
            incMatrix[from][edgeIndex] = 1;
            incMatrix[to][edgeIndex] = 1;
        }
        edgeIndex++;
    }
}

void Graph::addVertex() {
    int n = adjMatrix.size();
    // Добавляем столбец
    for (int i = 0; i < n; ++i) {
        adjMatrix[i].append(0);
    }
    // Добавляем строку
    QVector<int> newRow(n + 1, 0);
    adjMatrix.append(newRow);

    // ВАЖНО: Создаем копию перед обновлением!
    QVector<QVector<int>> matrixCopy = adjMatrix;
    createFromAdjacencyMatrix(matrixCopy);
}

void Graph::removeVertex(int index) {
    if (index < 0 || index >= adjMatrix.size()) return;

    adjMatrix.removeAt(index);
    for (int i = 0; i < adjMatrix.size(); ++i) {
        adjMatrix[i].removeAt(index);
    }

    // ВАЖНО: Копия
    QVector<QVector<int>> matrixCopy = adjMatrix;
    createFromAdjacencyMatrix(matrixCopy);
}

void Graph::addEdge(int u, int v, int weight) {
    if (u < 0 || u >= adjMatrix.size() || v < 0 || v >= adjMatrix.size()) return;

    adjMatrix[u][v] = weight;

    // Если граф НЕ ориентированный, добавляем обратную связь
    if (!isDirected) {
        adjMatrix[v][u] = weight;
    }

    // Важно: копируем для обновления кэшей
    QVector<QVector<int>> matrixCopy = adjMatrix;
    createFromAdjacencyMatrix(matrixCopy);
}

void Graph::removeEdge(int u, int v) {
    addEdge(u, v, 0); // addEdge уже делает копию, так что тут менять не надо
}

QVector<QPair<int, int>> Graph::getPrimMST() const {
    QVector<QPair<int, int>> mstEdges;
    int n = adjMatrix.size();
    if (n == 0) return mstEdges;

    // Массив для хранения минимального веса ребра до вершины
    QVector<int> key(n, INT_MAX);
    // Массив родителей (откуда пришли)
    QVector<int> parent(n, -1);
    // Массив посещенных
    QVector<bool> inMST(n, false);

    // Начинаем с вершины 0
    key[0] = 0;

    for (int count = 0; count < n - 1; ++count) {
        // 1. Ищем вершину с минимальным key, которая еще не в MST
        int min = INT_MAX, u = -1;
        for (int v = 0; v < n; ++v) {
            if (!inMST[v] && key[v] < min) {
                min = key[v];
                u = v;
            }
        }

        if (u == -1) break; // Граф несвязный

        inMST[u] = true;

        // Если это не корень, добавляем ребро в результат
        if (parent[u] != -1) {
            mstEdges.append({parent[u], u});
        }

        // 2. Обновляем соседей
        for (int v = 0; v < n; ++v) {
            int weight = adjMatrix[u][v];
            // Если есть ребро, v не в MST, и вес меньше текущего известного
            if (weight > 0 && !inMST[v] && weight < key[v]) {
                parent[v] = u;
                key[v] = weight;
            }
        }
    }

    return mstEdges;
}

Polynomial Graph::getChromaticPolynomial() const {
    if (adjMatrix.size() > 12) {
        // Защита от зависания
        return Polynomial(); // Возвращаем пустоту как ошибку
    }
    return calculateChromPoly(adjMatrix);
}

Polynomial Graph::calculateChromPoly(QVector<QVector<int>> currentMatrix) const {
    int n = currentMatrix.size();

    // 1. Ищем ребро
    int u = -1, v = -1;
    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (currentMatrix[i][j] > 0) {
                u = i; v = j;
                break;
            }
        }
        if (u != -1) break;
    }

    // 2. Если ребер нет -> x^n
    if (u == -1) {
        Polynomial res = Polynomial::Number(1);
        for (int i = 0; i < n; ++i) res = res * Polynomial::X();
        return res;
    }

    // 3. Deletion (G - e)
    QVector<QVector<int>> matrixDel = currentMatrix;
    matrixDel[u][v] = 0;
    matrixDel[v][u] = 0;
    Polynomial pDel = calculateChromPoly(matrixDel);

    // 4. Contraction (G * e)
    QVector<QVector<int>> matrixContr(n - 1, QVector<int>(n - 1));

    // Лямбда для пересчета индексов
    auto mapIdx = [&](int oldIdx) {
        if (oldIdx == v) return u;
        if (oldIdx > v) return oldIdx - 1;
        return oldIdx;
    };

    for (int i = 0; i < n; ++i) {
        if (i == v) continue;
        for (int j = 0; j < n; ++j) {
            if (j == v) continue;

            if (currentMatrix[i][j] > 0) {
                int newI = mapIdx(i);
                int newJ = mapIdx(j);
                if (newI != newJ) {
                    matrixContr[newI][newJ] = 1;
                    matrixContr[newJ][newI] = 1;
                }
            }
        }
    }

    Polynomial pContr = calculateChromPoly(matrixContr);

    return pDel - pContr;
}
