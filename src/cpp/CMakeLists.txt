cmake_minimum_required(VERSION 3.16)
project(radux-menu VERSION 1.0.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED gtkmm-4.0)
pkg_check_modules(YAML_CPP REQUIRED yaml-cpp)

# Find X11 (optional - for X11-specific features like pointer warping)
find_package(X11)

# Source files
set(SOURCES
    main.cpp
    radial_menu.cpp
    config_loader.cpp
    color_theme.cpp
    hotkey_manager.cpp
    usage_tracker.cpp
    platform_Utilities.cpp
)

set(HEADERS
    radial_menu.hpp
    config_loader.hpp
    menu_item.hpp
    color_theme.hpp
    hotkey_manager.hpp
    usage_tracker.hpp
    command_blacklist.hpp
    shell_Utilities.hpp
    platform_Utilities.hpp
    debug.hpp
)

# Create executable
add_executable(radux-menu ${SOURCES} ${HEADERS})

# Link libraries
target_link_libraries(radux-menu
    ${GTKMM_LIBRARIES}
    ${YAML_CPP_LIBRARIES}
)

# Link X11 if available (for pointer warping on X11)
if(X11_FOUND)
    target_link_libraries(radux-menu X11)
    target_compile_definitions(radux-menu PRIVATE HAS_X11=1)
    message(STATUS "X11 found - pointer warping support enabled")
else()
    message(STATUS "X11 not found - building for Wayland-only (pointer warping disabled)")
endif()

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
