//uncomment to print to serial for debugging
//#define DEBUG_MODE

/************************ Adafruit IO Config *******************************/

// visit io.adafruit.com if you need to create an account,
// or if you need your Adafruit IO key.
#define IO_USERNAME    "mouzy34"
#define IO_KEY         "be212370855e48fdb5f53e86b8980482"

// Adafruit IO configuration
#define IO_SERVER           "io.adafruit.com"  // Adafruit IO server name.
#define IO_SERVERPORT       1883  // Adafruit IO port. 


/******************************* FONA **************************************/

// includes
#include "Adafruit_FONA.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"
#include <SoftwareSerial.h>
#include <Adafruit_SleepyDog.h>

// FONA pin configuration
#define FONA_RX 9
#define FONA_TX 8
#define FONA_RST 4
#define FONA_RI  7

// FONA GPRS configuration
#define FONA_APN             "internet"  
#define FONA_USERNAME        ""  
#define FONA_PASSWORD        ""  

// Maximum number of connect failures in a row before resetting the device.  
#define MAX_CONN_FAILURES      120
uint8_t connFailures = 0; 

/* function prototypes */
void halt();

// module instances
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);
SoftwareSerial *fonaSerial = &fonaSS;

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

// Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
Adafruit_MQTT_FONA mqtt(&fona, IO_SERVER, IO_SERVERPORT, IO_USERNAME, IO_KEY);

// find and initialize the FONA
void initializeFona() {  
  fonaSerial->begin(4800);
  if (! fona.begin(*fonaSerial)) {
    Serial.println(F("Couldn't find FONA"));
    halt();
  }
  Serial.println(F("FONA is OK"));  
}

// connect to the cellular network
void initializeNetwork() {
  Serial.println(F("Checking for network..."));
  while (fona.getNetworkStatus() != 1) {
   delay(500);
  }
  Serial.println(F("Registered (home)"));  
}

// enable the GPRS
void initializeGPRS() {
  // set the APN settings
  fona.setGPRSNetworkSettings(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD));
  delay(2000);

  //reset GPRS
  Serial.println(F("Disabling GPRS"));
  fona.enableGPRS(false);
  delay(2000);
  
  Serial.println(F("Enabling GPRS"));
  if (!fona.enableGPRS(true)) {
    Serial.println(F("Failed to turn GPRS on, resetting..."));
    halt();
  }
  
  Serial.println(F("Connected to Cellular!"));
  
}

// Function to connect and reconnect as necessary to the MQTT server.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    #ifdef DEBUG_MODE
      Serial.println("MQTT Already Connected!");
    #endif
    
    return;
  }

  Serial.println("Connecting to MQTT... ");  

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    if (connFailures >= MAX_CONN_FAILURES) halt();
    connFailures++;
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    
    #ifdef DEBUG_MODE
      Serial.println("Retrying MQTT connection in 5 seconds...");
    #endif
    
    delay(5000);  // wait 5 seconds
  }
  
  connFailures = 0;

  Serial.println("MQTT Connected!");
  
}

// resets the device
void halt() {
  Watchdog.enable(8000);
  while(1);
}



