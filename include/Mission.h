#pragma once

#include "Galaxy.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace chronos {

enum class MissionStatus {
    Pending,
    Assigned,
    InTransit,
    Completed,
    Failed,
    Cancelled
};

struct Mission {
    int id = 0;

    int origin = 0;
    int destination = 0;

    std::string cargoType;
    double cargoAmount = 0.0;

    double deadline = 0.0;
    int priority = 1;
    double importance = 1.0;

    std::vector<std::string> requiredCapabilities{"cargo"};

    MissionStatus status = MissionStatus::Pending;

    void validate(const Galaxy& galaxy) const {
        if (id < 0) {
            throw std::invalid_argument(
                "Mission ID must be non-negative"
            );
        }

        if (!galaxy.hasPlanet(origin) ||
            !galaxy.hasPlanet(destination)) {
            throw std::invalid_argument(
                "Unknown mission origin or destination"
            );
        }

        if (cargoType.empty()) {
            throw std::invalid_argument(
                "Mission cargo type cannot be empty"
            );
        }

        if (!std::isfinite(cargoAmount) ||
            cargoAmount <= 0.0) {
            throw std::invalid_argument(
                "Mission cargo amount must be finite and positive"
            );
        }

        if (!std::isfinite(deadline) || deadline < 0.0) {
            throw std::invalid_argument(
                "Mission deadline must be finite and non-negative"
            );
        }

        if (priority < 0) {
            throw std::invalid_argument(
                "Mission priority must be non-negative"
            );
        }

        if (!std::isfinite(importance) ||
            importance < 0.0) {
            throw std::invalid_argument(
                "Mission importance must be finite and non-negative"
            );
        }

        for (const auto& capability : requiredCapabilities) {
            if (capability.empty()) {
                throw std::invalid_argument(
                    "Required capability cannot be empty"
                );
            }
        }
    }
};

}