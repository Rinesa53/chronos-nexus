#pragma once

#include "ConstructionEffects.h"
#include "FlightSimulator.h"
#include "InfrastructureOptimizer.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace chronos {

enum class ConstructionStatus {
    Building,
    Completed,
    Failed
};

struct ConstructionJob {
    Facility facility;

    double startedAt = 0.0;
    double completesAt = 0.0;

    std::size_t sequence = 0;

    ConstructionStatus status =
        ConstructionStatus::Building;
};

struct ConstructionStatistics {
    std::size_t started = 0;
    std::size_t completed = 0;
    std::size_t failed = 0;

    int spentBudget = 0;
    double completedValue = 0.0;
};

struct ConstructionRecord {
    double time;
    int facilityId;
    int planetId;
    ConstructionStatus status;
    std::string detail;
};

class ConstructionSimulator {
    struct Event {
        double time;
        std::size_t sequence;
        int facilityId;

        bool operator>(const Event& other) const {
            return std::tie(time, sequence) >
                   std::tie(other.time, other.sequence);
        }
    };

    Galaxy& galaxy;
    FlightSimulator& flights;

    int budgetRemaining;
    double committedValue = 0.0;
    std::size_t nextSequence = 0;

    std::map<int, ConstructionJob> jobs;

    std::priority_queue<
        Event,
        std::vector<Event>,
        std::greater<Event>
    > events;

    ConstructionStatistics stats;
    std::vector<ConstructionRecord> records;

    void ensureClockOrder() const {
        if (!events.empty() &&
            events.top().time < currentTime()) {
            throw std::logic_error(
                "Construction completion was skipped; advance through ConstructionSimulator"
            );
        }
    }

    void record(
        const ConstructionJob& job,
        const std::string& detail = ""
    ) {
        records.push_back({
            currentTime(),
            job.facility.id,
            job.facility.planetId,
            job.status,
            detail
        });
    }

    void finish(int facilityId) {
        auto& job = jobs.at(facilityId);

        try {
            Planet next = ConstructionEffects::preview(
                galaxy,
                galaxy.getPlanet(job.facility.planetId),
                job.facility
            );

            galaxy.replacePlanetState(
                std::move(next)
            );

            job.status = ConstructionStatus::Completed;

            ++stats.completed;

            stats.completedValue +=
                job.facility.strategicValue;

            record(job);
        } catch (const std::invalid_argument& error) {
            job.status = ConstructionStatus::Failed;
            ++stats.failed;
            record(job, error.what());
        } catch (const std::overflow_error& error) {
            job.status = ConstructionStatus::Failed;
            ++stats.failed;
            record(job, error.what());
        }
    }

public:
    ConstructionSimulator(
        FlightSimulator& simulation,
        int budget
    )
        : galaxy(simulation.getGalaxy()),
          flights(simulation),
          budgetRemaining(budget) {
        if (budget < 0) {
            throw std::invalid_argument(
                "Negative construction budget"
            );
        }
    }

    ConstructionSimulator(
        const ConstructionSimulator&
    ) = delete;

    ConstructionSimulator& operator=(
        const ConstructionSimulator&
    ) = delete;

    double currentTime() const {
        return flights.currentTime();
    }

    int remainingBudget() const {
        return budgetRemaining;
    }

    const ConstructionStatistics& statistics() const {
        return stats;
    }

    const std::vector<ConstructionRecord>& history() const {
        return records;
    }

    const ConstructionJob& getJob(int id) const {
        return jobs.at(id);
    }

    void startPlan(
        const std::vector<Facility>& catalog,
        const InfrastructurePlan& plan
    ) {
        ensureClockOrder();

        std::map<int, const Facility*> byId;

        for (const auto& facility : catalog) {
            facility.validate(galaxy);

            if (!byId.emplace(
                    facility.id,
                    &facility
                ).second) {
                throw std::invalid_argument(
                    "Duplicate facility in catalog"
                );
            }
        }

        std::set<int> selected;
        std::vector<ConstructionJob> staged;

        int cost = 0;
        double nextValue = committedValue;

        for (int id : plan.facilityIds) {
            auto entry = byId.find(id);

            if (entry == byId.end() ||
                jobs.contains(id) ||
                !selected.insert(id).second) {
                throw std::invalid_argument(
                    "Unknown or previously started facility"
                );
            }

            const Facility& facility = *entry->second;

            if (facility.constructionCost >
                budgetRemaining - cost) {
                throw std::invalid_argument(
                    "Construction plan exceeds remaining budget"
                );
            }

            cost += facility.constructionCost;

            double completion =
                currentTime() +
                facility.constructionTime;

            nextValue += facility.strategicValue;

            if (!std::isfinite(completion) ||
                !std::isfinite(nextValue)) {
                throw std::overflow_error(
                    "Construction time or value overflow"
                );
            }

            staged.push_back({
                facility,
                currentTime(),
                completion,
                nextSequence + staged.size(),
                ConstructionStatus::Building
            });
        }

        // Validate the combined effects of pending
        // and newly proposed construction projects.
        std::vector<ConstructionJob> timeline = staged;

        for (const auto& [id, job] : jobs) {
            if (job.status == ConstructionStatus::Building) {
                timeline.push_back(job);
            }
        }

        std::sort(
            timeline.begin(),
            timeline.end(),
            [](const ConstructionJob& a,
               const ConstructionJob& b) {
                return std::tie(
                    a.completesAt,
                    a.sequence
                ) < std::tie(
                    b.completesAt,
                    b.sequence
                );
            }
        );

        std::map<int, Planet> projected;

        for (const auto& job : timeline) {
            int planetId = job.facility.planetId;

            auto entry = projected.try_emplace(
                planetId,
                galaxy.getPlanet(planetId)
            ).first;

            entry->second = ConstructionEffects::preview(
                galaxy,
                entry->second,
                job.facility
            );
        }

        budgetRemaining -= cost;
        stats.spentBudget += cost;
        committedValue = nextValue;

        for (auto& job : staged) {
            int id = job.facility.id;

            auto& saved = jobs.emplace(
                id,
                std::move(job)
            ).first->second;

            events.push({
                saved.completesAt,
                saved.sequence,
                id
            });

            ++stats.started;
            record(saved);
        }

        nextSequence += staged.size();
    }

    void advanceTo(double targetTime) {
        if (!std::isfinite(targetTime) ||
            targetTime < currentTime()) {
            throw std::invalid_argument(
                "Construction time cannot go backwards"
            );
        }

        ensureClockOrder();

        while (!events.empty() &&
               events.top().time <= targetTime) {
            Event event = events.top();

            flights.advanceTo(event.time);

            finish(event.facilityId);

            events.pop();
        }

        flights.advanceTo(targetTime);
    }
};

}