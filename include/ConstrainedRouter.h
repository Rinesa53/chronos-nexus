#pragma once

#include "Dijkstra.h"
#include "Spacecraft.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>
#include "RouteReservations.h"

namespace chronos {

struct ConstrainedRouteRequest {
    int origin = 0;
    int destination = 0;

    double cargoAmount = 0.0;
    double timeLimit = 0.0;
    double minimumReliability = 0.0;

    RouteWeights weights;

    const RouteReservations* reservedCargo = nullptr;

    std::size_t maxStates = 10000;
    std::size_t maxLabelsPerNode = 16;
    std::size_t maxLabelsCreated = 100000;
};

struct ConstrainedRouteResult {
    PathResult path;

    bool optimal = false;
    bool budgetExhausted = false;
    bool labelsPruned = false;

    std::size_t statesExplored = 0;
    std::size_t labelsCreated = 0;
    std::size_t candidatesPruned = 0;
};

class ConstrainedRouter {
    static constexpr std::size_t none =
        std::numeric_limits<std::size_t>::max();

    struct Label {
        int node = 0;
        std::size_t parent = none;

        double cost = 0.0;
        double distance = 0.0;
        double fuel = 0.0;
        double time = 0.0;
        double risk = 0.0;
        double loss = 0.0;

        bool active = true;
    };

    static bool dominates(
        const Label& a,
        const Label& b
    ) {
        return
            a.cost <= b.cost &&
            a.fuel <= b.fuel &&
            a.time <= b.time &&
            a.loss <= b.loss;
    }

    static PathResult reconstruct(
        const std::vector<Label>& labels,
        std::size_t goal,
        std::size_t explored,
        const Spacecraft& ship
    ) {
        const Label& last = labels[goal];

        PathResult path;

        path.found = true;
        path.totalCost = last.cost;
        path.totalDistance = last.distance;
        path.totalFuel = last.fuel;
        path.totalTime = last.time;
        path.totalRisk = last.risk;

        path.totalReliability =
            ship.reliability * std::exp(-last.loss);

        path.exploredNodes = explored;

        for (std::size_t at = goal;
             at != none;
             at = labels[at].parent) {
            path.path.push_back(labels[at].node);
        }

        std::reverse(
            path.path.begin(),
            path.path.end()
        );

        return path;
    }

public:
    static ConstrainedRouteResult find(
        const Galaxy& galaxy,
        const Spacecraft& ship,
        const ConstrainedRouteRequest& request
    ) {
        ship.validate(galaxy);

        if (!galaxy.hasPlanet(request.origin) ||
            !galaxy.hasPlanet(request.destination) ||
            ship.currentLocation != request.origin ||
            !std::isfinite(request.cargoAmount) ||
            request.cargoAmount <= 0.0 ||
            !std::isfinite(request.timeLimit) ||
            request.timeLimit < 0.0 ||
            !std::isfinite(request.minimumReliability) ||
            request.minimumReliability < 0.0 ||
            request.minimumReliability > 1.0 ||
            request.maxStates == 0 ||
            request.maxLabelsPerNode == 0 ||
            request.maxLabelsCreated == 0) {
            throw std::invalid_argument(
                "Invalid constrained route request"
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
                    "Invalid route weight"
                );
            }
        }

        ConstrainedRouteResult result;

        if (!ship.operational ||
            request.cargoAmount > ship.cargoCapacity ||
            ship.reliability <
                request.minimumReliability) {
            return result;
        }

        double maxLoss =
            std::numeric_limits<double>::infinity();

        if (request.minimumReliability > 0.0) {
            maxLoss =
                std::log(ship.reliability) -
                std::log(request.minimumReliability);
        }

        std::vector<Label> labels;

        Label root;
        root.node = request.origin;

        labels.push_back(root);

        std::unordered_map<
            int,
            std::vector<std::size_t>
        > frontier;

        frontier[request.origin].push_back(0);

        using Entry = std::pair<double, std::size_t>;

        std::priority_queue<
            Entry,
            std::vector<Entry>,
            std::greater<Entry>
        > queue;

        queue.push({0.0, 0});

        std::size_t bestGoal = none;

        if (request.origin == request.destination) {
            bestGoal = 0;
        }

        auto finish = [&](
            std::size_t goal,
            bool proven
        ) {
            result.labelsCreated = labels.size();

            result.path.exploredNodes =
                result.statesExplored;

            if (goal != none) {
                result.path = reconstruct(
                    labels,
                    goal,
                    result.statesExplored,
                    ship
                );

                result.optimal =
                    proven && !result.labelsPruned;
            }

            return result;
        };

        while (!queue.empty() &&
               result.statesExplored <
                   request.maxStates) {
            auto [cost, index] = queue.top();
            queue.pop();

            if (!labels[index].active) {
                continue;
            }

            // Copy because growing the vector can
            // invalidate references to its elements.
            Label current = labels[index];

            ++result.statesExplored;

            if (current.node == request.destination) {
                return finish(index, true);
            }

            for (const auto& edge :
                 galaxy.getNeighbors(current.node)) {
                if (!edge.available ||
    availableRouteCapacity(
        edge,
        request.reservedCargo
    ) < request.cargoAmount ||
    edge.reliability == 0.0) {
    ++result.candidatesPruned;
    continue;
}

                double dt =
                    edge.travelTime / ship.speed;

                Label next;

                next.node = edge.to;
                next.parent = index;

                next.distance =
                    current.distance + edge.distance;

                next.fuel =
                    current.fuel + edge.fuelCost;

                next.time =
                    current.time + dt;

                next.risk =
                    current.risk + edge.risk;

                next.loss =
                    current.loss -
                    std::log(edge.reliability);

                next.cost =
                    current.cost +
                    request.weights.distance *
                        edge.distance +
                    request.weights.fuel *
                        edge.fuelCost +
                    request.weights.time * dt +
                    request.weights.risk *
                        edge.risk;

                if (next.fuel > ship.currentFuel ||
                    next.time > request.timeLimit ||
                    next.loss > maxLoss) {
                    ++result.candidatesPruned;
                    continue;
                }

                for (double value : {
                    next.distance,
                    next.fuel,
                    next.time,
                    next.risk,
                    next.loss,
                    next.cost
                }) {
                    if (!std::isfinite(value)) {
                        throw std::overflow_error(
                            "Constrained route value overflow"
                        );
                    }
                }

                auto& at = frontier[next.node];

                bool dominated = false;

                for (std::size_t id : at) {
                    if (dominates(labels[id], next)) {
                        dominated = true;
                        break;
                    }
                }

                if (dominated) {
                    ++result.candidatesPruned;
                    continue;
                }

                if (labels.size() >=
                    request.maxLabelsCreated) {
                    result.budgetExhausted = true;

                    return finish(bestGoal, false);
                }

                for (std::size_t id : at) {
                    if (dominates(next, labels[id])) {
                        labels[id].active = false;
                    }
                }

                at.erase(
                    std::remove_if(
                        at.begin(),
                        at.end(),
                        [&](std::size_t id) {
                            return !labels[id].active;
                        }
                    ),
                    at.end()
                );

                std::size_t nextId = labels.size();

                labels.push_back(next);
                at.push_back(nextId);

                if (next.node == request.destination &&
                    (bestGoal == none ||
                     next.cost < labels[bestGoal].cost)) {
                    bestGoal = nextId;
                }

                if (at.size() >
                    request.maxLabelsPerNode) {
                    auto worst = std::max_element(
                        at.begin(),
                        at.end(),
                        [&](std::size_t a, std::size_t b) {
                            return
                                labels[a].cost ==
                                    labels[b].cost
                                    ? a < b
                                    : labels[a].cost <
                                      labels[b].cost;
                        }
                    );

                    labels[*worst].active = false;
                    at.erase(worst);

                    result.labelsPruned = true;
                    ++result.candidatesPruned;
                }

                if (labels[nextId].active) {
                    queue.push({
                        next.cost,
                        nextId
                    });
                }
            }
        }

        result.budgetExhausted = !queue.empty();

        return finish(
            bestGoal,
            !result.budgetExhausted
        );
    }
};

}