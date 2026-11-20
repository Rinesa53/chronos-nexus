
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