//wifi and mqtt lib
#include <WiFi.h>
#include <PubSubClient.h>
//websocket lib
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <Arduino_JSON.h>
//sensor lib
#include <Adafruit_Sensor.h>
#include <DHT.h>

//connection declaration
#define WIFI_SSID "HUAWEI-N9c4"
#define WIFI_PASSWORD "12345678"
AsyncWebServer server(80);
// Create a WebSocket object
AsyncWebSocket ws("/ws");
#define MQTT_SERVER "192.168.100.113"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "esp32_client"
#define MQTT_TOPIC_TEMPERATURE "temperature"
#define MQTT_TOPIC_HUMIDITY "humidity"
#define MQTT_TOPIC_POTENTIOMETER "potentiometer"
#define MQTT_TOPIC_TILT "tilt"
#define MQTT_TOPIC_DISTANCE "distance"

//pin declaration
#define DHT_PIN 18
#define DHT_TYPE DHT22
#define POTENTIOMETER_PIN 36
#define BUTTON_ADD_PIN 26
#define BUTTON_SUBTRACT_PIN 25
#define BUTTON_RESET_PIN 27
#define DEBOUNCE_TIME  50

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
DHT dht(DHT_PIN, DHT_TYPE);

//data variable
float temp = 0;
float humidity = 0;
float car_speed = 0;
int tilt = 0;
float distance = 0;

int incrementButtonState = 0;
int subtractButtonState = 0;
int resetButtonState = 0;

// Variables to store previous button states
int prevIncrementButtonState = 0;
int prevSubtractButtonState = 0;
int prevResetButtonState = 0;

// Variable to store the last time a button was pressed
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50;
unsigned long resetButtonStartTime = 0;

//JSON conversion for data transfer in WebSocket Notification
JSONVar packetValues;
String message = "";

String getpacketValues(){
  packetValues["temperature"] = String(temp);
  packetValues["tilt"] = String(tilt);
  packetValues["speed"] = String(car_speed);
  packetValues["distance"] = String(distance);

  String jsonString = JSON.stringify(packetValues);
  return jsonString;
}

// Initialize SPIFFS
void initFS() {
  if (!SPIFFS.begin()) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  else{
   Serial.println("SPIFFS mounted successfully");
  }
}

//Connect to WiFi
void connectToWiFi() {
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  Serial.println(WiFi.localIP());
}

//connect to MQTT
void connectToMQTT() {
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
  mqttClient.setCallback(callback);
  while (!mqttClient.connected()) {
    Serial.println("Connecting to MQTT broker...");
    if (mqttClient.connect(MQTT_CLIENT_ID)) {
      Serial.println("Connected to MQTT broker");
//      mqttClient.subscribe(MQTT_TOPIC_LED);
    } else {
      Serial.print("MQTT connection failed. Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

//handle MQTT callback
void callback(char* topic, byte* payload, unsigned int length) {
  
}

//publish functions
void publishTemperatureAndHumidity(float temperature, float humidity) {
  String temperatureStr = String(temperature, 1);
  String humidityStr = String(humidity, 1);
  
  mqttClient.publish(MQTT_TOPIC_TEMPERATURE, temperatureStr.c_str());
  mqttClient.publish(MQTT_TOPIC_HUMIDITY, humidityStr.c_str());
}

void publishPotentiometerValue(float potValue) {
  String potentiometerValue = String(potValue);
  mqttClient.publish(MQTT_TOPIC_POTENTIOMETER, potentiometerValue.c_str());
}

void publishTiltValue(int tiltValue) {
  String tiltValueStr = String(tiltValue);
  mqttClient.publish(MQTT_TOPIC_TILT, tiltValueStr.c_str());
}

void publishDistanceValue(float distance) {
  String tiltValueStr = String(distance);
  mqttClient.publish(MQTT_TOPIC_DISTANCE, tiltValueStr.c_str());
}

//notify function
void notifyClients(String packetValues) {
  ws.textAll(packetValues);
}

//websocket handler
void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    message = (char*)data;
    if (strcmp((char*)data, "getValues") == 0) {
      notifyClients(getpacketValues());
    }
  }
}
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void setup() {
  //set pins
  Serial.begin(115200);
  pinMode(POTENTIOMETER_PIN, INPUT);
  pinMode(BUTTON_ADD_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SUBTRACT_PIN, INPUT_PULLUP);
  pinMode(BUTTON_RESET_PIN, INPUT_PULLUP);
  digitalWrite(BUTTON_RESET_PIN, HIGH);
  dht.begin();

  //initiate Web Folder and connections
  initFS();
  connectToWiFi();
  connectToMQTT();

  initWebSocket();
  
  // Web Server Root URL
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });
  
  server.serveStatic("/", SPIFFS, "/");

  // Start server
  server.begin();
}

void loop() {
  //check connection
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  //button handlers
  incrementButtonState = digitalRead(BUTTON_ADD_PIN);
  subtractButtonState = digitalRead(BUTTON_SUBTRACT_PIN);

  // Check if the increment button state has changed
  if (incrementButtonState != prevIncrementButtonState) {
    lastDebounceTime = millis();
  }

  // Check if the increment button has been stable for the debounce delay
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Check if the increment button is pressed
    if (incrementButtonState == LOW) {
      tilt++;
    }
  }

  // Check if the subtract button state has changed
  if (subtractButtonState != prevSubtractButtonState) {
    lastDebounceTime = millis();
  }

  // Check if the subtract button has been stable for the debounce delay
  if ((millis() - lastDebounceTime) > debounceDelay) {
    // Check if the subtract button is pressed
    if (subtractButtonState == LOW) {
      tilt--;
    }
  }

  // Update the previous button states
  prevIncrementButtonState = incrementButtonState;
  prevSubtractButtonState = subtractButtonState;

  resetButtonState = digitalRead(BUTTON_RESET_PIN);

  // Check if the reset button state has changed
  if (resetButtonState != prevResetButtonState) {
    // If the button is pressed, record the start time
    if (resetButtonState == LOW) {
      resetButtonStartTime = millis();
    }
  }

  // Check if the reset button has been held for 1 second
  if (resetButtonState == LOW && (millis() - resetButtonStartTime) >= 1000) {
    tilt = 0;  // Reset the tilt value
    distance = 0; //reset distance value
  }

  // Update the previous reset button state
  prevResetButtonState = resetButtonState;

  //read sensor and publish
  temp = dht.readTemperature();
  humidity = dht.readHumidity();
  car_speed = analogRead(POTENTIOMETER_PIN)/1400.0;
  distance += car_speed * 0.2;
  if (!isnan(temp) && !isnan(humidity)) {
    publishTemperatureAndHumidity(temp, humidity);
  }
  publishPotentiometerValue(car_speed);

//  Serial.println("add");
//  Serial.println(digitalRead(BUTTON_ADD_PIN));
//  Serial.println("subs");
//  Serial.println(digitalRead(BUTTON_SUBTRACT_PIN));

  publishTiltValue(tilt);  // Publish the updated tilt value
  publishDistanceValue(distance);
  
  //websocket actions
  ws.cleanupClients();
//  Serial.println(getpacketValues());
  notifyClients(getpacketValues());
  
  delay(200);
}
