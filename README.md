

# RADUX

<p align="center">
  <img width="200" alt="fullicon" src="https://github.com/user-attachments/assets/4d050cd2-be31-4fbf-bbee-fe408be540af" />
</p>


A simplistic Linux radial menu for the [lazy](https://xkcd.com/1319/).


## Features

- **Radial menu interface** - Quick access menu at cursor position
- **Nested submenus** - Organize items hierarchically
- **Hotkey support** - Keyboard shortcuts with context-aware mapping
- **Gesture support** - 2-finger scroll to navigate items
- **Theme customization** - Custom colors, fonts, and sizes
- **Icon support** - SVG/PNG icons for menu items
- **Usage tracking** - Most-used item highlighting
- **Multi-monitor** - Works across multiple monitors
- **Smooth animations** - Configurable animation speed

**NOTE: RADUX ONLY SUPPORTS X11 FOR NOW. WAYLAND SUPPORT WILL BE IMPLEMENTED SOON**

## Installation

### From Source

```bash
cd src/cpp
cmake -B build
cmake --build build
sudo cmake --install build
```

The binary will be installed to `bin/radux-menu` in the project directory. (I felt like having a more specific name like this, but you may change it just to `radux` like I did)


### From releases

Download the binary :)


## Configuration

### Config File Locations

Radux searches for configuration in this order:

1. `~/.config/radux/config.yaml` (primary)
2. `./config.yaml` (fallback, relative to current directory)

You can also specify a config file explicitly:
```bash
radux-menu --config /path/to/config.yaml
```

### Basic (example) Configuration

```yaml
# Geometry
radius: 120              # Distance from center to outer edge
inner-radius: 50         # Center circle radius

# Animation
animation-speed: 500     # Duration in milliseconds (or multiplier like "1.5x")

# Auto-close (optional)
auto-close-milliseconds: 5000  # Close after 5 seconds of inactivity (0 = disabled)

# Theme Colors
hover-color: "#4a90d9"
background-color: "#2c2c2c"
border-color: "#666666"
font-color: "#ffffff"
center-color: "#1a1a1a"
font-size: 16

# Menu Items
items:
  - label: "Browser"
    command: "brave"
    description: "Web Browser"
    hotkey: "b"
    priority: 5

  - label: "Terminal"
    command: "st"
    description: "Terminal\\nwith tmux"
    hotkey: "t"
    priority: 8

  - label: "Development"
    hotkey: "d"
    priority: 7
    background-color: "#2a3a2a"  # Custom color for this item
    submenu:
      - label: "VSCode"
        command: "code"
        hotkey: "Ctrl+Shift+V"
        priority: 10

      - label: "Neovim"
        command: "st -e nvim"
        hotkey: "Ctrl+Shift+N"
```

## Item Attributes

### Basic Attributes

| Attribute | Type | Required | Description |
|-----------|------|----------|-------------|
| `label` | string | Yes | Display text for the item |
| `command` | string | Yes* | Shell command to execute (not for submenus) |
| `description` | string | No | Tooltip text (supports `\n` for newlines) |

### Visual Attributes

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `icon` | string | - | Path to SVG/PNG icon file |
| `priority` | int (0-10) | 0 | Button size (2% per priority level) |
| `background-color` | hex | global | Custom background color |
| `hover-color` | hex | global | Custom hover color |
| `font-color` | hex | global | Custom text color |

### Interaction Attributes

| Attribute | Type | Default | Description |
|-----------|------|---------|-------------|
| `hotkey` | string | - | Key combination (e.g., "b", "Ctrl+1") |
| `notify` | boolean | false | Send command stdout to notification |

### Submenu Attributes

To create a submenu, omit `command` and add `submenu`:

```yaml
- label: "Parent Item"
  description: "Contains sub-items"
  hotkey: "p"
  submenu:
    - label: "Child 1"
      command: "command1"
    - label: "Child 2"
      command: "command2"
```

**Color Inheritance**: Submenu items inherit colors from their parent item if not overridden.

## Hotkeys

### Format

Hotkeys support modifier combinations:

- **Single key**: `hotkey: "b"`
- **With modifiers**: `hotkey: "Ctrl+1"`
- **Multiple modifiers**: `hotkey: "Ctrl+Shift+V"`

**Supported modifiers**:
- `Ctrl` or `Control`
- `Alt`
- `Shift`
- `Super`, `Win`, or `Meta`

### Context-Aware Behavior

Hotkeys are **context-specific** - the same hotkey can be used in different menus:

```yaml
items:
  - label: "Development"
    hotkey: "d"
    submenu:
      - label: "VSCode"
        hotkey: "v"        # "v" opens VSCode when in Development menu
        command: "code"

  - label: "Design"
    hotkey: "d"            # Same hotkey, different menu
    submenu:
      - label: "Figma"
        hotkey: "v"        # "v" opens Figma when in Design menu
        command: "figma"
```

## Icons

### Item Icons

Place your icons in `~/.config/radux/icons/` (or any path):

```yaml
items:
  - label: "Browser"
    icon: "~/.config/radux/icons/brave.svg"
    command: "brave"
```

Supported formats:
- SVG (recommended)
- PNG
- Any image format supported by GdkPixbuf

### Most-Used Highlighting

When an item in the **root menu** has been used 10+ times:
- It gets highlighted when the menu opens
- Press `Enter` to execute it quickly
- The highlight clears when you interact with other items

## Keyboard Navigation

| Key | Action |
|-----|--------|
| `Escape` | Go back / Close menu |
| `Enter` | Execute hovered or most-used item |
| Hotkey | Execute associated item |

## Command Line Usage

```bash
# Show help
radux-menu --help

# Open at mouse position
radux-menu

# Open at specific coordinates
radux-menu 500 300

# Use specific config file
radux-menu --config /path/to/config.yaml

# Use inline configuration
radux-menu --cli "Terminal:st:Terminal;Brave:brave:Browser"
```

## Examples

### Simple Menu

```yaml
radius: 120
inner-radius: 50

items:
  - label: "Firefox"
    command: "firefox"
    hotkey: "f"

  - label: "Terminal"
    command: "st"
    hotkey: "t"
```

### Nested Menu with Icons

```yaml
radius: 150
inner-radius: 60
font-size: 18

# Global theme
hover-color: "#4a90d9"
background-color: "#2c2c2c"

items:
  - label: "Development"
    hotkey: "d"
    icon: "~/.config/radux/icons/dev.svg"
    background-color: "#1a3a1a"
    submenu:
      - label: "VSCode"
        icon: "~/.config/radux/icons/vscode.svg"
        command: "code"
        hotkey: "v"
        notify: true

      - label: "Terminal"
        icon: "~/.config/radux/icons/terminal.svg"
        command: "st -e tmux"
        hotkey: "t"
```

### With Notifications

```yaml
items:
  - label: "Check Updates"
    command: "checkupdates"
    notify: true  # Sends output to desktop notification

  - label: "System Info"
    command: "neofetch"
    notify: true
```

## Troubleshooting

### Menu Doesn't Appear

1. Check config file location: `~/.config/radux/config.yaml`
2. Validate YAML syntax: `yamllint ~/.config/radux/config.yaml`
3. Check for syntax errors in terminal output

### Hotkeys Not Working

1. Ensure hotkey is defined in config
2. Check for conflicting shortcuts in your WM/DE

### Icons Not Displaying

1. Verify icon path is correct
2. Check file permissions
3. Ensure image format is supported (SVG/PNG)

### Multi-Monitor Issues

- The menu uses X11 for screen detection
- Check xdotool is installed: `which xdotool`
