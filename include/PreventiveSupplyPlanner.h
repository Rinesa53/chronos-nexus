#pragma once

#include "MissionEvaluator.h"
#include "ResourcePredictor.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace chronos {

struct SupplyRequest {
    int missionId = 0;
    int source = 0;
    int destination = 0;

    std::string resource;

    double currentTime = 0.0;
    double horizonDays = 10.0;
    double safetyStock = 0.0;
    double minimumReliability = 0.0;

    RouteWeights weights;
    std::size_t maxCandidateChecks = 1000;
};

struct SupplyProposal {
    std::optional<Mission> mission;
    std::string reason;
    std::size_t candidateChecks = 0;
};

class PreventiveSupplyPlanner {
public:
    static SupplyProposal propose(
        const Galaxy& galaxy,
        const std::vector<Spacecraft>& ships,
        const std::vector<Mission>& existingMissions,
        const SupplyRequest& request
    ) {
        if (request.missionId < 0 ||
            request.source == request.destination ||
            !galaxy.hasPlanet(request.source) ||
            !galaxy.hasPlanet(request.destination) ||
            !std::isfinite(request.currentTime) ||
            request.currentTime < 0.0 ||
            !std::isfinite(request.horizonDays) ||
            request.horizonDays <= 0.0 ||
            !std::isfinite(request.minimumReliability) ||
            request.minimumReliability < 0.0 ||
            request.minimumReliability > 1.0 ||
            request.maxCandidateChecks == 0) {
            throw std::invalid_argument(
                "Invalid preventive supply request"
            );
        }

        for (double weight : {
            request.weights.distance,
            request.weights.fuel,
            request.weights.time,
            request.weights.risk
        }) {
            if (!std::isfinite(weight) || weight < 0.0) {
                throw std::invalid_argument(
                    "Invalid supply route weights"
                );
            }
        }

        const auto& source =
            galaxy.getPlanet(request.source);

        const auto& destination =
            galaxy.getPlanet(request.destination);

        auto donor = ResourcePredictor::predict(
            source,
            request.resource,
            request.horizonDays,
            request.safetyStock
        );

        auto receiver = ResourcePredictor::predict(
            destination,
            request.resource,
            request.horizonDays,
            request.safetyStock
        );

        double timeWindow = request.horizonDays;

        if (receiver.daysUntilSafetyStock &&
            *receiver.daysUntilSafetyStock > 0.0) {
            timeWindow = std::min(
                timeWindow,
                *receiver.daysUntilSafetyStock
            );
        } else if (receiver.daysUntilEmpty &&
                   *receiver.daysUntilEmpty > 0.0) {
            timeWindow = std::min(
                timeWindow,
                *receiver.daysUntilEmpty
            );
        }

        double deadline =
            request.currentTime + timeWindow;

        if (!std::isfinite(deadline)) {
            throw std::overflow_error(
                "Supply deadline overflow"
            );
        }

        double incoming = 0.0;
        double outgoing = 0.0;

        std::set<int> missionIds;

        for (const auto& mission : existingMissions) {
            mission.validate(galaxy);

            if (!missionIds.insert(mission.id).second ||
                mission.id == request.missionId) {
                throw std::invalid_argument(
                    "Duplicate supply mission ID"
                );
            }

            bool waiting =
                mission.status == MissionStatus::Pending ||
                mission.status == MissionStatus::Assigned;

            bool active =
                waiting ||
                mission.status == MissionStatus::InTransit;

            if (!active ||
                mission.cargoType != request.resource ||
                mission.deadline < request.currentTime) {
                continue;
            }

            if (mission.destination == request.destination &&
                mission.deadline <= deadline) {
                incoming += mission.cargoAmount;
            }

            if (waiting &&
                mission.origin == request.source) {
                outgoing += mission.cargoAmount;
            }
        }

        if (!std::isfinite(incoming) ||
            !std::isfinite(outgoing)) {
            throw std::overflow_error(
                "Supply commitments overflow"
            );
        }

        std::vector<const Spacecraft*> candidates;
        std::set<int> shipIds;

        for (const auto& ship : ships) {
            ship.validate(galaxy);

            if (!shipIds.insert(ship.id).second) {
                throw std::invalid_argument(
                    "Duplicate spacecraft ID"
                );
            }

            if (ship.operational &&
                ship.currentLocation == request.source &&
                ship.cargoCapacity > 0.0) {
                candidates.push_back(&ship);
            }
        }

        std::sort(
            candidates.begin(),
            candidates.end(),
            [](const Spacecraft* a, const Spacecraft* b) {
                return a->id < b->id;
            }
        );

        SupplyProposal result;

        double need = std::max(
            0.0,
            receiver.requiredDelivery - incoming
        );

        double surplus = std::max(
            0.0,
            donor.currentStock +
            std::min(0.0, donor.netChangePerDay) *
                request.horizonDays -
            request.safetyStock -
            outgoing
        );

        if (need <= 0.0) {
            result.reason =
                "No uncovered supply requirement";

            return result;
        }

        if (surplus <= 0.0) {
            result.reason =
                "Source has no safe surplus";

            return result;
        }

        double capacity =
            destination.storageCapacity.contains(
                request.resource
            )
                ? destination.storageCapacity.at(
                    request.resource
                )
                : 0.0;

        for (const Spacecraft* ship : candidates) {
            if (result.candidateChecks >=
                request.maxCandidateChecks) {
                result.reason =
                    "Supply candidate budget exhausted";

                return result;
            }

            ++result.candidateChecks;

            RouteWeights weights = request.weights;
            weights.time /= ship->speed;

            auto route = Dijkstra::findShortestPath(
                galaxy,
                request.source,
                request.destination,
                weights
            );

            if (!route.found) {
                continue;
            }

            double arrival =
                std::max(
                    request.currentTime,
                    ship->availabilityTime
                ) +
                route.totalTime / ship->speed;

            if (!std::isfinite(arrival) ||
                arrival > deadline) {
                continue;
            }

            auto atArrival = ResourcePredictor::predict(
                destination,
                request.resource,
                arrival - request.currentTime
            );

            double room = std::max(
                0.0,
                capacity -
                atArrival.stockAtHorizon -
                incoming
            );

            double quantity = std::min({
                need,
                surplus,
                ship->cargoCapacity,
                room
            });

            for (std::size_t i = 1;
                 i < route.path.size();
                 ++i) {
                const auto& edges = galaxy.getNeighbors(
                    route.path[i - 1]
                );

                int target = route.path[i];

                auto edge = std::find_if(
                    edges.begin(),
                    edges.end(),
                    [target](const Route& e) {
                        return e.to == target;
                    }
                );

                if (edge == edges.end()) {
                    throw std::logic_error(
                        "Missing supply route edge"
                    );
                }

                quantity = std::min(
                    quantity,
                    edge->capacity
                );
            }

            if (quantity <= 0.0) {
                continue;
            }

            Mission mission;

            mission.id = request.missionId;
            mission.origin = request.source;
            mission.destination = request.destination;

            mission.cargoType = request.resource;
            mission.cargoAmount = quantity;

            mission.deadline = deadline;
            mission.priority = 10;

            mission.importance = std::max(
                1,
                destination.strategicImportance
            );

            if (request.resource == "medicine") {
                mission.requiredCapabilities = {
                    "cargo", "medical"
                };
            }

            auto evaluation = MissionEvaluator::evaluate(
                galaxy,
                *ship,
                mission,
                request.currentTime,
                request.weights,
                request.minimumReliability
            );

            if (evaluation.feasible) {
                result.mission = mission;
                result.reason = "Supply mission proposed";

                return result;
            }
        }

        result.reason =
            "No feasible supply candidate found";

        return result;
    }
};

}