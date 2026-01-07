#include <Arduino.h>
#include <max6675.h>
#include <queue>

/// Pin definitions
// RGB LED pins
#define RED_PIN 27
#define GREEN_PIN 13
#define BLUE_PIN 33

// Potentiometer pins
#define BRIGHTNESS_PIN 35
#define POT_2 34

// Thermocouple pins
#define THERMO_DO 23
#define THERMO_CS 19
#define THERMO_CLK 5

// Button pins
#define SWITCH_1 4
#define SWITCH_2 RX2
#define SWITCH_3 TX2

// If your switches pull the input LOW when active, set this to true.
// Otherwise leave as false for active-HIGH switches.
#define SWITCH_ACTIVE_LOW true

// Create MAX6675 thermocouple instance
MAX6675 thermocouple(THERMO_CLK, THERMO_CS, THERMO_DO);

// put function declarations here:
void setColorPWM(byte r, byte g, byte b);
void fadeToColor(byte r1, byte g1, byte b1, byte r2, byte g2, byte b2);
float getTemp();
void setColorPWMFromTemp(float maxTemp, float minTemp);
void blinkChristmas(int cycles, int onMs, int offMs);
void setColorPWMFromTempWithBlink(float maxTemp, float minTemp);

const int fadeTime = 1000;
const int steps = 100;
const int delayTime = fadeTime / steps;

std::queue<float> lastTemps;

// Timer Variables
/*
array of floats
temperature threshold for turn off LED
if last high temperature was recorded a certain amount of time ago, then turn off LEDs


*/



/* PARAMETER CONFIGURATION */
const float maxTemp = 150.0; // Maximum temperature for scaling
const float minTemp = 250.0; // Minimum temperature for scaling

byte mode = 0;

// Array of colors to cycle through (R, G, B)
const byte colors[][3] = {
  {255,   0,   0},   // Red
  {255, 160,  16},   // Orange
  {255, 255,   0},   // Yellow
  {  0, 255,   0},   // Green
  {  0, 255, 255},   // Cyan
  {  0,   0, 255},   // Blue
  {255,   0, 255},   // Magenta
  {255, 105, 180},   // Hot Pink
  {255, 255, 255}    // White
};

int colorCount = sizeof(colors) / sizeof(colors[0]);


void setup() {
  Serial.begin(9600);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BRIGHTNESS_PIN, INPUT);
  pinMode(POT_2, INPUT);
  pinMode(SWITCH_1, INPUT_PULLUP);
  pinMode(SWITCH_2, INPUT_PULLUP);
  pinMode(SWITCH_3, INPUT_PULLUP);
  delay(500);
}

void loop() {

  bool b1 = digitalRead(SWITCH_1);
  bool b2 = digitalRead(SWITCH_2);
  bool b3 = digitalRead(SWITCH_3);
  //delay(500);

  // Normalize for active-low wiring if necessary, then compute a 3-bit mode:
  // bit2 = switch1, bit1 = switch2, bit0 = switch3
  auto computeModeFromSwitches = [](bool sb1, bool sb2, bool sb3) -> byte {
    bool s1 = SWITCH_ACTIVE_LOW ? !sb1 : sb1;
    bool s2 = SWITCH_ACTIVE_LOW ? !sb2 : sb2;
    bool s3 = SWITCH_ACTIVE_LOW ? !sb3 : sb3;
    return ( (byte)(s1 ? 1 : 0) << 2 )
         | ( (byte)(s2 ? 1 : 0) << 1 )
         | ( (byte)(s3 ? 1 : 0) << 0 );
  };

  mode = computeModeFromSwitches(b3, b2, b1);
  //Serial.println(mode);
  float temp = getTemp();
  
  // keep average of last 5 temperature readings
  if (lastTemps.size() >= 5) {
    lastTemps.pop();
  }
  lastTemps.push(temp);

  float averageLastTemps = 0;
  for (int i = 0; i < 5; i++) {
    averageLastTemps += lastTemps.front();
  }
  
  
  switch (mode)
  {
  case 0:
    /* Test Temperature-based color setting */
    setColorPWMFromTempWithBlink(80, 70);
    break;
  case 1:
    /* Temperature-based color setting */
    if (temp > averageLastTemps) {
      setColorPWM(255, 0, 0); // Set to red when temperature is rising
    } else {
      setColorPWMFromTemp(maxTemp, minTemp);
    }
    
    //Serial.print("F = ");
    //Serial.println(thermocouple.readFahrenheit());
    break;
  case 2:
    /* Color cycling */
    for (int i = 0; i < colorCount; i++) {
      int next = (i + 1) % colorCount;
      fadeToColor(colors[i][0], colors[i][1], colors[i][2],
                  colors[next][0], colors[next][1], colors[next][2]);
    }
    break;
  case 3:
    /* Christmas blink */
    blinkChristmas(10, 400, 0);
    break;
  }

}






float getTemp() {
  float temp = thermocouple.readFahrenheit();
  delay(500); // MAX6675 needs 250ms to stabilize
  return temp;
}

void setColorPWMFromTemp(float maxTemp, float minTemp) {
  float temperature = getTemp();
  byte r, g, b;
  float potValue = analogRead(BRIGHTNESS_PIN);        // 0 - 4096
  float brightness = potValue / 4096;             // 0.0 to 1.0
  //brightness = 1.0; // for use when potentiometer is not connected

  if (temperature <= minTemp) {
    // Green for cold temperatures
    r = 0;
    g = 255;
    b = 0;
  } else if (temperature >= maxTemp) {
    // Red for hot temperatures
    r = 255;
    g = 0;
    b = 0;
  } else {
    // Gradient from Blue to Red
    float ratio = (temperature - minTemp) / (maxTemp - minTemp);
    r = (byte)(ratio * 255);
    g = (byte)((1 - ratio) * 255);
    b = 0;
  }
  analogWrite(RED_PIN,   r * brightness);
  analogWrite(GREEN_PIN, g * brightness);
  analogWrite(BLUE_PIN,  b * brightness);
}

void setColorPWMFromTempWithBlink(float maxTemp, float minTemp) {
  float temperature = getTemp();
  byte r, g, b;
  float blink = 0;

  if (temperature <= minTemp) {
    // Green for cold temperatures
    r = 0;
    g = 255;
    b = 0;
    blink = 0;
  } else if (temperature >= maxTemp) {
    // Red for hot temperatures
    r = 255;
    g = 0;
    b = 0;
    blink = 200;
  } else {
    // Gradient from Green to Red
    float ratio = (temperature - minTemp) / (maxTemp - minTemp);
    r = (byte)(ratio * 255);
    g = (byte)((1 - ratio) * 255);
    b = 0;
    blink = 200 / ratio;
  }
  //Serial.println(blink);
  if (blink != 0) {
    setColorPWM(0, 0, 0);
    delay((blink));
  }
  setColorPWM(r, g, b);
  delay(blink);
}

// Set color with PWM and brightness scaling
void setColorPWM(byte r, byte g, byte b) {
  float potValue = analogRead(BRIGHTNESS_PIN);        // 0 - 4096
  float brightness = potValue / 4096.0;             // 0.0 to 1.0
  //Serial.println(brightness);
  //brightness = 0.8; // for use when potentiometer is not connected

  analogWrite(RED_PIN,   r * brightness);
  analogWrite(GREEN_PIN, g * brightness);
  analogWrite(BLUE_PIN,  b * brightness);
}

void fadeToColor(byte r1, byte g1, byte b1, byte r2, byte g2, byte b2) {
  for (int i = 0; i <= steps; i++) {
    byte r = map(i, 0, steps, r1, r2);
    byte g = map(i, 0, steps, g1, g2);
    byte b = map(i, 0, steps, b1, b2);
    setColorPWM(r, g, b);
    delay(delayTime);
  }
}

// Blink the strip in Christmas colors (Red -> Green -> White)
// cycles: how many full sequences to run
// onMs: how long each color stays on (ms)
// offMs: how long between sequences (ms)
void blinkChristmas(int cycles, int onMs, int offMs) {
  for (int c = 0; c < cycles; c++) {
    // Red
    setColorPWM(255, 0, 0);
    delay(onMs);

    // Green
    setColorPWM(0, 255, 0);
    delay(onMs);

    // White (accent)
    setColorPWM(255, 255, 255);
    delay(onMs);

    // short blackout between cycles so the blink is obvious
    setColorPWM(0, 0, 0);
    delay(offMs);
  }
}