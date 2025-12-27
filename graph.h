#ifndef GRAPH_H
#define GRAPH_H

#include <QVector>
#include <QMap>
#include <QPair>
#include <climits>

class Graph
{
public:
    Graph();

    int nodeCount() const;
    int edgeCount() const;

    void clear();
    void createFromAdjacencyMatrix(const QVector<QVector<int>>& matrix);
    void createFromIncidenceMatrix(const QVector<QVector<int>>& matrix);

    const QVector<QVector<int>>& adjacencyMatrix() const;
    const QVector<QVector<int>>& incidenceMatrix() const;

    QVector<int> calculateDegrees() const;
    QVector<QVector<int>> getDistanceMatrix() const;
    QVector<int> getEccentricities() const;
    int getRadius() const;
    int getDiameter() const;
    int getMedian() const;
    int getTransmissionNumber() const;
    bool isEulerian() const;
    bool isConnected() const;
    bool isTree() const;
    QVector<int> findArticulationPoints() const;
    QVector<QPair<int, int>> findBridges() const;
    QVector<int> topologicalSort() const;
    QVector<QVector<int>> findConnectedComponents() const;
    QVector<int> findShortestPath(int start, int end) const;
    int vertexConnectivity() const;
    int edgeConnectivity() const;
    bool isComplete() const;
    QVector<int> greedyColoring() const;
    bool isBipartite() const;
    QVector<int> bipartiteColoring() const;
     QVector<QVector<int>> findAllCycles() const;
     bool hasCycle() const;
     QVector<QPair<int, int>> findCycleEdges() const;
    QVector<int> bfs(int start) const;
    QVector<int> dfs(int start) const;
    QVector<int> dijkstra(int start, int end) const;
    QVector<QPair<int, int>> getBridges() const;  // Возвращает рёбра-мосты
    QVector<int> getArticulationPoints() const;


private:
    QVector<QVector<int>> adjMatrix;
    QVector<QVector<int>> incMatrix;
    QMap<QPair<int, int>, int> edges; // Храним ребра с i < j

    void updateIncidenceMatrix();
    void floydWarshall(QVector<QVector<int>>& dist) const;
    void findAllCyclesUtil(int v, int start, int depth, QVector<bool>& visited,
                          QVector<int>& path, QVector<QVector<int>>& cycles) const;
       bool hasCycleDFSUtil(int v, int parent, QVector<int>& visited, QVector<QPair<int, int>>& cycleEdges) const;
       void findCyclesFromVertex(int start, int current, QVector<bool>& visited,
                               QVector<int>& path, QVector<QVector<int>>& allCycles) const;
       QVector<QVector<int>> removeDuplicateCycles(QVector<QVector<int>>& cycles) const;

};


#endif // GRAPH_H
