/**
 * NAVISECURE: Arduino Mega Main Digital Controller
 * Description: Acts as the primary system coordinator. Runs state verification 
 * machines, handles sensors self-tests, controls ignition relay cuts, 
 * interprets GPS positions, handles GSM emergency protocols.
 * Hardware Connections:
 * - I2C LCD (16x2/20x4): SDA -> Pin 20, SCL -> Pin 21
 * - MPU6050 Accelerometer: SDA -> Pin 20, SCL -> Pin 21
 * - Raspberry Pi USB Serial: Main USB Port (Serial)
 * - ESP32 Hardware UART: RX2 (Pin 19) <- ESP32 TXD (Pin 17)
 * - SIM800L GSM: TX1 (Pin 18) -> SIM800L RXD, RX1 (Pin 19) -> SIM800L TXD 
 * - Neo-6M GPS: SoftwareSerial TX (Pin 10) -> GPS RX, RX (Pin 11) -> GPS TX
 * - Relay Output: Pin 8 (Optocoupler input)
 * - Diagnostics Analog Sensors: A0 (TPMS mock), A1 (Load Cell mock), A2 (Engine Temp)
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>

// --- DEVICE INSTANCES ---
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD Address 0x27, 16 Cols, 2 Rows
SoftwareSerial gpsSerial(11, 10);   // RX (GPS TX), TX (GPS RX)

// --- SYSTEM HARDWARE PINS ---
const int IGNITION_RELAY_PIN = 8;
const int TPMS_PIN = A0;           // Potentiometer mocking tire pressure
const int LOAD_CELL_PIN = A1;      // Potentiometer mocking load detector
const int TEMP_SENSOR_PIN = A2;    // Thermistor mocking engine/cabin temp

// --- SYSTEM CONSTANTS & TRIPS ---
const float TEMP_LIMIT = 85.0;     // Overheat safety limit in C
const int LOAD_LIMIT = 800;        // Max load limit rating
const int TPMS_MIN_PRESSURE = 300; // Low pressure threshold
const float G_IMPACT_THRESHOLD = 4.5; // G-force crash threshold

// --- STATE MACHINE TYPES ---
enum SystemState {
  STATE_WAITING_AUTH,
  STATE_PREDRIVE_DIAGNOSTICS,
  STATE_READY_TO_START,
  STATE_TRANSIT_ACTIVE,
  STATE_CRASH_ALERT
};

SystemState currentState = STATE_WAITING_AUTH;

// --- GLOBAL TELEMETRY VARIABLES ---
bool isFaceVerified = false;
bool isPhonePaired = false;
unsigned long stateTimer = 0;
unsigned long statusPrintTimer = 0;
int countdownValue = 60;

// Simple MPU6050 addressing variables
const int MPU_addr = 0x68;
int16_t AcX, AcY, AcZ;
float gForceX, gForceY, gForceZ, totalG;

// Mock GPS Variables for reliable telemetry compilation
String gpsLat = "12.9254";  // Default fallback Location (BNMIT Bangalore)
String gpsLng = "77.5658";

void setup() {
  // Initialize communication ports
  Serial.begin(9600);   // To Raspberry Pi (USB connection)
  Serial1.begin(9600);  // To SIM800L GSM (Hardware Serial 1)
  Serial2.begin(9600);  // To ESP32 BT Controller (Hardware Serial 2)
  gpsSerial.begin(9600); // To Neo-6M GPS

  // I/O Pin Config
  pinMode(IGNITION_RELAY_PIN, OUTPUT);
  digitalWrite(IGNITION_RELAY_PIN, LOW); // Vehicle locked initially

  // Initialize LCD Screen
  lcd.init();
  lcd.backlight();
  displayScreen("  NAVISECURE  ", "SYSTEM STARTUP ");
  delay(2000);

  // Initialize MPU6050 Accelerometer
  Wire.begin();
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x6B); // Power Management 1 register
  Wire.write(0);    // Wake up MPU6050
  Wire.endTransmission(true);

  // Trigger Auth from RPi
  triggerPiVerification();
}

void loop() {
  readSerialFeeds();
  readAccelerometer();
  readGPSData();

  switch (currentState) {
    case STATE_WAITING_AUTH:
      handleWaitingAuth();
      break;

    case STATE_PREDRIVE_DIAGNOSTICS:
      handlePreDriveDiagnostics();
      break;

    case STATE_READY_TO_START:
      handleReadyToStart();
      break;

    case STATE_TRANSIT_ACTIVE:
      handleTransitActive();
      break;

    case STATE_CRASH_ALERT:
      handleCrashAlert();
      break;
  }
}

// --- CORE SYSTEM LOGIC FUNCTIONS ---

void handleWaitingAuth() {
  static unsigned long lastTriggerAttempt = 0;
  
  if (millis() - lastTriggerAttempt > 10000) {
    lastTriggerAttempt = millis();
    triggerPiVerification();
  }

  if (!isFaceVerified) {
    displayScreen("WAITING IDENTITY", "IRIS SCAN REQ...");
  } else if (!isPhonePaired) {
    displayScreen("FACE VERIFIED!", "PAIR SMARTPHONE ");
  }

  // Transition when both verification stages are passed
  if (isFaceVerified && isPhonePaired) {
    currentState = STATE_PREDRIVE_DIAGNOSTICS;
    displayScreen("VERIFIED OK!", "RUNNING DIAGS...");
    delay(2000);
  }
}

void handlePreDriveDiagnostics() {
  int pressure = analogRead(TPMS_PIN);
  int load = analogRead(LOAD_CELL_PIN);
  int tempRaw = analogRead(TEMP_SENSOR_PIN);
  float temperature = (tempRaw * 0.48828); // Standard Linear mapping

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:"); lcd.print(temperature, 1); lcd.print("C L:"); lcd.print(load);
  lcd.setCursor(0, 1);
  lcd.print("P:"); lcd.print(pressure);

  delay(1500);

  // Validate sensor inputs
  if (temperature > TEMP_LIMIT) {
    displayScreen("DIAGNOSTICS FAIL", "ENGINE OVERHEAT ");
    delay(3000);
    return;
  }
  if (load > LOAD_LIMIT) {
    displayScreen("DIAGNOSTICS FAIL", "OVERLOAD LIMIT ");
    delay(3000);
    return;
  }
  if (pressure < TPMS_MIN_PRESSURE) {
    displayScreen("DIAGNOSTICS FAIL", "LOW TIRE PRESS  ");
    delay(3000);
    return;
  }

  // All checks passed
  currentState = STATE_READY_TO_START;
}

void handleReadyToStart() {
  displayScreen("CHECKLIST PASSED", "IGNITION ENABLED");
  digitalWrite(IGNITION_RELAY_PIN, HIGH); // Allow vehicle start
  delay(3000);
  currentState = STATE_TRANSIT_ACTIVE;
}

void handleTransitActive() {
  // Display active speed/diagnostics cycle in normal mode
  static unsigned long uiRefresh = 0;
  if (millis() - uiRefresh > 1000) {
    uiRefresh = millis();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("NAV: TRANSIT OK ");
    lcd.setCursor(0, 1);
    lcd.print("CRASH SCAN ACTIVE");
  }

  // Constantly evaluate total dynamic G-forces for crash detection
  if (totalG >= G_IMPACT_THRESHOLD) {
    currentState = STATE_CRASH_ALERT;
    stateTimer = millis();
    countdownValue = 60;
    digitalWrite(IGNITION_RELAY_PIN, LOW); // Kill Engine starter line immediately
    displayScreen("IMPACT DETECTED!", "DISPATCH IN 60S");
    delay(1000);
  }
}

void handleCrashAlert() {
  if (millis() - stateTimer >= 1000) {
    stateTimer = millis();
    countdownValue--;

    if (countdownValue > 0) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DISPATCHING HELP");
      lcd.setCursor(0, 1);
      lcd.print("COUNTDOWN: "); lcd.print(countdownValue); lcd.print("s");

      // Optional Driver Hardware Override Check
      // If a button is pressed or driver enters serial payload, we can reset:
      // if (digitalRead(OVERRIDE_PIN) == HIGH) { currentState = STATE_TRANSIT_ACTIVE; digitalWrite(IGNITION_RELAY_PIN, HIGH); }
    } else {
      // Execute Automated Emergency Protocols
      displayScreen("SMS DISPATCHED  ", "DIALING EMER CONTACT");
      sendRescueSMS();
      dialEmergencyContact();
      while (true) {
        // Halt system in locked-down state until manually serviced
        displayScreen("SYS SHUTDOWN", "RESCUE DISPATCHD");
        delay(2000);
      }
    }
  }
}

// --- PERIPHERAL / SENSOR FUNCTIONS ---

void triggerPiVerification() {
  Serial.println("TRIGGER_AUTH"); // Output over main USB port
}

void readSerialFeeds() {
  // 1. Process Raspberry Pi inputs
  if (Serial.available() > 0) {
    String piData = Serial.readStringUntil('\n');
    piData.trim();
    if (piData == "AUTH_SUCCESS") {
      isFaceVerified = true;
    } else if (piData == "AUTH_FAILED") {
      isFaceVerified = false;
    }
  }

  // 2. Process ESP32 Bluetooth tracking inputs
  if (Serial2.available() > 0) {
    String espData = Serial2.readStringUntil('\n');
    espData.trim();
    if (espData == "BT_PAIRED" || espData == "BT_OK") {
      isPhonePaired = true;
    } else if (espData == "BT_LOST" || espData == "BT_DISCONNECTED") {
      isPhonePaired = false;
      // Safety constraint: If phone drops mid-transit, signal warning but do not kill engine on highway
      if (currentState == STATE_TRANSIT_ACTIVE) {
        displayScreen("WARNING: PH LOST", "LINK BROKEN      ");
      }
    }
  }
}

void readAccelerometer() {
  Wire.beginTransmission(MPU_addr);
  Wire.write(0x3B);  // Start register for Accel Data
  Wire.endTransmission(false);
  Wire.requestFrom(MPU_addr, 6, true);
  
  AcX = Wire.read() << 8 | Wire.read();  
  AcY = Wire.read() << 8 | Wire.read();  
  AcZ = Wire.read() << 8 | Wire.read();  

  // Scale G sensitivity limits (FS_SEL = 0, Scale is 16384 LSB/g)
  gForceX = AcX / 16384.0;
  gForceY = AcY / 16384.0;
  gForceZ = AcZ / 16384.0;

  // Calculate net Acceleration vector
  totalG = sqrt(gForceX * gForceX + gForceY * gForceY + gForceZ * gForceZ);
}

void readGPSData() {
  // Simplified high-performance parsing of NMEA GPGGA sequences
  while (gpsSerial.available() > 0) {
    String gpsLine = gpsSerial.readStringUntil('\n');
    if (gpsLine.startsWith("$GPGGA")) {
      // Manual field indexing for performance optimization without heavy external parsers
      int firstComma = gpsLine.indexOf(',');
      int secondComma = gpsLine.indexOf(',', firstComma + 1);
      int thirdComma = gpsLine.indexOf(',', secondComma + 1); // Latitude start
      int fourthComma = gpsLine.indexOf(',', thirdComma + 1);  // N/S marker
      int fifthComma = gpsLine.indexOf(',', fourthComma + 1);  // Longitude start
      int sixthComma = gpsLine.indexOf(',', fifthComma + 1);   // E/W marker
      
      if (thirdComma != -1 && fourthComma != -1 && fifthComma != -1 && sixthComma != -1) {
        gpsLat = gpsLine.substring(thirdComma + 1, fourthComma);
        gpsLng = gpsLine.substring(fifthComma + 1, sixthComma);
      }
    }
  }
}

void sendRescueSMS() {
  Serial1.println("AT+CMGF=1"); // Set SMS to text mode
  delay(500);
  Serial1.println("AT+CMGS=\"+919900990099\""); // Replace with designated Emergency Number
  delay(500);
  Serial1.print("CRASH ALERT! Vehicle BNMIT-07. Location: Latitude ");
  Serial1.print(gpsLat);
  Serial1.print(", Longitude ");
  Serial1.println(gpsLng);
  Serial1.write(26); // ASCII character for Ctrl+Z to send SMS
  delay(3000);
}

void dialEmergencyContact() {
  Serial1.println("ATD+919900990099;"); // Initiates automatic hands-free voice call
  delay(1000);
}

void displayScreen(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}
