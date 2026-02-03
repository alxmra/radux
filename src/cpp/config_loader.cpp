#include "config_loader.hpp"
#include "command_blacklist.hpp"
#include <yaml-cpp/yaml.h>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <filesystem>
#include <cstdlib>
#include <limits>

// PathValidator implementation
std::string PathValidator::get_home_directory() {
    const char* home = std::getenv("HOME");
    if (!home) {
        home = std::getenv("USERPROFILE");
    }
    return home ? home : "/";
}

std::string PathValidator::normalize_path(const std::string& path) {
    try {
        std::filesystem::path p(path);
        std::filesystem::path canonical = std::filesystem::weakly_canonical(p);
        return canonical.string();
    } catch (...) {
        return path;
    }
}

bool PathValidator::is_in_directory(const std::string& path, const std::string& directory) {
    try {
        std::filesystem::path p = normalize_path(path);
        std::filesystem::path dir = normalize_path(directory);

        // Check if path starts with directory
        auto path_str = p.string();
        auto dir_str = dir.string();

        // Ensure directory ends with separator for proper matching
        if (!dir_str.empty() && dir_str.back() != '/') {
            dir_str += '/';
        }
        if (!path_str.empty() && path_str.back() != '/' &&
            std::filesystem::is_directory(p)) {
            // For directories, add trailing slash for comparison
            path_str += '/';
        }

        return path_str.rfind(dir_str, 0) == 0;
    } catch (...) {
        return false;
    }
}

bool PathValidator::is_config_path_allowed(const std::string& filepath) {
    std::string normalized = normalize_path(filepath);

    // Allow paths in ~/.config/radux/
    std::string home = get_home_directory();
    std::string config_dir = home + "/.config/radux";

    if (is_in_directory(normalized, config_dir)) {
        return true;
    }

    // Allow paths relative to current working directory (./)
    std::string cwd = std::filesystem::current_path().string();

    if (is_in_directory(normalized, cwd)) {
        return true;
    }

    // Absolute path outside allowed directories
    std::cerr << "SECURITY ERROR: Config file path not allowed: " << filepath << "\n";
    std::cerr << "Config files must be in ~/.config/radux/ or relative to current directory.\n";
    return false;
}

RadialConfig RadialConfig::from_yaml(const std::string& filepath) {
    RadialConfig config;

    // SECURITY: Validate config file path
    if (!PathValidator::is_config_path_allowed(filepath)) {
        std::cerr << "Refusing to load config from disallowed path.\n";
        return config;
    }

    // SECURITY: Check file size before parsing
    try {
        std::filesystem::path file_path(filepath);
        if (!std::filesystem::exists(file_path)) {
            std::cerr << "Config file does not exist: " << filepath << "\n";
            return config;
        }

        size_t file_size = std::filesystem::file_size(file_path);
        if (file_size > SecurityLimits::MAX_CONFIG_FILE_SIZE) {
            std::cerr << "SECURITY ERROR: Config file too large (" << file_size << " bytes).\n";
            std::cerr << "Maximum allowed size: " << SecurityLimits::MAX_CONFIG_FILE_SIZE << " bytes.\n";
            return config;
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error accessing config file: " << e.what() << "\n";
        return config;
    }

    try {
        YAML::Node yaml_config = YAML::LoadFile(filepath);

        // Read radius with bounds checking
        if (yaml_config["radius"]) {
            int r = yaml_config["radius"].as<int>();
            config.radius = std::clamp(r, 50, 500);
        }

        // Read center_radius with bounds checking
        if (yaml_config["inner-radius"]) {
            int cr = yaml_config["inner-radius"].as<int>();
            config.center_radius = std::clamp(cr, 10, 200);
        } else if (yaml_config["center_radius"]) {
            int cr = yaml_config["center_radius"].as<int>();
            config.center_radius = std::clamp(cr, 10, 200);
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
                        config.animation_speed_ms = std::clamp(static_cast<int>(500 * mult), 100, 5000);
                    } catch (...) {
                        // Keep default
                    }
                } else {
                    // Milliseconds format: "500"
                    try {
                        config.animation_speed_ms = std::clamp(std::stoi(speed_str), 100, 5000);
                    } catch (...) {
                        // Keep default
                    }
                }
            }
        }

        // Parse auto-close
        if (yaml_config["auto-close-milliseconds"]) {
            int ac = yaml_config["auto-close-milliseconds"].as<int>();
            config.auto_close_milliseconds = std::clamp(ac, 0, 60000); // Max 60 seconds
        }

        // Read items with depth and count limits
        if (yaml_config["items"]) {
            size_t item_count = 0;
            for (const auto& item : yaml_config["items"]) {
                if (item_count >= SecurityLimits::MAX_MENU_ITEMS) {
                    std::cerr << "WARNING: Maximum menu items limit reached ("
                              << SecurityLimits::MAX_MENU_ITEMS << "). Skipping remaining items.\n";
                    break;
                }

                MenuItem menu_item = parse_menu_item(item, config.theme, 0);
                if (menu_item.is_valid()) {
                    config.items.push_back(menu_item);
                    item_count++;
                }
            }
        }

        // SECURITY: Check total item count
        if (config.count_total_items() > SecurityLimits::MAX_TOTAL_ITEMS) {
            std::cerr << "SECURITY ERROR: Total menu items exceed limit ("
                      << SecurityLimits::MAX_TOTAL_ITEMS << ").\n";
            config.items.clear();
            return config;
        }

    } catch (const YAML::Exception& e) {
        std::cerr << "Error loading YAML: " << e.what() << "\n";
    }

    return config;
}

MenuItem RadialConfig::parse_menu_item(const YAML::Node& node, const Theme& parent_theme, int depth) {
    MenuItem item;

    // SECURITY: Check recursion depth
    if (depth >= SecurityLimits::MAX_YAML_DEPTH) {
        std::cerr << "SECURITY ERROR: Maximum submenu nesting depth reached ("
                  << SecurityLimits::MAX_YAML_DEPTH << ").\n";
        return item;
    }

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

        size_t submenu_count = 0;
        for (const auto& sub : node["submenu"]) {
            if (submenu_count >= SecurityLimits::MAX_MENU_ITEMS) {
                std::cerr << "WARNING: Maximum submenu items limit reached ("
                          << SecurityLimits::MAX_MENU_ITEMS << "). Skipping remaining items.\n";
                break;
            }

            MenuItem subitem = parse_menu_item(sub, effective_theme, depth + 1);
            if (subitem.is_valid()) {
                item.submenu.push_back(subitem);
                submenu_count++;
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

    // SECURITY: Validate all commands against blacklist
    auto& blacklist = CommandBlacklist::instance();
    for (const auto& item : items) {
        if (!validate_item_commands(item, blacklist)) {
            return false;
        }
    }

    return true;
}

// Validate commands in a menu item (including submenus)
bool RadialConfig::validate_item_commands(const MenuItem& item, CommandBlacklist& blacklist) const {
    // Check submenu items recursively
    if (item.has_submenu()) {
        for (const auto& subitem : item.submenu) {
            if (!validate_item_commands(subitem, blacklist)) {
                return false;
            }
        }
    } else if (!item.command.empty()) {
        // Check if command is blacklisted
        if (blacklist.is_blacklisted(item.command)) {
            std::cerr << "SECURITY ERROR in config: " << blacklist.get_blacklisted_info(item.command) << "\n";
            std::cerr << "  Item: " << item.label << "\n";
            std::cerr << "  Command: " << item.command << "\n";
            return false;
        }

        // Check for dangerous patterns
        if (blacklist.has_dangerous_patterns(item.command)) {
            std::cerr << "SECURITY ERROR in config: " << blacklist.get_blacklisted_info(item.command) << "\n";
            std::cerr << "  Item: " << item.label << "\n";
            std::cerr << "  Command: " << item.command << "\n";
            return false;
        }
    }

    return true;
}

// Count total items recursively (including all submenus)
size_t RadialConfig::count_total_items() const {
    size_t count = 0;

    for (const auto& item : items) {
        count += count_items_recursive(item);
    }

    return count;
}

// Helper to count items recursively
size_t RadialConfig::count_items_recursive(const MenuItem& item) const {
    size_t count = 1; // Count this item

    if (item.has_submenu()) {
        for (const auto& subitem : item.submenu) {
            count += count_items_recursive(subitem);
        }
    }

    return count;
}
