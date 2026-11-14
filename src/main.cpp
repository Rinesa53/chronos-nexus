#include "Galaxy.h"
#include "Dijkstra.h"

#include <iostream>

using namespace chronos;

void printPath(const PathResult& result) {
    if (!result.found) {
        std::cout << "No route found.\n";
        return;
    }

    std::cout << "Shortest distance: "
              << result.totalDistance << "\n";

    std::cout << "Path: ";

    for (std::size_t i = 0; i < result.path.size(); ++i) {
        std::cout << result.path[i];

        if (i + 1 < result.path.size()) {
            std::cout << " -> ";
        }
    }

    std::cout << "\nExplored nodes: "
              << result.exploredNodes << "\n";
}

int main() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});
    galaxy.addPlanet({3, "Europa"});
    galaxy.addPlanet({4, "Titan"});
    galaxy.addPlanet({5, "Kepler"});

    galaxy.addRoute({1, 2, 15, 10, 25, 0.10, 500, 0.97});
    galaxy.addRoute({1, 4, 20, 12, 30, 0.08, 500, 0.98});
    galaxy.addRoute({2, 3, 10, 8, 20, 0.05, 400, 0.99});
    galaxy.addRoute({2, 4, 12, 9, 20, 0.07, 400, 0.98});
    galaxy.addRoute({3, 5, 7, 5, 12, 0.03, 300, 0.99});
    galaxy.addRoute({4, 5, 9, 6, 15, 0.04, 350, 0.99});

    std::cout << "=== CHRONOS NEXUS ===\n";
    std::cout << "Planets: " << galaxy.getPlanetCount() << "\n";
    std::cout << "Routes: " << galaxy.getRouteCount() << "\n\n";

    std::cout << "Initial route Earth -> Kepler:\n";
    printPath(Dijkstra::findShortestPath(galaxy, 1, 5));

    std::cout << "\nBlocking route Titan -> Kepler...\n";
    galaxy.setRouteAvailability(4, 5, false);

    std::cout << "Updated route Earth -> Kepler:\n";
    printPath(Dijkstra::findShortestPath(galaxy, 1, 5));

    return 0;
}