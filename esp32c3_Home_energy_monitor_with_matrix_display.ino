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
char pulseSpacingForWebpage[BUF_SIZE] = { " " };
char pulseDurationForWebpage[BUF_SIZE] = { " " };

String reading;
//MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);
//bool newMessageAvailable = true;
int minr = 0;
int maxr = 4095;
WebServer server(80);

// Timezone: Australia/Melbourne (UTC+10 or +11 with DST)
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 36000;  // 10 hours
//const int daylightOffset_sec = 3600;  // 1 hour for DST
const int daylightOffset_sec = 0;



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
        servePowerPage(powerForWebpage, pulseSpacingForWebpage, pulseDurationForWebpage);
      });

      server.begin();
      Serial.println("Web server started");

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
  server.handleClient();
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


void servePowerPage(const char* powerForWebpage, const char* pulseSpacingForWebpage, const char* pulseDurationForWebpage) {
  String html = "<!DOCTYPE html><html><head><meta http-equiv='refresh' content='2'>";
  html += "<title>Power Monitor</title></head><body>";
  html += "<h2>Power Reading:</h2><p style='font-size:24px;'>";
  html += String(powerForWebpage);
  html += "</p><h3>Pulse spacing:</h3><p style='font-size:20px;'>";  
  html += String(pulseSpacingForWebpage);  
  html += "</p><h3>Pulse duration:</h3><p style='font-size:20px;'>";  
  html += String(pulseDurationForWebpage);    
  html += "</p><h3>Timestamp:</h3><p style='font-size:20px;'>";
  html += getTimestamp();  // Assumes you have this function from earlier
  html += "</p></body></html>";
  server.send(200, "text/html", html);
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

