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
#define FONA_APN             "pwg"  
#define FONA_USERNAME        ""  
#define FONA_PASSWORD        ""  
 

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
    while (1);
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
  }
  Serial.println(F("Connected to Cellular!"));
  
}

// Function to connect and reconnect as necessary to the MQTT server.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    Serial.println("MQTT Already Connected!");
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}
