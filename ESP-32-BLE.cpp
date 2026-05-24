/**
 * NAVISECURE: ESP32 Proximity Bluetooth Tracking Node
 * Description: Monitors proximity connections with the driver's smartphone.
 * Uses Classical Bluetooth (SPP) to verify pairing connection,
 * outputting connection states straight to the Arduino Controller.
 * Hardware: ESP32 DevKit V1 (Connect ESP32 TX2/GPIO17 to Arduino RX2/Pin 17)
 */

#include "BluetoothSerial.h"

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error "Bluetooth is not enabled! Please run make menuconfig to enable it."
#endif

BluetoothSerial SerialBT;

// --- CONFIGURATION ---
const String TARGET_PHONE_NAME = "NaviSecure_Driver_Device"; // Mobile Bluetooth Broadcast Name
const long STATUS_INTERVAL = 1000; // Heartbeat interval to master (1 second)

unsigned long lastStatusTime = 0;
bool currentlyPaired = false;

void setup() {
  // Master Controller communications port
  Serial.begin(115200); 
  // Hardware UART link to Master (Serial2 on ESP32: TX=17, RX=16)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  // Set up local Bluetooth Broadcast ID
  if (!SerialBT.begin("NaviSecure_Onboard_System")) {
    Serial.println("[FATAL] Bluetooth initialization failed!");
    while(1);
  }
  
  Serial.println("[INFO] Bluetooth system ready. Awaiting connection from paired device...");
}

void loop() {
  // Check pairing state changes
  if (SerialBT.hasClient()) {
    if (!currentlyPaired) {
      currentlyPaired = true;
      Serial.println("[EVENT] Smartphone matched and connected.");
      Serial2.println("BT_PAIRED");
    }
  } else {
    if (currentlyPaired) {
      currentlyPaired = false;
      Serial.println("[EVENT] Smartphone disconnected or out of range.");
      Serial2.println("BT_LOST");
    }
  }

  // Consistent heartbeat payload sent to Arduino controller
  unsigned long currentTime = millis();
  if (currentTime - lastStatusTime >= STATUS_INTERVAL) {
    lastStatusTime = currentTime;
    if (currentlyPaired) {
      Serial2.println("BT_OK");
    } else {
      Serial2.println("BT_DISCONNECTED");
    }
  }
}
