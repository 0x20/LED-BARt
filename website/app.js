// ============================================================
// Constants
// ============================================================
var LEDBART_URL = "http://ledbart.local";
var WS_URL = "ws://ledbart.local:81";
var DISPLAY_WIDTH = 95;
var DISPLAY_HEIGHT = 7;
var MAX_CHARS = 19;

var PX = 4;
var GAP = 1;
var STEP = PX + GAP;

// ============================================================
// Frame utilities (depends on Font5x7 from font5x7.js)
// ============================================================
function createFrame() {
  return new Uint8Array(DISPLAY_WIDTH);
}

function setPixel(frame, x, y, on) {
  if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return;
  if (on) frame[x] |= (1 << y);
  else frame[x] &= ~(1 << y);
}

function getPixel(frame, x, y) {
  if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) return 0;
  return (frame[x] >> y) & 1;
}

function textToColumns(text) {
  var cols = [];
  for (var i = 0; i < text.length; i++) {
    var ch = text.charCodeAt(i);
    if (ch < 32 || ch > 126) ch = 32;
    for (var c = 0; c < 5; c++) {
      cols.push(Font5x7[(ch - 32) * 5 + c]);
    }
  }
  return cols;
}

function textToFrame(text) {
  var frame = createFrame();
  var cols = textToColumns(text);
  for (var i = 0; i < DISPLAY_WIDTH && i < cols.length; i++) {
    frame[i] = cols[i];
  }
  return frame;
}

// ============================================================
// Canvas renderer
// ============================================================
var canvas = document.getElementById("led-canvas");
var ctx = canvas.getContext("2d");
canvas.width = DISPLAY_WIDTH * STEP - GAP;
canvas.height = DISPLAY_HEIGHT * STEP - GAP;

function drawFrame(frame) {
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, canvas.width, canvas.height);

  for (var y = 0; y < DISPLAY_HEIGHT; y++) {
    for (var x = 0; x < DISPLAY_WIDTH; x++) {
      var lit = (frame[x] >> y) & 1;
      var px = x * STEP;
      var py = y * STEP;

      if (lit) {
        ctx.fillStyle = "#ff2200";
        ctx.shadowColor = "rgba(255,34,0,0.6)";
        ctx.shadowBlur = 3;
      } else {
        ctx.fillStyle = "#1a0800";
        ctx.shadowColor = "transparent";
        ctx.shadowBlur = 0;
      }
      ctx.fillRect(px, py, PX, PX);
    }
  }
  ctx.shadowBlur = 0;
}

// ============================================================
// WebSocket manager
// ============================================================
var wsManager = {
  ws: null,
  connected: false,
  reconnectDelay: 1000,
  reconnectTimer: null,
  dot: document.getElementById("connection-dot"),

  connect: function() {
    if (this.ws && (this.ws.readyState === WebSocket.CONNECTING || this.ws.readyState === WebSocket.OPEN)) return;
    try {
      this.ws = new WebSocket(WS_URL);
      this.ws.binaryType = "arraybuffer";
    } catch (e) {
      this._scheduleReconnect();
      return;
    }

    var self = this;
    this.ws.onopen = function() {
      self.connected = true;
      self.reconnectDelay = 1000;
      self.dot.className = "connection-dot connected";
      self.dot.title = "WebSocket connected";
    };
    this.ws.onclose = function() {
      self.connected = false;
      self.dot.className = "connection-dot disconnected";
      self.dot.title = "WebSocket disconnected";
      self._scheduleReconnect();
    };
    this.ws.onerror = function() {
      self.ws.close();
    };
  },

  _scheduleReconnect: function() {
    var self = this;
    clearTimeout(this.reconnectTimer);
    this.reconnectTimer = setTimeout(function() {
      self.reconnectDelay = Math.min(self.reconnectDelay * 2, 10000);
      self.connect();
    }, this.reconnectDelay);
  },

  sendFrame: function(frame) {
    if (this.ws && this.ws.readyState === WebSocket.OPEN) {
      this.ws.send(frame.buffer);
    }
  },

  disconnect: function() {
    clearTimeout(this.reconnectTimer);
    if (this.ws) this.ws.close();
  }
};


// ============================================================
// Pixel editor
// ============================================================
var editor = {
  frame: createFrame(),
  painting: false,
  paintValue: 1,

  canvasToGrid: function(clientX, clientY) {
    var rect = canvas.getBoundingClientRect();
    var scaleX = canvas.width / rect.width;
    var scaleY = canvas.height / rect.height;
    var cx = (clientX - rect.left) * scaleX;
    var cy = (clientY - rect.top) * scaleY;
    var gx = Math.floor(cx / STEP);
    var gy = Math.floor(cy / STEP);
    if (gx >= 0 && gx < DISPLAY_WIDTH && gy >= 0 && gy < DISPLAY_HEIGHT) {
      return { x: gx, y: gy };
    }
    return null;
  },

  handleStart: function(x, y) {
    this.painting = true;
    this.paintValue = getPixel(this.frame, x, y) ? 0 : 1;
    setPixel(this.frame, x, y, this.paintValue);
    drawFrame(this.frame);
    wsManager.sendFrame(this.frame);
  },

  handleMove: function(x, y) {
    if (!this.painting) return;
    setPixel(this.frame, x, y, this.paintValue);
    drawFrame(this.frame);
    wsManager.sendFrame(this.frame);
  },

  handleEnd: function() {
    this.painting = false;
  },

  clear: function() {
    this.frame = createFrame();
    drawFrame(this.frame);
    wsManager.sendFrame(this.frame);
  },

  fill: function() {
    this.frame = createFrame();
    for (var x = 0; x < DISPLAY_WIDTH; x++) this.frame[x] = 0x7F;
    drawFrame(this.frame);
    wsManager.sendFrame(this.frame);
  },

  send: function() {
    var hex = "";
    for (var i = 0; i < DISPLAY_WIDTH; i++) {
      var h = this.frame[i].toString(16);
      if (h.length < 2) h = "0" + h;
      hex += h;
    }
    var statusEl = document.getElementById("editor-status");
    statusEl.textContent = "Sending...";
    statusEl.className = "status";
    fetch(LEDBART_URL + "/pixels", {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: hex
    }).then(function(r) {
      if (r.ok) {
        statusEl.textContent = "Sent!";
        statusEl.className = "status ok";
      } else {
        statusEl.textContent = "Error: " + r.status;
        statusEl.className = "status err";
      }
    }).catch(function() {
      statusEl.textContent = "Connection failed";
      statusEl.className = "status err";
    });
  }
};

// ============================================================
// Tab/UI controller
// ============================================================
var currentTab = "tab-text";

function switchTab(tabId) {
  document.querySelector(".led-preview").classList.remove("editing");

  document.querySelectorAll(".tab").forEach(function(t) {
    t.classList.toggle("active", t.dataset.tab === tabId);
  });
  document.querySelectorAll(".tab-panel").forEach(function(p) {
    p.classList.toggle("active", p.id === tabId);
  });

  currentTab = tabId;

  if (tabId === "tab-text") {
    updateTextPreview();
  } else if (tabId === "tab-editor") {
    document.querySelector(".led-preview").classList.add("editing");
    drawFrame(editor.frame);
    wsManager.connect();
  }
}

document.querySelectorAll(".tab").forEach(function(t) {
  t.addEventListener("click", function() { switchTab(t.dataset.tab); });
});

// --- Text Tab ---
var textInput = document.getElementById("text-input");
var charCount = document.getElementById("char-count");
var sendBtn = document.getElementById("send-btn");
var textStatus = document.getElementById("text-status");

function updateTextPreview() {
  var text = textInput.value;
  while (text.length < MAX_CHARS) text += " ";
  drawFrame(textToFrame(text));
  var len = textInput.value.length;
  charCount.textContent = len + " / " + MAX_CHARS;
  charCount.className = "char-count" + (len > MAX_CHARS ? " over" : "");
}

async function sendText() {
  var text = textInput.value.trim();
  if (!text) return;
  sendBtn.disabled = true;
  textStatus.textContent = "Sending...";
  textStatus.className = "status";
  try {
    var r = await fetch(LEDBART_URL + "/text", {
      method: "POST",
      headers: { "Content-Type": "text/plain" },
      body: text
    });
    if (r.ok) {
      textStatus.textContent = "Sent!";
      textStatus.className = "status ok";
    } else {
      textStatus.textContent = "Error: " + r.status;
      textStatus.className = "status err";
    }
  } catch (e) {
    textStatus.textContent = "Connection failed";
    textStatus.className = "status err";
  }
  sendBtn.disabled = false;
}

textInput.addEventListener("input", updateTextPreview);
textInput.addEventListener("keydown", function(e) { if (e.key === "Enter") sendText(); });
sendBtn.addEventListener("click", sendText);

// --- Effects Tab (server-side effects via HTTP API) ---
var effectStatus = document.getElementById("effect-status");
var stopBtn = document.getElementById("stop-btn");
var speedSlider = document.getElementById("speed-slider");
var speedValue = document.getElementById("speed-value");

function startEffect(name) {
  var needsText = ["scroll", "blink", "wave", "inverted", "pulse"];
  var text = document.getElementById("effect-text-input").value;
  if (needsText.indexOf(name) !== -1 && !text) {
    effectStatus.textContent = "Enter text first";
    effectStatus.className = "status err";
    return;
  }

  var body = { effect: name, speed: parseInt(speedSlider.value) };
  if (text) body.text = text;

  effectStatus.textContent = "Starting...";
  effectStatus.className = "status";

  fetch(LEDBART_URL + "/effect", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  }).then(function(r) {
    if (r.ok) {
      document.querySelectorAll(".effect-btn").forEach(function(b) { b.classList.remove("active"); });
      document.querySelector('[data-effect="' + name + '"]').classList.add("active");
      stopBtn.disabled = false;
      effectStatus.textContent = "Running: " + name;
      effectStatus.className = "status ok";
    } else {
      effectStatus.textContent = "Error: " + r.status;
      effectStatus.className = "status err";
    }
  }).catch(function() {
    effectStatus.textContent = "Connection failed";
    effectStatus.className = "status err";
  });
}

document.querySelectorAll(".effect-btn").forEach(function(btn) {
  btn.addEventListener("click", function() {
    startEffect(btn.dataset.effect);
  });
});

stopBtn.addEventListener("click", function() {
  fetch(LEDBART_URL + "/effect", { method: "DELETE" }).then(function() {
    document.querySelectorAll(".effect-btn").forEach(function(b) { b.classList.remove("active"); });
    stopBtn.disabled = true;
    effectStatus.textContent = "Stopped";
    effectStatus.className = "status";
    drawFrame(createFrame());
  }).catch(function() {
    effectStatus.textContent = "Connection failed";
    effectStatus.className = "status err";
  });
});

speedSlider.addEventListener("input", function() {
  var val = parseInt(speedSlider.value);
  speedValue.textContent = val + "ms";
  // If an effect is running, restart it with the new speed
  var activeBtn = document.querySelector(".effect-btn.active");
  if (activeBtn) {
    startEffect(activeBtn.dataset.effect);
  }
});

// --- Pixel Editor (mouse/touch on canvas) ---
function handleEditorPointer(e, type) {
  if (currentTab !== "tab-editor") return;
  e.preventDefault();
  var touch = e.touches ? e.touches[0] : e;
  var pos = editor.canvasToGrid(touch.clientX, touch.clientY);
  if (!pos) return;
  if (type === "start") editor.handleStart(pos.x, pos.y);
  else if (type === "move") editor.handleMove(pos.x, pos.y);
}

canvas.addEventListener("mousedown", function(e) { handleEditorPointer(e, "start"); });
canvas.addEventListener("mousemove", function(e) { handleEditorPointer(e, "move"); });
document.addEventListener("mouseup", function() { editor.handleEnd(); });

canvas.addEventListener("touchstart", function(e) { handleEditorPointer(e, "start"); }, { passive: false });
canvas.addEventListener("touchmove", function(e) { handleEditorPointer(e, "move"); }, { passive: false });
document.addEventListener("touchend", function() { editor.handleEnd(); });

document.getElementById("editor-clear").addEventListener("click", function() { editor.clear(); });
document.getElementById("editor-fill").addEventListener("click", function() { editor.fill(); });

// --- Log section ---
var logBox = document.getElementById("log-box");
var logToggle = document.getElementById("log-toggle");

async function getLog() {
  try {
    var r = await fetch(LEDBART_URL + "/log");
    logBox.textContent = await r.text();
    logBox.scrollTop = logBox.scrollHeight;
  } catch (e) {
    logBox.textContent = "(could not reach LED-BARt)";
  }
}

logToggle.addEventListener("click", function() {
  var hidden = logBox.style.display === "none";
  logBox.style.display = hidden ? "block" : "none";
  logToggle.innerHTML = (hidden ? "\u25be" : "\u25b8") + " Log";
  if (hidden) getLog();
});

// ============================================================
// LAN detection
// ============================================================
var HACKERSPACE_SUBNET = "10.51.2.84";
var SUBNET_MASK = "255.255.252.0";

function ipToInt(ip) {
  return ip.split(".").reduce(function(n, o) { return (n << 8) | +o; }, 0) >>> 0;
}

function getLocalIPs() {
  return new Promise(function(resolve) {
    var ips = [], pc = new RTCPeerConnection({ iceServers: [] });
    pc.createDataChannel("");
    pc.createOffer().then(function(o) { pc.setLocalDescription(o); });
    pc.onicecandidate = function(e) {
      if (!e.candidate) { pc.close(); resolve(ips); return; }
      var m = e.candidate.candidate.match(/(\d+\.\d+\.\d+\.\d+)/);
      if (m) ips.push(m[1]);
    };
    setTimeout(function() { pc.close(); resolve(ips); }, 3000);
  });
}

(async function checkLAN() {
  var ips = await getLocalIPs();
  if (!ips.length) return;
  var mask = ipToInt(SUBNET_MASK);
  var net = ipToInt(HACKERSPACE_SUBNET) & mask;
  var onLAN = ips.some(function(ip) { return (ipToInt(ip) & mask) === net; });
  if (!onLAN) document.getElementById("lan-warning").style.display = "block";
})();

// ============================================================
// Init
// ============================================================
updateTextPreview();
