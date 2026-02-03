#pragma once

#include <string>
#include <vector>
#include "menu_item.hpp"
#include "color_theme.hpp"

// Forward declaration for YAML
namespace YAML {
    class Node;
}

// Forward declaration for blacklist
class CommandBlacklist;

// Security limits for YAML parsing
namespace SecurityLimits {
    constexpr size_t MAX_CONFIG_FILE_SIZE = 1024 * 1024;  // 1MB
    constexpr int MAX_YAML_DEPTH = 10;                      // Maximum nesting depth
    constexpr size_t MAX_MENU_ITEMS = 50;                   // Maximum items per menu
    constexpr size_t MAX_TOTAL_ITEMS = 200;                 // Maximum total items across all menus
}

// Path validation utilities
class PathValidator {
public:
    // Check if a config file path is allowed
    // Allowed paths: ~/.config/radux/ and ./ (relative to cwd)
    static bool is_config_path_allowed(const std::string& filepath);

    // Normalize a path (resolve . and ..)
    static std::string normalize_path(const std::string& path);

private:
    // Check if path is within allowed directory
    static bool is_in_directory(const std::string& path, const std::string& directory);

    // Get home directory
    static std::string get_home_directory();
};

class RadialConfig {
public:
    // Geometry
    int radius = 120;
    int center_radius = 40;

    // Items
    std::vector<MenuItem> items;

    // Theme
    Theme theme;

    // Animation
    int animation_speed_ms = 500;

    // Auto-close
    int auto_close_milliseconds = 0; // 0 = disabled

    // Load from YAML file
    static RadialConfig from_yaml(const std::string& filepath);

    // Parse from command-line string
    // Format: "title:description:action;title2:desc2:act2;..."
    static RadialConfig from_command_line(const std::string& cli_string);

    // Validate configuration
    bool validate() const;

private:
    // Helper to parse single item from CLI string
    static MenuItem parse_cli_item(const std::string& item_str);

    // Helper to parse menu item from YAML node (with depth tracking)
    static MenuItem parse_menu_item(const YAML::Node& node, const Theme& parent_theme, int depth = 0);

    // Validate commands in menu items (recursive for submenus)
    bool validate_item_commands(const MenuItem& item, CommandBlacklist& blacklist) const;

    // Count total items recursively
    size_t count_total_items() const;

    // Helper to count items recursively
    size_t count_items_recursive(const MenuItem& item) const;
};
