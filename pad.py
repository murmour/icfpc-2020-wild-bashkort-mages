
import io
import tkinter as tk
from subprocess import Popen, PIPE
import atexit
import re
import sys
from itertools import chain, cycle


eval_path = sys.argv[1]
images = []
pixels = set()
image_idx = 0
pixel_size = 10
process = None
combined_mode = True
undo_stack = []
number_overlay = None
min_x = 0
min_y = 0
len_x = 0
len_y = 0


root = tk.Tk()
root.geometry("1000x1000")

coord_label = tk.Label(root)
coord_label.place(x=0, y=0)

top_label = tk.Label(root)
top_label.pack()

canvas = tk.Canvas(root, width=0, height=0)
canvas.pack(fill=tk.BOTH, expand=tk.YES)


shortcuts = {}

def key(k):
    def d(f):
        global shortcuts
        shortcuts[k] = f
        return f
    return d


header_rx = re.compile(r'\-*@\((?P<x>\-?[0-9]+), (?P<y>\-?[0-9]+)\)$')

def read_image(fname: str):
    with io.open(fname, 'r') as h:
        header = h.readline()
        m = re.match(header_rx, header)
        if m is None:
            print(f'bad image: {fname} (header: {header})')
            return ([], 0, 0);
        lines = [l.strip() for l in h.readlines()]
        return (lines, int(m.group('x')), int(m.group('y')))


def update_pixels():
    global pixels, min_x, min_y, len_x, len_y
    pixels.clear()
    if images == []: return
    (_, min_x, min_y) = images[0]
    (len_x, len_y) = (0, 0)
    for (i, (lines, _min_x, _min_y)) in enumerate(images):
        _len_y = len(lines)
        if _len_y == 0: continue
        _len_x = len(lines[0])
        _max_x = max(min_x+len_x, _min_x+_len_x)
        _max_y = max(min_y+len_y, _min_y+_len_y)
        min_x = min(min_x, _min_x)
        min_y = min(min_y, _min_y)
        len_x = _max_x - min_x
        len_y = _max_y - min_y
        for y in range(_len_y):
            for x in range(_len_x):
                if lines[y][x] == '#':
                    pixels.add((i, _min_x+x, _min_y+y))


action_label = tk.Label(root)
action_label.pack()


def set_entry(entry, value):
    entry.delete(0, tk.END)
    entry.insert(0, value)


input_entry = tk.Entry(root)
input_entry.insert(0, '(100 100)')
input_entry.pack(fill=tk.X)

state_entry = tk.Entry(root)
state_entry.insert(0, '[2 [1 -1] 0 nil]') # galaxy
state_entry.pack(fill=tk.X)


def draw_pixel(x, y, color):
    x *= pixel_size
    y *= pixel_size
    canvas.create_rectangle(x, y, x+pixel_size, y+pixel_size, fill=color, width=0)


def update_single_image():
    x_pixels = len_x*pixel_size
    y_pixels = len_y*pixel_size
    canvas.config(width=x_pixels, height=y_pixels)
    canvas.create_rectangle(0, 0, x_pixels, y_pixels, fill='white')

    for i in range(min_x, min_x+len_x):
        for j in range(min_y, min_y+len_y):
            if (image_idx, i, j) in pixels:
                draw_pixel(i-min_x, j-min_y, 'black')

    text = f'Image {image_idx+1}/{len(images)} ({min_x}, {min_y})'
    top_label.config(text=text)


def update_combined_image():
    x_pixels = len_x*pixel_size
    y_pixels = len_y*pixel_size
    canvas.config(width=x_pixels, height=y_pixels)
    canvas.create_rectangle(0, 0, x_pixels, y_pixels, fill='white')

    colors = chain(['black'], cycle(['green', 'red', 'blue', 'purple', 'brown']))
    for (_image_idx, color) in zip(reversed(range(len(images))), colors):
        for i in range(min_x, min_x+len_x):
            for j in range(min_y, min_y+len_y):
                if (_image_idx, i, j) in pixels:
                    draw_pixel(i-min_x, j-min_y, color)

    text = f'Image {image_idx+1}/{len(images)} ({min_x}, {min_y})'
    top_label.config(text=f'{len(images)} combined images ({min_x}, {min_y})')


def fit_pixel_size():
    global pixel_size
    if len_x == 0 or len_y == 0: return
    cw = canvas.winfo_width()
    ch = canvas.winfo_height()
    pw = pixel_size * len_x
    ph = pixel_size * len_y
    new_pixel_size = pixel_size
    while pw + len_x <= cw and ph + len_y <= ch:
        pw += len_x
        ph += len_y
        new_pixel_size += 1
    while not (pw <= cw and ph <= ch):
        pw -= len_x
        ph -= len_y
        new_pixel_size -= 1
        if new_pixel_size <= 1: break
    if new_pixel_size != pixel_size:
        pixel_size = new_pixel_size
        update_image()


def on_resize(event):
    fit_pixel_size()


canvas.bind('<Configure>', on_resize)


def update_image():
    global number_overlay
    canvas.delete('all')
    number_overlay = None
    if images == []:
        top_label.config(text=f'No images')
        return
    if combined_mode:
        update_combined_image()
    else:
        update_single_image()
    fit_pixel_size()


@key("F3")
def undo():
    global images, image_idx
    if undo_stack != []:
        (old_images, old_input, old_state) = undo_stack.pop()
        set_entry(input_entry, old_input)
        set_entry(state_entry, old_state)
        images = old_images
        image_idx = 0
        update_pixels()
        update_image()


def send_inner(input, state):
    if process is None:
        return
    print(f'sending "{input}" and "{state}"')
    process.stdin.write(input.encode() + b'\n')
    process.stdin.write(state.encode() + b'\n')
    process.stdin.flush()
    image_count = int(process.stdout.readline().decode())
    if image_count == -1:
        print(f'got -1!')
        return
    state = process.stdout.readline().decode().strip()
    print(f'got "{image_count}" and "{state}"')
    return (image_count, state)


@key('Return')
def send():
    global images, image_idx
    if process is None:
        return
    input = input_entry.get()
    state = state_entry.get()
    undo_stack.append((images, input, state))
    response = send_inner(input, state)
    if response is None:
        return
    (image_count, state) = response
    images = [ read_image(f'image_{i}.log') for i in range(image_count) ]
    image_idx = 0
    update_pixels()
    update_image()
    set_entry(state_entry, state)


def on_click1(event):
    x = event.x // pixel_size
    y = event.y // pixel_size
    real_x = min_x + x
    real_y = min_y + y
    set_entry(input_entry, f'({real_x} {real_y})')
    send()

canvas.bind('<Button-1>', on_click1)


def scan_single_number(image_idx, x, y):
    def is_set(x, y):
        return (image_idx, x, y) in pixels

    # find top-left corner
    if not is_set(x, y):
        return None
    while is_set(x, y):
        y -= 1

    # scan frame
    nx = x+1
    ny = y+1
    while is_set(nx, y) and is_set(x, ny):
        nx += 1
        ny += 1

    if nx-x < 2 or nx-x > 10:
        return None

    negative = is_set(x, ny)

    # scan outer frame
    for i in range(x-1, nx+1):
        if is_set(i, y-1) or (i != x and is_set(i, ny)):
            return None
    for j in range(y-1, ny+1):
        if is_set(x-1, j) or is_set(nx, j):
            return None

    # calculate
    cur = 1
    sum = 0
    for j in range(y+1, ny):
        for i in range(x+1, nx):
            if is_set(i, j):
                sum += cur
            cur *= 2
    return sum


def scan_number(x, y):
    l = images if combined_mode else [images[image_idx]]
    real_x = min_x + x
    real_y = min_y + y
    for _image_idx in range(len(images)):
        i = scan_single_number(_image_idx, real_x, real_y)
        if i is not None:
            return i


def on_mouse_move(event):
    global number_overlay

    x = event.x // pixel_size
    y = event.y // pixel_size
    coord_label.config(text=f'x: {min_x+x}, y: {min_y+y}')

    i = scan_number(x, y)
    if i is None:
        if number_overlay is not None:
            (t, r) = number_overlay
            canvas.tag_lower(t)
            canvas.tag_lower(r)
        return

    overlay_pos_x = event.x
    overlay_pos_y = event.y-20

    if number_overlay is None:
        t = canvas.create_text((overlay_pos_x, overlay_pos_y), text=str(i), fill='white')
        r = canvas.create_rectangle(canvas.bbox(t), fill='black')
        canvas.tag_raise(t)
        number_overlay = (t, r)
    else:
        (t, r) = number_overlay
        canvas.itemconfig(t, text=str(i))
        canvas.coords(t, overlay_pos_x, overlay_pos_y)
        canvas.coords(r, canvas.bbox(t))
        canvas.tag_raise(r)
        canvas.tag_raise(t)

canvas.bind("<Motion>", on_mouse_move)


@key('Up')
def next_image():
    global image_idx
    image_idx += 1
    image_idx %= len(images)
    update_image()


@key('Down')
def prev_image():
    global image_idx
    image_idx -= 1
    image_idx %= len(images)
    update_image()


@key('F6')
def toggle_mode():
    global combined_mode
    combined_mode = not combined_mode
    update_image()


@key('F10')
def reload_eval():
    global process
    if eval_path is None: return
    if process is not None:
        process.kill()
        print('killed eval process')
    process = Popen([eval_path, 'loop'], stdin=PIPE, stdout=PIPE, shell=False)
    if process.poll() is not None:
        print("couldn't start eval process")
        process = None
    print('started eval process')


@key('Escape')
def quit():
    if process is not None:
        process.kill()
        print('killed eval process')
    exit(0)


def on_press(event):
    if event.keysym in shortcuts:
        shortcuts[event.keysym]()
    else:
        print(f'pressed {repr(event.keysym)}')

root.bind('<Key>', on_press)


legend_items = [f'{key}: {f.__name__}' for key, f in shortcuts.items()]
legend = ', '.join(legend_items)
bottom_label = tk.Label(root, text=(legend))
bottom_label.pack()


# for Windows
atexit.register(quit)

reload_eval()

update_image()

tk.mainloop()
