#include "radial_menu.hpp"
#include "config_loader.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>

// PID file path for single-instance enforcement
static const char* PID_FILE = "/tmp/radux-menu.pid";

// Kill existing radux-menu instance gracefully using PID file
static bool kill_existing_instance() {
    // Try to read the PID file
    std::ifstream pid_file(PID_FILE);
    if (!pid_file.is_open()) {
        return false;  // No PID file means no existing instance
    }

    pid_t old_pid;
    pid_file >> old_pid;
    pid_file.close();

    if (old_pid <= 0) {
        std::remove(PID_FILE);
        return false;
    }

    // Check if the process is actually running
    if (kill(old_pid, 0) != 0) {
        // Process doesn't exist, clean up stale PID file
        std::remove(PID_FILE);
        return false;
    }

    std::cerr << "Killing existing radux-menu instance (PID " << old_pid << ")...\n";

    // Send SIGTERM for graceful shutdown
    if (kill(old_pid, SIGTERM) == 0) {
        // Wait for process to exit gracefully
        for (int i = 0; i < 10; i++) {
            usleep(50000);  // 50ms
            if (kill(old_pid, 0) != 0) {
                // Process has exited
                std::cerr << "Existing instance terminated.\n";
                std::remove(PID_FILE);
                return true;
            }
        }

        // Still running after 500ms, force kill
        std::cerr << "Instance did not exit gracefully, forcing...\n";
        kill(old_pid, SIGKILL);
        usleep(100000);  // 100ms for cleanup
        std::remove(PID_FILE);
    }

    return true;
}

// Write PID file for current instance
static bool write_pid_file() {
    std::ofstream pid_file(PID_FILE);
    if (!pid_file.is_open()) {
        std::cerr << "Warning: Could not create PID file\n";
        return false;
    }

    pid_file << getpid() << std::endl;
    pid_file.close();

    // Set PID file to be deleted on exit
    // (Note: this may not work on all systems, so we also clean it up on shutdown)
    return true;
}

// Helper to find config file in standard locations
static std::string find_config_file() {
    // Check primary location: ~/.config/radux/config.yaml
    const char* home = std::getenv("HOME");
    if (home) {
        std::string primary_config = std::string(home) + "/.config/radux/config.yaml";
        std::ifstream f(primary_config);
        if (f.good()) {
            return primary_config;
        }
    }

    // Check fallback: ./config.yaml
    std::ifstream f2("config.yaml");
    if (f2.good()) {
        return "config.yaml";
    }

    return ""; // No config found
}

// Helper to get mouse position using xdotool
static bool get_mouse_position(int& x, int& y) {
    // Use full path to xdotool
    FILE* pipe = popen("/usr/bin/xdotool getmouselocation --shell 2>/dev/null", "r");
    if (!pipe) {
        std::cerr << "Failed to run xdotool\n";
        return false;
    }

    char buffer[256];
    // Read entire output
    size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, pipe);
    buffer[bytes_read] = '\0';
    pclose(pipe);

    // Parse output - might be multiline or single line
    int mouse_x = 0, mouse_y = 0;
    if (sscanf(buffer, "X=%d Y=%d", &mouse_x, &mouse_y) == 2) {
        x = mouse_x;
        y = mouse_y;
        return true;
    }

    std::cerr << "Failed to parse xdotool output: " << buffer << "\n";
    return false;
}

// Global variables for communication between main and signal handler
static int g_x = 0;
static int g_y = 0;
static RadialConfig g_config;
static RadialMenu* g_window = nullptr;

class RadialApplication : public Gtk::Application {
public:
    RadialApplication() : Gtk::Application("com.github.raduxmenu", Gio::Application::Flags::NONE) {}

protected:
    void on_activate() override {
        // Get mouse position if coordinates not provided
        if (g_x == 0 && g_y == 0) {
            if (get_mouse_position(g_x, g_y)) {
                std::cout << "Mouse position: " << g_x << ", " << g_y << "\n";
            } else {
                std::cerr << "Warning: Could not get mouse position, using screen center\n";
            }
        }

        // Create the radial menu window (it's a Gtk::Window subclass)
        g_window = new RadialMenu(g_config);
        // Don't call set_application - it's handled automatically when we use add_window()

        // Add window to application (this sets the application internally)
        add_window(*g_window);

        // Position and show
        if (g_x != 0 || g_y != 0) {
            g_window->present_at(g_x, g_y);
        } else {
            g_window->present();
        }
    }

    void on_shutdown() override {
        // Clean up PID file
        std::remove(PID_FILE);

        delete g_window;
        g_window = nullptr;
        Gtk::Application::on_shutdown();
    }
};

int main(int argc, char** argv) {
    // Kill any existing radux-menu instance before starting
    kill_existing_instance();

    // Write our PID file
    write_pid_file();

    // Parse command line arguments
    std::string config_file;
    std::string cli_config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--cli" && i + 1 < argc) {
            cli_config = argv[++i];
        } else if (arg == "--config" && i + 1 < argc) {
            config_file = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [x] [y] [OPTIONS]\n"
                      << "\n"
                      << "Positional arguments:\n"
                      << "  x       X coordinate (default: mouse position)\n"
                      << "  y       Y coordinate (default: mouse position)\n"
                      << "\n"
                      << "Options:\n"
                      << "  --cli <config>    Override config with CLI string\n"
                      << "                    Format: \"title:description:action;title2:desc2:act2;...\"\n"
                      << "  --config <file>   Use custom YAML config file\n"
                      << "  --help, -h        Show this help message\n"
                      << "\n"
                      << "Config file search order:\n"
                      << "  1. ~/.config/radux/config.yaml\n"
                      << "  2. ./config.yaml (current directory)\n"
                      << "\n"
                      << "If no x,y coordinates are provided, the menu will appear at the mouse position.\n";
            return 0;
        } else if (i == 1) {
            // First positional argument - try to parse as x coordinate
            try {
                g_x = std::stoi(arg);
            } catch (...) {
                std::cerr << "Invalid x coordinate: " << arg << "\n";
                return 1;
            }
        } else if (i == 2) {
            // Second positional argument - try to parse as y coordinate
            try {
                g_y = std::stoi(arg);
            } catch (...) {
                std::cerr << "Invalid y coordinate: " << arg << "\n";
                return 1;
            }
        }
    }

    // Load configuration
    if (!cli_config.empty()) {
        // CLI override takes priority
        g_config = RadialConfig::from_command_line(cli_config);
        std::cout << "Using CLI configuration\n";
    } else if (!config_file.empty()) {
        // Use specified config file
        g_config = RadialConfig::from_yaml(config_file);
        std::cout << "Using config file: " << config_file << "\n";
    } else {
        // Auto-detect config file
        std::string detected = find_config_file();
        if (!detected.empty()) {
            g_config = RadialConfig::from_yaml(detected);
            std::cout << "Using config file: " << detected << "\n";
        } else {
            std::cerr << "No config file found. Tried:\n"
                      << "  - ~/.config/radux/config.yaml\n"
                      << "  - ./config.yaml\n"
                      << "Use --config <file> to specify a config file.\n";
            return 1;
        }
    }

    // Validate configuration
    if (!g_config.validate()) {
        std::cerr << "Invalid configuration\n";
        return 1;
    }

    // Create and run application
    RadialApplication app;
    return app.run(argc, argv);
}
