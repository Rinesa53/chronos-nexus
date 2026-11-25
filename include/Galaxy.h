
#pragma once

#include "Planet.h"
#include "Route.h"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace chronos {

class Galaxy {
private:
    std::unordered_map<int, Planet> planets;
    std::unordered_map<int, std::vector<Route>> adjacency;

    std::size_t routeCount = 0;

public:
    void addPlanet(const Planet& planet);
    void addRoute(const Route& route);

    bool hasPlanet(int id) const;

    const Planet& getPlanet(int id) const;

    const std::unordered_map<int, Planet>& getPlanets() const;

void replacePlanetResources(
    int id,
    std::unordered_map<std::string, double> resources
);

static void validatePlanetState(const Planet& planet);

void replacePlanetState(Planet planet);

    const std::vector<Route>& getNeighbors(int id) const;

    void setRouteAvailability(int from, int to, bool available);

    void replaceRoute(const Route& route);
    
    std::size_t getPlanetCount() const;
    std::size_t getRouteCount() const;
};

}
