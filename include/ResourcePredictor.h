#pragma once

#include "Planet.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace chronos {

struct ResourceForecast {
    int planetId = 0;
    std::string resource;

    double currentStock = 0.0;
    double netChangePerDay = 0.0;

    double stockAtHorizon = 0.0;
    double unmetDemand = 0.0;
    double overflowWaste = 0.0;
    double requiredDelivery = 0.0;

    std::optional<double> daysUntilEmpty;
    std::optional<double> daysUntilSafetyStock;

    bool shortageExpected = false;
    bool needsDelivery = false;
};

class ResourcePredictor {
    static double readValue(
        const std::unordered_map<std::string, double>& values,
        const std::string& resource
    ) {
        auto it = values.find(resource);

        double value =
            it == values.end() ? 0.0 : it->second;

        if (!std::isfinite(value) || value < 0.0) {
            throw std::invalid_argument(
                "Resource values must be finite and non-negative"
            );
        }

        return value;
    }

public:
    static ResourceForecast predict(
        const Planet& planet,
        const std::string& resource,
        double horizonDays,
        double safetyStock = 0.0
    ) {
        if (resource.empty() ||
            !std::isfinite(horizonDays) ||
            horizonDays < 0.0 ||
            !std::isfinite(safetyStock) ||
            safetyStock < 0.0) {
            throw std::invalid_argument(
                "Invalid prediction parameters"
            );
        }

        double stock = readValue(
            planet.resources, resource
        );

        double production = readValue(
            planet.production, resource
        );

        double consumption = readValue(
            planet.consumption, resource
        );

        double capacity = readValue(
            planet.storageCapacity, resource
        );

        if (stock > capacity || safetyStock > capacity) {
            throw std::invalid_argument(
                "Stock or safety stock exceeds storage capacity"
            );
        }

        ResourceForecast result;

        result.planetId = planet.id;
        result.resource = resource;
        result.currentStock = stock;
        result.netChangePerDay = production - consumption;

        double raw =
            stock + result.netChangePerDay * horizonDays;

        if (!std::isfinite(raw)) {
            throw std::overflow_error(
                "Resource forecast is too large"
            );
        }

        result.stockAtHorizon = std::clamp(
            raw, 0.0, capacity
        );

        result.unmetDemand = std::max(0.0, -raw);

        result.overflowWaste = std::max(
            0.0, raw - capacity
        );

        result.requiredDelivery = std::max(
            0.0, safetyStock - raw
        );

        if (!std::isfinite(result.requiredDelivery)) {
            throw std::overflow_error(
                "Required delivery is too large"
            );
        }

        result.shortageExpected =
            result.unmetDemand > 0.0;

        result.needsDelivery =
            result.requiredDelivery > 0.0;

        if (result.netChangePerDay < 0.0) {
            double deficit = -result.netChangePerDay;

            double emptyTime = stock / deficit;

            double safetyTime =
                std::max(0.0, stock - safetyStock) / deficit;

            if (std::isfinite(emptyTime)) {
                result.daysUntilEmpty = emptyTime;
            }

            if (std::isfinite(safetyTime)) {
                result.daysUntilSafetyStock = safetyTime;
            }
        }

        return result;
    }
};

}