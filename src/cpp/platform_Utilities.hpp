#pragma once

#include <string>
#include <optional>
#include <memory>

// Platform abstraction layer for display operations
// Works on both X11 and Wayland via GTK4/GDK APIs
class PlatformDisplay {
public:
    PlatformDisplay();
    ~PlatformDisplay();

    // Prevent copying
    PlatformDisplay(const PlatformDisplay&) = delete;
    PlatformDisplay& operator=(const PlatformDisplay&) = delete;

    // Get screen dimensions (works on both X11 and Wayland)
    bool get_screen_geometry(int& width, int& height) const;

    // Get mouse pointer position (works on both X11 and Wayland)
    bool get_pointer_position(int& x, int& y) const;

    // Move mouse pointer (X11 only, returns false on Wayland)
    // Note: Wayland does not allow applications to warp the pointer for security reasons
    bool warp_pointer(int x, int y);

    // Check if running on Wayland
    bool is_wayland() const { return is_wayland_; }

private:
    bool is_wayland_;
    void* gdk_display_;  // GdkDisplay* (opaque to avoid including GDK headers in header file)

    // X11-specific (only used when not on Wayland)
    std::unique_ptr<class X11DisplayBackend> x11_backend_;
};

// X11 backend implementation (only used on X11)
class X11DisplayBackend {
public:
    X11DisplayBackend();
    ~X11DisplayBackend();

    bool get_screen_geometry(int& width, int& height);
    bool get_pointer_position(int& x, int& y);
    bool warp_pointer(int x, int y);

private:
    void* display_;  // Display*
};
