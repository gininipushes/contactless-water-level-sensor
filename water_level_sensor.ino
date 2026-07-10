// ─── Water Level Monitor — Reference Corrected + Temporal Filter ─────────────

#include <WiFi.h>
#include <WebServer.h>

// ── Water sensor pins (low → high) ──
const int NUM_PINS             = 5;
const int touchPins[NUM_PINS]  = {32, 33, 27, 14, 12};
const int levels[NUM_PINS]     = {20, 40, 60, 80, 100};

// ── Reference pin (never touches water) ──
const int REF_PIN              = 13;   // GPIO13 (T4) — mount above 100% mark
int       refBaseline          = 0;    // set during first loop cycles

// ── Temporal filter settings ──
const int BUFFER_SIZE          = 10;   // readings kept per pin
const int CONFIRM_THRESHOLD    = 7;    // out of BUFFER_SIZE, how many must agree

// ── Paste your thresholds from calibration here ──
int thresholds[NUM_PINS] = {50, 50, 50, 50, 50};  // <-- replace
// ─────────────────────────────────────────────────────────────────────────────

const char* ssid     = "ESP32_Touch";
const char* password = "12345678";

WebServer server(80);
WiFiClient sseClient;
bool clientConnected  = false;
int  currentLevel     = 0;

// ── Circular buffers ─────────────────────────────────────────────────────────
int  readingBuffer[NUM_PINS][BUFFER_SIZE];
int  bufferIndex[NUM_PINS];
bool bufferFull[NUM_PINS];

void initBuffers() {
  for (int i = 0; i < NUM_PINS; i++) {
    bufferIndex[i] = 0;
    bufferFull[i]  = false;
    for (int j = 0; j < BUFFER_SIZE; j++) readingBuffer[i][j] = 9999;
  }
}

// Push new reading into circular buffer
void pushReading(int pin, int val) {
  readingBuffer[pin][bufferIndex[pin]] = val;
  bufferIndex[pin] = (bufferIndex[pin] + 1) % BUFFER_SIZE;
  if (bufferIndex[pin] == 0) bufferFull[pin] = true;
}

// Returns true if majority of buffer readings say "wet"
bool isPinWet(int pinIdx) {
  int count   = 0;
  int samples = bufferFull[pinIdx] ? BUFFER_SIZE : bufferIndex[pinIdx];
  if (samples == 0) return false;
  for (int j = 0; j < samples; j++) {
    if (readingBuffer[pinIdx][j] < thresholds[pinIdx]) count++;
  }
  return count >= CONFIRM_THRESHOLD;
}

// ── Reference correction ──────────────────────────────────────────────────────
// Returns how much the environment has drifted from calibration baseline.
// Since touch values DROP when capacitance increases, a negative drift means
// the environment is adding capacitance — we add it back to each threshold.
int getRefDrift() {
  if (refBaseline == 0) return 0; // not calibrated yet
  int refNow = touchRead(REF_PIN);
  return refBaseline - refNow;    // positive = env raised capacitance (readings dropped)
}

// ── SSE ───────────────────────────────────────────────────────────────────────
void sendSSE(String msg) {
  if (clientConnected && sseClient.connected()) {
    sseClient.print("data: " + msg + "\n\n");
  }
}

// ── HTML page ─────────────────────────────────────────────────────────────────
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Water Level</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: sans-serif;
      background: #0a1628;
      color: #eee;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 30px 16px;
    }
    h1 { color: #4fc3f7; font-size: 1.4em; margin-bottom: 4px; letter-spacing: 1px; }
    #status { font-size: 0.8em; color: #555; margin-bottom: 30px; }
    #pct-label {
      font-size: 3em;
      font-weight: bold;
      color: #4fc3f7;
      margin-bottom: 6px;
      transition: all 0.6s;
    }
    #level-text { font-size: 1em; color: #aaa; margin-bottom: 6px; }
    #drift-label { font-size: 0.75em; color: #666; margin-bottom: 20px; }
    .vessel-wrap { position: relative; width: 160px; }
    .vessel-top {
      width: 160px; height: 30px;
      border-radius: 50%;
      border: 4px solid #4fc3f7;
      background: transparent;
      position: relative; z-index: 3;
    }
    .vessel-body {
      width: 160px; height: 320px;
      border-left: 4px solid #4fc3f7;
      border-right: 4px solid #4fc3f7;
      border-bottom: 4px solid #4fc3f7;
      border-radius: 0 0 16px 16px;
      overflow: hidden;
      position: relative;
      background: #0d1f3c;
    }
    #water-fill {
      position: absolute;
      bottom: 0; left: 0; right: 0;
      height: 0%;
      background: linear-gradient(180deg, #29b6f6 0%, #0277bd 100%);
      transition: height 1s cubic-bezier(0.4, 0, 0.2, 1);
      border-radius: 0 0 12px 12px;
    }
    #water-fill::before {
      content: '';
      position: absolute;
      top: -6px; left: -10px; right: -10px;
      height: 12px;
      background: rgba(144, 202, 249, 0.5);
      border-radius: 50%;
      animation: wave 2s ease-in-out infinite alternate;
    }
    @keyframes wave {
      from { transform: scaleX(1.0) translateY(0px); }
      to   { transform: scaleX(1.05) translateY(3px); }
    }
    .tick {
      position: absolute;
      right: -36px;
      display: flex; align-items: center;
      font-size: 0.7em; color: #4fc3f7;
    }
    .tick::before {
      content: ''; display: block;
      width: 10px; height: 2px;
      background: #4fc3f7; margin-right: 4px;
    }
    .vessel-bottom {
      width: 160px; height: 30px;
      border-radius: 50%;
      border: 4px solid #4fc3f7;
      background: #0a1628;
      margin-top: -15px;
      position: relative; z-index: 3;
    }
    .sensor-row {
      display: flex; gap: 10px;
      margin-top: 30px;
      flex-wrap: wrap; justify-content: center;
    }
    .sensor-dot {
      display: flex; flex-direction: column;
      align-items: center; gap: 5px;
      font-size: 0.75em; color: #555;
    }
    .dot {
      width: 14px; height: 14px;
      border-radius: 50%;
      background: #222; border: 2px solid #333;
      transition: all 0.3s;
    }
    .dot.active {
      background: #29b6f6; border-color: #29b6f6;
      box-shadow: 0 0 8px #29b6f6;
    }
    /* reference indicator */
    #ref-box {
      margin-top: 20px;
      background: #111e30;
      border: 1px solid #1e3a5f;
      border-radius: 8px;
      padding: 10px 18px;
      font-size: 0.8em;
      color: #607d8b;
      text-align: center;
    }
    #ref-box span { color: #4fc3f7; font-weight: bold; }
  </style>
</head>
<body>
  <h1>Water Level Monitor</h1>
  <div id="status">Connecting...</div>
  <div id="pct-label">0%</div>
  <div id="level-text">Empty</div>
  <div id="drift-label">Env drift: <span id="drift-val">--</span></div>

  <div class="vessel-wrap">
    <div class="vessel-top"></div>
    <div class="vessel-body">
      <div class="tick" style="bottom: 80%;">100%</div>
      <div class="tick" style="bottom: 60%;">80%</div>
      <div class="tick" style="bottom: 40%;">60%</div>
      <div class="tick" style="bottom: 20%;">40%</div>
      <div class="tick" style="bottom:  5%;">20%</div>
      <div id="water-fill"></div>
    </div>
    <div class="vessel-bottom"></div>
  </div>

  <div class="sensor-row">
    <div class="sensor-dot"><div class="dot" id="dot-20"></div>20%</div>
    <div class="sensor-dot"><div class="dot" id="dot-40"></div>40%</div>
    <div class="sensor-dot"><div class="dot" id="dot-60"></div>60%</div>
    <div class="sensor-dot"><div class="dot" id="dot-80"></div>80%</div>
    <div class="sensor-dot"><div class="dot" id="dot-100"></div>100%</div>
  </div>

  <div id="ref-box">
    Reference sensor drift: <span id="ref-drift">--</span>
    &nbsp;|&nbsp; Raw ref: <span id="ref-raw">--</span>
  </div>

  <script>
    const levelTexts = {
      0: "Empty", 20: "Very Low", 40: "Low",
      60: "Half Full", 80: "Almost Full", 100: "Full!"
    };

    function updateUI(data) {
      const level = data.level;
      const drift = data.drift;
      const refRaw = data.ref;

      document.getElementById("water-fill").style.height = level + "%";
      document.getElementById("pct-label").textContent   = level + "%";
      document.getElementById("level-text").textContent  = levelTexts[level] || level + "%";
      document.getElementById("drift-val").textContent   = drift;
      document.getElementById("ref-drift").textContent   = (drift >= 0 ? "+" : "") + drift;
      document.getElementById("ref-raw").textContent     = refRaw;

      [20, 40, 60, 80, 100].forEach(l => {
        const dot = document.getElementById("dot-" + l);
        if (dot) dot.classList.toggle("active", l <= level && level > 0);
      });
    }

    const evtSource = new EventSource("/events");
    evtSource.onopen  = () => document.getElementById("status").textContent = "Connected to ESP32";
    evtSource.onerror = () => document.getElementById("status").textContent = "Disconnected — refresh to reconnect";

    evtSource.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        updateUI(data);
      } catch(e) {}
    };
  </script>
</body>
</html>
)rawliteral";

void handleRoot()   { server.send_P(200, "text/html", htmlPage); }

void handleEvents() {
  sseClient = server.client();
  clientConnected = true;
  sseClient.print("HTTP/1.1 200 OK\r\n");
  sseClient.print("Content-Type: text/event-stream\r\n");
  sseClient.print("Cache-Control: no-cache\r\n");
  sseClient.print("Connection: keep-alive\r\n\r\n");
  sseClient.flush();
  // push current state immediately
  String payload = "{\"level\":" + String(currentLevel) +
                   ",\"drift\":" + String(getRefDrift()) +
                   ",\"ref\":"   + String(touchRead(REF_PIN)) + "}";
  sseClient.print("data: " + payload + "\n\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  initBuffers();

  // Warm up reference baseline over 20 samples
  Serial.println("Warming up reference sensor...");
  long refSum = 0;
  for (int i = 0; i < 20; i++) { refSum += touchRead(REF_PIN); delay(50); }
  refBaseline = (int)(refSum / 20);
  Serial.print("Reference baseline: ");
  Serial.println(refBaseline);

  WiFi.softAP(ssid, password);
  Serial.println("AP started!");
  Serial.println("Connect phone to WiFi: ESP32_Touch  |  Password: 12345678");
  Serial.println("Then open browser to:  http://192.168.4.1");

  server.on("/",       handleRoot);
  server.on("/events", handleEvents);
  server.begin();
}

void loop() {
  server.handleClient();

  int drift = getRefDrift();

  // Apply drift correction to thresholds and push readings into buffer
  for (int i = 0; i < NUM_PINS; i++) {
    int correctedThreshold = thresholds[i] - drift; // compensate env shift
    int rawVal = touchRead(touchPins[i]);
    // Store corrected comparison result as binary in buffer
    pushReading(i, rawVal + drift); // normalize reading back to calibration baseline
  }

  // Determine highest confirmed wet level
  int detectedLevel = 0;
  for (int i = 0; i < NUM_PINS; i++) {
    if (isPinWet(i)) detectedLevel = levels[i];
  }

  if (detectedLevel != currentLevel) {
    currentLevel = detectedLevel;
    Serial.print("Water level: "); Serial.print(currentLevel); Serial.println("%");
    Serial.print("Env drift:   "); Serial.println(drift);
  }

  // Send SSE update every 500ms regardless (keeps drift display fresh on phone)
  static unsigned long lastSend = 0;
  if (millis() - lastSend > 500) {
    String payload = "{\"level\":" + String(currentLevel) +
                     ",\"drift\":" + String(drift) +
                     ",\"ref\":"   + String(touchRead(REF_PIN)) + "}";
    sendSSE(payload);
    lastSend = millis();
  }

  delay(100); // 10 readings/sec into buffer
}
