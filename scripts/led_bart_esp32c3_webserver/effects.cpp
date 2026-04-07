#include "effects.h"

// ============================================================
// Shared state (declared extern in effects.h)
// ============================================================
String activeEffect = "";
String effectText = "";
int    effectSpeed = 100;
unsigned long lastFrame = 0;
uint8_t frameBuf[NUM_COLS];

// ============================================================
// Per-effect state
// ============================================================

// Scroll
static int scrollOffset = 0;
static int scrollColsLen = 0;
static uint8_t scrollCols[MAX_SCROLL_COLS];

// Blink
static bool blinkOn = true;

// Wave
static int waveTick = 0;

// Rain
static uint8_t rainDrops[NUM_COLS];

// Game of Life
static uint8_t golGrid[DISPLAY_HEIGHT][NUM_COLS];
static uint8_t golPrevFrame[NUM_COLS];
static int golStale = 0;

// Pulse
static int pulseTick = 0;

// ============================================================
// Helpers
// ============================================================

int textToColumns(const char *text, int textLen, uint8_t *out, int maxCols) {
  int col = 0;
  for (int i = 0; i < textLen && col + 5 <= maxCols; i++) {
    uint8_t ch = text[i];
    if (ch < 32 || ch > 126) ch = ' ';
    int idx = (ch - 32) * 5;
    for (int k = 0; k < 5; k++) {
      out[col++] = Font5x7[idx + k];
    }
  }
  return col;
}

static void renderTextToFrameBuf(const String &text) {
  String padded = text;
  while ((int)padded.length() < TEKST_LEN) padded += ' ';
  memset(frameBuf, 0, NUM_COLS);
  textToColumns(padded.c_str(), TEKST_LEN, frameBuf, NUM_COLS);
}

// ============================================================
// Init functions
// ============================================================

static void initScroll() {
  String pad = "";
  for (int i = 0; i < TEKST_LEN; i++) pad += ' ';
  String full = pad + effectText + pad;
  memset(scrollCols, 0, MAX_SCROLL_COLS);
  scrollColsLen = textToColumns(full.c_str(), full.length(), scrollCols, MAX_SCROLL_COLS);
  scrollOffset = 0;
}

static void initBlink() { blinkOn = true; }
static void initWave()  { waveTick = 0; }

static void initRain() {
  memset(rainDrops, 0, NUM_COLS);
  for (int i = 0; i < 30; i++) {
    int x = random(NUM_COLS);
    int y = random(DISPLAY_HEIGHT);
    rainDrops[x] |= (1 << y);
  }
}

static void initGol() {
  for (int y = 0; y < DISPLAY_HEIGHT; y++)
    for (int x = 0; x < NUM_COLS; x++)
      golGrid[y][x] = (random(100) < 30) ? 1 : 0;
  memset(golPrevFrame, 0, NUM_COLS);
  golStale = 0;
}

static void initPulse() { pulseTick = 0; }

// ============================================================
// Tick functions
// ============================================================

static void tickScroll() {
  memset(frameBuf, 0, NUM_COLS);
  for (int i = 0; i < NUM_COLS; i++) {
    int ci = scrollOffset + i;
    if (ci >= 0 && ci < scrollColsLen)
      frameBuf[i] = scrollCols[ci];
  }
  scrollOffset++;
  if (scrollOffset >= scrollColsLen) scrollOffset = 0;
}

static void tickBlink() {
  blinkOn = !blinkOn;
  if (blinkOn) renderTextToFrameBuf(effectText);
  else memset(frameBuf, 0, NUM_COLS);
}

static void tickWave() {
  uint8_t textCols[NUM_COLS];
  memset(textCols, 0, NUM_COLS);
  String padded = effectText;
  while ((int)padded.length() < TEKST_LEN) padded += ' ';
  textToColumns(padded.c_str(), TEKST_LEN, textCols, NUM_COLS);

  memset(frameBuf, 0, NUM_COLS);
  for (int x = 0; x < NUM_COLS; x++) {
    int shift = (int)round(sin((x + waveTick) * 0.15) * 1.5);
    uint8_t srcCol = textCols[x];
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
      int srcY = y - shift;
      if (srcY >= 0 && srcY < DISPLAY_HEIGHT && ((srcCol >> srcY) & 1))
        frameBuf[x] |= (1 << y);
    }
  }
  waveTick++;
}

static void tickRain() {
  uint8_t next[NUM_COLS];
  memset(next, 0, NUM_COLS);
  for (int x = 0; x < NUM_COLS; x++) {
    for (int y = DISPLAY_HEIGHT - 1; y >= 0; y--) {
      if ((rainDrops[x] >> y) & 1) {
        if (y + 1 < DISPLAY_HEIGHT)
          next[x] |= (1 << (y + 1));
        else
          next[random(NUM_COLS)] |= (1 << 0);
      }
    }
  }
  if (random(100) < 30)
    next[random(NUM_COLS)] |= (1 << 0);
  memcpy(rainDrops, next, NUM_COLS);
  memcpy(frameBuf, next, NUM_COLS);
}

static void tickSparkle() {
  memset(frameBuf, 0, NUM_COLS);
  int count = 20 + random(30);
  for (int i = 0; i < count; i++)
    frameBuf[random(NUM_COLS)] |= (1 << random(DISPLAY_HEIGHT));
}

static int golCountNeighbors(int gx, int gy) {
  int count = 0;
  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {
      if (dy == 0 && dx == 0) continue;
      int ny = (gy + dy + DISPLAY_HEIGHT) % DISPLAY_HEIGHT;
      int nx = (gx + dx + NUM_COLS) % NUM_COLS;
      count += golGrid[ny][nx];
    }
  }
  return count;
}

static void tickGol() {
  memset(frameBuf, 0, NUM_COLS);
  int alive = 0;
  for (int x = 0; x < NUM_COLS; x++) {
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
      if (golGrid[y][x]) {
        frameBuf[x] |= (1 << y);
        alive++;
      }
    }
  }

  bool same = (memcmp(frameBuf, golPrevFrame, NUM_COLS) == 0);
  if (same) golStale++;
  else golStale = 0;
  memcpy(golPrevFrame, frameBuf, NUM_COLS);

  if (alive == 0 || golStale > 10) {
    initGol();
    return;
  }

  uint8_t next[DISPLAY_HEIGHT][NUM_COLS];
  for (int y = 0; y < DISPLAY_HEIGHT; y++)
    for (int x = 0; x < NUM_COLS; x++) {
      int n = golCountNeighbors(x, y);
      next[y][x] = ((golGrid[y][x] && n == 2) || n == 3) ? 1 : 0;
    }
  memcpy(golGrid, next, sizeof(golGrid));
}

static void tickPulse() {
  uint8_t textCols[NUM_COLS];
  memset(textCols, 0, NUM_COLS);
  String padded = effectText;
  while ((int)padded.length() < TEKST_LEN) padded += ' ';
  textToColumns(padded.c_str(), TEKST_LEN, textCols, NUM_COLS);

  float level = fabs(sin(pulseTick * 0.08)) * DISPLAY_HEIGHT;
  int visibleRows = (int)round(level);
  float centerY = (DISPLAY_HEIGHT - 1) / 2.0;

  memset(frameBuf, 0, NUM_COLS);
  for (int x = 0; x < NUM_COLS; x++) {
    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
      float dist = fabs(y - centerY);
      if (dist <= visibleRows / 2.0 && ((textCols[x] >> y) & 1))
        frameBuf[x] |= (1 << y);
    }
  }
  pulseTick++;
}

// ============================================================
// Public API
// ============================================================

bool isValidEffect(const String &name) {
  return name == "scroll" || name == "blink" || name == "wave" ||
         name == "rain"   || name == "sparkle" || name == "gol" ||
         name == "inverted" || name == "pulse";
}

void stopEffect() {
  activeEffect = "";
  effectText = "";
}

bool initEffect() {
  memset(frameBuf, 0, NUM_COLS);
  lastFrame = millis();

  if (activeEffect == "scroll")       initScroll();
  else if (activeEffect == "blink")   initBlink();
  else if (activeEffect == "wave")    initWave();
  else if (activeEffect == "rain")    initRain();
  else if (activeEffect == "sparkle") { /* stateless */ }
  else if (activeEffect == "gol")     initGol();
  else if (activeEffect == "pulse")   initPulse();
  else if (activeEffect == "inverted") {
    // One-shot: render inverted text into frameBuf, caller sends it
    renderTextToFrameBuf(effectText);
    for (int i = 0; i < NUM_COLS; i++)
      frameBuf[i] = (~frameBuf[i]) & 0x7F;
    activeEffect = "";
    return true;
  }
  return false;
}

void tickEffect() {
  if (activeEffect == "scroll")       tickScroll();
  else if (activeEffect == "blink")   tickBlink();
  else if (activeEffect == "wave")    tickWave();
  else if (activeEffect == "rain")    tickRain();
  else if (activeEffect == "sparkle") tickSparkle();
  else if (activeEffect == "gol")     tickGol();
  else if (activeEffect == "pulse")   tickPulse();
}
