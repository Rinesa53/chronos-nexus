#include "Galaxy.h"
#include "Dijkstra.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace chronos;

// Test 1: Find the shortest path.
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

// Test 2: Recalculate when a route is blocked.
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

// Test 3: Destination cannot be reached.
void testUnreachablePlanet() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});

    auto result = Dijkstra::findShortestPath(galaxy, 1, 2);

    assert(!result.found);
    assert(result.path.empty());

    std::cout << "[PASS] Unreachable planet\n";
}

// Test 4: Origin equals destination.
void testSameOriginDestination() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});

    auto result = Dijkstra::findShortestPath(galaxy, 1, 1);

    assert(result.found);
    assert(result.totalDistance == 0.0);
    assert((result.path == std::vector<int>{1}));

    std::cout << "[PASS] Same origin and destination\n";
}

// Test 5: Reject unknown planets.
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

// Test 6: Reject duplicate routes.
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

// Test 7: Different weights can select different paths.
void testWeightedRouting() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});
    galaxy.addPlanet({3, "Europa"});

    // Route fields:
    // from, to, distance, travelTime, fuelCost,
    // risk, capacity, reliability

    galaxy.addRoute({1, 2, 10, 4, 10, 0.1, 100, 0.99});
    galaxy.addRoute({2, 3, 5, 4, 10, 0.1, 100, 0.99});
    galaxy.addRoute({1, 3, 20, 2, 5, 0.3, 100, 0.95});

    // Default weights: minimize distance.
    auto distanceResult =
        Dijkstra::findShortestPath(galaxy, 1, 3);

    assert(distanceResult.found);
    assert(
        (distanceResult.path == std::vector<int>{1, 2, 3})
    );
    assert(
        std::abs(distanceResult.totalCost - 15.0) < 1e-9
    );
    assert(
        std::abs(distanceResult.totalFuel - 20.0) < 1e-9
    );

    // Verify combined reliability along the two-edge path.
    assert(
        std::abs(
            distanceResult.totalReliability - 0.99 * 0.99
        ) < 1e-9
    );

    // Minimize time.
    RouteWeights timeWeights{0.0, 0.0, 1.0, 0.0};

    auto timeResult =
        Dijkstra::findShortestPath(
            galaxy, 1, 3, timeWeights
        );

    assert(timeResult.found);
    assert(
        (timeResult.path == std::vector<int>{1, 3})
    );
    assert(
        std::abs(timeResult.totalCost - 2.0) < 1e-9
    );
    assert(
        std::abs(timeResult.totalDistance - 20.0) < 1e-9
    );
    assert(
        std::abs(timeResult.totalTime - 2.0) < 1e-9
    );
    assert(
        std::abs(timeResult.totalFuel - 5.0) < 1e-9
    );
    assert(
        std::abs(timeResult.totalRisk - 0.3) < 1e-9
    );
    assert(
        std::abs(timeResult.totalReliability - 0.95) < 1e-9
    );

    // Minimize fuel.
    RouteWeights fuelWeights{0.0, 1.0, 0.0, 0.0};

    auto fuelResult =
        Dijkstra::findShortestPath(
            galaxy, 1, 3, fuelWeights
        );

    assert(fuelResult.found);
    assert(
        (fuelResult.path == std::vector<int>{1, 3})
    );
    assert(
        std::abs(fuelResult.totalCost - 5.0) < 1e-9
    );

    // Minimize risk.
    RouteWeights riskWeights{0.0, 0.0, 0.0, 1.0};

    auto riskResult =
        Dijkstra::findShortestPath(
            galaxy, 1, 3, riskWeights
        );

    assert(riskResult.found);
    assert(
        (riskResult.path == std::vector<int>{1, 2, 3})
    );
    assert(
        std::abs(riskResult.totalCost - 0.2) < 1e-9
    );

    // Combine distance, fuel, time and risk.
    RouteWeights mixedWeights{1.0, 1.0, 1.0, 10.0};

    auto mixedResult =
        Dijkstra::findShortestPath(
            galaxy, 1, 3, mixedWeights
        );

    assert(mixedResult.found);
    assert(
        (mixedResult.path == std::vector<int>{1, 3})
    );

    // Direct path: 20 + 5 + 2 + 10 * 0.3 = 30.
    // Via Mars:   15 + 20 + 8 + 10 * 0.2 = 45.
    assert(
        std::abs(mixedResult.totalCost - 30.0) < 1e-9
    );

    std::cout << "[PASS] Weighted routing\n";
}

// Test 8: Reject negative weights.
void testInvalidWeights() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});

    galaxy.addRoute({1, 2, 10, 4, 10, 0.1, 100, 0.99});

    RouteWeights invalidWeights{-1.0, 0.0, 0.0, 0.0};

    bool thrown = false;

    try {
        Dijkstra::findShortestPath(
            galaxy, 1, 2, invalidWeights
        );
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout << "[PASS] Invalid weights rejected\n";
}

int main() {
    std::cout << "=== CHRONOS NEXUS GRAPH TESTS ===\n\n";

    testShortestPath();
    testBlockedRoute();
    testUnreachablePlanet();
    testSameOriginDestination();
    testInvalidPlanet();
    testDuplicateRoute();
    testWeightedRouting();
    testInvalidWeights();

    std::cout << "\nAll graph tests passed!\n";

    return 0;
}