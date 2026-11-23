#pragma once

#include "Route.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <utility>

namespace chronos {

using RouteReservations =
    std::map<std::pair<int, int>, double>;

inline double availableRouteCapacity(
    const Route& route,
    const RouteReservations* reservations = nullptr
) {
    double used = 0.0;

    if (reservations != nullptr) {
        auto entry = reservations->find({
            route.from,
            route.to
        });

        if (entry != reservations->end()) {
            used = entry->second;
        }
    }

    if (!std::isfinite(used) || used < 0.0) {
        throw std::invalid_argument(
            "Reserved cargo must be finite and non-negative"
        );
    }

    return std::max(
        0.0,
        route.capacity - used
    );
}

}