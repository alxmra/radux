#include "color_theme.hpp"
#include <yaml-cpp/yaml.h>
#include <sstream>
#include <algorithm>
#include <cmath>

Color Color::from_hex(const std::string& hex) {
    Color c;
    c.a = 1.0; // Default opaque

    std::string h = hex;
    // Remove # if present
    if (!h.empty() && h[0] == '#') {
        h = h.substr(1);
    }

    // Parse #RRGGBB or #RRGGBBAA
    if (h.length() >= 6) {
        try {
            unsigned int value;
            std::stringstream ss;

            // Red
            ss << std::hex << h.substr(0, 2);
            ss >> value;
            c.r = value / 255.0;
            ss.clear();

            // Green
            ss << std::hex << h.substr(2, 2);
            ss >> value;
            c.g = value / 255.0;
            ss.clear();

            // Blue
            ss << std::hex << h.substr(4, 2);
            ss >> value;
            c.b = value / 255.0;
            ss.clear();

            // Alpha (optional)
            if (h.length() >= 8) {
                ss << std::hex << h.substr(6, 2);
                ss >> value;
                c.a = value / 255.0;
            }
        } catch (...) {
            // If parsing fails, return transparent (unset)
            c.a = 0.0;
        }
    }

    return c;
}

void Color::set_as_source(const Cairo::RefPtr<Cairo::Context>& cr) const {
    cr->set_source_rgba(r, g, b, a);
}

Theme Theme::from_yaml(const YAML::Node& node) {
    Theme theme; // Starts with default colors

    if (node["background-color"]) {
        theme.background_color = Color::from_hex(node["background-color"].as<std::string>());
    }
    if (node["hover-color"]) {
        theme.hover_color = Color::from_hex(node["hover-color"].as<std::string>());
    }
    if (node["border-color"]) {
        theme.border_color = Color::from_hex(node["border-color"].as<std::string>());
    }
    if (node["font-color"]) {
        theme.font_color = Color::from_hex(node["font-color"].as<std::string>());
    }
    if (node["center-color"]) {
        theme.center_color = Color::from_hex(node["center-color"].as<std::string>());
    }
    if (node["font-size"]) {
        theme.font_size = node["font-size"].as<int>();
    }

    return theme;
}

Theme Theme::inherit_from(const Theme& parent) const {
    Theme result = *this;

    if (!result.background_color.is_set()) {
        result.background_color = parent.background_color;
    }
    if (!result.hover_color.is_set()) {
        result.hover_color = parent.hover_color;
    }
    if (!result.border_color.is_set()) {
        result.border_color = parent.border_color;
    }
    if (!result.font_color.is_set()) {
        result.font_color = parent.font_color;
    }
    if (!result.center_color.is_set()) {
        result.center_color = parent.center_color;
    }
    if (result.font_size == 14 && parent.font_size != 14) {
        result.font_size = parent.font_size;
    }

    return result;
}
