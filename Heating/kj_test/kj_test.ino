//simple ds18B20 test

#include <DallasTemperature.h>
#include <OneWire.h>

#define ONE_WIRE_BUS 0                          //D1 pin of nodemcu

OneWire oneWire(ONE_WIRE_BUS);
 
DallasTemperature sensors(&oneWire);            // Pass the oneWire reference to Dallas Temperature.

void setup() {
  Serial.begin(9600); 
  sensors.begin();
}

void loop() { 
  sensors.requestTemperatures();                // Send the command to get temperatures  
  Serial.println("Temperature is: ");
  Serial.println(sensors.getTempCByIndex(0));   // Why "byIndex"? You can have more than one IC on the same bus. 0 refers to the first IC on the wire
  delay(500);
}