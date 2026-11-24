#include "ResourceSimulator.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace chronos;

Planet simulatedPlanet(
    int id,
    double stock,
    double production,
    double consumption,
    double capacity
) {
    Planet planet{
        id,
        "P" + std::to_string(id)
    };

    planet.resources["food"] = stock;
    planet.production["food"] = production;
    planet.consumption["food"] = consumption;
    planet.storageCapacity["food"] = capacity;

    return planet;
}

template <typename Action>
void expectInvalidSimulation(Action action) {
    bool thrown = false;

    try {
        action();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);
}

void testProductionAndConsumption() {
    Galaxy galaxy;

    galaxy.addPlanet(
        simulatedPlanet(1, 100, 3, 7, 200)
    );

    ResourceSimulator simulator(galaxy);

    auto report = simulator.advanceTo(5);

    // 100 + (3 - 7) * 5 = 80.
    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        80.0
    );

    assert(report.fromTime == 0.0);
    assert(report.toTime == 5.0);

    assert(report.totals.produced == 15.0);
    assert(report.totals.requestedConsumption == 35.0);
    assert(report.totals.consumed == 35.0);
    assert(report.totals.shortage == 0.0);

    assert(simulator.currentTime() == 5.0);

    std::cout
        << "[PASS] Time changes production and consumption\n";
}

void testShortageAccumulates() {
    Galaxy galaxy;

    galaxy.addPlanet(
        simulatedPlanet(1, 10, 0, 7, 100)
    );

    ResourceSimulator simulator(galaxy);

    auto first = simulator.advanceTo(3);

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        0.0
    );

    assert(first.totals.consumed == 10.0);
    assert(first.totals.shortage == 11.0);

    auto second = simulator.advanceTo(5);

    assert(second.totals.shortage == 14.0);

    assert(simulator.totals().shortage == 25.0);
    assert(simulator.totals().consumed == 10.0);

    std::cout
        << "[PASS] Shortage accumulates without negative stock\n";
}

void testWasteAtStorageLimit() {
    Galaxy galaxy;

    galaxy.addPlanet(
        simulatedPlanet(1, 90, 10, 0, 100)
    );

    ResourceSimulator simulator(galaxy);

    auto report = simulator.advanceTo(3);

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        100.0
    );

    assert(report.totals.produced == 30.0);
    assert(report.totals.waste == 20.0);

    std::cout
        << "[PASS] Storage overflow is reported as waste\n";
}

void testMultiplePlanetsAndResources() {
    Galaxy galaxy;

    auto planet = simulatedPlanet(
        1, 100, 0, 10, 200
    );

    planet.resources["water"] = 20.0;
    planet.consumption["water"] = 2.0;
    planet.storageCapacity["water"] = 100.0;

    galaxy.addPlanet(planet);

    galaxy.addPlanet(
        simulatedPlanet(2, 30, 2, 0, 100)
    );

    ResourceSimulator simulator(galaxy);

    auto report = simulator.advanceTo(2);

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        80.0
    );

    assert(
        galaxy.getPlanet(1).resources.at("water") ==
        16.0
    );

    assert(
        galaxy.getPlanet(2).resources.at("food") ==
        34.0
    );

    assert(report.planetsUpdated == 2);
    assert(report.resourceEntries == 3);

    assert(
        report.byResource.at("food").consumed ==
        20.0
    );

    assert(
        report.byResource.at("water").consumed ==
        4.0
    );

    std::cout
        << "[PASS] Multiple planets and resource types evolve\n";
}

void testSplitTimeMatchesSingleAdvance() {
    Galaxy first;

    first.addPlanet(
        simulatedPlanet(1, 100, 3, 7, 200)
    );

    Galaxy second = first;

    ResourceSimulator oneStep(first);
    ResourceSimulator manySteps(second);

    oneStep.advanceTo(30);

    manySteps.advanceTo(3);
    manySteps.advanceTo(10);
    manySteps.advanceTo(30);

    assert(
        first.getPlanet(1).resources.at("food") ==
        second.getPlanet(1).resources.at("food")
    );

    assert(
        oneStep.totals().shortage ==
        manySteps.totals().shortage
    );

    assert(
        oneStep.totals().consumed ==
        manySteps.totals().consumed
    );

    std::cout
        << "[PASS] Split intervals match one interval for constant rates\n";
}

void testInvalidDataDoesNotPartiallyUpdateWorld() {
    Galaxy galaxy;

    galaxy.addPlanet(
        simulatedPlanet(1, 100, 0, 1, 200)
    );

    // Invalid: stock 50 exceeds capacity 10.
    galaxy.addPlanet(
        simulatedPlanet(2, 50, 0, 0, 10)
    );

    ResourceSimulator simulator(galaxy);

    expectInvalidSimulation([&]() {
        simulator.advanceTo(2);
    });

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        100.0
    );

    assert(
        galaxy.getPlanet(2).resources.at("food") ==
        50.0
    );

    assert(simulator.currentTime() == 0.0);
    assert(simulator.totals().consumed == 0.0);

    std::cout
        << "[PASS] Invalid input does not partially advance the world\n";
}

void testTimeValidationAndZeroInterval() {
    Galaxy galaxy;

    galaxy.addPlanet(
        simulatedPlanet(1, 100, 0, 1, 200)
    );

    ResourceSimulator simulator(galaxy);

    simulator.advanceTo(2);

    expectInvalidSimulation([&]() {
        simulator.advanceTo(1);
    });

    expectInvalidSimulation([&]() {
        simulator.advanceTo(
            std::numeric_limits<double>::quiet_NaN()
        );
    });

    auto report = simulator.advanceTo(2);

    assert(report.totals.consumed == 0.0);

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        98.0
    );

    assert(simulator.currentTime() == 2.0);

    std::cout
        << "[PASS] Backwards time is rejected and zero interval is safe\n";
}

void testRefillDoesNotPayPastShortage() {
    Galaxy galaxy;

    galaxy.addPlanet(
        simulatedPlanet(1, 10, 0, 7, 100)
    );

    ResourceSimulator simulator(galaxy);

    simulator.advanceTo(5);

    galaxy.replacePlanetResources(
        1,
        {{"food", 20.0}}
    );

    auto report = simulator.advanceTo(6);

    assert(
        galaxy.getPlanet(1).resources.at("food") ==
        13.0
    );

    assert(report.totals.shortage == 0.0);

    assert(simulator.totals().shortage == 25.0);

    std::cout
        << "[PASS] Past unmet demand is a statistic, not inventory debt\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS RESOURCE SIMULATION TESTS ===\n\n";

    testProductionAndConsumption();
    testShortageAccumulates();
    testWasteAtStorageLimit();
    testMultiplePlanetsAndResources();
    testSplitTimeMatchesSingleAdvance();
    testInvalidDataDoesNotPartiallyUpdateWorld();
    testTimeValidationAndZeroInterval();
    testRefillDoesNotPayPastShortage();

    std::cout
        << "\nAll resource simulation tests passed!\n";

    return 0;
}