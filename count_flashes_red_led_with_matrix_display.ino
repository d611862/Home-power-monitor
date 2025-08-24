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

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN   13
#define DATA_PIN  11
#define CS_PIN    10


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
char curMessage[BUF_SIZE] = { "POWER" };
char newMessage[BUF_SIZE] = { " " };
String reading;
MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);
//bool newMessageAvailable = true;

// the setup routine runs once when you press reset:
void setup() {

    P.begin(); 

    pinMode(LED_BUILTIN, OUTPUT);
    pinMode(A1, INPUT);
    pinMode(4, OUTPUT);
    delay(500);    
    String welcome = "I have the power !!";
    P.displayClear();
    P.displayScroll(welcome.c_str(), PA_LEFT, PA_SCROLL_LEFT, 100);
    while (!P.displayAnimate()) {
    }
    P.displayAnimate();
    P.displayText("ready", PA_RIGHT, 0, 0, PA_PRINT, PA_NO_EFFECT);
}

// the loop routine runs over and over again forever:
void loop() {

  // read the input on analog pin 1
  // take 10 readings and use the average
  analogRead(A1);
  sensorValue = 0;
  for (int i=0; i<10; i++) {
    sensorValue = sensorValue + analogRead(A1);
  }
  sensorValue = sensorValue / 10;
  
  // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5V):
  float currentVoltage = sensorValue * (5.0 / 1023.0);

  if (prevVoltage < 1.0 && currentVoltage > 2.5) {
    if (newCycle) { // this is to account for the starting pulse. 
      newCycle = false;
      startTime = millis();
    }
    else {
      positiveEdge++; // only count from the 2nd positive edge. when the 2nd positive edge is seen, that's one period.
      if (positiveEdge == 1) {
        timeToFirstEdge = millis() - startTime;
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
  /* else if (currentVoltage < 1.0 && prevVoltage > 2.5) {
    negativeEdge++;
  } */
  
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
    positiveEdge = 0;
    newCycle = true;
    //sendToDisplay();

    if (powerWatts < 1.0) {
      reading = String(powerWatts*1000,0) + " w";
    }
    else {
      reading = String(powerWatts,1)  + " kw";
    }
    strncpy(newMessage, reading.c_str(), BUF_SIZE - 1);
    newMessage[BUF_SIZE - 1] = '\0';  // Ensure null-termination
      //strcpy(curMessage, newMessage);
      //P.displayReset();

    P.displayAnimate();
    P.displayText(newMessage, PA_RIGHT, 0, 0, PA_PRINT, PA_NO_EFFECT);
  }

  //print out the voltage value you read for troubleshooting and checking pulse shape, pulse high/low voltages
  //Serial.println("Min:0,Max:5.1,Voltage:"+ String(currentVoltage));
  //delay(5);
}
