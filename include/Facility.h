#pragma once

#include "Galaxy.h"

#include <cmath>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace chronos {

enum class FacilityType {
    EnergyPlant,
    Farm,
    Hospital,
    Factory,
    DefenseSystem,
    ResearchStation,
    Spaceport,
    Mine
};

struct Facility {
    int id = 0;
    int planetId = 0;

    std::string name;
    FacilityType type = FacilityType::Farm;

    int constructionCost = 0;
    double constructionTime = 0.0;
    double strategicValue = 0.0;

    std::unordered_map<std::string, double> productionImpact;

    long long populationImpact = 0;
    double energyImpact = 0.0;

    void validate(const Galaxy& galaxy) const {
        if (id < 0 ||
            name.empty() ||
            !galaxy.hasPlanet(planetId)) {
            throw std::invalid_argument(
                "Invalid facility identity or planet"
            );
        }

        if (constructionCost <= 0 ||
            !std::isfinite(constructionTime) ||
            constructionTime < 0.0 ||
            !std::isfinite(strategicValue) ||
            strategicValue < 0.0 ||
            !std::isfinite(energyImpact)) {
            throw std::invalid_argument(
                "Invalid facility cost, time or value"
            );
        }

        for (const auto& [resource, impact] :
             productionImpact) {
            if (resource.empty() ||
                !std::isfinite(impact)) {
                throw std::invalid_argument(
                    "Invalid facility resource impact"
                );
            }
        }
    }
};

}