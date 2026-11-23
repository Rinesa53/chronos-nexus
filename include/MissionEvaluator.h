#pragma once

#include "ConstrainedRouter.h"
#include "Mission.h"
#include "Spacecraft.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>

namespace chronos {

enum class RoutingMode {
    Constrained,
    WeightedDijkstra
};

struct RoutingOptions {
    RoutingMode mode = RoutingMode::Constrained;

    std::size_t maxStates = 10000;
    std::size_t maxLabelsPerNode = 16;
    std::size_t maxLabelsCreated = 100000;
};

struct MissionEvaluation {
    bool feasible = false;
    std::string reason;

    double departureTime = 0.0;
    double arrivalTime = 0.0;
    double combinedReliability = 0.0;

    PathResult route;

    bool routeOptimal = false;
    bool routingBudgetExhausted = false;
    bool routingLabelsPruned = false;

    std::size_t routingStatesExplored = 0;
    std::size_t routingLabelsCreated = 0;
    std::size_t routingCandidatesPruned = 0;
};

class MissionEvaluator {
public:
    static MissionEvaluation evaluate(
        const Galaxy& galaxy,
        const Spacecraft& ship,
        const Mission& mission,
        double currentTime,
        const RouteWeights& weights = RouteWeights{},
        double minimumReliability = 0.0,
        const RoutingOptions& routing = RoutingOptions{}
    ) {
        ship.validate(galaxy);
        mission.validate(galaxy);

        if (!std::isfinite(currentTime) ||
            currentTime < 0.0 ||
            !std::isfinite(minimumReliability) ||
            minimumReliability < 0.0 ||
            minimumReliability > 1.0 ||
            routing.maxStates == 0 ||
            routing.maxLabelsPerNode == 0 ||
            routing.maxLabelsCreated == 0) {
            throw std::invalid_argument(
                "Invalid mission evaluation parameters"
            );
        }

        for (double weight : {
            weights.distance,
            weights.fuel,
            weights.time,
            weights.risk
        }) {
            if (!std::isfinite(weight) || weight < 0.0) {
                throw std::invalid_argument(
                    "Invalid mission route weights"
                );
            }
        }

        MissionEvaluation result;

        if (mission.status != MissionStatus::Pending) {
            result.reason = "Mission is not pending";
            return result;
        }

        if (!ship.operational) {
            result.reason =
                "Spacecraft is not operational";

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
                    "Missing required capability: " +
                    required;

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

        if (routing.mode == RoutingMode::Constrained) {
            ConstrainedRouteRequest request;

            request.origin = mission.origin;
            request.destination = mission.destination;
            request.cargoAmount = mission.cargoAmount;

            request.timeLimit =
                mission.deadline - result.departureTime;

            request.minimumReliability =
                minimumReliability;

            request.weights = weights;

            request.maxStates = routing.maxStates;

            request.maxLabelsPerNode =
                routing.maxLabelsPerNode;

            request.maxLabelsCreated =
                routing.maxLabelsCreated;

            auto search = ConstrainedRouter::find(
                galaxy,
                ship,
                request
            );

            result.route = search.path;
            result.routeOptimal = search.optimal;

            result.routingBudgetExhausted =
                search.budgetExhausted;

            result.routingLabelsPruned =
                search.labelsPruned;

            result.routingStatesExplored =
                search.statesExplored;

            result.routingLabelsCreated =
                search.labelsCreated;

            result.routingCandidatesPruned =
                search.candidatesPruned;
        } else {
            RouteWeights adjustedWeights = weights;
            adjustedWeights.time /= ship.speed;

            result.route = Dijkstra::findShortestPath(
                galaxy,
                mission.origin,
                mission.destination,
                adjustedWeights
            );

            result.route.totalTime /= ship.speed;

            result.routingStatesExplored =
                result.route.exploredNodes;
        }

        if (!result.route.found) {
            result.reason =
                result.routingBudgetExhausted ||
                result.routingLabelsPruned
                    ? "No feasible route found within search limits"
                    : "No feasible route";

            return result;
        }

        result.arrivalTime =
            result.departureTime +
            result.route.totalTime;

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

        // Keep the original MissionEvaluation convention:
        // route.totalReliability includes edges only.
        // combinedReliability includes the spacecraft too.
        double routeReliability = 1.0;

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
                [to](const Route& e) {
                    return e.to == to;
                }
            );

            if (edge == neighbors.end()) {
                throw std::logic_error(
                    "Selected route contains a missing edge"
                );
            }

            if (!edge->available ||
                mission.cargoAmount > edge->capacity) {
                result.reason =
                    "Selected route is unavailable or lacks capacity";

                return result;
            }

            routeReliability *= edge->reliability;
        }

        result.route.totalReliability =
            routeReliability;

        result.combinedReliability =
            ship.reliability * routeReliability;

        if (result.combinedReliability <
            minimumReliability) {
            result.reason =
                "Selected route does not meet reliability threshold";

            return result;
        }

        // If the unconstrained cheapest route satisfies
        // every checked constraint, it is also cheapest
        // among the feasible routes for this query.
        if (routing.mode == RoutingMode::WeightedDijkstra) {
            result.routeOptimal = true;
        }

        result.feasible = true;
        result.reason = "Assignment is feasible";

        return result;
    }
};

}