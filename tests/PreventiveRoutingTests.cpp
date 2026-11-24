#include "PreventiveSupplyPlanner.h"
#include "MissionScheduler.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace chronos;

Galaxy preventiveGalaxy(
    bool slowCheapRoute = false,
    double cheapCapacity = 10.0,
    double otherCapacity = 10.0
) {
    Galaxy galaxy;

    for (int id = 1; id <= 4; ++id) {
        Planet planet{
            id,
            "P" + std::to_string(id)
        };

        planet.resources["food"] =
            id == 1 ? 100.0 : (id == 4 ? 10.0 : 0.0);

        planet.production["food"] = 0.0;

        planet.consumption["food"] =
            id == 1 ? 1.0 : (id == 4 ? 2.0 : 0.0);

        planet.storageCapacity["food"] = 200.0;

        galaxy.addPlanet(planet);
    }

    if (slowCheapRoute) {
        galaxy.addRoute({
            1, 2, 1, 4, 4, 0.1, 100, 0.99
        });

        galaxy.addRoute({
            2, 4, 1, 4, 3, 0.1, 100, 0.99
        });

        galaxy.addRoute({
            1, 3, 2, 0.25, 1, 0.1, 100, 0.99
        });

        galaxy.addRoute({
            3, 2, 2, 0.25, 1, 0.1, 100, 0.99
        });
    } else {
        galaxy.addRoute({
            1, 2, 1, 1, 1, 0.1, cheapCapacity, 0.99
        });

        galaxy.addRoute({
            2, 4, 1, 1, 1, 0.1, cheapCapacity, 0.99
        });

        galaxy.addRoute({
            1, 3, 2, 1, 1, 0.1, otherCapacity, 0.99
        });

        galaxy.addRoute({
            3, 4, 2, 1, 1, 0.1, otherCapacity, 0.99
        });
    }

    return galaxy;
}

Spacecraft preventiveShip() {
    Spacecraft ship;

    ship.id = 1;
    ship.name = "Supply ship";
    ship.currentLocation = 1;

    ship.cargoCapacity = 50.0;
    ship.fuelCapacity = 50.0;
    ship.currentFuel = 5.0;

    return ship;
}

SupplyRequest preventiveRequest() {
    SupplyRequest request;

    request.missionId = 100;
    request.source = 1;
    request.destination = 4;
    request.resource = "food";
    request.horizonDays = 10.0;

    return request;
}

void testAlternativeBeforeDeadline() {
    auto galaxy = preventiveGalaxy(true);
    auto ship = preventiveShip();
    auto request = preventiveRequest();

    auto proposal = PreventiveSupplyPlanner::propose(
        galaxy, {ship}, {}, request
    );

    assert(proposal.mission.has_value());

    auto result = MissionScheduler::plan(
        galaxy, {ship}, {*proposal.mission}, 0.0
    );

    assert(result.assignments.size() == 1);

    const auto& evaluation =
        result.assignments[0].evaluation;

    assert(
        (evaluation.route.path ==
         std::vector<int>{1, 3, 2, 4})
    );

    assert(evaluation.route.totalFuel == 5.0);
    assert(evaluation.arrivalTime == 4.5);

    assert(
        evaluation.arrivalTime <
        proposal.mission->deadline
    );

    std::cout
        << "[PASS] Supply finds a timely fuel-feasible alternative\n";
}

void testReservedRouteAndReducedCargo() {
    auto galaxy = preventiveGalaxy(false, 10.0, 4.0);

    RouteReservations reserved;
    reserved[{1, 2}] = 10.0;

    auto request = preventiveRequest();
    request.routing.reservedCargo = &reserved;

    auto proposal = PreventiveSupplyPlanner::propose(
        galaxy, {preventiveShip()}, {}, request
    );

    assert(proposal.mission.has_value());
    assert(proposal.mission->cargoAmount == 4.0);

    SchedulerConfig config;
    config.routing = request.routing;

    auto result = MissionScheduler::plan(
        galaxy,
        {preventiveShip()},
        {*proposal.mission},
        0.0,
        config
    );

    assert(result.assignments.size() == 1);

    assert(
        (result.assignments[0].evaluation.route.path ==
         std::vector<int>{1, 3, 4})
    );

    assert(reserved.at({1, 2}) == 10.0);

    std::cout
        << "[PASS] Supply respects reservations and residual capacity\n";
}

void testZeroCapacityIsNotAUsefulCorridor() {
    auto galaxy = preventiveGalaxy(false, 0.0, 4.0);

    auto proposal = PreventiveSupplyPlanner::propose(
        galaxy,
        {preventiveShip()},
        {},
        preventiveRequest()
    );

    assert(proposal.mission.has_value());
    assert(proposal.mission->cargoAmount == 4.0);

    std::cout
        << "[PASS] Probe skips routes with zero free capacity\n";
}

void testBudgetIsReported() {
    auto request = preventiveRequest();
    request.routing.maxStates = 1;

    auto result = PreventiveSupplyPlanner::propose(
        preventiveGalaxy(),
        {preventiveShip()},
        {},
        request
    );

    assert(!result.mission.has_value());
    assert(result.routingBudgetExhausted);
    assert(result.routingStatesExplored == 1);

    std::cout
        << "[PASS] Supply search budget exhaustion is reported\n";
}

void testArrivalStorageRoom() {
    Galaxy galaxy;

    Planet donor{1, "Donor"};

    donor.resources["food"] = 100.0;
    donor.storageCapacity["food"] = 200.0;

    Planet receiver{4, "Receiver"};

    receiver.resources["food"] = 9.0;
    receiver.consumption["food"] = 2.0;
    receiver.storageCapacity["food"] = 10.0;

    galaxy.addPlanet(donor);
    galaxy.addPlanet(receiver);

    galaxy.addRoute({
        1, 4, 1, 1, 1, 0, 100, 1
    });

    auto result = PreventiveSupplyPlanner::propose(
        galaxy,
        {preventiveShip()},
        {},
        preventiveRequest()
    );

    assert(result.mission.has_value());

    // At arrival: 9 - 2 * 1 = 7.
    // Storage room: 10 - 7 = 3.
    assert(result.mission->cargoAmount == 3.0);

    std::cout
        << "[PASS] Cargo respects predicted storage room at arrival\n";
}

void testInvalidRepairBudget() {
    auto request = preventiveRequest();
    request.maxCargoRepairs = 0;

    bool thrown = false;

    try {
        PreventiveSupplyPlanner::propose(
            preventiveGalaxy(),
            {preventiveShip()},
            {},
            request
        );
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout
        << "[PASS] Invalid cargo repair budget is rejected\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS PREVENTIVE ROUTING TESTS ===\n\n";

    testAlternativeBeforeDeadline();
    testReservedRouteAndReducedCargo();
    testZeroCapacityIsNotAUsefulCorridor();
    testBudgetIsReported();
    testArrivalStorageRoom();
    testInvalidRepairBudget();

    std::cout
        << "\nAll preventive routing tests passed!\n";

    return 0;
}