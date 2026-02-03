#pragma once

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <string>
#include <stdexcept>

// X11 utilities for native window and display operations
// Replaces dependency on xdotool
class X11Display {
public:
    X11Display() : display_(nullptr) {
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            throw std::runtime_error("Failed to open X11 display");
        }
    }

    ~X11Display() {
        if (display_) {
            XCloseDisplay(display_);
        }
    }

    // Prevent copying
    X11Display(const X11Display&) = delete;
    X11Display& operator=(const X11Display&) = delete;

    Display* get() const { return display_; }

    int screen() const { return DefaultScreen(display_); }
    Window root_window() const { return RootWindow(display_, screen()); }

    // Get screen dimensions
    void get_screen_geometry(int& width, int& height) const {
        width = DisplayWidth(display_, screen());
        height = DisplayHeight(display_, screen());
    }

    // Move mouse pointer
    void warp_pointer(int x, int y) {
        XWarpPointer(display_, None, root_window(), 0, 0, 0, 0, x, y);
        XFlush(display_);
    }

    // Get mouse pointer position
    void get_pointer_position(int& x, int& y) const {
        Window root, child;
        int root_x, root_y, win_x, win_y;
        unsigned int mask;
        XQueryPointer(display_, root_window(), &root, &child,
                      &root_x, &root_y, &win_x, &win_y, &mask);
        x = root_x;
        y = root_y;
    }

private:
    Display* display_;
};

// X11 window operations
class X11WindowOps {
public:
    explicit X11WindowOps(const X11Display& display) : display_(display) {}

    // Find window by name
    Window find_window_by_name(const std::string& name) {
        Window root = display_.root_window();
        Window result = search_windows(root, name);
        return result;
    }

    // Move window to position
    void move_window(Window window, int x, int y) {
        if (window == None) return;

        XMoveWindow(display_.get(), window, x, y);
        XFlush(display_.get());

        // Wait for window to move (sync with server)
        XSync(display_.get(), False);
    }

private:
    const X11Display& display_;

    Window search_windows(Window window, const std::string& name) {
        // Check if this window matches by name
        if (window_has_name(window, name)) {
            return window;
        }

        // Search children
        Window root, parent;
        Window* children = nullptr;
        unsigned int nchildren;

        if (XQueryTree(display_.get(), window, &root, &parent,
                       &children, &nchildren) == 0) {
            return None;
        }

        Window result = None;
        for (unsigned int i = 0; i < nchildren; i++) {
            result = search_windows(children[i], name);
            if (result != None) {
                break;
            }
        }

        if (children) {
            XFree(children);
        }

        return result;
    }

    bool window_has_name(Window window, const std::string& name) {
        XTextProperty prop;
        if (XGetWMName(display_.get(), window, &prop) == 0) {
            return false;
        }

        bool found = false;
        if (prop.value && prop.nitems > 0) {
            char** list = nullptr;
            int count = 0;

            if (XTextPropertyToStringList(&prop, &list, &count) != 0) {
                for (int i = 0; i < count; i++) {
                    if (list[i] && std::string(list[i]) == name) {
                        found = true;
                        break;
                    }
                }
                XFreeStringList(list);
            }
        }

        if (prop.value) {
            XFree(prop.value);
        }

        return found;
    }
};
