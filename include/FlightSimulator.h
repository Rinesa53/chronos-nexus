#pragma once

#include "FlightPlanValidator.h"
#include "ResourceSimulator.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace chronos {

class FlightSimulator {
    struct ActiveFlight {
        ValidatedFlight plan;
        std::size_t token = 0;
        std::size_t nextEdge = 0;
        bool loaded = false;
    };

    struct Event {
        double time;
        int phase;
        std::size_t sequence;
        int shipId;
        std::size_t token;

        bool operator>(const Event& other) const {
            return std::tie(time, phase, sequence) >
                   std::tie(
                       other.time,
                       other.phase,
                       other.sequence
                   );
        }
    };

    Galaxy& galaxy;
    ResourceSimulator clock;

    std::map<int, Spacecraft> ships;
    std::map<int, Mission> missions;
    std::map<int, ActiveFlight> flights;

    RouteReservations heldRoutes;
    StockReservations heldStock;

    std::priority_queue<
        Event,
        std::vector<Event>,
        std::greater<Event>
    > events;

    FlightStatistics stats;
    std::vector<FlightRecord> records;

    std::size_t nextToken = 0;
    std::size_t nextSequence = 0;

    template <typename Map, typename Key>
    static void release(
        Map& reservations,
        const Key& key,
        double amount
    ) {
        auto entry = reservations.find(key);

        if (entry == reservations.end()) {
            return;
        }

        double left = std::max(
            0.0,
            entry->second - amount
        );

        if (left == 0.0) {
            reservations.erase(entry);
        } else {
            entry->second = left;
        }
    }

    void record(
        const ActiveFlight& flight,
        const std::string& kind,
        double amount,
        const std::string& detail = ""
    ) {
        records.push_back({
            currentTime(),
            kind,
            flight.plan.missionId,
            flight.plan.spacecraftId,
            ships.at(
                flight.plan.spacecraftId
            ).currentLocation,
            amount,
            detail
        });
    }

    void enqueue(
        double time,
        int phase,
        const ActiveFlight& flight
    ) {
        events.push({
            time,
            phase,
            nextSequence++,
            flight.plan.spacecraftId,
            flight.token
        });
    }

    double deposit(
        int planetId,
        const std::string& resource,
        double amount
    ) {
        const auto& planet = galaxy.getPlanet(planetId);
        auto resources = planet.resources;

        auto capacityEntry =
            planet.storageCapacity.find(resource);

        double capacity =
            capacityEntry == planet.storageCapacity.end()
                ? 0.0
                : capacityEntry->second;

        double stored = std::min(
            amount,
            std::max(
                0.0,
                capacity - resources[resource]
            )
        );

        resources[resource] += stored;

        galaxy.replacePlanetResources(
            planetId,
            std::move(resources)
        );

        return stored;
    }

    void releaseRemaining(const ActiveFlight& flight) {
        double amount =
            missions.at(flight.plan.missionId).cargoAmount;

        const auto& path = flight.plan.path;

        for (std::size_t i = flight.nextEdge;
             i + 1 < path.size();
             ++i) {
            release(
                heldRoutes,
                std::make_pair(path[i], path[i + 1]),
                amount
            );
        }
    }

    void fail(
        int shipId,
        const std::string& reason
    ) {
        auto& flight = flights.at(shipId);
        auto& ship = ships.at(shipId);

        auto& mission =
            missions.at(flight.plan.missionId);

        releaseRemaining(flight);

        if (flight.loaded) {
            double returned = deposit(
                ship.currentLocation,
                mission.cargoType,
                mission.cargoAmount
            );

            stats.resourcesReturned += returned;

            stats.cargoWaste +=
                mission.cargoAmount - returned;
        } else {
            release(
                heldStock,
                std::make_pair(
                    mission.origin,
                    mission.cargoType
                ),
                mission.cargoAmount
            );
        }

        mission.status = MissionStatus::Failed;
        ship.availabilityTime = currentTime();

        ++stats.failedMissions;

        record(
            flight,
            "failed",
            mission.cargoAmount,
            reason
        );

        flights.erase(shipId);
    }

    void deliver(int shipId) {
        auto& flight = flights.at(shipId);
        auto& ship = ships.at(shipId);

        auto& mission =
            missions.at(flight.plan.missionId);

        double delivered = deposit(
            mission.destination,
            mission.cargoType,
            mission.cargoAmount
        );

        stats.resourcesDelivered += delivered;

        stats.cargoWaste +=
            mission.cargoAmount - delivered;

        bool success =
            delivered >= mission.cargoAmount &&
            currentTime() <= mission.deadline + 1e-9;

        mission.status = success
            ? MissionStatus::Completed
            : MissionStatus::Failed;

        if (success) {
            ++stats.completedMissions;
        } else {
            ++stats.failedMissions;
        }

        ship.availabilityTime = currentTime();

        record(
            flight,
            success ? "delivered" : "delivery_failed",
            delivered,
            success
                ? ""
                : "Late arrival or insufficient destination storage"
        );

        flights.erase(shipId);
    }

    void startEdge(int shipId) {
        auto& flight = flights.at(shipId);
        auto& ship = ships.at(shipId);

        auto& mission =
            missions.at(flight.plan.missionId);

        if (flight.nextEdge + 1 >=
            flight.plan.path.size()) {
            deliver(shipId);
            return;
        }

        int from = flight.plan.path[flight.nextEdge];
        int to = flight.plan.path[flight.nextEdge + 1];

        const Route& route = FlightPlanValidator::edge(
            galaxy, from, to
        );

        auto reserved = heldRoutes.find({from, to});

        double used =
            reserved == heldRoutes.end()
                ? 0.0
                : reserved->second;

        double duration = route.travelTime / ship.speed;
        double arrival = currentTime() + duration;

        if (ship.currentLocation != from ||
            !route.available ||
            route.reliability == 0.0 ||
            used > route.capacity ||
            mission.cargoAmount > route.capacity ||
            ship.currentFuel < route.fuelCost ||
            !std::isfinite(arrival)) {
            fail(
                shipId,
                "Next edge is no longer feasible"
            );

            return;
        }

        ship.currentFuel = std::max(
            0.0,
            ship.currentFuel - route.fuelCost
        );

        ++stats.edgesEntered;

        stats.fuelConsumed += route.fuelCost;
        stats.totalDistance += route.distance;

        stats.operatingCost +=
            duration * ship.operatingCost;

        stats.totalRisk += route.risk;

        record(
            flight,
            "edge_entered",
            mission.cargoAmount,
            std::to_string(from) +
                " -> " +
                std::to_string(to)
        );

        enqueue(arrival, 0, flight);
    }

    void depart(int shipId) {
        auto& flight = flights.at(shipId);

        auto& mission =
            missions.at(flight.plan.missionId);

        auto resources =
            galaxy.getPlanet(mission.origin).resources;

        if (resources[mission.cargoType] <
            mission.cargoAmount) {
            fail(
                shipId,
                "Source stock became insufficient before departure"
            );

            return;
        }

        resources[mission.cargoType] -=
            mission.cargoAmount;

        galaxy.replacePlanetResources(
            mission.origin,
            std::move(resources)
        );

        release(
            heldStock,
            std::make_pair(
                mission.origin,
                mission.cargoType
            ),
            mission.cargoAmount
        );

        flight.loaded = true;
        mission.status = MissionStatus::InTransit;

        stats.resourcesLoaded += mission.cargoAmount;

        record(
            flight,
            "departed",
            mission.cargoAmount
        );

        startEdge(shipId);
    }

    void arrive(int shipId) {
        auto& flight = flights.at(shipId);
        auto& ship = ships.at(shipId);

        auto& mission =
            missions.at(flight.plan.missionId);

        int from = flight.plan.path[flight.nextEdge];
        int to = flight.plan.path[flight.nextEdge + 1];

        release(
            heldRoutes,
            std::make_pair(from, to),
            mission.cargoAmount
        );

        ship.currentLocation = to;
        ++flight.nextEdge;

        record(
            flight,
            "arrived_node",
            mission.cargoAmount
        );

        if (flight.nextEdge + 1 >=
            flight.plan.path.size()) {
            deliver(shipId);
        } else if (
            currentTime() > mission.deadline + 1e-9
        ) {
            fail(shipId, "Mission deadline passed");
        } else {
            startEdge(shipId);
        }
    }

public:
    FlightSimulator(
        Galaxy& world,
        const std::vector<Spacecraft>& initialShips,
        const std::vector<Mission>& initialMissions,
        double initialTime = 0.0
    )
        : galaxy(world),
          clock(world, initialTime) {
        for (const auto& ship : initialShips) {
            ship.validate(world);

            if (!ships.emplace(ship.id, ship).second) {
                throw std::invalid_argument(
                    "Duplicate simulation spacecraft ID"
                );
            }
        }

        for (const auto& mission : initialMissions) {
            addMission(mission);
        }

        clock.advanceTo(initialTime);
    }

    FlightSimulator(const FlightSimulator&) = delete;

    FlightSimulator& operator=(
        const FlightSimulator&
    ) = delete;

    Galaxy& getGalaxy() {
    return galaxy;
}

const Galaxy& getGalaxy() const {
    return galaxy;
}
    double currentTime() const {
        return clock.currentTime();
    }

    const Spacecraft& getShip(int id) const {
        return ships.at(id);
    }

    const Mission& getMission(int id) const {
        return missions.at(id);
    }

    bool isBusy(int id) const {
        return flights.contains(id);
    }

    const FlightStatistics& statistics() const {
        return stats;
    }

    const ResourceTotals& resourceStatistics() const {
        return clock.totals();
    }

    const std::vector<FlightRecord>& history() const {
        return records;
    }

    const RouteReservations& routeReservations() const {
        return heldRoutes;
    }

    const StockReservations& stockReservations() const {
        return heldStock;
    }

    void addMission(const Mission& mission) {
        mission.validate(galaxy);

        if (mission.status != MissionStatus::Pending ||
            !missions.emplace(
                mission.id,
                mission
            ).second) {
            throw std::invalid_argument(
                "Simulation requires a new pending mission"
            );
        }
    }

    std::vector<Spacecraft> freeShips() const {
        std::vector<Spacecraft> result;

        for (const auto& [id, ship] : ships) {
            if (ship.operational &&
                !flights.contains(id)) {
                result.push_back(ship);
            }
        }

        return result;
    }

    std::vector<Mission> pendingMissions() const {
        std::vector<Mission> result;

        for (const auto& [id, mission] : missions) {
            if (mission.status == MissionStatus::Pending) {
                result.push_back(mission);
            }
        }

        return result;
    }

    double cargoOnboard() const {
        double result = 0.0;

        for (const auto& [id, flight] : flights) {
            if (flight.loaded) {
                result += missions.at(
                    flight.plan.missionId
                ).cargoAmount;
            }
        }

        return result;
    }

    void acceptPlan(
        const ScheduleResult& plan,
        double minimumReliability = 0.0
    ) {
        if (!std::isfinite(minimumReliability) ||
            minimumReliability < 0.0 ||
            minimumReliability > 1.0) {
            throw std::invalid_argument(
                "Invalid simulation reliability threshold"
            );
        }

        auto routeCopy = heldRoutes;
        auto stockCopy = heldStock;

        std::set<int> shipIds;
        std::set<int> missionIds;

        std::vector<ValidatedFlight> checked;

        for (const auto& assignment : plan.assignments) {
            const auto& ship =
                ships.at(assignment.spacecraftId);

            const auto& mission =
                missions.at(assignment.missionId);

            if (flights.contains(ship.id) ||
                !shipIds.insert(ship.id).second ||
                !missionIds.insert(mission.id).second) {
                throw std::invalid_argument(
                    "Spacecraft or mission assigned more than once"
                );
            }

            auto flight = FlightPlanValidator::validate(
                galaxy,
                ship,
                mission,
                assignment,
                currentTime(),
                routeCopy,
                stockCopy,
                minimumReliability
            );

            stockCopy[{
                mission.origin,
                mission.cargoType
            }] += mission.cargoAmount;

            for (std::size_t i = 1;
                 i < flight.path.size();
                 ++i) {
                routeCopy[{
                    flight.path[i - 1],
                    flight.path[i]
                }] += mission.cargoAmount;
            }

            checked.push_back(std::move(flight));
        }

        heldRoutes.swap(routeCopy);
        heldStock.swap(stockCopy);

        for (auto& planEntry : checked) {
            int shipId = planEntry.spacecraftId;
            int missionId = planEntry.missionId;

            ships.at(shipId).availabilityTime =
                planEntry.arrivalTime;

            missions.at(missionId).status =
                MissionStatus::Assigned;

            ActiveFlight flight{
                std::move(planEntry),
                ++nextToken,
                0,
                false
            };

            auto& saved = flights.emplace(
                shipId,
                std::move(flight)
            ).first->second;

            ++stats.scheduledMissions;

            record(
                saved,
                "assigned",
                missions.at(missionId).cargoAmount
            );

            enqueue(
                saved.plan.departureTime,
                1,
                saved
            );
        }
    }

    void advanceTo(double targetTime) {
        if (!std::isfinite(targetTime) ||
            targetTime < currentTime()) {
            throw std::invalid_argument(
                "Flight simulation time cannot go backwards"
            );
        }

        while (!events.empty() &&
               events.top().time <= targetTime) {
            Event event = events.top();

            auto flight = flights.find(event.shipId);

            if (flight == flights.end() ||
                flight->second.token != event.token) {
                events.pop();
                continue;
            }

            if (event.time > currentTime()) {
                clock.advanceTo(event.time);
            }

            events.pop();

            if (event.phase == 1) {
                depart(event.shipId);
            } else {
                arrive(event.shipId);
            }
        }

        if (targetTime > currentTime()) {
            clock.advanceTo(targetTime);
        }
    }
};

}