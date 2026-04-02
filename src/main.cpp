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

/* PARAMETER CONFIGURATION */
const float maxTemp = 200.0; // Maximum temperature for scaling (F)
const float minTemp = 100.0; // Minimum temperature for scaling (F)
const float offThreshold = 80.0; // Temperature threshold to turn off LEDs
const long int timeThreshold = 1800; // Time threshold (in seconds) to turn off LEDs
const long int minimumTimeAfterHeatTrigger = 600; // Minimum time (in seconds) that must pass after a heat trigger before LEDs can turn off


/// Variable and function declarations
// Create MAX6675 thermocouple instance
MAX6675 thermocouple(THERMO_CLK, THERMO_CS, THERMO_DO);
// Function declarations:
void setColorPWM(byte r, byte g, byte b);
void fadeToColor(byte r1, byte g1, byte b1, byte r2, byte g2, byte b2);
float getTemp();
void setColorPWMFromTemp(float maxTemp, float minTemp);
void blinkChristmas(int cycles, int onMs, int offMs);
void setColorPWMFromTempWithBlink(float maxTemp, float minTemp);
void setColorPWMFromTempWithFade(float maxTemp, float minTemp);
void setColorPWMFromTempAndTime(float maxTemp, float minTemp, unsigned long maxTimer);
// Variable declarations:
const int fadeTime = 1000;
const int steps = 100;
const int delayTime = fadeTime / steps;
long int prevTime = 0;
long int currTime = 0;
std::queue<float> lastTemps;
long int timePassed = 0;
byte mode = 0;
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
bool timerRunningFlag = false;

// Setup function to initialize serial communication, pin modes, and delay for sensor stabilization
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

// Main loop to read switch states, compute mode, read temperature, and control LED behavior based on mode
void loop() {

  // Read switch states and compute case switch mode
  bool b1 = digitalRead(SWITCH_1);
  bool b2 = digitalRead(SWITCH_2);
  bool b3 = digitalRead(SWITCH_3);
  auto computeModeFromSwitches = [](bool sb1, bool sb2, bool sb3) -> byte {
    bool s1 = SWITCH_ACTIVE_LOW ? !sb1 : sb1;
    bool s2 = SWITCH_ACTIVE_LOW ? !sb2 : sb2;
    bool s3 = SWITCH_ACTIVE_LOW ? !sb3 : sb3;
    return ( (byte)(s1 ? 1 : 0) << 2 )
         | ( (byte)(s2 ? 1 : 0) << 1 )
         | ( (byte)(s3 ? 1 : 0) << 0 );
  };
  mode = computeModeFromSwitches(b3, b2, b1);

  // Keep an average of the last few temperature readings
  float temp = getTemp();  
  if (lastTemps.size() >= 5) {lastTemps.pop();}
  lastTemps.push(temp);
  float averageLastTemps = 0;
  float sumLastTemps = 0;
  for (int i = 0; i < 5; i++) {sumLastTemps += lastTemps.front();}
  averageLastTemps = sumLastTemps / lastTemps.size();

  // Begining of case switcher
  switch (mode) {

    // Case 0 is for testing the functionality of temp based color settings, it operated between 75-85 degrees F.
    // This case is turned on by setting all three switches to OFF
    case 0:
      /* Test Temperature-based color setting */
      setColorPWMFromTempWithFade(85, 75);
      break; 
    // end of case 0

  // MAIN CODE BLOCK //////////////////////////////////////////////////////////////////////////////////////////
    // Case 1 is the main operation mode, it operates between the two high temp thesholds and turns off after a certain time below the low temp threshold.
    // This case is turned on by setting switch 1 to ON and the other two to OFF
    case 1: 
      /* Temperature-based color setting */
      if (temp > averageLastTemps+3) {
        setColorPWM(255, 0, 0); // Set to red when temperature is rising
        timerRunningFlag = true; // Start the timer when a heat trigger is detected
      } if (timerRunningFlag){
        setColorPWMFromTempAndTime(maxTemp, minTemp, minimumTimeAfterHeatTrigger);
      } if (!timerRunningFlag) {
        setColorPWMFromTemp(maxTemp, minTemp);
      }

      if (temp < offThreshold && timePassed > timeThreshold) {
        setColorPWM(0, 0, 0); // Turn off LEDs if temperature is below threshold
        while (temp < offThreshold) {
          temp = getTemp();
          delay(1000);
          Serial.print("Temperature below threshold. Current temp: ");
          Serial.println(temp);
        }
        timePassed = 0; // Reset time passed when temperature goes above threshold
      }

      // One Second Checker
      // do these actions after 1 second has passed
      currTime = millis()/1000;
      if (currTime - prevTime >= 1) {
        Serial.print(currTime);
        Serial.print(" | ");
        Serial.println(temp);
        timePassed+=1;
        prevTime = currTime;
      }

      break;
    // end of case 1

///////////////////////////////////////////////////////////////////////////////////////////////////////////


    case 2:
      /* Color cycling */
      for (int i = 0; i < colorCount; i++) {
        int next = (i + 1) % colorCount;
        fadeToColor(colors[i][0], colors[i][1], colors[i][2],
                    colors[next][0], colors[next][1], colors[next][2]);
      }
      break;
    // end of case 2

  }
  // end of case switch

}
// end of loop


// HELPER FUNCTIONS //////////////////////////////////////////////////////////////////////////////////////////

// Function to read temperature from MAX6675
float getTemp() {
  float temp = thermocouple.readFahrenheit();
  delay(500); // MAX6675 needs 250ms to stabilize
  return temp;
}

// Set color based on temperature with PWM and brightness scaling
void setColorPWMFromTemp(float maxTemp, float minTemp) {
  float temperature = getTemp();
  byte r, g, b;
  float potValue = analogRead(BRIGHTNESS_PIN);        // 0 - 4096
  float brightness = potValue / 4096;             // 0.0 to 1.0
  brightness = 0.8; // for use when potentiometer is not connected  -------------------------------------------------------------------------------------<<

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
    // Gradient from Green to Red
    float ratio = (temperature - minTemp) / (maxTemp - minTemp);
    r = (byte)(ratio * 255);
    g = (byte)((1 - ratio) * 255);
    b = 0;
  }
  analogWrite(RED_PIN,   r * brightness);
  analogWrite(GREEN_PIN, g * brightness);
  analogWrite(BLUE_PIN,  b * brightness);
}

// Set color based on temperature and time with PWM and brightness scaling
void setColorPWMFromTempAndTime(float maxTemp, float minTemp, unsigned long maxTimer) {
  static unsigned long startTime = 0;

  if (!timerRunningFlag) {
    timerRunningFlag = true;
    startTime = millis();
  }
  unsigned long elapsed = millis() - startTime;

  if (elapsed >= maxTimer * 1000UL) {
    timerRunningFlag = false;
  }

  float timerRatio = timerRunningFlag ? (1.0f - min(1.0f, (float)elapsed / (maxTimer * 1000UL))) : 0.0f;

  float temperature = getTemp();
  float tempRatio = 0.0f;
  if (temperature <= minTemp) {
    tempRatio = 0.0f;
  } else if (temperature >= maxTemp) {
    tempRatio = 1.0f;
  } else {
    tempRatio = (temperature - minTemp) / (maxTemp - minTemp);
  }

  // Use the more red value:
  // tempRatio=0=green,1=red; timerRatio=1=red at start then drops to 0 over maxTimer
  float ratio = max(tempRatio, timerRatio);
  byte r = (byte)(ratio * 255);
  byte g = (byte)((1 - ratio) * 255);
  byte b = 0;

  float potValue = analogRead(BRIGHTNESS_PIN);
  float brightness = potValue / 4096.0f;
  brightness = 0.8f; // for use when potentiometer is not connected

  analogWrite(RED_PIN, r * brightness);
  analogWrite(GREEN_PIN, g * brightness);
  analogWrite(BLUE_PIN, b * brightness);
}

// Set color based on temperature with PWM, brightness scaling, and blinking effect
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



// Smooth fade blink effect based on temperature
void setColorPWMFromTempWithFade(float maxTemp, float minTemp) {
  float temperature = getTemp();
  byte baseR, baseG, baseB;

  // ----- Determine Base Color (same logic) -----
  if (temperature <= minTemp) {
    baseR = 0;
    baseG = 255;
    baseB = 0;
  } else if (temperature >= maxTemp) {
    baseR = 255;
    baseG = 0;
    baseB = 0;
  } else {
    float ratio = (temperature - minTemp) / (maxTemp - minTemp);
    baseR = (byte)(ratio * 255);
    baseG = (byte)((1 - ratio) * 255);
    baseB = 0;
  }

  // ----- Map temperature to fade speed -----
  // ratio: 0 at minTemp (green) -> 1 at maxTemp (red)
  float ratio = constrain((temperature - minTemp) / (maxTemp - minTemp), 0.0f, 1.0f);

  // Tune these two to taste:
  const float minSpeed = 6.0f;    // slow at cold (green)
  const float maxSpeed = 120.0f;  // very fast at hot (red)

  // Higher temp -> higher speed
  float fadeSpeed = (minSpeed + ratio * (maxSpeed - minSpeed));

  // If you want *no* fade at the very coldest, clamp near zero:
  // if (ratio <= 0.01f) fadeSpeed = 0;

  // ----- Fade state (persists across calls) -----
  static int brightness = 0;
  static int direction = 1;  // +1 = brighter, -1 = dimmer

  if (fadeSpeed <= 0.0f) {
    // No fading (solid color)
    setColorPWM(baseR, baseG, baseB);
    delay(15);
    return;
  }

  // Advance brightness by fadeSpeed steps per frame
  brightness += (int)fadeSpeed * direction;

  if (brightness >= 255) {
    brightness = 255;
    direction  = -1;
  } else if (brightness <= 0) {
    brightness = 0;
    direction  = 1;
  }

  // Apply brightness scaling to base color
  byte r = (baseR * brightness) / 255;
  byte g = (baseG * brightness) / 255;
  byte b = (baseB * brightness) / 255;

  setColorPWM(r, g, b);

  // Frame time (smaller = smoother and effectively faster)
  delay(10);
}



// Set color with PWM and brightness scaling, used as a helper function for other modes
void setColorPWM(byte r, byte g, byte b) {
  float potValue = analogRead(BRIGHTNESS_PIN);        // 0 - 4096
  float brightness = potValue / 4096.0;             // 0.0 to 1.0
  //Serial.println(brightness);
  brightness = 0.8; // for use when potentiometer is not connected ---------------------------------------------------------------------------------------<<
  analogWrite(RED_PIN,   r * brightness);
  analogWrite(GREEN_PIN, g * brightness);
  analogWrite(BLUE_PIN,  b * brightness);
}

// Fade from one color to another over a specified time
void fadeToColor(byte r1, byte g1, byte b1, byte r2, byte g2, byte b2) {
  for (int i = 0; i <= steps; i++) {
    byte r = map(i, 0, steps, r1, r2);
    byte g = map(i, 0, steps, g1, g2);
    byte b = map(i, 0, steps, b1, b2);
    setColorPWM(r, g, b);
    delay(delayTime);
  }
}
