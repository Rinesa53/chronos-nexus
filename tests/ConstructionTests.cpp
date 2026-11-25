#include "ConstructionSimulator.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace chronos;

Galaxy constructionGalaxy(double production = 0.0) {
    Galaxy galaxy;

    Planet planet{1, "Earth"};

    planet.population = 100;
    planet.technologyLevel = 1;
    planet.defenseLevel = 1;

    planet.resources["food"] = 10;
    planet.production["food"] = production;
    planet.storageCapacity["food"] = 200;

    planet.resources["energy"] = 20;
    planet.production["energy"] = 0;
    planet.storageCapacity["energy"] = 100;

    galaxy.addPlanet(planet);

    return galaxy;
}

Facility constructionFacility(
    int id = 1,
    double duration = 2.0,
    double impact = 5.0
) {
    Facility facility;

    facility.id = id;
    facility.planetId = 1;

    facility.name =
        "Facility " + std::to_string(id);

    facility.constructionCost = 4;
    facility.constructionTime = duration;
    facility.strategicValue = 9;

    facility.productionImpact["food"] = impact;

    return facility;
}

template <typename Action>
void expectInvalidConstruction(Action action) {
    bool thrown = false;

    try {
        action();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);
}

void testEffectsActivateAtCompletion() {
    auto galaxy = constructionGalaxy();

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    std::vector<Facility> catalog{
        constructionFacility()
    };

    auto plan = InfrastructureOptimizer::select(
        galaxy, catalog, 10
    );

    construction.startPlan(catalog, plan);

    assert(construction.remainingBudget() == 6);

    construction.advanceTo(1);
    assert(galaxy.getPlanet(1).production.at("food") == 0);

    construction.advanceTo(2);
    assert(galaxy.getPlanet(1).resources.at("food") == 10);
    assert(galaxy.getPlanet(1).production.at("food") == 5);

    construction.advanceTo(4);
    assert(galaxy.getPlanet(1).resources.at("food") == 20);

    assert(construction.statistics().completed == 1);
    assert(construction.statistics().completedValue == 9);

    std::cout
        << "[PASS] Production changes only after construction completes\n";
}

void testPopulationEnergyAndLevels() {
    auto galaxy = constructionGalaxy();

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    auto research = constructionFacility();

    research.type = FacilityType::ResearchStation;
    research.productionImpact.clear();

    research.energyImpact = -2;
    research.populationImpact = 10;
    research.technologyImpact = 2;
    research.defenseImpact = 1;

    auto plan = InfrastructureOptimizer::select(
        galaxy, {research}, 10
    );

    construction.startPlan({research}, plan);
    construction.advanceTo(4);

    const auto& planet = galaxy.getPlanet(1);

    assert(planet.population == 110);
    assert(planet.technologyLevel == 3);
    assert(planet.defenseLevel == 2);
    assert(planet.resources.at("energy") == 16);

    auto energyPlant = constructionFacility(2);
    energyPlant.productionImpact.clear();
    energyPlant.energyImpact = 3;

    auto preview = ConstructionEffects::preview(
        galaxy, planet, energyPlant
    );

    assert(preview.production.at("energy") == 3);
    assert(galaxy.getPlanet(1).production.at("energy") == 0);

    std::cout
        << "[PASS] Population, energy, technology and defense effects apply\n";
}

void testChronologicalCompletion() {
    auto galaxy = constructionGalaxy();

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    std::vector<Facility> catalog{
        constructionFacility(1, 1, 2),
        constructionFacility(2, 3, 3)
    };

    construction.startPlan(
        catalog,
        InfrastructureOptimizer::select(
            galaxy, catalog, 10
        )
    );

    construction.advanceTo(4);

    // Days 0-1: no production.
    // Days 1-3: 2/day.
    // Days 3-4: 5/day.
    assert(galaxy.getPlanet(1).resources.at("food") == 19);
    assert(construction.statistics().completed == 2);
    assert(construction.currentTime() == flights.currentTime());

    std::cout
        << "[PASS] Multiple projects complete in chronological order\n";
}

void testActualCostIsChecked() {
    auto galaxy = constructionGalaxy();

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 6);

    auto facility = constructionFacility();
    facility.constructionCost = 7;

    InfrastructurePlan forged;
    forged.facilityIds = {1};
    forged.totalCost = 0;

    expectInvalidConstruction([&]() {
        construction.startPlan({facility}, forged);
    });

    assert(construction.remainingBudget() == 6);
    assert(construction.statistics().started == 0);
    assert(construction.history().empty());

    std::cout
        << "[PASS] Admission recomputes cost without trusting plan metadata\n";
}

void testFacilityCannotBeBuiltTwice() {
    auto galaxy = constructionGalaxy();

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    std::vector<Facility> catalog{
        constructionFacility()
    };

    auto plan = InfrastructureOptimizer::select(
        galaxy, catalog, 10
    );

    construction.startPlan(catalog, plan);
    construction.advanceTo(2);

    expectInvalidConstruction([&]() {
        construction.startPlan(catalog, plan);
    });

    assert(construction.statistics().started == 1);
    assert(construction.statistics().spentBudget == 4);

    std::cout
        << "[PASS] A facility ID cannot be constructed twice\n";
}

void testCombinedEffectsAreValidatedBeforeSpending() {
    auto galaxy = constructionGalaxy(10);

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    std::vector<Facility> catalog{
        constructionFacility(1, 1, -7),
        constructionFacility(2, 2, -7)
    };

    auto plan = InfrastructureOptimizer::select(
        galaxy, catalog, 10
    );

    expectInvalidConstruction([&]() {
        construction.startPlan(catalog, plan);
    });

    assert(construction.remainingBudget() == 10);
    assert(construction.statistics().started == 0);
    assert(galaxy.getPlanet(1).production.at("food") == 10);

    std::cout
        << "[PASS] Incompatible combined effects do not spend budget\n";
}

void testChangedStateCanFailCompletionSafely() {
    auto galaxy = constructionGalaxy(2);

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    auto facility = constructionFacility(1, 2, -1);

    construction.startPlan(
        {facility},
        InfrastructureOptimizer::select(
            galaxy, {facility}, 10
        )
    );

    Planet changed = galaxy.getPlanet(1);
    changed.production["food"] = 0;

    galaxy.replacePlanetState(changed);

    construction.advanceTo(2);

    assert(
        construction.getJob(1).status ==
        ConstructionStatus::Failed
    );

    assert(construction.statistics().failed == 1);
    assert(construction.remainingBudget() == 6);
    assert(galaxy.getPlanet(1).production.at("food") == 0);

    std::cout
        << "[PASS] Invalid completion leaves the planet unchanged\n";
}

void testConstructionAndFlightsShareClock() {
    auto galaxy = constructionGalaxy();

    Planet destination{2, "Mars"};
    destination.population = 10;
    destination.resources["food"] = 0;
    destination.storageCapacity["food"] = 100;

    galaxy.addPlanet(destination);

    galaxy.addRoute({
        1, 2, 10, 2, 1, 0, 100, 1
    });

    Spacecraft ship;

    ship.id = 1;
    ship.name = "Cargo ship";
    ship.currentLocation = 1;
    ship.cargoCapacity = 20;
    ship.fuelCapacity = 10;
    ship.currentFuel = 10;

    Mission mission;

    mission.id = 1;
    mission.origin = 1;
    mission.destination = 2;
    mission.cargoType = "food";
    mission.cargoAmount = 10;
    mission.deadline = 10;

    FlightSimulator flights(
        galaxy, {ship}, {mission}
    );

    auto flightPlan = MissionScheduler::plan(
        galaxy,
        flights.freeShips(),
        flights.pendingMissions(),
        0
    );

    flights.acceptPlan(flightPlan);

    ConstructionSimulator construction(flights, 10);

    auto farm = constructionFacility(1, 1, 2);
    farm.planetId = 2;

    construction.startPlan(
        {farm},
        InfrastructureOptimizer::select(
            galaxy, {farm}, 10
        )
    );

    construction.advanceTo(3);

    // Farm produces 4 during days 1-3.
    // Ship delivers 10 at day 2.
    assert(galaxy.getPlanet(2).resources.at("food") == 14);

    assert(
        flights.getMission(1).status ==
        MissionStatus::Completed
    );

    assert(construction.statistics().completed == 1);
    assert(flights.currentTime() == 3);

    std::cout
        << "[PASS] Flights and construction use one simulation clock\n";
}

void testInstantConstruction() {
    auto galaxy = constructionGalaxy();

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    auto facility = constructionFacility(1, 0, 1);

    construction.startPlan(
        {facility},
        InfrastructureOptimizer::select(
            galaxy, {facility}, 10
        )
    );

    construction.advanceTo(0);

    assert(
        construction.getJob(1).status ==
        ConstructionStatus::Completed
    );

    assert(construction.currentTime() == 0);

    construction.advanceTo(1);

    assert(galaxy.getPlanet(1).resources.at("food") == 11);

    std::cout
        << "[PASS] Zero-time construction completes at the current time\n";
}

void testSkippedConstructionClockIsDetected() {
    auto galaxy = constructionGalaxy();

    FlightSimulator flights(galaxy, {}, {});
    ConstructionSimulator construction(flights, 10);

    auto facility = constructionFacility(1, 1);

    construction.startPlan(
        {facility},
        InfrastructureOptimizer::select(
            galaxy, {facility}, 10
        )
    );

    // Deliberately bypass the construction controller.
    flights.advanceTo(2);

    bool thrown = false;

    try {
        construction.advanceTo(3);
    } catch (const std::logic_error&) {
        thrown = true;
    }

    assert(thrown);

    std::cout
        << "[PASS] Skipping a pending completion is detected\n";
}

void testPopulationOverflowIsRejected() {
    auto galaxy = constructionGalaxy();

    Planet planet = galaxy.getPlanet(1);

    planet.population =
        std::numeric_limits<long long>::max();

    galaxy.replacePlanetState(planet);

    auto facility = constructionFacility();
    facility.populationImpact = 1;

    bool thrown = false;

    try {
        ConstructionEffects::preview(
            galaxy,
            galaxy.getPlanet(1),
            facility
        );
    } catch (const std::overflow_error&) {
        thrown = true;
    }

    assert(thrown);

    assert(
        galaxy.getPlanet(1).population ==
        std::numeric_limits<long long>::max()
    );

    assert(galaxy.getPlanet(1).production.at("food") == 0);

    std::cout
        << "[PASS] Population overflow cannot corrupt the planet\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS CONSTRUCTION TESTS ===\n\n";

    testEffectsActivateAtCompletion();
    testPopulationEnergyAndLevels();
    testChronologicalCompletion();
    testActualCostIsChecked();
    testFacilityCannotBeBuiltTwice();
    testCombinedEffectsAreValidatedBeforeSpending();
    testChangedStateCanFailCompletionSafely();
    testConstructionAndFlightsShareClock();
    testInstantConstruction();
    testSkippedConstructionClockIsDetected();
    testPopulationOverflowIsRejected();

    std::cout
        << "\nAll construction tests passed!\n";

    return 0;
}