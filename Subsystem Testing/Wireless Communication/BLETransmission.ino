/*
  TEST: Wireless Communication Test: BLE (Bluetooth Low Energy)

  PURPOSE:
  Verify BLE communication between the ESP32 and a smartphone application.

  METHOD:
  - ESP32 acts as a BLE server
  - Sends a toggling value (0/1) every second
  - Smartphone app reads or receives notifications

  EXPECTED RESULT:
  - Phone successfully connects
  - Value updates every 2 seconds
  - App responds accordingly: UI change, vibration, notifications
*/

#include <NimBLEDevice.h>

NimBLECharacteristic *testCharacteristic;
bool deviceConnected = false;
bool testState = false;

class MyServerCallbacks: public NimBLEServerCallbacks{
  void onConnect(NimBLEServer* pServer){
    deviceConnected = true;
  }

  void onDisconnect(NimBLEServer* pServer){
    deviceConnected = false;
  }
};

void setup(){
  Serial.begin(115200);

  // Initialize BLE
  NimBLEDevice::init("BLE_Test_Device");

  NimBLEServer *pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create service
  NimBLEService *pService = pServer->createService("1234");

  // Create characteristic
  testCharacteristic = pService->createCharacteristic("5678",NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

  testCharacteristic->createDescriptor("2902");

  pService->start();

  // Start advertising
  NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID("1234");
  pAdvertising->start();

  Serial.println("BLE Test Ready");
}

void loop(){
  if(deviceConnected){
    uint8_t value;

    if(testState){
      value = 1;
    }
    else{
      value = 0;
    }

    testCharacteristic->setValue(&value, 1);
    testCharacteristic->notify();

    Serial.print("Sent: ");
    Serial.println(value);

    testState = !testState;
  }

  delay(2000);
}