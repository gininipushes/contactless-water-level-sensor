// ─── Water Level Sensor Calibration (with Reference Pin) ─────────────────────

const int NUM_PINS    = 5;
const int NUM_SAMPLES = 20;

// Water sensor pins ordered lowest to highest
const int touchPins[NUM_PINS]      = {32, 33, 27, 14, 12};
const String pinLabels[NUM_PINS]   = {"D32(20%)", "D33(40%)", "D27(60%)", "D14(80%)", "D12(100%)"};
const String levelLabels[NUM_PINS] = {"20%", "40%", "60%", "80%", "100%"};

// Reference pin — never touches water, mounted above 100% mark
const int REF_PIN = 13;  // GPIO13 (T4)

int baselineReadings[NUM_PINS];
int touchedReadings[NUM_PINS];
int thresholds[NUM_PINS];
int refBaseline = 0;

int averagedRead(int pin) {
  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += touchRead(pin);
    delay(20);
  }
  return (int)(sum / NUM_SAMPLES);
}

void printArray(const String label, int arr[], int len) {
  Serial.print(label);
  Serial.print(" = {");
  for (int i = 0; i < len; i++) {
    Serial.print(arr[i]);
    if (i < len - 1) Serial.print(", ");
  }
  Serial.println("};");
}

void waitForKey(String prompt) {
  Serial.println(prompt);
  while (Serial.available() == 0) { delay(50); }
  while (Serial.available() > 0)  { Serial.read(); }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n========================================");
  Serial.println("    Water Level Sensor Calibration      ");
  Serial.println("========================================\n");

  // ── Step 1: Reference pin baseline ───────────────────────────────────────
  Serial.println("STEP 1: Calibrating reference sensor (GPIO13).");
  Serial.println("        Make sure bottle is EMPTY and nothing is touching any wire.");
  waitForKey(">> Press Enter when ready...");

  Serial.print("  Reading reference baseline... ");
  refBaseline = averagedRead(REF_PIN);
  Serial.println(refBaseline);
  Serial.println("  Reference baseline captured!\n");

  // ── Step 2: Water sensor baselines (all at once, bottle empty) ───────────
  Serial.println("STEP 2: Reading baseline for all water sensors (bottle still empty).");
  waitForKey(">> Press Enter when ready...");

  for (int i = 0; i < NUM_PINS; i++) {
    Serial.print("  Reading baseline for ");
    Serial.print(pinLabels[i]);
    Serial.print("... ");
    baselineReadings[i] = averagedRead(touchPins[i]);
    Serial.println(baselineReadings[i]);
  }
  Serial.println("  All baselines done!\n");

  // ── Step 3: Touched readings one by one ──────────────────────────────────
  Serial.println("STEP 3: Fill the bottle to each level when prompted.");
  Serial.println("        Keep the wire at that level submerged in water.\n");
  delay(1000);

  for (int i = 0; i < NUM_PINS; i++) {
    Serial.print("--> Fill bottle to ");
    Serial.print(levelLabels[i]);
    Serial.println(" — wire at that level should be submerged.");
    waitForKey(">> Press Enter when ready...");

    Serial.print("  Reading touched value for ");
    Serial.print(pinLabels[i]);
    Serial.print("... ");
    touchedReadings[i] = averagedRead(touchPins[i]);
    Serial.println(touchedReadings[i]);

    // Sanity check — touched should be lower than baseline
    if (touchedReadings[i] >= baselineReadings[i]) {
      Serial.println("  !! WARNING: Touched value is not lower than baseline.");
      Serial.println("     Make sure the wire tip is actually in the water.");
      Serial.println("     You can redo this level — press Enter to re-read.");
      waitForKey(">> Press Enter to re-read this level...");
      touchedReadings[i] = averagedRead(touchPins[i]);
      Serial.print("  Re-read value: ");
      Serial.println(touchedReadings[i]);
    }

    Serial.println("  Done!\n");
  }

  // ── Step 4: Compute thresholds ────────────────────────────────────────────
  for (int i = 0; i < NUM_PINS; i++) {
    thresholds[i] = (baselineReadings[i] + touchedReadings[i]) / 2;
  }

  // ── Results ───────────────────────────────────────────────────────────────
  Serial.println("========================================");
  Serial.println("          Calibration Results           ");
  Serial.println("========================================\n");

  printArray("int baselineReadings[]", baselineReadings, NUM_PINS);
  printArray("int touchedReadings[] ", touchedReadings,  NUM_PINS);
  printArray("int thresholds[]      ", thresholds,       NUM_PINS);
  Serial.print("int refBaseline        = ");
  Serial.print(refBaseline);
  Serial.println(";");

  Serial.println("\n>> Copy thresholds[] AND refBaseline into Code 2!");
  Serial.println("========================================\n");
}

void loop() {}
