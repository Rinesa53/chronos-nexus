#pragma once

#include "Galaxy.h"

#include <cstddef>
#include <vector>

namespace chronos {

struct RouteWeights {
    double distance = 1.0;
    double fuel = 0.0;
    double time = 0.0;
    double risk = 0.0;
};

struct PathResult {
    bool found = false;

    double totalCost = 0.0;
    double totalDistance = 0.0;
    double totalFuel = 0.0;
    double totalTime = 0.0;
    double totalRisk = 0.0;
    double totalReliability = 1.0;

    std::vector<int> path;
    std::size_t exploredNodes = 0;
};

class Dijkstra {
public:
    static PathResult findShortestPath(
        const Galaxy& galaxy,
        int start,
        int destination,
        const RouteWeights& weights = RouteWeights{}
    );
};

}