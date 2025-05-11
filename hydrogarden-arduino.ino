#include <EEPROM.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"
#include <PubSubClient.h>
#define NUMBER_OF_CIRCUITS 8
#define TIMEOUT_MILLIS 300000

struct GeneratedTask
{
  int circuitCode;
  bool mode; // true = on, false = off
};

namespace Device {

  enum MqttConnectionStatus {
    CONNECTION_TIMEOUT = -4,
    CONNECTION_LOST = -3,
    CONNECT_FAILED = -2,
    DISCONNECTED = -1,
    CONNECTED = 0,
    CONNECT_BAD_PROTOCOL = 1,
    CONNECT_BAD_CLIENT_ID = 2,
    CONNECT_UNAVAILABLE = 3,
    CONNECT_BAD_CREDENTIALS = 4,
    CONNECT_UNAUTHORIZED = 5
  };
  
  String mqttStatusToString(MqttConnectionStatus status) {
    switch (status) {
        case CONNECTION_TIMEOUT: return "MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time";
        case CONNECTION_LOST: return "MQTT_CONNECTION_LOST - the network connection was broken";
        case CONNECT_FAILED: return "MQTT_CONNECT_FAILED - the network connection failed";
        case DISCONNECTED: return "MQTT_DISCONNECTED - the client is disconnected cleanly";
        case CONNECTED: return "MQTT_CONNECTED - the client is connected";
        case CONNECT_BAD_PROTOCOL: return "MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT";
        case CONNECT_BAD_CLIENT_ID: return "MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier";
        case CONNECT_UNAVAILABLE: return "MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection";
        case CONNECT_BAD_CREDENTIALS: return "MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected";
        case CONNECT_UNAUTHORIZED: return "MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect";
        default: return "Unknown MQTT status";
    }
  }

  enum Level {
    TRACE, DEBUG, INFO, WARN, ERROR, FATAL
  };
  
  String levelToString(Level level){
    switch(level){
      case TRACE:
        return "TRACE";
      case DEBUG:
        return "DEBUG";
      case INFO:
        return "INFO";
      case WARN:
        return "WARN";
      case ERROR:
        return "ERROR";
      case FATAL:
        return "FATAL";
    }
    return "UNKNOWN";
  }



WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const String API_URL = "http://localhost:30437/api";
String commandBuffer[10];

//const char* brokerUrl = "192.168.0.102";
const char* brokerUrl = "130.61.230.127";
const u_int16_t brokerPort = 1883;

int pinFromCircuitCode(int circuitCode)
{
  switch (circuitCode)
  {
  case 1:
    return 15;
    break;
  case 2:
    return 2;
    break;
  case 3:
    return 4;
    break;
  case 4:
    return 5;
    break;
  case 5:
    return 18;
    break;
  case 6:
    return 19;
    break;
  case 7:
    return 21;
    break;
  case 8:
    return 22;
    break;
  default:
    return -1;
    break;
  }
}

void openCircuit(int circuitCode)
{
  int pin = pinFromCircuitCode(circuitCode);
  digitalWrite(pin, HIGH);
  EEPROM.put<bool>(pin * 2, true);
  EEPROM.commit();
}

void closeCircuit(int circuitCode)
{
  int pin = pinFromCircuitCode(circuitCode);
  digitalWrite(pin, LOW);
  EEPROM.put<bool>(pin * 2, false);
  EEPROM.commit();
}

void toggleCircuit(int circuitCode, bool mode)
{
  if (mode)
  {
    openCircuit(circuitCode);
  }
  else
  {
    closeCircuit(circuitCode);
  }
}

bool readState(int circuitCode)
{
  int pin = pinFromCircuitCode(circuitCode);
  return EEPROM.readBool(pin * 2);
}

void writeState(int circuitCode, bool state)
{
  int pin = pinFromCircuitCode(circuitCode);
  EEPROM.put<bool>(pin * 2, state);
  EEPROM.commit();
}

void printState(Level level)
{
  String buf = "";  
  for (int i = 1; i <= NUMBER_OF_CIRCUITS; i++)
  {
    int pin = pinFromCircuitCode(i);
    char pinStr[8];
    char codeStr[8];
    char stateStr[2];  
    itoa(pin, pinStr, 10);
    itoa(i, codeStr, 10);
    itoa(readState(i), stateStr, 10);

    const char* lineTemplate = "Circuit %s (pin %s):%s ";
    String line = formatString(lineTemplate, codeStr,pinStr, stateStr);
    
    buf.concat(line);  
  }

  log(buf.c_str(), level);  
}


void mqttCallback(const char* topic, byte* payload, unsigned int length) {
  String payloadString(payload, length);
  String topicString(topic);

  JsonDocument doc;

  deserializeJson(doc,payloadString);
  const String commandName = doc["commandName"];
  if(commandName == "ChangeSingleCircuitStateRequest") {
    int circuitCode = doc["circuitId"];
    bool newState = doc["newState"];

    toggleCircuit(circuitCode, newState);
    printState(DEBUG);

  } else if(commandName == "ChangeMultipleCircuitStateRequest") {
    JsonArray requests = doc["requests"];

    for(JsonVariant request: requests){
      int circuitCode = request["circuitId"];
      bool newState = request["newState"];

      toggleCircuit(circuitCode, newState);
      printState(DEBUG);
    }
  }

}




 

void mqttReconnect(){
  while (!mqttClient.connected()) {
    info("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESPClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    mqttClient.setServer(brokerUrl, brokerPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(2048);
    if (mqttClient.connect(clientId.c_str(),MQTT_USER, MQTT_PASSWORD)) {
      info(formatString("Connected to MQTT broker at %s:%u", brokerUrl, brokerPort).c_str());
      mqttClient.subscribe("toDevice", 1);
      mqttClient.publish("CircuitStateRefreshDeviceRequestQueue","{\"commandName\":\"RequestCircuitStateRefresh\"}");
      
    } else {
      Device::MqttConnectionStatus status = static_cast<Device::MqttConnectionStatus>(mqttClient.state());
      warn(formatString("Failed to connect to mqtt client, reason: %s", Device::mqttStatusToString(status)).c_str());
      // Wait 5 seconds before retrying
      delay(5000);
    }
}
}

void wifiReconnect(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setHostname("hydrogarden");
  info("Attempting wifi reconnection");
  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
  }
  info("Connected to wifi");
}

const String formatString(const char *format, ...) {
  static const u_int BUF_LEN=2048; 
  char buf[BUF_LEN]; 
  va_list args;
  va_start(args, format);
  vsnprintf(buf, BUF_LEN, format, args);
  va_end(args);
  return String(buf);
}





void log(const char* message, Level level) {

  const char* log_template = "Device says %s: %s";
  if(Serial.availableForWrite()){
    Serial.printf(log_template,levelToString(level),message);
    Serial.println();
  }

  if(mqttClient.connected()){
    static const size_t MAX_BUF_LEN = 1024;
    const char* jsonTemplate = "{\"commandName\":\"LogMessagePayload\", \"timestamp\":null, \"level\":\"%s\", \"message\":\"%s\"}";
    char json[MAX_BUF_LEN];
    snprintf(json,MAX_BUF_LEN,jsonTemplate,levelToString(level),message);
    mqttClient.publish("LogEventDeviceRequestQueue",json);
  }
}

void trace(const char* message){
  return log(message,TRACE);
}

void debug(const char* message){
  return log(message,DEBUG);
}

void info(const char* message){
  return log(message,INFO);
}

void warn(const char* message){
  return log(message,WARN);
}

void error(const char* message){
  return log(message,ERROR);
}

void fatal(const char* message){
  return log(message,FATAL);
}

}

void disableAllCircuits(){
  for(int i =1; i<=8; i++){
    Device::closeCircuit(i);
  }
}

void setup()
{
  Serial.begin(9600);
  while (!Serial)
    ;
  Device::info("Initialising...");
  EEPROM.begin(512);
  for (int circuitCode = 1; circuitCode <= NUMBER_OF_CIRCUITS; circuitCode++)
  {
    int pin = Device::pinFromCircuitCode(circuitCode);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setHostname("hydrogarden");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(30);
  }
  Device::info("Connected to WiFi");
};

void loop()
{
  Device::mqttClient.loop();
  if(!Device::wifiClient.connected()){
    disableAllCircuits();
    Device::wifiReconnect();
  }

  if(!Device::mqttClient.connected()){
    disableAllCircuits();
    Device::mqttReconnect();
  }
}
