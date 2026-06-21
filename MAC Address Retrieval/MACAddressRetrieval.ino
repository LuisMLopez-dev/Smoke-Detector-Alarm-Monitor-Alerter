#include <WiFi.h> // Control of wirelesss network connections for Wi-Fi enabled microcontrollers

// This sketch is to be uploaded into each receiver unit to obtain its MAC address, and then update the listener unit with the MAC addresses
void setup(){
  Serial.begin(115200); // Baud rate: speed for serial communication

  WiFi.mode(WIFI_STA); // Initialize WiFi hardware

  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress()); //Print MAC Address
}

void loop(){
}