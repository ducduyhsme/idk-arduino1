/*
 * Project Group 4 SEMI - Solar Tracker                              
 * Hardware:  Arduino Uno R3, 2 servo motors, 4 light sensors
 * Features: Automatic Mode, Manual Mode, Excel Data Logging
 */

#include <Servo. h>            // Includes the Servo library to control servo motors

// --- Pin Definitions ---
#define LDR_TL_PIN A0          // Defines analog pin A0 for the Top-Left Light Dependent Resistor (LDR) sensor
#define LDR_TR_PIN A1          // Defines analog pin A1 for the Top-Right LDR sensor
#define LDR_BL_PIN A2          // Defines analog pin A2 for the Bottom-Left LDR sensor
#define LDR_BR_PIN A3          // Defines analog pin A3 for the Bottom-Right LDR sensor

#define POT_PIN A4             // Defines analog pin A4 for the potentiometer used in manual control mode
#define VOLT_PIN A5            // Defines analog pin A5 for reading the solar panel voltage through a voltage divider

#define SERVO_LR_PIN 6         // Defines digital pin 6 for the Left-Right servo motor (continuous rotation type)
#define SERVO_UD_PIN 5         // Defines digital pin 5 for the Up-Down servo motor (standard 180-degree type)

#define BTN_MODE_PIN 12        // Defines digital pin 12 for the button that switches between Auto and Manual modes
#define BTN_AXIS_PIN 11        // Defines digital pin 11 for the button that switches between LR and UD servo control in Manual mode

// --- Constants & Configuration ---
#define TOLERANCE 10           // Defines the tolerance threshold (±10) for light difference before servo movement is triggered
#define NIGHT_LIMIT 8          // Defines the minimum average light level; below this value, night mode activates
#define LOAD_RESISTANCE 10. 0  // Defines the load resistance value (10 ohms) used for current calculation

// Speeds for Continuous Servo (90 is Stop)
#define LR_SPEED_RIGHT 80       // Defines the speed value to rotate the LR servo to the right (values < 90 move one direction)
#define LR_SPEED_LEFT 100       // Defines the speed value to rotate the LR servo to the left (values > 90 move opposite direction)
#define LR_STOP 90              // Defines the value that stops the continuous rotation servo (90 = neutral/stop)

Servo servoLR;                   // Creates a Servo object named 'servoLR' to control the Left-Right continuous rotation servo
Servo servoUD;                   // Creates a Servo object named 'servoUD' to control the Up-Down standard servo

// --- Variables ---
bool isAutoMode = true;               // Boolean variable to track current mode; true = Automatic mode, false = Manual mode
bool manualControlLR = true;          // Boolean variable for manual mode; true = controlling LR servo, false = controlling UD servo

int posUD = 45; // Integer variable storing the current position of the Up-Down servo (initialized to 45 degrees)

// Button State Tracking
int lastBtnModeState = HIGH;          // Stores the previous state of the mode button (HIGH = not pressed, using INPUT_PULLUP)
int lastBtnAxisState = HIGH;          // Stores the previous state of the axis button (HIGH = not pressed, using INPUT_PULLUP)

void setup() {                        // setup() function runs once when Arduino starts or resets
  Serial.begin(9600);                 // Initializes serial communication at 9600 baud rate for data logging
  Serial.println("CLEARDATA");        // Sends command to PLX-DAQ Excel add-in to clear previous data
  Serial.println("LABEL,Time,Mode,Voltage(V),Current(mA),Power(mW)");  // Sends column headers to Excel for data logging

  servoLR.attach(SERVO_LR_PIN);       // Attaches the servoLR object to pin 6 to enable control of the LR servo
  servoUD.attach(SERVO_UD_PIN);       // Attaches the servoUD object to pin 5 to enable control of the UD servo
  
  pinMode(BTN_MODE_PIN, INPUT_PULLUP); // Sets pin 12 as input with internal pull-up resistor (button reads HIGH when not pressed)
  pinMode(BTN_AXIS_PIN, INPUT_PULLUP); // Sets pin 11 as input with internal pull-up resistor (button reads HIGH when not pressed)
  
  // Initial positions
  servoLR.write(LR_STOP);             // Sets the LR servo to stop position (90) at startup
  servoUD.write(posUD);               // Sets the UD servo to its initial position (45 degrees) at startup
  
  delay(500);                         // Waits 500 milliseconds to allow servos to reach their initial positions
}

void loop() {                         // loop() function runs repeatedly after setup() completes
  // --- 1. Check Mode Switch Button ---
  int btnModeState = digitalRead(BTN_MODE_PIN);  // Reads the current state of the mode button (HIGH or LOW)
  if (btnModeState == LOW && lastBtnModeState == HIGH) {  // Checks if button was just pressed (falling edge detection)
    isAutoMode = !isAutoMode;         // Toggles between Auto mode and Manual mode
    servoLR.write(LR_STOP);           // Stops the LR servo for safety when switching modes
    delay(200);                       // Waits 200ms for button debouncing to prevent multiple toggles
  }
  lastBtnModeState = btnModeState;    // Updates the stored button state for next loop iteration

  // --- 2. Execute Mode Logic ---
  if (isAutoMode) {                   // Checks if the system is in Automatic mode
    runAutomaticMode();               // Calls the function to execute automatic solar tracking logic
  } else {                            // Otherwise, the system is in Manual mode
    runManualMode();                  // Calls the function to execute manual control logic
  }

  // --- 3. Data Logging to Excel ---
  logDataToExcel();                   // Calls the function to send voltage, current, and power data to Excel
  
  delay(50);                          // Waits 50 milliseconds before the next loop iteration (controls update rate)
}

// --- Automatic Mode Logic based on Flowchart ---
void runAutomaticMode() {             // Function that implements automatic solar tracking based on LDR readings
  // 1. Read the analog value from each LDR sensor
  int topleft = analogRead(LDR_TL_PIN);   // Reads analog value (0-1023) from the Top-Left LDR sensor
  int topright = analogRead(LDR_TR_PIN);  // Reads analog value (0-1023) from the Top-Right LDR sensor
  int botleft = analogRead(LDR_BL_PIN);   // Reads analog value (0-1023) from the Bottom-Left LDR sensor
  int botright = analogRead(LDR_BR_PIN);  // Reads analog value (0-1023) from the Bottom-Right LDR sensor

  // 2. Calculate the average values
  int avgtop = (topright + topleft) / 2;    // Calculates the average light intensity of the top two LDRs
  int avgbot = (botright + botleft) / 2;    // Calculates the average light intensity of the bottom two LDRs
  int avgright = (topright + botright) / 2; // Calculates the average light intensity of the right two LDRs
  int avgleft = (topleft + botleft) / 2;    // Calculates the average light intensity of the left two LDRs

  // 3. Calculate the differences (Azimuth & elevation)
  int diffelev = avgtop - avgbot;           // Calculates elevation difference (positive = more light on top)
  int diffazi = avgright - avgleft;         // Calculates azimuth difference (positive = more light on right)
  int avgsum = (avgtop + avgbot + avgright + avgleft) / 4;  // Calculates overall average light intensity

  // 4. Check Night Mode (avgsum < 8)
  if (avgsum < NIGHT_LIMIT) {               // Checks if average light is below night threshold (too dark)
    // "Rotate the Servo Motors to the initial position"
    servoLR.write(LR_STOP);                 // Stops the LR servo during night mode
    posUD = 30;                             // Sets UD position to 30 degrees (facing east for sunrise)
    servoUD.write(posUD);                   // Moves the UD servo to the sunrise/horizon position
    return;                                 // Exits the function early, skipping the rest of the tracking logic
  }

  // 5. Azimuth (Left-Right) Control
  // Check |diffazi| <= 10
  if (abs(diffazi) <= TOLERANCE) {          // Checks if the left-right light difference is within tolerance
    // "Stop the Left-Right Servo Motor"
    servoLR.write(LR_STOP);                 // Stops the LR servo (panel is aligned horizontally with sun)
  } 
  else {                                    // The difference exceeds tolerance, servo needs to move
    // Check diffazi > 0
    if (diffazi > 0) {                      // If positive, more light is on the right side
      // "Left-Right Servo Motor move PV panel Right"
      servoLR.write(LR_SPEED_RIGHT);        // Rotates the LR servo to move the panel toward the right
    } else {                                // If negative, more light is on the left side
      // "Left-Right Servo Motor move PV panel Left"
      servoLR.write(LR_SPEED_LEFT);         // Rotates the LR servo to move the panel toward the left
    }
  }

  // 6. Elevation (Up-Down) Control
  // Check |diffelev| <= 10
  if (abs(diffelev) <= TOLERANCE) {         // Checks if the top-bottom light difference is within tolerance
    // "Stop the Up-Down Servo Motor"
    // Do nothing (position doesn't change)  // Panel is aligned vertically with sun, no adjustment needed
  } 
  else {                                    // The difference exceeds tolerance, servo needs to move
    // Check diffelev > 0
    if (diffelev > 0) {                     // If positive, more light is on the top
      // "Up-Down Servo Motor move PV panel Up"
      posUD = posUD + 1;                    // Increments the UD position by 1 degree to tilt panel up
    } else {                                // If negative, more light is on the bottom
      // "Up-Down Servo Motor move PV panel Down"
      posUD = posUD - 1;                    // Decrements the UD position by 1 degree to tilt panel down
    }
    // Write the new position for the standard servo
    posUD = constrain(posUD, 0, 180);       // Constrains posUD value to valid servo range (0 to 180 degrees)
    servoUD.write(posUD);                   // Sends the new position to the UD servo to move it
  }
  // The logic then goes back to Start       // After completing, the loop() will call this function again
}

// --- Manual Mode Logic ---
void runManualMode() {                      // Function that allows manual control of servos using a potentiometer
  int btnAxisState = digitalRead(BTN_AXIS_PIN);  // Reads the current state of the axis selection button
  if (btnAxisState == LOW && lastBtnAxisState == HIGH) {  // Checks if button was just pressed (falling edge)
    manualControlLR = !manualControlLR;     // Toggles between controlling LR servo and UD servo
    servoLR. write(LR_STOP);                 // Stops the LR servo for safety when switching axis control
    delay(200);                             // Waits 200ms for button debouncing
  }
  lastBtnAxisState = btnAxisState;          // Updates the stored button state for next loop iteration

  int potVal = analogRead(POT_PIN);         // Reads the analog value (0-1023) from the potentiometer
  int angle = map(potVal, 0, 1023, 0, 180); // Maps the potentiometer value to a servo angle (0 to 180 degrees)

  if (manualControlLR) {                    // Checks if currently controlling the LR servo
    // Potentiometer controls speed/direction of continuous servo
    servoLR.write(angle);                   // Sets LR servo speed/direction based on potentiometer (90=stop)
  } else {                                  // Otherwise, controlling the UD servo
    // Potentiometer controls position of standard servo
    servoUD.write(angle);                   // Sets UD servo position based on potentiometer value
    posUD = angle;                          // Updates the posUD variable to track current UD position
  }
}

// --- Data Logging Logic ---
void logDataToExcel() {                     // Function that calculates and sends power data to Excel via serial
  float sensorValue = analogRead(VOLT_PIN); // Reads the analog value (0-1023) from the voltage sensor pin
  float voltage = sensorValue * (5.0 / 1023.0);  // Converts analog reading to voltage (assumes 5V reference)
  float current = (voltage / LOAD_RESISTANCE) * 1000;  // Calculates current in milliamps using Ohm's law (I=V/R)
  float power = voltage * current;          // Calculates power in milliwatts (P = V × I)

  Serial.print("DATA,TIME,");               // Sends data prefix and TIME keyword (PLX-DAQ inserts timestamp)
  if (isAutoMode) Serial.print("Auto");     // If in Auto mode, prints "Auto" to the serial output
  else Serial. print("Manual");              // If in Manual mode, prints "Manual" to the serial output
  Serial.print(",");                        // Prints a comma delimiter for CSV formatting
  Serial.print(voltage);                    // Prints the calculated voltage value
  Serial.print(",");                        // Prints a comma delimiter for CSV formatting
  Serial.print(current);                    // Prints the calculated current value in milliamps
  Serial. print(",");                        // Prints a comma delimiter for CSV formatting
  Serial.println(power);                    // Prints the calculated power value and ends the line
}
