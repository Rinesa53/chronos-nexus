#include "FlightSimulator.h"

#include <cassert>
#include <iostream>
#include <stdexcept>

using namespace chronos;

Planet flightPlanet(
    int id,
    double stock,
    double capacity,
    double consumption = 0.0
) {
    Planet planet{
        id,
        "P" + std::to_string(id)
    };

    planet.resources["food"] = stock;
    planet.storageCapacity["food"] = capacity;
    planet.consumption["food"] = consumption;

    return planet;
}

Galaxy flightGalaxy(
    double sourceStock = 100.0,
    double destinationCapacity = 100.0,
    double sourceConsumption = 0.0,
    double destinationStock = 0.0,
    double destinationConsumption = 0.0
) {
    Galaxy galaxy;

    galaxy.addPlanet(
        flightPlanet(
            1, sourceStock, 200, sourceConsumption
        )
    );

    galaxy.addPlanet(
        flightPlanet(2, 0, 100)
    );

    galaxy.addPlanet(
        flightPlanet(
            3,
            destinationStock,
            destinationCapacity,
            destinationConsumption
        )
    );

    galaxy.addRoute({
        1, 2, 10, 1, 2, 0.1, 100, 0.99
    });

    galaxy.addRoute({
        2, 3, 20, 1, 3, 0.1, 100, 0.99
    });

    galaxy.addRoute({
        2, 1, 10, 1, 2, 0.1, 100, 0.99
    });

    return galaxy;
}

Spacecraft flightShip(
    int id = 1,
    int location = 1
) {
    Spacecraft ship;

    ship.id = id;
    ship.name = "Explorer";
    ship.currentLocation = location;

    ship.cargoCapacity = 20.0;
    ship.fuelCapacity = 20.0;
    ship.currentFuel = 20.0;

    ship.operatingCost = 2.0;

    return ship;
}

Mission flightMission(
    int id = 1,
    int origin = 1,
    int destination = 3,
    double amount = 10.0
) {
    Mission mission;

    mission.id = id;
    mission.origin = origin;
    mission.destination = destination;

    mission.cargoType = "food";
    mission.cargoAmount = amount;
    mission.deadline = 10.0;

    return mission;
}

ScheduleResult nextFlightPlan(
    const Galaxy& galaxy,
    const FlightSimulator& simulator
) {
    SchedulerConfig config;

    config.routing.reservedCargo =
        &simulator.routeReservations();

    return MissionScheduler::plan(
        galaxy,
        simulator.freeShips(),
        simulator.pendingMissions(),
        simulator.currentTime(),
        config
    );
}

void testDepartureIntermediateAndArrival() {
    auto galaxy = flightGalaxy();

    FlightSimulator simulator(
        galaxy,
        {flightShip()},
        {flightMission()}
    );

    auto plan = nextFlightPlan(galaxy, simulator);
    assert(plan.assignments.size() == 1);

    simulator.acceptPlan(plan);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Assigned
    );

    assert(simulator.isBusy(1));

    simulator.advanceTo(0);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::InTransit
    );

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        90.0
    );

    assert(simulator.getShip(1).currentFuel == 18.0);
    assert(simulator.cargoOnboard() == 10.0);

    simulator.advanceTo(1);

    assert(simulator.getShip(1).currentLocation == 2);
    assert(simulator.getShip(1).currentFuel == 15.0);

    simulator.advanceTo(2);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Completed
    );

    assert(simulator.getShip(1).currentLocation == 3);

    assert(
        galaxy.getPlanet(3).resources.at("food") ==
        10.0
    );

    assert(!simulator.isBusy(1));
    assert(simulator.cargoOnboard() == 0.0);

    assert(simulator.routeReservations().empty());
    assert(simulator.stockReservations().empty());

    assert(simulator.statistics().fuelConsumed == 5.0);
    assert(simulator.statistics().totalDistance == 30.0);
    assert(simulator.statistics().operatingCost == 4.0);

    std::cout
        << "[PASS] Departure, intermediate location and arrival are executed\n";
}

void testWaitingDoesNotLoadEarly() {
    auto galaxy = flightGalaxy();
    auto ship = flightShip();

    ship.availabilityTime = 2.0;

    FlightSimulator simulator(
        galaxy, {ship}, {flightMission()}
    );

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(1);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Assigned
    );

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        100.0
    );

    assert(simulator.getShip(1).currentFuel == 20.0);

    simulator.advanceTo(2);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::InTransit
    );

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        90.0
    );

    std::cout
        << "[PASS] Future departure does not load cargo early\n";
}

void testResourcesEvolveDuringFlight() {
    auto galaxy = flightGalaxy(
        100, 100, 0, 1, 1
    );

    FlightSimulator simulator(
        galaxy,
        {flightShip()},
        {flightMission()}
    );

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(3);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Completed
    );

    assert(
        galaxy.getPlanet(3).resources.at("food") ==
        9.0
    );

    assert(
        simulator.resourceStatistics().shortage ==
        1.0
    );

    std::cout
        << "[PASS] Consumption continues before and after delivery\n";
}

void testShortageBeforeDepartureFailsSafely() {
    auto galaxy = flightGalaxy(10, 100, 6);
    auto ship = flightShip();

    ship.availabilityTime = 2.0;

    FlightSimulator simulator(
        galaxy,
        {ship},
        {flightMission(1, 1, 3, 5)}
    );

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(2);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Failed
    );

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        0.0
    );

    assert(simulator.getShip(1).currentFuel == 20.0);

    assert(
        simulator.statistics().resourcesLoaded ==
        0.0
    );

    assert(simulator.routeReservations().empty());
    assert(simulator.stockReservations().empty());

    std::cout
        << "[PASS] Source shortage cancels departure without negative inventory\n";
}

void testInvalidBatchDoesNotPartiallyCommit() {
    auto galaxy = flightGalaxy(10);

    auto first = MissionScheduler::plan(
        galaxy,
        {flightShip(1)},
        {flightMission(1)},
        0.0
    );

    auto second = MissionScheduler::plan(
        galaxy,
        {flightShip(2)},
        {flightMission(2)},
        0.0
    );

    assert(first.assignments.size() == 1);
    assert(second.assignments.size() == 1);

    first.assignments.push_back(
        second.assignments[0]
    );

    FlightSimulator simulator(
        galaxy,
        {flightShip(1), flightShip(2)},
        {flightMission(1), flightMission(2)}
    );

    bool thrown = false;

    try {
        simulator.acceptPlan(first);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    assert(
        simulator.statistics().scheduledMissions ==
        0
    );

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Pending
    );

    assert(
        simulator.getMission(2).status ==
        MissionStatus::Pending
    );

    assert(simulator.routeReservations().empty());
    assert(simulator.stockReservations().empty());

    std::cout
        << "[PASS] An incompatible batch is rejected before admission\n";
}

void testBlockedNextEdgeStopsAtReachedPlanet() {
    auto galaxy = flightGalaxy();

    FlightSimulator simulator(
        galaxy,
        {flightShip()},
        {flightMission()}
    );

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(0);

    galaxy.setRouteAvailability(2, 3, false);

    simulator.advanceTo(1);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Failed
    );

    assert(simulator.getShip(1).currentLocation == 2);

    assert(
        galaxy.getPlanet(2).resources.at("food") ==
        10.0
    );

    assert(simulator.getShip(1).currentFuel == 18.0);

    assert(
        simulator.statistics().resourcesReturned ==
        10.0
    );

    assert(simulator.routeReservations().empty());

    std::cout
        << "[PASS] A blocked next edge stops the flight without teleporting\n";
}

void testDestinationCapacityIsRespected() {
    auto galaxy = flightGalaxy(100, 5);

    FlightSimulator simulator(
        galaxy,
        {flightShip()},
        {flightMission()}
    );

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(2);

    assert(
        galaxy.getPlanet(3).resources.at("food") ==
        5.0
    );

    assert(
        simulator.statistics().resourcesDelivered ==
        5.0
    );

    assert(simulator.statistics().cargoWaste == 5.0);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Failed
    );

    std::cout
        << "[PASS] Destination storage is never exceeded\n";
}

void testShipCanPerformAnotherMissionAfterArrival() {
    auto galaxy = flightGalaxy();

    FlightSimulator simulator(
        galaxy,
        {flightShip()},
        {
            flightMission(1, 1, 2, 10),
            flightMission(2, 2, 1, 5)
        }
    );

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(1);

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(2);

    assert(
        simulator.statistics().completedMissions ==
        2
    );

    assert(simulator.getShip(1).currentLocation == 1);

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        95.0
    );

    assert(
        galaxy.getPlanet(2).resources.at("food") ==
        5.0
    );

    std::cout
        << "[PASS] Ship reuse uses its actual reached location\n";
}

void testArrivalPrecedesDepartureAtSameTime() {
    Galaxy galaxy;

    galaxy.addPlanet(flightPlanet(1, 100, 200));
    galaxy.addPlanet(flightPlanet(2, 5, 100, 5));
    galaxy.addPlanet(flightPlanet(3, 0, 100));

    galaxy.addRoute({
        1, 2, 10, 1, 2, 0, 100, 1
    });

    galaxy.addRoute({
        2, 3, 10, 1, 2, 0, 100, 1
    });

    auto secondShip = flightShip(2, 2);
    secondShip.availabilityTime = 1.0;

    FlightSimulator simulator(
        galaxy,
        {flightShip(1), secondShip},
        {
            flightMission(1, 1, 2, 10),
            flightMission(2, 2, 3, 5)
        }
    );

    auto plan = nextFlightPlan(galaxy, simulator);
    assert(plan.assignments.size() == 2);

    simulator.acceptPlan(plan);
    simulator.advanceTo(2);

    assert(
        simulator.statistics().completedMissions ==
        2
    );

    assert(
        simulator.resourceStatistics().shortage ==
        0.0
    );

    assert(
        galaxy.getPlanet(3).resources.at("food") ==
        5.0
    );

    std::cout
        << "[PASS] Arriving supplies are processed before simultaneous departures\n";
}

void testZeroTimeEdge() {
    Galaxy galaxy;

    galaxy.addPlanet(flightPlanet(1, 100, 200));
    galaxy.addPlanet(flightPlanet(2, 0, 100));

    galaxy.addRoute({
        1, 2, 1, 0, 0, 0, 10, 1
    });

    FlightSimulator simulator(
        galaxy,
        {flightShip()},
        {flightMission(1, 1, 2, 5)}
    );

    simulator.acceptPlan(
        nextFlightPlan(galaxy, simulator)
    );

    simulator.advanceTo(0);

    assert(
        simulator.getMission(1).status ==
        MissionStatus::Completed
    );

    assert(simulator.currentTime() == 0.0);

    assert(
        galaxy.getPlanet(2).resources.at("food") ==
        5.0
    );

    std::cout
        << "[PASS] Zero-time travel is processed without advancing the clock\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS FLIGHT SIMULATION TESTS ===\n\n";

    testDepartureIntermediateAndArrival();
    testWaitingDoesNotLoadEarly();
    testResourcesEvolveDuringFlight();
    testShortageBeforeDepartureFailsSafely();
    testInvalidBatchDoesNotPartiallyCommit();
    testBlockedNextEdgeStopsAtReachedPlanet();
    testDestinationCapacityIsRespected();
    testShipCanPerformAnotherMissionAfterArrival();
    testArrivalPrecedesDepartureAtSameTime();
    testZeroTimeEdge();

    std::cout
        << "\nAll flight simulation tests passed!\n";

    return 0;
}