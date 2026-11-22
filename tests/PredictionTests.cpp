#include "ResourcePredictor.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace chronos;

Planet predictionPlanet() {
    Planet planet{1, "Earth"};

    planet.resources["food"] = 10000.0;
    planet.production["food"] = 300.0;
    planet.consumption["food"] = 700.0;
    planet.storageCapacity["food"] = 20000.0;

    return planet;
}

void testShortageTime() {
    auto planet = predictionPlanet();

    auto result = ResourcePredictor::predict(
        planet, "food", 30.0
    );

    assert(result.daysUntilEmpty.has_value());

    assert(
        std::abs(*result.daysUntilEmpty - 25.0) < 1e-9
    );

    assert(result.netChangePerDay == -400.0);
    assert(result.stockAtHorizon == 0.0);
    assert(result.unmetDemand == 2000.0);
    assert(result.requiredDelivery == 2000.0);

    assert(result.shortageExpected);
    assert(result.needsDelivery);

    // Prediction must not modify the current inventory.
    assert(planet.resources.at("food") == 10000.0);

    std::cout
        << "[PASS] Shortage time and unmet demand\n";
}

void testSafetyStockPrediction() {
    auto planet = predictionPlanet();

    auto result = ResourcePredictor::predict(
        planet, "food", 24.0, 1000.0
    );

    // After 24 days:
    // 10000 - 400 * 24 = 400.
    // To retain safety stock 1000, delivery needed = 600.

    assert(!result.shortageExpected);
    assert(result.stockAtHorizon == 400.0);
    assert(result.requiredDelivery == 600.0);
    assert(result.needsDelivery);

    assert(result.daysUntilSafetyStock.has_value());

    assert(
        std::abs(
            *result.daysUntilSafetyStock - 22.5
        ) < 1e-9
    );

    std::cout
        << "[PASS] Preventive safety stock prediction\n";
}

void testStableAndGrowingResources() {
    auto planet = predictionPlanet();

    planet.production["food"] = 700.0;

    auto result = ResourcePredictor::predict(
        planet, "food", 30.0
    );

    assert(result.stockAtHorizon == 10000.0);
    assert(!result.daysUntilEmpty.has_value());
    assert(!result.needsDelivery);

    planet.production["food"] = 800.0;

    result = ResourcePredictor::predict(
        planet, "food", 30.0
    );

    assert(result.stockAtHorizon == 13000.0);
    assert(!result.shortageExpected);
    assert(!result.daysUntilEmpty.has_value());

    std::cout
        << "[PASS] Stable and growing resources\n";
}

void testStorageOverflow() {
    auto planet = predictionPlanet();

    planet.production["food"] = 1200.0;

    auto result = ResourcePredictor::predict(
        planet, "food", 30.0
    );

    // 10000 + (1200 - 700) * 30 = 25000.
    // Storage capacity is 20000, so 5000 cannot be stored.

    assert(result.stockAtHorizon == 20000.0);
    assert(result.overflowWaste == 5000.0);
    assert(!result.needsDelivery);

    std::cout
        << "[PASS] Storage capacity and overflow\n";
}

void testEmptyStockAndZeroHorizon() {
    auto planet = predictionPlanet();

    auto result = ResourcePredictor::predict(
        planet, "food", 0.0
    );

    assert(result.stockAtHorizon == 10000.0);
    assert(result.unmetDemand == 0.0);

    planet.resources["food"] = 0.0;

    result = ResourcePredictor::predict(
        planet, "food", 1.0
    );

    assert(result.daysUntilEmpty.has_value());
    assert(*result.daysUntilEmpty == 0.0);
    assert(result.unmetDemand == 400.0);

    std::cout
        << "[PASS] Empty stock and zero horizon\n";
}

template <typename Action>
void expectInvalidPrediction(Action action) {
    bool thrown = false;

    try {
        action();
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);
}

void testInvalidPredictionData() {
    auto planet = predictionPlanet();

    expectInvalidPrediction([&]() {
        ResourcePredictor::predict(
            planet, "food", -1.0
        );
    });

    expectInvalidPrediction([&]() {
        ResourcePredictor::predict(
            planet, "food", 10.0, 25000.0
        );
    });

    planet.production["food"] =
        std::numeric_limits<double>::quiet_NaN();

    expectInvalidPrediction([&]() {
        ResourcePredictor::predict(
            planet, "food", 10.0
        );
    });

    planet = predictionPlanet();
    planet.resources["food"] = 30000.0;

    expectInvalidPrediction([&]() {
        ResourcePredictor::predict(
            planet, "food", 10.0
        );
    });

    std::cout
        << "[PASS] Invalid prediction data rejected\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS PREDICTION TESTS ===\n\n";

    testShortageTime();
    testSafetyStockPrediction();
    testStableAndGrowingResources();
    testStorageOverflow();
    testEmptyStockAndZeroHorizon();
    testInvalidPredictionData();

    std::cout << "\nAll prediction tests passed!\n";

    return 0;
}