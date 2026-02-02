#include "hotkey_manager.hpp"
#include <cctype>
#include <algorithm>
#include <iostream>

Hotkey Hotkey::from_string(const std::string& str) {
    Hotkey hk;
    hk.combo = str;
    hk.keyval = GDK_KEY_VoidSymbol;
    hk.modifiers = Gdk::ModifierType(0);

    std::string upper = str;
    std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);

    // Parse modifiers
    size_t pos = 0;
    while (pos < upper.length()) {
        size_t plus_pos = upper.find('+', pos);

        if (plus_pos == std::string::npos) {
            // Last part is the key
            std::string key = upper.substr(pos);
            hk.keyval = gdk_keyval_from_name(key.c_str());

            // Also try lowercase if uppercase didn't work
            if (hk.keyval == GDK_KEY_VoidSymbol || hk.keyval == 0) {
                std::string lower_key = str.substr(pos);  // Use original case for the key part
                hk.keyval = gdk_keyval_from_name(lower_key.c_str());
            }
            break;
        }

        std::string mod = upper.substr(pos, plus_pos - pos);

        if (mod == "CTRL" || mod == "CONTROL") {
            hk.modifiers |= Gdk::ModifierType::CONTROL_MASK;
        } else if (mod == "ALT") {
            hk.modifiers |= Gdk::ModifierType::ALT_MASK;
        } else if (mod == "SHIFT") {
            hk.modifiers |= Gdk::ModifierType::SHIFT_MASK;
        } else if (mod == "SUPER" || mod == "WIN" || mod == "META") {
            hk.modifiers |= Gdk::ModifierType::SUPER_MASK;
        }

        pos = plus_pos + 1;
    }

    // Normalize: ignore Caps Lock
    hk.modifiers &= ~(Gdk::ModifierType::LOCK_MASK);

    return hk;
}

bool Hotkey::matches(guint keyval, Gdk::ModifierType state) const {
    Gdk::ModifierType normalized_state = state;
    // Normalize: ignore Caps Lock
    normalized_state &= ~(Gdk::ModifierType::LOCK_MASK);

    // Direct match
    if (this->keyval == keyval && this->modifiers == normalized_state) {
        return true;
    }

    // For letter keys, try case-insensitive matching
    // Convert both to uppercase for comparison
    guint this_upper = gdk_keyval_to_upper(this->keyval);
    guint key_upper = gdk_keyval_to_upper(keyval);

    if (this_upper == key_upper && this->modifiers == normalized_state) {
        return true;
    }

    return false;
}

void HotkeyManager::build_map(const std::vector<MenuItem>& items) {
    clear();
    item_hotkeys_.resize(items.size());

    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].hotkey) {
            Hotkey hk = Hotkey::from_string(*items[i].hotkey);
            hotkey_map_[hk.combo] = i;
            item_hotkeys_[i] = hk;
            std::cerr << "HotkeyManager: Registered hotkey '" << hk.combo
                      << "' (keyval=" << hk.keyval
                      << ", modifiers=" << static_cast<int>(hk.modifiers)
                      << ") for item " << i << " (" << items[i].label << ")\n";
        }
    }
}

std::optional<size_t> HotkeyManager::find_item(guint keyval, Gdk::ModifierType state) const {
    for (const auto& [combo, index] : hotkey_map_) {
        if (index < item_hotkeys_.size() && item_hotkeys_[index]) {
            if (item_hotkeys_[index]->matches(keyval, state)) {
                return index;
            }
        }
    }
    return std::nullopt;
}

void HotkeyManager::clear() {
    hotkey_map_.clear();
    item_hotkeys_.clear();
}

std::string HotkeyManager::get_hotkey_for_item(size_t index) const {
    if (index < item_hotkeys_.size() && item_hotkeys_[index]) {
        return item_hotkeys_[index]->combo;
    }
    return "";
}
