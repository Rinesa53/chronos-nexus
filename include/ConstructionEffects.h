#pragma once

#include "Facility.h"

#include <limits>
#include <stdexcept>

namespace chronos {

class ConstructionEffects {
    static long long addPopulation(
        long long value,
        long long delta
    ) {
        if ((delta > 0 &&
             value >
                std::numeric_limits<long long>::max() -
                delta) ||
            (delta < 0 &&
             value <
                std::numeric_limits<long long>::min() -
                delta)) {
            throw std::overflow_error(
                "Population impact overflow"
            );
        }

        return value + delta;
    }

    static int addLevel(int value, int delta) {
        long long next =
            static_cast<long long>(value) + delta;

        if (next < 0 ||
            next > std::numeric_limits<int>::max()) {
            throw std::invalid_argument(
                "Invalid technology or defense impact"
            );
        }

        return static_cast<int>(next);
    }

public:
    static Planet preview(
        const Galaxy& galaxy,
        const Planet& current,
        const Facility& facility
    ) {
        facility.validate(galaxy);
        Galaxy::validatePlanetState(current);

        if (current.id != facility.planetId) {
            throw std::invalid_argument(
                "Facility belongs to a different planet"
            );
        }

        Planet result = current;

        for (const auto& [resource, impact] :
             facility.productionImpact) {
            result.production[resource] += impact;
        }

        if (facility.energyImpact >= 0.0) {
            result.production["energy"] +=
                facility.energyImpact;
        } else {
            result.consumption["energy"] +=
                -facility.energyImpact;
        }

        result.population = addPopulation(
            result.population,
            facility.populationImpact
        );

        result.technologyLevel = addLevel(
            result.technologyLevel,
            facility.technologyImpact
        );

        result.defenseLevel = addLevel(
            result.defenseLevel,
            facility.defenseImpact
        );

        Galaxy::validatePlanetState(result);

        return result;
    }
};

}