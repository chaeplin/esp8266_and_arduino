#include "EmonLib.h"                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

long timemillis;

void setup()
{  
  Serial.begin(115200);
  timemillis = millis();
  emon1.current(A0, 111.1);             // Current: input pin, calibration.
}

void loop()
{
  long now = millis();
  double Irms = emon1.calcIrms(1480);  // Calculate Irms only
  timemillis = millis();
  
  Serial.print("Irms -> :  ");
  Serial.print(Irms);
  Serial.print(" VIrms --> : ");
  Serial.print(Irms*230.0);
  Serial.print(" now --> : ");
  Serial.print(now);
  Serial.print(" timemillis ---> : ");
  Serial.print(timemillis);
  Serial.print(" diff ---> : ");
  Serial.println(timemillis - now);
  
  delay (1000);
}

/* result of esp8266 

Irms -> :  0.27 VIrms --> : 62.81 now --> : 11103 timemillis ---> : 11175 diff ---> : 72
Irms -> :  0.29 VIrms --> : 66.02 now --> : 12175 timemillis ---> : 12757 diff ---> : 582
Irms -> :  0.28 VIrms --> : 65.10 now --> : 13758 timemillis ---> : 13830 diff ---> : 72
Irms -> :  0.09 VIrms --> : 21.09 now --> : 14830 timemillis ---> : 14902 diff ---> : 72
Irms -> :  0.07 VIrms --> : 16.45 now --> : 15903 timemillis ---> : 15975 diff ---> : 72
Irms -> :  0.35 VIrms --> : 79.66 now --> : 16976 timemillis ---> : 17558 diff ---> : 582
Irms -> :  0.28 VIrms --> : 63.73 now --> : 18558 timemillis ---> : 18630 diff ---> : 72
Irms -> :  0.09 VIrms --> : 21.36 now --> : 19631 timemillis ---> : 19703 diff ---> : 72
Irms -> :  0.34 VIrms --> : 78.18 now --> : 20704 timemillis ---> : 21286 diff ---> : 582
Irms -> :  0.28 VIrms --> : 63.96 now --> : 22286 timemillis ---> : 22358 diff ---> : 72
Irms -> :  0.10 VIrms --> : 22.11 now --> : 23359 timemillis ---> : 23431 diff ---> : 72
Irms -> :  0.33 VIrms --> : 76.32 now --> : 24431 timemillis ---> : 25014 diff ---> : 583
Irms -> :  0.28 VIrms --> : 63.40 now --> : 26014 timemillis ---> : 26086 diff ---> : 72
Irms -> :  0.10 VIrms --> : 22.01 now --> : 27087 timemillis ---> : 27159 diff ---> : 72
Irms -> :  0.07 VIrms --> : 15.57 now --> : 28159 timemillis ---> : 28231 diff ---> : 72
Irms -> :  0.07 VIrms --> : 15.74 now --> : 29232 timemillis ---> : 29304 diff ---> : 72
Irms -> :  0.07 VIrms --> : 15.60 now --> : 30304 timemillis ---> : 30377 diff ---> : 73
Irms -> :  0.07 VIrms --> : 17.02 now --> : 31377 timemillis ---> : 31449 diff ---> : 72
Irms -> :  0.08 VIrms --> : 18.15 now --> : 32450 timemillis ---> : 32522 diff ---> : 72
Irms -> :  0.35 VIrms --> : 79.79 now --> : 33522 timemillis ---> : 34104 diff ---> : 582
Irms -> :  0.27 VIrms --> : 63.05 now --> : 35105 timemillis ---> : 35177 diff ---> : 72
Irms -> :  0.09 VIrms --> : 20.88 now --> : 36177 timemillis ---> : 36250 diff ---> : 73
Irms -> :  0.07 VIrms --> : 16.13 now --> : 37250 timemillis ---> : 37322 diff ---> : 72
Irms -> :  0.07 VIrms --> : 15.40 now --> : 38323 timemillis ---> : 38395 diff ---> : 72
Irms -> :  0.06 VIrms --> : 13.79 now --> : 39395 timemillis ---> : 39467 diff ---> : 72
Irms -> :  0.07 VIrms --> : 17.23 now --> : 40468 timemillis ---> : 40540 diff ---> : 72
Irms -> :  0.07 VIrms --> : 16.46 now --> : 41540 timemillis ---> : 41613 diff ---> : 73
Irms -> :  0.35 VIrms --> : 80.85 now --> : 42613 timemillis ---> : 43195 diff ---> : 582
Irms -> :  0.27 VIrms --> : 62.38 now --> : 44196 timemillis ---> : 44268 diff ---> : 72
Irms -> :  0.29 VIrms --> : 66.06 now --> : 45268 timemillis ---> : 45850 diff ---> : 582
Irms -> :  0.29 VIrms --> : 65.87 now --> : 46851 timemillis ---> : 46923 diff ---> : 72
Irms -> :  0.10 VIrms --> : 22.67 now --> : 47923 timemillis ---> : 47995 diff ---> : 72
Irms -> :  0.34 VIrms --> : 78.04 now --> : 48996 timemillis ---> : 49578 diff ---> : 582
Irms -> :  0.28 VIrms --> : 63.43 now --> : 50578 timemillis ---> : 50650 diff ---> : 72
Irms -> :  0.09 VIrms --> : 19.98 now --> : 51651 timemillis ---> : 51723 diff ---> : 72
Irms -> :  0.34 VIrms --> : 79.02 now --> : 52723 timemillis ---> : 53306 diff ---> : 583
Irms -> :  0.28 VIrms --> : 63.37 now --> : 54306 timemillis ---> : 54378 diff ---> : 72
Irms -> :  0.09 VIrms --> : 21.25 now --> : 55379 timemillis ---> : 55451 diff ---> : 72
Irms -> :  0.34 VIrms --> : 77.38 now --> : 56451 timemillis ---> : 57033 diff ---> : 582
Irms -> :  0.28 VIrms --> : 63.51 now --> : 58034 timemillis ---> : 58106 diff ---> : 72
Irms -> :  0.09 VIrms --> : 21.61 now --> : 59106 timemillis ---> : 59179 diff ---> : 73

*/


/* result of Uno 
Irms -> :  0.03 VIrms --> : 7.96 now --> : 110383 timemillis ---> : 110657 diff ---> : 274
Irms -> :  0.03 VIrms --> : 7.96 now --> : 111660 timemillis ---> : 111932 diff ---> : 272
Irms -> :  0.03 VIrms --> : 6.59 now --> : 112934 timemillis ---> : 113209 diff ---> : 275
Irms -> :  0.03 VIrms --> : 7.30 now --> : 114211 timemillis ---> : 114486 diff ---> : 275
Irms -> :  0.03 VIrms --> : 7.96 now --> : 115488 timemillis ---> : 115763 diff ---> : 275
Irms -> :  0.03 VIrms --> : 7.96 now --> : 116764 timemillis ---> : 117040 diff ---> : 276
Irms -> :  0.03 VIrms --> : 7.30 now --> : 118042 timemillis ---> : 118317 diff ---> : 275
Irms -> :  0.04 VIrms --> : 9.65 now --> : 119319 timemillis ---> : 119593 diff ---> : 274
Irms -> :  0.02 VIrms --> : 3.69 now --> : 120596 timemillis ---> : 120865 diff ---> : 269
Irms -> :  0.02 VIrms --> : 4.91 now --> : 121868 timemillis ---> : 122142 diff ---> : 274
Irms -> :  0.02 VIrms --> : 4.86 now --> : 123145 timemillis ---> : 123420 diff ---> : 275
Irms -> :  0.03 VIrms --> : 6.59 now --> : 124423 timemillis ---> : 124697 diff ---> : 274
Irms -> :  0.04 VIrms --> : 8.59 now --> : 125700 timemillis ---> : 125975 diff ---> : 275
Irms -> :  0.02 VIrms --> : 3.69 now --> : 126978 timemillis ---> : 127251 diff ---> : 273
Irms -> :  0.03 VIrms --> : 7.96 now --> : 128252 timemillis ---> : 128528 diff ---> : 276
Irms -> :  0.02 VIrms --> : 4.91 now --> : 129530 timemillis ---> : 129806 diff ---> : 276
Irms -> :  0.03 VIrms --> : 7.34 now --> : 130808 timemillis ---> : 131084 diff ---> : 276
Irms -> :  0.03 VIrms --> : 7.30 now --> : 132085 timemillis ---> : 132361 diff ---> : 276
Irms -> :  0.03 VIrms --> : 7.99 now --> : 133363 timemillis ---> : 133639 diff ---> : 276
Irms -> :  0.03 VIrms --> : 5.79 now --> : 134641 timemillis ---> : 134916 diff ---> : 275
Irms -> :  0.02 VIrms --> : 4.86 now --> : 135918 timemillis ---> : 136194 diff ---> : 276
Irms -> :  0.01 VIrms --> : 1.93 now --> : 137195 timemillis ---> : 137464 diff ---> : 269
Irms -> :  0.03 VIrms --> : 5.79 now --> : 138467 timemillis ---> : 138736 diff ---> : 269
Irms -> :  0.04 VIrms --> : 8.59 now --> : 139739 timemillis ---> : 140014 diff ---> : 275
Irms -> :  0.02 VIrms --> : 3.69 now --> : 141017 timemillis ---> : 141291 diff ---> : 274
Irms -> :  0.03 VIrms --> : 5.79 now --> : 142294 timemillis ---> : 142568 diff ---> : 274
Irms -> :  0.03 VIrms --> : 6.63 now --> : 143570 timemillis ---> : 143846 diff ---> : 276
Irms -> :  0.03 VIrms --> : 6.63 now --> : 144848 timemillis ---> : 145124 diff ---> : 276
Irms -> :  0.02 VIrms --> : 4.86 now --> : 146125 timemillis ---> : 146395 diff ---> : 270
Irms -> :  0.02 VIrms --> : 3.69 now --> : 147397 timemillis ---> : 147666 diff ---> : 269
Irms -> :  0.02 VIrms --> : 3.69 now --> : 148669 timemillis ---> : 148938 diff ---> : 269
Irms -> :  0.02 VIrms --> : 3.69 now --> : 149941 timemillis ---> : 150211 diff ---> : 270
Irms -> :  0.03 VIrms --> : 6.63 now --> : 151214 timemillis ---> : 151489 diff ---> : 275
Irms -> :  0.03 VIrms --> : 5.84 now --> : 152492 timemillis ---> : 152766 diff ---> : 274
Irms -> :  0.03 VIrms --> : 5.79 now --> : 153768 timemillis ---> : 154044 diff ---> : 276
Irms -> :  0.02 VIrms --> : 4.86 now --> : 155046 timemillis ---> : 155319 diff ---> : 273
Irms -> :  0.02 VIrms --> : 4.89 now --> : 156321 timemillis ---> : 156597 diff ---> : 276
Irms -> :  0.01 VIrms --> : 1.93 now --> : 157599 timemillis ---> : 157869 diff ---> : 270
Irms -> :  0.03 VIrms --> : 7.35 now --> : 158871 timemillis ---> : 159145 diff ---> : 274
Irms -> :  0.02 VIrms --> : 3.69 now --> : 160148 timemillis ---> : 160422 diff ---> : 274
Irms -> :  0.01 VIrms --> : 1.93 now --> : 161425 timemillis ---> : 161694 diff ---> : 269
Irms -> :  0.02 VIrms --> : 4.90 now --> : 162697 timemillis ---> : 162972 diff ---> : 275
Irms -> :  0.03 VIrms --> : 7.35 now --> : 163974 timemillis ---> : 164248 diff ---> : 274
Irms -> :  0.02 VIrms --> : 3.69 now --> : 165251 timemillis ---> : 165524 diff ---> : 273
Irms -> :  0.02 VIrms --> : 3.72 now --> : 166526 timemillis ---> : 166802 diff ---> : 276

*/

