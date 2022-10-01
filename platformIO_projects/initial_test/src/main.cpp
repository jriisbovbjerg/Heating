#include <Arduino.h>
#define BlueLED 2 //Define blinking LED pin
#define LED 5
#define LDR 17
int counter;
bool light = false;
bool blueLight = false;

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;        // will store last time LED was updated
unsigned long previousBlinkMillis = 0;        // will store last time LED was updated

// constants won't change :
const long info_interval = 60000;           // interval at which to report (milliseconds)
const long blink_time_off = 450;           // interval at which to report (milliseconds)
const long blink_time_on = 50;           // interval at which to report (milliseconds)
const long factor = 3600000 / info_interval;



void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  pinMode(BlueLED, OUTPUT); // Initialize the LED pin as an output
  pinMode(LDR, INPUT);
}

void loop() {
	int sensorValue = analogRead(LDR);   // read the input on analog pin 0
	
	unsigned long currentMillis = millis();
	
	if ((currentMillis - previousBlinkMillis) >= (blink_time_off)) {
    	if (!blueLight) {
			digitalWrite(BlueLED, LOW); // Turn the LED on (Note that LOW is the voltage level)
			blueLight = true;
		}
	}
	if ((currentMillis - previousBlinkMillis) >= (blink_time_off + blink_time_on)) {
	 if (blueLight) {
			digitalWrite(BlueLED, HIGH); // Turn the LED off by making the voltage HIGH
			blueLight = false;
			previousBlinkMillis = currentMillis;
		}
	}


	if (currentMillis - previousMillis >= info_interval) {
    	// save the last time you blinked the LED
    	previousMillis = currentMillis;
		//Serial.print("sensorvalue: ");  
		//Serial.println(sensorValue);
		Serial.print(" in 60 sec - counts: ");  
		Serial.print(counter);
		Serial.print(" - current power consumption: ");  
		Serial.print(counter * factor / 1000.0);
		Serial.println(" kW ");

		counter=0;
	}

  if (sensorValue >=900) {
	  digitalWrite(LED, HIGH);
	  if (!light) {
		  light = true;
		  counter++ ;
	  }
	}
	else {
		light =false;
    	digitalWrite(LED, LOW);
	}
}

