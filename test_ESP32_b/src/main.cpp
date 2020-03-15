// dmf version 3.12.20+ 

// TO REVISE - HANDLE NOTIFY->BLINK LED WITH CLEANER CODE
// TO REVISE - ADD TIMER FOR USE OF WRITEVALUE TO CHANGE PERIOD OF SERVER BLINK 

/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 */

#include <arduino.h>
#include <elapsedMillis.h>

#include "BLEDevice.h"

// The remote service we wish to connect to.
static BLEUUID serviceUUID("6f2c6692-64a7-11ea-bc55-0242ac130003");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("9669c6a0-64a7-11ea-bc55-0242ac130003");

// -------------- [to explain] ---------

static BLERemoteCharacteristic* pRemoteCharacteristic;

static BLEAdvertisedDevice* myDevice;

// ------  LED parameters ---------

#define LEDINDICATOR GPIO_NUM_25   // GPIO 25 (Output)
#define SWITCHLED    GPIO_NUM_27   // GPIO 27 (Input)

static boolean toggleLocalLED = false; 
boolean indicatorState = LOW; 

// ------------- callback functions ---------------

String notifyValue;

// what to do if there is a 'notify' event
// notice this is a function with four parameters
static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    // 3.13.20 The string passed here is often not being properly terminated
    // so that you get garbage characters. I don't know why - the string is 
    // read properly by my phone BLE connection. The following terminates
    // the string so that it is as expected. 
    pData[length] = '\0';
    Serial.println((char*)pData);

    notifyValue = (char*) pData; 
    Serial.println(notifyValue); 

    if (length == 18) {   // just capturing the indicator LED 2 message for now
      toggleLocalLED = !toggleLocalLED; 
    }

}

// ----------- [ to explain ] -----------

static boolean connected = false;

class MyClientCallback : public BLEClientCallbacks {
  
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    Serial.println("onDisconnect");
  }
};

// ------------- [what is this] -----------------

bool connectToServer() {
    Serial.print("Forming a connection to ");
    Serial.println(myDevice->getAddress().toString().c_str());
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify())
      pRemoteCharacteristic->registerForNotify(notifyCallback);
    
    // dmf - added to avoid warning during compilation of example code
    return true; 
}

// ----------------- [What is this] ------------------------

static boolean doConnect = false;
static boolean doScan = false;

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

// -------- ISR to respond to button push -----

volatile boolean switchedOn = LOW; 

// [to clarify] ISR handling configuration 
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR captureButton() {
    portENTER_CRITICAL_ISR(&mux);
    switchedOn = !switchedOn;
    portEXIT_CRITICAL_ISR(&mux);
}

// ----------------- setup() ------------------------

void setup() {

  Serial.begin(115200);

  pinMode(LEDINDICATOR, OUTPUT); 
  pinMode(SWITCHLED, INPUT_PULLUP); 

  attachInterrupt(digitalPinToInterrupt(SWITCHLED), captureButton, FALLING);

  Serial.println("Starting Arduino BLE Client application...");
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we 
  // want to use to be informed when we have 
  // detected a new device.  Specify that we 
  // want active scanning and start the scan
  // to run for 5 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  pBLEScan->start(5, false);
} // End of setup.

// --------------- loop() ------------------------

// This is the Arduino main loop function.
void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {

    connected = connectToServer(); 

    if (connected) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, 
  // update the characteristic each time we 
  // are reached with the current time since boot.
  if (connected) {

    // respond to the notification by toggling the LED (server->client)
    if (toggleLocalLED) {
      indicatorState = !indicatorState;
      digitalWrite(LEDINDICATOR, indicatorState); 
      toggleLocalLED = !toggleLocalLED; 
    }

    // write a new value for the Characteristic (client->server)
    String newValue = "Time since boot: " + String(millis()/1000);
    Serial.println("Setting new characteristic value to \"" + newValue + "\"");
    // Set the characteristic's value to be the array of bytes that is actually 
    // a string.
    pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());

  }else if(doScan){
    
    BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
  
  }
  
  delay(1000); // Delay a second between loops.

} // End of loop

// ------------------------------------------