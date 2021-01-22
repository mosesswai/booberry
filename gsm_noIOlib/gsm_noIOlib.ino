/* Booberry project!
 * Code: Linda - GSM, no IO library
 * by Moses.
 */

// contains network info
#include "config.h"

/***** LED pins *****/
#define RED_PIN   12
#define GREEN_PIN 11
#define BLUE_PIN  10

/***** sensor pins *****/
#define TOUCH_PIN A1

/***** time intervals *****/
#define BLINK_INTERVAL        500
#define WAIT_INTERVAL         1500
#define RESEND_INTERVAL       20000
#define PING_INTERVAL         150000

#define NEUTRAL_INTERVAL      3000
#define CONTINENTAL_INTERVAL  5000
#define MOONISH_INTERVAL      10000
#define PLANETARY_INTERVAL    15000

/***** feeds *****/
#define MAIN_FEED     1
#define SELF_FEED     2
#define OTHER_FEED    3

/***** set up feeds *****/
// booberry - this is the main feed
Adafruit_MQTT_Publish feed = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/booberry");
Adafruit_MQTT_Subscribe feedSubscription = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/booberry");

// ready fields to make sure both feeds are engaged
Adafruit_MQTT_Publish receivedSelf = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/boo-liz");
//Adafruit_MQTT_Publish receivedOther = Adafruit_MQTT_Publish(&mqtt, IO_USERNAME "/feeds/boo-moz");
Adafruit_MQTT_Subscribe receivedOtherSubscription = Adafruit_MQTT_Subscribe(&mqtt, IO_USERNAME "/feeds/boo-moz");

// Maximum number of publish failures in a row before resetting the whole sketch.  
#define MAX_TX_FAILURES      100
uint8_t txfailures = 0;  

/***** device states *****/
typedef enum {
  NEUTRAL, WARM
} BooState;

typedef enum {
  CONTINENTAL, MOONISH, PLANETARY, UNIVERSAL
} Warmth;

/***** module variables *****/
// main state of the device
BooState state;

// lighting states
Warmth warmth;

// state of the touch sensor
boolean touched = false;

// keep track of whether state has changed
boolean updated = false;

// whether to check before engaging
boolean waiting = false;

// if the timer for the touch sensor is on
boolean listening = false;

// whether we need to update the feed
boolean queued = false;

// allows touch to be used again
boolean restored = true;

// keeps track of light state during blinking
boolean lightsOn = false;

// keeps track of time for various activities
unsigned long lastTouchTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastWaitTime = 0;
unsigned long lastPingTime = 0;

// keeps track of publish state and value
bool pendingPublish = true;
int lastPublish = 0;

//setup function
void setup() {
  // set pin modes
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);    

  // initial state
  state = NEUTRAL;
  warmth = CONTINENTAL;

  // keep yellow light on for set-up
  ledsON(HIGH, HIGH, LOW);

  // create feed subscriptions
  mqtt.subscribe(&feedSubscription);
  mqtt.subscribe(&receivedOtherSubscription);

  /* for debugging */
  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
//  while(! Serial);

  // initialize the FONA to connect to the internet
  initializeFona();
  initializeNetwork();
  initializeGPRS();
  
  // connect to io.adafruit.com
  Serial.println("Connecting to Adafruit IO...");
  MQTT_connect();
  Serial.println("Connected to Adafruit IO!");

  // indicate succesful setup, switch yellow lights off
  ledsOFF();  
}


//loop function
void loop() {
  // Ensure the connection to the MQTT server is alive
  MQTT_connect();
  
  // resend update feed if needed.
  checkWaiting();

  // republish if not previously successful
  checkPublish();

  // read the touch sensor
  readTouch();

  // light up the lights
  lighting();

  // listen to subscription packets
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(500))) {
    if (subscription == &feedSubscription) {
      handleFeed((char *)feedSubscription.lastread);
    }
    if (subscription == &receivedOtherSubscription) {
      handleOther((char *)receivedOtherSubscription.lastread);
    }    
  }

  // pings when necessary to maintain connection;
  checkPing();

}


/* Function: publishIO
 * Send data to required feed, IO
 * Tries to publish 5 times before resetting
 */
void publishIO(int feedTo, uint32_t value) {
  lastPublish = value; 

  #ifdef DEBUG_MODE
    Serial.print(F("\nSending val "));
    Serial.print(value);
    Serial.print("...");
  #endif
  
  bool successful;
  
  switch(feedTo) {
    case MAIN_FEED: 
      successful = feed.publish(value);
      pendingPublish = true;
      break;
    case SELF_FEED: 
      successful = receivedSelf.publish(value);
      break;
//    case OTHER_FEED: successful = receivedOther.publish(value);
    default: break;
  }
  
  if (!successful) {
    
    #ifdef DEBUG_MODE
      Serial.println(F("Failed"));
    #endif
    
    txfailures++;
  } else {
    
    #ifdef DEBUG_MODE
      Serial.println(F("OK!"));
    #endif
    
    txfailures = 0;
    pendingPublish = false;
  }
  
}

/* Function: handleFeed 
 * Is called whenever the feed receives new data 
 */
void handleFeed ( char *data ) {
  #ifdef DEBUG_MODE
    Serial.print(F("Got main: "));
    Serial.println(data);
  #endif
  
  // convert the data to integer
  int reading = atoi(data);

  switch (reading) {
    case 0: state = NEUTRAL; break;
    case 1: state = WARM; warmth = CONTINENTAL; break;
    case 2: state = WARM; warmth = MOONISH; break;
    case 3: state = WARM; warmth = PLANETARY; break;
    default: break;
  }

  //easter egg when both touch at the same time.
  if (listening) {
    state = WARM;
    warmth = UNIVERSAL;
    queued = false;
    restored = false;
  }

  waiting = true;
  lastWaitTime = millis();
  
  updated = true;

  pendingPublish = false;

  publishIO(SELF_FEED, 1);

  #ifdef DEBUG_MODE
    Serial.print("state changed to: ");
    Serial.println(state);
  
    Serial.print("warmth changed to: ");
    Serial.println(warmth);
  #endif
}


/* Function: handleOther
 * Checks whether the other device has received the
 * message before allowing itself to engage.
 */
void handleOther ( char *data ) {
  // convert the data to integer
  int reading = atoi(data);

  if(reading == 1) {
    waiting = false;
//    publishIO(OTHER_FEED, 0);

    #ifdef DEBUG_MODE
      Serial.println("delivered");
    #endif
  }
}


/* Function: checkWaiting
 * Reupdates the feed if the other device has not 
 * received the message.
 */
void checkWaiting() {
  if(waiting) {
    unsigned long currTime = millis();
    if ( (currTime - lastWaitTime) > RESEND_INTERVAL) {
      if (state == NEUTRAL) {
        publishIO(MAIN_FEED, 0);
      } else {
        updateFeed(); 
      }
      lastWaitTime = currTime;
    }
  }
}


/* Function: checkPublish() {
 * Republishes if there was a previously unsuccessful publish
 */
void checkPublish() {
  if (pendingPublish) {
    publishIO(MAIN_FEED, lastPublish);
  }
}


/* Function: checkPing
 * Pings after half the connection alive time to maintain it
 */
void checkPing() {
  unsigned long currentTime = millis();
  if (currentTime - lastPingTime > PING_INTERVAL) {
    mqtt.ping();
    lastPingTime = currentTime;
  }
}

/* Function: readTouch
 * Checks the state of the touch sensor. If it is touched
 * the function listens for a number of seconds depending
 * on the state of the device.
 */
void readTouch() {
  touched = digitalRead(TOUCH_PIN);

  if (touched && restored) {
    if (listening) {
      
      switch (state) {
        case NEUTRAL: listeningNeutral(); break;
        case WARM : listeningWarm(); break;
        default: break;
      }
      
    } else {
      lastTouchTime = millis();
      listening = true;
    }
  } else if (queued) {
    queued = false;
    listening = false;
    restored = false;
    updateFeed();
    
  } else if (listening) {
    listening = false;
  } else if (!touched && !restored) {
    restored = true;
  }

}

// listening to touch sensor when in warm state
void listeningWarm() {
  unsigned long currentTime = millis();
  if (currentTime - lastTouchTime > NEUTRAL_INTERVAL) {
    listening = false;
    restored = false;
    publishIO(MAIN_FEED, 0);
  }
}

// listening to touch sensor when in neutral state
void listeningNeutral() {
  unsigned long currentTime = millis();
  if (currentTime - lastTouchTime > PLANETARY_INTERVAL) {
    queued = true;
    warmth = PLANETARY;
  } else if (currentTime - lastTouchTime > MOONISH_INTERVAL) {
    queued = true;
    warmth = MOONISH;
  } else if (currentTime - lastTouchTime > CONTINENTAL_INTERVAL) {
    queued = true;
    warmth = CONTINENTAL;
  }
}


/* Function: updateFeed
 * Updated the IO feed depending on the warmth state.
 */
void updateFeed() {
  switch (warmth) {
    case CONTINENTAL: publishIO(MAIN_FEED, 1); break;
    case MOONISH: publishIO(MAIN_FEED, 2); break;
    case PLANETARY: publishIO(MAIN_FEED, 3); break;
    default: break;
  }
}


/* Function: lighting
 * Controls the lighting of the LEDs when in the updated or
 * the queued state, which blinks the LEDs.
 */
void lighting() {
  if (waiting) {
    
    blinkLED(WAIT_INTERVAL);
    
  } else if (updated) {
    
    if (state == NEUTRAL) {
      ledsOFF(); 
    } else {
      controlLED();
    }
    updated = false;
    
  } else if (queued) {
    blinkLED(BLINK_INTERVAL);
  } 
}


// blinks the LED when in queue or waiting mode
void blinkLED(unsigned long interval) {
  unsigned long currentTime = millis();
  if(currentTime - lastBlinkTime > interval) {
    if(lightsOn) {
      ledsOFF();
      lightsOn = false;
    } else {
      controlLED();
      lightsOn = true;
    }
    lastBlinkTime = currentTime;
  }
}


// turns on the lEDs depending on the state
void controlLED() {
  switch (warmth) {
    // magenta
    case CONTINENTAL: ledsON(LOW, HIGH, HIGH); break;
    // red
    case MOONISH: ledsON(HIGH, LOW, LOW); break;
    // purple
    case PLANETARY: ledsON(HIGH, LOW, HIGH); break;
    // white
    case UNIVERSAL: ledsON(HIGH, HIGH, HIGH); break;
  }
}


// turns on the LEDs
void ledsON(int red, int green, int blue) {
  digitalWrite(RED_PIN, red);
  digitalWrite(GREEN_PIN, green);
  digitalWrite(BLUE_PIN, blue);
}

// turns off the LEDS
void ledsOFF() {
  digitalWrite(RED_PIN, LOW);
  digitalWrite(GREEN_PIN, LOW);
  digitalWrite(BLUE_PIN, LOW);
}

