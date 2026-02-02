#pragma once

#include <string>
#include <vector>
#include "menu_item.hpp"
#include "color_theme.hpp"

// Forward declaration for YAML
namespace YAML {
    class Node;
}

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

    // Helper to parse menu item from YAML node
    static MenuItem parse_menu_item(const YAML::Node& node, const Theme& parent_theme);
};
