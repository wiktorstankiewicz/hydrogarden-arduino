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


WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

const String API_URL = "http://localhost:30437/api";
String commandBuffer[10];

const char* brokerUrl = "192.168.0.102";
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

void printState()
{
  for (int i = 1; i <= NUMBER_OF_CIRCUITS; i++)
  {
    int pin = pinFromCircuitCode(i);
    Serial.println("Circuit " + String(i) + ": " + readState(i));
  }
}

void mqttCallback(const char* topic, byte* payload, unsigned int length) {
  Serial.println("asdasd");
  String payloadString(payload, length);
  String topicString(topic);

  JsonDocument doc;

  deserializeJson(doc,payloadString);
  const String commandName = doc["commandName"];
  Serial.println(commandName);
  if(commandName == "ChangeSingleCircuitStateRequest") {
    int circuitCode = doc["circuitId"];
    bool newState = doc["newState"];
    Serial.println(circuitCode);
    Serial.println(newState);

    toggleCircuit(circuitCode, newState);
    printState();

  } else if(commandName == "ChangeMultipleCircuitStateRequest") {
    JsonArray requests = doc["requests"];

    for(JsonVariant request: requests){
      int circuitCode = request["circuitId"];
      bool newState = request["newState"];
      Serial.println(circuitCode);
      Serial.println(newState);

      toggleCircuit(circuitCode, newState);
      printState();
    }
  }

  Serial.println(payloadString);
  Serial.println(topicString);
}

void setup()
{
  Serial.begin(9600);
  while (!Serial)
    ;
  Serial.println("Initialising...");
  EEPROM.begin(512);
  for (int circuitCode = 1; circuitCode <= NUMBER_OF_CIRCUITS; circuitCode++)
  {
    int pin = pinFromCircuitCode(circuitCode);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, readState(circuitCode));
  }
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setHostname("hydrogarden");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(30);
  }
  Serial.println("Connected to WiFi");

  

  
};


 

void mqttReconnect(){
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESPClient-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    mqttClient.setServer(brokerUrl, brokerPort);
    mqttClient.setCallback(mqttCallback);
    mqttClient.setBufferSize(2048);

    if (mqttClient.connect(clientId.c_str(),"arduino", "arduino")) {
      Serial.printf("Connected to MQTT broker at %s:%u", brokerUrl, brokerPort);
      mqttClient.subscribe("toDevice", 1);

      mqttClient.publish("toServer","{\"commandName\":\"RequestCircuitStateRefresh\"}");
      
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
}
}

void wifiReconnect(){
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setHostname("hydrogarden");
  Serial.println("Attempting wifi reconnection");
  while(WiFi.status() != WL_CONNECTED){
      Serial.print(".");
  }
  Serial.println();
}

void tokeniseString(char *input, char **tokens)
{
  char *token = strtok(input, " ");
  int i = 0;
  while (token != NULL)
  {
    tokens[i] = token;
    token = strtok(NULL, " ");
    i++;
  }
}

void readCommand()
{
  int i = 0;
  while (Serial.available())
  {
    commandBuffer[i] = Serial.readStringUntil(' ');
    i++;
  }
}

GeneratedTask *parseCommand()
{
  int circuitCode = -1;
  bool mode = false;
  if (commandBuffer[0] == "toggle")
  {
    circuitCode = commandBuffer[1].toInt();
    mode = !readState(circuitCode);
    Serial.println("Toggled circuit " + String(circuitCode) + " to " + String(mode));
    return new GeneratedTask{circuitCode, mode};
  }
  else if (commandBuffer[0] == "set")
  {
    circuitCode = commandBuffer[1].toInt();
    mode = commandBuffer[2].toInt();
    toggleCircuit(circuitCode, mode);
    Serial.println("Set circuit " + String(circuitCode) + " to " + String(mode));
  }
  else if (commandBuffer[0] == "get")
  {
    int circuitCode = commandBuffer[1].toInt();
    bool state = readState(circuitCode);
    Serial.println("Circuit " + String(circuitCode) + " is " + String(state));
  }
  else if (commandBuffer[0] == "getall")
  {
    for (int i = 1; i <= NUMBER_OF_CIRCUITS; i++)
    {
      Serial.println("Circuit " + String(i) + " is " + String(readState(i)));
    }
  }
  else if (commandBuffer[0] == "help")
  {
    Serial.println("Commands:");
    Serial.println("toggle <circuitCode>");
    Serial.println("set <circuitCode> <mode>");
    Serial.println("get <circuitCode>");
    Serial.println("getall");
    Serial.println("help");
  }
  else
  {
    Serial.println("Invalid command");
  }

  if (circuitCode != -1)
  {
    return new GeneratedTask{circuitCode, mode};
  }
  else
  {
    return 0;
  }
}

GeneratedTask *tryGettingTaskFromSerial()
{
  if (Serial && Serial.available() > 0)
  {
    readCommand();
    return parseCommand();
  }
  return 0;
}

void loop()
{
  mqttClient.loop();
  if(!wifiClient.connected()){
    wifiReconnect();
  }

  if(!mqttClient.connected()){
    mqttReconnect();
  }
  GeneratedTask *taskFromHttp = 0;

  GeneratedTask *taskFromSerial = 0;
  GeneratedTask *taskToPerform = 0;

  taskFromSerial = tryGettingTaskFromSerial();


  taskToPerform = taskFromHttp ? taskFromHttp : taskFromSerial;

  if (taskToPerform == 0)
  {
    // Serial.println("No task");
  }
  else
  {
    toggleCircuit(taskToPerform->circuitCode, taskToPerform->mode);
    if (taskToPerform->mode)
    {
      Serial.println("Task: Circuit " + String(taskToPerform->circuitCode) + " on");
    }
    else
    {
      Serial.println("Task: Circuit " + String(taskToPerform->circuitCode) + " off");
    }
    printState();
  }

  if (taskFromHttp != 0)
  {
    delete taskFromHttp;
  }
  if (taskFromSerial != 0)
  {
    delete taskFromSerial;
  }

  delay(100);
}
