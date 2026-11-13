cmake_minimum_required(VERSION 3.16)
project(ais_lab3 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(${PROJECT_NAME}
    example/main.cpp
    src/db/KnowledgeBase.cpp
    src/engine/FuzzyMath.cpp
    src/engine/Inference.cpp
    src/engine/SvgPlot.cpp
    src/engine/Report.cpp
    src/engine/SelfTest.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE include)
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json::nlohmann_json)
