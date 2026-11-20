#include "Galaxy.h"
#include "Mission.h"
#include "MissionEvaluator.h"
#include "Spacecraft.h"

#include <cassert>
#include <cmath>
#include <iostream>

using namespace chronos;

Galaxy createMissionGalaxy() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});

    // distance = 10
    // reference travel time = 8
    // fuel = 20
    // capacity = 50
    // reliability = 0.98
    galaxy.addRoute({
        1, 2, 10, 8, 20, 0.1, 50, 0.98
    });

    return galaxy;
}

Spacecraft createTestShip() {
    Spacecraft ship;

    ship.id = 1;
    ship.name = "Medical Explorer";
    ship.currentLocation = 1;

    ship.cargoCapacity = 100.0;
    ship.fuelCapacity = 100.0;
    ship.currentFuel = 100.0;

    ship.speed = 2.0;
    ship.reliability = 0.99;
    ship.availabilityTime = 0.0;
    ship.operatingCost = 5.0;

    ship.capabilities = {"cargo", "medical"};

    return ship;
}

Mission createTestMission() {
    Mission mission;

    mission.id = 1;
    mission.origin = 1;
    mission.destination = 2;

    mission.cargoType = "medicine";
    mission.cargoAmount = 20.0;

    mission.deadline = 10.0;
    mission.priority = 10;
    mission.importance = 5.0;

    mission.requiredCapabilities = {"cargo", "medical"};

    return mission;
}

void testFeasibleAssignment() {
    Galaxy galaxy = createMissionGalaxy();
    Spacecraft ship = createTestShip();
    Mission mission = createTestMission();

    RouteWeights timeWeights{0.0, 0.0, 1.0, 0.0};

    auto result = MissionEvaluator::evaluate(
        galaxy,
        ship,
        mission,
        0.0,
        timeWeights,
        0.95
    );

    assert(result.feasible);
    assert(result.route.found);

    // Reference time 8 / speed 2 = actual time 4.
    assert(
        std::abs(result.route.totalTime - 4.0) < 1e-9
    );

    assert(
        std::abs(result.arrivalTime - 4.0) < 1e-9
    );

    // With time-only weights, cost is actual travel time.
    assert(
        std::abs(result.route.totalCost - 4.0) < 1e-9
    );

    assert(
        std::abs(
            result.combinedReliability - 0.99 * 0.98
        ) < 1e-9
    );

    // Evaluation must not consume fuel or change status.
    assert(ship.currentFuel == 100.0);
    assert(mission.status == MissionStatus::Pending);

    std::cout << "[PASS] Feasible assignment\n";
}

void testShipRequirements() {
    Galaxy galaxy = createMissionGalaxy();
    Mission mission = createTestMission();

    Spacecraft ship = createTestShip();
    ship.operational = false;

    auto result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    ship = createTestShip();
    ship.currentLocation = 2;

    result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    ship = createTestShip();
    ship.cargoCapacity = 10.0;

    result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    ship = createTestShip();
    ship.capabilities = {"cargo"};

    result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    std::cout << "[PASS] Ship requirements checked\n";
}

void testFuelAndDeadline() {
    Galaxy galaxy = createMissionGalaxy();
    Spacecraft ship = createTestShip();
    Mission mission = createTestMission();

    ship.currentFuel = 19.0;

    auto result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    ship = createTestShip();
    mission.deadline = 3.0;

    result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    std::cout << "[PASS] Fuel and deadline checked\n";
}

void testAvailability() {
    Galaxy galaxy = createMissionGalaxy();
    Spacecraft ship = createTestShip();
    Mission mission = createTestMission();

    // Departure at 6, travel time 4, arrival at 10.
    ship.availabilityTime = 6.0;

    auto result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(result.feasible);
    assert(result.departureTime == 6.0);
    assert(result.arrivalTime == 10.0);

    // Departure at 7 means arrival at 11: too late.
    ship.availabilityTime = 7.0;

    result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    // Availability at 11 is already past the deadline.
    ship.availabilityTime = 11.0;

    result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    std::cout << "[PASS] Availability checked\n";
}

void testRouteRequirements() {
    Galaxy galaxy = createMissionGalaxy();
    Spacecraft ship = createTestShip();
    Mission mission = createTestMission();

    // Ship capacity is 100, but route capacity is only 50.
    mission.cargoAmount = 60.0;

    auto result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    mission = createTestMission();

    // Combined reliability is 0.9702, below 0.99.
    result = MissionEvaluator::evaluate(
        galaxy,
        ship,
        mission,
        0.0,
        RouteWeights{},
        0.99
    );

    assert(!result.feasible);

    galaxy.setRouteAvailability(1, 2, false);

    result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    std::cout << "[PASS] Route requirements checked\n";
}

void testMissionStatus() {
    Galaxy galaxy = createMissionGalaxy();
    Spacecraft ship = createTestShip();
    Mission mission = createTestMission();

    mission.status = MissionStatus::Completed;

    auto result = MissionEvaluator::evaluate(
        galaxy, ship, mission, 0.0
    );

    assert(!result.feasible);

    std::cout << "[PASS] Non-pending mission rejected\n";
}

int main() {
    std::cout << "=== CHRONOS NEXUS MISSION TESTS ===\n\n";

    testFeasibleAssignment();
    testShipRequirements();
    testFuelAndDeadline();
    testAvailability();
    testRouteRequirements();
    testMissionStatus();

    std::cout << "\nAll mission tests passed!\n";

    return 0;
}