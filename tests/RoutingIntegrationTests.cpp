#include "MissionScheduler.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace chronos;

Galaxy integrationGalaxy() {
    Galaxy galaxy;

    for (int id = 1; id <= 4; ++id) {
        Planet planet{
            id,
            "P" + std::to_string(id)
        };

        planet.resources["food"] = 10.0;
        galaxy.addPlanet(planet);
    }

    galaxy.addRoute({
        1, 2, 1, 1, 4, 0.1, 100, 0.99
    });

    galaxy.addRoute({
        1, 3, 2, 1, 1, 0.1, 100, 0.99
    });

    galaxy.addRoute({
        3, 2, 2, 1, 1, 0.1, 100, 0.99
    });

    galaxy.addRoute({
        2, 4, 1, 1, 3, 0.1, 100, 0.99
    });

    return galaxy;
}

Spacecraft integrationShip() {
    Spacecraft ship;

    ship.id = 1;
    ship.name = "Explorer";
    ship.currentLocation = 1;

    ship.cargoCapacity = 10.0;
    ship.fuelCapacity = 10.0;
    ship.currentFuel = 5.0;

    return ship;
}

Mission integrationMission() {
    Mission mission;

    mission.id = 1;
    mission.origin = 1;
    mission.destination = 4;

    mission.cargoType = "food";
    mission.cargoAmount = 1.0;
    mission.deadline = 10.0;

    return mission;
}

void testEvaluatorFindsAlternative() {
    auto galaxy = integrationGalaxy();
    auto ship = integrationShip();
    auto mission = integrationMission();

    RoutingOptions baseline;
    baseline.mode = RoutingMode::WeightedDijkstra;

    auto oldResult = MissionEvaluator::evaluate(
        galaxy,
        ship,
        mission,
        0.0,
        RouteWeights{},
        0.0,
        baseline
    );

    assert(!oldResult.feasible);

    auto result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(result.feasible);
    assert(result.routeOptimal);
    assert(result.route.totalFuel == 5.0);

    assert(
        (result.route.path ==
         std::vector<int>{1, 3, 2, 4})
    );

    std::cout
        << "[PASS] Evaluator finds fuel-feasible alternative\n";
}

void testWaitingTimeAndSpeed() {
    auto galaxy = integrationGalaxy();
    auto ship = integrationShip();
    auto mission = integrationMission();

    ship.speed = 2.0;
    ship.availabilityTime = 102.0;
    mission.deadline = 105.0;

    auto result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 100.0
    );

    assert(result.feasible);
    assert(result.departureTime == 102.0);

    // Three edges, each with reference time 1.
    // Actual duration = 3 / speed 2 = 1.5.
    assert(result.route.totalTime == 1.5);
    assert(result.arrivalTime == 103.5);

    std::cout
        << "[PASS] Waiting and speed are applied once\n";
}

void testReliabilityAppliedOnce() {
    auto galaxy = integrationGalaxy();
    auto ship = integrationShip();

    ship.reliability = 0.99;

    auto result = MissionEvaluator::evaluate(
        galaxy,
        ship,
        integrationMission(),
        0.0,
        RouteWeights{},
        0.96
    );

    assert(result.feasible);

    // Three edges: 0.99^3.
    assert(
        std::abs(
            result.route.totalReliability -
            0.970299
        ) < 1e-9
    );

    // Ship and three edges: 0.99^4.
    assert(
        std::abs(
            result.combinedReliability -
            0.96059601
        ) < 1e-9
    );

    std::cout
        << "[PASS] Ship reliability is multiplied once\n";
}

void testSchedulerModes() {
    auto galaxy = integrationGalaxy();
    auto ship = integrationShip();
    auto mission = integrationMission();

    auto result = MissionScheduler::plan(
        galaxy, {ship}, {mission}, 0.0
    );

    assert(result.assignments.size() == 1);

    SchedulerConfig baseline;

    baseline.routing.mode =
        RoutingMode::WeightedDijkstra;

    auto oldResult = MissionScheduler::plan(
        galaxy,
        {ship},
        {mission},
        0.0,
        baseline
    );

    assert(oldResult.assignments.empty());

    std::cout
        << "[PASS] Scheduler can compare routing modes\n";
}

void testBudgetMetadataReachesScheduler() {
    auto galaxy = integrationGalaxy();

    galaxy.addRoute({
        1, 4, 20, 1, 5, 0.1, 100, 0.99
    });

    SchedulerConfig config;
    config.routing.maxStates = 1;

    auto result = MissionScheduler::plan(
        galaxy,
        {integrationShip()},
        {integrationMission()},
        0.0,
        config
    );

    assert(result.assignments.size() == 1);

    const auto& evaluation =
        result.assignments[0].evaluation;

    assert(evaluation.feasible);
    assert(evaluation.routingBudgetExhausted);
    assert(!evaluation.routeOptimal);
    assert(evaluation.routingStatesExplored == 1);

    std::cout
        << "[PASS] Feasible budget-limited route retains metadata\n";
}

void testExpiredAvailability() {
    auto ship = integrationShip();
    ship.availabilityTime = 11.0;

    auto result = MissionEvaluator::evaluate(
        integrationGalaxy(),
        ship,
        integrationMission(),
        0.0
    );

    assert(!result.feasible);

    std::cout
        << "[PASS] Availability after deadline is rejected\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS ROUTING INTEGRATION TESTS ===\n\n";

    testEvaluatorFindsAlternative();
    testWaitingTimeAndSpeed();
    testReliabilityAppliedOnce();
    testSchedulerModes();
    testBudgetMetadataReachesScheduler();
    testExpiredAvailability();

    std::cout
        << "\nAll routing integration tests passed!\n";

    return 0;
}