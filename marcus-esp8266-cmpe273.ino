/*
  ESP8266 Marcus v2.0 Relay Control by theGeekyLad
*/

// imports
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>

// wifi config
#ifndef STASSID
//#define STASSID "The 192 Network 2.4G"
//#define STAPSK  "Mumbai254"
#define STASSID "OnePlus 8"
#define STAPSK  "rahul123"
#endif

// -------------------------------------------------------------------
// constants & variables
// -------------------------------------------------------------------

const char* ssid = STASSID;
const char* password = STAPSK;
ESP8266WebServer server(2880);
const int DEVICE_COUNT = 4;
const int DEVICES[] = {D5, D6, D7, D8};
int DEVICE_STATES[] = {0, 0, 0, 0};
const int BUTTON = 4;  // pin of touch button
//const int BUTTON_DEVICE_INDEX_DAY = 1;  // device that touch button operates during daytime
//const int BUTTON_DEVICE_INDEX_NIGHT = 3;  // device that touch button operates during nighttime
const int BUTTON_DEVICE_INDEX_FAN = 0;  // device that touch button operates during daytime
const int BUTTON_DEVICE_INDEX_LIGHT = 3;  // device that touch button operates during nighttime
const char* BOARD_NAME = "marcus-switchboard-bedroom-a";

// pir control
const int PIR_PIN = 9;
const int DEVICE_PIR = 2;  // device that PIR sensor operates
boolean isPirInit = false;
boolean wasTriggered = false;
boolean userInRoom = false;
int timePirInit = millis();
int timePirTrigger = millis();

// -------------------------------------------------------------------

// tasks
const int TASK_STATE_CHANGE = 0;
const int TASK_STATE_REPORT = 1;
const int TASK_TIME_REPORT = 2;

// mqtt
//const char* mqtt_server = "test.mosquitto.org";
const char* mqtt_server = "4.tcp.ngrok.io";
WiFiClient espClient;
PubSubClient client(espClient);

void blink(int n, int d) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_BUILTIN, LOW);
    delay(d);
    digitalWrite(LED_BUILTIN, HIGH);
    delay(d);
  }
}

void lightUp() {
  digitalWrite(LED_BUILTIN, LOW);
}

void lightOff() {
  digitalWrite(LED_BUILTIN, HIGH);
}

void changePowerState(int deviceIndex, int newPowerState) {
  digitalWrite(DEVICES[deviceIndex], newPowerState == 0 ? LOW : HIGH);  // ON or OFF
  DEVICE_STATES[deviceIndex] = newPowerState;
  Serial.println("\nPower state has been changed.");
  Serial.println("- Device: " + String(deviceIndex));
  Serial.println("- New Power State: " + String(newPowerState));

  // for all other devices, except for the PIR device
  if (deviceIndex != DEVICE_PIR) {
    if (newPowerState == 1) userInRoom = true;  // user is in the room
    else {
      int i;
      for (i = 1; i < DEVICE_COUNT; i++)
        if (DEVICES[i] != DEVICE_PIR && DEVICE_STATES[i] == 1) break;
      if (i == DEVICE_COUNT) userInRoom = false;
    }
  }
}

void togglePowerState(int deviceIndex) {
  Serial.println("\nPower state is to be toggled as follows.");
  changePowerState(deviceIndex, ((DEVICE_STATES[deviceIndex] + 1) % 2));
}

String getStates() {
  String states = "";
  for (int i = 0; i < DEVICE_COUNT; i++)
    states += String(i) + ":" + String(DEVICE_STATES[i]) + ",";
  return states.substring(0, states.length() - 1);
}

String getStateReport(String states) {
  return (String(BOARD_NAME) + "::" + states).c_str();
}

// -------------------------------------------------------------------
// MQTT CALLBACK
// -------------------------------------------------------------------

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\nMessage arrived [");
  Serial.print(topic);
  Serial.print("] ");

  // get message as string
  String msg = "";
  for (int i = 0; i < length; i++) {
    msg.concat((char)payload[i]);
  }
  Serial.print("Command received on MQTT: ");
  Serial.println(msg);

  // get task
  int firstSeparator = msg.indexOf("::");
  int task = msg.substring(0, firstSeparator).toInt();
  String cmd = msg.substring(firstSeparator+2);

  // run task
  if (task == TASK_STATE_CHANGE) {
    Serial.println("Received power state change task.");
    controlSwitches(cmd);
  } else if (task == TASK_STATE_REPORT) {
    Serial.println("Received power state report task.");
    String stateReport = getStateReport(getStates());
    Serial.println("Going to publish: " + stateReport);
    client.publish("marcus-server-query", stateReport.c_str());
  }
//  else if (task == TASK_TIME_REPORT) {
//    int dayOrNight = cmd.toInt();
//    if (dayOrNight == 1)
//      togglePowerState(BUTTON_DEVICE_INDEX_DAY);
//    else
//      togglePowerState(BUTTON_DEVICE_INDEX_NIGHT);
//  }

  Serial.println("Command run.");

  // Switch on the LED if an 1 was received as first character
//  if ((char)payload[0] == '1') {
//    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
//    // but actually the LED is on; this is because
//    // it is active low on the ESP-01)
//  } else {
//    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
//  }
}

void reconnectServer() {
  // Loop until we're reconnected
  int attempts = 1;
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected to device cloud.");
      blink(3, 500);
      // publish device states (useful for reconnects)
      // TODO enable if you're caching
//      client.publish("marcus-server-query", getStateReport(getStates()).c_str());
//      Serial.println("Sent states to device cloud.");
      // resubscribe
      client.subscribe(BOARD_NAME);
      Serial.println("Subscribed to " + String(BOARD_NAME));
      break;
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      if (attempts == 10) break;
      delay(5000);
      attempts++;
    }
  }
}

String controlSwitches(String powerStates) {
  int i;
  String op = "";
  while ((i = powerStates.indexOf(',')) != -1 || (i = powerStates.length()) != 0) {
    String thisDevicePowerPair = powerStates.substring(0, i);
    int indexOfColon = thisDevicePowerPair.indexOf(':');
    String thisDeviceStr = thisDevicePowerPair.substring(0, indexOfColon);
    String thisPowerStr = thisDevicePowerPair.substring(indexOfColon + 1);
    int thisDevice = thisDeviceStr.toInt();
    int thisPower = thisPowerStr.toInt();
    op += thisDeviceStr + ":" + thisPowerStr + ", ";
    changePowerState(thisDevice, thisPower);
    if (i == powerStates.length()) powerStates = "";
    else powerStates = powerStates.substring(i+1);
  }
  // TODO enable if you're caching
//  client.publish("marcus-server-query", getStateReport(getStates()).c_str());
  return op;
}

// -------------------------------------------------------------------
// SETUP
// -------------------------------------------------------------------

void setup() {

  // initialize wifi
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // initialize built-in led
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // initialize pir pin
  pinMode(PIR_PIN, INPUT);
  
  // initialize relay pins
  for (int i = 0; i < 4; i++) {
    pinMode(DEVICES[i], OUTPUT);
    digitalWrite(DEVICES[i], HIGH);  // OFF
  }

  // initialize touch button
  pinMode(BUTTON, INPUT);

  // initialize switch buttons
//  pinMode(BUTTONS[0], INPUT);
//  pinMode(BUTTONS[1], INPUT);
//  
//  pinMode(BUTTONS[2], OUTPUT);
//  pinMode(BUTTONS[3], OUTPUT);
//  
//  digitalWrite(BUTTONS[2], LOW);
//  digitalWrite(BUTTONS[3], LOW);

  // wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lightUp();
    Serial.print(".");
  }
  blink(3, 500);
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // start mdns
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }

  // -----------------------------------

  // endpoint for power states
  server.on("/", []() {
    Serial.println("Ping.");
    
    // interpreting user's power state changes
    String powerStates = server.arg("power");

    String res = controlSwitches(powerStates);

    // sending success note
    server.sendHeader("Access-Control-Allow-Origin","*");
    server.send(200, "text/plain", res);
  });

  // -----------------------------------

  // start server
  server.begin();

  // -----------------------------------

  // setup mqtt
  client.setServer(mqtt_server, 13887);
  client.setCallback(callback);
}

// -------------------------------------------------------------------
// LOOP
// -------------------------------------------------------------------

int touchButtonPressTime = 0;
boolean touchButtonHasPressed = false;
boolean secondDeviceHasBeenToggled = false;
void loop() {
  // wifi
  server.handleClient();
  MDNS.update();

  // mqtt
  if (!client.connected()) {
    lightUp();
    reconnectServer();
  }
  client.loop();

  // touch button
  if (digitalRead(BUTTON) == 1) {
    if (!touchButtonHasPressed) {
//      Serial.println("Requested marcus-device-cloud for 'time of day' to process button request.");
      Serial.println("Toggling fan.");
      togglePowerState(BUTTON_DEVICE_INDEX_FAN);
      
      touchButtonPressTime = millis();
      touchButtonHasPressed = true;
    }    
    else if (!secondDeviceHasBeenToggled && (millis() - touchButtonPressTime > 2000)) {
      Serial.println("Toggling light.");
      togglePowerState(BUTTON_DEVICE_INDEX_LIGHT);
      secondDeviceHasBeenToggled = true;
    }
  } else {
    touchButtonHasPressed = false;
    secondDeviceHasBeenToggled = false;
  }

  // motion detection
  if (millis() < timePirTrigger)
    timePirTrigger = millis() - 10000;
  if (!isPirInit) {
    if (millis() - timePirInit > 90000) {
      isPirInit = true;
      Serial.println("Blink LED because PIR has been initialized.");
    }
  }
  else if (userInRoom) {
    wasTriggered = false;
    timePirTrigger = millis();
  }
  else if (digitalRead(PIR_PIN) == HIGH && (millis() - timePirTrigger > 10000)) {
    Serial.println("Got a high! If night time, trigger a lamp on at this point.");
    changePowerState(DEVICE_PIR, 1);
    timePirTrigger = millis();
    wasTriggered = true;
  }
  else if (wasTriggered && (millis() - timePirTrigger > 30000)) {
    Serial.println("No one was in this room for a while. If night time, shut off lamps at this point");
    changePowerState(DEVICE_PIR, 0);
    wasTriggered = false;
  }

//  int button0 = digitalRead(BUTTONS[0]);
//  int button1 = digitalRead(BUTTONS[1]);
//
//  if (button0 == 0) {
//    digitalWrite(DEVICES[1], LOW);
//  } else digitalWrite(DEVICES[1], HIGH);
//
//  if (button1 == 0) {
//    digitalWrite(DEVICES[3], LOW);
//  } else digitalWrite(DEVICES[3], HIGH);
}
