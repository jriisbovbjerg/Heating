#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ESP8266WebServerSecureAxTLS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WebServerSecureBearSSL.h>
#include <ESP8266WebServerSecure.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

#define DEBUG

//#define LED
#define TEMP
//#define DISTANCE
#define LIGHT

#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
#endif

//Wifi
const char* ssid = "agerdal";
const char* password = "97721314";

//LEDLoop
unsigned long durationLED = 2000;  // duration of the LED blink...
unsigned long lastLED = 0;
int led = 2;
int state = 0;

//HTTP
ESP8266WebServer server(80);

WiFiUDP udp; //UDP for influx
byte udp_host[] = {192, 168, 10, 7}; // the IP address of InfluxDB host
int udp_port = 8089; // the port that the InfluxDB UDP plugin is listening on

String hostname  = "ArduinoOTA.getHostname";
String payload = "default";

#ifdef LED
  void LEDLoop() {
    if (millis() - lastLED > durationLED) {
      digitalWrite(led, state);
      state = !state;
      lastLED = millis();
    }
  }
#endif

void send_value(String location, String value) {
  payload = "temp";
  payload += ",host="     + hostname;
  payload += ",location=" + location;
  payload += " value="    + value;
  udp.beginPacket(udp_host, udp_port);
  udp.print(payload);
  udp.endPacket();
}

#ifdef TEMP
  //DS18B20
  #define ONE_WIRE_BUS 0 //Pin to which is attached a temperature sensor D3=0, D2=4
  #define ONE_WIRE_MAX_DEV 15 //The maximum number of devices

  OneWire oneWire(ONE_WIRE_BUS); 
  DallasTemperature DS18B20(&oneWire);            // Pass the oneWire reference to Dallas Temperature.

  int numberOfDevices; //Number of temperature devices found

  DeviceAddress devAddr[ONE_WIRE_MAX_DEV];  //An array device temperature sensors
  float tempDev[ONE_WIRE_MAX_DEV]; //Saving the last measurement of temperature
  float tempDevLast[ONE_WIRE_MAX_DEV]; //Previous temperature measurement

  unsigned long lastTemp; //Time of last measurement
  const int durationTemp = 3000; //The time between temperature measurement
    //Convert device id to String
  String GetAddressToString(DeviceAddress deviceAddress){
    String str = "";
    for (uint8_t i = 0; i < 8; i++){
      if( deviceAddress[i] < 16 ) str += String(0, HEX);
      str += String(deviceAddress[i], HEX);
    }
    return str;
  }

  void setupDS18B20(){
    DS18B20.begin();
    
    numberOfDevices = DS18B20.getDeviceCount();
    Serial.print( "Device count: " );
    Serial.println( numberOfDevices );

    lastTemp = millis();
    DS18B20.requestTemperatures();

    // Loop through each device, print out address
    for(int i=0;i<numberOfDevices; i++){
      // Search the wire for address
      if( DS18B20.getAddress(devAddr[i], i) ){
        //devAddr[i] = tempDeviceAddress;
        Serial.print("Found device ");
        Serial.print(i, DEC);
        Serial.print(" with address: " + GetAddressToString(devAddr[i]));
        Serial.println();
      }else{
        Serial.print("Found ghost device at ");
        Serial.print(i, DEC);
        Serial.print(" but could not detect address. Check power and cabling");
      }

      //Get resolution of DS18b20
      Serial.print("Resolution: ");
      Serial.print(DS18B20.getResolution( devAddr[i] ));
      Serial.println();

      //Read temperature from DS18b20
      float tempC = DS18B20.getTempC( devAddr[i] );
      Serial.print("Temp C: ");
      Serial.println(tempC);
    }
  }

  //Loop measuring the temperature
  void TempLoop () {
    if( millis() - lastTemp > durationTemp ){ //Take a measurement at a fixed time (durationTemp = 5000ms, 5s)
      String value;
      char temperatureString[6];
      DS18B20.setWaitForConversion(true); //No waiting for measurement
      DS18B20.requestTemperatures(); //Initiate the temperature measurement
      for(int i=0; i<numberOfDevices; i++){
        
        float tempC = DS18B20.getTempC( devAddr[i] ); //Measuring temperature in Celsius
        dtostrf(tempC, 2, 2, temperatureString);
        String value = temperatureString;

        String location = GetAddressToString( devAddr[i] );
        tempDev[i] = tempC; //Save the measured value to the array for use in http reply
        send_value(location, value);
        delay(100);
      }
      
      lastTemp = millis();  //Remember the last time measurement
    }
  }
#endif

#ifdef LIGHT
  // The address will be different depending on whether you leave
  // the ADDR pin float (addr 0x39), or tie it to ground or vcc. In those cases
  // use TSL2561_ADDR_LOW (0x29) or TSL2561_ADDR_HIGH (0x49) respectively
  Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 000001);

  float ligthValue;
  unsigned long lastLigth;
  const int durationLight = 5000; // The time between ligth measurements

  void displaySensorDetails(void) {
    sensor_t sensor;
    tsl.getSensor(&sensor);
    Serial.println("------------------------------------");
    Serial.print  ("Sensor:       "); Serial.println(sensor.name);
    Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
    Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
    Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
    Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
    Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
    Serial.println("------------------------------------");
    Serial.println("");
    delay(500);
  }

  void configureSensor(void) {
    /* You can also manually set the gain or enable auto-gain support */
    // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
    // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
    tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
    
    /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
    // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
    // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
    tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

    /* Update these values depending on what you've set above! */  
    Serial.println("------------------------------------");
    Serial.print  ("Gain:         "); Serial.println("Auto");
    Serial.print  ("Timing:       "); Serial.println("402 ms");
    Serial.println("------------------------------------");

  }

    void setupLightSensor() {
    
    Serial.println("Light Sensor Test"); Serial.println("");
  
    /* Initialise the sensor */
    //use tsl.begin() to default to Wire, 
    //tsl.begin(&Wire2) directs api to use Wire2, etc.
    if(!tsl.begin())
    {
      /* There was a problem detecting the TSL2561 ... check your connections */
      Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
      while(1);
    }
    
    /* Display some basic information on this sensor */
    displaySensorDetails();
    
    /* Setup the sensor gain and integration time */
    configureSensor();
  }

  void LightLoop() {
    if (millis() - lastLigth > durationLight ) {
      /* Get a new sensor event */ 
      sensors_event_t event;

      tsl.getEvent(&event);
      ligthValue = event.light;
      send_value("light-01", String(event.light));

      /* Display the results (light is measured in lux) */
      if (event.light) {
        Serial.print(event.light); Serial.println(" lux");
        
      }
      else {
        /* If event.light = 0 lux the sensor is probably saturated
        and no reliable data could be generated! */
        Serial.println("Sensor overload");
      }
    }
  }
#endif
  
#ifdef DISTANCE
  unsigned long lastPing; 
  const int durationPing = 60000; //The time between distance measurement

  const int trigPin = 2;  //D4
  const int echoPin = 4;  //D2

  long cm, fillpct;
  long duration;
  int distance;

  void DistLoop(){
    if (millis() - lastPing > durationPing ) {
      digitalWrite(trigPin, LOW); // Clears the trigPin
      delayMicroseconds(2);
      digitalWrite(trigPin, HIGH); // Sets the trigPin on HIGH state for 10 micro seconds
      delayMicroseconds(10);
      digitalWrite(trigPin, LOW);
      duration = pulseIn(echoPin, HIGH); // Reads the echoPin, returns the sound wave travel time in microseconds
      distance = duration * 0.034 / 2; // Calculating the distance
      lastPing = millis();  //Remember the last time measurement
    }
  }
  
  long scaleMeasurementToDisplay(long value, long maxX, long minX, long maxY, long minY) {
    long m = (maxY - minY) / (maxX - minX);
    long b = minY + ( m * minX);
    return ( value * m ) + b;
  }
#endif

void HandleRoot(){
  char temperatureString[6];

  String message = "Hostname: ";
  message += hostname;
  message += ".local\r\n<br>";

  message += "Last Payload: ";
  message += payload;
  message += "\r\n<br>";

  #ifdef LIGHT
    message += "Ligth [lux]: ";
    message += ligthValue;
    message += "\r\n<br>";
  #endif

  #ifdef DISTANCE
    message += "Distance: ";
    message += distance;
    message += "\r\n<br>";
  #endif

  message += "Time: ";
  message += millis();
  message += "\r\n<br>";
  
  #ifdef TEMP
    message += "Number of Temperature devices: ";
    message += numberOfDevices;
    message += "\r\n<br>";

    message += "<table border='1'>\r\n";
    message += "<tr><td>Device id</td><td>Temperature [C]</td></tr>\r\n";
    for(int i=0;i<numberOfDevices;i++){
      dtostrf(tempDev[i], 2, 2, temperatureString);
      message += "<tr><td>";
      message += GetAddressToString( devAddr[i] );
      message += "</td>\r\n";
      message += "<td>";
      message += temperatureString;
      message += "</td></tr>\r\n";
      message += "\r\n";
    }
    
    message += "</table>\r\n";
  #endif

  server.send(200, "text/html", message );
}

void HandleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/html", message);
}

void setup() {
  
  pinMode(led, OUTPUT);
  // We start by connecting to a WiFi network
  #ifdef DEBUG
    Serial.begin(115200);
    Serial.println();
    DEBUG_PRINT ("Connecting to ");
    DEBUG_PRINT (ssid);
  #endif

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    ESP.restart();
  }
  DEBUG_PRINT ("");
  DEBUG_PRINT ("WiFi connected");
  DEBUG_PRINT ("IP address: ");
  DEBUG_PRINT (WiFi.localIP());   //You can get IP address assigned to ESP
  ArduinoOTA.begin();
  hostname = ArduinoOTA.getHostname();
  DEBUG_PRINT ("hostname: ");
  DEBUG_PRINT (hostname);
  
  DEBUG_PRINT ("Setting up MDNS responder...");
  MDNS.begin(hostname);

  MDNS.addService("http","tcp", 80 );
  
  server.on("/", HandleRoot);
  server.onNotFound( HandleNotFound );
  server.begin();
  
  digitalWrite(led, 1);
  #ifdef LED
    lastLED = millis();
  #endif

  //Setup DS18b20 temperature sensor
  #ifdef TEMP
    setupDS18B20();
  #endif

  #ifdef DISTANCE
    lastPing = millis();
    pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
    pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  #endif

  #ifdef LIGHT
    setupLightSensor();
  #endif
}

void loop() {
  ArduinoOTA.handle();
  MDNS.update();

  server.handleClient();

  #ifdef LED
    LEDLoop();
  #endif

  #ifdef TEMP
    TempLoop();
  #endif

  #ifdef DISTANCE
    DistLoop();
  #endif

  #ifdef LIGHT
    LightLoop();
  #endif
}