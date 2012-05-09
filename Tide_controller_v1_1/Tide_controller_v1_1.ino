/* Tide_controller_v1.1 
  Copyright (C) 2012 Luke Miller
  
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

  Written under v1.0 of the Arduino IDE.
  
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

//#include <mySoftwareSerial.h>
//#include <PololuQik.h>
//#include <myPololuWheelEncoders.h>
#include <Wire.h>
#include <RTClib.h>

// Initialize harmonic constant arrays. These each hold 37 values for
// the tide site that was extracted using the R scripts. If you wish
// to make predictions for a different site, it will be necessary to
// replace the Amp and Kappa values with values particular to your
// site. These are available from NOAA's http://tidesandcurrent.noaa.gov site.
// Kappa here is referred to as "Phase" on NOAA's site. The order of the
// constants is shown below in the names. Unfortunately this does not match
// NOAA's order, so you will have to rearrange NOAA's values if you want to 
// put new site values in here.
// The Speed, Equilarg and Nodefactor arrays can all stay the same for any 
// site. 
// All tide predictions are output in Greenwich Mean Time. 

// Selected station:  San Diego, San Diego Bay, California 
const float Datum = 2.9396 ; // units in feet
// Harmonic constant names: J1, K1, K2, L2, M1, M2, M3, M4, M6, M8, N2, 2N2, O1, OO1, P1, Q1, 2Q1, R2, S1, S2, S4, S6, T2, LDA2, MU2, NU2, RHO1, MK3, 2MK3, MN4, MS4, 2SM2, MF, MSF, MM, SA, SSA
// These names match the NOAA names, except LDA2 here is LAM2 on NOAA's site
const float Amp[] = {0.066,1.137,0.221,0.049,0.043,1.824,0.011,0,0.014,0,0.426,0.053,0.723,0.036,0.357,0.135,0.014,0.006,0.013,0.751,0,0,0.047,0.01,0.051,0.083,0.028,0,0,0,0,0,0,0,0,0.227,0};
const float Kappa[] = {220.6,208,134.3,144,220.4,143.2,346.1,0,61.4,0,123.8,97.5,192.4,241.1,205.5,184.7,182.1,140.1,338.8,140.2,0,0,127.3,128.8,93,128.8,184.3,0,0,0,0,0,0,0,0,179.3,0};
const float Speed[] = {15.58544,15.04107,30.08214,29.52848,14.49669,28.9841,43.47616,57.96821,86.95231,115.9364,28.43973,27.89535,13.94304,16.1391,14.95893,13.39866,12.85429,30.04107,15,30,60,90,29.95893,29.45563,27.96821,28.51258,13.47151,44.02517,42.92714,57.42383,58.9841,31.0159,1.098033,1.015896,0.5443747,0.0410686,0.0821373};
const float Equilarg[4][37] = { 
{174.95,18.51,216.55,157.82,231.31,194.25,291.37,28.5,222.75,57,41.93,249.62,171.62,53.64,349.93,19.3,226.99,176.92,180,0,0,0,3.08,29.59,26.6,178.91,156.28,212.76,9.99,236.18,194.25,165.75,31.01,165.75,152.32,280.07,200.13},
{275.37,17.7,214.57,348.14,159.57,270.19,225.29,180.38,90.58,0.77,16.09,121.98,248.92,333.61,349.19,354.82,100.72,177.65,180,0,0,0,2.35,287.37,178.91,73.02,51.75,287.89,162.69,286.28,270.19,89.81,312.35,89.81,254.1,280.81,201.62},
{0.02,14.86,208.97,182.74,72.78,10.35,195.53,20.71,31.06,41.42,27.53,44.7,353.16,221.23,349.43,10.33,27.51,177.39,180,0,0,0,2.61,198.06,19.83,2.65,345.46,25.21,5.85,37.88,10.35,349.65,204.04,349.65,342.83,280.57,201.15},
{83.38,11.3,202.4,3.66,322.99,110.42,165.63,220.84,331.26,81.68,38.87,327.32,98.58,105.09,349.67,27.03,315.49,177.14,180,0,0,0,2.86,108.65,220.64,292.19,280.36,121.72,209.54,149.29,110.42,249.58,93.25,249.58,71.55,280.33,200.67} 
 };

const float Nodefactor[4][37] = { 
{0.9491,0.9602,0.8878,1.1647,1.1232,1.017,1.0257,1.0343,1.0519,1.0698,1.017,1.017,0.9349,0.7887,1,0.9349,0.9349,1,1,1,1,1,1,1.017,1.017,1.017,0.9349,0.9765,0.9931,1.0343,1.017,1.017,0.8592,1.017,1.0577,1,1},
{0.8928,0.9234,0.8156,1.2048,0.878,1.0272,1.0411,1.0552,1.0839,1.1134,1.0272,1.0272,0.8748,0.6334,1,0.8748,0.8748,1,1,1,1,1,1,1.0272,1.0272,1.0272,0.8748,0.9485,0.9743,1.0552,1.0272,1.0272,0.7448,1.0272,1.0937,1,1},
{0.8492,0.8957,0.768,1.0216,1.3201,1.0344,1.052,1.0699,1.1067,1.1448,1.0344,1.0344,0.829,0.5315,1,0.829,0.829,1,1,1,1,1,1,1.0344,1.0344,1.0344,0.829,0.9265,0.9583,1.0699,1.0344,1.0344,0.6642,1.0344,1.119,1,1},
{0.8278,0.8824,0.7472,0.878,1.5575,1.0377,1.0571,1.0768,1.1173,1.1594,1.0377,1.0377,0.8068,0.4868,1,0.8068,0.8068,1,1,1,1,1,1,1.0377,1.0377,1.0377,0.8068,0.9156,0.9501,1.0768,1.0377,1.0377,0.6271,1.0377,1.1307,1,1} 
 };


/*
Required connections between Arduino and qik 2s9v1:

      Arduino    qik 2s9v1
           5V -> VCC
          GND -> GND
Digital Pin 8 -> TX pin on 2s9v1 (optional if you don't need talk-back from the unit) 
Digital Pin 9 -> RX pin on 2s9v1
Digital Pin 10 -> RESET
*/
//PololuQik2s9v1 qik(8, 9, 10);

//PololuWheelEncoders encoder;

RTC_DS1307 RTC;
unsigned int YearIndx = 0;    // Used to index rows in the Equilarg/Nodefactor arrays
const unsigned int startYear = 2012;  // 1st year in the Equilarg/Nodefactor datasets
float currHours = 0;          // Elapsed hours since start of year
const int adjustGMT = 8;     // Time zone adjustment to get time in GMT

// Define unixtime values for the start of each year
//                               2012        2013        2014        2015
unsigned long startSecs[] = {1325376000, 1356998400, 1388534400, 1420070400};


long Total = 0;  // Total turns during this actuation
float TotalTurns = 0; // Total turns overall (i.e. current position)
int secs = 0; // Keep track of previous seconds value in main loop



//**************************************************************************
// Welcome to the setup loop
void setup(void)
{

  
  Wire.begin();
  RTC.begin();
  // If the Real Time Clock has begun to drift, you can reset it by pulling its
  // backup battery, powering down the Arduino, replacing the backup battery
  // and then compiling/uploading this sketch to the Arduino. It will only reset
  // the time if the real time clock is halted due to power loss.
//  if (! RTC.isrunning()) {
//    Serial.println("RTC is NOT running!");
//    // following line sets the RTC to the date & time this sketch was compiled
//    RTC.adjust(DateTime(__DATE__, __TIME__));
//  }
    // The clock reset function may be better left to a stand-alone sketch.

    //************************************
  // For debugging
  Serial.begin(9600);
  //************************************
  DateTime now = RTC.now();
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(' ');
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.println(now.second(), DEC);
  delay(2000);
  
  // Initialize qik 2s9v1 serial motor controller
  // The value in parentheses is the serial comm speed
  // for the 2s9v1 controller
//  qik.init(38400);

  //  encoder.init(); 255 refers to non-existing pins
  // Requires two pins per encoder, here on pins 2,3
  // Take the motor's encoder lines A and B (yellow and white on my motor)
  // and connect them to pins 2 and 3 on the Arduino.
  // Additionally, supply the encoder +5V from the Arduino's 5V line
//  encoder.init(2,3,255,255);
  
  // TODO: Create limit switch routine for initializing tide height value
  //       after a restart. 
  // TODO: Have user select tide height limits outside of which the motor
  //       won't turn any further.

  

}

//**************************************************************************
// Welcome to the Main loop
void loop(void)
{
  
  // Get current time, store in object 'now'
  DateTime now = RTC.now();
  
  // If it is the start of a new minute, calculate new tide height and
  // adjust motor position
  if (now.second() == 0) {
    
    // Calculate difference between current year and starting year.
    YearIndx = startYear - now.year();
    // Calculate hours since start of current year
    currHours = (now.unixtime() - startSecs[YearIndx]) / float(3600);
    // Shift currHours to Greenwich Mean Time
    currHours = currHours + adjustGMT;
    
    // *****************Calculate current tide height*************
    float results = Datum; // initialize results variable
//    for (int harms = 0; harms < 37; harms++) {
//    // Calculate each component of the overall tide equation 
//    // The currHours value is assumed to be in hours from the start of the
//    // year, in the Greenwich Mean Time zone, not your local time zone.
//    // There is no daylight savings time adjustment here.  
//      results = results + (Nodefactor[YearIndx][harms] * Amp[harms] * cos( (Speed[harms] * currHours + Equilarg[YearIndx][harms] - Kappa[harms]) * DEG_TO_RAD));
//   }
     //******************End of Tide Height calculation*************
    //********************************
     // For debugging
       // Print current time to serial monitor
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.println(now.second(), DEC);
    Serial.print("currHours: ");
    Serial.print(currHours);
    Serial.print(", Tide: ");
    Serial.print(results, 3);
    Serial.println(" ft.");
     // end of debugging stuff
     //********************************   
   delay(300);
  }

 
 // TODO: calculate how far motor needs to turn to reach new 
 //       tide height.
 // TODO: monitor interrupts to count motor revolutions and 
 //       and stop motor at correct new tide height.
 // TODO: include limit switch checking routines
 
} // end of main loop
