#include <avr/pgmspace.h>
#include <Wire.h>
#include <RtcDS3231.h>

RtcDS3231 Rtc;

volatile bool balm_isr;
volatile uint32_t rtc_interuptCount = 0;

void alm_isr()
{
  rtc_interuptCount++;
  balm_isr = true;
}

void setup ()
{
  Serial.begin(115200);

  Serial.print("compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);

  balm_isr = false;


  //--------RTC SETUP ------------
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();

  if (!Rtc.IsDateTimeValid())
  {
    // Common Cuases:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");

    // following line sets the RTC to the date & time this sketch was compiled
    // it will also reset the valid flag internally unless the Rtc device is
    // having an issue

    Rtc.SetDateTime(compiled);
  }

  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled)
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled)
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled)
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }

  // never assume the Rtc was last configured by you, so
  // just clear them to your needed state
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePinClockFrequency(DS3231SquareWaveClock_1Hz);
  //Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeClock);

  attachInterrupt(0, alm_isr, FALLING);
}

void loop ()
{
  if (!Rtc.IsDateTimeValid())
  {
    // Common Cuases:
    //    1) the battery on the device is low or even missing and the power line was disconnected
    Serial.println("RTC lost confidence in the DateTime!");
  }

  if (balm_isr)
  {
    RtcDateTime now = Rtc.GetDateTime();
    printDateTime(now);
    Serial.print(" - ");

    RtcTemperature temp = Rtc.GetTemperature();
    Serial.print(temp.AsFloat());
    Serial.print("C - ");
    Serial.println(rtc_interuptCount);

    balm_isr = false;
  }
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
  char datestring[20];

  snprintf_P(datestring,
             countof(datestring),
             PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
             dt.Month(),
             dt.Day(),
             dt.Year(),
             dt.Hour(),
             dt.Minute(),
             dt.Second() );
  Serial.print(datestring);
}

