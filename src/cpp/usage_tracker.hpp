#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <optional>

// Usage tracking system - stub implementation for now
// Will be fully implemented with JSON persistence later

struct UsageEntry {
    std::string menu_path;
    int count = 0;
};

class UsageTracker {
public:
    static constexpr int HIGHLIGHT_THRESHOLD = 10;

    bool load(const std::string& filepath) { return true; } // Stub
    bool save(const std::string& filepath) { return true; } // Stub

    void record_usage(const std::string& item_label, const std::vector<std::string>& menu_path) {
        // Stub - will track usage statistics
    }

    std::optional<std::string> get_most_used_root_item() const {
        return std::nullopt; // Stub
    }

    void clear_highlight() {}
    bool should_highlight(const std::string& item_label) const { return false; }

private:
    std::unordered_map<std::string, UsageEntry> usage_data_;
    std::optional<std::string> most_used_root_item_;
};
