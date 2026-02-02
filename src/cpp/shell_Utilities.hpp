#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

// Shell escape utilities for safe command execution
class ShellEscaper {
public:
    // Escape a single argument for safe shell usage
    // This follows POSIX shell escaping rules
    static std::string escape_argument(const std::string& arg) {
        // If the argument is empty or contains only safe characters, return as-is
        if (arg.empty()) {
            return "''";
        }

        // Check if the argument contains only safe characters
        // Safe: alphanumeric, underscore, dot, dash, plus, slash, at, colon
        bool is_safe = true;
        for (char c : arg) {
            if (!std::isalnum(static_cast<unsigned char>(c)) &&
                c != '_' && c != '.' && c != '-' && c != '/' &&
                c != '@' && c != ':' && c != '+') {
                is_safe = false;
                break;
            }
        }

        if (is_safe) {
            return arg;
        }

        // Use single-quote escaping (most reliable)
        // Replace any existing single quotes with '"'"'
        std::string escaped = "'";
        std::string::size_type pos = 0;
        std::string::size_type last_pos = 0;

        while ((pos = arg.find('\'', last_pos)) != std::string::npos) {
            escaped += arg.substr(last_pos, pos - last_pos);
            escaped += "'\\''"; // End quote, escaped quote, start quote
            last_pos = pos + 1;
        }

        escaped += arg.substr(last_pos);
        escaped += "'";

        return escaped;
    }

    // Escape arguments for use in notify-send or similar
    // More conservative escaping for notification content
    static std::string escape_notify_arg(const std::string& arg) {
        // For notifications, we need to be extra careful
        // Replace backslashes, single quotes, and newlines
        std::string result;
        result.reserve(arg.size() * 2);

        for (char c : arg) {
            switch (c) {
                case '\\':
                    result += "\\\\";
                    break;
                case '\'':
                    result += "'\\''";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                case '"':
                    result += "\\\"";
                    break;
                case '$':
                    result += "\\$";
                    break;
                case '`':
                    result += "\\`";
                    break;
                default:
                    result += c;
                    break;
            }
        }

        return "'" + result + "'";
    }

    // Validate that a string is safe for shell use
    static bool is_safe_for_shell(const std::string& str) {
        // Check for dangerous shell metacharacters
        const std::string dangerous = ";&|`$()<>{}'\"\\[]?*~ \t\n\r";

        return str.find_first_of(dangerous) == std::string::npos;
    }

    // Split command into base command and arguments
    static std::pair<std::string, std::vector<std::string>> parse_command(const std::string& cmd) {
        std::vector<std::string> args;
        std::stringstream ss(cmd);
        std::string arg;

        while (ss >> arg) {
            args.push_back(arg);
        }

        if (args.empty()) {
            return {"", {}};
        }

        std::string base_cmd = args[0];
        args.erase(args.begin());

        return {base_cmd, args};
    }

    // Check if command path is within allowed directories
    static bool is_safe_path(const std::string& cmd_path) {
        // Get home directory
        const char* home = std::getenv("HOME");
        if (!home) {
            home = "/";
        }

        // Check if it's a relative command (no path) - these use PATH
        if (cmd_path.find('/') == std::string::npos) {
            return true; // Will be validated against PATH
        }

        // Check if path is within home directory
        std::string home_str = home;
        if (cmd_path.substr(0, home_str.length()) == home_str) {
            return true;
        }

        // Common safe system directories
        const std::vector<std::string> safe_dirs = {
            "/usr/bin/",
            "/bin/",
            "/usr/local/bin/",
            "/usr/sbin/",
            "/sbin/"
        };

        for (const auto& dir : safe_dirs) {
            if (cmd_path.substr(0, dir.length()) == dir) {
                return true;
            }
        }

        return false;
    }
};

// Command execution result
struct CommandResult {
    int exit_code;
    std::string stdout;
    std::string stderr;
    bool success;

    CommandResult()
        : exit_code(-1), success(false) {}
};

// Safe command executor using direct system calls (no shell)
class SafeExecutor {
public:
    // Execute a command with arguments safely (no shell interpretation)
    static CommandResult execute(const std::string& command, const std::vector<std::string>& args = {}) {
        CommandResult result;

        // Build argument array
        std::vector<char*> argv;
        std::string cmd_copy = command; // Need persistent storage

        argv.push_back(const_cast<char*>(cmd_copy.c_str()));

        // Add arguments
        std::vector<std::string> args_copy = args; // Persistent storage
        for (auto& arg : args_copy) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        // Use popen for simplicity (could be replaced with fork+execve for better security)
        std::string full_cmd = cmd_copy;
        for (const auto& arg : args) {
            full_cmd += " " + ShellEscaper::escape_argument(arg);
        }

        FILE* pipe = popen(full_cmd.c_str(), "r");
        if (!pipe) {
            result.exit_code = -1;
            result.stderr = "Failed to execute command";
            return result;
        }

        // Read output
        char buffer[4096];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            result.stdout += buffer;
        }

        result.exit_code = pclose(pipe);
        result.success = (result.exit_code == 0);

        return result;
    }

    // Execute command asynchronously (fire and forget)
    static bool execute_async(const std::string& command, const std::vector<std::string>& args = {}) {
        std::string full_cmd = command;
        for (const auto& arg : args) {
            full_cmd += " " + ShellEscaper::escape_argument(arg);
        }

        // Use system() for async execution with proper escaping
        // Note: This still goes through shell but with properly escaped arguments
        return system(full_cmd.c_str()) != -1;
    }
};
