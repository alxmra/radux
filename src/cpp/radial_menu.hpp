#pragma once

#include <gtkmm.h>
#include <cmath>
#include <chrono>
#include <unordered_map>
#include "menu_item.hpp"
#include "config_loader.hpp"
#include "color_theme.hpp"

// Forward declarations
class HotkeyManager;
class UsageTracker;

class RadialMenu : public Gtk::Window {
public:
    explicit RadialMenu(const RadialConfig& config);
    virtual ~RadialMenu();

    // Show menu at specific screen coordinates
    void present_at(int x, int y);

private:
    // Configuration
    RadialConfig config_;
    int radius_;
    int center_radius_;

    // Menu stack for nested navigation
    std::vector<std::vector<MenuItem>> menu_stack_;
    std::vector<MenuItem>* current_items_;
    int hovered_button_ = -1;

    // Track current menu path for usage tracking
    std::vector<std::string> current_menu_path_;

    // GTK widgets
    Gtk::DrawingArea area_;

    // Animation state
    double animation_progress_ = 0.0;
    bool is_animating_in_ = false;
    bool is_animating_out_ = false;
    bool is_closing_ = false;
    std::chrono::steady_clock::time_point animation_start_;
    guint animation_tick_id_ = 0;

    // Animation timing
    int animation_speed_ms_;

    // Auto-close timer
    std::chrono::steady_clock::time_point last_activity_;
    guint auto_close_timeout_id_ = 0;

    // Input systems (will be initialized when implemented)
    std::unique_ptr<HotkeyManager> hotkey_manager_;
    std::unique_ptr<UsageTracker> usage_tracker_;

    // Signal handlers
    void on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);
    void on_motion(double x, double y);
    void on_click(int n_press, double x, double y);
    bool on_key_press(guint keyval, guint keycode, Gdk::ModifierType state);
    bool on_scroll(double dx, double dy);

    // Animation callback
    bool on_animation_tick(const Glib::RefPtr<Gdk::FrameClock>& frame_clock);

    // Geometry helpers
    std::pair<double, double> get_center() const;
    int get_button_at_pos(double x, double y) const;
    double get_button_radius(int index) const;

    // Easing functions for smooth animations
    static double ease_out_cubic(double t);
    static double ease_out_back(double t);
    static double ease_out_elastic(double t);

    // Drawing helpers
    void draw_button(const Cairo::RefPtr<Cairo::Context>& cr,
                     int index, int total,
                     double cx, double cy);
    void draw_center(const Cairo::RefPtr<Cairo::Context>& cr,
                     double cx, double cy);
    void draw_text(const Cairo::RefPtr<Cairo::Context>& cr,
                   double x, double y, const std::string& text,
                   int font_size = 14, bool bold = true);
    void draw_multiline_text(const Cairo::RefPtr<Cairo::Context>& cr,
                             double cx, double cy, const std::string& text);
    bool draw_icon(const Cairo::RefPtr<Cairo::Context>& cr,
                   double x, double y, const std::string& icon_path, double size);
    bool load_icon_from_file(const std::string& icon_path,
                            Glib::RefPtr<Gdk::Pixbuf>& pixbuf);

    // Menu navigation
    void push_menu(const std::vector<MenuItem>& submenu, const std::string& label = "");
    void pop_menu();

    // Setup
    void setup_window();
    void setup_css();
    void setup_controllers();

    // Command execution
    void execute_command(const MenuItem& item);

    // Animations
    void start_open_animation();
    void start_close_animation();

    // Activity tracking for auto-close
    void reset_activity_timer();
    bool on_auto_close_timeout();
};
