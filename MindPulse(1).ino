//MINDPULSE FINAL CODE BY JES (CYBERTHORN)
// MindPulse is context-aware: Button choses bw/ Modes [EXAM--LECTURE--GROUP]
//
//DHT(T&H)--> if uncomfortable: FAN ON
//LDR --> WHITE LIGHT LED 
//KY-038(sound)--> contributes to SI
//HC-SR501(PIR)-->MOTION/NO MOTION Display (lcd)
//MQ-135(gases: NH4,CO2)--> FAN ON & contributes to CLI
//SI/CLI (Stress / Cognitive Load Index)--> G-Y-R LED + buzzer [0-50-75-100]
//
//SI = 0.6(noise risk)+0.25(comfort risk) + 0.15(light risk)
//CLI = 0.6(air risk)+0.25(comfort risk) + 0.15(noise risk)
// rise = raw - base; risk =map(rise,low,high,0-100);
//adaptive baseline : base(new)=(1-a)*Base(old)+ a*Current
//prediction: rate= current-previous /t; timerisk = critical threshold - current index/ rate;
//Testing? button(modes);light; motion(display);sound(clap),dht(fan)

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MINDPULSE - CyberThorn
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// ---------- PINS ----------
#define DHTPIN 2
#define DHTTYPE DHT22

const byte PIR_PIN    = 3;
const byte GREEN_LED  = 4;
const byte YELLOW_LED = 5;
const byte RED_LED    = 6;
const byte BUTTON_PIN = 7;
const byte BUZZER_PIN = 8;
const byte RELAY_PIN  = 9;
const byte LIGHT_LED  = 10;

const byte LDR_PIN   = A0;
const byte SOUND_PIN = A1;
const byte AIR_PIN   = A2;

// !!! Change if LCD has diff add
LiquidCrystal_I2C lcd(0x27, 16, 2);
DHT dht(DHTPIN, DHTTYPE);


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TUNING CONSTANTS
// !!! TEST THESE ON YOUR REAL HARDWARE
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// LDR
// Lower ADC = darker with our wiring
const int LDR_DARK = 400;            // !!! TUNE
const int LDR_BRIGHT = 700;          // !!! TUNE
const int LIGHT_LOW_THRESHOLD = 450; // !!! TUNE

// Sound rise ABOVE learned baseline
const int NOISE_LOW = 10;            // !!! TUNE
const int NOISE_HIGH = 150;          // !!! TUNE

// HIGH sound classification changes by mode
const int SOUND_HIGH_LECTURE = 60;   // !!! TUNE
const int SOUND_HIGH_GROUP   = 100;  // !!! TUNE
const int SOUND_HIGH_EXAM    = 30;   // !!! TUNE

// MQ135 rise ABOVE learned baseline
const int AIR_LOW = 20;              // !!! TUNE
const int AIR_HIGH = 200;            // !!! TUNE
const int AIR_ALERT = 30;            // !!! TUNE

// Temperature / humidity
const float TEMP_LOW = 18;           // !!! TUNE
const float TEMP_HIGH = 30;          // !!! TUNE

const float HUM_LOW = 30;            // !!! TUNE
const float HUM_HIGH = 65;           // !!! TUNE

// SI / CLI status
const int WARNING_LEVEL = 50;        // !!! TUNE
const int CRITICAL_LEVEL = 75;       // !!! TUNE

// Adaptive baseline
const float EMA_ALPHA = 0.02;        // !!! TUNE carefully

// Prediction
const unsigned long PREDICT_INTERVAL = 30000;
const float MIN_TREND_RATE = 0.5;    // !!! TUNE


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// VARIABLES
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

enum Mode { LECTURE, GROUP, EXAM };
Mode mode = LECTURE;

float temp = 0;
float humidity = 0;

int ldrRaw = 0;
int airRaw = 0;

int soundAmplitude = 0;

float soundBase = 0;
float airBase = 0;

float noiseRisk = 0;
float airRisk = 0;
float lightRisk = 0;
float comfortRisk = 0;

float SI = 0;
float CLI = 0;

bool motionDetected = false;
bool soundHigh = false;
bool airHigh = false;
bool lightLow = false;
bool comfortBad = false;

bool fanOn = false;

float previousSI = 0;
float previousCLI = 0;
float timeToRisk = 999;

unsigned long lastRead = 0;
unsigned long lastLCD = 0;
unsigned long lastPrediction = 0;

bool lastButton = HIGH;
unsigned long buttonPressedAt = 0;

byte lcdPage = 0;


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// HELPER FUNCTIONS
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

float limit100(float value) {
  return constrain(value, 0, 100);
}


float scaleRisk(float value, float low, float high) {

  if (value <= low) return 0;
  if (value >= high) return 100;

  return (value - low) * 100.0 / (high - low);
}


const char* modeName() {

  if (mode == LECTURE) return "LECTURE";
  if (mode == GROUP) return "GROUP";

  return "EXAM";
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// SOUND READING
// Peak-to-peak amplitude over 100 ms
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int readSoundAmplitude() {

  int minimum = 1023;
  int maximum = 0;

  unsigned long start = millis();

  while (millis() - start < 100) {

    int value = analogRead(SOUND_PIN);

    if (value < minimum) minimum = value;
    if (value > maximum) maximum = value;
  }

  return maximum - minimum;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// STARTUP CALIBRATION
// Learns normal sound + air baseline
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void calibrate() {

  lcd.clear();
  lcd.print("Calibrating...");
  lcd.setCursor(0, 1);
  lcd.print("Keep room normal");

  soundBase = 0;
  airBase = 0;

  for (int i = 0; i < 20; i++) {

    soundBase += readSoundAmplitude();
    airBase += analogRead(AIR_PIN);

    delay(200);
  }

  soundBase /= 20.0;
  airBase /= 20.0;

  lcd.clear();
  lcd.print("MINDPULSE READY");

  delay(1000);
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MODE BUTTON
// LECTURE -> GROUP -> EXAM
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void checkButton() {

  bool button = digitalRead(BUTTON_PIN);

  if (button == LOW && lastButton == HIGH) {
    buttonPressedAt = millis();
  }

  if (button == HIGH && lastButton == LOW) {

    // Debounce
    if (millis() - buttonPressedAt > 50) {

      if (mode == LECTURE)
        mode = GROUP;

      else if (mode == GROUP)
        mode = EXAM;

      else
        mode = LECTURE;

      // Show mode immediately
      lcd.clear();
      lcd.print("MODE CHANGED:");
      lcd.setCursor(0, 1);
      lcd.print(modeName());

      delay(500);
    }
  }

  lastButton = button;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// READ ALL SENSORS
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void readSensors() {

  // ---------- DHT22 ----------

  temp = dht.readTemperature();
  humidity = dht.readHumidity();


  // ---------- LDR ----------

  ldrRaw = analogRead(LDR_PIN);

  lightLow = (ldrRaw < LIGHT_LOW_THRESHOLD);

  lightRisk =
    100 - scaleRisk(ldrRaw, LDR_DARK, LDR_BRIGHT);


  // ---------- PIR ----------

  // Display information only
  motionDetected = digitalRead(PIR_PIN);


  // ---------- SOUND ----------

  soundAmplitude = readSoundAmplitude();

  float soundRise = soundAmplitude - soundBase;

  if (soundRise < 0)
    soundRise = 0;

  noiseRisk =
    scaleRisk(soundRise, NOISE_LOW, NOISE_HIGH);


  // Mode-specific HIGH sound threshold

  int soundThreshold;

  if (mode == LECTURE)
    soundThreshold = SOUND_HIGH_LECTURE;

  else if (mode == GROUP)
    soundThreshold = SOUND_HIGH_GROUP;

  else
    soundThreshold = SOUND_HIGH_EXAM;


  soundHigh = (soundRise >= soundThreshold);


  // Slowly adapt sound baseline
  // only during relatively normal sound

  if (!soundHigh) {

    soundBase =
      (1 - EMA_ALPHA) * soundBase +
      EMA_ALPHA * soundAmplitude;
  }


  // ---------- MQ135 ----------

  airRaw = analogRead(AIR_PIN);

  float airRise = airRaw - airBase;

  if (airRise < 0)
    airRise = 0;

  airRisk =
    scaleRisk(airRise, AIR_LOW, AIR_HIGH);

  airHigh = (airRise >= AIR_ALERT);


  // Slowly adapt air baseline
  // only when air is considered normal

  if (!airHigh) {

    airBase =
      (1 - EMA_ALPHA) * airBase +
      EMA_ALPHA * airRaw;
  }


  // ---------- COMFORT ----------

  comfortRisk = 0;

  if (!isnan(temp)) {

    if (temp < TEMP_LOW)
      comfortRisk += (TEMP_LOW - temp) * 5;

    else if (temp > TEMP_HIGH)
      comfortRisk += (temp - TEMP_HIGH) * 5;
  }


  if (!isnan(humidity)) {

    if (humidity < HUM_LOW)
      comfortRisk += (HUM_LOW - humidity) * 2;

    else if (humidity > HUM_HIGH)
      comfortRisk += (humidity - HUM_HIGH) * 2;
  }


  comfortRisk = limit100(comfortRisk);

  comfortBad =
    temp < TEMP_LOW ||
    temp > TEMP_HIGH ||
    humidity < HUM_LOW ||
    humidity > HUM_HIGH;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// SI + CLI
//
// SI  = 60% Noise
//       25% Comfort
//       15% Lighting
//
// CLI = 60% Air
//       25% Comfort
//       15% Noise
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void calculateIndices() {

  SI =
      0.60 * noiseRisk
    + 0.25 * comfortRisk
    + 0.15 * lightRisk;


  CLI =
      0.60 * airRisk
    + 0.25 * comfortRisk
    + 0.15 * noiseRisk;


  SI = limit100(SI);
  CLI = limit100(CLI);
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// TREND PREDICTION
//
// Rate = change in index / change in time
//
// Time To Risk =
// (Critical Level - Current Index) / Rate
//
// Prediction is DISPLAY ONLY.
// It does NOT control LEDs/buzzer.
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void predictRisk() {

  if (millis() - lastPrediction < PREDICT_INTERVAL)
    return;


  float minutes =
    (millis() - lastPrediction) / 60000.0;


  float siRate =
    (SI - previousSI) / minutes;

  float cliRate =
    (CLI - previousCLI) / minutes;


  previousSI = SI;
  previousCLI = CLI;

  lastPrediction = millis();

  timeToRisk = 999;


  // SI prediction

  if (siRate > MIN_TREND_RATE &&
      SI < CRITICAL_LEVEL) {

    float t =
      (CRITICAL_LEVEL - SI) / siRate;

    if (t > 0)
      timeToRisk = t;
  }


  // CLI prediction

  if (cliRate > MIN_TREND_RATE &&
      CLI < CRITICAL_LEVEL) {

    float t =
      (CRITICAL_LEVEL - CLI) / cliRate;

    if (t > 0 && t < timeToRisk)
      timeToRisk = t;
  }
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// OUTPUTS
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void updateOutputs() {

  // ---------- WHITE LIGHT ----------

  // Dark -> light ON
  digitalWrite(LIGHT_LED,
               lightLow ? HIGH : LOW);


  // ---------- FAN ----------

  // Fan ON if:
  // 1. Temperature too high
  // 2. Humidity too high
  // 3. Air/gas level high

  fanOn =
      temp > TEMP_HIGH
      || humidity > HUM_HIGH
      || airHigh;


  // !!! Relay currently assumed ACTIVE LOW
  digitalWrite(RELAY_PIN,
               fanOn ? HIGH : LOW);


  // ---------- STATUS ----------

  bool critical =
      SI >= CRITICAL_LEVEL ||
      CLI >= CRITICAL_LEVEL;


  bool warning =
      !critical &&
      (SI >= WARNING_LEVEL ||
       CLI >= WARNING_LEVEL);


  // GREEN = Normal

  digitalWrite(GREEN_LED,
               !warning && !critical);


  // YELLOW = Warning

  digitalWrite(YELLOW_LED,
               warning);


  // RED = Critical

  digitalWrite(RED_LED,
               critical);


  // ---------- BUZZER ----------

  if (critical) {

    // Continuous alarm
    digitalWrite(BUZZER_PIN, HIGH);

  }

  else if (warning) {

    // Intermittent beep
    // 500 ms ON / 500 ms OFF

    if ((millis() / 500) % 2 == 0)
      digitalWrite(BUZZER_PIN, HIGH);

    else
      digitalWrite(BUZZER_PIN, LOW);

  }

  else {

    digitalWrite(BUZZER_PIN, LOW);
  }
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LCD
// 5 pages - changes every 2 seconds
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void updateLCD() {

  lcd.clear();


  // ---------- PAGE 1 ----------
  // SI + CLI + overall status

  if (lcdPage == 0) {

    lcd.setCursor(0, 0);

    lcd.print("SI:");
    lcd.print((int)SI);

    lcd.print(" CLI:");
    lcd.print((int)CLI);


    lcd.setCursor(0, 1);

    if (SI >= CRITICAL_LEVEL ||
        CLI >= CRITICAL_LEVEL)

      lcd.print("STATUS: CRITICAL");


    else if (SI >= WARNING_LEVEL ||
             CLI >= WARNING_LEVEL)

      lcd.print("STATUS: WARNING");


    else

      lcd.print("STATUS: NORMAL");
  }


  // ---------- PAGE 2 ----------
  // Temperature + humidity

  else if (lcdPage == 1) {

    lcd.setCursor(0, 0);

    lcd.print("T:");
    lcd.print(temp, 1);
    lcd.print("C ");

    lcd.print("H:");
    lcd.print((int)humidity);
    lcd.print("%");


    lcd.setCursor(0, 1);

    if (comfortBad)
      lcd.print("Comfort: ALERT");

    else
      lcd.print("Comfort: NORMAL");
  }


  // ---------- PAGE 3 ----------
  // Sound + motion

  else if (lcdPage == 2) {

    lcd.setCursor(0, 0);

    if (soundHigh)
      lcd.print("Sound: HIGH");

    else
      lcd.print("Sound: NORMAL");


    lcd.setCursor(0, 1);

    if (motionDetected)
      lcd.print("MOTION");

    else
      lcd.print("NO MOTION");
  }


  // ---------- PAGE 4 ----------
  // Air + lighting

  else if (lcdPage == 3) {

    lcd.setCursor(0, 0);

    if (airHigh)
      lcd.print("Air: HIGH");

    else
      lcd.print("Air: NORMAL");


    lcd.setCursor(0, 1);

    if (lightLow)
      lcd.print("Light: LOW");

    else
      lcd.print("Light: OK");
  }


  // ---------- PAGE 5 ----------
  // Mode + prediction

  else {

    lcd.setCursor(0, 0);

    lcd.print("Mode:");
    lcd.print(modeName());


    lcd.setCursor(0, 1);

    if (timeToRisk < 999) {

      lcd.print("Risk ~");
      lcd.print(timeToRisk, 1);
      lcd.print(" min");

    }

    else {

      lcd.print("Trend: STABLE");
    }
  }


  lcdPage++;

  if (lcdPage > 4)
    lcdPage = 0;
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// SERIAL MONITOR
// Detailed values for testing/debugging
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void debugSerial() {

  Serial.print("Mode=");
  Serial.print(modeName());


  Serial.print(" | T=");
  Serial.print(temp);

  Serial.print(" H=");
  Serial.print(humidity);

  Serial.print(" LDR=");
  Serial.print(ldrRaw);


  Serial.print(" | SoundAmp=");
  Serial.print(soundAmplitude);

  Serial.print(" SoundBase=");
  Serial.print(soundBase);

  Serial.print(" Sound=");
  Serial.print(soundHigh ? "HIGH" : "NORMAL");


  Serial.print(" | AirRaw=");
  Serial.print(airRaw);

  Serial.print(" AirBase=");
  Serial.print(airBase);

  Serial.print(" Air=");
  Serial.print(airHigh ? "HIGH" : "NORMAL");


  Serial.print(" | Motion=");
  Serial.print(motionDetected ? "YES" : "NO");


  Serial.print(" | Risks N=");
  Serial.print(noiseRisk);

  Serial.print(" A=");
  Serial.print(airRisk);

  Serial.print(" L=");
  Serial.print(lightRisk);

  Serial.print(" C=");
  Serial.print(comfortRisk);


  Serial.print(" | SI=");
  Serial.print(SI);

  Serial.print(" CLI=");
  Serial.print(CLI);


  Serial.print(" | Fan=");
  Serial.print(fanOn ? "ON" : "OFF");


  Serial.print(" Light=");
  Serial.print(lightLow ? "ON" : "OFF");


  Serial.print(" | Prediction=");

  if (timeToRisk < 999) {

    Serial.print(timeToRisk);
    Serial.println(" min");

  }

  else {

    Serial.println("STABLE");
  }
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// SETUP
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void setup() {

  Serial.begin(9600);


  pinMode(PIR_PIN, INPUT);

  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LIGHT_LED, OUTPUT);


  // Everything safe/off initially

  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LIGHT_LED, LOW);

  // !!! Assumes ACTIVE LOW relay
  digitalWrite(RELAY_PIN, LOW);


  dht.begin();

  lcd.init();
  lcd.backlight();


  calibrate();


  previousSI = 0;
  previousCLI = 0;

  lastPrediction = millis();
}


// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// MAIN LOOP
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

void loop() {

  checkButton();


  // Read/process once per second

  if (millis() - lastRead >= 1000) {

    lastRead = millis();


    readSensors();

    calculateIndices();

    predictRisk();

    updateOutputs();

    debugSerial();
  }


  // Alternate LCD page every 2 seconds

  if (millis() - lastLCD >= 2000) {

    lastLCD = millis();

    updateLCD();
  }


  // Keep buzzer blinking correctly during WARNING
  // instead of waiting for next sensor reading

  bool warning =
      (SI >= WARNING_LEVEL ||
       CLI >= WARNING_LEVEL)
      &&
      SI < CRITICAL_LEVEL
      &&
      CLI < CRITICAL_LEVEL;


  if (warning) {

    digitalWrite(
      BUZZER_PIN,
      ((millis() / 500) % 2 == 0)
    );
  }
}
