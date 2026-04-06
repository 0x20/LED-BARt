#!/usr/bin/env python3
"""Game of Life on the LED-BARt (95x7 pixel display).

Uses WebSocket (binary frames) for fast pixel updates.
Falls back to HTTP for text mode.
"""

import time
import random
import sys
import requests
import numpy as np
import websocket

LEDBAR_IP = "10.51.1.141"
LEDBAR_URL = f"http://{LEDBAR_IP}"
LEDBAR_WS = f"ws://{LEDBAR_IP}:81"
WIDTH = 95
HEIGHT = 7
CHARS = 19
FPS = 10


def grid_to_columns(grid):
    """Convert 95x7 grid to 95 column bytes (bit 0 = row 0, etc)."""
    cols = bytearray(WIDTH)
    for x in range(WIDTH):
        b = 0
        for y in range(HEIGHT):
            if grid[y][x]:
                b |= (1 << y)
        cols[x] = b
    return bytes(cols)


def step(grid):
    """Advance Game of Life one generation (toroidal wrap)."""
    padded = np.pad(grid, 1, mode='wrap')
    neighbors = sum(
        np.roll(np.roll(padded, dy, axis=0), dx, axis=1)
        for dy in (-1, 0, 1) for dx in (-1, 0, 1)
        if (dy, dx) != (0, 0)
    )[1:-1, 1:-1]
    return ((grid & (neighbors == 2)) | (neighbors == 3)).astype(np.int8)


def print_grid(grid):
    """Print grid to terminal for debugging."""
    for y in range(HEIGHT):
        print(''.join('█' if grid[y][x] else '·' for x in range(WIDTH)))
    print()


def send_text(text):
    """Send text via POST /text."""
    try:
        r = requests.post(
            f"{LEDBAR_URL}/text",
            headers={"Content-Type": "text/plain"},
            data=text,
            timeout=2,
        )
        return r.ok
    except requests.RequestException as e:
        print(f"Send failed: {e}", file=sys.stderr)
        return False


def gol_mode(density, fps):
    """Run Game of Life, send pixel data via WebSocket."""
    grid = np.array([
        [1 if random.random() < density else 0 for _ in range(WIDTH)]
        for _ in range(HEIGHT)
    ], dtype=np.int8)

    print(f"Game of Life on LED-BARt ({WIDTH}x{HEIGHT})")
    print(f"Density: {density}, FPS: {fps}")
    print(f"Connecting to {LEDBAR_WS}")

    ws = websocket.WebSocket()
    ws.connect(LEDBAR_WS)
    print("WebSocket connected. Ctrl+C to stop\n")

    gen = 0
    prev_cols = None
    stale_count = 0
    interval = 1.0 / fps

    try:
        while True:
            t0 = time.monotonic()

            cols = grid_to_columns(grid)
            alive = int(np.sum(grid))

            if cols == prev_cols:
                stale_count += 1
            else:
                stale_count = 0
            prev_cols = cols

            if alive == 0 or stale_count > 10:
                print(f"[gen {gen}] Reseeding (alive={alive}, stale={stale_count})")
                grid = np.array([
                    [1 if random.random() < density else 0 for _ in range(WIDTH)]
                    for _ in range(HEIGHT)
                ], dtype=np.int8)
                stale_count = 0
                continue

            if gen % 30 == 0:
                print(f"[gen {gen}] alive={alive:3d}")

            ws.send_binary(cols)

            grid = step(grid)
            gen += 1

            elapsed = time.monotonic() - t0
            remaining = interval - elapsed
            if remaining > 0:
                time.sleep(remaining)

    except KeyboardInterrupt:
        print("\nStopped.")
        ws.close()
        send_text("  Game  of  Life   ")


def counter_mode():
    """Fill screen pixel by pixel for testing."""
    print(f"Pixel fill test. Connecting to {LEDBAR_WS}")
    ws = websocket.WebSocket()
    ws.connect(LEDBAR_WS)
    print("WebSocket connected.\n")

    try:
        for n in range(1, WIDTH * HEIGHT + 1):
            cols = bytearray(WIDTH)
            pixel = 0
            for row in range(HEIGHT):
                for x in range(WIDTH):
                    if pixel < n:
                        cols[x] |= (1 << row)
                        pixel += 1
            ws.send_binary(bytes(cols))
        print("Fill complete.")
        time.sleep(2)
    except KeyboardInterrupt:
        pass
    ws.close()


def main():
    mode = sys.argv[1] if len(sys.argv) > 1 else "gol"

    if mode == "counter":
        counter_mode()
    else:
        density = float(sys.argv[2]) if len(sys.argv) > 2 else 0.3
        fps = float(sys.argv[3]) if len(sys.argv) > 3 else FPS
        gol_mode(density, fps)


if __name__ == "__main__":
    main()
