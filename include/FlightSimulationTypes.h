#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace chronos {

using StockReservations =
    std::map<std::pair<int, std::string>, double>;

struct ValidatedFlight {
    int missionId = 0;
    int spacecraftId = 0;

    std::vector<int> path;

    double departureTime = 0.0;
    double arrivalTime = 0.0;
    double minimumReliability = 0.0;
};

struct FlightStatistics {
    std::size_t scheduledMissions = 0;
    std::size_t completedMissions = 0;
    std::size_t failedMissions = 0;
    std::size_t edgesEntered = 0;

    double resourcesLoaded = 0.0;
    double resourcesDelivered = 0.0;
    double resourcesReturned = 0.0;
    double cargoWaste = 0.0;

    double fuelConsumed = 0.0;
    double totalDistance = 0.0;
    double operatingCost = 0.0;
    double totalRisk = 0.0;
};

struct FlightRecord {
    double time = 0.0;
    std::string kind;

    int missionId = 0;
    int spacecraftId = 0;
    int location = 0;

    double amount = 0.0;
    std::string detail;
};

}