#pragma once

#include <string>
#include <optional>
#include <vector>
#include <unordered_map>
#include <gtkmm.h>
#include "menu_item.hpp"

// Hotkey system - stub implementation for now
// Will be fully implemented in the next phase

struct Hotkey {
    std::string combo;
    guint keyval;
    Gdk::ModifierType modifiers;

    static Hotkey from_string(const std::string& str);
    bool matches(guint keyval, Gdk::ModifierType state) const;
};

class HotkeyManager {
public:
    void build_map(const std::vector<MenuItem>& items);
    std::optional<size_t> find_item(guint keyval, Gdk::ModifierType state) const;
    void clear();
    std::string get_hotkey_for_item(size_t index) const;

private:
    std::unordered_map<std::string, size_t> hotkey_map_;
    std::vector<std::optional<Hotkey>> item_hotkeys_;
};
