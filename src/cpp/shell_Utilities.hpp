#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

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

// Safe command executor using fork+execve (NO shell involved)
class SafeExecutor {
public:
    // Execute a command with arguments safely using execve (no shell interpretation)
    static CommandResult execute(const std::string& command, const std::vector<std::string>& args = {}) {
        CommandResult result;

        // Create pipes for stdout and stderr
        int stdout_pipe[2];
        int stderr_pipe[2];

        if (pipe(stdout_pipe) == -1 || pipe(stderr_pipe) == -1) {
            result.exit_code = -1;
            result.stderr = "Failed to create pipes";
            return result;
        }

        pid_t pid = fork();

        if (pid == -1) {
            // Fork failed
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            close(stderr_pipe[0]);
            close(stderr_pipe[1]);
            result.exit_code = -1;
            result.stderr = "Failed to fork process";
            return result;
        }

        if (pid == 0) {
            // Child process
            // Close unused pipe ends
            close(stdout_pipe[0]);
            close(stderr_pipe[0]);

            // Redirect stdout and stderr
            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stderr_pipe[1], STDERR_FILENO);
            close(stdout_pipe[1]);
            close(stderr_pipe[1]);

            // Build argv array for execve
            std::vector<char*> argv;
            std::string cmd_copy = command;

            argv.push_back(const_cast<char*>(cmd_copy.c_str()));

            // Add arguments
            std::vector<std::string> args_copy = args;
            for (auto& arg : args_copy) {
                argv.push_back(const_cast<char*>(arg.c_str()));
            }
            argv.push_back(nullptr);

            // Execute command (no shell involved)
            execve(command.c_str(), argv.data(), environ);

            // If execve returns, it failed
            _exit(127);
        }

        // Parent process
        close(stdout_pipe[1]);
        close(stderr_pipe[1]);

        // Read output
        char buffer[4096];
        ssize_t bytes_read;

        // Read stdout
        while ((bytes_read = read(stdout_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            result.stdout += buffer;
        }
        close(stdout_pipe[0]);

        // Read stderr
        while ((bytes_read = read(stderr_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytes_read] = '\0';
            result.stderr += buffer;
        }
        close(stderr_pipe[0]);

        // Wait for child process
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            result.exit_code = WEXITSTATUS(status);
        } else {
            result.exit_code = -1;
        }

        result.success = (result.exit_code == 0);

        return result;
    }

    // Execute command asynchronously using fork+execve (fire and forget, no shell)
    static bool execute_async(const std::string& command, const std::vector<std::string>& args = {}) {
        pid_t pid = fork();

        if (pid == -1) {
            return false;
        }

        if (pid == 0) {
            // Child process

            // Double fork to avoid zombie processes
            pid_t grandchild = fork();

            if (grandchild == -1) {
                _exit(1);
            }

            if (grandchild == 0) {
                // Grandchild process
                // Build argv array for execve
                std::vector<char*> argv;
                std::string cmd_copy = command;

                argv.push_back(const_cast<char*>(cmd_copy.c_str()));

                // Add arguments
                std::vector<std::string> args_copy = args;
                for (auto& arg : args_copy) {
                    argv.push_back(const_cast<char*>(arg.c_str()));
                }
                argv.push_back(nullptr);

                // Redirect stdin/stdout/stderr to /dev/null
                int devnull = open("/dev/null", O_RDWR);
                if (devnull != -1) {
                    dup2(devnull, STDIN_FILENO);
                    dup2(devnull, STDOUT_FILENO);
                    dup2(devnull, STDERR_FILENO);
                    close(devnull);
                }

                // Execute command (no shell involved)
                execve(command.c_str(), argv.data(), environ);

                // If execve returns, it failed
                _exit(127);
            }

            // Child exits immediately
            _exit(0);
        }

        // Parent process
        int status;
        waitpid(pid, &status, 0);

        return WIFEXITED(status) && WEXITSTATUS(status) == 0;
    }
};
