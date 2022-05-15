// Using an Arduino with Python LESSON 18: 3D Pong Game With Arduino and vPython.
// https://www.youtube.com/watch?v=czhneFmwJos
// https://toptechboy.com/

// https://www.arduino.cc/en/Tutorial/BuiltInExamples/SerialEvent
// https://www.best-microcontroller-projects.com/arduino-strtok.html
// https://www.e-tinkers.com/2020/01/do-you-know-arduino-sprintf-and-floating-point/
// https://www.leonardomiliani.com/en/2013/un-semplice-crc8-per-arduino/

// CRC-8/MAXIM verification: https://crccalc.com/?method=crc8

#include <Watchdog.h>         // A simple watchdog library (by Peter Polidoro, installed via Library Manager).

// Arduino I/O pin defines.
#define JSTKXPIN A0           // Joystick X axis.
#define JSTKYPIN A1           // Joystick Y axis.
#define JSTKZPIN 4            // Joystick push button.
#define B0LEDPIN 5            // Bit 0 red LED.
#define B1LEDPIN 6            // Bit 1 red LED.
#define B2LEDPIN 7            // Bit 2 red LED.
#define PBUZRPIN 8            // A passive piezo buzzer.
#define HBLEDPIN LED_BUILTIN  // Usually digital I/O pin 13.

// Some (re)defines to make LED and buzzer control easier to follow.
#define ON HIGH
#define OFF LOW
#define ALLON 0x07

// Function prototypes - this allows the definition of default values.
void binDispLEDs(int action = 0);
void soundPBuzzer(char note = 'L', int duration = 100);

//Code loop job defines.
#define JOB1CYCLE 25          // Job 1 execution cycle: 0.025s - Get the data: Read the joystick.
#define JOB2CYCLE 50          // Job 2 execution cycle: 0.050s - Share the results: Transmit the joystick data to the serial console.
#define JOB3CYCLE 200         // Job 3 execution cycle: 0.200s - Read commands: Parse any received serial commands.
#define JOB4CYCLE 200         // Job 4 execution cycle: 0.200s - Action commands: Action any received sensor data read serial commands.
#define JOB5CYCLE 200         // Job 5 execution cycle: 0.200s - Action commands: Action any received LED and buzzer serial commands.
#define JOB9CYCLE 500         // Job 9 execution cycle: 0.500s - Toggle the heartbeat LED and reset the watchdog.

// Hardware test defines.
# define TSTDRTN 250

// Transmit data buffer.
#define TXBUFFERMAX 32              // The maximum size of the data buffer in which sensor data characters are stored before sending.
char txBuffer[TXBUFFERMAX + 1];     // A character array variable to hold outgoing sensor data characters.
// Receive data buffer.
#define RXBUFFERMAX 32              // The maximum size of the data buffer in which received command characters are stored.
char rxBuffer[RXBUFFERMAX + 1];     // A character array variable to hold incoming command character data.
// Transmit/Receive buffer delimiters.
const char crcDelimiter[] = ":~!";  // The delimiter between the command and CRC8 checksum can be any of these characters.
const char cmdDelimiter[] = " ,=";  // The delimiter between the command subject and command action can be any of these characters.

// Receive command flag.
bool commandReady = false;          // A flag to indicate that the current command is ready to be actioned.

// Watchdog initialisation.
Watchdog cerberous;

void setup() {
  // Initialise the joystick pins.
  pinMode(JSTKXPIN, INPUT);        // Joystick X axis.
  pinMode(JSTKYPIN, INPUT);        // Joystick Y axis.
  pinMode(JSTKZPIN, INPUT_PULLUP); // Joystick push button.
  // Initialise the LED pins.
  pinMode(B0LEDPIN, OUTPUT);       // Bit 0.
  pinMode(B1LEDPIN, OUTPUT);       // Bit 1.
  pinMode(B2LEDPIN, OUTPUT);       // Bit 2.
  pinMode(HBLEDPIN, OUTPUT);       // Builtin LED, usually on pin 13.
  // Initialise the buzzer pin.
  pinMode(PBUZRPIN, OUTPUT);       // Passive piezo buzzer.
  // LED test - turn ON the LEDs.
  binDispLEDs(ALLON);
  delay(TSTDRTN);
  // LED test - turn OFF the LEDs.
  binDispLEDs(OFF);
  // Buzzer test - a medium beep.
  soundPBuzzer('M', TSTDRTN);
  delay(TSTDRTN);
  // Start the serial port.
  Serial.begin(115200);            // As fast as we can go!
  while(!Serial);                  // Wait for the serial I/O to start.
  // Start the watchdog.
  cerberous.enable();              // The default watchdog timeout period is 1000ms.
}

void loop() {
  // Initialise the heartbeat status variable - OFF = LOW = 0.
  static bool hbStatus = OFF;
  // Initialise the joystick variables.
  static int jstkXValue;
  static int jstkYValue;
  static int jstkXValueNow;
  static int jstkYValueNow;
  static int jstkXValueOld = 512;    // Joystick centered.
  static int jstkYValueOld = 512;    // Joystick centered.
  static boolean jstkZValue;
  static boolean jstkZValueNow;
  static boolean jstkZValueOld = 1;  // Button not pressed.
  // Initialise the sensor data read command action variable.
  static int getSData = -1;          // No action, no change.
  static bool jstkDataEnabled = false;
  // Initialise the LED command action variable.
  static int binDispLEDsAction = -1; // No action, no change.
  // Initialise the passive buzzer command action status variables.
  static char pBuzzerActionN;
  static int pBuzzerActionD = -1;    // No action, no change.
  // Initialise the CRC8 checksum variable.
  byte chksumCRC8 = 0;
  // Record the current time. When a single timeNow is used for all jobs it ensures they are synchronised.
  unsigned long timeNow = millis();
  // Job variables. Set to timeNow so that jobs do not start immediately - this allows sensors to settle.
  static unsigned long timeMark1 = timeNow; // Last time Job 1 was executed.
  static unsigned long timeMark2 = timeNow; // Last time Job 2 was executed.
  static unsigned long timeMark3 = timeNow; // Last time Job 3 was executed.
  static unsigned long timeMark4 = timeNow; // Last time Job 4 was executed.
  static unsigned long timeMark5 = timeNow; // Last time Job 5 was executed.
  static unsigned long timeMark9 = timeNow; // Last time Job 9 was executed.
  // Job 1 - Get the data: Read the joystick.
  if (timeNow - timeMark1 >= JOB1CYCLE) {
    timeMark1 = timeNow;
    // Do something...
    // Get the joystick readings.
    if (jstkDataEnabled) {
      jstkXValueNow = analogRead(JSTKXPIN);             // X axis, left = 0, right = 1023.
      jstkYValueNow = 1023 - analogRead(JSTKYPIN);      // Y axis, down = 0, up = 1023 (corrected).
      jstkXValue = (jstkXValueOld + jstkXValueNow) / 2; // Previous and current value averaged.
      jstkXValueOld = jstkXValueNow;
      jstkYValue = (jstkYValueOld + jstkYValueNow) / 2; // Previous and current value averaged.
      jstkYValueOld = jstkYValueNow;
      jstkZValueNow = digitalRead(JSTKZPIN);            // Push button, pressed = 0, not pressed = 1.
      // Simple switch debouncing - Z must be the same for 2 readings in a row to be noticed.
      if (jstkZValueNow == jstkZValueOld) {
        jstkZValue = jstkZValueNow;
      }
      jstkZValueOld = jstkZValueNow;
    }
  }
  // Job 2 - Share the results: Transmit the joystick data to the serial console.
  if (timeNow - timeMark2 >= JOB2CYCLE) {
    timeMark2 = timeNow;
    // Do something...   
    // Construct the send data string.
    if (jstkDataEnabled) {
      sprintf(txBuffer, "%d,%d,%d", jstkXValue, jstkYValue, jstkZValue);
      // Calculate the CRC8 checksum of the txBuffer.
      chksumCRC8 = calcCRC8((byte*)txBuffer); // Cast the char array pointer to a byte array pointer.
      // Print the results.
      Serial.print(txBuffer);
      // Add the CRC8 checksum to the end.
      Serial.print("!");
      Serial.println(chksumCRC8);
    }
  }
  // Job 3 - Read commands: Parse any received serial commands.
  if (timeNow - timeMark3 >= JOB3CYCLE) {
    timeMark3 = timeNow;
    // Do something...
    // If we have received something via the serial port then parse it.
    if (commandReady) {
      // Parse the received data - NULL is returned if nothing is found by strtok().
      char *commands = strtok(rxBuffer, crcDelimiter); // A pointer to a NULL terminated part of the receive buffer.
      char *chksum  = strtok(NULL, crcDelimiter);      // A pointer to another NULL terminated part of the receive buffer.
      // Lets check the CRC8 checksum, if there is one.
      if (chksum != NULL) {
        if (calcCRC8((byte*)commands) != (byte)atoi(chksum)) {
          commands = NULL; // Cancel all commands if the CRC8 checksum has failed.
        }
      }
      // Extract the first command.
      char *subject = strtok(commands, cmdDelimiter);  // A pointer to a NULL terminated part of the receive buffer.
      char *action  = strtok(NULL, cmdDelimiter);      // A pointer to another NULL terminated part of the receive buffer.
      // Extract the action parts.
      // Parse the command - NULL is returned if nothing is found by strtok().
      while (subject != NULL and action != NULL) {
        // Test and extract a command action - minimal input testing to ensure there is a valid action.
        if (strcmp(subject, "GSDt") == 0 and strlen(action) > 0) {
          getSData = atoi(action);            // We have a recognised subject and an action for it.
        }
        // Test and extract a command action - minimal input testing to ensure there is a valid action.
        if (strcmp(subject, "LEDs") == 0 and strlen(action) > 0) {
          binDispLEDsAction = atoi(action);            // We have a recognised subject and an action for it.
        }
        // Test and extract a command action - minimal input testing to ensure there is a valid action.
        if (strcmp(subject, "Beep") == 0 and strlen(action) > 1) {
          pBuzzerActionN = *action;                    // We have a recognised subject and an action for it.
          pBuzzerActionD = atoi(action + 1);           // We have a recognised subject and an action for it.
        }
        // Extract the next command.
        subject = strtok(NULL, cmdDelimiter);          // A pointer to a NULL terminated part of the receive buffer.
        action  = strtok(NULL, cmdDelimiter);          // A pointer to another NULL terminated part of the receive buffer.
      }
      // All done, so clear the ready flag for more commands to be received.
      commandReady = false;
    }
  }
  // Job 4 - Action commands: Action any received sensor data read serial commands.
  if (timeNow - timeMark4 >= JOB4CYCLE) {
    timeMark4 = timeNow;
    // Do something...
    // If we have received a sensor data read serial command.
    if (getSData >= 0) {
      if (getSData == 0) {
        jstkDataEnabled = false;
      }
      if (getSData == 1) {
        jstkDataEnabled = true;
      }
      getSData = -1; // Clear the actioned action.
    }
  }
  // Job 5 - Action commands: Action any received LED and buzzer serial commands.
  if (timeNow - timeMark5 >= JOB5CYCLE) {
    timeMark5 = timeNow;
    // Do something...
    // If we have received a binary display LEDs serial command.
    if (binDispLEDsAction >= 0) {
      binDispLEDs(binDispLEDsAction);
      binDispLEDsAction = -1; // Clear the actioned action.
    }
    // If we have received a passive buzzer serial command.
    if (pBuzzerActionD > 0) {
      soundPBuzzer(pBuzzerActionN, pBuzzerActionD);
      pBuzzerActionD = -1;    // Clear the actioned action.
    } 
  }
  // Job 9 - Toggle the heartbeat LED and reset the watchdog.
  if (timeNow - timeMark9 >= JOB9CYCLE) {
    timeMark9 = timeNow;
    // Do something...
    // Toggle the heartbeat status.
    hbStatus = !hbStatus;
    digitalWrite(HBLEDPIN, hbStatus);
    // Reset the watchdog.
    cerberous.reset();
  }
}

// SerialEvent automatically executes after each run of main loop if new serial data has been received.
void serialEvent() {
  // We must preserve the command buffer index between calls as this function may not collect a whole command in a single call.
  static byte bufferIndex = 0;
  while (Serial.available() and not commandReady) {
    // Get the new byte of data from the serial rx buffer.
    char rxChar = (char)Serial.read();
    // If we have received the end of command delimiter or reached the end of the buffer, finish the string and set a flag for the main loop to action the command.
    if (rxChar == '\n' or bufferIndex == RXBUFFERMAX) {
      rxBuffer[bufferIndex] = '\0'; // Terminate the string.
      commandReady = true;          // Set the command redy flag for the main loop.
      bufferIndex = 0;              // Reset the buffer index in readyness for the next command.
    }
    // Otherwise, if we are builiding a new command, add the data to the command buffer and increment the buffer index.
    if (not commandReady) {
      rxBuffer[bufferIndex++] = rxChar;
    }
  }
}

// Turn ON/OFF the binary display LEDs as per the command action.
void binDispLEDs(int action) {
  // Valid action values are 0 - 7 using bits 0, 1 and 2 (3x LEDs), otherwise the requested action is ignored.
  if (action >= 0 and action <= 7) {
    // Action value bit 0.
    if (bitRead(action, 0) == 1) {
      digitalWrite(B0LEDPIN, ON);
    }
    else {
      digitalWrite(B0LEDPIN, OFF);
    }
    // Action value bit 1.
    if (bitRead(action, 1) == 1) {
      digitalWrite(B1LEDPIN, ON);
    }
    else {
      digitalWrite(B1LEDPIN, OFF);
    }
    // Action value bit 2.
    if (bitRead(action, 2) == 1) {
      digitalWrite(B2LEDPIN, ON);
    }
    else {
      digitalWrite(B2LEDPIN, OFF);
    }
  }
}

// Turn ON the passive buzzer at the given frequency for the duration specified.
void soundPBuzzer(char note, int duration) {
  if (note == 'L') {
    tone(PBUZRPIN, 440, duration); // Low note    = L, 440hz.
  }
  if (note == 'M') {
    tone(PBUZRPIN, 660, duration); // Medium note = M, 660hz.
  }
  if (note == 'H') {
    tone(PBUZRPIN, 880, duration); // High note   = H, 880hz.
  }
}

// Calculate the CRC8 checksum of a null terminated character array.
// Based on the CRC8 formulas by Dallas/Maxim (GNU GPL 3.0 license).
byte calcCRC8(byte* dataBuffer) {
  // Initialise the CRC8 checksum.
  byte chksumCRC8 = 0;
  // While the byte to be process is not the null terminator.
  while(*dataBuffer != '\0') {
    byte currentByte = *dataBuffer; // Get the byte to be processed.
    // Process each bit of the byte. 
    for (byte bitCounter = 0; bitCounter < 8; bitCounter++) {
        byte sum = (chksumCRC8 ^ currentByte) & 0x01;
        chksumCRC8 >>= 1;
        if (sum) {
           chksumCRC8 ^= 0x8C;
        }
        currentByte >>= 1;
     }
     dataBuffer++;
  }
  return chksumCRC8;
}

// EOF
