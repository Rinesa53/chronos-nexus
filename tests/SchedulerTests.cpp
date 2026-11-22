#include "MissionScheduler.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace chronos;

Galaxy schedulerGalaxy(
    double stock = 100.0,
    double routeCapacity = 100.0
) {
    Galaxy galaxy;

    Planet earth{1, "Earth"};
    earth.resources["medicine"] = stock;

    galaxy.addPlanet(earth);
    galaxy.addPlanet({2, "Mars"});

    galaxy.addRoute({
        1, 2, 10, 4, 10, 0.1, routeCapacity, 0.99
    });

    return galaxy;
}

Spacecraft schedulerShip(
    int id,
    double operatingCost = 1.0
) {
    Spacecraft ship;

    ship.id = id;
    ship.name = "Cargo ship";
    ship.currentLocation = 1;

    ship.cargoCapacity = 100.0;
    ship.fuelCapacity = 100.0;
    ship.currentFuel = 100.0;

    ship.operatingCost = operatingCost;
    ship.capabilities = {"cargo", "medical"};

    return ship;
}

Mission schedulerMission(int id, int priority = 1) {
    Mission mission;

    mission.id = id;
    mission.origin = 1;
    mission.destination = 2;

    mission.cargoType = "medicine";
    mission.cargoAmount = 20.0;

    mission.deadline = 10.0;
    mission.priority = priority;
    mission.requiredCapabilities = {"cargo", "medical"};

    return mission;
}

void testPriorityAndShipReuse() {
    auto galaxy = schedulerGalaxy();

    auto result = MissionScheduler::plan(
        galaxy,
        {schedulerShip(1)},
        {schedulerMission(1, 1), schedulerMission(2, 10)},
        0.0
    );

    assert(result.assignments.size() == 1);
    assert(result.assignments[0].missionId == 2);

    assert(result.unassigned.size() == 1);
    assert(result.unassigned[0].missionId == 1);

    std::cout
        << "[PASS] Priority and single use of spacecraft\n";
}

void testOperatingCost() {
    auto galaxy = schedulerGalaxy();

    auto result = MissionScheduler::plan(
        galaxy,
        {schedulerShip(1, 5.0), schedulerShip(2, 1.0)},
        {schedulerMission(1)},
        0.0
    );

    assert(result.assignments.size() == 1);
    assert(result.assignments[0].spacecraftId == 2);

    // Time = 4, operating cost per time unit = 1.
    assert(result.assignments[0].operatingCost == 4.0);

    std::cout
        << "[PASS] Cheapest evaluated feasible spacecraft\n";
}

void testSharedRouteCapacity() {
    auto galaxy = schedulerGalaxy(100.0, 30.0);

    auto result = MissionScheduler::plan(
        galaxy,
        {schedulerShip(1), schedulerShip(2)},
        {schedulerMission(1), schedulerMission(2)},
        0.0
    );

    // Two loads of 20 would exceed route capacity 30.
    assert(result.assignments.size() == 1);
    assert(result.unassigned.size() == 1);

    std::cout
        << "[PASS] Shared route capacity respected\n";
}

void testSourceInventory() {
    auto galaxy = schedulerGalaxy(30.0);

    auto result = MissionScheduler::plan(
        galaxy,
        {schedulerShip(1), schedulerShip(2)},
        {schedulerMission(1), schedulerMission(2)},
        0.0
    );

    // Two loads of 20 would exceed source stock 30.
    assert(result.assignments.size() == 1);
    assert(result.unassigned.size() == 1);

    assert(
        result.unassigned[0].reason ==
        "Insufficient source inventory"
    );

    std::cout
        << "[PASS] Source inventory not overallocated\n";
}

void testInputsUnchangedAndDeterminism() {
    auto galaxy = schedulerGalaxy();

    std::vector<Spacecraft> ships{
        schedulerShip(2),
        schedulerShip(1)
    };

    std::vector<Mission> missions{
        schedulerMission(1)
    };

    auto result = MissionScheduler::plan(
        galaxy, ships, missions, 0.0
    );

    assert(result.assignments.size() == 1);

    // Equal costs: choose the smaller spacecraft ID.
    assert(result.assignments[0].spacecraftId == 1);

    // Planning must not execute deliveries.
    assert(
        galaxy.getPlanet(1).resources.at("medicine") == 100.0
    );

    assert(ships[0].currentFuel == 100.0);

    assert(
        missions[0].status == MissionStatus::Pending
    );

    std::cout
        << "[PASS] Deterministic planning without executing\n";
}

void testCandidateBudget() {
    auto galaxy = schedulerGalaxy();

    SchedulerConfig config;
    config.maxCandidateChecks = 1;

    auto result = MissionScheduler::plan(
        galaxy,
        {schedulerShip(1), schedulerShip(2)},
        {schedulerMission(1, 10), schedulerMission(2)},
        0.0,
        config
    );

    assert(result.candidateChecks == 1);
    assert(result.assignments.size() == 1);
    assert(result.unassigned.size() == 1);

    assert(
        result.unassigned[0].reason ==
        "Candidate budget exhausted"
    );

    std::cout
        << "[PASS] Candidate evaluation budget respected\n";
}

void testDuplicateIds() {
    auto galaxy = schedulerGalaxy();

    bool thrown = false;

    try {
        MissionScheduler::plan(
            galaxy,
            {schedulerShip(1), schedulerShip(1)},
            {schedulerMission(1)},
            0.0
        );
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout
        << "[PASS] Duplicate spacecraft IDs rejected\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS SCHEDULER TESTS ===\n\n";

    testPriorityAndShipReuse();
    testOperatingCost();
    testSharedRouteCapacity();
    testSourceInventory();
    testInputsUnchangedAndDeterminism();
    testCandidateBudget();
    testDuplicateIds();

    std::cout << "\nAll scheduler tests passed!\n";

    return 0;
}