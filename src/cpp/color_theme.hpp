#pragma once

#include <string>
#include <optional>
#include <cairomm/cairomm.h>

// Forward declaration for YAML
namespace YAML {
    class Node;
}

struct Color {
    double r, g, b, a;

    // Default: transparent black (unset state)
    Color() : r(0), g(0), b(0), a(0) {}

    // From RGB values (0-255)
    static Color from_rgb(int red, int green, int blue, int alpha = 255) {
        Color c;
        c.r = red / 255.0;
        c.g = green / 255.0;
        c.b = blue / 255.0;
        c.a = alpha / 255.0;
        return c;
    }

    // From hex string (#RRGGBB or #RRGGBBAA)
    static Color from_hex(const std::string& hex);

    // Convert to RGB tuple for Cairo
    void set_as_source(const Cairo::RefPtr<Cairo::Context>& cr) const;

    // Check if color is set (non-transparent)
    bool is_set() const { return a > 0.0; }
};

struct Theme {
    Color background_color;     // Button background
    Color hover_color;          // Hovered button
    Color border_color;         // Button borders
    Color font_color;           // Text color
    Color center_color;         // Center circle
    int font_size = 14;

    // Default constructor with standard colors
    Theme()
        : font_size(14)
    {
        // Set default colors matching the original hardcoded values
        background_color = Color::from_rgb(34, 34, 34);  // rgba(0.2, 0.2, 0.2, 0.85)
        background_color.a = 0.85;
        hover_color = Color::from_rgb(76, 128, 204);     // rgba(0.3, 0.5, 0.8, 0.9)
        hover_color.a = 0.9;
        border_color = Color::from_rgb(230, 230, 230);   // rgba(0.9, 0.9, 0.9, 0.9)
        border_color.a = 0.9;
        font_color = Color::from_rgb(255, 255, 255);     // White
        center_color = Color::from_rgb(38, 38, 38);      // rgba(0.15, 0.15, 0.15, 0.9)
        center_color.a = 0.9;
    }

    // Parse from YAML node
    static Theme from_yaml(const YAML::Node& node);

    // Merge: child theme overrides parent values that are set
    Theme inherit_from(const Theme& parent) const;
};
