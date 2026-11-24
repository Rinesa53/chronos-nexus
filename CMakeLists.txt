
cmake_minimum_required(VERSION 3.16)

project(ChronosNexus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(chronos_core
    src/Galaxy.cpp
    src/Dijkstra.cpp
)

target_include_directories(chronos_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(chronos_nexus
    src/main.cpp
)

target_link_libraries(chronos_nexus PRIVATE
    chronos_core
)

include(CTest)

if(BUILD_TESTING)
    add_executable(graph_tests
        tests/GraphTests.cpp
    )

    target_link_libraries(graph_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(graph_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(graph_tests PRIVATE -UNDEBUG)
    endif()

    add_test(
        NAME GalaxyGraphTests
        COMMAND graph_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(model_tests
        tests/ModelTests.cpp
    )

    target_link_libraries(model_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(model_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(model_tests PRIVATE -UNDEBUG)
    endif()

    add_test(
        NAME CivilizationModelTests
        COMMAND model_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(mission_tests
        tests/MissionTests.cpp
    )

    target_link_libraries(mission_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(mission_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(mission_tests PRIVATE -UNDEBUG)
    endif()

    add_test(
        NAME MissionFeasibilityTests
        COMMAND mission_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(scheduler_tests
        tests/SchedulerTests.cpp
    )

    target_link_libraries(scheduler_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(scheduler_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(scheduler_tests PRIVATE -UNDEBUG)
    endif()

    add_test(
        NAME MissionSchedulerTests
        COMMAND scheduler_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(prediction_tests
        tests/PredictionTests.cpp
    )

    target_link_libraries(prediction_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(prediction_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(prediction_tests PRIVATE -UNDEBUG)
    endif()

    add_test(
        NAME ResourcePredictionTests
        COMMAND prediction_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(infrastructure_tests
        tests/InfrastructureTests.cpp
    )

    target_link_libraries(infrastructure_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(infrastructure_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(infrastructure_tests PRIVATE -UNDEBUG)
    endif()

    add_test(
        NAME InfrastructureOptimizationTests
        COMMAND infrastructure_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(preventive_supply_tests
        tests/PreventiveSupplyTests.cpp
    )

    target_link_libraries(preventive_supply_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(
            preventive_supply_tests PRIVATE /UNDEBUG
        )
    else()
        target_compile_options(
            preventive_supply_tests PRIVATE -UNDEBUG
        )
    endif()

    add_test(
        NAME PreventiveSupplyTests
        COMMAND preventive_supply_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(constrained_routing_tests
        tests/ConstrainedRoutingTests.cpp
    )

    target_link_libraries(constrained_routing_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(
            constrained_routing_tests PRIVATE /UNDEBUG
        )
    else()
        target_compile_options(
            constrained_routing_tests PRIVATE -UNDEBUG
        )
    endif()

    add_test(
        NAME ConstrainedRoutingTests
        COMMAND constrained_routing_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(routing_integration_tests
        tests/RoutingIntegrationTests.cpp
    )

    target_link_libraries(routing_integration_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(
            routing_integration_tests PRIVATE /UNDEBUG
        )
    else()
        target_compile_options(
            routing_integration_tests PRIVATE -UNDEBUG
        )
    endif()

    add_test(
        NAME RoutingIntegrationTests
        COMMAND routing_integration_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(capacity_routing_tests
        tests/CapacityRoutingTests.cpp
    )

    target_link_libraries(capacity_routing_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(
            capacity_routing_tests PRIVATE /UNDEBUG
        )
    else()
        target_compile_options(
            capacity_routing_tests PRIVATE -UNDEBUG
        )
    endif()

    add_test(
        NAME CapacityRoutingTests
        COMMAND capacity_routing_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(preventive_routing_tests
        tests/PreventiveRoutingTests.cpp
    )

    target_link_libraries(preventive_routing_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(
            preventive_routing_tests PRIVATE /UNDEBUG
        )
    else()
        target_compile_options(
            preventive_routing_tests PRIVATE -UNDEBUG
        )
    endif()

    add_test(
        NAME PreventiveRoutingTests
        COMMAND preventive_routing_tests
    )
endif()
if(BUILD_TESTING)
    add_executable(resource_simulation_tests
        tests/ResourceSimulationTests.cpp
    )

    target_link_libraries(resource_simulation_tests PRIVATE
        chronos_core
    )

    if(MSVC)
        target_compile_options(
            resource_simulation_tests PRIVATE /UNDEBUG
        )
    else()
        target_compile_options(
            resource_simulation_tests PRIVATE -UNDEBUG
        )
    endif()

    add_test(
        NAME ResourceSimulationTests
        COMMAND resource_simulation_tests
    )
endif()