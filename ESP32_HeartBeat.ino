#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

MAX30105 particleSensor;

const byte RATE_SIZE = 4;  //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE];     //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0;  //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;
long irValue;
int DataisReady = 0;
float temperature;
float temperatureF;

enum State {
  IDLE,
  MEASURE_TEMPERATURE,
  MEASURE_BP
};

State currentState = IDLE;
unsigned long measurementStartTime;
unsigned long DataSendStartTime;

void initializeSensor() {
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 was not found. Please check wiring/power.");
    while (1)
      ;
  }

  particleSensor.setup();                     // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  // Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   // Turn off Green LED
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initializing...");
  //initializeSensor();
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 was not found. Please check wiring/power. ");
    while (1)
      ;
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");
  particleSensor.setup();                     // Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A);  // Turn Red LED to low to indicate the sensor is running
  particleSensor.setPulseAmplitudeGreen(0);   // Turn off Green LED
}

void loop() {
  switch (currentState) {
    case IDLE:
      checkFingerDetection();
      break;

    case MEASURE_TEMPERATURE:
      measureTemperature();
      break;

    case MEASURE_BP:
      measureBP();
      break;
  }
}
void checkFingerDetection() {
  irValue = particleSensor.getIR();

  if (irValue > 70000) {
    currentState = MEASURE_TEMPERATURE;
    DataisReady = 0;
    measurementStartTime = millis();
  }  // Add an additional condition to transition to MEASURE_TEMPERATURE
}
void measureTemperature() {
  particleSensor.setup(0);  // Configure sensor. Turn off LEDs
  particleSensor.enableDIETEMPRDY();
  temperature = particleSensor.readTemperature();
  Serial.print("temperatureC = ");
  Serial.print(temperature, 2);

  temperatureF = particleSensor.readTemperatureF();
  Serial.print("   temperatureF = ");
  Serial.print(temperatureF, 2);

  Serial.println();

  // Check if 10 seconds have passed
  if (millis() - measurementStartTime >= 10000) {
    // Before transitioning to MEASURE_BP, re-initialize the sensor settings
    particleSensor.setup();                     // Configure sensor with default settings
    particleSensor.setPulseAmplitudeRed(0x0A);  // Turn Red LED to low to indicate the sensor is running
    particleSensor.setPulseAmplitudeGreen(0);   // Turn off Green LED
    delay(1000);
    currentState = MEASURE_BP;
  }
}
void measureBP() {
  irValue = particleSensor.getIR();
  if (checkForBeat(irValue) == true) {
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute;  //Store this reading in the array
      rateSpot %= RATE_SIZE;                     //Wrap variable

      //Take average of readings
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }
  if (beatAvg > 65 && beatAvg < 80 && beatsPerMinute > 65 && DataisReady == 0) {
    // Send the BPM data to database
    Serial.println("Data is Ready");
    Serial.print("IR=");
    Serial.print(irValue);
    Serial.print(", BPM=");
    Serial.print(beatsPerMinute);
    Serial.print(", Avg BPM=");
    Serial.print(beatAvg);
    delay(1000);
    DataisReady = 1;
    DataSendStartTime = millis();
  }
  if (irValue < 5000) {
    Serial.println("No Finger Detected");
    if (DataisReady == 1) {
      unsigned long TimeRemaining = millis() - DataSendStartTime;
      Serial.print("TimeRemaining: ");
      Serial.println(TimeRemaining / 1000);
      if (TimeRemaining >= 10000) {
        currentState = IDLE;
        Serial.println("CurrentState Changed to IDLE");
      }
      
    }
  }
}
