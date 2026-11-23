#include "MissionScheduler.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace chronos;

Galaxy capacityGalaxy() {
    Galaxy galaxy;

    for (int id = 1; id <= 4; ++id) {
        Planet planet{
            id,
            "P" + std::to_string(id)
        };

        planet.resources["food"] = 100.0;
        galaxy.addPlanet(planet);
    }

    galaxy.addRoute({
        1, 2, 1, 1, 1, 0.1, 10, 0.99
    });

    galaxy.addRoute({
        2, 4, 1, 1, 1, 0.1, 10, 0.99
    });

    galaxy.addRoute({
        1, 3, 2, 1, 1, 0.1, 10, 0.99
    });

    galaxy.addRoute({
        3, 4, 2, 1, 1, 0.1, 10, 0.99
    });

    return galaxy;
}

Spacecraft capacityShip(int id) {
    Spacecraft ship;

    ship.id = id;
    ship.name = "Cargo ship";
    ship.currentLocation = 1;

    ship.cargoCapacity = 10.0;
    ship.fuelCapacity = 100.0;
    ship.currentFuel = 100.0;

    return ship;
}

Mission capacityMission(
    int id,
    double amount = 10.0
) {
    Mission mission;

    mission.id = id;
    mission.origin = 1;
    mission.destination = 4;

    mission.cargoType = "food";
    mission.cargoAmount = amount;
    mission.deadline = 10.0;

    return mission;
}

void testSecondMissionUsesAlternative() {
    auto galaxy = capacityGalaxy();

    auto result = MissionScheduler::plan(
        galaxy,
        {capacityShip(1), capacityShip(2)},
        {capacityMission(1), capacityMission(2)},
        0.0
    );

    assert(result.assignments.size() == 2);

    assert(
        (result.assignments[0].evaluation.route.path ==
         std::vector<int>{1, 2, 4})
    );

    assert(
        (result.assignments[1].evaluation.route.path ==
         std::vector<int>{1, 3, 4})
    );

    std::cout
        << "[PASS] Second mission uses an unreserved alternative\n";
}

void testPartialReservation() {
    auto galaxy = capacityGalaxy();

    RouteReservations reserved;
    reserved[{1, 2}] = 7.0;

    RoutingOptions routing;
    routing.reservedCargo = &reserved;

    auto result = MissionEvaluator::evaluate(
        galaxy,
        capacityShip(1),
        capacityMission(1, 5.0),
        0.0,
        RouteWeights{},
        0.0,
        routing
    );

    assert(result.feasible);

    assert(
        (result.route.path ==
         std::vector<int>{1, 3, 4})
    );

    assert(reserved.at({1, 2}) == 7.0);

    std::cout
        << "[PASS] Partial reservation reduces usable capacity\n";
}

void testExistingReservationsPreserved() {
    auto galaxy = capacityGalaxy();

    RouteReservations reserved;

    reserved[{1, 2}] = 10.0;
    reserved[{2, 4}] = 10.0;

    SchedulerConfig config;
    config.routing.reservedCargo = &reserved;

    auto result = MissionScheduler::plan(
        galaxy,
        {capacityShip(1), capacityShip(2)},
        {capacityMission(1), capacityMission(2)},
        0.0,
        config
    );

    assert(result.assignments.size() == 1);
    assert(result.unassigned.size() == 1);

    assert(
        (result.assignments[0].evaluation.route.path ==
         std::vector<int>{1, 3, 4})
    );

    // The caller's reservation table must not change.
    assert(reserved.size() == 2);
    assert(reserved.at({1, 2}) == 10.0);

    std::cout
        << "[PASS] External reservations are respected without mutation\n";
}

void testBaselineRejectsReservedRoute() {
    auto galaxy = capacityGalaxy();

    RouteReservations reserved;
    reserved[{1, 2}] = 10.0;

    RoutingOptions routing;

    routing.mode = RoutingMode::WeightedDijkstra;
    routing.reservedCargo = &reserved;

    auto result = MissionEvaluator::evaluate(
        galaxy,
        capacityShip(1),
        capacityMission(1),
        0.0,
        RouteWeights{},
        0.0,
        routing
    );

    assert(!result.feasible);

    std::cout
        << "[PASS] Baseline does not accept overbooked capacity\n";
}

void testInvalidReservation() {
    auto galaxy = capacityGalaxy();

    RouteReservations reserved;
    reserved[{1, 2}] = -1.0;

    RoutingOptions routing;
    routing.reservedCargo = &reserved;

    bool thrown = false;

    try {
        MissionEvaluator::evaluate(
            galaxy,
            capacityShip(1),
            capacityMission(1),
            0.0,
            RouteWeights{},
            0.0,
            routing
        );
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout
        << "[PASS] Negative reservation is rejected\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS CAPACITY ROUTING TESTS ===\n\n";

    testSecondMissionUsesAlternative();
    testPartialReservation();
    testExistingReservationsPreserved();
    testBaselineRejectsReservedRoute();
    testInvalidReservation();

    std::cout
        << "\nAll capacity routing tests passed!\n";

    return 0;
}