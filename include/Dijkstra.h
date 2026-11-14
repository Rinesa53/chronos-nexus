
#pragma once

#include "Galaxy.h"

#include <cstddef>
#include <vector>

namespace chronos {

struct PathResult {
    bool found = false;
    double totalDistance = 0.0;
    std::vector<int> path;
    std::size_t exploredNodes = 0;
};

class Dijkstra {
public:
    static PathResult findShortestPath(
        const Galaxy& galaxy,
        int start,
        int destination
    );
};

}
