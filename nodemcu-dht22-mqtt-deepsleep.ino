/*
  humidity and temperature by 
  NodeMCU + DHT22 + LED + MQTT + DeepSleep
  My first steps in C++
  Based on documentation found on:  
  - https://randomnerdtutorials.com/esp8266-dht11dht22-temperature-and-humidity-web-server-with-arduino-ide/
  - https://diyprojects.io/esp8266-dht22-mqtt-make-iot-include-home-assistant/
  - https://randomnerdtutorials.com/esp8266-deep-sleep-with-arduino-ide/
  - https://lowvoltage.github.io/2017/07/09/Onboard-LEDs-NodeMCU-Got-Two
  
  Licence : MIT
*/

/*  //// Wiring
 *   
 *  //NodeMCU
 *  D0(Wake) -> RST on 470 Ohm resistor   //to wakeup from deepsleep
 *  
 *  //DHT22
 *  1     -> GND
 *  2 None
 *  3     -> DATA + 3V3 on 4.7k Ohm resistor
 *  4     -> 3V3  
 *  
 *  //BMP280
 *  GND   -> GND
 *  VCC   -> 3V3
 *  SCL   -> D1
 *  SDA   -> D2
 *  CSB   -> 3V3 
 *  SDO   -> 3V3
 *
 */


/*
  Summary:
  - connect to Wifi
  - connect to MQTT-broker
  - read sensor every 10 seconds until countdown is over (see variable icountdown)
  - go to deepsleep for 10 minutes
  - reset to start over
  # optional: use onboard led (see variable useled = true/false); 
              different led blinking signals to distinct between events
              - waiting for wifi     - simple blinking
              - wifi-connect success - 6 fast blinks
              - read-sensor          - 2 fast blinks
              - go deepsleep         - 2 long blinks
*/


/* --------------------------------------------- */
 
// nodemcu libraries
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// DHT-sensor libraries
#include "DHT.h"          // library for DHT-sensors

// BMP280-sensor libraries
#include <Wire.h>
#include <Adafruit_BMP280.h>
#include <SPI.h>
 
// ### Sensetive Data ###
#define wifi_ssid "<wifi-ssid>"             // wifi ssid
#define wifi_password "<wifi-password>"     // wifi-password

#define mqtt_server "<ip-adress-mqtt-host>"      // mqtt-ip
#define mqtt_user "<username>"                   // mqtt-user
#define mqtt_password "<password>"               // mqtt-password
// ### Sensetive Data ###

//mqtt definitions
#define temperature_topic "nodemcu-1/dht22/temperature"             //Topic temperature
#define humidity_topic "nodemcu-1/dht22/humidity"                   //Topic humidity
#define pressure_topic "nodemcu-1/bmp280/pressure"                  //Topic pressure
#define secondarytemperature_topic "nodemcu-1/bmp280/temperature2"  //Topic temperature secondary
#define altitude_topic "nodemcu-1/bmp280/altitude"                  //Topic altitude

//DHT22 definitions
#define DHTPIN 14             // DHT Pin; NodeMCU D2 = 4; D4 = 2; D5 = 14
// Un-comment your sensor
//#define DHTTYPE DHT11       // DHT 11
#define DHTTYPE DHT22         // DHT 22  (AM2302)

//BMP280 definitions
#define SEALEVELPRESSURE_HPA (1030.50)  // needed to calculate altitude, set sealevelpressure of your location here
#define BMP_SCK  (13)
#define BMP_MISO (12)
#define BMP_MOSI (11)
#define BMP_CS   (10)

Adafruit_BMP280 bmp; // I2C
//Adafruit_BMP280 bmp(BMP_CS); // hardware SPI
//Adafruit_BMP280 bmp(BMP_CS, BMP_MOSI, BMP_MISO,  BMP_SCK);

// Variables
char message_buff[100];       //Buffer to decode MQTT messages

int icountdown = 3;           // count of sensor-measures in one cycle
int n = 0;                    // arrayposition

float t[5] = { };             // array temperature
float sumt = 0;               // sum of temperature for arithmetic mean
float arithmetict = 0;        // arithmetic mean temperature

float h[5] = { };             // array humidity
float sumh = 0;               // sum of humidity for arithmetic mean
float arithmetich = 0;        // arithmetic mean humidity

float p[5] = { };             // array pressure
float sump = 0;               // sum of pressure for arithmetic mean
float arithmeticp = 0;        // arithmetic mean pressure

float t2[5] = { };             // array secondary temperature
float sumt2 = 0;               // sum of secondary temperature for arithmetic mean
float arithmetict2 = 0;        // arithmetic mean secondary temperature

float a[5] = { };             // array altitude
float suma = 0;               // sum of altitude for arithmetic mean
float arithmetica = 0;        // arithmetic mean altitude

int deepsleepduration = 10;   // Duration of DeepSleep between measures in minutes
bool useled = true;           // Only blink if true
int iblink = 0;               // variable for increment count of led blinks

long lastMsg = 0;
long lastRecu = 0;

bool debug = false;             // Display log message if True

// Create nodemcu objects
WiFiClient espClient;
PubSubClient client(espClient);

// Create dht22 objects
DHT dht(DHTPIN, DHTTYPE);

/* --------------------------------------------- */

void setup() {
  Serial.begin(9600);
  Serial.println("Power up. >>>");
  
  // BMP280 init
  if (!bmp.begin()) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring!"));
    while (1);
  } 

  // BMP280 Default settings from datasheet.
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     // Operating Mode
                  Adafruit_BMP280::SAMPLING_X2,     // Temp. oversampling
                  Adafruit_BMP280::SAMPLING_X16,    // Pressure oversampling
                  Adafruit_BMP280::FILTER_X16,      // Filtering.
                  Adafruit_BMP280::STANDBY_MS_500); // Standby time.
                     
  if ( useled ) {
    Serial.println("useled = true >> Use onboard-LED for status report.");
    pinMode(LED_BUILTIN, OUTPUT);           // Initialize the LED_BUILTIN pin as an output
  } else {
    Serial.println("useled = false >> Dark-mode. No LED-Disco in my bedroom.");
    digitalWrite(LED_BUILTIN, HIGH);      // Turn the LED off by making the voltage HIGH
  }
  setup_wifi();                           //Connect to Wifi network
  client.setServer(mqtt_server, 1883);    // Configure MQTT connexion
  client.setCallback(callback);           // callback function to execute when a MQTT message
  dht.begin();
}

// Wifi-connection
void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ssid: ");
  Serial.print(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
 
    if ( useled ) {
      // short led-blink while trying to connect to wifi
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
      delay(450);
    } else {
      delay(500);   
    }
  }

  Serial.print("Ok. ");
  Serial.print("=> local IP: ");
  Serial.println(WiFi.localIP());

  if ( useled ) {
    // 6 fast led blinks when WiFi is connected
    iblink = 0;
    while (iblink < 6) {
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
      delay(25);
      digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
      delay(75);
      iblink++;
    }
  }
}  

//connect mqtt
void reconnect() {

  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker.5757657678687876876876j5jj5..  ");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_password)) {
      Serial.println("OK");
    } else {
      Serial.print("MQTT Connecting failed. Error: ");
      Serial.print(client.state());
      Serial.println(" Wait 5 secondes before to retry");
      delay(5000);
    }
  }
}

/* --------------------------------------------- */

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  // Read humidity and temperature every 10 seconds and publish to mqtt-broker until countdown is over
  if (now - lastMsg > 1000 * 10) {
    lastMsg = now;
    
    if ( debug ) {
      Serial.print("arraynumber: ");
      Serial.println(n);
    }
    
    // Read DHT22 temperature in Celcius
    t[n] = dht.readTemperature();
    sumt=sumt+t[n];   // sum measures for arithmetic mean
    if ( debug ) {
    Serial.print("sum temperature: ");
    Serial.println(sumt);
    }
    // Read DHT22 humidity
    h[n] = dht.readHumidity();
    sumh=sumh+h[n];   // sum measures for arithmetic mean
    if ( debug ) {
      Serial.print("sum humidity: ");    
      Serial.println(sumh);
    }
    // Oh, nothing to send
    if ( isnan(t[n]) || isnan(h[n])) {
      Serial.println("No readings. Check DHT sensor!");
      return;
    }

    // Read BMP280 temperature in Celcius
    t2[n] = bmp.readTemperature();
    sumt2=sumt2+t2[n];   // sum measures for arithmetic mean
    if ( debug ) {
      Serial.print("secondary temperature: ");    
      Serial.println(t2[n]);
      Serial.print("sum secondary temperature: ");    
      Serial.print(sumt2);
      Serial.println(" °C");      
    }
    // Read BMP280 pressure in hPa
    p[n] = bmp.readPressure()/100;
    sump=sump+p[n];   // sum measures for arithmetic mean
    if ( debug ) {
      Serial.print("pressure: ");    
      Serial.println(p[n]);
      Serial.print("sum pressure: ");    
      Serial.print(sump);
      Serial.println(" hPa");      
    }
    // Read BMP280 altitude
    a[n] = bmp.readAltitude(SEALEVELPRESSURE_HPA);
    suma=suma+a[n];   // sum measures for arithmetic mean
    if ( debug ) {
      Serial.print("altitude: ");    
      Serial.println(a[n]);
      Serial.print("sum altitude: ");    
      Serial.print(suma);
      Serial.println(" m");      
    }

    if ( debug ) {
      Serial.print("Temperature : ");
      Serial.print(t[n]);
      Serial.print(" | Humidity : ");
      Serial.println(h[n]);
    }

    //Loop countdown until DeepSleep
    icountdown = --icountdown;
    Serial.print("Countdown: ");
    Serial.println(icountdown);

    if ( useled ) {
      // ### 2 fast led blinks when topics are published to mqtt broker
      iblink = 0;
      while (iblink < 2) {
        digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
        delay(25);
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
        delay(75);
        iblink++;
      }
      //
    }

    n++;
  }
  if (now - lastRecu > 100 ) {
    lastRecu = now;
    client.subscribe("homeassistant/switch1");
  }

  // Go DeepSleep when countdown is over
  if (icountdown == 0) {
    
    // Calculate arithmetic mean
    arithmetict = sumt/n;     // mean of dht22 temperature
    arithmetich = sumh/n;     // mean of dht22 humidity
    arithmeticp = sump/n;     // mean of bmp280 pressure  
    arithmetica = suma/n;     // mean of bmp280 altitude
    arithmetict2 = sumt2/n;   // mean of bmp280 temperature

    // Announce publishing of values
    Serial.print("Sending ");
    Serial.print("temperature: ");    
    Serial.print(arithmetict);
    Serial.print(" | ");    
    Serial.print("humidity: ");
    Serial.print(arithmetich);
    Serial.print(" | ");    
    Serial.print("pressure: ");
    Serial.print(arithmeticp);    
    Serial.print(" | ");    
    Serial.print("altitude: ");
    Serial.print(arithmetica);
    Serial.print(" | ");    
    Serial.print("secondary temperature: ");
    Serial.print(arithmetict2); 
    Serial.print("...");        
   
   // Publish topics
    client.publish(temperature_topic, String(arithmetict).c_str(), true);                 // Publish dht22 temperature on temperature_topic
    client.publish(humidity_topic, String(arithmetich).c_str(), true);                    // Publish dht22 humidity on humidity_topic
    client.publish(pressure_topic, String(arithmeticp).c_str(), true);                    // Publish bmp280 humidity on humidity_topic
    client.publish(secondarytemperature_topic, String(arithmetict2).c_str(), true);       // Publish bmp280 temperature on humidity_topic
    client.publish(altitude_topic, String(arithmetica).c_str(), true);                    // Publish bmp280 altitude on humidity_topic
    Serial.println("Ok"); // Done
    
    Serial.print("Going to DeepSleep for ");
    Serial.print(deepsleepduration);
    Serial.println(" Minutes.");
    if ( useled ) {
      // ### 2 led blinks before going to DeepSleep
      iblink = 0;
      while (iblink < 2) {
        digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
        delay(500);
        iblink++;
      }
    }
    else {
      delay(2000); // wait 2 seconds to give the device time to finish publishing before power down
    }
    Serial.println("Power down. <<<<");  
    ESP.deepSleep(deepsleepduration * 60 * 1000000);  // 1 Second = 1000000
  }
  delay(5000);  // reloop after 5 seconds
}

/* --------------------------------------------- */

// MQTT callback function
// from http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
void callback(char* topic, byte* payload, unsigned int length) {

  int i = 0;
  if ( debug ) {
    Serial.println("Message recu =>  topic: " + String(topic));
    Serial.print(" | longueur: " + String(length, DEC));
  }
  // create character buffer with ending null terminator (string)
  for (i = 0; i < length; i++) {
    message_buff[i] = payload[i];
  }
  message_buff[i] = '\0';

  String msgString = String(message_buff);
  if ( debug ) {
    Serial.println("Payload: " + msgString);
  }
}
