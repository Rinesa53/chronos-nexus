#pragma once

#include <string>
#include <unordered_map>

namespace chronos {

struct Planet {
    int id;
    std::string name;

    double x = 0.0;
    double y = 0.0;

    long long population = 0;

    int technologyLevel = 0;
    int defenseLevel = 0;
    int strategicImportance = 0;

    std::unordered_map<std::string, double> resources;
    std::unordered_map<std::string, double> production;
    std::unordered_map<std::string, double> consumption;
    std::unordered_map<std::string, double> storageCapacity;
};

}