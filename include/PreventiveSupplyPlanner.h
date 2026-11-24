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
    RoutingOptions routing;

    std::size_t maxCandidateChecks = 1000;
    std::size_t maxCargoRepairs = 3;
};

struct SupplyProposal {
    std::optional<Mission> mission;
    std::string reason;

    std::size_t candidateChecks = 0;
    std::size_t routingStatesExplored = 0;

    bool routingBudgetExhausted = false;
    bool routingLabelsPruned = false;
};

class PreventiveSupplyPlanner {
    static double roomAtArrival(
        const Planet& destination,
        const SupplyRequest& request,
        double arrival,
        double incoming
    ) {
        auto forecast = ResourcePredictor::predict(
            destination,
            request.resource,
            arrival - request.currentTime
        );

        auto entry = destination.storageCapacity.find(
            request.resource
        );

        double capacity =
            entry == destination.storageCapacity.end()
                ? 0.0
                : entry->second;

        return std::max(
            0.0,
            capacity -
            forecast.stockAtHorizon -
            incoming
        );
    }

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
            request.maxCandidateChecks == 0 ||
            request.maxCargoRepairs == 0 ||
            request.routing.maxStates == 0 ||
            request.routing.maxLabelsPerNode == 0 ||
            request.routing.maxLabelsCreated == 0) {
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

        for (const Spacecraft* ship : candidates) {
            if (result.candidateChecks >=
                request.maxCandidateChecks) {
                result.reason =
                    "Supply candidate budget exhausted";

                return result;
            }

            ++result.candidateChecks;

            double departure = std::max(
                request.currentTime,
                ship->availabilityTime
            );

            if (departure > deadline) {
                continue;
            }

            // Search for a corridor before deciding
            // the real cargo amount.
            ConstrainedRouteRequest probe;

            probe.origin = request.source;
            probe.destination = request.destination;

            probe.cargoAmount = 0.0;
            probe.requirePositiveCapacity = true;

            probe.timeLimit = deadline - departure;

            probe.minimumReliability =
                request.minimumReliability;

            probe.weights = request.weights;

            probe.reservedCargo =
                request.routing.reservedCargo;

            probe.maxStates =
                request.routing.maxStates;

            probe.maxLabelsPerNode =
                request.routing.maxLabelsPerNode;

            probe.maxLabelsCreated =
                request.routing.maxLabelsCreated;

            auto search = ConstrainedRouter::find(
                galaxy,
                *ship,
                probe
            );

            result.routingStatesExplored +=
                search.statesExplored;

            result.routingBudgetExhausted |=
                search.budgetExhausted;

            result.routingLabelsPruned |=
                search.labelsPruned;

            if (!search.path.found) {
                continue;
            }

            double arrival =
                departure + search.path.totalTime;

            double room = roomAtArrival(
                destination,
                request,
                arrival,
                incoming
            );

            double quantity = std::min({
                need,
                surplus,
                ship->cargoCapacity,
                room
            });

            const auto& path = search.path.path;

            for (std::size_t i = 1;
                 i < path.size();
                 ++i) {
                const auto& edges =
                    galaxy.getNeighbors(path[i - 1]);

                int target = path[i];

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
                    availableRouteCapacity(
                        *edge,
                        request.routing.reservedCargo
                    )
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

            for (std::size_t repair = 0;
                 repair < request.maxCargoRepairs;
                 ++repair) {
                mission.cargoAmount = quantity;

                auto evaluation = MissionEvaluator::evaluate(
                    galaxy,
                    *ship,
                    mission,
                    request.currentTime,
                    request.weights,
                    request.minimumReliability,
                    request.routing
                );

                result.routingStatesExplored +=
                    evaluation.routingStatesExplored;

                result.routingBudgetExhausted |=
                    evaluation.routingBudgetExhausted;

                result.routingLabelsPruned |=
                    evaluation.routingLabelsPruned;

                if (!evaluation.feasible) {
                    break;
                }

                double actualRoom = roomAtArrival(
                    destination,
                    request,
                    evaluation.arrivalTime,
                    incoming
                );

                if (quantity <= actualRoom) {
                    result.mission = mission;
                    result.reason =
                        "Supply mission proposed";

                    return result;
                }

                quantity = std::min(
                    quantity,
                    actualRoom
                );

                if (quantity <= 0.0) {
                    break;
                }
            }
        }

        result.reason =
            "No feasible supply candidate found within limits";

        return result;
    }
};

}