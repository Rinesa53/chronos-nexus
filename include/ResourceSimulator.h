#pragma once

#include "Galaxy.h"
#include "ResourcePredictor.h"
#include "ResourceSimulationTypes.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace chronos {

class ResourceSimulator {
    struct PlanetUpdate {
        int id;

        std::unordered_map<
            std::string,
            double
        > resources;
    };

    Galaxy& galaxy;
    double time;

    ResourceTotals cumulative;

    std::unordered_map<
        std::string,
        ResourceTotals
    > cumulativeByResource;

    static double valueOrZero(
        const std::unordered_map<std::string, double>& values,
        const std::string& resource
    ) {
        auto entry = values.find(resource);

        return entry == values.end()
            ? 0.0
            : entry->second;
    }

    static void addTo(
        ResourceTotals& target,
        const ResourceTotals& amount
    ) {
        ResourceTotals next;

        next.produced =
            target.produced + amount.produced;

        next.requestedConsumption =
            target.requestedConsumption +
            amount.requestedConsumption;

        next.consumed =
            target.consumed + amount.consumed;

        next.shortage =
            target.shortage + amount.shortage;

        next.waste =
            target.waste + amount.waste;

        for (double value : {
            next.produced,
            next.requestedConsumption,
            next.consumed,
            next.shortage,
            next.waste
        }) {
            if (!std::isfinite(value)) {
                throw std::overflow_error(
                    "Resource statistics overflow"
                );
            }
        }

        target = next;
    }

public:
    explicit ResourceSimulator(
        Galaxy& world,
        double initialTime = 0.0
    )
        : galaxy(world),
          time(initialTime) {
        if (!std::isfinite(initialTime) ||
            initialTime < 0.0) {
            throw std::invalid_argument(
                "Invalid initial simulation time"
            );
        }
    }

    ResourceSimulator(
        const ResourceSimulator&
    ) = delete;

    ResourceSimulator& operator=(
        const ResourceSimulator&
    ) = delete;

    double currentTime() const {
        return time;
    }

    const ResourceTotals& totals() const {
        return cumulative;
    }

    const std::unordered_map<
        std::string,
        ResourceTotals
    >& totalsByResource() const {
        return cumulativeByResource;
    }

    ResourceStepResult advanceTo(double targetTime) {
        if (!std::isfinite(targetTime) ||
            targetTime < time) {
            throw std::invalid_argument(
                "Simulation time must be finite and cannot go backwards"
            );
        }

        double dt = targetTime - time;

        ResourceStepResult report;

        report.fromTime = time;
        report.toTime = targetTime;

        std::vector<PlanetUpdate> updates;

        // Calculate all updates before changing the world.
        for (const auto& [id, planet] :
             galaxy.getPlanets()) {
            std::set<std::string> names;

            auto collect = [&](const auto& values) {
                for (const auto& entry : values) {
                    names.insert(entry.first);
                }
            };

            collect(planet.resources);
            collect(planet.production);
            collect(planet.consumption);
            collect(planet.storageCapacity);

            PlanetUpdate update{
                id,
                planet.resources
            };

            for (const auto& resource : names) {
                auto forecast = ResourcePredictor::predict(
                    planet,
                    resource,
                    dt
                );

                ResourceTotals amount;

                amount.produced =
                    valueOrZero(
                        planet.production,
                        resource
                    ) * dt;

                amount.requestedConsumption =
                    valueOrZero(
                        planet.consumption,
                        resource
                    ) * dt;

                amount.shortage =
                    forecast.unmetDemand;

                amount.waste =
                    forecast.overflowWaste;

                amount.consumed = std::max(
                    0.0,
                    amount.requestedConsumption -
                    amount.shortage
                );

                if (!std::isfinite(amount.produced) ||
                    !std::isfinite(
                        amount.requestedConsumption
                    )) {
                    throw std::overflow_error(
                        "Resource production or consumption overflow"
                    );
                }

                update.resources[resource] =
                    forecast.stockAtHorizon;

                addTo(report.totals, amount);

                addTo(
                    report.byResource[resource],
                    amount
                );

                ++report.resourceEntries;
            }

            updates.push_back(std::move(update));
        }

        report.planetsUpdated = updates.size();

        ResourceTotals nextCumulative = cumulative;

        addTo(
            nextCumulative,
            report.totals
        );

        auto nextByResource = cumulativeByResource;

        for (const auto& [resource, amount] :
             report.byResource) {
            addTo(
                nextByResource[resource],
                amount
            );
        }

        // Apply only after validation has succeeded.
        for (auto& update : updates) {
            galaxy.replacePlanetResources(
                update.id,
                std::move(update.resources)
            );
        }

        cumulative = nextCumulative;
        cumulativeByResource.swap(nextByResource);

        time = targetTime;

        return report;
    }
};

}