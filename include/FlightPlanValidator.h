#pragma once

#include "FlightSimulationTypes.h"
#include "MissionScheduler.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace chronos {

class FlightPlanValidator {
public:
    static const Route& edge(
        const Galaxy& galaxy,
        int from,
        int to
    ) {
        if (!galaxy.hasPlanet(from)) {
            throw std::invalid_argument(
                "Unknown route origin in flight plan"
            );
        }

        const auto& neighbors = galaxy.getNeighbors(from);

        auto found = std::find_if(
            neighbors.begin(),
            neighbors.end(),
            [to](const Route& route) {
                return route.to == to;
            }
        );

        if (found == neighbors.end()) {
            throw std::invalid_argument(
                "Missing route in flight plan"
            );
        }

        return *found;
    }

    static ValidatedFlight validate(
        const Galaxy& galaxy,
        const Spacecraft& ship,
        const Mission& mission,
        const MissionAssignment& assignment,
        double now,
        const RouteReservations& reservedRoutes,
        const StockReservations& reservedStock,
        double minimumReliability = 0.0
    ) {
        ship.validate(galaxy);
        mission.validate(galaxy);

        if (!std::isfinite(now) ||
            now < 0.0 ||
            !std::isfinite(minimumReliability) ||
            minimumReliability < 0.0 ||
            minimumReliability > 1.0) {
            throw std::invalid_argument(
                "Invalid flight validation parameters"
            );
        }

        if (assignment.missionId != mission.id ||
            assignment.spacecraftId != ship.id ||
            !assignment.evaluation.feasible ||
            !assignment.evaluation.route.found ||
            mission.status != MissionStatus::Pending ||
            !ship.operational ||
            ship.currentLocation != mission.origin ||
            mission.cargoAmount > ship.cargoCapacity) {
            throw std::invalid_argument(
                "Assignment cannot start in the current state"
            );
        }

        for (const auto& capability :
             mission.requiredCapabilities) {
            if (std::find(
                    ship.capabilities.begin(),
                    ship.capabilities.end(),
                    capability
                ) == ship.capabilities.end()) {
                throw std::invalid_argument(
                    "Spacecraft lacks mission capability"
                );
            }
        }

        double departure =
            assignment.evaluation.departureTime;

        const auto& path =
            assignment.evaluation.route.path;

        if (!std::isfinite(departure) ||
            departure < now ||
            departure < ship.availabilityTime ||
            path.empty() ||
            path.front() != mission.origin ||
            path.back() != mission.destination) {
            throw std::invalid_argument(
                "Invalid departure or flight path"
            );
        }

        const auto& stock =
            galaxy.getPlanet(mission.origin).resources;

        auto available = stock.find(mission.cargoType);

        double quantity =
            available == stock.end()
                ? 0.0
                : available->second;

        auto reservation = reservedStock.find({
            mission.origin,
            mission.cargoType
        });

        double held =
            reservation == reservedStock.end()
                ? 0.0
                : reservation->second;

        if (!std::isfinite(quantity) ||
            quantity < 0.0 ||
            !std::isfinite(held) ||
            held < 0.0 ||
            mission.cargoAmount > quantity - held) {
            throw std::invalid_argument(
                "Insufficient unreserved source stock"
            );
        }

        RouteReservations routeUsage;

        double fuel = 0.0;
        double duration = 0.0;
        double reliability = ship.reliability;

        for (std::size_t i = 1; i < path.size(); ++i) {
            const Route& route = edge(
                galaxy,
                path[i - 1],
                path[i]
            );

            double free = availableRouteCapacity(
                route,
                &reservedRoutes
            );

            double usedHere = routeUsage[{
                route.from,
                route.to
            }];

            if (!route.available ||
                route.reliability == 0.0 ||
                mission.cargoAmount > free - usedHere) {
                throw std::invalid_argument(
                    "Flight path is unavailable or over capacity"
                );
            }

            routeUsage[{
                route.from,
                route.to
            }] += mission.cargoAmount;

            fuel += route.fuelCost;
            duration += route.travelTime / ship.speed;
            reliability *= route.reliability;
        }

        double arrival = departure + duration;

        if (!std::isfinite(fuel) ||
            !std::isfinite(arrival) ||
            fuel > ship.currentFuel ||
            arrival > mission.deadline ||
            reliability < minimumReliability) {
            throw std::invalid_argument(
                "Flight violates fuel, deadline or reliability constraints"
            );
        }

        return {
            mission.id,
            ship.id,
            path,
            departure,
            arrival,
            minimumReliability
        };
    }
};

}