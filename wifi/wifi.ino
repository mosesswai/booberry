/* Booberry project!
 * Code: Moses - WIFI
 * by Moses.
 */

// contains network info
#include "config.h"

/***** LED pins *****/
#define RED_PIN   12
#define GREEN_PIN 14
#define BLUE_PIN  13

/***** sensor pins *****/
#define TOUCH_PIN 4

/***** time intervals *****/
#define BLINK_INTERVAL        500
#define DEFAULT_INTERVAL      3000
#define CONTINENTAL_INTERVAL  5000
#define MOONISH_INTERVAL      10000
#define PLANETARY_INTERVAL    15000

/***** set up feeds *****/
// booberry - this is the main feed
AdafruitIO_Feed *feed = io.feed("booberry");

// ready fields to make sure both feeds are engaged
AdafruitIO_Feed *readyMoz = io.feed("boo-moz");
AdafruitIO_Feed *readyLiz = io.feed("boo-liz");

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

// if the timer for the touch sensor is on
boolean listening = false;

// whether we need to update the feed
boolean queued = false;

// allows touch to be used again
boolean restored = true;

// keeps track of light state during blinking
boolean lightsOn = false;

// keeps track of time for the touch sensor and blinker
int lastTime = 0;
int lastBlinkTime = 0;

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

  // make sure LEDs are off
  ledsOFF();

  // create message handlers
  feed->onMessage(handleFeed);

  /* for debugging */
  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());    
}


//loop function
void loop() {
  
  // keeps the client connected to io.adafruit.com, 
  // and processes any incoming data.
  io.run();

  // read the touch sensor
  readTouch();

  // light up the lights
  lighting();
 
}


/* Function: handleFeed 
 * Is called whenever the feed receives new data 
 */
void handleFeed ( AdafruitIO_Data *data ) {
  // convert the data to integer
  int reading = data->toInt();

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

  updated = true;
  
  Serial.print("state changed to: ");
  Serial.println(state);

  Serial.print("warmth changed to: ");
  Serial.println(warmth);
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
      lastTime = millis();
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
  int currentTime = millis();
  if (currentTime - lastTime > DEFAULT_INTERVAL) {
    listening = false;
    restored = false;
    feed->save(0);
  }
}

// listening to touch sensor when in neutral state
void listeningNeutral() {
  int currentTime = millis();
  if (currentTime - lastTime > PLANETARY_INTERVAL) {
    queued = true;
    warmth = PLANETARY;
  } else if (currentTime - lastTime > MOONISH_INTERVAL) {
    queued = true;
    warmth = MOONISH;
  } else if (currentTime - lastTime > CONTINENTAL_INTERVAL) {
    queued = true;
    warmth = CONTINENTAL;
  }
}


/* Function: updateFeed
 * Updated the IO feed depending on the warmth state.
 */
void updateFeed() {
  switch (warmth) {
    case CONTINENTAL: feed->save(1); break;
    case MOONISH: feed->save(2); break;
    case PLANETARY: feed->save(3); break;
    default: break;
  }
}


/* Function: lighting
 * Controls the lighting of the LEDs when in the updated or
 * the queued state, which blinks the LEDs.
 */
void lighting() {
  if (updated) {
    
    if (state == NEUTRAL) {
      ledsOFF(); 
    } else {
      controlLED();
    }
    updated = false;
    
  } else if (queued) {
    blinkLED();
  } 
}


// blinks the LED when in queue mode
void blinkLED() {
  int currentTime = millis();
  if(currentTime - lastBlinkTime > BLINK_INTERVAL) {
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
