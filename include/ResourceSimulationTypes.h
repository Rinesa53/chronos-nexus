#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace chronos {

struct ResourceTotals {
    double produced = 0.0;
    double requestedConsumption = 0.0;
    double consumed = 0.0;
    double shortage = 0.0;
    double waste = 0.0;
};

struct ResourceStepResult {
    double fromTime = 0.0;
    double toTime = 0.0;

    std::size_t planetsUpdated = 0;
    std::size_t resourceEntries = 0;

    ResourceTotals totals;

    std::unordered_map<
        std::string,
        ResourceTotals
    > byResource;
};

}