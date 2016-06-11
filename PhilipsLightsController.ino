#include <ESP8266WiFi.h>
#include <RestClient.h>
#include <ArduinoJson.h>
#include "PhilipsLightsController_private.h"

int LED[] = {2, 0, 15};
int button[] = {13, 12, 4};

volatile bool clickStatus[] = {false, false, false}; 
int8_t lightStatus[] = {0, 0, 0};

char bufIpBridge[50];

RestClient client = NULL;

void setup() {
  Serial.begin(9600); 
  
  for(int i = 0; i < 3; i++) {
    pinMode(button[i], INPUT_PULLUP);
  }

  attachInterrupt(digitalPinToInterrupt(button[0]), clickInterrupt_0, RISING);
  attachInterrupt(digitalPinToInterrupt(button[1]), clickInterrupt_1, RISING);
  attachInterrupt(digitalPinToInterrupt(button[2]), clickInterrupt_2, RISING);

  for(int i = 0; i<3; i++) {
    pinMode(LED[i], OUTPUT);
    digitalWrite(LED[i], LOW);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Unable to connect to wifi. Restarting");
    ESP.restart();
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

  String ipBridge = getBridgeIP();
  ipBridge.toCharArray(bufIpBridge, 50);
  client = RestClient(bufIpBridge, 80);
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

String getBridgeIP() {
  String ip = "";
  do {
    RestClient hueClient = RestClient("www.meethue.com", 443, "FF 35 0A BC 00 9A A4 E8 78 DE F7 95 3E 3B 78 56 69 97 D5 FE");
    DynamicJsonBuffer jsonBuffer;
    String response = "";
  
    int statusCode = hueClient.get("/api/nupnp", &response);
    if(statusCode != 200) {
      Serial.println("Incorrect response from meethue.");
      delay(200);
      continue;
    }
    
    JsonArray& jsonResponse = jsonBuffer.parseArray(response);
    if(!jsonResponse.success())
    {
      Serial.println("Error parsing json response from meethue");
      delay(200);
      continue;
    }
  
    Serial.print("IP Bridge: ");
    Serial.println(jsonResponse[0]["internalipaddress"].asString());

    ip = jsonResponse[0]["internalipaddress"].asString();
    
  } while (ip.length() == 0); // Retry until we get a valid ip
  return ip;
}

void clickInterrupt_0(){
  clickStatus[0] = true;
}

void clickInterrupt_1(){
  clickStatus[1] = true;
}

void clickInterrupt_2(){
  clickStatus[2] = true;
}

void loop(){
  updateLightStatus(lightStatus);

  for(int i = 0; i < 3; i++){
    if(lightStatus[i] == 1) 
      digitalWrite(LED[i], HIGH);
    else if(lightStatus[i] == 0)
      digitalWrite(LED[i], LOW);
    else if(lightStatus[i] == -1)
      digitalWrite(LED[i], LOW);
  }

  for(int i = 0; i < 3; i++){
    if(clickStatus[i]){
      clickStatus[i] = false;
      if(lightStatus[i] != -1){
        setLightStatus(i+1, !lightStatus[i]);
      } else {
        for(int j=0; j<5; j++){
          delay(20);
          digitalWrite(LED[i], HIGH);
          delay(20);
          digitalWrite(LED[i], LOW);
        }
      }
    }
  }
  
  delay(100);
}

void updateLightStatus(int8_t* lightStatus) {
  DynamicJsonBuffer jsonBuffer;
  String response = "";

  char url[50];
  sprintf(url, "/api/%s/lights", username);
  int statusCode = client.get(url, &response);
  if(statusCode != 200){
    Serial.print("Status code: ");
    Serial.println(statusCode);
    Serial.println("Incorrect response from bridge");
    return;
  }
  
  JsonObject& root = jsonBuffer.parseObject(response);
  if (!root.success())
  {
    Serial.println("Error parsing json response");
    return;
  }

  for(int i = 1; i <= 3; i++){
    if(root[String(i)]["state"]["reachable"].as<bool>())
      lightStatus[i-1] = root[String(i)]["state"]["on"].as<bool>();
    else
      lightStatus[i-1] = -1;
  }
}

void setLightStatus(int idLight, bool newStatus) {
  StaticJsonBuffer<200> jsonBuffer;

  JsonObject& root = jsonBuffer.createObject();
  root["on"] = newStatus;
  root["bri"] = 254;

  char body[256];
  root.printTo(body, sizeof(body));

  char url[60];
  sprintf(url, "/api/%s/lights/%d/state", username, idLight);
  int statusCode = client.put(url, body);
}
