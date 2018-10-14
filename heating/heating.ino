#include <ESP8266WebServerSecureAxTLS.h>
#include <ESP8266WebServer.h>
#include <ESP8266WebServerSecureBearSSL.h>
#include <ESP8266WebServerSecure.h>
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>

//DS18B20
#define ONE_WIRE_BUS D3 //Pin to which is attached a temperature sensor
#define ONE_WIRE_MAX_DEV 15 //The maximum number of devices

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature DS18B20(&oneWire);

int numberOfDevices; //Number of temperature devices found

DeviceAddress devAddr[ONE_WIRE_MAX_DEV];  //An array device temperature sensors
float tempDev[ONE_WIRE_MAX_DEV]; //Saving the last measurement of temperature
float tempDevLast[ONE_WIRE_MAX_DEV]; //Previous temperature measurement

long lastTemp; //The last measurement
const int durationTemp = 6000; //The frequency of temperature measurement


//Distance
long lastPing; 
const int durationPing = 5000; //The frequency of distance measurement

const int trigPin = 2;  //D4
const int echoPin = 4;  //D2

long cm, fillpct;
long duration;
int distance;


//Wifi
const char* ssid = "agerdal";
const char* password = "97721314";


//HTTP

long lastMsg = 0;
int led = 2;
int state = 1;

ESP8266WebServer server(80);

WiFiUDP udp; //UDP for influx
byte udp_host[] = {192, 168, 10, 7}; // the IP address of InfluxDB host
int udp_port = 8089; // the port that the InfluxDB UDP plugin is listening on

String hostname  = "ArduinoOTA.getHostname";
String payload = "default";

//Convert device id to String
String GetAddressToString(DeviceAddress deviceAddress){
  String str = "";
  for (uint8_t i = 0; i < 8; i++){
    if( deviceAddress[i] < 16 ) str += String(0, HEX);
    str += String(deviceAddress[i], HEX);
  }
  return str;
}

//Loop measuring the temperature
void TempLoop(long now){
  if( now - lastTemp > durationTemp ){ //Take a measurement at a fixed time (durationTemp = 5000ms, 5s)
    for(int i=0; i<numberOfDevices; i++){
      float tempC = DS18B20.getTempC( devAddr[i] ); //Measuring temperature in Celsius
      tempDev[i] = tempC; //Save the measured value to the array
      delay(100);
    }
    DS18B20.setWaitForConversion(true); //No waiting for measurement
    DS18B20.requestTemperatures(); //Initiate the temperature measurement
    lastTemp = millis();  //Remember the last time measurement
    sendUDPTemps();
  }
}

void PingLoop(long now){
  if (now - lastPing > durationPing ) {
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

void HandleRoot(){
  String message = "Number of devices: ";
  message += numberOfDevices;
  message += "\r\n<br>";
  char temperatureString[6];

  message += "<table border='1'>\r\n";
  message += "<tr><td>Device id</td><td>Temperature</td></tr>\r\n";
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
  message += "Hostname: ";
  message += hostname;
  message += ".local\r\n";
  message += "Payload: ";
  message += payload;
  message += "\r\n";
  message += "Distance: ";
  message += distance;
  message += "\r\n";
  message += "Time: ";
  message += millis();
  message += "\r\n";

  server.send(200, "text/html", message );
}

void sendUDPTemps(){
  char temperatureString[6];
  for(int i=0;i<numberOfDevices;i++){
    dtostrf(tempDev[i], 2, 2, temperatureString);
    String location = GetAddressToString( devAddr[i] );
    String value = temperatureString;
    delay(100);
    send_value(location, value);
  }
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


void send_value(String location, String value) {
  payload = "temp";
  payload += ",host="     + hostname;
  payload += ",location=" + location;
  payload += " value="    + value;
  udp.beginPacket(udp_host, udp_port);
  udp.print(payload);
  udp.endPacket();
}

void setup() {
  pinMode(led, OUTPUT);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.begin();
  hostname = ArduinoOTA.getHostname();
  
  server.on("/", HandleRoot);
  server.onNotFound( HandleNotFound );
  server.begin();
  
  //Setup DS18b20 temperature sensor
  DS18B20.begin();
  numberOfDevices = DS18B20.getDeviceCount();
  lastTemp = millis();
  //DS18B20.requestTemperatures();

  lastPing = millis();
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
}

void loop() {
  ArduinoOTA.handle();
  long now = millis();
  if (now - lastMsg > 1000) {
    digitalWrite(led, state);
    state = !state;
    lastMsg = now;
  }

  long t = millis();
  server.handleClient();
  TempLoop( t );
  PingLoop ( t );  
}