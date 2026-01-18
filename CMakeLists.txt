cmake_minimum_required(VERSION 3.16)
project(grobid_test LANGUAGES CXX)

# -------------------------------
# C++ standard
# -------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------
# Qt6 (Core module for QProcess/QThread)
# Adjust to Qt5 if you have Qt5
# -------------------------------
find_package(Qt6 COMPONENTS Core REQUIRED)

# -------------------------------
# Include directories
# -------------------------------
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml2
    ${CMAKE_CURRENT_SOURCE_DIR}/json/include
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib
)

# -------------------------------
# Source files
# -------------------------------
set(SOURCES
    main.cpp              # optional, create a main.cpp to call your utils
    grobid_utils.cpp
    tinyxml2/tinyxml2.cpp # compile tinyxml2
)

# -------------------------------
# Add executable
# -------------------------------
add_executable(grobid_test ${SOURCES})

# -------------------------------
# Link Qt
# -------------------------------
target_link_libraries(grobid_test Qt6::Core)

# -------------------------------
# Threads (needed on macOS/Linux)
# -------------------------------
find_package(Threads REQUIRED)
target_link_libraries(grobid_test Threads::Threads)

set_target_properties(grobid_test PROPERTIES
    MACOSX_RPATH ON
)
