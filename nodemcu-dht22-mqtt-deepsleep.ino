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

#include <ESP8266WiFi.h>
#include <PubSubClient.h> 
#include "DHT.h"          // library for DHT-sensors

// ### Sensetive Data ###
#define wifi_ssid "<wifi-ssid>"             // wifi ssid
#define wifi_password "<wifi-password>"     // wifi-password

#define mqtt_server "<ip-adress-mqtt-host>"      // mqtt-ip
#define mqtt_user "<username>"                   // mqtt-user
#define mqtt_password "<password>"               // mqtt-password
// ### Sensetive Data ###

#define temperature_topic "dht22-1/sensor/temperature"  //Topic temperature
#define humidity_topic "dht22-1/sensor/humidity"        //Topic humidity

//Buffer to decode MQTT messages
char message_buff[100];

int icountdown = 5;           // count of sensor-measures before deepsleep
int deepsleepduration = 10;  // Duration of DeepSleep between measures in minutes
bool useled = true;          // Only blink if true
int iblink = 0;              // variable for increment count of led blinks
long lastMsg = 0;
long lastRecu = 0;

bool debug = false;             // Display log message if True

#define DHTPIN 4                // DHT Pin; NodeMCU D2 = 4

// Un-comment your sensor
//#define DHTTYPE DHT11       // DHT 11
#define DHTTYPE DHT22        // DHT 22  (AM2302)

// Create objects
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  Serial.begin(9600);
  Serial.println("Power up. >>>");  
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
    Serial.print("Connecting to MQTT broker...  ");
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

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  // Read humidity and temperature every 10 seconds and publish to mqtt-broker until countdown is over
  if (now - lastMsg > 1000 * 10) {
    lastMsg = now;
    // Read humidity
    float h = dht.readHumidity();
    // Read temperature in Celcius
    float t = dht.readTemperature();

    // Oh, nothing to send
    if ( isnan(t) || isnan(h)) {
      Serial.println("No readings. Check DHT sensor!");
      return;
    }

    if ( debug ) {
      Serial.print("Temperature : ");
      Serial.print(t);
      Serial.print(" | Humidity : ");
      Serial.println(h);
    }
    // Publish topics
    client.publish(temperature_topic, String(t).c_str(), true);   // Publish temperature on temperature_topic
    client.publish(humidity_topic, String(h).c_str(), true);      // and humidity

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
  }
  if (now - lastRecu > 100 ) {
    lastRecu = now;
    client.subscribe("homeassistant/switch1");
  }

  // Go DeepSleep when countdown is over
  if (icountdown == 0) {
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
    Serial.println("Power down. <<<<");  
    ESP.deepSleep(deepsleepduration * 60 * 1000000);  // 1 Second = 1000000
  }
  delay(5000);  // reloop after 5 seconds
}

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
