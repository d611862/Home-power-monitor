/*
  ReadAnalogVoltage

  Reads an analog input on pin 0, converts it to voltage, and prints the result to the Serial Monitor.
  Graphical representation is available using Serial Plotter (Tools > Serial Plotter menu).
  Attach the center pin of a potentiometer to pin A0, and the outside pins to +5V and ground.

  This example code is in the public domain.

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

//    pinMode(10, OUTPUT);
//    pinMode(11, OUTPUT);
//    pinMode(13, OUTPUT);
  // initialize serial communication at 9600 bits per second:
   // Serial.begin(115200);
//    Wire.begin();
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
    //P.displayReset();
    //delay(2000);   

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
/*  if (negativeEdge >= 10) {
    //Serial.println("10 -ve edges");
    negativeEdge = 0;
  }  
*/

/*  if (counter > 400) {
    digitalWrite(4, HIGH);
    counter = 0;  // turn the LED on (HIGH is the voltage level)
  } else if (counter == 30 ) {                    // wait for a second
    digitalWrite(4, LOW);   // turn the LED off by making the voltage LOW
  }
  
  counter++;
*/
  // print out the value you read:
  //Serial.println("Min:0,Max:5.1,Voltage:"+ String(currentVoltage));
  //delay(5);
}

void sendToDisplay() {

}
