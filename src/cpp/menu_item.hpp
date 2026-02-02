#pragma once

#include <string>
#include <vector>
#include <optional>
#include "color_theme.hpp"

struct MenuItem {
    // Basic properties
    std::string label;
    std::string description;
    std::string command;
    std::vector<MenuItem> submenu;

    // Visual enhancements
    std::optional<std::string> icon;           // Path to .svg file
    std::optional<Theme> theme_override;       // Custom colors for this item
    int priority = 0;                          // 0-10, affects button size

    // Interaction
    std::optional<std::string> hotkey;         // e.g., "Ctrl+1"
    bool notify = false;                       // Send stdout to notify-send

    // Default constructor
    MenuItem() = default;

    // Constructor for leaf items (execute command)
    MenuItem(const std::string& label,
             const std::string& command,
             const std::string& description = "")
        : label(label)
        , description(description)
        , command(command)
        , submenu()
        , priority(0)
        , notify(false)
    {}

    // Constructor for submenu items (open nested menu)
    MenuItem(const std::string& label,
             const std::vector<MenuItem>& submenu,
             const std::string& description = "")
        : label(label)
        , description(description)
        , command()
        , submenu(submenu)
        , priority(0)
        , notify(false)
    {}

    // Check if this item has a submenu
    bool has_submenu() const {
        return !submenu.empty();
    }

    // Check if this is a valid item
    bool is_valid() const {
        return !label.empty();
    }

    // Check if item has an icon
    bool has_icon() const {
        return icon.has_value() && !icon->empty();
    }

    // Get effective theme (inherit from parent if needed)
    Theme get_effective_theme(const Theme& parent) const {
        if (theme_override) {
            return theme_override->inherit_from(parent);
        }
        return parent;
    }
};
