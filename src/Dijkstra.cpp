#include "Dijkstra.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chronos {

namespace {

bool validWeight(double value) {
    return std::isfinite(value) && value >= 0.0;
}

double calculateCost(
    const Route& route,
    const RouteWeights& weights
) {
    double cost =
        weights.distance * route.distance +
        weights.fuel * route.fuelCost +
        weights.time * route.travelTime +
        weights.risk * route.risk;

    if (!std::isfinite(cost)) {
        throw std::overflow_error("Route cost is too large");
    }

    return cost;
}

}

PathResult Dijkstra::findShortestPath(
    const Galaxy& galaxy,
    int start,
    int destination,
    const RouteWeights& weights
) {
    if (!galaxy.hasPlanet(start) ||
        !galaxy.hasPlanet(destination)) {
        throw std::invalid_argument(
            "Unknown start or destination planet"
        );
    }

    if (!validWeight(weights.distance) ||
        !validWeight(weights.fuel) ||
        !validWeight(weights.time) ||
        !validWeight(weights.risk)) {
        throw std::invalid_argument(
            "Weights must be finite and non-negative"
        );
    }

    PathResult result;

    using QueueEntry = std::pair<double, int>;

    std::priority_queue<
        QueueEntry,
        std::vector<QueueEntry>,
        std::greater<QueueEntry>
    > queue;

    std::unordered_map<int, double> costs;
    std::unordered_map<int, Route> previousRoute;

    costs[start] = 0.0;
    queue.push({0.0, start});

    while (!queue.empty()) {
        auto [currentCost, current] = queue.top();
        queue.pop();

        if (currentCost > costs.at(current)) {
            continue;
        }

        ++result.exploredNodes;

        if (current == destination) {
            result.found = true;
            break;
        }

        for (const auto& route : galaxy.getNeighbors(current)) {
            if (!route.available) {
                continue;
            }

            double candidate =
                currentCost + calculateCost(route, weights);

            if (!std::isfinite(candidate)) {
                throw std::overflow_error(
                    "Path cost is too large"
                );
            }

            auto it = costs.find(route.to);

            if (it == costs.end() || candidate < it->second) {
                costs[route.to] = candidate;
                previousRoute.insert_or_assign(route.to, route);
                queue.push({candidate, route.to});
            }
        }
    }

    if (!result.found) {
        return result;
    }

    result.totalCost = costs.at(destination);

    int current = destination;
    result.path.push_back(current);

    while (current != start) {
        const Route& route = previousRoute.at(current);

        result.totalDistance += route.distance;
        result.totalFuel += route.fuelCost;
        result.totalTime += route.travelTime;
        result.totalRisk += route.risk;
        result.totalReliability *= route.reliability;

        current = route.from;
        result.path.push_back(current);
    }

    std::reverse(result.path.begin(), result.path.end());

    return result;
}

}