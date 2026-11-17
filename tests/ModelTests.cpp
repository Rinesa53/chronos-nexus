#include "Galaxy.h"
#include "Spacecraft.h"
#include "Mission.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace chronos;

Galaxy createGalaxy() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});
    galaxy.addPlanet({2, "Mars"});

    return galaxy;
}

Spacecraft createSpacecraft() {
    Spacecraft ship;

    ship.id = 1;
    ship.name = "Explorer";
    ship.currentLocation = 1;

    ship.cargoCapacity = 100.0;
    ship.fuelCapacity = 200.0;
    ship.currentFuel = 150.0;

    ship.speed = 1.0;
    ship.reliability = 0.98;
    ship.availabilityTime = 0.0;
    ship.operatingCost = 5.0;

    ship.capabilities = {"cargo", "medical"};

    return ship;
}

Mission createMission() {
    Mission mission;

    mission.id = 1;
    mission.origin = 1;
    mission.destination = 2;

    mission.cargoType = "medicine";
    mission.cargoAmount = 30.0;

    mission.deadline = 20.0;
    mission.priority = 10;
    mission.importance = 5.0;

    mission.requiredCapabilities = {"cargo", "medical"};

    return mission;
}

template <typename Action>
void expectInvalidArgument(Action action) {
    bool thrown = false;

    try {
        action();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);
}

void testValidSpacecraft() {
    Galaxy galaxy = createGalaxy();
    Spacecraft ship = createSpacecraft();

    ship.validate(galaxy);

    assert(ship.operational);
    assert(ship.currentFuel <= ship.fuelCapacity);

    std::cout << "[PASS] Valid spacecraft\n";
}

void testInvalidSpacecraftFuel() {
    Galaxy galaxy = createGalaxy();
    Spacecraft ship = createSpacecraft();

    ship.currentFuel = -1.0;

    expectInvalidArgument([&]() {
        ship.validate(galaxy);
    });

    ship.currentFuel = ship.fuelCapacity + 1.0;

    expectInvalidArgument([&]() {
        ship.validate(galaxy);
    });

    std::cout << "[PASS] Invalid spacecraft fuel rejected\n";
}

void testInvalidSpacecraftProperties() {
    Galaxy galaxy = createGalaxy();

    Spacecraft ship = createSpacecraft();
    ship.speed = 0.0;

    expectInvalidArgument([&]() {
        ship.validate(galaxy);
    });

    ship = createSpacecraft();
    ship.reliability = 1.1;

    expectInvalidArgument([&]() {
        ship.validate(galaxy);
    });

    ship = createSpacecraft();
    ship.currentLocation = 99;

    expectInvalidArgument([&]() {
        ship.validate(galaxy);
    });

    ship = createSpacecraft();
    ship.cargoCapacity =
        std::numeric_limits<double>::quiet_NaN();

    expectInvalidArgument([&]() {
        ship.validate(galaxy);
    });

    std::cout << "[PASS] Invalid spacecraft properties rejected\n";
}

void testValidMission() {
    Galaxy galaxy = createGalaxy();
    Mission mission = createMission();

    mission.validate(galaxy);

    assert(mission.status == MissionStatus::Pending);
    assert(mission.requiredCapabilities.size() == 2);

    std::cout << "[PASS] Valid mission\n";
}

void testInvalidMissionCargo() {
    Galaxy galaxy = createGalaxy();
    Mission mission = createMission();

    mission.cargoAmount = 0.0;

    expectInvalidArgument([&]() {
        mission.validate(galaxy);
    });

    mission = createMission();
    mission.cargoAmount = -5.0;

    expectInvalidArgument([&]() {
        mission.validate(galaxy);
    });

    mission = createMission();
    mission.cargoType.clear();

    expectInvalidArgument([&]() {
        mission.validate(galaxy);
    });

    std::cout << "[PASS] Invalid mission cargo rejected\n";
}

void testInvalidMissionProperties() {
    Galaxy galaxy = createGalaxy();
    Mission mission = createMission();

    mission.destination = 99;

    expectInvalidArgument([&]() {
        mission.validate(galaxy);
    });

    mission = createMission();
    mission.deadline = -1.0;

    expectInvalidArgument([&]() {
        mission.validate(galaxy);
    });

    mission = createMission();
    mission.priority = -1;

    expectInvalidArgument([&]() {
        mission.validate(galaxy);
    });

    mission = createMission();
    mission.importance =
        std::numeric_limits<double>::infinity();

    expectInvalidArgument([&]() {
        mission.validate(galaxy);
    });

    std::cout << "[PASS] Invalid mission properties rejected\n";
}

int main() {
    std::cout << "=== CHRONOS NEXUS MODEL TESTS ===\n\n";

    testValidSpacecraft();
    testInvalidSpacecraftFuel();
    testInvalidSpacecraftProperties();

    testValidMission();
    testInvalidMissionCargo();
    testInvalidMissionProperties();

    std::cout << "\nAll model tests passed!\n";

    return 0;
}