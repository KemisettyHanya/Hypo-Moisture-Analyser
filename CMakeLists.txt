cmake_minimum_required(VERSION 3.16)
project(HypoMoistureAnalyser VERSION 1.0.0 LANGUAGES CXX)

# Standard C++17 requirement
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization and Warning flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O3)
endif()

# Dependencies
find_package(OpenCV 4 REQUIRED)
message(STATUS "Found OpenCV: ${OpenCV_VERSION}")

find_package(Threads REQUIRED)

find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    message(STATUS "Found nlohmann_json via CMake package")
else()
    message(STATUS "nlohmann_json package not found via CMake, will use system headers (/usr/include)")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OpenCV_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/Config.cpp
    src/BucketIdManager.cpp
    src/CaptureSource.cpp
    src/SampleAnalyzer.cpp
    src/ReportLogger.cpp
    src/UI.cpp
    src/main.cpp
)

set(HEADERS
    include/Config.hpp
    include/BucketIdManager.hpp
    include/CaptureSource.hpp
    include/SampleAnalyzer.hpp
    include/ReportLogger.hpp
    include/UI.hpp
)

# Target executable
add_executable(hypo_inspector ${SOURCES} ${HEADERS})

target_link_libraries(hypo_inspector
    PRIVATE
    ${OpenCV_LIBS}
    Threads::Threads
)

if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(hypo_inspector PRIVATE nlohmann_json::nlohmann_json)
endif()

# Install rules
install(TARGETS hypo_inspector DESTINATION bin)

# Test target
enable_testing()
add_executable(test_hypo
    tests/test_hypo.cpp
    src/Config.cpp
    src/BucketIdManager.cpp
    src/CaptureSource.cpp
    src/SampleAnalyzer.cpp
    src/ReportLogger.cpp
)
target_link_libraries(test_hypo
    PRIVATE
    ${OpenCV_LIBS}
    Threads::Threads
)
if(TARGET nlohmann_json::nlohmann_json)
    target_link_libraries(test_hypo PRIVATE nlohmann_json::nlohmann_json)
endif()
add_test(NAME UnitIntegrationTests COMMAND test_hypo)
