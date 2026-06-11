#ifndef GRAPH_H
#define GRAPH_H

#include <QVector>
#include <QMap>
#include <QPair>
#include <QQueue>
#include <QStack>
#include <QString>
#include <cmath>
#include <climits> // Для INT_MAX

class Polynomial {
public:
    QVector<long long> coeffs; 

    Polynomial() {}

    // Создать многочлен "x" (x^1)
    static Polynomial X() {
        Polynomial p;
        p.coeffs = {0, 1}; // 0*x^0 + 1*x^1
        return p;
    }

    // Создать число (константу)
    static Polynomial Number(long long n) {
        Polynomial p;
        p.coeffs = {n};
        return p;
    }

    // Операция вычитания: P1 - P2
    Polynomial operator-(const Polynomial& other) const {
        Polynomial res;
        int maxSize = qMax(coeffs.size(), other.coeffs.size());
        res.coeffs.resize(maxSize);

        for (int i = 0; i < maxSize; ++i) {
            long long a = (i < coeffs.size()) ? coeffs[i] : 0;
            long long b = (i < other.coeffs.size()) ? other.coeffs[i] : 0;
            res.coeffs[i] = a - b;
        }
        return res;
    }

    // Операция умножения: P1 * P2
    Polynomial operator*(const Polynomial& other) const {
        Polynomial res;
        res.coeffs.fill(0, coeffs.size() + other.coeffs.size() - 1);
        for (int i = 0; i < coeffs.size(); ++i) {
            for (int j = 0; j < other.coeffs.size(); ++j) {
                res.coeffs[i + j] += coeffs[i] * other.coeffs[j];
            }
        }
        return res;
    }

    // Вывод в строку
    QString toString() const {
        QString res;
        bool first = true;
        for (int i = coeffs.size() - 1; i >= 0; --i) {
            long long c = coeffs[i];
            if (c == 0) continue;

            if (!first) {
                res += (c > 0) ? " + " : " - ";
                c = std::abs(c);
            } else {
                if (c < 0) res += "-";
            }

            if (c != 1 || i == 0) res += QString::number(c);
            if (i > 0) res += "x";
            if (i > 1) res += QString("<sup>%1</sup>").arg(i);

            first = false;
        }
        return res.isEmpty() ? "0" : res;
    }
};

// === СТРУКТУРА ДЛЯ ШАГОВ АЛГОРИТМА (Для обучения) ===
struct AlgoStep {
    int nodeId;
    int edgeTargetId;
    Qt::GlobalColor color; // Или QColor
    QString message;
};

// === ТЕПЕРЬ КЛАСС GRAPH ===
class Graph {
public:
    Graph();

    // Основные методы
    int nodeCount() const;
    int edgeCount() const;
    void clear();

    // Создание
    void createFromAdjacencyMatrix(const QVector<QVector<int>>& matrix);
    void createFromIncidenceMatrix(const QVector<QVector<int>>& matrix);

    // Редактирование
    void addVertex();
    void removeVertex(int index);
    void addEdge(int u, int v, int weight = 1);
    void removeEdge(int u, int v);

    // Настройки
    void setDirected(bool directed) { isDirected = directed; }
    bool getDirected() const { return isDirected; }

    // Геттеры матриц
    const QVector<QVector<int>>& adjacencyMatrix() const;
    const QVector<QVector<int>>& incidenceMatrix() const;

    // Алгоритмы
    QVector<int> calculateDegrees() const;
    bool isConnected() const;
    bool isEulerian() const;
    bool isComplete() const;
    bool isBipartite() const;
    bool hasCycle() const;

    // Сложные алгоритмы
    QVector<QVector<int>> findConnectedComponents() const;
    QVector<QVector<int>> findAllCycles() const;
    QVector<int> greedyColoring() const;
    QVector<int> bipartiteColoring() const;
    QVector<int> getArticulationPoints() const;
    QVector<QPair<int, int>> getBridges() const;

    // Метрики
    QVector<QVector<int>> getDistanceMatrix() const;
    QVector<int> getEccentricities() const;
    int getRadius() const;
    int getDiameter() const;
    int getMedian() const;
    int getTransmissionNumber() const;

    // Пути и обходы
    QVector<int> bfs(int start) const;
    QVector<int> dfs(int start) const;
    QVector<int> dijkstra(int start, int end) const;

    // Новые функции
    QVector<QPair<int, int>> getPrimMST() const; // Мин. остовное дерево

    // === ХРОМАТИЧЕСКИЙ МНОГОЧЛЕН ===
    Polynomial getChromaticPolynomial() const;

private:
    QVector<QVector<int>> adjMatrix;
    QVector<QVector<int>> incMatrix;
    QMap<QPair<int, int>, int> edges;
    bool isDirected = false;

    // Внутренние методы
    void updateIncidenceMatrix();
    void floydWarshall(QVector<QVector<int>>& dist) const;

    // Хелперы для алгоритмов
    bool hasCycleDFSUtil(int v, int parent, QVector<int>& visited, QVector<QPair<int, int>>& cycleEdges) const;
    void findCyclesFromVertex(int start, int current, QVector<bool>& visited, QVector<int>& path, QVector<QVector<int>>& allCycles) const;
    QVector<QVector<int>> removeDuplicateCycles(QVector<QVector<int>>& cycles) const;
    QVector<int> findArticulationPoints() const;
    QVector<QPair<int, int>> findBridges() const;

    // Воркер для многочлена
    Polynomial calculateChromPoly(QVector<QVector<int>> matrix) const;
};

#endif // GRAPH_H
