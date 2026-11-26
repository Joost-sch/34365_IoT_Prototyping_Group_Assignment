#include <Wire.h>
#include <LowPower.h>
#include <SoftwareSerial.h>

// =========================================================================
// PIN DEFINITIONS & HARDWARE SETTINGS
// =========================================================================
const int mpuIntPin = 2;          // MPU-6500 INT pin (ATmega328P INT0)
const int pressureSensorPin = A0; // Thin film pressure sensor voltage divider
const int loraRxPin = 10;         // Connect to RN2483A TX
const int loraTxPin = 11;         // Connect to RN2483A RX
const int loraResetPin = 9;       // Connect to RN2483A RST

const uint8_t MPU6500_ADDR = 0x68;
SoftwareSerial loraSerial(loraRxPin, loraTxPin);

// =========================================================================
// TTN OTAA CREDENTIALS (Must be MSB format from TTN Console)
// =========================================================================
const char* DEV_EUI = "0004A30B010680BA";                 // 16 hex chars
const char* APP_EUI = "B2D337899DD08D35";                 // 16 hex chars
const char* APP_KEY = "905553992010C7E2BC23D9E5BA90DBEC"; // 32 hex chars

// =========================================================================
// THRESHOLDS & TIMING CONSTANTS
// =========================================================================
const uint8_t WOM_THRESHOLD = 10;            // Wake sensitivity (4mg per LSB)
const float STILLNESS_THRESHOLD_G = 0.05;    // Max accel variance (g) to be considered "on table"
const uint32_t STILLNESS_REQUIRED_MS = 2000; // Bottle must be still for 2s before weighing
const uint32_t MEASURE_DURATION = 10000;     // 10-second weight measurement window
const float NOISE_DEADBAND_G = 5.0;          // Ignore shifts smaller than 5g
const float EMPTY_THRESHOLD_G = 15.0;        // Weights <= 15g are considered ~0.0g (empty)
const float REFILL_THRESHOLD_G = 25.0;       // Weight jump >= 25g triggers auto-recalibration

// =========================================================================
// STATE VARIABLES
// =========================================================================
float storedWeight = 0.0;      // Baseline weight (grams)
float totalWaterDrunk = 0.0;   // Cumulative daily intake (mL / grams)
volatile bool motionDetected = false;
bool loraIsAsleep = false;

// ISR triggered when MPU-6500 INT pin goes LOW
void wakeUp() {
  motionDetected = true;
  detachInterrupt(digitalPinToInterrupt(mpuIntPin));
}

// =========================================================================
// I2C HELPER FUNCTIONS (MPU-6500)
// =========================================================================
void writeRegister(uint8_t reg, uint8_t data) {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(reg);
  Wire.write(data);
  Wire.endTransmission();
}

uint8_t clearMPUInterruptStatus() {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(0x3A);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6500_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

void configureMPU6500_WOM() {
  writeRegister(0x6B, 0x80); // Reset
  delay(100);
  writeRegister(0x6B, 0x00); // Wake up
  delay(10);
  writeRegister(0x6C, 0x07); // Disable gyro, enable accel
  writeRegister(0x19, 0x00); // Sample rate divider
  writeRegister(0x1C, 0x00); // +-2g range (16384 LSB/g)
  writeRegister(0x1D, 0x09); // Low-power DLPF
  writeRegister(0x69, 0xC0); // Enable Wake-on-Motion logic
  writeRegister(0x1F, WOM_THRESHOLD);
  writeRegister(0x37, 0xB0); // Active LOW, latched interrupt
  writeRegister(0x38, 0x40); // Enable WOM interrupt
  writeRegister(0x6B, 0x20); // Enter low-power duty-cycle mode
}

// Reads raw accelerometer magnitude in g's to check if bottle is stationary
float getAccelMagnitude() {
  Wire.beginTransmission(MPU6500_ADDR);
  Wire.write(0x3B); // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6500_ADDR, (uint8_t)6);

  if (Wire.available() == 6) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    int16_t ay = (Wire.read() << 8) | Wire.read();
    int16_t az = (Wire.read() << 8) | Wire.read();

    float gx = ax / 16384.0;
    float gy = ay / 16384.0;
    float gz = az / 16384.0;

    return sqrt(gx * gx + gy * gy + gz * gz);
  }
  return 1.0;
}

// Blocks until the bottle has been completely still for STILLNESS_REQUIRED_MS
void waitUntilBottleIsStill() {
  Serial.println(F("[MOTION] Waiting for bottle to be placed down and become still..."));
  
  // Temporarily wake MPU from duty-cycle mode for continuous accel polling
  writeRegister(0x6B, 0x00); 
  delay(20);

  uint32_t stillStartTime = 0;
  float lastMag = getAccelMagnitude();

  while (true) {
    delay(100);
    float currentMag = getAccelMagnitude();
    float delta = abs(currentMag - lastMag);
    lastMag = currentMag;

    // Check if acceleration change is tiny AND total magnitude is close to 1g (resting flat)
    if (delta < STILLNESS_THRESHOLD_G && abs(currentMag - 1.0) < 0.15) {
      if (stillStartTime == 0) {
        stillStartTime = millis();
      } else if (millis() - stillStartTime >= STILLNESS_REQUIRED_MS) {
        Serial.println(F("[MOTION] Bottle is stationary on table."));
        break;
      }
    } else {
      stillStartTime = 0; // Reset timer if movement resumes
    }
  }

  // Put MPU back into low-power WOM duty-cycle mode
  writeRegister(0x6B, 0x20);
}

// =========================================================================
// WEIGHT MEASUREMENT HELPER
// =========================================================================
float measureAverageWeight(uint32_t durationMs) {
  analogRead(pressureSensorPin); // Dummy read to settle ADC after sleep
  delay(10);

  uint32_t startTime = millis();
  float sum = 0.0;
  int count = 0;

  while (millis() - startTime < durationMs) {
    int raw = analogRead(pressureSensorPin);
    sum += (raw * 10.2);
    count++;
    delay(250); // Sample 4 times per second
  }
  return (count > 0) ? (sum / count) : 0.0;
}

// =========================================================================
// RN2483A LORAWAN HELPER FUNCTIONS
// =========================================================================

String readLoRaLine(uint32_t timeout = 2000) {
  loraSerial.listen();
  uint32_t start = millis();
  String line = "";
  while (millis() - start < timeout) {
    while (loraSerial.available()) {
      char c = loraSerial.read();
      if (c == '\n') {
        line.trim();
        if (line.length() > 0) return line;
      } else if (c != '\r') {
        line += c;
      }
    }
  }
  line.trim();
  return line;
}

// Forces RN2483A to switch to or wake at 9600 baud and clears its UART parser
void autobaud9600() {
  loraSerial.end();
  pinMode(loraTxPin, OUTPUT);
  digitalWrite(loraTxPin, LOW); // Send UART Break condition (min ~2ms at 9600)
  delay(25);
  digitalWrite(loraTxPin, HIGH);
  delay(10);

  loraSerial.begin(9600);
  loraSerial.listen();
  loraSerial.write(0x55);       // Autobaud synchronization character
  delay(50);
  loraSerial.print("\r\n");     // Terminate line so RN2483A command buffer is clean
  delay(150);
  while (loraSerial.available()) loraSerial.read(); // Flush "ok" or "invalid_param"
}

String sendLoRaCmd(String cmd, uint32_t timeout = 2000) {
  loraSerial.listen();
  while (loraSerial.available()) loraSerial.read(); // Flush RX buffer
  Serial.print(F("  >> CMD: "));
  Serial.println(cmd);

  loraSerial.print(cmd);
  loraSerial.print("\r\n"); // RN2483A strictly requires explicit \r\n

  String resp = readLoRaLine(timeout);
  Serial.print(F("  << RSP: "));
  Serial.println(resp);
  return resp;
}

// Wakes RN2483A from sys sleep and verifies it responds to commands
void wakeLoRa() {
  Serial.println(F("[LORA] Waking RN2483A from sleep..."));

  for (int attempt = 1; attempt <= 3; attempt++) {
    autobaud9600();
    
    // Test if the RN2483A UART is awake and synced
    String ver = sendLoRaCmd("sys get ver", 1000);
    if (ver.startsWith("RN2483")) {
      loraIsAsleep = false;
      return;
    }
    Serial.println(F("  [!] Wake attempt failed, retrying..."));
    delay(200);
  }
}

void initLoRaWAN() {
  Serial.println(F("[LORA] Hard resetting RN2483A..."));
  pinMode(loraResetPin, OUTPUT);
  digitalWrite(loraResetPin, LOW);
  delay(200);
  digitalWrite(loraResetPin, HIGH);
  delay(1000); // Wait for RN2483A to finish booting

  // Downshift RN2483A baud rate to 9600 so SoftwareSerial doesn't corrupt data
  autobaud9600();
  loraIsAsleep = false;

  // Verify communication and reset MAC layer (EU868)
  sendLoRaCmd("sys get ver");
  sendLoRaCmd("mac reset 868");

  // Configure TTN v3 parameters
  sendLoRaCmd(String("mac set deveui ") + DEV_EUI);
  sendLoRaCmd(String("mac set appeui ") + APP_EUI);
  sendLoRaCmd(String("mac set appkey ") + APP_KEY);
  sendLoRaCmd("mac set rx2 3 869525000"); // Required for TTN EU868 RX2 window (SF9)
  sendLoRaCmd("mac set dr 5");            // Start at SF7 for reliable initial join
  sendLoRaCmd("mac set adr on");
  sendLoRaCmd("mac save", 5000);          // Saving to EEPROM takes ~1-2 seconds

  Serial.println(F("[LORA] Joining TTN via OTAA..."));
  String joinCmdResp = sendLoRaCmd("mac join otaa", 2000);

  if (joinCmdResp == "ok") {
    // Wait up to 15 seconds for the second asynchronous line ("accepted" or "denied")
    String netStatus = readLoRaLine(15000);
    Serial.println("  Network status: " + netStatus);
    if (netStatus == "accepted") {
      Serial.println(F("[LORA] Successfully connected to TTN!"));
      return;
    }
  }
  Serial.println(F("[LORA] Warning: OTAA join failed or timed out. Check keys/gateway."));
}

void sendLoRaHexPayload(float drunkDelta, float currentBottleWeight) {
  // Wake the RN2483A properly if it was put to sleep
  if (loraIsAsleep) {
    wakeLoRa();
  }

  // Convert floats to 16-bit unsigned integers (grams/mL)
  uint16_t deltaInt = (drunkDelta > 0) ? (uint16_t)round(drunkDelta) : 0;
  uint16_t weightInt = (currentBottleWeight > 0) ? (uint16_t)round(currentBottleWeight) : 0;

  // Format into a 4-byte (8-character) Hex string
  char hexPayload[9];
  sprintf(hexPayload, "%04X%04X", deltaInt, weightInt);

  Serial.print(F("[LORA] Sending Hex Payload (Port 1): "));
  Serial.println(hexPayload);

  // Attempt transmission (with retry in case EU868 duty cycle returns "no_free_ch")
  for (int attempt = 1; attempt <= 5; attempt++) {
    String resp = sendLoRaCmd("mac tx uncnf 1 " + String(hexPayload), 2000);
    
    if (resp == "ok") {
      // Wait up to 20s for "mac_tx_ok" or downlink after TTN's 5s/6s RX1/RX2 windows close!
      String txStatus = readLoRaLine(20000);
      Serial.println("  TX Status: " + txStatus);
      break;
    } 
    else if (resp == "no_free_ch" || resp == "busy") {
      Serial.print(F("  [!] Radio returned "));
      Serial.print(resp);
      Serial.println(F(". Waiting 10s before retry..."));
      delay(10000);
    } 
    else {
      Serial.println(F("  [!] Unexpected MAC response. Re-syncing UART and retrying..."));
      autobaud9600();
    }
  }
}

void sleepLoRa() {
  Serial.println(F("[LORA] Putting RN2483A to sleep..."));
  
  delay(200);
  while (loraSerial.available()) loraSerial.read();

  loraSerial.print("sys sleep 86400000\r\n");
  
  String immediateResp = readLoRaLine(500);
  if (immediateResp == "busy") {
    Serial.println(F("  [!] Radio still busy with RX windows, waiting 5s before sleeping..."));
    delay(5000);
    while (loraSerial.available()) loraSerial.read();
    loraSerial.print("sys sleep 86400000\r\n");
    delay(200);
  }

  loraIsAsleep = true;
}

// =========================================================================
// SETUP (STEP 1 & STEP 2)
// =========================================================================
void setup() {
  Serial.begin(115200);
  pinMode(mpuIntPin, INPUT_PULLUP);

  Wire.begin();
  Wire.setClock(400000);
  delay(100);

  Serial.println(F("\n=========================================="));
  Serial.println(F("   SMART WATER BOTTLE SYSTEM STARTING     "));
  Serial.println(F("=========================================="));

  // 1a. Initialize LoRaWAN connection
  initLoRaWAN();

  // 1b. Configure Accelerometer Wake-on-Motion
  Serial.println(F("[INIT] Configuring MPU-6500 Wake-on-Motion..."));
  configureMPU6500_WOM();
  clearMPUInterruptStatus();

  // 1c. Establish starting weight over 5 seconds
  Serial.println(F("[INIT] Measuring starting baseline weight over 5 seconds..."));
  storedWeight = measureAverageWeight(5000);

  // 2. Store starting weight & report
  Serial.println(F("------------------------------------------"));
  Serial.print(F("[INIT COMPLETE] Stored Baseline Weight: "));
  Serial.print(storedWeight, 1);
  Serial.println(F(" g"));
  Serial.println(F("==========================================\n"));

  // Send initial baseline weight (0g drunk, current bottle weight)
  sendLoRaHexPayload(0, storedWeight);
}

// =========================================================================
// MAIN LOOP (STEPS 3 THROUGH 8)
// =========================================================================
void loop() {
  // 3. Go to sleep (both LoRa radio and Pro Mini)
  sleepLoRa();
  clearMPUInterruptStatus();
  motionDetected = false;
  attachInterrupt(digitalPinToInterrupt(mpuIntPin), wakeUp, LOW);

  Serial.println(F("[SLEEP] Radio & MCU entering deep sleep..."));
  Serial.flush();

  LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);

  // =========================================================================
  // 4. WAKE UP FROM MOVEMENT DETECTED BY ACCELEROMETER
  // =========================================================================
  delay(50);
  uint8_t intStatus = clearMPUInterruptStatus();

  if (motionDetected || (intStatus & 0x40)) {
    Serial.println(F("\n=========================================="));
    Serial.println(F("[WAKE] Bottle moved!"));

    // 5. Check if bottle is still moving; wait until it rests on table again
    waitUntilBottleIsStill();

    // 6. Measure new weight over 10 seconds and subtract from baseline
    Serial.println(F("[MEASURE] Weighing bottle for 10 seconds..."));
    float newWeight = measureAverageWeight(MEASURE_DURATION);
    float weightChange = newWeight - storedWeight; // Positive = weight added, Negative = water drunk

    Serial.println(F("------------------------------------------"));
    Serial.print(F("  Previous Baseline : ")); Serial.print(storedWeight, 1); Serial.println(F(" g"));
    Serial.print(F("  New Measured Wt   : ")); Serial.print(newWeight, 1); Serial.println(F(" g"));
    Serial.print(F("  Net Weight Change : ")); Serial.print(weightChange, 1); Serial.println(F(" g"));

    // 7. Evaluate state: Empty (~0g), Refill Spike, or Water Drunk
    // CASE A: Baseline was ~0.0g (empty) and a large weight spike is detected -> Auto-Recalibrate!
    if (storedWeight <= EMPTY_THRESHOLD_G && newWeight >= REFILL_THRESHOLD_G) {
      storedWeight = newWeight;
      Serial.print(F("  >> [REFILL / RECALIBRATED] Spike from ~0g detected! New Baseline: "));
      Serial.print(storedWeight, 1);
      Serial.println(F(" g"));
      sendLoRaHexPayload(0, storedWeight);
    }
    // CASE B: Bottle was not empty, but weight increased significantly -> Refilled / Topped up!
    else if (weightChange >= REFILL_THRESHOLD_G) {
      storedWeight = newWeight;
      Serial.print(F("  >> [REFILLED] Bottle topped up (+"));
      Serial.print(weightChange, 1);
      Serial.print(F(" g). New Baseline: "));
      Serial.print(storedWeight, 1);
      Serial.println(F(" g"));
      sendLoRaHexPayload(0, storedWeight);
    }
    // CASE C: Weight decreased beyond the deadband -> Water Drunk (including down to 0g)
    else if (weightChange < -NOISE_DEADBAND_G) {
      float drunkAmount = storedWeight - newWeight;
      totalWaterDrunk += drunkAmount;
      storedWeight = newWeight; // If newWeight is ~0.0g, next refill spike triggers CASE A!

      Serial.print(F("  >> [DRANK] "));
      Serial.print(drunkAmount, 1);
      Serial.println(F(" mL"));
      if (storedWeight <= EMPTY_THRESHOLD_G) {
        Serial.println(F("  >> [INFO] Bottle is now at ~0.0g (empty). Next weight spike will auto-recalibrate."));
      }
      sendLoRaHexPayload(drunkAmount, storedWeight);
    }
    // CASE D: Minor shift within noise deadband
    else {
      Serial.println(F("  >> [NO CHANGE] Movement within noise deadband."));
    }

    Serial.print(F("  TOTAL DRUNK TODAY : ")); Serial.print(totalWaterDrunk, 1); Serial.println(F(" mL"));
    Serial.println(F("==========================================\n"));
  }
  // 8. Loop repeats -> goes right back to sleep
}