#include "PreventiveSupplyPlanner.h"
#include "MissionScheduler.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace chronos;

Galaxy supplyGalaxy(
    double donorStock = 100.0,
    double receiverStock = 10.0,
    double routeCapacity = 100.0
) {
    Galaxy galaxy;

    Planet donor{1, "Earth"};

    donor.resources["food"] = donorStock;
    donor.production["food"] = 0.0;
    donor.consumption["food"] = 1.0;
    donor.storageCapacity["food"] = 200.0;

    Planet receiver{2, "Mars"};

    receiver.resources["food"] = receiverStock;
    receiver.production["food"] = 0.0;
    receiver.consumption["food"] = 2.0;
    receiver.storageCapacity["food"] = 100.0;

    galaxy.addPlanet(donor);
    galaxy.addPlanet(receiver);

    galaxy.addRoute({
        1, 2, 10, 1, 5, 0.1, routeCapacity, 0.99
    });

    return galaxy;
}

Spacecraft supplyShip() {
    Spacecraft ship;

    ship.id = 1;
    ship.name = "Supply ship";
    ship.currentLocation = 1;

    ship.cargoCapacity = 50.0;
    ship.fuelCapacity = 50.0;
    ship.currentFuel = 50.0;

    return ship;
}

SupplyRequest supplyRequest() {
    SupplyRequest request;

    request.missionId = 100;
    request.source = 1;
    request.destination = 2;
    request.resource = "food";

    request.currentTime = 100.0;
    request.horizonDays = 10.0;

    return request;
}

void testForecastToSchedule() {
    auto galaxy = supplyGalaxy();
    auto ship = supplyShip();
    auto request = supplyRequest();

    auto proposal = PreventiveSupplyPlanner::propose(
        galaxy, {ship}, {}, request
    );

    assert(proposal.mission.has_value());

    const auto& mission = *proposal.mission;

    // Receiver has 10 units and consumes 2 per day.
    // It empties after 5 days: absolute deadline = 105.
    assert(mission.cargoAmount == 10.0);
    assert(mission.deadline == 105.0);

    SchedulerConfig config;
    config.weights = request.weights;
    config.minimumReliability =
        request.minimumReliability;

    auto schedule = MissionScheduler::plan(
        galaxy,
        {ship},
        {mission},
        request.currentTime,
        config
    );

    assert(schedule.assignments.size() == 1);

    assert(
        schedule.assignments[0].evaluation.arrivalTime ==
        101.0
    );

    assert(
        schedule.assignments[0].evaluation.arrivalTime <
        mission.deadline
    );

    // Planning does not execute the shipment.
    assert(
        galaxy.getPlanet(1).resources.at("food") == 100.0
    );

    std::cout
        << "[PASS] Forecast creates a schedulable preventive mission\n";
}

void testExistingMissionAvoidsDuplicate() {
    auto galaxy = supplyGalaxy();
    auto request = supplyRequest();

    auto first = PreventiveSupplyPlanner::propose(
        galaxy, {supplyShip()}, {}, request
    );

    assert(first.mission.has_value());

    Mission existing = *first.mission;
    existing.id = 7;
    existing.deadline = 102.0;

    auto result = PreventiveSupplyPlanner::propose(
        galaxy, {supplyShip()}, {existing}, request
    );

    assert(!result.mission.has_value());

    assert(
        result.reason ==
        "No uncovered supply requirement"
    );

    std::cout
        << "[PASS] Existing timely mission avoids duplicate demand\n";
}

void testDonorReserve() {
    auto galaxy = supplyGalaxy(20.0);
    auto request = supplyRequest();

    request.safetyStock = 5.0;

    auto result = PreventiveSupplyPlanner::propose(
        galaxy, {supplyShip()}, {}, request
    );

    assert(result.mission.has_value());
    assert(result.mission->cargoAmount == 5.0);

    // Donor retains:
    // 20 stock - 10 days * 1 consumption - 5 cargo = 5.
    assert(
        20.0 - 10.0 - result.mission->cargoAmount ==
        5.0
    );

    std::cout
        << "[PASS] Donor future reserve is protected\n";
}

void testRouteCapacityLimitsShipment() {
    auto galaxy = supplyGalaxy(100.0, 10.0, 3.0);

    auto result = PreventiveSupplyPlanner::propose(
        galaxy, {supplyShip()}, {}, supplyRequest()
    );

    assert(result.mission.has_value());
    assert(result.mission->cargoAmount == 3.0);

    std::cout
        << "[PASS] Shipment is reduced to route capacity\n";
}

void testNoNeedOrNoFeasibleRoute() {
    auto healthy = supplyGalaxy(100.0, 50.0);

    auto result = PreventiveSupplyPlanner::propose(
        healthy, {supplyShip()}, {}, supplyRequest()
    );

    assert(!result.mission.has_value());

    auto blocked = supplyGalaxy();
    blocked.setRouteAvailability(1, 2, false);

    result = PreventiveSupplyPlanner::propose(
        blocked, {supplyShip()}, {}, supplyRequest()
    );

    assert(!result.mission.has_value());

    auto galaxy = supplyGalaxy();
    auto ship = supplyShip();

    ship.currentFuel = 1.0;

    result = PreventiveSupplyPlanner::propose(
        galaxy, {ship}, {}, supplyRequest()
    );

    assert(!result.mission.has_value());

    std::cout
        << "[PASS] Unnecessary and infeasible proposals are rejected\n";
}

void testDuplicateMissionId() {
    auto galaxy = supplyGalaxy();
    auto request = supplyRequest();

    auto first = PreventiveSupplyPlanner::propose(
        galaxy, {supplyShip()}, {}, request
    );

    assert(first.mission.has_value());

    bool thrown = false;

    try {
        PreventiveSupplyPlanner::propose(
            galaxy,
            {supplyShip()},
            {*first.mission},
            request
        );
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout
        << "[PASS] Duplicate mission ID rejected\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS PREVENTIVE SUPPLY TESTS ===\n\n";

    testForecastToSchedule();
    testExistingMissionAvoidsDuplicate();
    testDonorReserve();
    testRouteCapacityLimitsShipment();
    testNoNeedOrNoFeasibleRoute();
    testDuplicateMissionId();

    std::cout
        << "\nAll preventive supply tests passed!\n";

    return 0;
}