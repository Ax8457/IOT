/*
    IOT Application
    Personal Project Axel BIEGALSKI

*/

/*
  LIBRARIES AND VARS; STRUCT ....
*/
//include
#include "DHT.h"
#include <ArduinoJson.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <PubSubClient.h>

//LED
#define LED_GREEN 18
#define LED_RED 19
#define LED_YELLOW 33
//NeoPixel
#define LED_PIN 26
#define LED_COUNT 1
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_RGBW + NEO_KHZ800); // 1 LED on pin 2
//DHT 
#define DHTPIN 17 
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE); 
// MQTT connection details
#define MQTT_HOST "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_DEVICEID "redacted
#define MQTT_USER "" // no need for authentication, for now
#define MQTT_TOKEN "" // no need for authentication, for now
#define MQTT_TOPIC "redacted"
#define MQTT_TOPIC_DISPLAY "redacted" 
//https://www.hivemq.com/demos/websocket-client/
//user: redacted , password:redacted
//clientID redacted
//MQTT object
void callback(char* topic, byte* payload, unsigned int length);
WiFiClient wifiClient;
PubSubClient mqtt(MQTT_HOST, MQTT_PORT, callback, wifiClient);
//Time
const char* ntpServer = "pool.ntp.org"; //ntp server
const long  gmtOffset_sec = 0;
const int   daylightOffset_sec = 3600;
//struct for DHT metrics
  //create struct
struct DHTMetrics {
  float h;
  float t;
  float f;
};
//json object
StaticJsonDocument<512> jsonDoc;//allocate memory
//overall system status
int isOk = 0;
/*
    FUNCTIONS
*/
//update led color
void NeoPixel_setColor(int status) {
  //green
  if (status == 0){
    strip.setPixelColor(0, strip.Color(50, 0, 0)); 
    strip.show();
  }
  //red
  else if (status == 1){
    strip.setPixelColor(0, strip.Color(0, 50, 0)); 
    strip.show(); 
  }
}

//Wifi setup
void setup_wifi(){
  WiFi.mode(WIFI_STA); //Set the ESP to station mode (client)
  char* ssid = "redacted";
  char* password = "redacted";
  WiFi.begin(ssid, password);
  delay(3000);
  if (WiFi.status() == WL_CONNECTED){
    Serial.println("[+] Connected to Wifi,Ip :");
    Serial.println(WiFi.localIP());
    flash_LED(LED_YELLOW,1);
    //NeoPixel_setColor(0);
  }
}

/*
DEBUG
//print local time
void printLocalTime(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}
*/

//update neopixel led if connection 
void check_wifi_state(){
    while(WiFi.status() != WL_CONNECTED){
      
      delay(1000);
      Serial.println("=> Reconnecting to Wifi ...");
    } 
    
}

//read DHT metrics
DHTMetrics get_DHT_metrics()
{
  //float metrics 
  DHTMetrics m; //dedicated struct created
  m.h = dht.readHumidity();
  m.t = dht.readTemperature();
  m.f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(m.h) || isnan(m.t) || isnan(m.f)) {
    Serial.println(F("[x] Failed to read from DHT sensor!"));
    isOk = +1;
    m.t = 0;
    m.h = 0;
    m.f = 0;
    return m;
  }
  //return metrics
  return m;
}

//create metrics in JSON format
JsonObject craft_metrics_json(DHTMetrics m){
  //get time
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("[x] Failed to obtain time");
    isOk += 1;
    JsonObject err = jsonDoc.to<JsonObject>();
    err["error"] = "<[x] Failed to obtain time";
    return err;
  }
  char timeStr[50];  
  strftime(timeStr, sizeof(timeStr), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  //create payload
  JsonObject payload = jsonDoc.to<JsonObject>();
  JsonObject metrics = payload.createNestedObject("Metrics");
  //metrics
  metrics["Temperature_C"] = m.t;
  metrics["Temperature_F"] = m.f;
  metrics["Humidity"] = m.h;
  JsonObject time = payload.createNestedObject("Time");
  time["Time"] = timeStr;

  //return payload
  return payload;
}

//connect MQTT broker
void connect_MQTT_broker(){
  if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
    Serial.println("[+] MQTT Connected");
    mqtt.subscribe(MQTT_TOPIC_DISPLAY);
    
  } else {
    Serial.println("[x] MQTT Failed to connect!");
    isOk += 1;
    ESP.restart();
  }
}

//callback function
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] : ");
  
  payload[length] = 0; // ensure valid content is zero terminated so can treat as c-string
  Serial.println((char *)payload);
}

//check MQTT connection state
void check_MQTT_connectionState(){
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(MQTT_DEVICEID, MQTT_USER, MQTT_TOKEN)) {
      Serial.println("MQTT Connected");
      mqtt.subscribe(MQTT_TOPIC_DISPLAY);
      
      mqtt.loop();
    } else {
      Serial.println("MQTT Failed to connect!");
      isOk += 1;
      delay(5000);
    }
  }
}

//send JSON message to MQTT broker
void send_JSON_MQTT(JsonObject p ){
  char jsonBuffer[512];
  serializeJson(p,jsonBuffer,sizeof(jsonBuffer));
  //Serial.println("[=>] Sending msg to Broker"); 
  if (!mqtt.publish(MQTT_TOPIC, jsonBuffer)) {
    Serial.println("[x] MQTT Publish failed");
    isOk += 1;
  }
  else{
    //Serial.println("[OK] Message Sent");
  }
}

//Led Green
void flash_LED(int led_pin,int ledMode){
  switch(ledMode){
    case 0:
      digitalWrite(led_pin, LOW);  // LED off
      break;
    case 1:
      digitalWrite(led_pin, HIGH); // LED on
      break;
    case 2:
      digitalWrite(led_pin, HIGH);
      delay(500);
      digitalWrite(led_pin, LOW);
      delay(500);                   // LED flashing
      break;
    default:
      break;
  }
}

//check system status
void check_system(){
  if (isOk == 0){
    flash_LED(LED_RED,0);
    flash_LED(LED_GREEN, 1);
  }
  else{
    Serial.println("[!] System in error state");
    flash_LED(LED_GREEN, 0);
    flash_LED(LED_RED,1);
  }
}

/*
      SETUP & LOOP
*/
//setup
void setup() {
  //LED Green & Red
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);

  Serial.begin(115200);
  //init & start devices connected
  setup_wifi();
  dht.begin();
  strip.begin(); 
  //time
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //conect mqtt
  connect_MQTT_broker();
}

void loop() {
  mqtt.loop();
  isOk = 0; // reset error counter 
  check_wifi_state(); 
  check_MQTT_connectionState();
  DHTMetrics m = get_DHT_metrics();
  JsonObject p = craft_metrics_json(m);
  send_JSON_MQTT(p);
  check_system();
  delay(5000);
}
