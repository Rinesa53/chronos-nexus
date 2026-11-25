#pragma once

namespace chronos {

struct Route {
    int from;
    int to;

    double distance;
    double travelTime;
    double fuelCost;
    double risk;

    double capacity;
    double reliability;

    bool available = true;
    bool wormhole = false;
};

}