/* Tide_controller_v2.1
  Copyright (C) 2013 Luke Miller
 This version is set up to work on a lead-screw driven rack that has
 a limited travel range. There should be a limit switch at each end 
 of the rack's travel, and the distance between the values for 
 upperPos and lowerPos must be equal to the distance between those 
 limit switches. Designed to work with daughterboard rev 6. 
 
 Copyright (C) 2013 Luke Miller
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see http://www.gnu.org/licenses/.
 
 
 This program is designed to calculate the current tide height
 and control a motor that changes the water level in a tank.
 
 Written under v1.0.2 of the Arduino IDE.
 
 The harmonics constants for the tide prediction are taken from 
 the XTide harmonics file. The original harmonics.tcd file is 
 available at 
 http://www.flaterco.com/xtide/files.html
 As with XTide, the predictions generated by this program should
 not be used for navigation, and no accuracy or warranty is given
 or implied for these tide predictions. Hell, the chances are 
 pretty good that the tide predictions generated here are 
 completely wrong.
 
 
*/
//********************************************************************************

// Initial setup
//*******************************
// Header files for talking to real time clock
#include <Wire.h>
#include <SPI.h>  // Required for RTClib to compile properly
#include <RTClib.h> // From https://github.com/MrAlvin/RTClib
#include <SoftwareSerial.h>
// Real Time Clock setup
RTC_DS3231 RTC;     // Uncomment this version if you use the new DS3231 clock 
// RTC_DS1307 RTC;  // Uncomment this version if you use the older DS1307 clock

// Tide calculation library setup
//#include "TideMontereyHarborlib.h"
#include "TidelibSanDiegoSanDiegoBay.h"
TideCalc myTideCalc;  // Create TideCalc object called myTideCalc 


//-----------------------------------------------------------------------------
// ************** HERE IS THE ONLY VALUE YOU NEED TO CHANGE ***************
// ************** HERE IS THE ONLY VALUE YOU NEED TO CHANGE ***************
float upperPos = 5.0; // Upper limit, located at upperLimitSwitch. Units = ft.
// ************** THAT WAS THE ONLY VALUE YOU NEED TO CHANGE ***************
// ************** THAT WAS THE ONLY VALUE YOU NEED TO CHANGE ***************
// So what does changing upperPos do? It sets the maximum tide height that you
// want the tide rack to halt at. Here's an example:
// If you want to recreate the conditions of a mud flat that
// sits at 4.5 ft above the zero tide level, you want the tide rack to travel
// above that height to fully submerge the mud in your tank. We'll round up
// to 5.0 ft for upperPos, so that any high tide above 4.5ft will submerge the
// mud in the tank, up to a depth of 6 inches above the mud.
// (4.5ft + 6 inches = 5.0ft). If the predicted tide goes above 5.0ft, the tide
// rack will hold at its upper travel limit, leaving the mud submerged until 
// the tide drops back below the upperPos value. This all assumes that you have
// set up the tide rack and tank so that the upper travel limit of the tide rack
// is six inches above the mud level in your tanks. 
// ----------------------------------------------------------------------------
// Do not change the value of lowerPos. It is set to accomodate the travel
// limits of the tide rack, which has roughly 2.75 ft between the upper and 
// lower travel limits. 
float lowerPos = upperPos - 2.75; // Lower limit, located at lowerLimitSwitch, units = ft.
// The value for upperPos is taken to be the "home" position, so when ever the
// upperLimitSwitch is activated, the motor is at exactly the value of upperPos.
// In contrast, the value of lowerPos can be less precise, it is just close to 
// the position of where the lowerLimitSwitch will activate, and is used as a 
// backup check to make sure the rack doesn't exceed its travel limits. The 
// lowerLimitSwitch is the main determinant of when the motor stops downward
// travel. This is mainly to accomodate the use of magnetic switches where the
// precise position of activation is hard to determine without lots of trial 
// and error.
float currPos;  // Current position, within limit switch range.    Units = ft.
float results;  // results holds the output from the tide calc.    Units = ft.

int currMinute; // Keep track of current minute value in main loop

//****** 7 Segment LED display setup
#define txPin 7
#define rxPin 4 // not used, but must be defined
SoftwareSerial mySerial = SoftwareSerial(rxPin, txPin);

//---------------------------------------------------------------------------------------
/*  Stepper motor notes
 This uses a Big Easy Driver to control a unipolar stepper motor. This
 takes two input wires: a direction wire and step wire. The direction is
 set by pulling the direction pin high or low. A single step is taken by 
 pulling the step pin high. 
 By default, when you leave MS1, MS2, and MS3 high on the Big Easy Driver, the 
 driver defaults to 1/16 microstep mode. If a full step is 1.8° (200 steps
 per revolution), then microstep mode moves 1/16 of that, 
 (1.8° * 1/16 = 0.1125° per step), or 3200 microsteps per full revolution 
 of the motor. 
 Finally, there is also an Enable pin that can be used to power down the 
 Big Easy Driver between moves, minimizing heat production. 
 */
const byte stepperDir = 8;  // define stepper direction pin. Connect to 
// Big Easy Driver DIR pin
const byte stepperStep = 9; // define stepper step pin. Connect to 
// Big Easy Driver STEP pin.
const byte stepperEnable = 12; // define motor driver enable pin
// Connect to Big Easy Driver Enable pin. Pull high to shut off motor

const int stepDelay = 200; // define delay between steps of the motor. This
// affects the speed at which the motor will turn. Units are microseconds
// For microstepping 1/16 steps, use a value around 200. For full step mode,
// the delay needs to be lengthened to around 5000. 
//*******************************

//-----------------------------------------------------------------------------
// Conversion factor, feet per motor step
// Divide desired travel (in ft.) by this value
// to calculate the number of steps that
// must occur.
const float stepConv = 0.000002604;   // Value for 10 tpi lead screw, microstep
//const float stepConv = 0.00004165;   // Value for 10 tpi lead screw, fullstep
/*  10 tooth-per-inch lead screw = 0.1 inches per revolution
 0.1 inches per rev / 12" = 0.008333 ft per revolution
 0.008333 ft per rev / 200 steps per rev = 0.00004165 ft per step
 Assumes a 200 step per revolution motor being controlled in normal
 stepping mode (not microstepping).
 0.008333 ft per rev / 3200 steps per rev = 0.000002604 ft per step
 Assumes a 200 step per rev stepper motor being controlled in 1/16th
 microstepping mode ( = 3200 steps per revolution).
 We're using feet here because the tide prediction routine outputs
 tide height in units of feet. 
 */

float heightDiff;    // Float variable to hold height difference, in ft.                                    
long stepVal = 0;     // Store the number of steps needed
// to achieve the new height


//---------------------------------------------------------------------------
// Define the digital pin numbers for the limit switches. These will be 
// wired to one lead from a magentic reed switch. The 2nd lead from each reed 
// switch will be wired to ground on the Arduino.
const byte lowerLimitSwitch = 10;
const byte upperLimitSwitch = 11;
// Define digital pin number for the overrideButton
// Pressing this button will run the carriage down, until it hits the lower 
// limit switch
const byte overrideButton = 13; 

// Define highLimitLED and lowLimitLED pins
const byte lowLimitLED = 5;  // On digital pin 5
const byte highLimitLED = 6; // On digital pin 6


//**************************************************************************
// Welcome to the setup loop
void setup(void)
{  
  Wire.begin();
  RTC.begin();
  //--------------------------------------------------
  pinMode(stepperDir, OUTPUT);   // direction pin for Big Easy Driver
  pinMode(stepperStep, OUTPUT);  // step pin for Big Easy driver. One step per rise.
  pinMode(stepperEnable, OUTPUT); // enable pin for Big Easy driver motor power
  digitalWrite(stepperDir, LOW);
  digitalWrite(stepperStep, LOW);
  digitalWrite(stepperEnable, HIGH); // turns off motor power when high  

  // Set up switches and input button as inputs. 
  pinMode(lowerLimitSwitch, INPUT);
  digitalWrite(lowerLimitSwitch, HIGH);  // turn on internal pull-up resistor
  pinMode(upperLimitSwitch, INPUT);
  digitalWrite(upperLimitSwitch, HIGH);  // turn on internal pull-up resistor
  pinMode(overrideButton, INPUT);
  digitalWrite(overrideButton, HIGH);        // turn on internal pull-up resistor
  // When using the internal pull-up resistors for the switches above, the
  // default state for the input pin is HIGH (+5V), and goes LOW (0V) when
  // the switch/button connects to ground. Thus, a LOW value indicates that the
  // button or switch has been activated. 
  
  // Set LED pins as outputs.
  pinMode(lowLimitLED, OUTPUT);
  digitalWrite(lowLimitLED, LOW);
  pinMode(highLimitLED, OUTPUT);
  digitalWrite(highLimitLED, LOW);
  
  // For debugging output to serial monitor
  Serial.begin(115200);
  //************************************
  DateTime now = RTC.now();
  currMinute = now.minute(); // Store current minute value
  printTime(now);  // Call printTime function to print date/time to serial
  Serial.println("Calculating tides for: ");
  Serial.println(myTideCalc.returnStationID());
  // Calculate current tide height
  results = myTideCalc.currentTide(now);
  
  // Set up 7-segment display
  pinMode(txPin, OUTPUT);
  mySerial.begin(9600);
  mySerial.print("v"); // reset display
  mySerial.print("z"); // command byte for brightness control
  mySerial.write(1); // Set display brightness (1 - 254)
  mySerial.print("w"); // command byte for setting decimals/colon
  mySerial.write(2); // turn on 2nd decimal point
  sevenSegDisplay(results); // function to print result to 7-segment display
  delay(4000);
  
  //************************************
  // Spin motor to position rack at the home position (at upperLimitSwitch)
  // The while loop will continue until the upperLimitSwitch is activated 
  // (i.e. driven LOW). 
  Serial.println("Returning to upper limit");
  digitalWrite(stepperEnable, LOW); // turn motor power on
  delay(100);  
  while (digitalRead(upperLimitSwitch) != LOW) {
    // Set motor direction, HIGH = counterclockwise
    digitalWrite(stepperDir, HIGH);
    // Move stepper a single step
    digitalWrite(stepperStep, HIGH);
    delayMicroseconds(stepDelay);
    digitalWrite(stepperStep, LOW);
  }
  digitalWrite(highLimitLED, HIGH); // turn on upper limit LED
  digitalWrite(stepperEnable, HIGH); // turn motor power off  
  currPos = upperPos; // currPos should now equal upperPos
  Serial.print("Current position: ");
  Serial.print(currPos);
  Serial.println(" ft.");
  delay(5000); // pause 5 seconds
  //  Lower carriage to proper height
  // Calculate distance to current tide height
  heightDiff = results - currPos;       // Units of feet.
  stepVal = (long)(heightDiff / stepConv); // convert to motor steps
  if (stepVal < 0) stepVal = -stepVal; // convert negative values
  // If the heightDiff is negative (carriage needs to drop) and the
  // upperLimitSwitch is activated (carriage is at home), move down.
  // This will ignore the case where the current tide height is lower
  // than the programmed lower travel limit. 
  if ( (heightDiff < 0) & (digitalRead(upperLimitSwitch) == LOW)) {
      digitalWrite(highLimitLED, LOW); // turn off upper limit LED
      digitalWrite(stepperEnable, LOW); // turn motor power on
      delay(100);      
      // Set motor direction to move downward
      digitalWrite(stepperDir, LOW);       
      // Run motor the desired number of steps
      for (long steps = 0; steps < stepVal; steps++) {
        digitalWrite(stepperStep, HIGH);
        delayMicroseconds(stepDelay);
        digitalWrite(stepperStep, LOW);
        // check lowerLimitSwitch each step, quit if activated
        if (digitalRead(lowerLimitSwitch) == LOW)  {
          // Update current position value
          currPos = currPos - (steps * stepConv);
          Serial.println("Hit lower limit switch");
          digitalWrite(lowLimitLED, HIGH); // turn on lowLimitLED
          break;  // break out of for loop
        }
      }
      // If the lowerLimitSwitch wasn't activated, then the motor
      // stopped above the limit switch. Set currPos to whatever
      // value is in 'results' currently. 
      if (digitalRead(lowerLimitSwitch) == HIGH) {  //switch not triggered
        currPos = results; // set currPos equal to current tide height
      }
      digitalWrite(stepperEnable, HIGH); // turn motor power off     
  }
  // Signal that main loop is starting
  mySerial.write(0x76); // reset position to start
  mySerial.write("RUn");// spell "Run" on 7-segment display
  
  Serial.print("Current position: ");
  Serial.print(currPos,2);
  Serial.println(" ft.");
  delay(5000);
}  // End of setup loop.

//**************************************************************************
// Welcome to the Main loop
void loop(void)
{
  // Get current time, store in object 'now'
  DateTime now = RTC.now();

  // If it is the start of a new minute, calculate new tide height and
  // adjust motor position
  if (now.minute() != currMinute) {
    // If now.minute doesn't equal currMinute, a new minute has turned
    // over, so it's time to update the tide height. We only want to do
    // this once per minute. 
    currMinute = now.minute();                   // update currMinute

    
    Serial.println();
    printTime(now);
    Serial.print("Previous tide ht: ");
    Serial.print(results,4);
    Serial.println(" ft.");   
    Serial.print("Previous position: ");
    Serial.print(currPos);
    Serial.println(" ft.");
    // Calculate new tide height based on current time
    results = myTideCalc.currentTide(now);
    sevenSegDisplay(results); // function to print result to 7-segment display
    // Calculate height difference between currPos and
    // new tide height. Value may be positive or negative depending on
    // direction of the tide. Positive values mean the water level
    // needs to be raised to meet the new predicted tide level.
    heightDiff = results - currPos;       // Units of feet.
    // Convert heightDiff into number of steps that must pass
    // to achieve the new height. The result is cast as an unsigned 
    // long integer.
    stepVal = (long)(heightDiff / stepConv);
    if (stepVal < 0) stepVal = -stepVal; // convert negative values


    //********************************
    // For debugging
    Serial.print("Height diff: ");
    Serial.print(heightDiff, 5);
    Serial.println(" ft.");
    Serial.print("stepVal calc: ");
    Serial.println(stepVal);
    Serial.print("Target height: ");
    Serial.print(results, 4);
    Serial.println(" ft.");
    Serial.println(); // blank line
    //******************************************************************
    // Motor movement section
    // ************** Lower water level to new position ****************
    // If the heightDiff is negative, AND the target level is less than 
    // the upperPos limit, AND the target level is greater than the 
    // lowerPos limit AND the lowerLimitSwitch 
    // hasn't been activated, then the motor can be moved downward. 
    if ( (heightDiff < 0) & (results < upperPos) & 
      (results > lowerPos) & (digitalRead(lowerLimitSwitch) == HIGH) )
    {
      digitalWrite(stepperEnable, LOW); // turn motor power on
      delay(100);      
      // Set motor direction to move downward
      digitalWrite(stepperDir, LOW);       
      // Run motor the desired number of steps
      for (unsigned int steps = 0; steps < stepVal; steps++) {
        digitalWrite(stepperStep, HIGH);
        delayMicroseconds(stepDelay);
        digitalWrite(stepperStep, LOW);
        // check lowerLimitSwitch each step, quit if it is activated
        if (digitalRead(lowerLimitSwitch) == LOW) {
          // Calculate how far the motor managed to turn before
          // hitting the lower limit switch, record that as the new
          // currPos value.
          currPos = currPos - (steps * stepConv);
          Serial.println("Hit lower limit switch");
          digitalWrite(lowLimitLED, HIGH);
          break;  // break out of for loop
        }
      }
      delay(100);
      digitalWrite(stepperEnable, HIGH); // turn motor power off      
      if (digitalRead(lowerLimitSwitch) == HIGH) {
        // If the lower limit wasn't reached, then the currPos should
        // be equal to the new water level stored in 'results'.
        currPos = results;
      }
      digitalWrite(highLimitLED, LOW);  // highLimitLED should be off     
    }
    // ************Raise water level to new position******************
    // If the heightDiff is positive, AND the target level is greater 
    // than the lowerPos limit, AND the target level is less than the 
    // upperPos limit (plus a 0.2 ft buffer), AND the upperLimitSwitch 
    // hasn't been activated, then the motor can be moved.
    else if ( (heightDiff > 0) & (results > lowerPos) & 
      (results < (upperPos + 0.2)) & (digitalRead(upperLimitSwitch) == HIGH) )
    {
      digitalWrite(stepperEnable, LOW); // turn on motor power
      delay(100);      
      // Set motor direction in reverse
      digitalWrite(stepperDir, HIGH);
      // Run motor the desired number of steps
      for (unsigned int steps = 0; steps < stepVal; steps++) {
        digitalWrite(stepperStep, HIGH);
        delayMicroseconds(stepDelay);
        digitalWrite(stepperStep, LOW);
        // check upperLimitSwitch each step, quit if it is activated
        if (digitalRead(upperLimitSwitch) == LOW) {
          // Since the upper limit switch is the "home" position
          // we assume that the currPos = upperPos when the
          // upperLimitSwitch is activated.
          currPos = upperPos;
          Serial.println("Hit upper limit switch");
          digitalWrite(highLimitLED, HIGH);  // turn on high limit LED
          break;  // break out of for loop
        }
      }
      delay(100);
      digitalWrite(stepperEnable, HIGH); // turn off motor power      
      if (digitalRead(upperLimitSwitch) == HIGH) {
        // If the upper limit wasn't reached, then currPos should
        // be equal to the new water level stored in 'results'.
        currPos = results;
      }
      digitalWrite(lowLimitLED, LOW); // Turn off low limit LED if it's on.   
    }
    // End of motor movement section. If the current tide ht is outside
    // either of the travel limits, then there should be no motor 
    // movement, and the currPos value should not be changed. 
    //*******************************************************************
    if (digitalRead(upperLimitSwitch) == LOW) {
      Serial.println("At upper limit, no movement");
      Serial.println();
      digitalWrite(highLimitLED, HIGH);
      digitalWrite(lowLimitLED, LOW);
    }
    if (digitalRead(lowerLimitSwitch) == LOW) {
      Serial.println("At lower limit, no movement");
      Serial.println();
      digitalWrite(lowLimitLED, HIGH);
      digitalWrite(highLimitLED, LOW);
    }
    // The lower limit is a bit more flexible depending on 
    // lower reed switch placement and lowerPos value, so if
    // tide height is less than lowerPos (but the carriage hasn't
    // reached the lower limit switch), go ahead and turn 
    // lower limit LED on to show user that we've reached 
    // the lower limit. 
    if (results < lowerPos) {
      digitalWrite(lowLimitLED, HIGH); // turn LED on
    }
  }    // End of if (now.minute() != currMinute) statement

  // ************* Position override routine ******************
  // Moving the carriage down is a pain to do by hand, so the user
  // can press a button to drive the carriage down with the motor.
  // The program will pause for 10 seconds after the movement, in
  // case the user wants to unplug the power to keep the carriage at
  // its new position. Remember that turning the Arduino off and on
  // again will cause it to run the motor up to its highest position
  // during the setup routine, if you want to raise the tide rack 
  // instead of lower it. 
  if ( (digitalRead(overrideButton) == LOW) & 
    (digitalRead(lowerLimitSwitch) == HIGH) ) {
      digitalWrite(stepperEnable, LOW); // turn on motor power
      delay(100);     
    // Set motor direction to move downward
    digitalWrite(stepperDir, LOW);
    while( (digitalRead(overrideButton) == LOW) & 
      (digitalRead(lowerLimitSwitch) == HIGH)) {
        // go through 2400 steps = 0.75 revolutions = 0.075 inches = .00625ft
        // go through 150 steps = 0.75 revolutions = 0.075 inches = .00625ft
      for (int steps = 0; steps <= 150; steps++) {
        digitalWrite(stepperStep, HIGH);
        delayMicroseconds(stepDelay);
        digitalWrite(stepperStep, LOW);
      }
      // Update currPos to reflect new position
      currPos = currPos - 0.00625;
      if (digitalRead(lowerLimitSwitch) == LOW) {
        digitalWrite(lowLimitLED, HIGH); // turn on lower limit LED
        digitalWrite(stepperEnable, HIGH); // turn off motor power
      }
    }
    digitalWrite(stepperEnable, HIGH); // turn off motor power
    delay(10000); // Give user time to power down Arduino to hold position
    // If the Arduino stays powered up, it should try to return the 
    // carriage to the correct height based on the difference between the
    // new value of currPos and the tide height.
  }
} // End of main loop


//******************************************************
// Function for printing the current date/time to the 
// serial port in a nicely formatted layout.
void printTime(DateTime now) {
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  if (now.minute() < 10) {
    Serial.print("0");
    Serial.print(now.minute());
  }
  else if (now.minute() >= 10) {
    Serial.print(now.minute());
  }
  Serial.print(':');
  if (now.second() < 10) {
    Serial.print("0");
    Serial.println(now.second());
  }
  else if (now.second() >= 10) {
    Serial.println(now.second());
  }
}
//********************************************************

//********************************************************
// Function to print tide height to 7-segment 4-digit display
// from Sparkfun. This takes a tide height float value and 
// multiplies it by 100, then converts to an int to remove the
// decimal point. The converted value (now 2 or 3 digits) is
// displayed, with the negative sign if necessary.
void sevenSegDisplay(float result) {
  mySerial.print("w");  //decimal point control command
  mySerial.write(2); // set 2nd decimal point
  // multiply tide height by 100 to get rid of decimal point
 int noDecimal = (int)(result * 100);
 // Print out negative tide height values
 if (noDecimal < 0) { // Negative tide heights
  mySerial.write(0x76); // reset position to start
  mySerial.write("-"); // print negative sign
  noDecimal = noDecimal * -1; // remove negative sign of noDecimal
  if (noDecimal > -100) { // values between 0 and -1, only 2 decimal places
    mySerial.print(0); // print zero before decimal point
    mySerial.print(noDecimal); // print last two digits
  }
  else if(noDecimal <= -100) { // values less than -1
    mySerial.print(noDecimal); // print 3-digit value
  }
 }
 else if (noDecimal >= 0) { // Positive tide heights
   mySerial.write(0x76); // reset position to start
   mySerial.print("x"); // print blank space in 1st position
   if (noDecimal < 100) { // values between 0.00 and 0.99 (0 and 99)
     mySerial.print(0); // print zero before decimal point
     mySerial.print(noDecimal); // print last two digits
   }
   else if (noDecimal >= 100) { // values greater than 1.00 (100)
     mySerial.print(noDecimal); // print 3-digit value
   }
 }
}
