#pragma once

#include <iostream>
#include <string>
#include <sstream>

// Debug logging utilities
// Only logs debug messages if DEBUG mode is enabled
class DebugLog {
public:
    // Enable or disable debug logging
    static void set_enabled(bool enabled) {
        enabled_ = enabled;
    }

    // Check if debug logging is enabled
    static bool is_enabled() {
        return enabled_;
    }

    // Stream-based debug logger
    class Logger {
    public:
        Logger(bool newline = false) : newline_(newline) {}

        ~Logger() {
            if (DebugLog::enabled_) {
                if (newline_) {
                    std::cerr << "\n";
                }
                std::cerr << std::flush;
            }
        }

        template<typename T>
        Logger& operator<<(const T& val) {
            if (DebugLog::enabled_) {
                std::cerr << val;
            }
            return *this;
        }

    private:
        bool newline_;
    };

private:
    static bool enabled_;
};

// Initialize static member
inline bool DebugLog::enabled_ = false;

// Convenience macros for debug logging with stream syntax
#define DEBUG_LOG DebugLog::Logger(false)
#define DEBUG_LOGLN DebugLog::Logger(true)

// Always log errors regardless of debug mode
#define ERROR_LOG(msg) std::cerr << "ERROR: " << msg << std::endl
#define SECURITY_LOG(msg) std::cerr << "SECURITY: " << msg << std::endl
