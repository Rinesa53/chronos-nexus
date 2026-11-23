#include "ConstrainedRouter.h"

#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

using namespace chronos;

Galaxy constrainedGalaxy(
    double alternateCapacity = 100.0
) {
    Galaxy galaxy;

    for (int id = 1; id <= 4; ++id) {
        galaxy.addPlanet({
            id,
            "Planet " + std::to_string(id)
        });
    }

    galaxy.addRoute({
        1, 2, 1, 1, 4, 0.1, 100, 0.99
    });

    galaxy.addRoute({
        1, 3, 2, 1, 1, 0.1, alternateCapacity, 0.99
    });

    galaxy.addRoute({
        3, 2, 2, 1, 1, 0.1, 100, 0.99
    });

    galaxy.addRoute({
        2, 4, 1, 1, 3, 0.1, 100, 0.99
    });

    return galaxy;
}

Spacecraft constrainedShip() {
    Spacecraft ship;

    ship.id = 1;
    ship.name = "Explorer";
    ship.currentLocation = 1;

    ship.cargoCapacity = 10.0;
    ship.fuelCapacity = 10.0;
    ship.currentFuel = 5.0;

    return ship;
}

ConstrainedRouteRequest constrainedRequest() {
    ConstrainedRouteRequest request;

    request.origin = 1;
    request.destination = 4;
    request.cargoAmount = 1.0;
    request.timeLimit = 10.0;

    return request;
}

void testFuelFeasibleAlternative() {
    auto galaxy = constrainedGalaxy();

    auto ordinary = Dijkstra::findShortestPath(
        galaxy, 1, 4
    );

    assert(ordinary.totalFuel == 7.0);

    auto result = ConstrainedRouter::find(
        galaxy,
        constrainedShip(),
        constrainedRequest()
    );

    assert(result.path.found);
    assert(result.optimal);

    assert(result.path.totalFuel == 5.0);
    assert(result.path.totalCost == 5.0);

    assert(
        (result.path.path ==
         std::vector<int>{1, 3, 2, 4})
    );

    std::cout
        << "[PASS] Fuel-feasible alternative retained\n";
}

void testTimeCapacityAndAvailability() {
    auto request = constrainedRequest();
    request.timeLimit = 2.0;

    auto result = ConstrainedRouter::find(
        constrainedGalaxy(),
        constrainedShip(),
        request
    );

    assert(!result.path.found);

    result = ConstrainedRouter::find(
        constrainedGalaxy(0.5),
        constrainedShip(),
        constrainedRequest()
    );

    assert(!result.path.found);

    auto galaxy = constrainedGalaxy();
    galaxy.setRouteAvailability(1, 3, false);

    result = ConstrainedRouter::find(
        galaxy,
        constrainedShip(),
        constrainedRequest()
    );

    assert(!result.path.found);

    std::cout
        << "[PASS] Time, capacity and blocked routes respected\n";
}

void testReliability() {
    auto request = constrainedRequest();
    request.minimumReliability = 0.97;

    auto result = ConstrainedRouter::find(
        constrainedGalaxy(),
        constrainedShip(),
        request
    );

    assert(result.path.found);
    assert(result.path.totalReliability >= 0.97);

    request.minimumReliability = 0.98;

    result = ConstrainedRouter::find(
        constrainedGalaxy(),
        constrainedShip(),
        request
    );

    assert(!result.path.found);

    std::cout
        << "[PASS] Reliability threshold respected\n";
}

void testLabelCapCanLoseFeasiblePath() {
    auto request = constrainedRequest();
    request.maxLabelsPerNode = 1;

    auto result = ConstrainedRouter::find(
        constrainedGalaxy(),
        constrainedShip(),
        request
    );

    assert(result.labelsPruned);
    assert(!result.path.found);
    assert(!result.optimal);

    std::cout
        << "[PASS] Heuristic label pruning is reported\n";
}

void testAnytimeBudget() {
    auto galaxy = constrainedGalaxy();

    galaxy.addRoute({
        1, 4, 20, 1, 5, 0.1, 100, 0.99
    });

    auto request = constrainedRequest();
    request.maxStates = 1;

    auto result = ConstrainedRouter::find(
        galaxy,
        constrainedShip(),
        request
    );

    assert(result.budgetExhausted);
    assert(result.path.found);
    assert(!result.optimal);

    assert(result.path.totalCost == 20.0);
    assert(result.statesExplored == 1);

    request.maxStates = 1000;

    result = ConstrainedRouter::find(
        galaxy,
        constrainedShip(),
        request
    );

    assert(result.optimal);
    assert(result.path.totalCost == 5.0);

    std::cout
        << "[PASS] Feasible incumbent returned at search budget\n";
}

void testLabelMemoryBudget() {
    auto request = constrainedRequest();
    request.maxLabelsCreated = 1;

    auto result = ConstrainedRouter::find(
        constrainedGalaxy(),
        constrainedShip(),
        request
    );

    assert(result.budgetExhausted);
    assert(result.labelsCreated == 1);
    assert(!result.path.found);

    std::cout
        << "[PASS] Stored label budget respected\n";
}

void testAgainstExhaustivePaths() {
    std::mt19937 random(42);

    for (int trial = 0; trial < 20; ++trial) {
        Galaxy galaxy;

        for (int id = 1; id <= 5; ++id) {
            galaxy.addPlanet({
                id,
                "P" + std::to_string(id)
            });
        }

        for (int from = 1; from <= 5; ++from) {
            for (int to = 1; to <= 5; ++to) {
                if (from != to) {
                    galaxy.addRoute({
                        from,
                        to,
                        double(1 + random() % 8),
                        double(1 + random() % 4),
                        double(1 + random() % 5),
                        0.0,
                        100.0,
                        1.0
                    });
                }
            }
        }

        auto ship = constrainedShip();
        ship.currentFuel = 7.0;

        auto request = constrainedRequest();

        request.destination = 5;
        request.timeLimit = 6.0;
        request.maxLabelsPerNode = 10000;

        double best =
            std::numeric_limits<double>::infinity();

        std::vector<bool> visited(6, false);

        std::function<
            void(int, double, double, double)
        > visit;

        visit = [&](
            int node,
            double cost,
            double fuel,
            double time
        ) {
            if (node == 5) {
                best = std::min(best, cost);
                return;
            }

            visited[node] = true;

            for (const auto& edge :
                 galaxy.getNeighbors(node)) {
                if (!visited[edge.to] &&
                    fuel + edge.fuelCost <= 7.0 &&
                    time + edge.travelTime <= 6.0) {
                    visit(
                        edge.to,
                        cost + edge.distance,
                        fuel + edge.fuelCost,
                        time + edge.travelTime
                    );
                }
            }

            visited[node] = false;
        };

        visit(1, 0.0, 0.0, 0.0);

        auto result = ConstrainedRouter::find(
            galaxy, ship, request
        );

        assert(!result.budgetExhausted);
        assert(!result.labelsPruned);

        assert(
            result.path.found == std::isfinite(best)
        );

        if (result.path.found) {
            assert(result.optimal);

            assert(
                std::abs(
                    result.path.totalCost - best
                ) < 1e-9
            );
        }
    }

    std::cout
        << "[PASS] Compared with exhaustive paths on 20 graphs\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS CONSTRAINED ROUTING TESTS ===\n\n";

    testFuelFeasibleAlternative();
    testTimeCapacityAndAvailability();
    testReliability();
    testLabelCapCanLoseFeasiblePath();
    testAnytimeBudget();
    testLabelMemoryBudget();
    testAgainstExhaustivePaths();

    std::cout
        << "\nAll constrained routing tests passed!\n";

    return 0;
}