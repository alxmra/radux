#include "config_loader.hpp"
#include <yaml-cpp/yaml.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <filesystem>

RadialConfig RadialConfig::from_yaml(const std::string& filepath) {
    RadialConfig config;

    try {
        YAML::Node yaml_config = YAML::LoadFile(filepath);

        // Read radius
        if (yaml_config["radius"]) {
            config.radius = yaml_config["radius"].as<int>();
        }

        // Read center_radius (supports both inner-radius and center_radius)
        if (yaml_config["inner-radius"]) {
            config.center_radius = yaml_config["inner-radius"].as<int>();
        } else if (yaml_config["center_radius"]) {
            config.center_radius = yaml_config["center_radius"].as<int>();
        }

        // Parse theme (global colors)
        if (yaml_config["hover-color"] || yaml_config["background-color"] ||
            yaml_config["border-color"] || yaml_config["font-color"] ||
            yaml_config["center-color"] || yaml_config["font-size"]) {
            config.theme = Theme::from_yaml(yaml_config);
        }

        // Parse animation speed
        if (yaml_config["animation-speed"]) {
            auto speed = yaml_config["animation-speed"];
            if (speed.IsScalar()) {
                std::string speed_str = speed.as<std::string>();
                // Check if it ends with 'x' (multiplier) or is a number (milliseconds)
                if (!speed_str.empty() && (speed_str.back() == 'x' || speed_str.back() == 'X')) {
                    // Multiplier format: "1.5x"
                    try {
                        double mult = std::stod(speed_str.substr(0, speed_str.length() - 1));
                        config.animation_speed_ms = static_cast<int>(500 * mult);
                    } catch (...) {
                        // Keep default
                    }
                } else {
                    // Milliseconds format: "500"
                    try {
                        config.animation_speed_ms = std::stoi(speed_str);
                    } catch (...) {
                        // Keep default
                    }
                }
            }
        }

        // Parse auto-close
        if (yaml_config["auto-close-milliseconds"]) {
            config.auto_close_milliseconds = yaml_config["auto-close-milliseconds"].as<int>();
        }

        // Read items
        if (yaml_config["items"]) {
            for (const auto& item : yaml_config["items"]) {
                MenuItem menu_item = parse_menu_item(item, config.theme);
                if (menu_item.is_valid()) {
                    config.items.push_back(menu_item);
                }
            }
        }
    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading YAML: " << e.what() << "\n";
    }

    return config;
}

MenuItem RadialConfig::parse_menu_item(const YAML::Node& node, const Theme& parent_theme) {
    MenuItem item;

    if (!node["label"]) {
        std::cerr << "Warning: Item missing label, skipping\n";
        return item;
    }

    item.label = node["label"].as<std::string>();
    item.command = node["command"] ? node["command"].as<std::string>() : "";
    item.description = node["description"] ? node["description"].as<std::string>() : "";

    // Parse icon
    if (node["icon"]) {
        item.icon = node["icon"].as<std::string>();
    }

    // Parse hotkey
    if (node["hotkey"]) {
        item.hotkey = node["hotkey"].as<std::string>();
    }

    // Parse priority (clamp to 0-10)
    if (node["priority"]) {
        int pri = node["priority"].as<int>();
        item.priority = std::clamp(pri, 0, 10);
    }

    // Parse notify
    if (node["notify"]) {
        item.notify = node["notify"].as<bool>();
    }

    // Parse theme override (item-level colors)
    if (node["background-color"] || node["hover-color"] ||
        node["border-color"] || node["font-color"]) {
        item.theme_override = Theme::from_yaml(node);
    }

    // Handle color-inheritance for submenus
    if (node["submenu"]) {
        // Get effective theme for this submenu (inherits from parent if item has colors)
        Theme effective_theme = item.get_effective_theme(parent_theme);

        for (const auto& sub : node["submenu"]) {
            MenuItem subitem = parse_menu_item(sub, effective_theme);
            if (subitem.is_valid()) {
                item.submenu.push_back(subitem);
            }
        }
    } else {
        // Leaf item - must have command
        if (item.command.empty()) {
            std::cerr << "Warning: Item '" << item.label << "' missing command, skipping\n";
            return MenuItem(); // Return invalid item
        }
    }

    return item;
}

RadialConfig RadialConfig::from_command_line(const std::string& cli_string) {
    RadialConfig config;
    std::vector<MenuItem> items;

    std::stringstream ss(cli_string);
    std::string item_str;

    while (std::getline(ss, item_str, ';')) {
        // Trim whitespace
        item_str.erase(0, item_str.find_first_not_of(" \t"));
        item_str.erase(item_str.find_last_not_of(" \t") + 1);

        if (!item_str.empty()) {
            MenuItem item = parse_cli_item(item_str);
            if (item.is_valid()) {
                items.push_back(item);
            }
        }
    }

    config.items = items;
    return config;
}

MenuItem RadialConfig::parse_cli_item(const std::string& item_str) {
    // Format: "title:description:action"
    // Description is optional, can be "title::action" or "title:action"

    std::string parts[3] = {"", "", ""};
    size_t start = 0;
    int part_idx = 0;

    for (size_t i = 0; i < item_str.size() && part_idx < 3; ++i) {
        if (item_str[i] == ':' && (i == 0 || item_str[i-1] != '\\')) {
            parts[part_idx++] = item_str.substr(start, i - start);
            start = i + 1;
        }
    }
    // Last part
    if (start < item_str.size() && part_idx < 3) {
        parts[part_idx] = item_str.substr(start);
    }

    std::string label = parts[0];
    std::string description = parts[1];
    std::string command = parts[2];

    // Handle "title:action" format (no description)
    if (command.empty() && !description.empty()) {
        command = description;
        description = "";
    }

    // Use label as description if not provided
    if (description.empty()) {
        description = label;
    }

    return MenuItem(label, command, description);
}

bool RadialConfig::validate() const {
    if (radius < 1) {
        std::cerr << "Invalid radius: " << radius << "\n";
        return false;
    }

    if (center_radius < 1) {
        std::cerr << "Invalid center_radius: " << center_radius << "\n";
        return false;
    }

    if (items.empty()) {
        std::cerr << "No items configured\n";
        return false;
    }

    for (const auto& item : items) {
        if (!item.is_valid()) {
            std::cerr << "Invalid item found\n";
            return false;
        }
    }

    return true;
}
