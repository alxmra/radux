cmake_minimum_required(VERSION 3.16)
project(radux-menu VERSION 1.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)

# Source files
set(SOURCES
    main.cpp
    radial_menu.cpp
    config_loader.cpp
    color_theme.cpp
    hotkey_manager.cpp
    usage_tracker.cpp
)

set(HEADERS
    radial_menu.hpp
    config_loader.hpp
    menu_item.hpp
    color_theme.hpp
    hotkey_manager.hpp
    usage_tracker.hpp
)

# Create executable
add_executable(radux-menu ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(radux-menu
    ${GTKMM_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
)

# Include directories
target_include_directories(radux-menu PRIVATE
    ${GTKMM_INCLUDE_DIRS}
    ${YAML_CPP_INCLUDE_DIRS}
)

# Compiler flags
target_compile_options(radux-menu PRIVATE
    ${GTKMM_CFLAGS_OTHER}
    ${YAML_CPP_CFLAGS_OTHER}
    -Wall -Wextra -O3 -flto
)

# Install target (to ../bin)
install(TARGETS radux-menu DESTINATION ../bin)
