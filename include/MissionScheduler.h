#pragma once

#include "MissionEvaluator.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace chronos {

struct SchedulerConfig {
    RouteWeights weights;
    RoutingOptions routing;
    double minimumReliability = 0.0;
    std::size_t maxCandidateChecks = 100000;
};

struct MissionAssignment {
    int missionId;
    int spacecraftId;
    double operatingCost;
    MissionEvaluation evaluation;
};

struct UnassignedMission {
    int missionId;
    std::string reason;
};

struct ScheduleResult {
    std::vector<MissionAssignment> assignments;
    std::vector<UnassignedMission> unassigned;
    std::size_t candidateChecks = 0;
};

class MissionScheduler {
    using EdgeKey = std::pair<int, int>;
    using Reservations = std::map<EdgeKey, double>;

    static bool hasCapacity(
        const Galaxy& galaxy,
        const PathResult& path,
        double amount,
        const Reservations& reserved
    ) {
        for (std::size_t i = 1; i < path.path.size(); ++i) {
            int from = path.path[i - 1];
            int to = path.path[i];

            const auto& neighbors = galaxy.getNeighbors(from);

            auto edge = std::find_if(
                neighbors.begin(),
                neighbors.end(),
                [to](const Route& route) {
                    return route.to == to;
                }
            );

            if (edge == neighbors.end()) {
                throw std::logic_error(
                    "Missing scheduled route"
                );
            }

            auto entry = reserved.find({from, to});

            double used =
                entry == reserved.end() ? 0.0 : entry->second;

            if (used + amount > edge->capacity) {
                return false;
            }
        }

        return true;
    }

public:
    static ScheduleResult plan(
        const Galaxy& galaxy,
        const std::vector<Spacecraft>& ships,
        const std::vector<Mission>& missions,
        double currentTime,
        const SchedulerConfig& config = SchedulerConfig{}
    ) {
        if (!std::isfinite(currentTime) ||
            currentTime < 0.0 ||
            !std::isfinite(config.minimumReliability) ||
            config.minimumReliability < 0.0 ||
            config.minimumReliability > 1.0 ||
            config.maxCandidateChecks == 0) {
            throw std::invalid_argument(
                "Invalid scheduler configuration"
            );
        }

        for (double weight : {
            config.weights.distance,
            config.weights.fuel,
            config.weights.time,
            config.weights.risk
        }) {
            if (!std::isfinite(weight) || weight < 0.0) {
                throw std::invalid_argument(
                    "Invalid route weight"
                );
            }
        }

        std::set<int> shipIds;

        for (const auto& ship : ships) {
            ship.validate(galaxy);

            if (!shipIds.insert(ship.id).second) {
                throw std::invalid_argument(
                    "Duplicate spacecraft ID"
                );
            }
        }

        std::set<int> missionIds;
        std::vector<const Mission*> pending;

        for (const auto& mission : missions) {
            mission.validate(galaxy);

            if (!missionIds.insert(mission.id).second) {
                throw std::invalid_argument(
                    "Duplicate mission ID"
                );
            }

            if (mission.status == MissionStatus::Pending) {
                pending.push_back(&mission);
            }
        }

        std::sort(
            pending.begin(),
            pending.end(),
            [](const Mission* a, const Mission* b) {
                if (a->priority != b->priority) {
                    return a->priority > b->priority;
                }

                if (a->importance != b->importance) {
                    return a->importance > b->importance;
                }

                if (a->deadline != b->deadline) {
                    return a->deadline < b->deadline;
                }

                return a->id < b->id;
            }
        );

        ScheduleResult result;
        std::set<int> usedShips;
        Reservations reserved;

if (config.routing.reservedCargo != nullptr) {
    reserved = *config.routing.reservedCargo;
}
        std::map<
            std::pair<int, std::string>,
            double
        > allocated;

        for (const Mission* mission : pending) {
            auto resourceKey = std::make_pair(
                mission->origin,
                mission->cargoType
            );

            const auto& inventory =
                galaxy.getPlanet(mission->origin).resources;

            auto stockEntry =
                inventory.find(mission->cargoType);

            double stock =
                stockEntry == inventory.end()
                    ? 0.0
                    : stockEntry->second;

            if (!std::isfinite(stock) || stock < 0.0) {
                throw std::invalid_argument(
                    "Invalid source inventory"
                );
            }

            if (mission->cargoAmount >
                stock - allocated[resourceKey]) {
                result.unassigned.push_back({
                    mission->id,
                    "Insufficient source inventory"
                });

                continue;
            }

            const Spacecraft* bestShip = nullptr;
            MissionEvaluation bestEvaluation;

            double bestCost =
                std::numeric_limits<double>::infinity();

            for (const auto& ship : ships) {
                if (usedShips.contains(ship.id)) {
                    continue;
                }

                if (result.candidateChecks >=
                    config.maxCandidateChecks) {
                    break;
                }

                ++result.candidateChecks;

                RoutingOptions routing = config.routing;
                routing.reservedCargo = &reserved;

                auto evaluation = MissionEvaluator::evaluate(
                    galaxy,
                    ship,
                    *mission,
                    currentTime,
                    config.weights,
                    config.minimumReliability,
                    routing
                );

                if (!evaluation.feasible ||
                    !hasCapacity(
                        galaxy,
                        evaluation.route,
                        mission->cargoAmount,
                        reserved
                    )) {
                    continue;
                }

                double cost =
                    ship.operatingCost *
                    evaluation.route.totalTime;

                if (!std::isfinite(cost)) {
                    continue;
                }

                if (bestShip == nullptr ||
                    cost < bestCost ||
                    (cost == bestCost &&
                     ship.id < bestShip->id)) {
                    bestShip = &ship;
                    bestCost = cost;
                    bestEvaluation = std::move(evaluation);
                }
            }

            if (bestShip == nullptr) {
                result.unassigned.push_back({
                    mission->id,
                    result.candidateChecks >=
                        config.maxCandidateChecks
                        ? "Candidate budget exhausted"
                        : "No feasible candidate found by current routing"
                });

                continue;
            }

            usedShips.insert(bestShip->id);

            allocated[resourceKey] += mission->cargoAmount;

            const auto& path = bestEvaluation.route.path;

            for (std::size_t i = 1; i < path.size(); ++i) {
                reserved[{path[i - 1], path[i]}] +=
                    mission->cargoAmount;
            }

            result.assignments.push_back({
                mission->id,
                bestShip->id,
                bestCost,
                std::move(bestEvaluation)
            });
        }

        return result;
    }
};

}