#include "radial_menu.hpp"
#include "hotkey_manager.hpp"
#include "usage_tracker.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <cairomm/cairomm.h>

// Static CSS for transparent background
static const char* CSS_DATA = R"(
    window {
        background-color: transparent;
    }
)";

RadialMenu::RadialMenu(const RadialConfig& config)
    : config_(config)
    , radius_(config.radius)
    , center_radius_(config.center_radius)
    , current_items_(nullptr)
    , hovered_button_(-1)
    , animation_progress_(0.0)
    , is_animating_in_(false)
    , is_animating_out_(false)
    , is_closing_(false)
    , animation_speed_ms_(config.animation_speed_ms)
    , auto_close_timeout_id_(0)
{
    // Initialize input systems
    hotkey_manager_ = std::make_unique<HotkeyManager>();
    usage_tracker_ = std::make_unique<UsageTracker>();

    setup_css();
    setup_window();
    setup_controllers();

    // Initialize menu stack with root items
    menu_stack_.push_back(config_.items);
    current_items_ = &menu_stack_.back();

    // Build hotkey map for root menu
    hotkey_manager_->build_map(*current_items_);

    // Load usage tracking data
    const char* home = std::getenv("HOME");
    if (home) {
        std::string data_path = std::string(home) + "/.config/radux/data.json";
        usage_tracker_->load(data_path);
    }
}

RadialMenu::~RadialMenu() {
    // Clean up auto-close timer
    if (auto_close_timeout_id_ != 0) {
        g_source_remove(auto_close_timeout_id_);
    }

    // Save usage data
    const char* home = std::getenv("HOME");
    if (home) {
        std::string data_path = std::string(home) + "/.config/radux/data.json";
        usage_tracker_->save(data_path);
    }
}

void RadialMenu::setup_window() {
    set_title("Radial Menu");
    set_decorated(false);
    set_resizable(false);

    // Calculate window size based on radius
    // The ease_out_back animation overshoots to ~1.08x scale, so we need padding
    // Maximum scale during animation is approximately 1.08
    double max_scale = 1.08;
    double max_radius = radius_ * max_scale;
    int diameter = static_cast<int>(max_radius * 2);

    // Add generous margin for rendering safety and priority-based button expansion
    int margin = 30;
    int window_size = diameter + margin;
    set_default_size(window_size, window_size);

    // Add drawing area
    area_.set_draw_func(sigc::mem_fun(*this, &RadialMenu::on_draw));
    set_child(area_);
}

void RadialMenu::setup_css() {
    auto css = Gtk::CssProvider::create();
    css->load_from_data(CSS_DATA);
    Gtk::StyleContext::add_provider_for_display(
        get_display(),
        css,
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
}

void RadialMenu::setup_controllers() {
    // Motion controller for hover detection
    auto motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect(sigc::mem_fun(*this, &RadialMenu::on_motion));
    area_.add_controller(motion);

    // Click controller for button selection
    auto click = Gtk::GestureClick::create();
    click->signal_pressed().connect(sigc::mem_fun(*this, &RadialMenu::on_click));
    area_.add_controller(click);

    // Key controller for hotkeys and Escape to go back/close
    auto key = Gtk::EventControllerKey::create();
    key->signal_key_pressed().connect(sigc::mem_fun(*this, &RadialMenu::on_key_press), false);
    add_controller(key);

    // Scroll controller for gesture navigation
    auto scroll = Gtk::EventControllerScroll::create();
    scroll->signal_scroll().connect(sigc::mem_fun(*this, &RadialMenu::on_scroll), false);
    area_.add_controller(scroll);

    // Setup auto-close timer if configured
    if (config_.auto_close_milliseconds > 0) {
        reset_activity_timer();
        auto_close_timeout_id_ = g_timeout_add(
            100, // Check every 100ms
            [](gpointer data) -> gboolean {
                RadialMenu* self = static_cast<RadialMenu*>(data);
                return self->on_auto_close_timeout();
            },
            this
        );
    }
}

void RadialMenu::present_at(int x, int y) {
    // Get window dimensions
    int width, height;
    get_default_size(width, height);
    int half_width = width / 2;
    int half_height = height / 2;

    // Get screen dimensions using xdotool
    int screen_width = 1920;  // fallback default
    int screen_height = 1080; // fallback default

    // Try to get actual screen dimensions
    try {
        FILE* pipe = popen("xdotool getdisplaygeometry 2>/dev/null", "r");
        if (pipe) {
            char buffer[64];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                sscanf(buffer, "%d %d", &screen_width, &screen_height);
            }
            pclose(pipe);
        }
    } catch (...) {
        // Use fallback values
    }

    // Calculate where the window should be to fit on screen
    int target_x = x;
    int target_y = y;

    // Adjust X position if too close to left or right edge
    if (x < half_width) {
        target_x = half_width;
    } else if (x > screen_width - half_width) {
        target_x = screen_width - half_width;
    }

    // Adjust Y position if too close to top or bottom edge
    if (y < half_height) {
        target_y = half_height;
    } else if (y > screen_height - half_height) {
        target_y = screen_height - half_height;
    }

    // Move mouse to the adjusted position if needed
    if (target_x != x || target_y != y) {
        try {
            std::string cmd = "xdotool mousemove " +
                             std::to_string(target_x) + " " +
                             std::to_string(target_y);
            Glib::spawn_command_line_sync(cmd);
        } catch (...) {
            // Ignore xdotool errors
        }
    }

    // Present the window at the adjusted position
    present();

    int x_pos = target_x - half_width;
    int y_pos = target_y - half_height;

    // Spawn xdotool to move window
    try {
        std::string cmd = "xdotool search --name \"Radial Menu\" windowmove --sync " +
                         std::to_string(x_pos) + " " + std::to_string(y_pos);
        Glib::spawn_command_line_async(cmd);
    } catch (...) {
        // Ignore xdotool errors
    }

    // Start open animation
    start_open_animation();
}

void RadialMenu::on_draw(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
    double cx = width / 2.0;
    double cy = height / 2.0;

    // Clear fully transparent
    cr->set_operator(Cairo::Context::Operator::SOURCE);
    cr->set_source_rgba(0, 0, 0, 0);
    cr->paint();
    cr->set_operator(Cairo::Context::Operator::OVER);

    // Combined animation: scale + radial wipe
    double scale = animation_progress_;
    double alpha = animation_progress_;

    // Handle closing animation (reverse)
    if (is_closing_) {
        scale = 1.0 - animation_progress_;
        alpha = 1.0 - animation_progress_;
    }

    // Animate scale from center
    cr->save();
    cr->translate(cx, cy);
    cr->scale(scale, scale);
    cr->translate(-cx, -cy);

    // Apply overall alpha for fade effect
    cr->push_group();

    // Calculate radial wipe angles (only for opening)
    double full_angle = 2 * M_PI * animation_progress_;
    double start_angle = -M_PI / 2;  // Start from top

    // Draw buttons with radial wipe effect (only during opening)
    size_t num_buttons = current_items_->size();
    double button_angle = 2 * M_PI / num_buttons;

    for (size_t i = 0; i < num_buttons; ++i) {
        double button_start = start_angle + i * button_angle;
        double button_end = button_start + button_angle;

        // Skip radial wipe during closing animation
        if (!is_closing_) {
            // Calculate how much of this button should be visible
            double reveal_start = std::max(start_angle, button_start);
            double reveal_end = std::min(start_angle + full_angle, button_end);

            if (reveal_end <= reveal_start) {
                continue; // Not yet revealed
            }

            // Save state before clipping
            cr->save();

            // Create radial wipe clip for this button
            cr->begin_new_path();
            cr->move_to(cx, cy);
            cr->arc(cx, cy, radius_ + 10, reveal_start, reveal_end);
            cr->line_to(cx, cy);
            cr->close_path();
            cr->clip();
        }

        // Draw the button
        draw_button(cr, i, num_buttons, cx, cy);

        if (!is_closing_) {
            cr->restore();
        }
    }

    // Draw center circle
    draw_center(cr, cx, cy);

    cr->pop_group_to_source();
    cr->paint_with_alpha(alpha);
    cr->restore();
}

void RadialMenu::draw_button(const Cairo::RefPtr<Cairo::Context>& cr,
                              int index, int total,
                              double cx, double cy) {
    const auto& item = (*current_items_)[index];

    // Get effective theme for this item (inherits from parent if needed)
    Theme theme = item.get_effective_theme(config_.theme);

    double button_angle = 2 * M_PI / total;
    double start = -M_PI / 2 + index * button_angle;
    double end = start + button_angle;

    // Get priority-based radius for text/icon positioning
    double tr = get_button_radius(index);

    // Calculate inner and outer radii for this button
    double inner_r = center_radius_;
    double outer_r = radius_;

    // Adjust for priority (affects button size)
    double priority_multiplier = 1.0 + (item.priority * 0.02);
    double radius_adjust = (outer_r - inner_r) * (priority_multiplier - 1.0) / 2.0;
    inner_r -= radius_adjust;
    outer_r += radius_adjust;

    // Draw arc segment
    cr->begin_new_path();
    cr->arc(cx, cy, outer_r, start, end);
    cr->arc_negative(cx, cy, inner_r, end, start);
    cr->close_path();

    // Fill with theme colors
    if (index == hovered_button_) {
        theme.hover_color.set_as_source(cr);
    } else {
        theme.background_color.set_as_source(cr);
    }
    cr->fill_preserve();

    // Stroke with theme border color
    theme.border_color.set_as_source(cr);
    cr->set_line_width(2);
    cr->stroke();

    // Calculate position for label/icon
    double mid = start + button_angle / 2;
    double tx = cx + tr * std::cos(mid);
    double ty = cy + tr * std::sin(mid);

    // Draw icon or label
    if (item.has_icon()) {
        double icon_size = 32;
        draw_icon(cr, tx, ty, *item.icon, icon_size);
    } else {
        draw_text(cr, tx, ty, item.label, theme.font_size, true);
    }

    // Draw hotkey hint if present
    if (item.hotkey && hotkey_manager_) {
        std::string hint = hotkey_manager_->get_hotkey_for_item(index);
        if (!hint.empty()) {
            double hint_y = ty + 22;
            draw_text(cr, tx, hint_y, "[" + hint + "]", 9, false);
        }
    }
}

void RadialMenu::draw_center(const Cairo::RefPtr<Cairo::Context>& cr,
                              double cx, double cy) {
    // Draw center circle with theme color
    cr->begin_new_path();
    cr->arc(cx, cy, center_radius_, 0, 2 * M_PI);

    if (menu_stack_.size() > 1) {
        // In submenu - use hover color for back button
        config_.theme.hover_color.set_as_source(cr);
    } else {
        config_.theme.center_color.set_as_source(cr);
    }
    cr->fill_preserve();

    config_.theme.border_color.set_as_source(cr);
    cr->set_line_width(2);
    cr->stroke();

    // Draw center icon or text
    if (menu_stack_.size() > 1) {
        // Try to load back.svg from ~/.config/radux/
        const char* home = std::getenv("HOME");
        if (home) {
            std::string back_icon_path = std::string(home) + "/.config/radux/back.svg";
            if (!draw_icon(cr, cx, cy, back_icon_path, center_radius_ * 0.6)) {
                // Fallback to text if icon not found
                draw_text(cr, cx, cy, "←", config_.theme.font_size, true);
            }
        } else {
            draw_text(cr, cx, cy, "←", config_.theme.font_size, true);
        }
    } else if (hovered_button_ >= 0 && hovered_button_ < static_cast<int>(current_items_->size())) {
        // Show description of hovered item
        const auto& item = (*current_items_)[hovered_button_];
        if (!item.description.empty()) {
            draw_multiline_text(cr, cx, cy, item.description);
        }
    }
}

void RadialMenu::draw_text(const Cairo::RefPtr<Cairo::Context>& cr,
                            double x, double y, const std::string& text,
                            int font_size, bool bold) {
    config_.theme.font_color.set_as_source(cr);
    cr->select_font_face("Sans",
                         Cairo::ToyFontFace::Slant::NORMAL,
                         bold ? Cairo::ToyFontFace::Weight::BOLD : Cairo::ToyFontFace::Weight::NORMAL);
    cr->set_font_size(font_size);

    Cairo::TextExtents extents;
    cr->get_text_extents(text, extents);
    cr->move_to(x - extents.width / 2 - extents.x_bearing, y - extents.height / 2 - extents.y_bearing);
    cr->show_text(text);
}

void RadialMenu::draw_multiline_text(const Cairo::RefPtr<Cairo::Context>& cr,
                                      double cx, double cy, const std::string& text) {
    cr->set_font_size(config_.theme.font_size - 2); // Slightly smaller for descriptions
    config_.theme.font_color.set_as_source(cr);

    // Split by newlines (supports \n in description)
    std::vector<std::string> lines;
    std::stringstream ss(text);
    std::string line;
    while (std::getline(ss, line)) {
        lines.push_back(line);
    }

    int line_height = config_.theme.font_size + 4;
    double start_y = cy - (lines.size() * line_height) / 2.0 + line_height / 2.0;

    for (size_t i = 0; i < lines.size(); ++i) {
        Cairo::TextExtents extents;
        cr->get_text_extents(lines[i], extents);
        cr->move_to(cx - extents.width / 2 - extents.x_bearing, start_y + i * line_height - extents.y_bearing);
        cr->show_text(lines[i]);
    }
}

double RadialMenu::get_button_radius(int index) const {
    if (index < 0 || index >= static_cast<int>(current_items_->size())) {
        return (radius_ + center_radius_) / 2.0;
    }

    const auto& item = (*current_items_)[index];
    double base_radius = (radius_ + center_radius_) / 2.0;

    // Add 2% per priority level
    double multiplier = 1.0 + (item.priority * 0.02);

    return base_radius * multiplier;
}

void RadialMenu::on_motion(double x, double y) {
    reset_activity_timer();

    int old = hovered_button_;
    hovered_button_ = get_button_at_pos(x, y);

    if (old != hovered_button_) {
        area_.queue_draw();
    }
}

void RadialMenu::on_click(int n_press, double x, double y) {
    reset_activity_timer();

    auto [cx, cy] = get_center();
    double dx = x - cx;
    double dy = y - cy;
    double dist = std::hypot(dx, dy);

    // Check if clicked outside menu
    if (dist > radius_) {
        start_close_animation();
        return;
    }

    // Check if clicked in center
    if (dist < center_radius_) {
        if (menu_stack_.size() > 1) {
            pop_menu();
        }
        return;
    }

    // Check if clicked on a button
    int button = get_button_at_pos(x, y);
    if (button >= 0 && button < static_cast<int>(current_items_->size())) {
        const auto& item = (*current_items_)[button];

        if (item.has_submenu()) {
            push_menu(item.submenu, item.label);
        } else {
            execute_command(item);
        }
    }
}

bool RadialMenu::on_key_press(guint keyval, guint keycode, Gdk::ModifierType state) {
    reset_activity_timer();

    // Check for hotkeys first
    if (hotkey_manager_) {
        auto item_index = hotkey_manager_->find_item(keyval, state);
        if (item_index && *item_index < current_items_->size()) {
            const auto& item = (*current_items_)[*item_index];
            if (item.has_submenu()) {
                push_menu(item.submenu, item.label);
            } else {
                execute_command(item);
            }
            return true;
        }
    }

    // Escape key
    if (keyval == GDK_KEY_Escape) {
        if (menu_stack_.size() > 1) {
            pop_menu();
        } else {
            start_close_animation();
        }
        return true;
    }

    // Enter key to execute most-used or hovered item
    if (keyval == GDK_KEY_Return) {
        if (hovered_button_ >= 0 && hovered_button_ < static_cast<int>(current_items_->size())) {
            const auto& item = (*current_items_)[hovered_button_];
            if (item.has_submenu()) {
                push_menu(item.submenu, item.label);
            } else {
                execute_command(item);
            }
        } else {
            // Check for most-used item
            auto most_used = usage_tracker_->get_most_used_root_item();
            if (most_used && menu_stack_.size() == 1) {
                // Find item with matching label
                for (size_t i = 0; i < current_items_->size(); ++i) {
                    if ((*current_items_)[i].label == *most_used) {
                        const auto& item = (*current_items_)[i];
                        if (!item.has_submenu()) {
                            execute_command(item);
                        }
                        break;
                    }
                }
            }
        }
        return true;
    }

    return false;
}

bool RadialMenu::on_scroll(double dx, double dy) {
    reset_activity_timer();

    const double SCROLL_THRESHOLD = 5.0;
    size_t num_items = current_items_->size();

    if (num_items == 0) {
        return false;
    }

    // Scroll down or right -> next item
    if (dy > SCROLL_THRESHOLD || dx > SCROLL_THRESHOLD) {
        hovered_button_ = (hovered_button_ + 1) % num_items;
        area_.queue_draw();
        return true;
    }

    // Scroll up or left -> previous item
    if (dy < -SCROLL_THRESHOLD || dx < -SCROLL_THRESHOLD) {
        hovered_button_ = (hovered_button_ - 1 + num_items) % num_items;
        area_.queue_draw();
        return true;
    }

    return false;
}

std::pair<double, double> RadialMenu::get_center() const {
    int width = get_width();
    int height = get_height();
    return {width / 2.0, height / 2.0};
}

int RadialMenu::get_button_at_pos(double x, double y) const {
    auto [cx, cy] = get_center();

    double dx = x - cx;
    double dy = y - cy;
    double dist = std::hypot(dx, dy);

    // Check if outside button area
    if (dist < center_radius_ || dist > radius_) {
        return -1;
    }

    // Calculate angle
    double angle = std::atan2(-dy, dx);
    if (angle < 0) {
        angle += 2 * M_PI;
    }

    // Convert to button index
    double angle_deg = (90 - (angle * 180 / M_PI));
    if (angle_deg < 0) {
        angle_deg += 360;
    }
    angle_deg = std::fmod(angle_deg, 360);

    double button_angle = 360.0 / current_items_->size();
    return static_cast<int>(angle_deg / button_angle);
}

void RadialMenu::push_menu(const std::vector<MenuItem>& submenu, const std::string& label) {
    menu_stack_.push_back(submenu);
    current_items_ = &menu_stack_.back();

    // Track menu path for usage tracking
    if (!label.empty()) {
        current_menu_path_.push_back(label);
    }

    hovered_button_ = -1;

    // Rebuild hotkey map for this menu
    if (hotkey_manager_) {
        hotkey_manager_->build_map(*current_items_);
    }

    // Restart animation for submenu
    start_open_animation();
}

void RadialMenu::pop_menu() {
    if (menu_stack_.size() > 1) {
        menu_stack_.pop_back();
        current_items_ = &menu_stack_.back();

        // Update menu path
        if (!current_menu_path_.empty()) {
            current_menu_path_.pop_back();
        }

        hovered_button_ = -1;

        // Rebuild hotkey map for parent menu
        if (hotkey_manager_) {
            hotkey_manager_->build_map(*current_items_);
        }

        // Restart animation when going back
        start_open_animation();
    }
}

void RadialMenu::execute_command(const MenuItem& item) {
    if (item.command.empty()) {
        return;
    }

    // Record usage
    if (usage_tracker_) {
        usage_tracker_->record_usage(item.label, current_menu_path_);
    }

    // Execute command
    if (item.notify) {
        // Execute synchronously and capture output
        try {
            std::string stdout;
            std::string stderr;
            int exit_code;

            Glib::spawn_command_line_sync(item.command, &stdout, &stderr, &exit_code);

            if (exit_code == 0 && !stdout.empty()) {
                // Send notification
                std::string notify_cmd = "notify-send '" + item.label + "' '" + stdout + "'";
                Glib::spawn_command_line_async(notify_cmd);
            } else if (exit_code != 0) {
                std::cerr << "Command failed: " << stderr << "\n";
            }
        } catch (const Glib::SpawnError& e) {
            std::cerr << "Failed to execute command: " << e.what() << "\n";
        }
    } else {
        // Execute asynchronously
        try {
            Glib::spawn_command_line_async(item.command);
        } catch (const Glib::SpawnError& e) {
            std::cerr << "Failed to execute command '" << item.command << "': " << e.what() << "\n";
        }
    }

    start_close_animation();
}

void RadialMenu::start_open_animation() {
    animation_progress_ = 0.0;
    is_animating_in_ = true;
    is_closing_ = false;
    animation_start_ = std::chrono::steady_clock::now();

    // Remove any existing animation tick
    if (animation_tick_id_ != 0) {
        remove_tick_callback(animation_tick_id_);
        animation_tick_id_ = 0;
    }

    // Add animation tick callback (60 FPS)
    animation_tick_id_ = add_tick_callback(
        sigc::mem_fun(*this, &RadialMenu::on_animation_tick)
    );
}

void RadialMenu::start_close_animation() {
    animation_progress_ = 0.0;
    is_animating_in_ = false;
    is_closing_ = true;
    animation_start_ = std::chrono::steady_clock::now();

    // Remove any existing animation tick
    if (animation_tick_id_ != 0) {
        remove_tick_callback(animation_tick_id_);
        animation_tick_id_ = 0;
    }

    // Add animation tick callback
    animation_tick_id_ = add_tick_callback(
        sigc::mem_fun(*this, &RadialMenu::on_animation_tick)
    );
}

bool RadialMenu::on_animation_tick(const Glib::RefPtr<Gdk::FrameClock>& frame_clock) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - animation_start_
    ).count();

    // Use configured animation speed
    int duration = animation_speed_ms_;

    if (is_closing_) {
        // Closing animation
        if (elapsed >= duration) {
            close(); // Actually close the window
            return false;
        }

        double t = static_cast<double>(elapsed) / duration;
        animation_progress_ = ease_out_back(t);

        area_.queue_draw();
        return true;
    }

    // Opening animation
    if (!is_animating_in_) {
        return false;  // Stop the tick
    }

    if (elapsed >= duration) {
        animation_progress_ = 1.0;
        is_animating_in_ = false;
        area_.queue_draw();
        return false;  // Stop the tick
    }

    // Calculate progress with easing (slightly slower for smoother feel)
    double t = static_cast<double>(elapsed) / duration;
    animation_progress_ = ease_out_back(t);

    area_.queue_draw();
    return true;  // Continue the tick
}

void RadialMenu::reset_activity_timer() {
    last_activity_ = std::chrono::steady_clock::now();
}

bool RadialMenu::on_auto_close_timeout() {
    if (config_.auto_close_milliseconds <= 0) {
        return false; // Disabled
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_activity_
    ).count();

    if (elapsed >= config_.auto_close_milliseconds) {
        start_close_animation();
        return false; // Stop timeout
    }

    return true; // Continue checking
}

bool RadialMenu::load_icon_from_file(const std::string& icon_path,
                                      Glib::RefPtr<Gdk::Pixbuf>& pixbuf) {
    try {
        // Expand ~ to home directory if present
        std::string expanded_path = icon_path;
        if (!expanded_path.empty() && expanded_path[0] == '~') {
            const char* home = std::getenv("HOME");
            if (home) {
                expanded_path = std::string(home) + expanded_path.substr(1);
            }
        }

        auto file = Gio::File::create_for_path(expanded_path);
        pixbuf = Gdk::Pixbuf::create_from_file(expanded_path);
        return !!pixbuf;
    } catch (const Glib::Error& e) {
        return false;
    }
}

bool RadialMenu::draw_icon(const Cairo::RefPtr<Cairo::Context>& cr,
                            double x, double y, const std::string& icon_path, double size) {
    Glib::RefPtr<Gdk::Pixbuf> pixbuf;
    if (!load_icon_from_file(icon_path, pixbuf)) {
        return false;
    }

    // Calculate scaled size maintaining aspect ratio
    int pw = pixbuf->get_width();
    int ph = pixbuf->get_height();
    double scale = std::min(size / pw, size / ph);
    int scaled_width = static_cast<int>(pw * scale);
    int scaled_height = static_cast<int>(ph * scale);

    // Scale the pixbuf
    auto scaled_pixbuf = pixbuf->scale_simple(scaled_width, scaled_height, Gdk::InterpType::BILINEAR);

    // Draw centered at (x, y)
    Gdk::Cairo::set_source_pixbuf(cr, scaled_pixbuf, x - scaled_width / 2, y - scaled_height / 2);
    cr->paint();

    return true;
}

// Easing functions
double RadialMenu::ease_out_cubic(double t) {
    return 1.0 - std::pow(1.0 - t, 3.0);
}

double RadialMenu::ease_out_back(double t) {
    const double c1 = 1.70158;
    const double c3 = c1 + 1.0;
    return 1.0 + c3 * std::pow(t - 1.0, 3.0) + c1 * std::pow(t - 1.0, 2.0);
}

double RadialMenu::ease_out_elastic(double t) {
    const double c4 = 2.0 * M_PI / 3.0;
    return t == 0.0 ? 0.0 : t == 1.0 ? 1.0 :
        std::pow(2.0, -10.0 * t) * std::sin((t * 10.0 - 0.75) * c4) + 1.0;
}
