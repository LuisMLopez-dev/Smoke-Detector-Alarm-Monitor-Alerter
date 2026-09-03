/*
  TEST: BLE Transmitter from microcontroller to phone

  PURPOSE:
  - Validate BLE communication between ESP32 and Android app
  - Confirm reliable advertising, connection, and reconnection behavior
  - Verify characteristic notifications correctly transmit alarm state

  METHOD:
  - ESP32 advertises with custom service UUID and device name "Safer Signal"
  - Android app scans for device by name or service UUID
  - Upon connection, ESP32 sends alternating values, 0 and 1, every 2 seconds
  - App receives notifications and updates UI/alarm state accordingly

  EXPECTED RESULTS:
  - App successfully connects and subscribes to notifications
  - Received values toggle between 0 (no alarm) and 1 (alarm)
  - App updates UI and triggers vibration/notification correctly
  - Device automatically reconnects after disconnection
*/

#include <NimBLEDevice.h>

#define DEVICE_NAME "Safer Signal"

// Custom service and characteristic UUIDs
static NimBLEUUID serviceUUID("12345678-1234-1234-1234-123456789001");
static NimBLEUUID charUUID("12345678-1234-1234-1234-123456789002");

NimBLECharacteristic *testCharacteristic;

// Test toggle state (simulates alarm ON/OFF)
bool testState = false;

// BLE Server Callbacks
class MyServerCallbacks : public NimBLEServerCallbacks {

  void onConnect(NimBLEServer* pServer) {
    Serial.println("Client CONNECTED");
  }

  void onDisconnect(NimBLEServer* pServer) {
    Serial.println("Client DISCONNECTED");

    // Restarts advertising so the phone can reconnect automatically
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->start();

    Serial.println("Advertising restarted");
  }
};

void setup(){

  Serial.begin(115200);

  // Initialize BLE device with name
  NimBLEDevice::init(DEVICE_NAME);

  // Create BLE server
  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create custom service
  NimBLEService *pService = pServer->createService(serviceUUID);

  // Create characteristic, for read and notify
  testCharacteristic = pService->createCharacteristic(charUUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  // CCCD descriptor, which are required for notifications on Android
  testCharacteristic->createDescriptor(NimBLEUUID((uint16_t)0x2902),NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);

  // Start the service
  pService->start();

  // Advertising Setup
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();

  // Main advertisement packet
  NimBLEAdvertisementData advData;
  advData.setName(DEVICE_NAME);
  advData.addServiceUUID(serviceUUID);

  pAdvertising->setAdvertisementData(advData);

  NimBLEAdvertisementData scanResp;
  scanResp.setName(DEVICE_NAME);
  pAdvertising->setScanResponseData(scanResp);

  // Start advertising
  pAdvertising->start();

  Serial.println("BLE Ready");
}

void loop() {

  // Convert boolean state to byte (0 or 1)
  uint8_t value;

  if (testState == true){
    value = 1;
  } 
  else{
    value = 0;
  }

  // Update characteristic value
  testCharacteristic->setValue(&value, 1);

  // Send notification to connected device
  testCharacteristic->notify();

  Serial.print("Sent: ");
  Serial.println(value);

  // Toggle state for next transmission
  testState = !testState;

  delay(2000);
}
