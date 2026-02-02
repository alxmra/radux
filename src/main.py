from Xlib import display
import subprocess
import os

def get_mouse_pos():
    """Returns a (x,y) tuple"""
    data = display.Display().screen().root.query_pointer()._data
    return data['root_x'], data['root_y']


def run_menu(x=None, y=None, config_override=None):
    """Invoke the C++ binary"""
    # Get mouse position if not provided
    if x is None or y is None:
        x, y = get_mouse_pos()
        print(f"Mouse position: {x}, {y}")

    # Path to C++ binary
    script_dir = os.path.dirname(os.path.abspath(__file__))
    binary_path = os.path.join(script_dir, '..', 'bin', 'radux-menu')

    # Build command
    cmd = [binary_path, str(x), str(y)]

    # Add config override if provided
    if config_override:
        cmd.extend(['--cli', config_override])

    print(f"Running: {' '.join(cmd)}")

    # Execute
    try:
        subprocess.run(cmd, check=True)
    except FileNotFoundError:
        print(f"Error: Binary not found at {binary_path}")
        print("Please build the C++ binary first:")
        print("  cd src/cpp && cmake -B build -S . && cmake --build build")
    except subprocess.CalledProcessError as e:
        print(f"Error running radial menu: {e}")


if __name__ == '__main__':
    run_menu()
