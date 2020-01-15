/*
  Messung der Temperatur und Luftfeuchtigkeit
  NodeMCU + DHT22 9 LED + MQTT + DeepSleep + Homeassistant


  Auf Grundlage von Beträgen gefunden auf
  - https://diyprojects.io/esp8266-dht22-mqtt-make-iot-include-home-assistant/
  - https://randomnerdtutorials.com/esp8266-deep-sleep-with-arduino-ide/
  - https://lowvoltage.github.io/2017/07/09/Onboard-LEDs-NodeMCU-Got-Two
  Licence : MIT
*/

#include <ESP8266WiFi.h>
#include <PubSubClient.h> 
#include "DHT.h"          // library for DHT-sensors

#define wifi_ssid "<wifi-ssid>"
#define wifi_password "<wifi-password>"

#define mqtt_server "<ip-adress-mqtt-host>"
#define mqtt_user "<username>"                         // mqtt-user
#define mqtt_password "<password>"                     // mqtt-password

#define temperature_topic "<room>/sensor/temperature"  //Topic temperature
#define humidity_topic "<room>/sensor/humidity"        //Topic humidity

//Buffer to decode MQTT messages
char message_buff[100];

int icountdown = 5;             // count of sensor-measures before
bool ledblink = false;          // Only blink if true
int iledblink = 0;              // variable for increment count of led blinks
long lastMsg = 0;
long lastRecu = 0;

bool debug = false;             // Display log message if True

#define DHTPIN 4                // DHT Pin; NodeMCU D2 = 4

// Un-comment your sensor
//#define DHTTYPE DHT11       // DHT 11
#define DHTTYPE DHT22         // DHT 22  (AM2302)

// Create objects
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);           // Initialize the LED_BUILTIN pin as an output
  Serial.begin(9600);
  delay(500);
  if ( ledblink ) {
    Serial.print("Ok. With Blink.");
  } else {
    Serial.print("Ok. Silent-mode. No Blink.");
    digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
  }
  setup_wifi();                           //Connect to Wifi network
  client.setServer(mqtt_server, 1883);    // Configure MQTT connexion
  client.setCallback(callback);           // callback function to execute when a MQTT message
  dht.begin();
}

//wifi-connection
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("nodemcu connecting to ");
  Serial.println(wifi_ssid);
  WiFi.begin(wifi_ssid, wifi_password);

  if ( ledblink ) {
    while (WiFi.status() != WL_CONNECTED) {
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
      delay(50);
      digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
      delay(500);
      Serial.print(".");
    }
  }

  Serial.println("");
  Serial.println("nodemcu online");
  Serial.print("=> ESP8266 IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print(" ");

  if ( ledblink ) {
    // 6 led blinks when WiFi is connected
    iledblink = 0;
    while (iledblink < 6) {
      digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
      delay(50);                      // Wait for a second
      digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
      delay(50);                      // Wait for two seconds
      iledblink++;
    }
  }
}

//connect mqtt
void reconnect() {

  while (!client.connected()) {
    Serial.print("nodemcu connecting to MQTT broker ...");
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
  // Send a message every 10 seconds
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

    if ( ledblink ) {
      // ### 2 led blinks when topics are published to mqtt broker
      iledblink = 0;
      while (iledblink < 2) {
        digitalWrite(LED_BUILTIN, LOW);                             // Turn the LED on by making the voltage LOW
        delay(25);                      // Wait for a second
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
        delay(75);                      // Wait for two seconds
        iledblink++;
      }
      //
    }
  }
  if (now - lastRecu > 100 ) {
    lastRecu = now;
    client.subscribe("homeassistant/switch1");
  }

  // Go DeepSleep when enough measures were done
  if (icountdown == 0) {
    Serial.print("Decided to sleep for 10 minutes...");

    if ( ledblink ) {
      // ### 6 fast led blinks when topics are published to mqtt broker
      iledblink = 0;
      while (iledblink < 2) {
        digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on by making the voltage LOW
        delay(500);
        digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
        delay(500);
        iledblink++;
      }
    }
    ESP.deepSleep(10 * 60 * 1000000);
  }
  delay(5000);                                                    // reloop after 5 seconds
}

// MQTT callback function
// D'après http://m2mio.tumblr.com/post/30048662088/a-simple-example-arduino-mqtt-m2mio
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