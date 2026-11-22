#include "InfrastructureOptimizer.h"

#include <cassert>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

using namespace chronos;

Galaxy infrastructureGalaxy() {
    Galaxy galaxy;

    galaxy.addPlanet({1, "Earth"});

    return galaxy;
}

Facility makeFacility(
    int id,
    int cost,
    double value
) {
    Facility facility;

    facility.id = id;
    facility.planetId = 1;
    facility.name = "Facility " + std::to_string(id);

    facility.constructionCost = cost;
    facility.constructionTime = 2.0;
    facility.strategicValue = value;

    facility.productionImpact["food"] = 5.0;

    return facility;
}

void testOptimalCombination() {
    auto galaxy = infrastructureGalaxy();

    std::vector<Facility> facilities{
        makeFacility(1, 6, 12),
        makeFacility(2, 5, 9),
        makeFacility(3, 5, 9)
    };

    auto result = InfrastructureOptimizer::select(
        galaxy, facilities, 10
    );

    assert(result.exact);
    assert(result.method == "knapsack-dp");

    assert(result.totalCost == 10);
    assert(result.totalValue == 18.0);

    assert(
        (result.facilityIds == std::vector<int>{2, 3})
    );

    std::cout
        << "[PASS] Optimal construction combination\n";
}

void testAgainstExhaustiveSearch() {
    auto galaxy = infrastructureGalaxy();

    std::vector<Facility> facilities{
        makeFacility(1, 2, 3),
        makeFacility(2, 3, 4),
        makeFacility(3, 4, 9),
        makeFacility(4, 5, 10),
        makeFacility(5, 1, 1)
    };

    for (int budget = 0; budget <= 15; ++budget) {
        double best = 0.0;

        // Enumerate every subset of these five facilities.
        for (unsigned mask = 0;
             mask < (1u << facilities.size());
             ++mask) {
            int cost = 0;
            double value = 0.0;

            for (std::size_t i = 0;
                 i < facilities.size();
                 ++i) {
                if (mask & (1u << i)) {
                    cost += facilities[i].constructionCost;
                    value += facilities[i].strategicValue;
                }
            }

            if (cost <= budget) {
                best = std::max(best, value);
            }
        }

        auto result = InfrastructureOptimizer::select(
            galaxy, facilities, budget
        );

        assert(result.exact);
        assert(result.totalValue == best);
        assert(result.totalCost <= budget);

        std::set<int> unique(
            result.facilityIds.begin(),
            result.facilityIds.end()
        );

        assert(
            unique.size() == result.facilityIds.size()
        );
    }

    std::cout
        << "[PASS] DP matches exhaustive search for 16 budgets\n";
}

void testEmptySelection() {
    auto galaxy = infrastructureGalaxy();

    auto result = InfrastructureOptimizer::select(
        galaxy,
        {makeFacility(1, 20, 30)},
        5
    );

    assert(result.facilityIds.empty());
    assert(result.totalCost == 0);
    assert(result.totalValue == 0.0);

    std::cout
        << "[PASS] No construction exceeds available budget\n";
}

void testGreedyFallback() {
    auto galaxy = infrastructureGalaxy();

    std::vector<Facility> facilities{
        makeFacility(1, 6, 12),
        makeFacility(2, 5, 9),
        makeFacility(3, 5, 9)
    };

    // Force the table limit to activate the fallback.
    auto result = InfrastructureOptimizer::select(
        galaxy, facilities, 10, 1
    );

    assert(!result.exact);
    assert(result.method == "density-greedy");

    assert(result.totalCost == 6);
    assert(result.totalValue == 12.0);

    std::cout
        << "[PASS] Bounded table uses feasible greedy fallback\n";
}

void testInvalidFacilities() {
    auto galaxy = infrastructureGalaxy();

    auto facility = makeFacility(1, 0, 10);

    bool thrown = false;

    try {
        InfrastructureOptimizer::select(
            galaxy, {facility}, 10
        );
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    facility = makeFacility(1, 5, 10);
    thrown = false;

    try {
        InfrastructureOptimizer::select(
            galaxy, {facility, facility}, 10
        );
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    facility.planetId = 99;
    thrown = false;

    try {
        facility.validate(galaxy);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }

    assert(thrown);

    std::cout
        << "[PASS] Invalid cost, duplicate ID and unknown planet rejected\n";
}

int main() {
    std::cout
        << "=== CHRONOS NEXUS INFRASTRUCTURE TESTS ===\n\n";

    testOptimalCombination();
    testAgainstExhaustiveSearch();
    testEmptySelection();
    testGreedyFallback();
    testInvalidFacilities();

    std::cout << "\nAll infrastructure tests passed!\n";

    return 0;
}