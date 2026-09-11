/*
  TEST: BLE Transmitter from microcontroller to phone

  PURPOSE:
  - Validate BLE communication between ESP32 and the Safer Signal Android app
  - Confirm reliable advertising, connection, and reconnection behavior
  - Verify characteristic notifications correctly transmit alarm state

  METHOD:
  - ESP32 advertises with device name "Safer Signal" and a custom service UUID
  - Android app scans for the device by name, then connects and subscribes
    to notifications on the alarm characteristic via CCCD 0x2902
  - ESP32 toggles the characteristic value between 0 and 1 every 2 seconds,
    and sends it as a BLE notification whenever a phone is connected
  - App receives notifications and updates UI/alarm state accordingly

  EXPECTED RESULTS:
  - App successfully connects and subscribes to notifications
  - Received values toggle between 0, no alarm, and 1, alarm
  - App updates UI and triggers vibration/notification correctly
  - Both sides recover automatically after a disconnection or BT toggle

  NOTES ON RECONNECTION:
  - ESP32 restarts advertising in the onDisconnect callback
  - Android app retries the saved device address every 5 seconds
*/

#include <NimBLEDevice.h>

#define DEVICE_NAME "Safer Signal"

static NimBLEUUID serviceUUID("12345678-1234-1234-1234-123456789001");
static NimBLEUUID charUUID("12345678-1234-1234-1234-123456789002");

NimBLECharacteristic *testCharacteristic;
NimBLEServer *pServer;
NimBLEAdvertising *pAdvertising;

bool deviceConnected = false;
bool oldDeviceConnected = false;

// Server callbacks
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        deviceConnected = true;
        Serial.println("Client connected");
        // Stop advertising while connected
        NimBLEDevice::getAdvertising()->stop();
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo, int reason) override {
        deviceConnected = false;
        Serial.printf("Client disconnected, reason=%d\n", reason);
        // Restart advertising so phone can reconnect
        NimBLEDevice::getAdvertising()->start();
        Serial.println("Advertising restarted");
    }
};

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Starting BLE...");

    NimBLEDevice::init(DEVICE_NAME);

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService *pService = pServer->createService(serviceUUID);

    testCharacteristic = pService->createCharacteristic(
        charUUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    // CCCD descriptor
    testCharacteristic->createDescriptor(
        NimBLEUUID((uint16_t)0x2902),
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
    );

    pService->start();

    // Advertising
    pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName(DEVICE_NAME);
    pAdvertising->addServiceUUID(serviceUUID);
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();

    Serial.println("BLE advertising started");
}

void loop() {
    static bool testState = false;
    uint8_t value = testState ? 1 : 0;

    // Handle connect/disconnect transitions
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }
    if (!deviceConnected && oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
    }

    testCharacteristic->setValue(&value, 1);

    if (deviceConnected) {
        testCharacteristic->notify();
        Serial.print("Sent: ");
        Serial.println(value);
    } else {
        Serial.println("Waiting for connection...");
    }

    testState = !testState;
    delay(2000);
}
