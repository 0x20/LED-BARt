// Effect definitions — each has init() and frame() methods.
// Depends on globals from app.js: createFrame, setPixel, getPixel,
// textToColumns, textToFrame, DISPLAY_WIDTH, DISPLAY_HEIGHT, MAX_CHARS
var effects = {};

function getEffectText() {
  return document.getElementById("effect-text-input").value || "LED-BARt";
}

// --- Scroll (marquee) ---
effects.scroll = {
  cols: [],
  offset: 0,
  init: function() {
    var text = getEffectText();
    var pad = "";
    for (var i = 0; i < MAX_CHARS; i++) pad += " ";
    this.cols = textToColumns(pad + text + pad);
    this.offset = 0;
  },
  frame: function() {
    var f = createFrame();
    for (var i = 0; i < DISPLAY_WIDTH; i++) {
      var ci = this.offset + i;
      if (ci >= 0 && ci < this.cols.length) f[i] = this.cols[ci];
    }
    this.offset++;
    if (this.offset >= this.cols.length) this.offset = 0;
    return f;
  }
};

// --- Blink ---
effects.blink = {
  on: true,
  init: function() { this.on = true; },
  frame: function() {
    this.on = !this.on;
    return this.on ? textToFrame(getEffectText()) : createFrame();
  }
};

// --- Wave (sinusoidal vertical shift) ---
effects.wave = {
  tick: 0,
  init: function() { this.tick = 0; },
  frame: function() {
    var cols = textToColumns(getEffectText());
    var f = createFrame();
    for (var x = 0; x < DISPLAY_WIDTH; x++) {
      var shift = Math.round(Math.sin((x + this.tick) * 0.15) * 1.5);
      var srcCol = x < cols.length ? cols[x] : 0;
      for (var y = 0; y < DISPLAY_HEIGHT; y++) {
        var srcY = y - shift;
        if (srcY >= 0 && srcY < DISPLAY_HEIGHT && ((srcCol >> srcY) & 1)) {
          f[x] |= (1 << y);
        }
      }
    }
    this.tick++;
    return f;
  }
};

// --- Rain (falling pixels) ---
effects.rain = {
  drops: null,
  init: function() {
    this.drops = createFrame();
    for (var i = 0; i < 30; i++) {
      setPixel(this.drops, Math.floor(Math.random() * DISPLAY_WIDTH), Math.floor(Math.random() * DISPLAY_HEIGHT), 1);
    }
  },
  frame: function() {
    var next = createFrame();
    for (var x = 0; x < DISPLAY_WIDTH; x++) {
      for (var y = DISPLAY_HEIGHT - 1; y >= 0; y--) {
        if (getPixel(this.drops, x, y)) {
          if (y + 1 < DISPLAY_HEIGHT) setPixel(next, x, y + 1, 1);
          else setPixel(next, Math.floor(Math.random() * DISPLAY_WIDTH), 0, 1);
        }
      }
    }
    if (Math.random() < 0.3) {
      setPixel(next, Math.floor(Math.random() * DISPLAY_WIDTH), 0, 1);
    }
    this.drops = next;
    return next;
  }
};

// --- Sparkle (random pixel flashes) ---
effects.sparkle = {
  init: function() {},
  frame: function() {
    var f = createFrame();
    var count = 20 + Math.floor(Math.random() * 30);
    for (var i = 0; i < count; i++) {
      setPixel(f, Math.floor(Math.random() * DISPLAY_WIDTH), Math.floor(Math.random() * DISPLAY_HEIGHT), 1);
    }
    return f;
  }
};

// --- Game of Life (toroidal wrap, auto-reseed) ---
effects.gol = {
  grid: null,
  prevCols: null,
  staleCount: 0,
  density: 0.3,

  init: function() {
    this.seed();
    this.prevCols = null;
    this.staleCount = 0;
  },

  seed: function() {
    this.grid = [];
    for (var y = 0; y < DISPLAY_HEIGHT; y++) {
      this.grid[y] = [];
      for (var x = 0; x < DISPLAY_WIDTH; x++) {
        this.grid[y][x] = Math.random() < this.density ? 1 : 0;
      }
    }
  },

  countNeighbors: function(gx, gy) {
    var count = 0;
    for (var dy = -1; dy <= 1; dy++) {
      for (var dx = -1; dx <= 1; dx++) {
        if (dy === 0 && dx === 0) continue;
        var ny = (gy + dy + DISPLAY_HEIGHT) % DISPLAY_HEIGHT;
        var nx = (gx + dx + DISPLAY_WIDTH) % DISPLAY_WIDTH;
        count += this.grid[ny][nx];
      }
    }
    return count;
  },

  step: function() {
    var next = [];
    for (var y = 0; y < DISPLAY_HEIGHT; y++) {
      next[y] = [];
      for (var x = 0; x < DISPLAY_WIDTH; x++) {
        var n = this.countNeighbors(x, y);
        next[y][x] = (this.grid[y][x] && n === 2) || n === 3 ? 1 : 0;
      }
    }
    this.grid = next;
  },

  frame: function() {
    var f = createFrame();
    var alive = 0;
    for (var x = 0; x < DISPLAY_WIDTH; x++) {
      for (var y = 0; y < DISPLAY_HEIGHT; y++) {
        if (this.grid[y][x]) {
          f[x] |= (1 << y);
          alive++;
        }
      }
    }
    var colStr = f.toString();
    if (colStr === this.prevCols) this.staleCount++;
    else this.staleCount = 0;
    this.prevCols = colStr;

    if (alive === 0 || this.staleCount > 10) {
      this.seed();
      this.staleCount = 0;
    } else {
      this.step();
    }
    return f;
  }
};

// --- Inverted (swap lit/unlit) ---
effects.inverted = {
  init: function() {},
  frame: function() {
    var base = textToFrame(getEffectText());
    var f = createFrame();
    for (var i = 0; i < DISPLAY_WIDTH; i++) {
      f[i] = (~base[i]) & 0x7F;
    }
    return f;
  }
};

// --- Pulse (text fades in/out via row masking from center) ---
effects.pulse = {
  tick: 0,
  init: function() { this.tick = 0; },
  frame: function() {
    var base = textToFrame(getEffectText());
    var level = Math.abs(Math.sin(this.tick * 0.08)) * DISPLAY_HEIGHT;
    var visibleRows = Math.round(level);
    var f = createFrame();
    var centerY = (DISPLAY_HEIGHT - 1) / 2;
    for (var x = 0; x < DISPLAY_WIDTH; x++) {
      for (var y = 0; y < DISPLAY_HEIGHT; y++) {
        var dist = Math.abs(y - centerY);
        if (dist <= visibleRows / 2 && ((base[x] >> y) & 1)) {
          f[x] |= (1 << y);
        }
      }
    }
    this.tick++;
    return f;
  }
};
