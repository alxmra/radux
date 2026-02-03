#include "platform_Utilities.hpp"
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <iostream>
#include <cstdio>
#include <cstring>

// X11 headers (only included when needed)
#ifdef HAS_X11
#include <X11/Xlib.h>
#endif

// Check if we're running on Wayland
static bool detect_wayland() {
    const char* wayland_display = g_getenv("WAYLAND_DISPLAY");
    if (wayland_display && wayland_display[0] != '\0') {
        return true;
    }

    // Also check GDK backend
    const char* gdk_backend = g_getenv("GDK_BACKEND");
    if (gdk_backend && strcmp(gdk_backend, "wayland") == 0) {
        return true;
    }

    return false;
}

// X11 Backend Implementation
#ifdef HAS_X11

#include <X11/Xlib.h>
#include <stdexcept>

struct X11DisplayWrapper {
    Display* display_;

    X11DisplayWrapper() : display_(nullptr) {
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            throw std::runtime_error("Failed to open X11 display");
        }
    }

    ~X11DisplayWrapper() {
        if (display_) {
            XCloseDisplay(display_);
        }
    }

    int screen() const { return DefaultScreen(display_); }
    Window root_window() const { return RootWindow(display_, screen()); }

    void get_screen_geometry(int& width, int& height) const {
        width = DisplayWidth(display_, screen());
        height = DisplayHeight(display_, screen());
    }

    void warp_pointer(int x, int y) {
        XWarpPointer(display_, None, root_window(), 0, 0, 0, 0, x, y);
        XFlush(display_);
    }

    void get_pointer_position(int& x, int& y) const {
        Window root, child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(display_, root_window(), &root, &child,
                      &root_x, &root_y, &win_x, &win_y, &mask);
        x = root_x;
        y = root_y;
    }
};

X11DisplayBackend::X11DisplayBackend() {
    try {
        display_ = nullptr;
    } catch (...) {
        display_ = nullptr;
    }
}

X11DisplayBackend::~X11DisplayBackend() {
    if (display_) {
        delete static_cast<X11DisplayWrapper*>(display_);
    }
}

bool X11DisplayBackend::get_screen_geometry(int& width, int& height) {
    if (!display_) {
        try {
            display_ = new X11DisplayWrapper();
        } catch (...) {
            return false;
        }
    }
    static_cast<X11DisplayWrapper*>(display_)->get_screen_geometry(width, height);
    return true;
}

bool X11DisplayBackend::get_pointer_position(int& x, int& y) {
    if (!display_) {
        try {
            display_ = new X11DisplayWrapper();
        } catch (...) {
            return false;
        }
    }
    static_cast<X11DisplayWrapper*>(display_)->get_pointer_position(x, y);
    return true;
}

bool X11DisplayBackend::warp_pointer(int x, int y) {
    if (!display_) {
        try {
            display_ = new X11DisplayWrapper();
        } catch (...) {
            return false;
        }
    }
    static_cast<X11DisplayWrapper*>(display_)->warp_pointer(x, y);
    return true;
}

#else // !HAS_X11

X11DisplayBackend::X11DisplayBackend() : display_(nullptr) {}
X11DisplayBackend::~X11DisplayBackend() {}
bool X11DisplayBackend::get_screen_geometry(int&, int&) { return false; }
bool X11DisplayBackend::get_pointer_position(int&, int&) { return false; }
bool X11DisplayBackend::warp_pointer(int, int) { return false; }

#endif // HAS_X11

// PlatformDisplay Implementation
PlatformDisplay::PlatformDisplay()
    : is_wayland_(false), gdk_display_(nullptr) {
    // Detect Wayland
    is_wayland_ = detect_wayland();

    // Only initialize X11 backend if not on Wayland
    if (!is_wayland_) {
#ifdef HAS_X11
        x11_backend_ = std::make_unique<X11DisplayBackend>();
#endif
    }
}

PlatformDisplay::~PlatformDisplay() = default;

bool PlatformDisplay::get_screen_geometry(int& width, int& height) const {
    // Try GTK4/GDK APIs first (work on both X11 and Wayland)
    GdkDisplay* display = gdk_display_get_default();
    if (display) {
        // GTK4: Get the list of monitors using gdk_display_get_monitors()
        GListModel* monitors = G_LIST_MODEL(gdk_display_get_monitors(display));
        if (monitors) {
            unsigned int n_monitors = g_list_model_get_n_items(monitors);
            if (n_monitors > 0) {
                // Get the first (default) monitor
                GdkMonitor* default_monitor = GDK_MONITOR(g_list_model_get_item(monitors, 0));
                if (default_monitor) {
                    GdkRectangle geometry;
                    gdk_monitor_get_geometry(default_monitor, &geometry);
                    width = geometry.width;
                    height = geometry.height;
                    g_object_unref(default_monitor);
                    return true;
                }
            }
        }
    }

    // Fallback: use xdotool (X11 only)
    FILE* pipe = popen("/usr/bin/xdotool getdisplaygeometry 2>/dev/null", "r");
    if (pipe) {
        char buffer[64];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            int w = 0, h = 0;
            if (sscanf(buffer, "%d %d", &w, &h) == 2) {
                width = w;
                height = h;
                pclose(pipe);
                return true;
            }
        }
        pclose(pipe);
    }

    // Last resort: X11 backend
    if (x11_backend_) {
        return x11_backend_->get_screen_geometry(width, height);
    }

    return false;
}

bool PlatformDisplay::get_pointer_position(int& x, int& y) const {
    // Try xdotool first (simple, works on X11)
    FILE* pipe = popen("/usr/bin/xdotool getmouselocation --shell 2>/dev/null", "r");
    if (pipe) {
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            int mouse_x = 0, mouse_y = 0;
            if (sscanf(buffer, "X=%d Y=%d", &mouse_x, &mouse_y) == 2) {
                x = mouse_x;
                y = mouse_y;
                pclose(pipe);
                return true;
            }
        }
        pclose(pipe);
    }

    // Fallback: X11 backend
    if (x11_backend_) {
        return x11_backend_->get_pointer_position(x, y);
    }

    return false;
}

bool PlatformDisplay::warp_pointer(int x, int y) {
    // Wayland does NOT allow warping the pointer for security reasons
    if (is_wayland_) {
        return false;
    }

    // On X11, use the X11 backend
    if (x11_backend_) {
        return x11_backend_->warp_pointer(x, y);
    }

    return false;
}
