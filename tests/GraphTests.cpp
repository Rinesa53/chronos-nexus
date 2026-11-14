
#include "Galaxy.h"
#include "Dijkstra.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace chronos;

// Test 1: Find the shortest path
void testShortestPath() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});
    galaxy.addPlanet({3, "Europa"});
    galaxy.addPlanet({4, "Titan"});

    galaxy.addRoute({1, 2, 10, 5, 10, 0.1, 100, 0.99});
    galaxy.addRoute({2, 3, 15, 5, 10, 0.1, 100, 0.99});
    galaxy.addRoute({1, 3, 40, 8, 20, 0.2, 100, 0.95});

    auto result = Dijkstra::findShortestPath(galaxy, 1, 3);

    assert(result.found);
    assert(std::abs(result.totalDistance - 25.0) < 1e-9);
    assert((result.path == std::vector<int>{1, 2, 3}));

    std::cout << "[PASS] Shortest path\n";
}

// Test 2: Recalculate when a route is blocked
void testBlockedRoute() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});
    galaxy.addPlanet({3, "Europa"});

    galaxy.addRoute({1, 2, 10, 5, 10, 0.1, 100, 0.99});
    galaxy.addRoute({2, 3, 10, 5, 10, 0.1, 100, 0.99});
    galaxy.addRoute({1, 3, 30, 8, 15, 0.1, 100, 0.99});

    galaxy.setRouteAvailability(2, 3, false);

    auto result = Dijkstra::findShortestPath(galaxy, 1, 3);

    assert(result.found);
    assert(std::abs(result.totalDistance - 30.0) < 1e-9);
    assert((result.path == std::vector<int>{1, 3}));

    std::cout << "[PASS] Blocked route\n";
}

// Test 3: Destination cannot be reached
void testUnreachablePlanet() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});

    auto result = Dijkstra::findShortestPath(galaxy, 1, 2);

    assert(!result.found);
    assert(result.path.empty());

    std::cout << "[PASS] Unreachable planet\n";
}

// Test 4: Origin equals destination
void testSameOriginDestination() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});

    auto result = Dijkstra::findShortestPath(galaxy, 1, 1);

    assert(result.found);
    assert(result.totalDistance == 0.0);
    assert((result.path == std::vector<int>{1}));

    std::cout << "[PASS] Same origin and destination\n";
}

// Test 5: Reject unknown planets
void testInvalidPlanet() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});

    bool thrown = false;

    try {
        Dijkstra::findShortestPath(galaxy, 1, 99);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout << "[PASS] Invalid planet\n";
}

// Test 6: Reject duplicate routes
void testDuplicateRoute() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});

    Route route{1, 2, 10, 5, 10, 0.1, 100, 0.99};

    galaxy.addRoute(route);

    bool thrown = false;

    try {
        galaxy.addRoute(route);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout << "[PASS] Duplicate route rejection\n";
}

int main() {
    std::cout << "=== CHRONOS NEXUS GRAPH TESTS ===\n\n";

    testShortestPath();
    testBlockedRoute();
    testUnreachablePlanet();
    testSameOriginDestination();
    testInvalidPlanet();
    testDuplicateRoute();

    std::cout << "\nAll graph tests passed!\n";

    return 0;
}
