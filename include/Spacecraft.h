#pragma once

#include "Galaxy.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace chronos {

struct Spacecraft {
    int id = 0;
    std::string name;

    int currentLocation = 0;

    double cargoCapacity = 0.0;
    double fuelCapacity = 0.0;
    double currentFuel = 0.0;

    double speed = 1.0;
    double reliability = 1.0;
    double availabilityTime = 0.0;
    double operatingCost = 0.0;

    bool operational = true;

    std::vector<std::string> capabilities{"cargo"};

    void validate(const Galaxy& galaxy) const {
        auto nonNegativeFinite = [](double value) {
            return std::isfinite(value) && value >= 0.0;
        };

        if (id < 0 || name.empty()) {
            throw std::invalid_argument(
                "Spacecraft requires a non-negative ID and a name"
            );
        }

        if (!galaxy.hasPlanet(currentLocation)) {
            throw std::invalid_argument(
                "Unknown spacecraft location"
            );
        }

        if (!nonNegativeFinite(cargoCapacity) ||
            !nonNegativeFinite(fuelCapacity) ||
            !nonNegativeFinite(currentFuel) ||
            !nonNegativeFinite(availabilityTime) ||
            !nonNegativeFinite(operatingCost)) {
            throw std::invalid_argument(
                "Spacecraft values must be finite and non-negative"
            );
        }

        if (currentFuel > fuelCapacity) {
            throw std::invalid_argument(
                "Current fuel exceeds fuel capacity"
            );
        }

        if (!std::isfinite(speed) || speed <= 0.0) {
            throw std::invalid_argument(
                "Spacecraft speed must be finite and positive"
            );
        }

        if (!std::isfinite(reliability) ||
            reliability < 0.0 ||
            reliability > 1.0) {
            throw std::invalid_argument(
                "Spacecraft reliability must be between 0 and 1"
            );
        }

        for (const auto& capability : capabilities) {
            if (capability.empty()) {
                throw std::invalid_argument(
                    "Spacecraft capability cannot be empty"
                );
            }
        }
    }
};

}