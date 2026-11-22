#pragma once

#include "Facility.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace chronos {

struct InfrastructurePlan {
    std::vector<int> facilityIds;

    int totalCost = 0;
    double totalValue = 0.0;

    bool exact = true;
    std::string method = "knapsack-dp";

    std::size_t candidatesEvaluated = 0;
};

class InfrastructureOptimizer {
    static InfrastructurePlan greedy(
        std::vector<const Facility*> facilities,
        int budget
    ) {
        InfrastructurePlan result;

        result.exact = false;
        result.method = "density-greedy";

        std::sort(
            facilities.begin(),
            facilities.end(),
            [](const Facility* a, const Facility* b) {
                double left =
                    a->strategicValue / a->constructionCost;

                double right =
                    b->strategicValue / b->constructionCost;

                return left == right
                    ? a->id < b->id
                    : left > right;
            }
        );

        for (const Facility* facility : facilities) {
            ++result.candidatesEvaluated;

            if (facility->constructionCost <=
                budget - result.totalCost) {
                result.facilityIds.push_back(facility->id);

                result.totalCost +=
                    facility->constructionCost;

                result.totalValue +=
                    facility->strategicValue;

                if (!std::isfinite(result.totalValue)) {
                    throw std::overflow_error(
                        "Infrastructure value overflow"
                    );
                }
            }
        }

        return result;
    }

public:
    static InfrastructurePlan select(
        const Galaxy& galaxy,
        const std::vector<Facility>& facilities,
        int budget,
        std::size_t maxTableCells = 2000000
    ) {
        if (budget < 0 || maxTableCells == 0) {
            throw std::invalid_argument(
                "Invalid infrastructure budget"
            );
        }

        std::set<int> ids;
        std::vector<const Facility*> eligible;

        for (const auto& facility : facilities) {
            facility.validate(galaxy);

            if (!ids.insert(facility.id).second) {
                throw std::invalid_argument(
                    "Duplicate facility ID"
                );
            }

            if (facility.constructionCost <= budget &&
                facility.strategicValue > 0.0) {
                eligible.push_back(&facility);
            }
        }

        InfrastructurePlan result;

        if (eligible.empty()) {
            return result;
        }

        std::size_t width =
            static_cast<std::size_t>(budget) + 1;

        // Division avoids overflowing the product:
        // eligible.size() * width.
        if (width > maxTableCells / eligible.size()) {
            return greedy(eligible, budget);
        }

        std::vector<double> dp(width, 0.0);

        std::vector<std::vector<unsigned char>> take(
            eligible.size(),
            std::vector<unsigned char>(width, 0)
        );

        for (std::size_t i = 0;
             i < eligible.size();
             ++i) {
            const Facility& facility = *eligible[i];

            // Descending order prevents selecting
            // the same facility more than once.
            for (int b = budget;
                 b >= facility.constructionCost;
                 --b) {
                ++result.candidatesEvaluated;

                double candidate =
                    dp[b - facility.constructionCost] +
                    facility.strategicValue;

                if (!std::isfinite(candidate)) {
                    throw std::overflow_error(
                        "Infrastructure value overflow"
                    );
                }

                if (candidate > dp[b]) {
                    dp[b] = candidate;
                    take[i][b] = 1;
                }
            }
        }

        result.totalValue = dp[budget];

        int remaining = budget;

        for (std::size_t i = eligible.size();
             i > 0;
             --i) {
            const Facility& facility = *eligible[i - 1];

            if (take[i - 1][remaining]) {
                result.facilityIds.push_back(facility.id);

                result.totalCost +=
                    facility.constructionCost;

                remaining -= facility.constructionCost;
            }
        }

        std::reverse(
            result.facilityIds.begin(),
            result.facilityIds.end()
        );

        return result;
    }
};

}