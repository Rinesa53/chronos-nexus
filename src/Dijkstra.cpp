
#include "Dijkstra.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chronos {

PathResult Dijkstra::findShortestPath(
    const Galaxy& galaxy,
    int start,
    int destination
) {
    if (!galaxy.hasPlanet(start) || !galaxy.hasPlanet(destination)) {
        throw std::invalid_argument("Unknown start or destination planet");
    }

    PathResult result;

    using QueueEntry = std::pair<double, int>;

    std::priority_queue<
        QueueEntry,
        std::vector<QueueEntry>,
        std::greater<QueueEntry>
    > queue;

    std::unordered_map<int, double> distances;
    std::unordered_map<int, int> previous;

    distances[start] = 0.0;
    queue.push({0.0, start});

    while (!queue.empty()) {
        auto [currentDistance, current] = queue.top();
        queue.pop();

        if (currentDistance > distances.at(current)) {
            continue;
        }

        ++result.exploredNodes;

        if (current == destination) {
            break;
        }

        for (const auto& route : galaxy.getNeighbors(current)) {
            if (!route.available) {
                continue;
            }

            double candidate = currentDistance + route.distance;

            auto it = distances.find(route.to);

            if (it == distances.end() || candidate < it->second) {
                distances[route.to] = candidate;
                previous[route.to] = current;
                queue.push({candidate, route.to});
            }
        }
    }

    if (!distances.contains(destination)) {
        return result;
    }

    result.found = true;
    result.totalDistance = distances.at(destination);

    int current = destination;
    result.path.push_back(current);

    while (current != start) {
        current = previous.at(current);
        result.path.push_back(current);
    }

    std::reverse(result.path.begin(), result.path.end());

    return result;
}

}
