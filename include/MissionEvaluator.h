#pragma once

#include "Dijkstra.h"
#include "Mission.h"
#include "Spacecraft.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace chronos {

struct MissionEvaluation {
    bool feasible = false;
    std::string reason;

    double departureTime = 0.0;
    double arrivalTime = 0.0;
    double combinedReliability = 0.0;

    PathResult route;
};

class MissionEvaluator {
public:
    static MissionEvaluation evaluate(
        const Galaxy& galaxy,
        const Spacecraft& ship,
        const Mission& mission,
        double currentTime,
        const RouteWeights& weights = RouteWeights{},
        double minimumReliability = 0.0
    ) {
        ship.validate(galaxy);
        mission.validate(galaxy);

        if (!std::isfinite(currentTime) ||
            currentTime < 0.0) {
            throw std::invalid_argument(
                "Current time must be finite and non-negative"
            );
        }

        if (!std::isfinite(minimumReliability) ||
            minimumReliability < 0.0 ||
            minimumReliability > 1.0) {
            throw std::invalid_argument(
                "Minimum reliability must be between 0 and 1"
            );
        }

        MissionEvaluation result;

        if (mission.status != MissionStatus::Pending) {
            result.reason = "Mission is not pending";
            return result;
        }

        if (!ship.operational) {
            result.reason = "Spacecraft is not operational";
            return result;
        }

        if (ship.currentLocation != mission.origin) {
            result.reason =
                "Spacecraft is not at the mission origin";
            return result;
        }

        if (mission.cargoAmount > ship.cargoCapacity) {
            result.reason =
                "Insufficient spacecraft cargo capacity";
            return result;
        }

        for (const auto& required :
             mission.requiredCapabilities) {
            auto found = std::find(
                ship.capabilities.begin(),
                ship.capabilities.end(),
                required
            );

            if (found == ship.capabilities.end()) {
                result.reason =
                    "Missing required capability: " + required;
                return result;
            }
        }

        result.departureTime = std::max(
            currentTime,
            ship.availabilityTime
        );

        if (result.departureTime > mission.deadline) {
            result.reason =
                "Spacecraft becomes available after the deadline";
            return result;
        }

        // Route times are reference times.
        // Actual time for this spacecraft is time / speed.
        RouteWeights adjustedWeights = weights;
        adjustedWeights.time /= ship.speed;

        result.route = Dijkstra::findShortestPath(
            galaxy,
            mission.origin,
            mission.destination,
            adjustedWeights
        );

        if (!result.route.found) {
            result.reason = "No available route";
            return result;
        }

        result.route.totalTime /= ship.speed;

        result.arrivalTime =
            result.departureTime + result.route.totalTime;

        result.combinedReliability =
            ship.reliability *
            result.route.totalReliability;

        if (result.route.totalFuel > ship.currentFuel) {
            result.reason =
                "Insufficient fuel for the selected route";
            return result;
        }

        if (!std::isfinite(result.arrivalTime) ||
            result.arrivalTime > mission.deadline) {
            result.reason =
                "Selected route misses the mission deadline";
            return result;
        }

        // The current Galaxy model permits only one route
        // for each ordered pair of planets.
        for (std::size_t i = 1;
             i < result.route.path.size();
             ++i) {
            int from = result.route.path[i - 1];
            int to = result.route.path[i];

            const auto& neighbors =
                galaxy.getNeighbors(from);

            auto edge = std::find_if(
                neighbors.begin(),
                neighbors.end(),
                [to](const Route& route) {
                    return route.to == to;
                }
            );

            if (edge == neighbors.end()) {
                throw std::logic_error(
                    "Selected route contains a missing edge"
                );
            }

            if (mission.cargoAmount > edge->capacity) {
                result.reason =
                    "Insufficient capacity on the selected route";
                return result;
            }
        }

        if (result.combinedReliability <
            minimumReliability) {
            result.reason =
                "Selected route does not meet reliability threshold";
            return result;
        }

        result.feasible = true;
        result.reason = "Assignment is feasible";

        return result;
    }
};

}