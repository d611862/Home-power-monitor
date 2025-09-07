/*
 Home Power monitor that counts red led flashes on Electricity meter and determines real time power usage in W or kW
 Uses a Light Sensitive Resistor that conencts via atatchment to the electricity meter.
 Arduino nano counts pulses and determines average usage over N pulses.
 This particular meter uses 3200 pulses per kWh.

  Reads an analog input on pin 1, converts it to voltage between 0v and 5v.

  Writes power usage to 8x32 MAX 7219 LED matrix. Refresh rate of ~1.5s for all power usage scenarios.

  Analog read is based on:
  https://docs.arduino.cc/built-in-examples/basics/ReadAnalogVoltage/
*/
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
//#include <Wire.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
//#include <ESPAsyncWebServer.h>
//#include <AsyncTCP.h>
#include <WebSocketsServer.h>

WebSocketsServer webSocket = WebSocketsServer(81);  // Port 81 for WebSocket



const char* ssid = "Telstra1002";
const char* password = "9350156141";
//const char* serverName = "http://192.168.1.27/post";

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
//#define CLK_PIN   13
//#define DATA_PIN  11
//#define CS_PIN    10
#define DATA_PIN 8   // DIN
#define CLK_PIN  4   // CLK
#define CS_PIN   9  // CS
#define TRIGGER_PIN 2  // GPIO2




#define	BUF_SIZE 10

int counter = 0;
int sensorValue = 0;
float prevVoltage = 1.5;
float currentVoltage = 1.5;
int startTime = 0;
int duration = 0;
int negativeEdge = 0;
int positiveEdge = 0;
int edgesTarget = 5;
int timeToFirstEdge = 0;
bool pulseHigh = false;
const int pulsesPerKWh = 3200; 
bool newCycle = true;
float powerWatts = 0.0;
int pulseDuration = 0;
long pulseStartUs = 0;
char curMessage[BUF_SIZE] = { "POWER" };
char newMessage[BUF_SIZE] = { " " };
char powerForWebpage[BUF_SIZE] = { " " };
int powerForJSON = 0;
char pulseSpacingForWebpage[BUF_SIZE] = { " " };
int pulseSpacingForJSON = 0;
char pulseDurationForWebpage[BUF_SIZE] = { " " };
int pulseDurationForJSON = 0;

unsigned long previousWifiMillis = 0;
unsigned long currentWifiMillis = 0;
const int wifiCheckInterval = 60000;

String reading;
//MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
//bool newMessageAvailable = true;
int minr = 0;
int maxr = 4095;
//WebServer server(80);
//AsyncWebServer server(80);
//AsyncWebSocket ws("/ws");
WebServer server(80);  // HTTP server


// Timezone: Australia/Melbourne (UTC+10 or +11 with DST)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 36000;  // 10 hours
//const int daylightOffset_sec = 3600;  // 1 hour for DST
const int daylightOffset_sec = 0;

struct PowerReading {
  String timestamp;
  int power;
};

const int HISTORY_SIZE = 60;
PowerReading history[HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;




// the setup routine runs once when you press reset:
void setup() {
  Serial.begin(115200);
    P.begin(); 

    //pinMode(LED_BUILTIN, OUTPUT);
    pinMode(1, INPUT);
    //pinMode(4, OUTPUT);
    delay(500);    
    String welcome = "I have the power !!";
    P.displayClear();
    P.displayScroll(welcome.c_str(), PA_LEFT, PA_SCROLL_LEFT, 100);
    while (!P.displayAnimate()) {
    }
    P.displayClear();
    P.displayText("ready", PA_RIGHT, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayAnimate();
    analogSetAttenuation(ADC_11db);
    previousWifiMillis = millis();
    pinMode(TRIGGER_PIN, INPUT);

    if (digitalRead(TRIGGER_PIN) == HIGH) { 
      Serial.println("\nWiFi trigger HIGH");
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      Serial.println("\nConnected to WiFi");
      Serial.print("Sender IP Address: ");
      Serial.println(WiFi.localIP()); 

      server.on("/", []() {
        server.send(200, "text/plain", "Try /power for readings");
      });
      server.on("/power", []() {
        String html = servePowerPage(powerForWebpage, pulseSpacingForWebpage, pulseDurationForWebpage);
        server.send(200, "text/html", html);
      });
      server.on("/api/history", HTTP_GET, []() {
        String json = "[";
        for (int i = 0; i < historyCount; i++) {
          int index = (historyIndex + i) % HISTORY_SIZE;
          json += "{";
          json += "\"timestamp\":\"" + history[index].timestamp + "\",";
          json += "\"power\":" + String(history[index].power);
          json += "}";
          if (i < historyCount - 1) json += ",";
        }
        json += "]";
        server.send(200, "application/json", json);
      });



/*      ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
                AwsEventType type, void *arg, uint8_t *data, size_t len) {
        if (type == WS_EVT_CONNECT) {
          Serial.println("Client connected");
    String payload = "{";
    payload += "\"power\":" + String(powerForWebpage) + ",";
    payload += "\"spacing\":" + String(pulseSpacingForWebpage) + ",";
    payload += "\"duration\":" + String(pulseDurationForWebpage) + ",";
    payload += "\"timestamp\":\"" + getTimestamp() + "\"";
    payload += "}";
    client->text(payload);  // Send to just this client
        }
      });

      server.addHandler(&ws);

      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Try /power for readings");
      });

      server.on("/power", HTTP_GET, [](AsyncWebServerRequest *request){
        String html = servePowerPage(powerForWebpage, pulseSpacingForWebpage, pulseDurationForWebpage);
        request->send(200, "text/html", html);
      });
*/
      server.begin();
      Serial.println("Web server started");
      webSocket.begin();
      webSocket.onEvent(webSocketEvent);

      setupTime();  // ⏰ Sync NTP time
    }
    else {
      Serial.println("\nWiFi trigger LOW");
    }
}

// the loop routine runs over and over again forever:
void loop() {

  // read the input on analog pin 1
  // take 10 readings and use the average
  server.handleClient(); // not needed for Async web server
  webSocket.loop();
  analogRead(1);
  sensorValue = 0;
  for (int i=0; i<10; i++) {
    sensorValue = sensorValue + analogRead(1);
  }
  sensorValue = sensorValue / 10;
  //Serial.println(String(minr) + ", " + String(maxr) + ", " + String(sensorValue));
  
  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  //float currentVoltage = sensorValue * (5.0 / 1023.0);
  float currentVoltage = sensorValue * (3.4 / 3500.0);

  if (prevVoltage < 1.0 && currentVoltage > 2.5) {
    if (newCycle) { // this is to account for the starting pulse. 
      newCycle = false;
      startTime = millis();
    }
    else {
      positiveEdge++; // only count from the 2nd positive edge. when the 2nd positive edge is seen, that's one period.
      if (positiveEdge == 1) {
        timeToFirstEdge = millis() - startTime;
        pulseStartUs = micros();
        // when power usage is low time gap between flashes increases. Average over a fewer pulses to maintain a reasonably high refresh interval
        // when power usage is high time gap between pulses is low. Average over more flashes to maintain accuracy and minimise error.
        // target number of positive edges to detect is 4 - this is for high power usage
        // for medium usage => 3
        // for low usage => 2
        // use time duration between pulse 0 and pulse 1 as an indicator of usage and set target pulse count for that measurement cycle
        edgesTarget = 4;
        if (timeToFirstEdge > 500) {
          edgesTarget = 2;
        }
        else if (timeToFirstEdge > 300) {
          edgesTarget = 3; 
        }
      }
    }
  }
  else if (positiveEdge == 1 && currentVoltage < 1.0 && prevVoltage > 2.5) {
    //pulseDuration = millis() - startTime - timeToFirstEdge;
    pulseDuration = micros() - pulseStartUs;
  }
  
  // ignore any readings between 1.0v and 2.5v because these are transient voltages
  // if 1.0v < voltage < 2.5v then do nothing 
  if (!(currentVoltage > 1.0 && currentVoltage < 2.5)) {
    prevVoltage = currentVoltage;
  }

  
  if (positiveEdge >= edgesTarget) {
    duration = millis() - startTime;
    float energy_kWh = 1.0 * edgesTarget / pulsesPerKWh;
    powerWatts = (energy_kWh * 3600000.0) / duration;
    //Serial.println("duration: " + String(duration) + ", power kW: " + String(powerWatts));

    // for display in html page. 
    reading = String(pulseDuration/1000.0,1) + " ms";
    strncpy(pulseDurationForWebpage, reading.c_str(), BUF_SIZE - 1);
    pulseDurationForWebpage[BUF_SIZE - 1] = '\0';  // Ensure null-termination   

    // for display in html page. 
    reading = String(1.0 * duration / positiveEdge, 0) + " ms";
    strncpy(pulseSpacingForWebpage, reading.c_str(), BUF_SIZE - 1);
    pulseSpacingForWebpage[BUF_SIZE - 1] = '\0';  // Ensure null-termination   

    // for display in html page. use W instead of kW
    reading = String(powerWatts*1000,0) + " w";
    strncpy(powerForWebpage, reading.c_str(), BUF_SIZE - 1);
    powerForWebpage[BUF_SIZE - 1] = '\0';  // Ensure null-termination

    powerForJSON = atoi(powerForWebpage);
    pulseSpacingForJSON = atoi(pulseSpacingForWebpage);
    pulseDurationForJSON = atoi(pulseDurationForWebpage);


    positiveEdge = 0;
    newCycle = true;
    //sendToDisplay();

    if (powerWatts < 1.0) {
      reading = String(powerWatts*1000,0) + " w";
    }
    else if (powerWatts > 10.0) {
      reading = "HIGH";
    }
    else {
      reading = String(powerWatts,1)  + " kw";
    }
    strncpy(newMessage, reading.c_str(), BUF_SIZE - 1);
    newMessage[BUF_SIZE - 1] = '\0';  // Ensure null-termination

    P.displayAnimate();
    P.displayText(newMessage, PA_RIGHT, 0, 0, PA_PRINT, PA_NO_EFFECT);
    logReadingToHistory(powerForJSON);
    // Push update to any connected clients
    if (webSocket.connectedClients() > 0) { 
      notifyClients();
    }
    

    
    currentWifiMillis = millis();
    if (currentWifiMillis - previousWifiMillis > wifiCheckInterval) {
      Serial.println("Periodic WiFi check");
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected. Attempting to reconnect...");
        WiFi.disconnect();
        WiFi.begin(ssid, password);
      }
      previousWifiMillis = millis();
    }




    ///////
/*  if (digitalRead(TRIGGER_PIN) == HIGH) { 
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "text/plain");
      http.setTimeout(1000);
      int httpResponseCode = http.POST(newMessage);
      Serial.print("POST response: ");
      Serial.println(httpResponseCode);
      http.end();
    } else {
      Serial.println("WiFi disconnected");
    }    
    ///////
  } */
  }
  //print out the voltage value you read for troubleshooting and checking pulse shape, pulse high/low voltages
  //Serial.println("Min:0,Max:5.1,Voltage:"+ String(currentVoltage));
  //delay(5);
}


String servePowerPage(const char* powerForWebpage, const char* pulseSpacingForWebpage, const char* pulseDurationForWebpage) {
  //String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='10'>";
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Power Monitor</title><script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script></head><body>";

  html += "<h2>Power Reading:</h2><p id='power' style='font-size:24px;'>";
  html += String(powerForWebpage);
  html += " W</p>";

  html += "<h3>Pulse spacing:</h3><p id='spacing' style='font-size:20px;'>";
  html += String(pulseSpacingForWebpage);
  html += " ms</p>";

  html += "<h3>Pulse duration:</h3><p id='duration' style='font-size:20px;'>";
  html += String(pulseDurationForWebpage);
  html += " ms</p>";

  html += "<h3>Timestamp:</h3><p id='timestamp' style='font-size:20px;'>";
  html += getTimestamp();
  html += "</p>";

html += "<canvas id=\"powerChart\" width=\"800\" height=\"400\"></canvas>";
html += "<script>";
html += "let powerChart;";
html += "let chartReady = false;";
html += "let pendingMessages = [];";

// Chart initialization
html += "window.onload = function() {";
html += "  fetch('/api/history')";
html += "    .then(res => res.json())";
html += "    .then(data => {";
html += "      const timestamps = data.map(d => d.timestamp);";
html += "      const powerValues = data.map(d => d.power);";
html += "      const ctx = document.getElementById('powerChart').getContext('2d');";
html += "      powerChart = new Chart(ctx, {";
html += "        type: 'line',";
html += "        data: {";
html += "          labels: timestamps,";
html += "          datasets: [{";
html += "            label: 'Power (W)',";
html += "            data: powerValues,";
html += "            borderColor: 'blue',";
html += "            fill: false,";
html += "            tension: 0.1";
html += "          }]";
html += "        },";
html += "        options: {";
html += "          scales: {";
html += "            x: { title: { display: true, text: 'Time' }, ticks: { autoSkip: true, maxTicksLimit: 20 } },";
html += "            y: { title: { display: true, text: 'Power (W)' } }";
html += "          }";
html += "        }";
html += "      });";
html += "      chartReady = true;";
html += "      pendingMessages.forEach(updateChart);";
html += "      pendingMessages = [];";
html += "    });";
html += "};";

// WebSocket setup
html += "const ws = new WebSocket('ws://' + location.hostname + ':81');";
html += "ws.onmessage = function(event) {";
html += "  try {";
html += "    const data = JSON.parse(event.data);";
html += "    document.getElementById('power').textContent = data.power + ' W';";
html += "    document.getElementById('spacing').textContent = data.spacing + ' ms';";
html += "    document.getElementById('duration').textContent = data.duration + ' ms';";
html += "    document.getElementById('timestamp').textContent = data.timestamp;";
html += "    if (!chartReady) { pendingMessages.push(data); } else { updateChart(data); }";
html += "  } catch (e) {";
html += "    console.error('Error in WebSocket handler:', e);";
html += "    console.error('Raw message:', event.data);";
html += "  }";
html += "};";

// Chart update function
html += "function updateChart(data) {";
html += "  powerChart.data.labels.push(data.timestamp);";
html += "  powerChart.data.datasets[0].data.push(data.power);";
html += "  if (powerChart.data.labels.length > 60) {";
html += "    powerChart.data.labels.shift();";
html += "    powerChart.data.datasets[0].data.shift();";
html += "  }";
html += "  powerChart.update();";
html += "}";
html += "</script>";

  html += "</body></html>";
  return html;
}

void setupTime() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "Time not available";
  char buffer[64];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

/*
void notifyClients() {
String payload = "{";
payload += "\"power\":" + String(powerForWebpage) + ",";
payload += "\"spacing\":" + String(pulseSpacingForWebpage) + ",";
payload += "\"duration\":" + String(pulseDurationForWebpage) + ",";
payload += "\"timestamp\":\"" + getTimestamp() + "\"";
payload += "}";
ws.textAll(payload);  // Broadcast to all clients
}
*/

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.println("Client connected");
    String json = "{";
    json += "\"power\":" + String(powerForJSON) + ",";
    json += "\"spacing\":" + String(pulseSpacingForJSON) + ",";
    json += "\"duration\":" + String(pulseDurationForJSON) + ",";
    json += "\"timestamp\":\"" + getTimestamp() + "\"";
    json += "}";
    webSocket.sendTXT(num, json);
  }
}

void notifyClients() {
  Serial.println("Notifying clients");
  String json = "{";
  json += "\"power\":" + String(powerForJSON) + ",";
  json += "\"spacing\":" + String(pulseSpacingForJSON) + ",";
  json += "\"duration\":" + String(pulseDurationForJSON) + ",";
  json += "\"timestamp\":\"" + getTimestamp() + "\"";
  json += "}";
  webSocket.broadcastTXT(json);  // Broadcast to all clients
}

void logReadingToHistory(int powerValue) {
  history[historyIndex].timestamp = getTimestamp();  // Your existing timestamp function
  history[historyIndex].power = powerValue;
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  if (historyCount < HISTORY_SIZE) historyCount++;
}


