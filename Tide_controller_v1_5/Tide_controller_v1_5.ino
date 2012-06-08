/* Tide_controller_v1.5
  Copyright (C) 2012 Luke Miller
 This version is set up to work on a lead-screw driven rack that has
 a limited travel range. There should be a limit switch at each end 
 of the rack's travel, and the distance between the values for 
 upperPos and lowerPos must be equal to the distance between those 
 limit switches. Tidal harmonic constituents have been moved to
 program memory, allowing more years of data to be stored.
 
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
 
 Written under v1.0.1 of the Arduino IDE.
 
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
#include <avr/pgmspace.h>    // Needed to store values in PROGMEM
// Header files for talking to real time clock
#include <Wire.h>
#include <RTClib.h>  // Available at https://github.com/adafruit/RTClib
// Real Time Clock setup
RTC_DS1307 RTC;      // This line remains the same even if you use the DS3231 chip
unsigned int YearIndx = 0;    // Used to index rows in the Equilarg/Nodefactor arrays
float currHours = 0;          // Elapsed hours since start of year
const int adjustGMT = 8;     // Time zone adjustment to get time in GMT. Make sure this is
// correct for the local standard time of the tide station. 
// No daylight savings time adjustments should be made. 
// 8 = Pacific Standard Time (America/Los_Angeles)

int currMinute; // Keep track of current minute value in main loop
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
 
 */
const int stepperDir = 8;  // define stepper direction pin. Connect to 
// Big Easy Driver DIR pin
const int stepperStep = 9; // define stepper step pin. Connect to 
// Big Easy Driver STEP pin.

//*******************************

//*******************************
//----------------------------------------------------------------------------------
/* Initialize harmonic constant arrays. These each hold 37 values for
   the tide site that was extracted using the R scripts 
   'tide_harmonics_parse.R' and 'tide_extract_1site_harmonics_progmem.R'. The 2nd
   script outputs a text file that contains code that can be copied and pasted
   into the section below. 
   If you wish to make predictions for a different site, it will be necessary to
   replace the Amp and Kappa values with values for your site. Either use the R
   scripts to get data for a new site, or get the new site values from NOAA.
   These are available from NOAA's http://tidesandcurrent.noaa.gov site.
   Kappa here is referred to as "Phase" on NOAA's site. The order of the
   constants is shown below in the names. Unfortunately this does not match
   NOAA's order, so you will have to rearrange NOAA's values if you want to 
   put new site values in here.
   The Speed, Equilarg and Nodefactor arrays can all stay the same for any 
   site. 
   
*/
//-----------------------------------------------------
// Begin pasted section from R script "tide_extract_1site_harmonics_progmem.R"
// Selected station:  Monterey, Monterey Harbor, California 
// The 'datum' printed here is the difference between mean sea level and 
// mean lower low water for the NOAA station. These two values can be 
// found for NOAA tide reference stations on the tidesandcurrents.noaa.gov
//  site under the datum page for each station.
const float Datum = 2.8281 ; // units in feet
// Harmonic constant names: J1, K1, K2, L2, M1, M2, M3, M4, M6, M8, N2, 2N2, O1, OO1, P1, Q1, 2Q1, R2, S1, S2, S4, S6, T2, LDA2, MU2, NU2, RHO1, MK3, 2MK3, MN4, MS4, 2SM2, MF, MSF, MM, SA, SSA
// These names match the NOAA names, except LDA2 here is LAM2 on NOAA's site
// Amp scaled by 1000, divide by 1000 to convert to original float value
PROGMEM prog_uint16_t Amp[] = {71,1199,121,23,38,1616,0,0,0,0,368,44,753,36,374,134,16,3,33,428,0,0,22,11,41,72,26,0,0,0,0,0,0,0,0,157,90};
// Kappa scaled by 10, so divide by 10 to convert to original float value
PROGMEM prog_uint16_t Kappa[] = {2334,2198,1720,2202,2259,1811,0,0,0,0,1546,1239,2034,2502,2156,1951,1994,1802,3191,1802,0,0,1678,1807,1146,1611,1966,0,0,0,0,0,0,0,0,2060,2839};
// Speed is unscaled, stored as the original float values
typedef float PROGMEM prog_float_t; // Need to define this type before use
PROGMEM prog_float_t Speed[] = {15.58544,15.04107,30.08214,29.52848,14.49669,28.9841,43.47616,57.96821,86.95231,115.9364,28.43973,27.89535,13.94304,16.1391,14.95893,13.39866,12.85429,30.04107,15,30,60,90,29.95893,29.45563,27.96821,28.51258,13.47151,44.02517,42.92714,57.42383,58.9841,31.0159,1.098033,1.015896,0.5443747,0.0410686,0.0821373};
// Equilarg scaled by 100. Divide by 100 to get original value.
PROGMEM prog_uint16_t Equilarg[10][37] = { 
{17495,1851,21655,15782,23131,19425,29137,2850,22275,5700,4193,24962,17162,5364,34993,1930,22699,17692,18000,0,0,0,308,2959,2660,17891,15628,21276,999,23618,19425,16575,3101,16575,15232,28007,20013},
{27537,1770,21457,34814,15957,27019,22529,18038,9058,77,1609,12198,24892,33361,34919,35482,10072,17765,18000,0,0,0,235,28737,17891,7302,5175,28789,16269,28627,27019,8981,31235,8981,25410,28081,20162},
{2,1486,20897,18274,7278,1035,19553,2071,3106,4142,2753,4470,35316,22123,34943,1033,2751,17739,18000,0,0,0,261,19806,1982,265,34546,2521,585,3788,1035,34965,20404,34965,34283,28057,20115},
{8338,1130,20240,366,32299,11042,16563,22084,33126,8168,3886,32732,9858,10509,34967,2703,31549,17714,18000,0,0,0,286,10865,22064,29219,28036,12172,20954,14929,11042,24958,9325,24958,7155,28033,20067},
{16642,756,19561,18209,21144,21046,13569,6092,27139,12185,5019,24992,20431,34800,34990,4404,24377,17688,18000,0,0,0,312,1921,6144,22171,21556,21802,5336,26064,21046,14954,34184,14954,16027,28010,20019},
{26433,526,19135,196,10240,28618,6927,21236,13854,6472,2412,12206,28391,26062,34916,2185,11979,17761,18000,0,0,0,239,27677,21353,11559,11332,29144,20711,31030,28618,7381,25836,7381,26206,28083,20169},
{34975,287,18644,20217,4275,2641,3961,5282,7923,10564,3563,4485,2745,15047,34940,3665,4588,17735,18000,0,0,0,265,18753,5451,4529,4633,2928,4995,6204,2641,33359,15152,33359,35078,28060,20121},
{7692,158,18341,3983,31462,12683,1024,25365,2048,14731,4732,32782,12941,4542,34963,4991,33040,17710,18000,0,0,0,290,9847,25568,33519,33777,12841,25207,17415,12683,23317,4800,23317,7950,28037,20073},
{16567,137,18252,21135,20658,22747,34121,9494,32241,18988,5924,25102,23002,30487,34987,6180,25357,17684,18000,0,0,0,316,964,9708,26530,26786,22884,9357,28670,22747,13253,30742,13253,16823,28013,20025},
{26975,307,18562,1726,9025,30397,27595,24795,19192,13590,3396,12394,30424,23522,34913,3422,12421,17757,18000,0,0,0,243,26798,24995,15997,16024,30704,24488,33793,30397,5603,23549,5603,27001,28087,20175} 
 };

// Nodefactor scaled by 10000. Divide by 10000 to get original float value.
PROGMEM prog_uint16_t Nodefactor[10][37] = { 
{9491,9602,8878,11647,11232,10169,10257,10343,10519,10698,10169,10169,9349,7887,10000,9349,9349,10000,10000,10000,10000,10000,10000,10169,10169,10169,9349,9765,9931,10343,10169,10169,8592,10169,10577,10000,10000},
{8928,9234,8156,12048,8780,10271,10411,10552,10839,11134,10271,10271,8748,6334,10000,8748,8748,10000,10000,10000,10000,10000,10000,10271,10271,10271,8748,9485,9743,10552,10271,10271,7448,10271,10936,10000,10000},
{8492,8957,7680,10216,13201,10344,10520,10699,11067,11448,10344,10344,8290,5315,10000,8290,8290,10000,10000,10000,10000,10000,10000,10344,10344,10344,8290,9265,9583,10699,10344,10344,6642,10344,11190,10000,10000},
{8278,8824,7472,8780,15575,10377,10571,10768,11173,11594,10377,10377,8068,4868,10000,8068,8068,10000,10000,10000,10000,10000,10000,10377,10377,10377,8068,9156,9501,10768,10377,10377,6271,10377,11307,10000,10000},
{8343,8864,7533,9704,14048,10367,10556,10747,11141,11550,10367,10367,8135,5000,10000,8135,8135,10000,10000,10000,10000,10000,10000,10367,10367,10367,8135,9189,9526,10747,10367,10367,6381,10367,11272,10000,10000},
{8669,9068,7865,11656,9653,10315,10477,10641,10976,11323,10315,10315,8475,5711,10000,8475,8475,10000,10000,10000,10000,10000,10000,10315,10315,10315,8475,9354,9650,10641,10315,10315,6961,10315,11090,10000,10000},
{9176,9394,8458,12040,9343,10229,10345,10463,10702,10947,10229,10229,9011,6981,10000,9011,9011,10000,10000,10000,10000,10000,10000,10229,10229,10229,9011,9609,9829,10463,10229,10229,7936,10229,10783,10000,10000},
{9761,9782,9272,9582,16115,10117,10176,10235,10354,10475,10117,10117,9643,8745,10000,9643,9643,10000,10000,10000,10000,10000,10000,10117,10117,10117,9643,9897,10012,10235,10117,10117,9188,10117,10390,10000,10000},
{10336,10176,10225,7337,19813,9992,9989,9985,9977,9969,9992,9992,10279,10859,10000,10279,10279,10000,10000,10000,10000,10000,10000,9992,9992,9992,10279,10168,10160,9985,9992,9992,10571,9992,9955,10000,10000},
{10835,10529,11201,10649,15935,9870,9805,9741,9614,9489,9870,9870,10850,13090,10000,10850,10850,10000,10000,10000,10000,10000,10000,9870,9870,9870,10850,10391,10257,9741,9870,9870,11923,9870,9530,10000,10000} 
 };

// Define unix time values for the start of each year.
//                                      2012       2013       2014       2015       2016       2017       2018       2019       2020       2021
PROGMEM prog_uint32_t startSecs[] = {1325376000,1356998400,1388534400,1420070400,1451606400,1483228800,1514764800,1546300800,1577836800,1609459200};

// 1st year of data in the Equilarg/Nodefactor/startSecs arrays.
const unsigned int startYear = 2012;
//------------------------------------------------------------------


// Define some variables that will hold float-converted versions of the constants above
float currAmp;
float currSpeed;
float currNodefactor;
float currEquilarg;
float currKappa;
//-----------------------------------------------------------------------------
float upperPos = 5.3; // Upper limit, located at upperLimitSwitch. Units = ft.
float lowerPos = 2.3; // Lower limit, located at lowerLimitSwitch. Units = ft.
float currPos;  // Current position, within limit switch range.    Units = ft.
float results;  // results holds the output from the tide calc.    Units = ft.
// The value for upperPos is taken to be the "home" position, so when ever the
// upperLimitSwitch is activated, the motor is at exactly the value of upperPos.
// In contrast, the value of lowerPos can be less precise, it is just close to 
// the position of where the lowerLimitSwitch will activate, and is used as a 
// backup check to make sure the motor doesn't exceed its travel limits. The 
// lowerLimitSwitch is the main determinant of when the motor stops downward
// travel. This is mainly to accomodate the use of magnetic switches where the
// precise position of activation is hard to determine without lots of trial 
// and error.

//-----------------------------------------------------------------------------
// Conversion factor, feet per motor step
// Divide desired travel (in ft.) by this value
// to calculate the number of steps that
// must occur.
const float stepConv = 0.000002604;   // Value for 10 tpi lead screw
/*  10 tooth-per-inch lead screw = 0.1 inches per revolution
 0.1 inches per rev / 12" = 0.008333 ft per revolution
 0.008333 ft per rev / 3200 steps per rev = 0.000002604 ft per step
 Assumes a 200 step per rev stepper motor being controlled in 1/16th
 microstepping mode ( = 3200 steps per revolution).
 We're using feet here because the tide prediction routine outputs
 tide height in units of feet. 
 */

float heightDiff;    // Float variable to hold height difference, in ft.                                    
long stepVal = 0;     // Store the number of steps needed
// to achieve the new height
long counts = 0;       // Store the number of steps that have
// gone by so far.

//---------------------------------------------------------------------------
// Define the digital pin numbers for the limit switches. These will be 
// wired to one lead from a magentic reed switch. The 2nd lead from each reed 
// switch will be wired to ground on the Arduino.
const int lowerLimitSwitch = 10;
const int upperLimitSwitch = 11;
// Define digital pin number for the homeButton
// Pressing this button will run the motor until it hits the upper limit switch
const int homeButton = 12; 

//**************************************************************************
// Welcome to the setup loop
void setup(void)
{  
  Wire.begin();
  RTC.begin();
  //--------------------------------------------------
  pinMode(stepperDir, OUTPUT);   // direction pin for Big Easy Driver
  pinMode(stepperStep, OUTPUT);  // step pin for Big Easy driver. One step per rise.
  digitalWrite(stepperDir, LOW);
  digitalWrite(stepperStep, LOW);

  // Set up switches and input button as inputs. 
  pinMode(lowerLimitSwitch, INPUT);
  digitalWrite(lowerLimitSwitch, HIGH);  // turn on internal pull-up resistor
  pinMode(upperLimitSwitch, INPUT);
  digitalWrite(upperLimitSwitch, HIGH);  // turn on internal pull-up resistor
  pinMode(homeButton, INPUT);
  digitalWrite(homeButton, HIGH);        // turn on internal pull-up resistor
  // When using the internal pull-up resistors for the switches above, the
  // default state for the input pin is HIGH (+5V), and goes LOW (0V) when
  // the switch/button connects to ground. Thus, a LOW value indicates that the
  // button or switch has been activated. 


  // For debugging output to serial monitor
  Serial.begin(115200);
  //************************************
  DateTime now = RTC.now();
  currMinute = now.minute(); // Store current minute value
  printTime(now);  // Call printTime function to print date/time to serial
  delay(4000);

  //************************************
  // Spin motor to position slide at the home position (at upperLimitSwitch)
  // The while loop will continue until the upperLimitSwitch is activated 
  // (i.e. driven LOW). 
  Serial.println("Returning to upper limit");
  while (digitalRead(upperLimitSwitch) != LOW) {
    // Set motor direction, HIGH = counterclockwise
    digitalWrite(stepperDir, HIGH);
    // Move stepper a single step
    digitalWrite(stepperStep, HIGH);
    delayMicroseconds(100);
    digitalWrite(stepperStep, LOW);
  }
  currPos = upperPos; // currPos should now equal upperPos
  Serial.print("Current position: ");
  Serial.print(currPos,2);
  Serial.println(" ft.");
  delay(2000);
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

      // Calculate difference between current year and starting year.
    YearIndx = now.year() - startYear;
    // Calculate hours since start of current year. Hours = seconds / 3600
    currHours = (now.unixtime() - pgm_read_dword_near(&startSecs[YearIndx])) / float(3600);
    // Shift currHours to Greenwich Mean Time
    currHours = currHours + adjustGMT;
    Serial.println();
    printTime(now);
    Serial.print("Previous tide ht: ");
    Serial.print(results,3);
    Serial.println(" ft.");   
    // *****************Calculate current tide height*************
    results = Datum; // initialize results variable, units of feet.
    for (int harms = 0; harms < 37; harms++) {
      // Many of the constants are stored as unsigned integers to 
      // save space. These steps convert them back to their real values.
      currNodefactor = pgm_read_word_near(&Nodefactor[YearIndx][harms]) / float(10000);
      currAmp = pgm_read_word_near(&Amp[harms]) / float(1000);
      currEquilarg = pgm_read_word_near(&Equilarg[YearIndx][harms]) / float(100);
      currKappa = pgm_read_word_near(&Kappa[harms]) / float(10);
      currSpeed = pgm_read_float_near(&Speed[harms]); // Speed was not scaled to integer

      // Calculate each component of the overall tide equation 
      // The currHours value is assumed to be in hours from the start of the
      // year, in the Greenwich Mean Time zone, not the local time zone.
      // There is no daylight savings time adjustment here.  
      results = results + (currNodefactor * currAmp * 
        cos( (currSpeed * currHours + currEquilarg - currKappa) * DEG_TO_RAD));
    }
    //******************End of Tide Height calculation*************

    // Calculate height difference between currPos and
    // new tide height. Value may be positive or negative depending on
    // direction of the tide. Positive values mean the water level
    // needs to be raised to meet the new predicted tide level.
    heightDiff = results - currPos;       // Units of feet.
    // Convert heightDiff into number of steps that must pass
    // to achieve the new height. The result is cast as an unsigned 
    // long integer.
    stepVal = (long)(heightDiff / stepConv);
    stepVal = abs(stepVal);  // remove negative sign if present


    //********************************
    // For debugging
    Serial.print("Height diff: ");
    Serial.print(heightDiff, 3);
    Serial.println(" ft.");
    Serial.print("stepVal calc: ");
    Serial.println(stepVal);
    Serial.print("Target height: ");
    Serial.print(results, 3);
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
      // Set motor direction to move downward
      digitalWrite(stepperDir, LOW);       
      // Run motor the desired number of steps
      for (int steps = 0; steps < stepVal; steps++) {
        digitalWrite(stepperStep, HIGH);
        delayMicroseconds(100);
        digitalWrite(stepperStep, LOW);
        // check lowerLimitSwitch each step, quit if it is activated
        if (digitalRead(lowerLimitSwitch) == LOW) {
          // Calculate how far the motor managed to turn before
          // hitting the lower limit switch, record that as the new
          // currPos value.
          currPos = currPos - (steps * stepConv);
          Serial.println("Hit lower limit switch");
          break;  // break out of for loop
        }
      }
      if (digitalRead(lowerLimitSwitch) == HIGH) {
        // If the lower limit wasn't reached, then the currPos should
        // be equal to the new water level stored in 'results'.
        currPos = results;
      }       
    }
    // ************Raise water level to new position******************
    // If the heightDiff is positive, AND the target level is greater 
    // than the lowerPos limit, AND the target level is less than the 
    // upperPos limit (plus a 0.025ft buffer), AND the upperLimitSwitch 
    // hasn't been activated, then the motor can be moved.
    else if ( (heightDiff > 0) & (results > lowerPos) & 
      (results < (upperPos + 0.025)) & (digitalRead(upperLimitSwitch) == HIGH) )
    {
      // Set motor direction in reverse
      digitalWrite(stepperDir, HIGH);
      // Run motor the desired number of steps
      for (int steps = 0; steps < stepVal; steps++) {
        digitalWrite(stepperStep, HIGH);
        delayMicroseconds(100);
        digitalWrite(stepperStep, LOW);
        // check upperLimitSwitch each step, quit if it is activated
        if (digitalRead(upperLimitSwitch) == LOW) {
          // Since the upper limit switch is the "home" position
          // we assume that the currPos = upperPos when the
          // upperLimitSwitch is activated.
          currPos = upperPos;
          Serial.println("Hit upper limit switch");
          break;  // break out of for loop
        }
      }
      if (digitalRead(upperLimitSwitch) == HIGH) {
        // If the upper limit wasn't reached, then currPos should
        // be equal to the new water level stored in 'results'.
        currPos = results;
      }     
    }
    // End of motor movement section. If the current tide ht is outside
    // either of the travel limits, then there should be no motor 
    // movement, and the currPos value should not be changed. 
    //*******************************************************************
    if (digitalRead(upperLimitSwitch) == LOW) {
      Serial.println("At upper limit switch, no movement");
      Serial.println();
    }
    if (digitalRead(lowerLimitSwitch) == LOW) {
      Serial.println("At lower limit switch, no movement");
      Serial.println();
    }
  }    // End of if (now.minute() != currMinute) statement

  // TODO: implement return-to-home-position routine.

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

