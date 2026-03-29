import ControllerBackend, ColourPallete
import time, sys, tty, termios
from PIL import Image, ImageEnhance
from collections import defaultdict


DELAY = 0.08
CANVAS_SIZE = 50
SATURATION = 3.0 # Anything less than 2.0 does not look good.
ACCELERATION_BREAK_INTERVAL = 2   # pause every N steps
ACCELERATION_BREAK_DELAY = 0.3    # how long to pause
COLOUR_PALLETE_TYPE = ColourPallete.EXT_RANGE
COLOUR_PALLETE = COLOUR_PALLETE_TYPE.COLOURS
IMG = "./images/wgia.jpg"

controller = ControllerBackend.Controller(115200, "/dev/cu.usbmodem101")

cur_x = 0
cur_y = 0

if COLOUR_PALLETE_TYPE.EXT == False:
    cur_palette_row = 0
    cur_palette_col = 0
else:
    cur_palette_row = 6 # First slot is always black, so we start at 6, 0
    cur_palette_col = 0 

PALETTE_ROWS = len(COLOUR_PALLETE_TYPE.COLOURS)
PALETTE_COLS = max(
    len(entry) if isinstance(entry, tuple) else 1
    for entry in COLOUR_PALLETE_TYPE.COLOURS
)

# Terminal Control
def get_key():
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch = sys.stdin.read(1)
        if ch == "\x1b":
            ch += sys.stdin.read(2)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
    return ch

def align_cursor():
    """
    Interactive alignment prompt.
    WASD or arrow keys move the controller cursor.
    Enter confirms and starts drawing.
    """

    print("=== Align cursor to starting position ===")
    print("  Arrow keys : D-Pad")
    print("  S          : A")
    print("  A          : B")
    print("  W          : X")
    print("  Q          : Y")
    print("  E          : L3")
    print("  P          : Preview Image")
    print("  Enter      : Start drawing")
    print("==========================================")

    while True:
        key = get_key()

        if key == "\x1b[A":          # Up arrow
            controller.UP()
        elif key == "\x1b[B":        # Down arrow
            controller.DOWN()
        elif key == "\x1b[C":        # Right arrow
            controller.RIGHT()
        elif key == "\x1b[D":        # Left arrow
            controller.LEFT()
        elif key in ("s", "S"):      # A button (Paint / Accept)
            controller.A()
        elif key in ("a", "A"):      # B button (Back)
            controller.B()
        elif key in ("w", "W"):      # X button (Tool)
            controller.X()
        elif key in ("q", "Q"):      # Y button (Colour)
            controller.Y()
        elif key in ("e", "E"):      # L3 (Hide HUD)
            controller.L3()
        elif key in ("1", "-"):      # Minus
            controller.L3()
        elif key in ("2", "+", "="): # Plus, primarly for accessing GP2040 web ui
            controller.L3()
        elif key in ("p", "P"):      # Preview Image
            pixel_map = load_and_quantize(IMG)
            preview_image(pixel_map)
        elif key in ("\r", "\n"):    # Enter
            print("Starting draw...")
            break
        elif key == "\x03":          # Ctrl+C to cancel
            print("\nCancelled.")
            sys.exit(0)



# Controller Movement
def move_to(target_x, target_y):
    global cur_x, cur_y

    dy = target_y - cur_y
    for i in range(abs(dy)):
        if dy > 0:
            controller.DOWN()
        else:
            controller.UP()
        if (i + 1) % ACCELERATION_BREAK_INTERVAL == 0:
            time.sleep(ACCELERATION_BREAK_DELAY)
        else:
            time.sleep(DELAY)
    cur_y = target_y

    dx = target_x - cur_x
    for i in range(abs(dx)):
        if dx > 0:
            controller.RIGHT()
        else:
            controller.LEFT()
        if (i + 1) % ACCELERATION_BREAK_INTERVAL == 0:
            time.sleep(ACCELERATION_BREAK_DELAY)
        else:
            time.sleep(DELAY)
    cur_x = target_x

def switch_color(hex_str):
    global cur_palette_row, cur_palette_col

    result = COLOUR_PALLETE_TYPE.get_position(hex_str)
    if result is None:
        print(f"Warning: {hex_str} not found in palette, skipping")
        return

    x, y = result

    # Clamp to grid bounds to prevent wrapping
    x = max(0, min(x, PALETTE_COLS - 1))
    y = max(0, min(y, PALETTE_ROWS - 1))

    if x == cur_palette_col and y == cur_palette_row:
        return

    # Release brush and open colour menu
    controller.A_RELEASE()
    time.sleep(DELAY * 5)
    controller.L3()
    time.sleep(DELAY * 5)
    controller.Y()  # Enter colour bar
    time.sleep(DELAY * 5)
    controller.Y()  # Enter colour grid
    time.sleep(DELAY * 5)

    d_x = x - cur_palette_col
    for _ in range(abs(d_x)):
        if d_x > 0:
            controller.RIGHT()
        else:
            controller.LEFT()
        time.sleep(DELAY)

    d_y = y - cur_palette_row
    for _ in range(abs(d_y)):
        if d_y > 0:
            controller.DOWN()
        else:
            controller.UP()
        time.sleep(DELAY)

    controller.A()
    time.sleep(DELAY * 5)
    controller.L3()
    time.sleep(DELAY * 5)

    cur_palette_col = x
    cur_palette_row = y
    print(f"Switched to {hex_str} at x={x}, y={y}")

# Image Processing 

def nearest_colour(pixel):
    r, g, b = pixel[:3]
    best_hex, best_dist = None, float("inf")
    for hex_str in COLOUR_PALLETE_TYPE.flat_colours():
        cr, cg, cb = COLOUR_PALLETE_TYPE.hex_to_rgb(hex_str)
        dist = (r - cr) ** 2 + (g - cg) ** 2 + (b - cb) ** 2
        if dist < best_dist:
            best_dist = dist
            best_hex = hex_str

    if COLOUR_PALLETE_TYPE.get_position(best_hex) is None:
        print(f"Warning: nearest colour {best_hex} has no valid position")

    return best_hex


def load_and_quantize(path):
    img = Image.open(path).convert("RGBA")
    img = img.resize((CANVAS_SIZE, CANVAS_SIZE), Image.LANCZOS)

    # Basic colour pallete looks like shit without boosting the saturation
    rgb = img.convert("RGB")
    rgb = ImageEnhance.Color(rgb).enhance(SATURATION)

    # Re-apply alpha
    r, g, b = rgb.split()
    _, _, _, a = img.split()
    img = Image.merge("RGBA", (r, g, b, a))

    pixel_map = {}
    for y in range(CANVAS_SIZE):
        for x in range(CANVAS_SIZE):
            px = img.getpixel((x, y))
            if px[3] < 128:
                continue
            pixel_map[(x, y)] = nearest_colour(px)  # now stores hex string
    return pixel_map

    return pixel_map


def get_runs(pixel_list):
    """Convert a list of (x, y) pixels into horizontal runs."""
    rows = defaultdict(list)
    for x, y in pixel_list:
        rows[y].append(x)

    runs = []
    for y, xs in rows.items():
        xs = sorted(xs)
        run_start = xs[0]
        run_len = 1
        for i in range(1, len(xs)):
            if xs[i] == xs[i - 1] + 1:
                run_len += 1
            else:
                runs.append((run_start, y, run_len))
                run_start = xs[i]
                run_len = 1
        runs.append((run_start, y, run_len))

    return runs

def order_runs_nearest_neighbor(runs):
    if not runs:
        return []

    remaining = list(runs)
    ordered = []
    cx, cy = cur_x, cur_y

    while remaining:
        best_idx = None
        best_dist = float("inf")
        best_reverse = False

        for i, (x, y, length) in enumerate(remaining):
            d_fwd = abs(x - cx) + abs(y - cy)
            d_rev = abs((x + length - 1) - cx) + abs(y - cy)

            if d_fwd <= d_rev and d_fwd < best_dist:
                best_dist = d_fwd
                best_idx = i
                best_reverse = False
            elif d_rev < d_fwd and d_rev < best_dist:
                best_dist = d_rev
                best_idx = i
                best_reverse = True

        x, y, length = remaining.pop(best_idx)
        ordered.append((x, y, length, best_reverse))

        # Update projected cursor end position
        cx = x if best_reverse else x + length - 1
        cy = y

    return ordered

def preview_image(pixel_map):
    preview = Image.new("RGB", (CANVAS_SIZE, CANVAS_SIZE), (200, 200, 200))
    for (x, y), hex_str in pixel_map.items():
        preview.putpixel((x, y), COLOUR_PALLETE_TYPE.hex_to_rgb(hex_str))
    preview = preview.resize((CANVAS_SIZE * 10, CANVAS_SIZE * 10), Image.NEAREST)
    preview.show()

# Drawing 
def draw_runs(runs):
    global cur_x, cur_y
    ordered = order_runs_nearest_neighbor(runs)
    for x, y, length, reverse in ordered:
        if reverse:
            move_to(x + length - 1, y)
            controller.A_HOLD()
            time.sleep(DELAY)
            for _ in range(length - 1):
                controller.LEFT()
                time.sleep(DELAY)
                cur_x -= 1
        else:
            move_to(x, y)
            controller.A_HOLD()
            time.sleep(DELAY)
            for _ in range(length - 1):
                controller.RIGHT()
                time.sleep(DELAY)
                cur_x += 1

        controller.A_RELEASE()
        time.sleep(DELAY)


def render_image(path):
    print(f"Loading {path}...")
    pixel_map = load_and_quantize(path)
    preview_image(pixel_map)
    time.sleep(5)

    by_color = defaultdict(list)
    for (x, y), hex_str in pixel_map.items():
        by_color[hex_str].append((x, y))

    print("Color breakdown:")
    for hex_str in COLOUR_PALLETE_TYPE.flat_colours():
        if hex_str in by_color:
            print(f"  {hex_str}: {len(by_color[hex_str])} pixels")

    print("Rendering...")
    controller.L3()
    time.sleep(DELAY)
    for hex_str in COLOUR_PALLETE_TYPE.flat_colours():
        if hex_str not in by_color:
            continue
        print(f"  Drawing {hex_str}...")
        switch_color(hex_str)
        runs = get_runs(by_color[hex_str])
        draw_runs(runs)

    print("Done!")


controller.A_RELEASE()
align_cursor()
render_image(IMG)
controller.L3()