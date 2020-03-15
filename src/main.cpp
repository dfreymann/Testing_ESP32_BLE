// dmf version 3.12.20+ 

//
// TO ADD:  RESPOND TO CHANGE IN CHARACTERISTIC VALUE BY CHANGING LED PERIOD
//

// Need to include "arduino.h" here
#include <arduino.h>
#include <elapsedMillis.h> 

/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/
// "BLEDevice.h" includes "BLEServer.h", "BLEClient.h", "BLEUtils.h", "BLEScan.h", "BLEAddress.h"
#include <BLEDevice.h>   
// [info]
#include <BLEAdvertisedDevice.h>
// [info]
#include <BLE2902.h>

// --------------

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
// These are locally generated 3.12.20 - 
#define SERVICE_UUID        "6f2c6692-64a7-11ea-bc55-0242ac130003"
#define CHARACTERISTIC_UUID "9669c6a0-64a7-11ea-bc55-0242ac130003"

// ------- led parameters --------

#define LEDBLINK     GPIO_NUM_12   // GPIO 12 (Output)
#define SWITCHLED    GPIO_NUM_27   // GPIO 27 (Input)
#define LEDINDICATOR GPIO_NUM_25   // GPIO 25 (Output)

boolean ledState = LOW; 
boolean indicatorState = LOW; 

// ------- timers --------

elapsedMillis elapsedTime; 
elapsedMillis elapsedITime;

unsigned int interval = 1000; 
unsigned int intervalI = 5000;

// -------- miscellaneous -------

int count = 0;
// flow control parameters from example code 
uint32_t value = 0; 


// -------- declared global - [explain importance] --------

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL; 

// -------- callback to respond to client connection ------

bool deviceConnected = false;
bool oldDeviceConnected = false;

// [to clarify how this is defined] 
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer){
    deviceConnected = true; 
  };  // note use of semicolon here 
  void onDisconnect(BLEServer* pServer){
    deviceConnected = false; 
  }
};    // note use of semicolon here

// ---------- callback to respond to writeValue change sent by client ---------
// 3.14.20 This will allow the Server to respond to changes that are 
// sent by the Client (via writeValue). [Note: following github user peirantan]

// Store the characteristic that is written from the client
std::string characteristicValue;

class MyCharacteristicCallbacks: public BLECharacteristicCallbacks {
	void onWrite(BLECharacteristic* pCharacteristic) {

		characteristicValue = pCharacteristic->getValue();
		
    if (characteristicValue == "0") {
			Serial.println("Server has received a turn-off write");

			count = 0;
			ledState = LOW;

			Serial.println("And so it is turning own LED off");
		} else if (characteristicValue == "1") {
			Serial.println("Server has received a turn-on write");
			ledState = HIGH;
			Serial.println("And so it is turning own LED on");
		} else if (characteristicValue == "2") {
			Serial.println("Server has received a blink slow write");
			count = 0;
		} else if (characteristicValue == "3") {
			Serial.println("Server has received a blink quick write");
			count = 0;
		}
	}
};    // note use of semicolon here

// -------- ISR to respond to button push -----

volatile boolean switchedOn = LOW; 

// [to clarify] ISR handling configuration 
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR captureButton() {
    portENTER_CRITICAL_ISR(&mux);
    switchedOn = !switchedOn;
    portEXIT_CRITICAL_ISR(&mux);
}

// ----------------- setup() ------------------

void setup() {

  Serial.begin(115200);

  pinMode(LEDBLINK, OUTPUT); 
  pinMode(LEDINDICATOR, OUTPUT); 
  pinMode(SWITCHLED, INPUT_PULLUP); 

  attachInterrupt(digitalPinToInterrupt(SWITCHLED), captureButton, FALLING);
 
// --------------
  Serial.println("Starting BLE work!");

  // Initialize BLE on the device [to clarify this]
  // It is given a name [signficance of this?] 
  BLEDevice::init("test_ESP32_a server code");

  // Create a BLE service 
  // Note: BLEServer* pServer is declared globally above
  pServer = BLEDevice::createServer();
  // Configure callbacks to respond to connection
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Here identify the service by the SERVICE_UUID and the 
  // characteristic by the CHARACTERISIC_UUID
  BLEService *pService = pServer->createService(SERVICE_UUID);
  // Want this to notify client so adding PROPERTY_NOTIFY
  // Note: BLECharacteristic* pCharacteristic is declared globally above
  pCharacteristic = pService->createCharacteristic(
                                      CHARACTERISTIC_UUID,
                                      BLECharacteristic::PROPERTY_READ   |
                                      BLECharacteristic::PROPERTY_WRITE  |
                                      BLECharacteristic::PROPERTY_NOTIFY |
                                      BLECharacteristic::PROPERTY_INDICATE
                                    );
  
  // Create the Characteristic Callback
  // 3.14.20 This will allow the Server to respond to changes that are 
  // sent by the Client (via writeValue). [Note: following github user peirantan]
	pCharacteristic->setCallbacks(new MyCharacteristicCallbacks);

  // Set the BLE Descriptor - this allows client to enable/disable NOTIFY and INDICATE
  pCharacteristic->addDescriptor(new BLE2902()); 

  // A characteristic has a value; this is an initialization text string 
  pCharacteristic->setValue("This is the initial test_ESP32_a value");

  // Turn on BLE on the device [to clarify this]
  pService->start();

  // [to clarify this] 
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);  // ** example sets this to false **
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);  // ** example sets this to 0x0 **
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
// ----------------

} // end setup()

// ------------- loop() ---------------

void loop() {

  if (elapsedTime > interval) {
    Serial.println("Toggling state 1"); 
    if (deviceConnected){
      pCharacteristic->setValue("Toggling state 1");
      pCharacteristic->notify(); 
      delay(3); 
    }
    ledState = !ledState; 
    digitalWrite(LEDBLINK, ledState);
    elapsedTime = 0; 
  }

  if (switchedOn != indicatorState) {
    Serial.println("Switch toggling 2");
    if (deviceConnected){
      pCharacteristic->setValue("Toggling state 2!!");
      pCharacteristic->notify(); 
      delay(3);
    }
    indicatorState = !indicatorState;
    digitalWrite(LEDINDICATOR, indicatorState); 
  }

  // handle connection and disconnection states
  if (deviceConnected && !oldDeviceConnected) { // connecting
    oldDeviceConnected = deviceConnected; 
  }
  if (!deviceConnected && oldDeviceConnected){  // disconnecting
    delay(500); 
    pServer->startAdvertising(); 
    Serial.println("start advertising"); 
    oldDeviceConnected = deviceConnected; 
  }

}

