
#include "Galaxy.h"

#include <cmath>
#include <stdexcept>

namespace chronos {

void Galaxy::addPlanet(const Planet& planet) {
    if (planets.contains(planet.id)) {
        throw std::invalid_argument("Planet ID already exists");
    }

    planets.emplace(planet.id, planet);
    adjacency.emplace(planet.id, std::vector<Route>{});
}

void Galaxy::addRoute(const Route& route) {
    if (!hasPlanet(route.from) || !hasPlanet(route.to)) {
        throw std::invalid_argument("Unknown planet");
    }

    if (route.from == route.to) {
        throw std::invalid_argument("Self routes not supported");
    }

    if (!std::isfinite(route.distance) || route.distance < 0 ||
        !std::isfinite(route.travelTime) || route.travelTime < 0 ||
        !std::isfinite(route.fuelCost) || route.fuelCost < 0 ||
        !std::isfinite(route.capacity) || route.capacity < 0 ||
        !std::isfinite(route.risk) || route.risk < 0 || route.risk > 1 ||
        !std::isfinite(route.reliability) ||
        route.reliability < 0 || route.reliability > 1) {
        throw std::invalid_argument("Invalid route");
    }

    auto& routes = adjacency.at(route.from);

    for (const auto& existing : routes) {
        if (existing.to == route.to) {
            throw std::invalid_argument("Duplicate route");
        }
    }

    routes.push_back(route);
    ++routeCount;
}

bool Galaxy::hasPlanet(int id) const {
    return planets.contains(id);
}

const Planet& Galaxy::getPlanet(int id) const {
    return planets.at(id);
}

const std::vector<Route>& Galaxy::getNeighbors(int id) const {
    return adjacency.at(id);
}

void Galaxy::setRouteAvailability(
    int from, int to, bool available
) {
    auto& routes = adjacency.at(from);

    for (auto& route : routes) {
        if (route.to == to) {
            route.available = available;
            return;
        }
    }

    throw std::out_of_range("Route not found");
}

std::size_t Galaxy::getPlanetCount() const {
    return planets.size();
}

std::size_t Galaxy::getRouteCount() const {
    return routeCount;
}

}
